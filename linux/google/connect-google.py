#!/usr/bin/env python3
"""Mac browser OAuth/PKCE, then hidden USB transfer. Never writes tokens to disk."""
import argparse,base64,hashlib,hmac,json,os,resource,secrets,select,sys,termios,time,urllib.parse,urllib.request,uuid,webbrowser
from http.server import BaseHTTPRequestHandler,HTTPServer
from pathlib import Path
from google_bundle import valid_bundle
from usb_transport import find_frame,send,wait_marker
SCOPE='https://www.googleapis.com/auth/calendar.readonly'

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs):return None

def read_client(path):
    raw=Path(path).read_bytes()
    if len(raw)>16384:raise RuntimeError('OAuth client JSON is too large.')
    value=json.loads(raw).get('installed')
    if not isinstance(value,dict) or not valid_bundle({'client_id':value.get('client_id'),'client_secret':value.get('client_secret'),'refresh_token':'validation'}):raise RuntimeError('Download a Desktop app OAuth client JSON.')
    return value['client_id'],value['client_secret']

def callback(query,state):
    values=urllib.parse.parse_qs(query,keep_blank_values=True)
    if set(values)-{'code','state','scope','authuser','prompt','error','error_description','hd','iss'} or len(values.get('state',[]))!=1 or not hmac.compare_digest(values['state'][0],state):return None
    if 'iss' in values and values['iss']!=['https://accounts.google.com']:return None
    if 'error' in values:return ('denied','')
    code=values.get('code',[])
    if len(code)!=1 or not 1<=len(code[0])<=4096:return None
    return ('code',code[0])

def authorize(client_id,client_secret):
    state=secrets.token_urlsafe(32);verifier=secrets.token_urlsafe(64)
    challenge=base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b'=').decode();result=[]
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def do_GET(self):
            parsed=urllib.parse.urlsplit(self.path);host=self.headers.get('Host')
            item=callback(parsed.query,state) if parsed.path=='/' and host=='127.0.0.1:'+str(self.server.server_port) and len(self.path)<=8192 else None
            self.send_response(200 if item else 400);self.send_header('Content-Type','text/plain; charset=utf-8');self.send_header('Cache-Control','no-store');self.send_header('Content-Security-Policy',"default-src 'none'");self.end_headers()
            self.wfile.write(b'Return to your terminal. You can close this tab.' if item else b'Invalid sign-in response.')
            if item:result.append(item)
    class BoundedServer(HTTPServer):
        def get_request(self):
            connection,address=super().get_request();connection.settimeout(3);return connection,address
    with BoundedServer(('127.0.0.1',0),Handler) as server:
        server.timeout=1;redirect='http://127.0.0.1:'+str(server.server_port)+'/'
        query=urllib.parse.urlencode({'client_id':client_id,'redirect_uri':redirect,'response_type':'code','scope':SCOPE,'state':state,'code_challenge':challenge,'code_challenge_method':'S256','access_type':'offline','prompt':'consent'})
        print('Opening Google sign-in in your browser. Approve read-only Calendar access.')
        if not webbrowser.open('https://accounts.google.com/o/oauth2/v2/auth?'+query):raise RuntimeError('The browser could not be opened.')
        deadline=time.monotonic()+300
        while not result and time.monotonic()<deadline:server.handle_request()
    if not result or result[0][0]!='code':raise RuntimeError('Google sign-in was cancelled or timed out.')
    request=urllib.request.Request('https://oauth2.googleapis.com/token',data=urllib.parse.urlencode({'client_id':client_id,'client_secret':client_secret,'code':result[0][1],'code_verifier':verifier,'redirect_uri':redirect,'grant_type':'authorization_code'}).encode(),headers={'Content-Type':'application/x-www-form-urlencoded'})
    with urllib.request.build_opener(NoRedirect).open(request,timeout=30) as response:
        raw=response.read(16385)
    if len(raw)>16384:raise RuntimeError('Google token response was too large.')
    value=json.loads(raw);bundle={'client_id':client_id,'client_secret':client_secret,'refresh_token':value.get('refresh_token')}
    if not valid_bundle(bundle) or SCOPE not in value.get('scope','').split():raise RuntimeError('Google did not grant remembered read-only Calendar access.')
    return bundle

def provision(port,bundle):
    fd=os.open(port,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK);old=termios.tcgetattr(fd);marker=('PORTHOLE_GOOGLE_'+uuid.uuid4().hex).encode()
    try:
        raw=termios.tcgetattr(fd);raw[0]=0;raw[1]=0;raw[2]=termios.CS8|termios.CREAD|termios.CLOCAL;raw[3]=0;raw[4]=termios.B115200;raw[5]=termios.B115200;raw[6][termios.VMIN]=0;raw[6][termios.VTIME]=0;termios.tcsetattr(fd,termios.TCSANOW,raw)
        send(fd,b'\x03\n');deadline=time.monotonic()+.7
        while time.monotonic()<deadline:
            if select.select([fd],[],[],.05)[0]:
                try:os.read(fd,4096)
                except BlockingIOError:pass
        send(fd,b'sudo /usr/bin/python3 -E -s /usr/local/lib/porthole/import-google.py '+marker+b'\n');wait_marker(fd,marker+b'_READY',15)
        send(fd,json.dumps(bundle,separators=(',',':')).encode()+b'\n');wait_marker(fd,marker+b'_SAVED',45)
    finally:
        try:send(fd,b'\x03')
        except Exception:pass
        termios.tcsetattr(fd,termios.TCSANOW,old);os.close(fd)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--client',required=True,help='Downloaded Desktop OAuth client JSON (keep private)');args=parser.parse_args()
    if sys.platform!='darwin' or not sys.stdin.isatty():raise RuntimeError('Run directly in a Mac terminal.')
    resource.setrlimit(resource.RLIMIT_CORE,(0,0));port=find_frame();client_id,client_secret=read_client(args.client);bundle=authorize(client_id,client_secret);provision(port,bundle);bundle.clear()
    print('Google sign-in encrypted on the frame. Choose calendars in Settings → Calendars → Google. iCloud remains connected.')
if __name__=='__main__':
    try:main()
    except (Exception,KeyboardInterrupt) as error:
        print(str(error) if isinstance(error,RuntimeError) else 'Google setup stopped. No credential was printed.',file=sys.stderr);sys.exit(1)
