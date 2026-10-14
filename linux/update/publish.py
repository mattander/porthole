#!/usr/bin/env python3
"""Create a signed, app-only release. The private key is never uploaded."""
import argparse,gzip,io,json,os,subprocess,sys,tarfile,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from update_common import FILES,KERNEL,PLATFORM,REPOSITORY,VERSION,Manifest,Rejected,arm_binary,sha,unpack

def bundle(directory,version,sequence,output,key):
    if not VERSION.fullmatch(version) or not 1<=sequence<=9007199254740991:raise ValueError('Use a numeric three/four-part version and positive sequence.')
    output.mkdir(mode=0o700,parents=True,exist_ok=False);files={};raw=io.BytesIO()
    with tarfile.open(fileobj=raw,mode='w',format=tarfile.USTAR_FORMAT) as archive:
        for name in FILES:
            data=(directory/name).read_bytes()
            if not arm_binary(data):raise ValueError('Release binaries must be little-endian ARMv7 ELF programs.')
            files[name]=sha(data);info=tarfile.TarInfo(name);info.size=len(data);info.mode=0o755;info.mtime=0;archive.addfile(info,io.BytesIO(data))
    compressed=gzip.compress(raw.getvalue(),mtime=0)
    manifest={'schema':1,'sequence':sequence,'version':version,'platform':PLATFORM,'kernel':KERNEL,'archive_size':len(compressed),'archive_sha256':sha(compressed),'files':files}
    encoded=(json.dumps(manifest,sort_keys=True,separators=(',',':'))+'\n').encode();unpack(Manifest.parse(encoded),compressed)
    (output/'porthole-manifest.json').write_bytes(encoded);(output/'porthole-armv7.tar.gz').write_bytes(compressed)
    subprocess.run(['openssl','dgst','-sha256','-sign',str(key),'-out',str(output/'porthole-manifest.sig'),str(output/'porthole-manifest.json')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    (output/'release-notes.md').write_text('Signed app-only ARMv7 release for the verified PF1007L SD Linux platform. Includes the native dashboard and read-only calendar backend. Preserves household configuration, credentials, Wi-Fi profiles and the briefing cache/spending record. Kernel, bootloader, drivers and internal storage are outside this update channel.\n')
    return manifest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    keygen=sub.add_parser('keygen',help='Create a local RSA-3072 signing key and public verification key.')
    keygen.add_argument('--directory',type=Path,required=True)
    release=sub.add_parser('release',help='Build/sign a release; optionally publish its three assets with gh.')
    release.add_argument('--binaries',type=Path,required=True);release.add_argument('--key',type=Path,required=True)
    release.add_argument('--version',required=True);release.add_argument('--sequence',type=int,default=None)
    release.add_argument('--output',type=Path,required=True);release.add_argument('--publish',action='store_true')
    release.add_argument('--repo');release.add_argument('--target',default='main')
    args=parser.parse_args();os.umask(0o077)
    if args.command=='keygen':
        args.directory.mkdir(mode=0o700,parents=True,exist_ok=False)
        private=args.directory/'signing-private.pem';public=args.directory/'signing-public.pem'
        subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:3072','-out',str(private)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        subprocess.run(['openssl','pkey','-in',str(private),'-pubout','-out',str(public)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        private.chmod(0o600);print('Local signing key created. Back up the private key securely; distribute only the public key.');return
    if args.publish and (not args.repo or not REPOSITORY.fullmatch(args.repo)):parser.error('--publish requires --repo OWNER/REPO')
    if not args.key.is_file() or args.key.stat().st_mode&0o077:parser.error('The signing key must be a private file with mode 0600 or stricter.')
    bundle(args.binaries,args.version,args.sequence if args.sequence is not None else int(time.time()),args.output,args.key)
    print('Signed release assembled. No profiles, credentials, install scripts or firmware are included.')
    if args.publish:
        assets=[str(args.output/name) for name in ['porthole-manifest.json','porthole-manifest.sig','porthole-armv7.tar.gz']]
        subprocess.run(['gh','release','create','v'+args.version,*assets,'--repo',args.repo,'--target',args.target,'--latest','--title','Porthole '+args.version,'--notes-file',str(args.output/'release-notes.md')],check=True)

if __name__=='__main__':main()
