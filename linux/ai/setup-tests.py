#!/usr/bin/env python3
"""Offline USB provisioning tests. Uses a pseudo-terminal, never a real frame/key."""
import contextlib,importlib.util,io,os,pty,re,select,sys,termios,threading,time,unittest
from pathlib import Path
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('key_setup',Path(__file__).with_name('set-openai-key.py'))
setup=importlib.util.module_from_spec(spec);spec.loader.exec_module(setup)

class Input:
    def isatty(self):return True

class SetupTests(unittest.TestCase):
    def test_discovers_only_the_matched_devices_serial_child(self):
        target={'IOObjectClass':'IOUSBHostDevice','idVendor':7531,'idProduct':260,'USB Product Name':'PF1007L Linux',
                'IORegistryEntryChildren':[{'IORegistryEntryChildren':[{'IOCalloutDevice':'/dev/cu.usbmodemExample42'}]}]}
        unrelated={'IOObjectClass':'IOUSBHostDevice','idVendor':1234,'idProduct':260,
                   'IORegistryEntryChildren':[{'IOCalloutDevice':'/dev/cu.usbmodemOther99'}]}
        self.assertEqual(setup.frame_port([unrelated,target]),'/dev/cu.usbmodemExample42')
        for registry in ([unrelated],[target,target],[dict(target,IORegistryEntryChildren=[])],
                         [dict(target,IORegistryEntryChildren=[{'IOCalloutDevice':'/tmp/untrusted'}])]):
            with self.assertRaises(RuntimeError):setup.frame_port(registry)

    def test_private_handshake_and_hidden_transfer(self):
        master,slave=pty.openpty();original=termios.tcgetattr(slave);result={};errors=[]
        fake='sk-'+('fixture_'*6);path=os.ttyname(slave)
        def receiver():
            try:
                data=b'';deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    if select.select([master],[],[],.1)[0]:
                        data+=os.read(master,4096)
                        found=re.search(rb'PORTHOLE_KEY_[a-f0-9]{32}',data)
                        if found and data.endswith(b'\n'):break
                marker=found.group(0);attrs=termios.tcgetattr(slave);attrs[3]&=~termios.ECHO;termios.tcsetattr(slave,termios.TCSANOW,attrs)
                os.write(master,marker+b'_READY\n');payload=b''
                while not payload.endswith(b'\n'):
                    if not select.select([master],[],[],2)[0]:raise RuntimeError('Fixture input timed out')
                    payload+=os.read(master,4096)
                result['matches']=payload.strip()==fake.encode();os.write(master,marker+b'_SAVED\n')
            except Exception as error:errors.append(type(error).__name__)
        worker=threading.Thread(target=receiver);worker.start();output=io.StringIO()
        try:
            with patch.object(setup.sys,'platform','darwin'),patch.object(setup.sys,'argv',['set-openai-key.py']),patch.object(setup.sys,'stdin',Input()),patch.object(setup,'find_frame',return_value=path),patch.object(setup.getpass,'getpass',return_value=fake),patch.object(setup.resource,'setrlimit'),contextlib.redirect_stdout(output):setup.main()
            worker.join(5);self.assertFalse(errors);self.assertTrue(result['matches']);self.assertNotIn(fake,output.getvalue());self.assertIn('encrypted',output.getvalue());self.assertEqual(original[3]&~termios.PENDIN,termios.tcgetattr(slave)[3]&~termios.PENDIN)
        finally:os.close(master);os.close(slave)
    def test_arguments_and_invalid_key_fail_before_serial(self):
        with patch.object(setup.sys,'platform','darwin'),patch.object(setup.sys,'argv',['set-openai-key.py','not-allowed']),patch.object(setup.sys,'stdin',Input()),patch.object(setup.os,'open') as opened:
            with self.assertRaises(RuntimeError):setup.main()
            opened.assert_not_called()
        with patch.object(setup.sys,'platform','darwin'),patch.object(setup.sys,'argv',['set-openai-key.py']),patch.object(setup.sys,'stdin',Input()),patch.object(setup,'find_frame',return_value='fixture'),patch.object(setup.getpass,'getpass',return_value='invalid'),patch.object(setup.resource,'setrlimit'),patch.object(setup.os,'open') as opened:
            with self.assertRaises(RuntimeError):setup.main()
            opened.assert_not_called()

if __name__=='__main__':unittest.main()
