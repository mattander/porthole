#!/usr/bin/python3
"""Fixed-purpose local credential writer. Never logs request bodies or secrets."""
import json,os,pwd,re,socket,struct,subprocess,threading
from pathlib import Path
from urllib.parse import urlsplit
SOCKET=Path('/run/porthole-credentials/socket')
TARGET=Path('/etc/credstore.encrypted/porthole-icloud')
OPENAI_TARGET=Path('/etc/credstore.encrypted/porthole-openai')

def valid_bundle(value):
    if not isinstance(value,dict):return False
    account=value.get('account');password=value.get('password');calendars=value.get('calendars')
    if not isinstance(account,str) or not 3<=len(account)<=254 or '@' not in account or any(c in account for c in ':\r\n\0'):return False
    if not isinstance(password,str) or not re.fullmatch(r'[a-z]{4}(?:-[a-z]{4}){3}',password):return False
    if not isinstance(calendars,list) or not 1<=len(calendars)<=16:return False
    for item in calendars:
        if not isinstance(item,dict) or not isinstance(item.get('url'),str) or not isinstance(item.get('name'),str) or len(item['name'])>100:return False
        url=urlsplit(item['url'])
        if url.scheme!='https' or url.username or url.password or url.fragment or url.port not in (None,443):return False
        if not re.fullmatch(r'(?:caldav|p[0-9]{1,3}-caldav)\.icloud\.com',url.hostname or ''):return False
    return True

def write_bundle(value):
    if not valid_bundle(value):raise ValueError('Invalid account bundle')
    data=json.dumps(value,separators=(',',':')).encode()
    temporary=TARGET.with_name(TARGET.name+'.new')
    try:
        subprocess.run(['/usr/bin/systemd-creds','encrypt','--with-key=host','--name=porthole-icloud','-',str(temporary)],input=data,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=15,check=True)
        os.chmod(temporary,0o600)
        with temporary.open('rb') as f:os.fsync(f.fileno())
        os.replace(temporary,TARGET)
        fd=os.open(TARGET.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(fd)
        finally:os.close(fd)
    finally:
        temporary.unlink(missing_ok=True)
        data=b''

def restart_calendar():
    subprocess.run(['/usr/bin/systemctl','try-restart','porthole-calendar.service'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=20,check=False)

def main():
    os.umask(0o077);allowed=pwd.getpwnam('porthole-calendar').pw_uid
    SOCKET.unlink(missing_ok=True)
    server=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM);server.bind(str(SOCKET));os.chmod(SOCKET,0o660);server.listen(8)
    while True:
        connection,_=server.accept();connection.settimeout(3)
        restart=False
        try:
            _,uid,_=struct.unpack('3i',connection.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12))
            if uid!=allowed:raise PermissionError('Unauthorized peer')
            data=b''
            while b'\n' not in data:
                part=connection.recv(4096)
                if not part:raise ValueError('Incomplete request')
                data+=part
                if len(data)>16384:raise ValueError('Oversize request')
            command=json.loads(data.split(b'\n',1)[0]);kind=command.get('type');reply={'ok':True}
            if kind=='status':reply['remembered']=TARGET.is_file()
            elif kind=='save':write_bundle(command.get('bundle'))
            elif kind=='forgetOpenai':
                OPENAI_TARGET.unlink(missing_ok=True);restart=True
            elif kind in ('forget','clear'):
                TARGET.unlink(missing_ok=True);restart=kind=='forget'
            else:raise ValueError('Unknown operation')
            connection.sendall(json.dumps(reply,separators=(',',':')).encode()+b'\n')
        except Exception:
            try:connection.sendall(b'{"ok":false,"error":"Credential storage failed"}\n')
            except OSError:pass
        finally:connection.close()
        if restart:threading.Timer(.25,restart_calendar).start()

if __name__=='__main__':main()
