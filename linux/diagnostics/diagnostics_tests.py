#!/usr/bin/env python3
import copy
import importlib.util
import json
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import threading
import unittest
from unittest.mock import patch

from diagnostics_admin import validate_certificate, apply
from diagnostics_common import EVENTS, REQUEST, certificate_id, tls_context
from diagnostics_server import DiagnosticServer
from export_diagnostics import record, collect
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('diagnostic_client', Path(__file__).with_name('diagnostics.py'))
client = importlib.util.module_from_spec(spec);spec.loader.exec_module(client)


class DiagnosticTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temporary.name)
        for name, purpose in [('server', 'serverAuth'), ('computer', 'clientAuth'), ('stranger', 'clientAuth')]:
            key, cert = cls.directory / (name + '.key'), cls.directory / (name + '.pem')
            subprocess.run(['openssl', 'req', '-x509', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:P-256',
                            '-nodes', '-days', '2', '-subj', '/CN=porthole-diagnostics', '-addext',
                            'subjectAltName=DNS:porthole-diagnostics', '-addext', 'basicConstraints=critical,CA:FALSE',
                            '-addext', 'keyUsage=critical,digitalSignature', '-addext', 'extendedKeyUsage=' + purpose,
                            '-keyout', str(key), '-out', str(cert)], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def server(self, approved=None):
        directory = self.directory
        certificate = (directory / 'computer.pem').read_text()
        context = tls_context(True, directory / 'server.pem', directory / 'server.key', certificate)
        server = DiagnosticServer(('127.0.0.1', 0), context,
                                  approved if approved is not None else {certificate_id(certificate)},
                                  collector=lambda: {'schema': 1, 'events': [], 'services': {}})
        thread = threading.Thread(target=server.serve_forever, daemon=True);thread.start()
        self.addCleanup(server.server_close);self.addCleanup(server.shutdown)
        return server

    def test_allowlisted_records_never_copy_free_text(self):
        entry = {'_SYSTEMD_UNIT': 'porthole-calendar.service', '__REALTIME_TIMESTAMP': '1792756800000000',
                 'MESSAGE': 'PORTHOLE_DIAGNOSTIC ' + json.dumps({'v': 1, 'event': 'briefing_waiting_calendar', 'http': 0, 'network': 0}),
                 'ACCOUNT': 'private@example.invalid'}
        accepted = record(entry)
        self.assertEqual(accepted['event'], 'briefing_waiting_calendar')
        self.assertNotIn('private', json.dumps(accepted))
        for change in ({'event': 'private-calendar-title'}, {'http': 'secret'}, {'http': True}, {'network': -1},
                       {'secret': 'sk-private'}, {'event': ['briefing_complete']}, {'v': True}):
            changed = copy.deepcopy(entry)
            changed['MESSAGE'] = 'PORTHOLE_DIAGNOSTIC ' + json.dumps({**json.loads(entry['MESSAGE'].split(' ', 1)[1]), **change})
            self.assertIsNone(record(changed))
        self.assertIsNone(record({**entry, '_SYSTEMD_UNIT': 'untrusted.service'}))
        self.assertIsNone(record({**entry, 'MESSAGE': 'raw network password'}))
        self.assertIsNone(record({**entry, '__REALTIME_TIMESTAMP': 'secret'}))

    def test_only_public_client_certificates_can_be_enrolled(self):
        pem = (self.directory / 'computer.pem').read_text()
        self.assertEqual(validate_certificate(pem), certificate_id(pem))
        for value in (pem + pem, (self.directory / 'computer.key').read_text(), 'x' * 2049, {}, pem + 'command'):
            with self.assertRaises((ValueError, subprocess.CalledProcessError)):
                validate_certificate(value)
        with self.assertRaises(ValueError):
            validate_certificate((self.directory / 'server.pem').read_text())

    def test_enrollment_limit_and_operation_boundaries(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary);(root / 'clients').mkdir()
            for i in range(8):
                (root / 'clients' / (str(i) + '.pem')).write_text('fixture')
            with patch('diagnostics_admin.ROOT', root), patch('diagnostics_admin.server_identity') as identity:
                with self.assertRaisesRegex(ValueError, 'Eight computers'):
                    apply({'operation': 'enroll', 'certificate': (self.directory / 'computer.pem').read_text()})
                identity.assert_not_called()
                for value in ({'operation': 'shell'}, {'operation': 'status', 'path': '/etc/shadow'},
                              {'operation': 'revoke', 'fingerprint': '../../etc/shadow'},
                              {'operation': 'enroll', 'certificate': 'not a certificate'}):
                    with self.assertRaises(ValueError):
                        apply(value)

    def test_admin_requires_root_and_physical_usb(self):
        import sys
        path = Path(__file__).with_name('diagnostics_admin.py')
        result = subprocess.run([sys.executable, '-I', str(path), 'PORTHOLE_DIAG_' + '0' * 32],
                                input=b'{"operation":"status"}\n', capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b'physical USB', result.stderr)

    def test_enrolled_client_and_pinned_server(self):
        server = self.server();d = self.directory
        value = client.fetch('127.0.0.1', d / 'computer.pem', d / 'computer.key', (d / 'server.pem').read_text(), server.server_address[1])
        self.assertEqual(value, {'schema': 1, 'events': [], 'services': {}})

    def test_unknown_certificate_and_missing_certificate_are_rejected(self):
        server = self.server();d = self.directory
        with self.assertRaises((ssl.SSLError, OSError, ValueError)):
            client.fetch('127.0.0.1', d / 'stranger.pem', d / 'stranger.key', (d / 'server.pem').read_text(), server.server_address[1])
        context = ssl.create_default_context(cafile=str(d / 'server.pem'));context.minimum_version = ssl.TLSVersion.TLSv1_3
        with self.assertRaises((ssl.SSLError, OSError)):
            with socket.create_connection(server.server_address, timeout=2) as raw:
                with context.wrap_socket(raw, server_hostname='porthole-diagnostics') as connection:
                    connection.sendall(REQUEST);connection.recv(4096)

    def test_pinned_server_mismatch_and_revoked_client_are_rejected(self):
        server = self.server();d = self.directory
        with self.assertRaises((ssl.SSLError, OSError)):
            client.fetch('127.0.0.1', d / 'computer.pem', d / 'computer.key', (d / 'stranger.pem').read_text(), server.server_address[1])
        server.approved = frozenset()
        with self.assertRaises((OSError, ValueError)):
            client.fetch('127.0.0.1', d / 'computer.pem', d / 'computer.key', (d / 'server.pem').read_text(), server.server_address[1])

    def test_wrong_request_has_no_commands_or_file_access(self):
        server = self.server();d = self.directory
        context = tls_context(False, d / 'computer.pem', d / 'computer.key', (d / 'server.pem').read_text())
        with socket.create_connection(server.server_address, timeout=2) as raw:
            with context.wrap_socket(raw, server_hostname='porthole-diagnostics') as connection:
                connection.sendall(b'cat /etc/shadow\n' + b'x' * len(REQUEST))
                self.assertEqual(connection.recv(4096), b'')

    def test_empty_history_is_valid_but_collection_failure_is_explicit(self):
        replies = [SimpleNamespace(returncode=1, stdout=b'', stderr=b''),
                   SimpleNamespace(stdout=b'active\n'), SimpleNamespace(stdout=b'active\n')]
        with patch('export_diagnostics.subprocess.run', side_effect=replies):
            self.assertEqual(collect()['events'], [])
        with patch('export_diagnostics.subprocess.run', return_value=SimpleNamespace(returncode=1, stdout=b'', stderr=b'Permission denied')):
            with self.assertRaises(RuntimeError):
                collect()
        server = self.server();d = self.directory
        def broken():
            raise RuntimeError('private details')
        server.collector = broken
        with self.assertRaisesRegex(RuntimeError, 'could not collect'):
            client.fetch('127.0.0.1', d / 'computer.pem', d / 'computer.key', (d / 'server.pem').read_text(), server.server_address[1])

    def test_fixed_event_schema_matches_native_logging(self):
        import re
        source = Path(__file__).parent.parent / 'native' / 'diagnostic-log.cpp'
        self.assertEqual(set(re.findall(r'return "([a-z_]+)"', source.read_text())), EVENTS)


if __name__ == '__main__':
    unittest.main()
