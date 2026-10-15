#!/usr/bin/env python3
"""Credential plan boundaries and interrupted/failed signed update recovery."""
import gzip
import io
import json
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest
from unittest.mock import patch

import migrations
import publish
import update_common as common
import update_service as service


def plan(*names):
    return migrations.CredentialPlan.parse(json.dumps({'schema': 1, 'add_credentials': list(names)}).encode())


def elf():
    data = bytearray(64)
    data[:6] = b'\x7fELF\x01\x01'
    data[16:18] = (2).to_bytes(2, 'little')
    data[18:20] = (40).to_bytes(2, 'little')
    return bytes(data)


class MigrationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.state = self.root / 'state'
        self.bins = self.root / 'bin'
        self.drop_in = self.root / 'credentials.conf'
        self.state.mkdir()
        self.bins.mkdir()
        self.old = {name: elf() + b'old' for name in common.FILES}
        self.new = {name: elf() + b'new' for name in common.FILES}
        for name, data in self.old.items():
            (self.bins / name).write_bytes(data)
        self.transaction = service.Transaction(self.state, self.bins, self.drop_in)
        self.release = {'version': '2026.10.14.5', 'sequence': 5}

    def assert_old(self):
        self.assertEqual({name: (self.bins / name).read_bytes() for name in common.FILES}, self.old)

    def test_rejects_injection_paths_values_duplicates_and_unbounded_names(self):
        for names in [[], ['porthole-google'] * 2, [True], ['../etc/passwd'],
                      ['porthole-google\nExecStart=/bin/sh'], ['porthole-google:secret'],
                      ['porthole-%i'], ['porthole-'], ['other-service'],
                      ['porthole-' + 'a' * 49], ['porthole-x' + str(i) for i in range(17)]]:
            with self.subTest(names=names), self.assertRaises(common.Rejected):
                plan(*names)
        for value in [{'schema': True, 'add_credentials': ['porthole-google']},
                      {'schema': 1, 'add_credentials': ['porthole-google'], 'secret': 'never'},
                      {'schema': 1, 'add_credentials': ['porthole-google'], 'command': 'never'}]:
            with self.assertRaises(common.Rejected):
                migrations.CredentialPlan.parse(json.dumps(value).encode())

    def test_additive_idempotent_configuration_and_legacy_releases_preserve_it(self):
        google = plan('porthole-google')
        self.transaction.prepare(self.new, self.release, google)
        self.transaction.commit()
        first = self.drop_in.read_bytes()
        self.transaction.prepare(self.new, self.release, google)
        self.transaction.commit()
        self.assertEqual(self.drop_in.read_bytes(), first)
        self.transaction.prepare(self.new, self.release, plan('porthole-new-service'))
        self.transaction.commit()
        expected = plan('porthole-google', 'porthole-new-service').render()
        self.assertEqual(self.drop_in.read_bytes(), expected)
        self.transaction.prepare(self.new, self.release)
        self.transaction.commit()
        self.assertEqual(self.drop_in.read_bytes(), expected)
        self.assertEqual(self.drop_in.stat().st_mode & 0o777, 0o644)

    def test_foreign_or_symlink_drop_in_never_overwritten(self):
        self.drop_in.write_bytes(b'[Service]\nExecStart=local-customization\n')
        with self.assertRaises(common.Rejected):
            self.transaction.prepare(self.new, self.release, plan('porthole-google'))
        self.assert_old()
        self.assertFalse((self.state / 'pending.json').exists())
        self.drop_in.unlink()
        target = self.root / 'target'
        target.write_bytes(plan('porthole-existing').render())
        self.drop_in.symlink_to(target)
        with self.assertRaises(OSError):
            self.transaction.prepare(self.new, self.release, plan('porthole-google'))
        self.assertEqual(target.read_bytes(), plan('porthole-existing').render())

    def test_cumulative_limit_fails_before_writing_binaries_or_journal(self):
        existing = plan(*('porthole-name' + str(i) for i in range(16))).render()
        self.drop_in.write_bytes(existing)
        with self.assertRaises(common.Rejected):
            self.transaction.prepare(self.new, self.release, plan('porthole-extra'))
        self.assert_old()
        self.assertEqual(self.drop_in.read_bytes(), existing)
        self.assertFalse((self.state / 'pending.json').exists())

    def test_power_loss_after_configuration_write_restores_absence_or_existing(self):
        for previous in [None, plan('porthole-existing').render()]:
            with self.subTest(previous=previous):
                for name, data in self.old.items():
                    (self.bins / name).write_bytes(data)
                if previous is not None:
                    self.drop_in.write_bytes(previous)
                self.transaction.prepare(self.new, self.release, plan('porthole-google'))
                reboot = service.Transaction(self.state, self.bins, self.drop_in)
                self.assertEqual(reboot.recover(), self.release)
                # Repeated recovery is safe if power fails before journal removal.
                self.assertEqual(reboot.recover(), self.release)
                reboot.commit()
                self.assert_old()
                self.assertEqual(self.drop_in.read_bytes() if self.drop_in.exists() else None, previous)

    def test_power_loss_between_binary_writes_restores_configuration_and_pair(self):
        self.drop_in.write_bytes(plan('porthole-existing').render())
        def interrupt(files):
            common.atomic(self.bins / common.FILES[0], files[common.FILES[0]], 0o755)
            raise OSError('power loss')
        with patch.object(self.transaction, 'replace', side_effect=interrupt):
            with self.assertRaises(OSError):
                self.transaction.prepare(self.new, self.release, plan('porthole-google'))
        self.transaction.recover()
        self.transaction.commit()
        self.assert_old()
        self.assertEqual(self.drop_in.read_bytes(), plan('porthole-existing').render())

    def test_corrupted_configuration_backup_blocks_all_recovery_writes(self):
        self.drop_in.write_bytes(plan('porthole-existing').render())
        self.transaction.prepare(self.new, self.release, plan('porthole-google'))
        (self.state / 'previous' / 'credentials.conf').write_bytes(b'corrupt')
        with self.assertRaises(common.Rejected):
            self.transaction.recover()
        self.assertEqual({name: (self.bins / name).read_bytes() for name in common.FILES}, self.new)
        self.assertTrue((self.state / 'pending.json').exists())

    def test_signed_migration_bundle_roundtrip_and_plan_tampering(self):
        private = self.root / 'key'
        public = self.root / 'public'
        subprocess.run(['openssl', 'genpkey', '-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048', '-out', str(private)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(['openssl', 'pkey', '-in', str(private), '-pubout', '-out', str(public)], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        source = self.root / 'migrations.json'
        source.write_text('{"schema":1,"add_credentials":["porthole-google"]}')
        output = self.root / 'release'
        publish.bundle(self.bins, '2026.10.14.6', 6, output, private, source)
        raw = (output / 'porthole-manifest.json').read_bytes()
        signature = (output / 'porthole-manifest.sig').read_bytes()
        manifest = common.verify_signature(raw, signature, public)
        self.assertEqual(manifest.schema, 2)
        archive = (output / 'porthole-armv7.tar.gz').read_bytes()
        contents = common.unpack(manifest, archive)
        self.assertEqual(plan('porthole-google'), migrations.CredentialPlan.parse(contents['migrations.json']))
        changed = gzip.compress(gzip.decompress(archive).replace(b'porthole-google', b'porthole-hacked'), mtime=0)
        with self.assertRaises(common.Rejected):
            common.unpack(manifest, changed)
        with self.assertRaises(common.Rejected):
            common.verify_signature(raw.replace(b'"schema":2', b'"schema":1'), signature, public)

    def test_success_and_failed_health_reload_before_start_and_restore_together(self):
        import tarfile
        for healthy in [False, True]:
            with self.subTest(healthy=healthy):
                for name, data in self.old.items():
                    (self.bins / name).write_bytes(data)
                self.drop_in.unlink(missing_ok=True)
                contents = {**self.new, 'migrations.json': b'{"schema":1,"add_credentials":["porthole-google"]}'}
                raw = io.BytesIO()
                with tarfile.open(fileobj=raw, mode='w', format=tarfile.USTAR_FORMAT) as archive:
                    for name, data in contents.items():
                        info = tarfile.TarInfo(name)
                        info.size = len(data)
                        archive.addfile(info, io.BytesIO(data))
                blob = gzip.compress(raw.getvalue())
                manifest = common.Manifest(6, '2026.10.14.6', len(blob), common.sha(blob), {name: common.sha(data) for name, data in contents.items()}, 2)
                cache = self.root / 'cache'
                cache.mkdir(exist_ok=True)
                for name, data in [('manifest.json', b'signed'), ('manifest.sig', b'signature'), ('app.tar.gz', blob)]:
                    (cache / name).write_bytes(data)
                instance = service.Installer.__new__(service.Installer)
                instance.lock = threading.RLock()
                instance.busy = False
                instance.value = {**self.release, 'highest': 5, 'phase': 'idle', 'reason': ''}
                with patch.object(service, 'STATE', self.state), patch.object(service, 'BIN', self.bins), patch.object(service, 'CACHE', cache), patch.object(service, 'guard'), patch.object(service, 'verify_signature', return_value=manifest), patch.object(service, 'Transaction', return_value=self.transaction), patch.object(instance, 'persist'), patch.object(instance, 'health', return_value=healthy), patch.object(service, 'command') as command:
                    self.assertTrue(instance.apply()['ok'])
                calls = [call.args[0] for call in command.call_args_list]
                reload = calls.index(['/usr/bin/systemctl', 'daemon-reload'])
                start = calls.index(['/usr/bin/systemctl', 'start', 'porthole-calendar', 'porthole-kiosk', 'porthole-orientation'])
                self.assertLess(reload, start)
                self.assertFalse((self.state / 'pending.json').exists())
                self.assertEqual(instance.value['highest'], 6)
                if healthy:
                    self.assertEqual(instance.value['phase'], 'installed')
                    self.assertEqual(self.drop_in.read_bytes(), plan('porthole-google').render())
                else:
                    self.assertEqual(instance.value['phase'], 'rolled-back')
                    self.assertFalse(self.drop_in.exists())
                    self.assert_old()
                    self.assertEqual(calls.count(['/usr/bin/systemctl', 'daemon-reload']), 2)


if __name__ == '__main__':
    unittest.main()
