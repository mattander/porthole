#!/usr/bin/python3
"""Root-only, fixed-destination USB provisioning. Never logs the key."""
import os,re,resource,signal,subprocess,sys,termios
from pathlib import Path

TARGET=Path('/etc/credstore.encrypted/porthole-openai')

def valid_key(value):
    return re.fullmatch(rb'sk-[A-Za-z0-9_-]{16,500}',value) is not None

def main():
    if os.geteuid()!=0 or not sys.stdin.isatty():
        raise RuntimeError('Use the USB setup command')
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));os.umask(0o077)
    if subprocess.check_output(['/usr/bin/findmnt','-n','-o','SOURCE','/']).strip()!=b'/dev/mmcblk0p2':
        raise RuntimeError('Expected the SD Linux system')
    if Path('/sys/class/block/mmcblk0/device/type').read_text().strip()!='SD':
        raise RuntimeError('Expected the verified SD root')
    if any(p.read_text().strip()=='MMC' for p in Path('/sys/class/block').glob('mmcblk*/device/type')):
        raise RuntimeError('Unexpected internal storage exposed')
    if len(sys.argv)!=2 or not re.fullmatch(r'PORTHOLE_KEY_[a-f0-9]{32}',sys.argv[1]):
        raise RuntimeError('Invalid setup handshake')
    marker=sys.argv[1];old=termios.tcgetattr(sys.stdin.fileno());hidden=old.copy();hidden[3]&=~(termios.ECHO|termios.ECHONL)
    temporary=TARGET.with_name(TARGET.name+'.new');key=b''
    try:
        termios.tcsetattr(sys.stdin.fileno(),termios.TCSANOW,hidden)
        print(marker+'_READY',flush=True)
        # The sender supplies one bounded line, only after the no-echo handshake.
        def timeout(_signum,_frame):raise TimeoutError('Key input timed out')
        signal.signal(signal.SIGALRM,timeout);signal.alarm(30)
        key=sys.stdin.buffer.readline(602).strip();signal.alarm(0)
        if not valid_key(key):raise RuntimeError('Invalid API key format')
        subprocess.run(['/usr/bin/systemd-creds','encrypt','--with-key=host','--name=porthole-openai','-',str(temporary)],input=key,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=15,check=True)
        temporary.chmod(0o600)
        with temporary.open('rb') as f:os.fsync(f.fileno())
        os.replace(temporary,TARGET)
        directory=os.open(TARGET.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(directory)
        finally:os.close(directory)
        key=b''
        result=subprocess.run(['/usr/bin/systemctl','restart','porthole-calendar.service'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30)
        if result.returncode!=0:raise RuntimeError('Key saved; calendar service needs attention')
        print(marker+'_SAVED',flush=True)
    finally:
        key=b'';temporary.unlink(missing_ok=True)
        termios.tcsetattr(sys.stdin.fileno(),termios.TCSANOW,old)

if __name__=='__main__':
    try:main()
    except (Exception,KeyboardInterrupt):
        print('Key setup did not complete. No secret was printed.',flush=True);sys.exit(1)
