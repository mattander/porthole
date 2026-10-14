#!/usr/bin/env python3
import importlib.util,json,tempfile,unittest
from pathlib import Path
from google_bundle import valid_bundle
spec=importlib.util.spec_from_file_location('connect_google',Path(__file__).with_name('connect-google.py'));helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
class SetupTests(unittest.TestCase):
    def test_schema_rejects_injected_or_oversize_credentials(self):
        good={'client_id':'fixture.apps.googleusercontent.com','client_secret':'fixture','refresh_token':'fixture'}
        self.assertTrue(valid_bundle(good))
        for name in good:
            bad=good.copy();bad[name]='line\nsecret';self.assertFalse(valid_bundle(bad))
        self.assertFalse(valid_bundle({**good,'target':'/tmp/other'}));self.assertFalse(valid_bundle({**good,'refresh_token':'x'*2049}))
    def test_callback_state_and_duplicate_code(self):
        self.assertEqual(helper.callback('state=nonce&code=fixture','nonce'),('code','fixture'))
        self.assertIsNone(helper.callback('state=wrong&code=fixture','nonce'));self.assertIsNone(helper.callback('state=nonce&code=a&code=b','nonce'))
        self.assertEqual(helper.callback('state=nonce&error=access_denied','nonce'),('denied',''))
    def test_only_installed_desktop_client(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'client.json';value={'client_id':'fixture.apps.googleusercontent.com','client_secret':'fixture'}
            path.write_text(json.dumps({'web':value}))
            with self.assertRaises(RuntimeError):helper.read_client(path)
            path.write_text(json.dumps({'installed':value}));self.assertEqual(helper.read_client(path),tuple(value.values()))
if __name__=='__main__':unittest.main()
