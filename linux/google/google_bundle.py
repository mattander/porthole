"""Bounded credential schema shared by the trusted importer and setup helper."""
import re

def valid_bundle(value):
    if not isinstance(value,dict) or set(value)!={'client_id','client_secret','refresh_token'}:return False
    for name,limit in (('client_id',254),('client_secret',512),('refresh_token',2048)):
        text=value[name]
        if not isinstance(text,str) or not 1<=len(text)<=limit or not re.fullmatch(r'[!-~]+',text):return False
    return value['client_id'].endswith('.apps.googleusercontent.com')
