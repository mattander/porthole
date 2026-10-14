#!/usr/bin/python3
"""Local-only privileged installer. Accepts fixed operations, never caller paths/code."""
import concurrent.futures,datetime,json,os,pwd,socket,struct,subprocess,sys,threading,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from update_common import FILES,KERNEL,MAX_ARCHIVE,MAX_MANIFEST,VERSION,Rejected,atomic,parse_json,read_regular,save_json,sha,unpack,verify_signature

STATE=Path('/var/lib/porthole-updater')
CACHE=Path('/var/lib/porthole-update-cache')
BIN=Path('/usr/local/bin')
PUBLIC=Path('/etc/porthole/update-public.pem')
SOCKET='/run/porthole-update/socket'
REASONS={'network','signature','archive','platform','storage','health','rollback'}
PHASES={'idle','checking','up-to-date','installing','installed','rolled-back','blocked','unavailable'}

def utc():return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')
def command(args,timeout=15):return subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,timeout=timeout,check=True).stdout

def guard():
    if command(['/usr/bin/findmnt','-n','-o','SOURCE','/']).strip()!=b'/dev/mmcblk0p2':raise Rejected('platform')
    if Path('/sys/class/block/mmcblk0/device/type').read_text().strip()!='SD':raise Rejected('platform')
    if any(p.read_text().strip()=='MMC' for p in Path('/sys/class/block').glob('mmcblk*/device/type')):raise Rejected('platform')
    if command(['/usr/bin/uname','-r']).strip().decode()!=KERNEL:raise Rejected('platform')

class Transaction:
    """Durable two-file transaction; reboot recovery restores the trusted local backup."""
    def __init__(self,state=STATE,binaries=BIN):self.state=Path(state);self.binaries=Path(binaries)
    def prepare(self,files,release):
        previous=self.state/'previous';previous.mkdir(mode=0o700,exist_ok=True);hashes={}
        for name in FILES:
            data=read_regular(self.binaries/name,MAX_ARCHIVE);atomic(previous/name,data,0o700);hashes[name]=sha(data)
        save_json(self.state/'pending.json',{'hashes':hashes,'release':release})
        self.replace(files)
    def replace(self,files):
        for name in FILES:atomic(self.binaries/name,files[name],0o755)
    def recover(self):
        pending=self.state/'pending.json'
        if not pending.exists():return False
        value=parse_json(read_regular(pending,MAX_MANIFEST));hashes=value.get('hashes')
        if not isinstance(hashes,dict) or set(hashes)!=set(FILES):raise Rejected('rollback')
        files={name:read_regular(self.state/'previous'/name,MAX_ARCHIVE) for name in FILES}
        if any(sha(files[name])!=hashes[name] for name in FILES):raise Rejected('rollback')
        release=value.get('release')
        if not isinstance(release,dict) or set(release)!={'version','sequence'} or not isinstance(release['version'],str) or type(release['sequence']) is not int:raise Rejected('rollback')
        self.replace(files);return release
    def commit(self):
        (self.state/'pending.json').unlink(missing_ok=True)
        fd=os.open(self.state,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(fd)
        finally:os.close(fd)

class Installer:
    def __init__(self):
        guard();self.lock=threading.RLock();self.busy=False;self.manual=False
        self.gui_uid=pwd.getpwnam('porthole').pw_uid;self.fetch_uid=pwd.getpwnam('porthole-update').pw_uid
        self.value={'schema':1,'automatic':True,'version':'local','sequence':0,'highest':0,'phase':'idle','reason':'','checked':''}
        if (STATE/'state.json').exists():
            value=parse_json(read_regular(STATE/'state.json',MAX_MANIFEST))
            if set(value)!=set(self.value) or value['schema']!=1 or type(value['automatic']) is not bool or type(value['sequence']) is not int or type(value['highest']) is not int or not 0<=value['sequence']<=value['highest'] or value['phase'] not in PHASES or value['reason'] not in REASONS|{''}:raise Rejected('storage')
            if not isinstance(value['version'],str) or (value['version']!='local' and not VERSION.fullmatch(value['version'])) or not isinstance(value['checked'],str) or value['highest']>9007199254740991:raise Rejected('storage')
            if value['checked'] and datetime.datetime.fromisoformat(value['checked']).tzinfo is None:raise Rejected('storage')
            self.value=value
        transaction=Transaction();recovered=transaction.recover()
        if recovered:self.value.update(recovered);self.value['phase']='rolled-back';self.value['reason']='health';self.persist();transaction.commit()
        elif self.value['phase'] in {'checking','installing'}:self.value['phase']='idle';self.persist()
    def persist(self):save_json(STATE/'state.json',self.value)
    def status(self):
        with self.lock:return {'ok':True,**{k:self.value[k] for k in ['automatic','version','phase','reason','checked']}}
    def dispatch(self,uid,request):
        kind=request.get('type')
        if uid==self.gui_uid:
            if kind=='status' and set(request)=={'type'}:return self.status()
            if kind=='automatic' and set(request)=={'type','enabled'} and type(request['enabled']) is bool:
                with self.lock:self.value['automatic']=request['enabled'];self.persist()
                return self.status()
            if kind=='check' and set(request)=={'type'}:
                with self.lock:
                    if self.busy or self.value['phase']=='checking':return self.status()
                    checked=self.value['checked']
                    if checked and (datetime.datetime.now(datetime.timezone.utc)-datetime.datetime.fromisoformat(checked)).total_seconds()<60:return self.status()
                    self.manual=True
                command(['/usr/bin/systemctl','start','--no-block','porthole-update-check.service'])
                return self.status()
        if uid==self.fetch_uid:
            if kind=='begin' and set(request)=={'type'}:
                with self.lock:
                    allowed=not self.busy and (self.value['automatic'] or self.manual);self.manual=False
                    if allowed:self.value['phase']='checking';self.value['reason']='';self.value['checked']=utc();self.persist()
                    return {'ok':True,'allowed':allowed,'highest':self.value['highest']}
            if kind=='current' and set(request)=={'type'}:
                with self.lock:
                    self.value['phase']='up-to-date' if self.value['sequence']==self.value['highest'] else 'blocked';self.persist()
                return self.status()
            if kind=='failure' and set(request)=={'type','reason'} and request['reason'] in REASONS:
                with self.lock:
                    if not self.busy:self.value['phase']='unavailable';self.value['reason']=request['reason'];self.persist()
                return self.status()
            if kind=='candidate' and set(request)=={'type'}:return self.apply()
        return {'ok':False}
    def apply(self):
        with self.lock:
            if self.busy:return {'ok':False}
            self.busy=True
        transaction=Transaction();stopped=False
        try:
            guard()
            raw=read_regular(CACHE/'manifest.json',MAX_MANIFEST);sig=read_regular(CACHE/'manifest.sig',1024)
            manifest=verify_signature(raw,sig,PUBLIC);files=unpack(manifest,read_regular(CACHE/'app.tar.gz',MAX_ARCHIVE))
            with self.lock:
                if manifest.sequence<=self.value['highest']:raise Rejected('archive')
                self.value['highest']=manifest.sequence;self.value['phase']='installing';self.value['reason']='';self.persist()
            # Execute candidate checks only as the existing unprivileged app users.
            staged=[]
            try:
                for name in FILES:
                    path=BIN/('.'+name+'-candidate');atomic(path,files[name],0o755);staged.append(path)
                    user='porthole' if name=='porthole' else 'porthole-calendar'
                    command(['/usr/bin/setpriv','--reuid',user,'--regid','porthole','--init-groups','--no-new-privs','--',str(path),'--self-check'])
            finally:
                for path in staged:path.unlink(missing_ok=True)
            command(['/usr/bin/systemctl','stop','porthole-kiosk','porthole-calendar']);stopped=True
            transaction.prepare(files,{k:self.value[k] for k in ['version','sequence']})
            command(['/usr/bin/systemctl','start','porthole-calendar','porthole-kiosk','porthole-orientation'])
            if not self.health():raise Rejected('health')
            with self.lock:
                self.value['version']=manifest.version;self.value['sequence']=manifest.sequence;self.value['phase']='installed';self.value['reason']='';self.persist()
            transaction.commit()
            return {'ok':True}
        except Exception as error:
            # A pending journal, not an exception's arbitrary text, determines recovery.
            if (STATE/'pending.json').exists():
                command(['/usr/bin/systemctl','stop','porthole-kiosk','porthole-calendar'])
                recovered=transaction.recover()
                with self.lock:self.value.update(recovered);self.value['phase']='rolled-back';self.value['reason']='health';self.persist()
                transaction.commit()
            else:
                with self.lock:self.value['phase']='blocked';self.value['reason']=error.code if isinstance(error,Rejected) else 'storage';self.persist()
            if stopped:command(['/usr/bin/systemctl','start','porthole-calendar','porthole-kiosk','porthole-orientation'])
            return {'ok':True}
        finally:
            with self.lock:self.busy=False
    def health(self):
        # A visible Qt window and responsive local backend establish process health.
        script="import json,socket; s=socket.socket(socket.AF_UNIX);s.settimeout(2);s.connect('/run/porthole-calendar/socket');s.sendall(b'{\"type\":\"ping\"}\\n');b=b''\nwhile b'\\n' not in b:\n chunk=s.recv(65536)\n if not chunk:raise ValueError()\n b+=chunk\n if len(b)>1048576:raise ValueError()\nv=json.loads(b.split(b'\\n')[0]);assert v['ok'] and v['instance']"
        deadline=time.monotonic()+35;stable=0;last_pid=None
        while time.monotonic()<deadline:
            try:
                for unit in ['porthole-calendar','porthole-kiosk','porthole-orientation']:command(['/usr/bin/systemctl','is-active','--quiet',unit])
                command(['/usr/bin/setpriv','--reuid','porthole','--regid','porthole','--init-groups','--no-new-privs','--','/usr/bin/python3','-I','-c',script],timeout=4)
                sockets=list(Path('/run/porthole-kiosk').glob('sway-ipc.*.sock'))
                if len(sockets)!=1:raise Rejected('health')
                tree=parse_json(command(['/usr/bin/setpriv','--reuid','porthole','--regid','porthole','--init-groups','--no-new-privs','--','/usr/bin/env','SWAYSOCK='+str(sockets[0]),'/usr/bin/swaymsg','-t','get_tree','-r'],timeout=4),limit=2*1024*1024)
                def windows(node):
                    if node.get('app_id')=='porthole':yield node
                    for child in node.get('nodes',[])+node.get('floating_nodes',[]):yield from windows(child)
                visible=[w for w in windows(tree) if w.get('visible') and w.get('rect',{}).get('width',0)>0]
                if len(visible)!=1:raise Rejected('health')
                pid=visible[0].get('pid')
                if type(pid) is not int:raise Rejected('health')
                executable=command(['/usr/bin/setpriv','--reuid','porthole','--regid','porthole','--init-groups','--no-new-privs','--','/usr/bin/readlink','/proc/'+str(pid)+'/exe']).strip().decode()
                if executable!=str(BIN/'porthole'):raise Rejected('health')
                stable=stable+1 if pid==last_pid else 1;last_pid=pid
                if stable>=5:return True
            except Exception:stable=0
            time.sleep(2)
        return False

def handle(client,installer):
    with client:
        client.settimeout(3)
        try:
            uid=struct.unpack('3i',client.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12))[1]
            if uid not in (installer.gui_uid,installer.fetch_uid):client.sendall(b'{"ok":false}\n');return
            data=b''
            while b'\n' not in data:
                chunk=client.recv(4096)
                if not chunk or len(data)+len(chunk)>4096:raise Rejected('storage')
                data+=chunk
            reply=installer.dispatch(uid,parse_json(data.split(b'\n')[0],4096))
        except Exception:reply={'ok':False}
        try:client.sendall(json.dumps(reply).encode()+b'\n')
        except OSError:pass

def main():
    if os.geteuid()!=0:raise Rejected('storage')
    os.umask(0o077);installer=Installer()
    Path(SOCKET).unlink(missing_ok=True)
    with socket.socket(socket.AF_UNIX) as server:
        server.bind(SOCKET);os.chmod(SOCKET,0o660);server.listen(8)
        notify=os.environ.get('NOTIFY_SOCKET')
        if notify:
            with socket.socket(socket.AF_UNIX,socket.SOCK_DGRAM) as n:n.connect(('\0'+notify[1:]) if notify.startswith('@') else notify);n.sendall(b'READY=1')
        command(['/usr/bin/systemctl','start','--no-block','porthole-calendar','porthole-kiosk','porthole-orientation'])
        slots=threading.BoundedSemaphore(8)
        def bounded(client):
            try:handle(client,installer)
            finally:slots.release()
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            while True:
                client,_=server.accept()
                if slots.acquire(blocking=False):pool.submit(bounded,client)
                else:client.close()

if __name__=='__main__':
    try:main()
    except Exception:sys.exit(1)
