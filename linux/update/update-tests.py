#!/usr/bin/env python3
"""Signature, archive, IPC privilege, transaction and reboot-recovery regressions."""
import gzip,importlib.util,io,json,os,subprocess,sys,tarfile,tempfile,threading,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parent))
import update_common as common
import update_service as service
import update_fetch as fetch
import publish

def elf():
    data=bytearray(64);data[:6]=b'\x7fELF\x01\x01';data[16:18]=(2).to_bytes(2,'little');data[18:20]=(40).to_bytes(2,'little');return bytes(data)

def archive(entries):
    raw=io.BytesIO()
    with tarfile.open(fileobj=raw,mode='w',format=tarfile.USTAR_FORMAT) as t:
        for name,data,kind in entries:
            i=tarfile.TarInfo(name);i.size=len(data);i.type=kind
            if kind!=tarfile.REGTYPE:i.linkname='../../etc/passwd'
            t.addfile(i,io.BytesIO(data))
    return gzip.compress(raw.getvalue(),mtime=0)

def manifest(data):
    value={'schema':1,'sequence':1,'version':'2026.10.14.1','platform':common.PLATFORM,'kernel':common.KERNEL,'archive_size':len(data),'archive_sha256':common.sha(data),'files':{k:common.sha(elf()) for k in common.FILES}}
    return common.Manifest.parse(json.dumps(value).encode())

class UpdateTests(unittest.TestCase):
    def test_signed_bundle_roundtrip_and_tamper_rejection(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'bin').mkdir()
            for name in common.FILES:(p/'bin'/name).write_bytes(elf())
            subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:2048','-out',str(p/'key')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            subprocess.run(['openssl','pkey','-in',str(p/'key'),'-pubout','-out',str(p/'pub')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            publish.bundle(p/'bin','2026.10.14.1',1,p/'release',p/'key');r=p/'release'
            raw=(r/'porthole-manifest.json').read_bytes();sig=(r/'porthole-manifest.sig').read_bytes();blob=(r/'porthole-armv7.tar.gz').read_bytes()
            m=common.verify_signature(raw,sig,p/'pub');self.assertEqual(common.unpack(m,blob),{k:elf() for k in common.FILES})
            for modified in [raw+b' ',raw.replace(b'2026.10.14.1',b'2026.10.14.2')]:
                with self.assertRaises(common.Rejected):common.verify_signature(modified,sig,p/'pub')
            with self.assertRaises(common.Rejected):common.unpack(m,blob+b'no')
    def test_archive_rejects_traversal_symlinks_duplicates_extra_files_and_bombs(self):
        valid=[(k,elf(),tarfile.REGTYPE) for k in common.FILES]
        for entries in [valid+[('../../etc/passwd',elf(),tarfile.REGTYPE)],valid+[valid[0]],[(k,elf(),tarfile.SYMTYPE) for k in common.FILES],valid+[('profile.json',b'private',tarfile.REGTYPE)]]:
            data=archive(entries)
            with self.assertRaises(common.Rejected):common.unpack(manifest(data),data)
        data=gzip.compress(b'\0'*(common.MAX_UNPACKED+1))
        with self.assertRaises(common.Rejected):common.unpack(manifest(data),data)
    def test_schema_rejects_duplicate_fields_wrong_platform_and_booleans(self):
        with self.assertRaises(common.Rejected):common.parse_json(b'{"schema":1,"schema":1}')
        data=archive([(k,elf(),tarfile.REGTYPE) for k in common.FILES]);m=manifest(data)
        values={'schema':1,'sequence':m.sequence,'version':m.version,'platform':common.PLATFORM,'kernel':common.KERNEL,'archive_size':m.archive_size,'archive_sha256':m.archive_sha256,'files':m.files}
        for key,value in [('sequence',True),('sequence',0),('platform','other'),('version','../../evil'),('files',{}),('archive_size',common.MAX_ARCHIVE+1)]:
            altered=dict(values);altered[key]=value
            with self.assertRaises(common.Rejected):common.Manifest.parse(json.dumps(altered).encode())
    def test_network_is_https_and_only_release_hosts(self):
        for url in ['http://github.com/a/b','https://github.com.evil.example/a','https://user:pass@github.com/a','https://github.com:444/a','file:///etc/passwd','https://api.github.com/a']:
            with self.assertRaises(common.Rejected):common.safe_url(url)
        self.assertEqual(common.safe_url('https://release-assets.githubusercontent.com/a?token=example'),'https://release-assets.githubusercontent.com/a?token=example')
    def test_symlink_and_oversize_candidate_files_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'file').write_bytes(b'abcd');(p/'link').symlink_to(p/'file')
            with self.assertRaises(OSError):common.read_regular(p/'link',10)
            with self.assertRaises(common.Rejected):common.read_regular(p/'file',3)
    def test_crash_after_partial_replace_restores_both_files_and_old_metadata(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);state=p/'state';bins=p/'bin';state.mkdir();bins.mkdir();old={k:b'old-'+k.encode() for k in common.FILES};new={k:b'new-'+k.encode() for k in common.FILES}
            for k,v in old.items():(bins/k).write_bytes(v)
            t=service.Transaction(state,bins)
            original=t.replace
            def crash(files):common.atomic(bins/common.FILES[0],files[common.FILES[0]],0o755);raise OSError('simulated power loss')
            with patch.object(t,'replace',side_effect=crash):
                with self.assertRaises(OSError):t.prepare(new,{'version':'local','sequence':0})
            reboot=service.Transaction(state,bins);self.assertEqual(reboot.recover(),{'version':'local','sequence':0});reboot.commit()
            self.assertEqual({k:(bins/k).read_bytes() for k in common.FILES},old);self.assertFalse((state/'pending.json').exists())
    def test_recovery_refuses_corrupted_backup(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'s').mkdir();(p/'b').mkdir()
            for k in common.FILES:(p/'b'/k).write_bytes(b'old')
            t=service.Transaction(p/'s',p/'b');t.prepare({k:b'new' for k in common.FILES},{'version':'local','sequence':0});(p/'s'/'previous'/common.FILES[0]).write_bytes(b'corrupted')
            with self.assertRaises(common.Rejected):t.recover()
    def test_gui_peer_cannot_submit_candidate_or_commands(self):
        instance=service.Installer.__new__(service.Installer);instance.gui_uid=100;instance.fetch_uid=101;instance.lock=threading.RLock();instance.busy=False;instance.manual=False
        instance.value={'automatic':True,'version':'local','phase':'idle','reason':'','checked':'','highest':4,'sequence':4}
        for uid,request in [(999,{'type':'status'}),(100,{'type':'candidate'}),(100,{'type':'check','command':'evil'}),(101,{'type':'automatic','enabled':False})]:self.assertEqual(instance.dispatch(uid,request),{'ok':False})
        with patch.object(instance,'persist'):
            self.assertTrue(instance.dispatch(100,{'type':'automatic','enabled':False})['ok']);self.assertFalse(instance.value['automatic'])
            self.assertFalse(instance.dispatch(101,{'type':'begin'})['allowed']);instance.manual=True;self.assertTrue(instance.dispatch(101,{'type':'begin'})['allowed'])
    def test_replayed_signed_sequence_does_not_install(self):
        instance=service.Installer.__new__(service.Installer);instance.lock=threading.RLock();instance.busy=False;instance.value={'highest':7,'sequence':7,'phase':'checking','reason':''}
        m=common.Manifest(7,'2026.10.14.1',1,'a'*64,{k:'b'*64 for k in common.FILES})
        with patch.object(service,'guard'),patch.object(service,'read_regular',return_value=b'x'),patch.object(service,'verify_signature',return_value=m),patch.object(service,'unpack',return_value={k:elf() for k in common.FILES}),patch.object(instance,'persist'),patch.object(service.Transaction,'prepare') as prepare:
            self.assertTrue(instance.apply()['ok']);prepare.assert_not_called();self.assertEqual(instance.value['highest'],7)

if __name__=='__main__':unittest.main()
