#!/usr/bin/env python3
"""Paste a key privately on the Mac; provision the verified SD Linux frame over USB."""
import getpass,os,plistlib,re,resource,select,subprocess,sys,termios,time,uuid,warnings

def frame_port(registry):
    # Follow the matched USB device's children rather than publishing/pinning a
    # serial number or choosing an unrelated modem elsewhere in the registry.
    def nodes(value):
        if isinstance(value,list):
            for child in value:yield from nodes(child)
        elif isinstance(value,dict):
            yield value
            yield from nodes(value.get('IORegistryEntryChildren',[]))
    matched=[node for node in nodes(registry) if node.get('IOObjectClass')=='IOUSBHostDevice'
             and node.get('idVendor')==7531 and node.get('idProduct')==260
             and node.get('USB Product Name')=='PF1007L Linux']
    ports=[node['IOCalloutDevice'] for device in matched for node in nodes(device)
           if isinstance(node.get('IOCalloutDevice'),str)
           and re.fullmatch(r'/dev/cu\.usbmodem[A-Za-z0-9_-]+',node['IOCalloutDevice'])]
    if len(matched)!=1 or len(ports)!=1:raise RuntimeError('Connect exactly one PF1007L frame running SD Linux over USB.')
    return ports[0]

def find_frame():
    raw=subprocess.check_output(['/usr/sbin/ioreg','-p','IOService','-r','-c','IOUSBHostDevice','-a','-l'])
    return frame_port(plistlib.loads(raw))

def send(fd,data):
    deadline=time.monotonic()+5;offset=0
    while offset<len(data):
        if time.monotonic()>deadline:raise RuntimeError('USB write timed out')
        if select.select([],[fd],[],.1)[1]:
            try:offset+=os.write(fd,data[offset:offset+128])
            except BlockingIOError:pass

def wait_marker(fd,marker,seconds):
    # Do not retain or print console output: it can contain unrelated private data.
    deadline=time.monotonic()+seconds;tail=b''
    while time.monotonic()<deadline:
        if select.select([fd],[],[],.2)[0]:
            try:data=os.read(fd,4096)
            except BlockingIOError:continue
            if not data:break
            tail=(tail+data)[-8192:]
            if marker in tail:return
    raise RuntimeError('Frame setup handshake timed out; key status is unconfirmed.')

def main():
    if sys.platform!='darwin' or not sys.stdin.isatty() or len(sys.argv)!=1:
        raise RuntimeError('Run this command directly in your Mac terminal, with no arguments.')
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));port=find_frame()
    with warnings.catch_warnings():
        warnings.simplefilter('error',getpass.GetPassWarning)
        secret=getpass.getpass('Paste the OpenAI API key (hidden): ').strip()
    if not re.fullmatch(r'sk-[A-Za-z0-9_-]{16,500}',secret):raise RuntimeError('That does not look like an OpenAI API key.')
    fd=os.open(port,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK);old=termios.tcgetattr(fd)
    marker=('PORTHOLE_KEY_'+uuid.uuid4().hex).encode('ascii')
    try:
        raw=termios.tcgetattr(fd);raw[0]=0;raw[1]=0;raw[2]=termios.CS8|termios.CREAD|termios.CLOCAL;raw[3]=0;raw[4]=termios.B115200;raw[5]=termios.B115200;raw[6][termios.VMIN]=0;raw[6][termios.VTIME]=0
        termios.tcsetattr(fd,termios.TCSANOW,raw);send(fd,b'\x03\n')
        # Flush only old console bytes; never log them.
        deadline=time.monotonic()+.7
        while time.monotonic()<deadline:
            if select.select([fd],[],[],.05)[0]:
                try:os.read(fd,4096)
                except BlockingIOError:pass
        send(fd,b'sudo /usr/bin/python3 -I /usr/local/lib/porthole/import-openai-key.py '+marker+b'\n')
        wait_marker(fd,marker+b'_READY',15)
        send(fd,secret.encode('ascii')+b'\n');secret=''
        wait_marker(fd,marker+b'_SAVED',45)
        print('API key saved in encrypted storage on the frame. In Settings → AI, enable the briefing and save.')
    finally:
        secret=''
        try:send(fd,b'\x03')
        except Exception:pass
        termios.tcsetattr(fd,termios.TCSANOW,old);os.close(fd)

if __name__=='__main__':
    try:main()
    except (Exception,KeyboardInterrupt) as error:
        # Our own errors are fixed text; never print subprocess exceptions or terminal bytes.
        print(str(error) if isinstance(error,RuntimeError) else 'Setup stopped. No API key was printed.',file=sys.stderr);sys.exit(1)
