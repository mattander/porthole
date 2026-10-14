#!/usr/bin/python3
"""Unprivileged outbound-only downloader. Root re-verifies the candidate independently."""
import json,socket,sys,time,urllib.error,urllib.request
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from update_common import MAX_ARCHIVE,MAX_MANIFEST,Rejected,Manifest,atomic,load_config,safe_url,unpack,verify_signature

CACHE=Path('/var/lib/porthole-update-cache')
SOCKET='/run/porthole-update/socket'

def ipc(value,timeout=5):
    with socket.socket(socket.AF_UNIX) as s:
        s.settimeout(timeout);s.connect(SOCKET);s.sendall(json.dumps(value).encode()+b'\n');data=b''
        while b'\n' not in data:
            chunk=s.recv(4096)
            if not chunk or len(data)+len(chunk)>8192:raise Rejected('storage')
            data+=chunk
        return json.loads(data.split(b'\n')[0])

class Redirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,req,fp,code,msg,headers,url):
        safe_url(url)
        return super().redirect_request(req,fp,code,msg,headers,url)

def download(url,maximum):
    request=urllib.request.Request(safe_url(url),headers={'User-Agent':'Porthole-signed-updater/1','Accept':'application/octet-stream'})
    deadline=time.monotonic()+45
    with urllib.request.build_opener(Redirect()).open(request,timeout=15) as response:
        chunks=[];size=0
        while True:
            block=response.read(min(65536,maximum-size+1));size+=len(block)
            if size>maximum or time.monotonic()>deadline:raise Rejected('network')
            if not block:break
            chunks.append(block)
        return b''.join(chunks)

def main():
    begin=ipc({'type':'begin'})
    if not begin.get('allowed'):return
    try:
        repository=load_config('/etc/porthole/update.json');base='https://github.com/'+repository+'/releases/'
        raw=download(base+'latest/download/porthole-manifest.json',MAX_MANIFEST)
        hint=Manifest.parse(raw);version_base=base+'download/v'+hint.version+'/'
        signature=download(version_base+'porthole-manifest.sig',1024)
        manifest=verify_signature(raw,signature,'/etc/porthole/update-public.pem')
        if manifest.sequence<=begin['highest']:
            ipc({'type':'current'});return
        archive=download(version_base+'porthole-armv7.tar.gz',manifest.archive_size)
        unpack(manifest,archive)
        for name,data in [('manifest.json',raw),('manifest.sig',signature),('app.tar.gz',archive)]:atomic(CACHE/name,data)
        response=ipc({'type':'candidate'},timeout=180)
        if not response.get('ok'):raise Rejected('storage')
    except Rejected as error:ipc({'type':'failure','reason':error.code})
    except (OSError,ValueError,urllib.error.URLError):ipc({'type':'failure','reason':'network'})

if __name__=='__main__':
    try:main()
    except Exception:sys.exit(1) # No raw network URLs, registry values or response bodies in logs.
