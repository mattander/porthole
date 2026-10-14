import importlib.util,json,unittest
from pathlib import Path
from unittest.mock import patch
spec=importlib.util.spec_from_file_location('vault',Path(__file__).with_name('credential-vault.py'));vault=importlib.util.module_from_spec(spec);spec.loader.exec_module(vault)
class VaultTests(unittest.TestCase):
 def bundle(self):return {'account':'fixture@example.invalid','password':'abcd-efgh-ijkl-mnop','calendars':[{'url':'https://p42-caldav.icloud.com/fixture/','name':'Fixture'}]}
 def test_accepts_only_calendar_service_credentials(self):
  self.assertTrue(vault.valid_bundle(self.bundle()))
  for value in ('PrimaryPassword!','abcd-efgh-ijkl-mnop\n'):
   bundle=self.bundle();bundle['password']=value;self.assertFalse(vault.valid_bundle(bundle))
 def test_rejects_untrusted_destinations(self):
  for value in ('http://caldav.icloud.com/','https://caldav.icloud.com.evil.example/','file:///etc/passwd','https://user:pass@caldav.icloud.com/','https://caldav.icloud.com:444/','https://caldav.icloud.com/#secret'):
   bundle=self.bundle();bundle['calendars'][0]['url']=value;self.assertFalse(vault.valid_bundle(bundle))
 def test_rejects_oversized_selection_and_bad_account(self):
  bundle=self.bundle();bundle['calendars']*=17;self.assertFalse(vault.valid_bundle(bundle))
  for value in ('account@example.invalid\nheader','account:injection@example.invalid','a\0@example.invalid'):
   bundle=self.bundle();bundle['account']=value;self.assertFalse(vault.valid_bundle(bundle))
if __name__=='__main__':unittest.main()
