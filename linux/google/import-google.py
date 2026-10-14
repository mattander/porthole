#!/usr/bin/python3
"""Root-only hidden USB import into one fixed encrypted credential destination."""
import json,os,re,resource,signal,subprocess,sys,termios
from pathlib import Path
from google_bundle import valid_bundle
TARGET=Path('/etc/credstore.encrypted/porthole-google')

def main():
    if os.geteuid()!=0 or not sys.stdin.isatty():raise RuntimeError('Use the USB setup helper')
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));os.umask(0o077)
    if subprocess.check_output(['/usr/bin/findmnt','-n','-o','SOURCE','/']).strip()!=b'/dev/mmcblk0p2' or Path('/sys/class/block/mmcblk0/device/type').read_text().strip()!='SD':raise RuntimeError('Expected SD Linux')
    if any(p.read_text().strip()=='MMC' for p in Path('/sys/class/block').glob('mmcblk*/device/type')):raise RuntimeError('Unexpected internal storage')
    if len(sys.argv)!=2 or not re.fullmatch(r'PORTHOLE_GOOGLE_[a-f0-9]{32}',sys.argv[1]):raise RuntimeError('Invalid handshake')
    marker=sys.argv[1];old=termios.tcgetattr(sys.stdin.fileno());hidden=old.copy();hidden[3]&=~(termios.ECHO|termios.ECHONL);temporary=TARGET.with_name(TARGET.name+'.new')
    try:
        termios.tcsetattr(sys.stdin.fileno(),termios.TCSANOW,hidden);print(marker+'_READY',flush=True)
        def timeout(_signum,_frame):raise TimeoutError('Setup timed out')
        signal.signal(signal.SIGALRM,timeout);signal.alarm(30)
        data=sys.stdin.buffer.readline(8193);signal.alarm(0)
        if len(data)>8192 or not valid_bundle(json.loads(data)):raise RuntimeError('Invalid credential bundle')
        subprocess.run(['/usr/bin/systemd-creds','encrypt','--with-key=host','--name=porthole-google','-',str(temporary)],input=data,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=15,check=True);data=b''
        temporary.chmod(0o600)
        with temporary.open('rb') as f:os.fsync(f.fileno())
        os.replace(temporary,TARGET);fd=os.open(TARGET.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(fd)
        finally:os.close(fd)
        if subprocess.run(['/usr/bin/systemctl','restart','porthole-calendar.service'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30).returncode:raise RuntimeError('Service restart failed')
        print(marker+'_SAVED',flush=True)
    finally:
        temporary.unlink(missing_ok=True);termios.tcsetattr(sys.stdin.fileno(),termios.TCSANOW,old)
if __name__=='__main__':
    try:main()
    except (Exception,KeyboardInterrupt):print('Google setup did not complete. No credential was printed.',flush=True);sys.exit(1)
