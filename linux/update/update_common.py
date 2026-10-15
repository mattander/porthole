"""Bounded, signed app-release format. Shared by publisher, fetcher and installer."""
import gzip,hashlib,io,json,os,re,stat,subprocess,tarfile,tempfile,urllib.parse,zlib
from dataclasses import dataclass
from pathlib import Path

MAX_MANIFEST=8192
MAX_ARCHIVE=8*1024*1024
MAX_UNPACKED=16*1024*1024
FILES=('porthole','porthole-calendar')
PLATFORM='pf1007l-arch-armv7'
KERNEL='7.2.10-1-armv7-ARCH'
VERSION=re.compile(r'[0-9]{1,4}(?:\.[0-9]{1,4}){2,3}')
REPOSITORY=re.compile(r'[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9][A-Za-z0-9_.-]{0,99}')
HOSTS={'github.com','release-assets.githubusercontent.com','objects.githubusercontent.com'}

class Rejected(Exception):
    def __init__(self,code):super().__init__(code);self.code=code

def pairs(items):
    result={}
    for k,v in items:
        if k in result:raise Rejected('archive')
        result[k]=v
    return result

def parse_json(raw,limit=MAX_MANIFEST):
    if len(raw)>limit:raise Rejected('archive')
    try:value=json.loads(raw,object_pairs_hook=pairs)
    except (ValueError,UnicodeError):raise Rejected('archive') from None
    if not isinstance(value,dict):raise Rejected('archive')
    return value

def sha(data):return hashlib.sha256(data).hexdigest()
def digest(value):return isinstance(value,str) and re.fullmatch(r'[0-9a-f]{64}',value) is not None

@dataclass(frozen=True)
class Manifest:
    sequence:int
    version:str
    archive_size:int
    archive_sha256:str
    files:dict[str,str]
    schema:int=1
    @classmethod
    def parse(cls,raw):
        v=parse_json(raw)
        if set(v)!={'schema','sequence','version','platform','kernel','archive_size','archive_sha256','files'} or type(v['schema']) is not int or v['schema'] not in (1,2):raise Rejected('archive')
        if v['platform']!=PLATFORM or v['kernel']!=KERNEL:raise Rejected('platform')
        if type(v['sequence']) is not int or not 1<=v['sequence']<=9007199254740991:raise Rejected('archive')
        if not isinstance(v['version'],str) or not VERSION.fullmatch(v['version']):raise Rejected('archive')
        if type(v['archive_size']) is not int or not 1<=v['archive_size']<=MAX_ARCHIVE or not digest(v['archive_sha256']):raise Rejected('archive')
        expected=set(FILES) if v['schema']==1 else set(FILES)|{'migrations.json'}
        if not isinstance(v['files'],dict) or set(v['files'])!=expected or not all(digest(x) for x in v['files'].values()):raise Rejected('archive')
        return cls(v['sequence'],v['version'],v['archive_size'],v['archive_sha256'],v['files'],v['schema'])

def verify_signature(raw,signature,public_key):
    if len(raw)>MAX_MANIFEST or not 256<=len(signature)<=1024:raise Rejected('signature')
    with tempfile.TemporaryDirectory(prefix='porthole-signature-') as d:
        p=Path(d);(p/'manifest').write_bytes(raw);(p/'signature').write_bytes(signature)
        result=subprocess.run(['openssl','dgst','-sha256','-verify',str(public_key),'-signature',str(p/'signature'),str(p/'manifest')],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=10)
        if result.returncode:raise Rejected('signature')
    return Manifest.parse(raw)

def arm_binary(data):
    return len(data)>=52 and data[:6]==b'\x7fELF\x01\x01' and int.from_bytes(data[16:18],'little') in (2,3) and int.from_bytes(data[18:20],'little')==40

def unpack(manifest,archive):
    if len(archive)!=manifest.archive_size or sha(archive)!=manifest.archive_sha256:raise Rejected('archive')
    try:
        inflater=zlib.decompressobj(31);raw=inflater.decompress(archive,MAX_UNPACKED+1)
        if len(raw)>MAX_UNPACKED or not inflater.eof or inflater.unused_data or inflater.unconsumed_tail:raise Rejected('archive')
        result={}
        with tarfile.open(fileobj=io.BytesIO(raw),mode='r:') as tar:
            for item in tar:
                if item.name not in manifest.files or item.name in result or not item.isreg() or item.pax_headers or item.sparse:raise Rejected('archive')
                maximum=4096 if item.name=='migrations.json' else MAX_ARCHIVE
                minimum=1 if item.name=='migrations.json' else 52
                if not minimum<=item.size<=maximum:raise Rejected('archive')
                stream=tar.extractfile(item)
                if stream is None:raise Rejected('archive')
                data=stream.read(MAX_ARCHIVE+1)
                if len(data)!=item.size or sha(data)!=manifest.files[item.name]:raise Rejected('archive')
                if item.name=='migrations.json':
                    from migrations import CredentialPlan
                    CredentialPlan.parse(data)
                elif not arm_binary(data):raise Rejected('archive')
                result[item.name]=data
        if set(result)!=set(manifest.files):raise Rejected('archive')
        return result
    except (tarfile.TarError,zlib.error,EOFError,ValueError):raise Rejected('archive') from None

def safe_url(url):
    u=urllib.parse.urlsplit(url)
    try:port=u.port
    except ValueError:raise Rejected('network') from None
    if u.scheme!='https' or u.hostname not in HOSTS or u.username or u.password or u.fragment or port not in (None,443):raise Rejected('network')
    return url

def read_regular(path,limit):
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        info=os.fstat(fd)
        if not stat.S_ISREG(info.st_mode) or info.st_size>limit:raise Rejected('archive')
        with os.fdopen(fd,'rb',closefd=False) as f:data=f.read(limit+1)
        if len(data)>limit:raise Rejected('archive')
        return data
    finally:os.close(fd)

def atomic(path,data,mode=0o600):
    path=Path(path);fd,temp=tempfile.mkstemp(prefix='.'+path.name+'-',dir=path.parent)
    try:
        os.fchmod(fd,mode)
        with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        os.replace(temp,path)
        parent=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(parent)
        finally:os.close(parent)
    finally:
        if os.path.exists(temp):os.unlink(temp)

def save_json(path,value):atomic(path,(json.dumps(value,sort_keys=True,separators=(',',':'))+'\n').encode())

def load_config(path):
    v=parse_json(read_regular(path,MAX_MANIFEST))
    if set(v)!={'repository'} or not isinstance(v['repository'],str) or not REPOSITORY.fullmatch(v['repository']):raise Rejected('storage')
    return v['repository']
