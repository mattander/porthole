#!/usr/bin/env python3
"""Create a signed app release, optionally with declarative credential migrations."""
import argparse,gzip,io,json,os,shutil,subprocess,sys,tarfile,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from update_common import FILES,KERNEL,PLATFORM,REPOSITORY,VERSION,Manifest,Rejected,arm_binary,sha,unpack
from migrations import CredentialPlan,MAX_PLAN
from update_common import read_regular

def bundle(directory,version,sequence,output,key,migrations=None):
    if not VERSION.fullmatch(version) or not 1<=sequence<=9007199254740991:raise ValueError('Use a numeric three/four-part version and positive sequence.')
    plan=CredentialPlan.parse(read_regular(migrations,MAX_PLAN)) if migrations is not None else None
    output.mkdir(mode=0o700,parents=True,exist_ok=False);files={};raw=io.BytesIO()
    with tarfile.open(fileobj=raw,mode='w',format=tarfile.USTAR_FORMAT) as archive:
        for name in FILES:
            data=(directory/name).read_bytes()
            if not arm_binary(data):raise ValueError('Release binaries must be little-endian ARMv7 ELF programs.')
            files[name]=sha(data);info=tarfile.TarInfo(name);info.size=len(data);info.mode=0o755;info.mtime=0;archive.addfile(info,io.BytesIO(data))
        if plan is not None:
            data=(json.dumps({'schema':1,'add_credentials':list(plan.names)},sort_keys=True,separators=(',',':'))+'\n').encode()
            files['migrations.json']=sha(data);info=tarfile.TarInfo('migrations.json');info.size=len(data);info.mode=0o644;info.mtime=0;archive.addfile(info,io.BytesIO(data))
    compressed=gzip.compress(raw.getvalue(),mtime=0)
    manifest={'schema':2 if plan is not None else 1,'sequence':sequence,'version':version,'platform':PLATFORM,'kernel':KERNEL,'archive_size':len(compressed),'archive_sha256':sha(compressed),'files':files}
    encoded=(json.dumps(manifest,sort_keys=True,separators=(',',':'))+'\n').encode();unpack(Manifest.parse(encoded),compressed)
    (output/'porthole-manifest.json').write_bytes(encoded);(output/'porthole-armv7.tar.gz').write_bytes(compressed)
    subprocess.run(['openssl','dgst','-sha256','-sign',str(key),'-out',str(output/'porthole-manifest.sig'),str(output/'porthole-manifest.json')],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    (output/'release-notes.md').write_text('Signed ARMv7 app release for the verified PF1007L SD Linux platform. Includes the native dashboard and read-only calendar backend. Preserves household configuration, secret credentials, Wi-Fi profiles and the briefing cache/spending record. Kernel, bootloader, drivers and internal storage are outside this update channel.\n')
    if plan is not None:
        with (output/'release-notes.md').open('a') as notes:
            notes.write('\nIncludes a signed, additive systemd credential-delivery migration. Requires the once-only migration bootstrap; older installers reject this release without replacing the app. No secret values or executable migration scripts are included.\n')
    return manifest

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest='command',required=True)
    keygen=sub.add_parser('keygen',help='Create a local RSA-3072 signing key and public verification key.')
    keygen.add_argument('--directory',type=Path,required=True)
    migration=sub.add_parser('migration-plan',help='Create a public, declarative credential-support plan; never supply secret values.')
    migration.add_argument('--add-credential',action='append',required=True,metavar='porthole-NAME')
    migration.add_argument('--output',type=Path,required=True)
    bootstrap=sub.add_parser('migration-bootstrap',help='Stage the once-only trusted upgrade for an existing frame updater.')
    bootstrap.add_argument('--output',type=Path,required=True)
    release=sub.add_parser('release',help='Build/sign a release; optionally publish its three assets with gh.')
    release.add_argument('--binaries',type=Path,required=True);release.add_argument('--key',type=Path,required=True)
    release.add_argument('--version',required=True);release.add_argument('--sequence',type=int,default=None)
    release.add_argument('--output',type=Path,required=True);release.add_argument('--publish',action='store_true')
    release.add_argument('--migrations',type=Path,help='Signed credential-support plan. Requires migration bootstrap on the frame.')
    release.add_argument('--repo');release.add_argument('--target',default='main')
    args=parser.parse_args();os.umask(0o077)
    if args.command=='migration-bootstrap':
        try:
            args.output.mkdir(mode=0o700,parents=True,exist_ok=False)
            source=Path(__file__).resolve().parent
            names=('update_common.py','update_fetch.py','update_service.py','migrations.py','porthole-update.service','install-migrations.sh')
            for name in names:shutil.copyfile(source/name,args.output/name)
            (args.output/'files.sha256').write_text(''.join(sha((args.output/name).read_bytes())+'  '+name+'\n' for name in names))
        except OSError:parser.error('Could not stage the bootstrap. Use a fresh output directory.')
        print('Migration bootstrap staged. Transfer through trusted maintenance and run bash install-migrations.sh as root.');return
    if args.command=='migration-plan':
        try:
            plan=CredentialPlan.parse(json.dumps({'schema':1,'add_credentials':args.add_credential}).encode())
            with args.output.open('x') as output:json.dump({'schema':1,'add_credentials':list(plan.names)},output,sort_keys=True);output.write('\n')
        except Rejected:parser.error('Use 1–16 unique credential names matching porthole-[a-z][a-z0-9-]{0,47}; no paths or values.')
        except OSError:parser.error('Could not create the plan. Use an existing directory and a fresh output filename.')
        print('Credential-support plan created. No secret values are included.');return
    if args.command=='keygen':
        args.directory.mkdir(mode=0o700,parents=True,exist_ok=False)
        private=args.directory/'signing-private.pem';public=args.directory/'signing-public.pem'
        subprocess.run(['openssl','genpkey','-algorithm','RSA','-pkeyopt','rsa_keygen_bits:3072','-out',str(private)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        subprocess.run(['openssl','pkey','-in',str(private),'-pubout','-out',str(public)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        private.chmod(0o600);print('Local signing key created. Back up the private key securely; distribute only the public key.');return
    if args.publish and (not args.repo or not REPOSITORY.fullmatch(args.repo)):parser.error('--publish requires --repo OWNER/REPO')
    if not args.key.is_file() or args.key.stat().st_mode&0o077:parser.error('The signing key must be a private file with mode 0600 or stricter.')
    try:bundle(args.binaries,args.version,args.sequence if args.sequence is not None else int(time.time()),args.output,args.key,args.migrations)
    except (Rejected,ValueError,OSError):parser.error('Release assembly failed. Check the binaries, plan, version, sequence and fresh output directory.')
    print('Signed release assembled. No profiles, secret credentials, install scripts or firmware are included.')
    if args.publish:
        assets=[str(args.output/name) for name in ['porthole-manifest.json','porthole-manifest.sig','porthole-armv7.tar.gz']]
        subprocess.run(['gh','release','create','v'+args.version,*assets,'--repo',args.repo,'--target',args.target,'--latest','--title','Porthole '+args.version,'--notes-file',str(args.output/'release-notes.md')],check=True)

if __name__=='__main__':main()
