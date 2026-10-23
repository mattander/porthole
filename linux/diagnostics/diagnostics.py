#!/usr/bin/env python3
"""Enroll this computer over USB, then read sanitized frame logs over Wi-Fi."""
import argparse
import base64
import ipaddress
import json
import os
from pathlib import Path
import re
import select
import socket
import ssl
import subprocess
import sys
import termios
import time
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'google'))
from diagnostics_common import MAX_RESPONSE, PORT, REQUEST, certificate_id, tls_context
from usb_transport import find_frame, send, wait_marker

DEFAULT = Path.home() / '.config' / 'porthole' / 'diagnostics'


def usb_operation(value):
    marker = ('PORTHOLE_DIAG_' + uuid.uuid4().hex).encode('ascii')
    fd = os.open(find_frame(), os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    old = termios.tcgetattr(fd)
    try:
        raw = termios.tcgetattr(fd)
        raw[0] = 0;raw[1] = 0;raw[2] = termios.CS8 | termios.CREAD | termios.CLOCAL;raw[3] = 0
        raw[4] = termios.B115200;raw[5] = termios.B115200;raw[6][termios.VMIN] = 0;raw[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, raw)
        send(fd, b'\x03\n')
        end = time.monotonic() + 0.7
        while time.monotonic() < end:
            if select.select([fd], [], [], 0.05)[0]:
                os.read(fd, 65536)
        send(fd, b'sudo -n /usr/bin/python3 -I /usr/local/lib/porthole/diagnostics/diagnostics_admin.py ' + marker + b'\n')
        wait_marker(fd, marker + b'_READY', 15)
        send(fd, json.dumps(value, separators=(',', ':')).encode() + b'\n')
        tail = b'';end = time.monotonic() + 50
        while time.monotonic() < end:
            if select.select([fd], [], [], 0.2)[0]:
                tail = (tail + os.read(fd, 65536))[-16384:]
                if marker + b'_FAILED' in tail:
                    raise RuntimeError('USB operation failed; inspect the frame through USB.')
                match = re.search(marker + rb'_RESULT ([A-Za-z0-9+/=]+)\r?\n', tail)
                if match:
                    return json.loads(base64.b64decode(match.group(1), validate=True))
        raise RuntimeError('USB result timed out; enrollment status is unconfirmed.')
    finally:
        send(fd, b'\x03')
        termios.tcsetattr(fd, termios.TCSANOW, old);os.close(fd)


def identity(directory):
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    if directory.is_symlink() or directory.stat().st_mode & 0o077:
        raise RuntimeError('Identity directory must be private (mode 0700).')
    key, cert = directory / 'computer-key.pem', directory / 'computer.pem'
    if key.exists() != cert.exists():
        raise RuntimeError('Incomplete local identity; preserve it and inspect before retrying.')
    if not key.exists():
        subprocess.run(['openssl', 'req', '-x509', '-newkey', 'ec', '-pkeyopt', 'ec_paramgen_curve:P-256',
                        '-nodes', '-days', '3650', '-subj', '/CN=porthole-diagnostic-computer',
                        '-addext', 'basicConstraints=critical,CA:FALSE', '-addext', 'keyUsage=critical,digitalSignature',
                        '-addext', 'extendedKeyUsage=clientAuth', '-keyout', str(key), '-out', str(cert)],
                       check=True, capture_output=True, timeout=15)
        key.chmod(0o600);cert.chmod(0o600)
    if key.is_symlink() or cert.is_symlink() or key.stat().st_mode & 0o077:
        raise RuntimeError('Local key must be a private regular file.')
    return key, cert


def fetch(host, certificate, key, server_certificate, port=PORT):
    # A fixed DNS name in the USB-pinned certificate survives DHCP address changes.
    context = tls_context(False, certificate, key, server_certificate)
    with socket.create_connection((host, port), timeout=8) as raw:
        with context.wrap_socket(raw, server_hostname='porthole-diagnostics') as connection:
            import hashlib
            if hashlib.sha256(connection.getpeercert(binary_form=True)).hexdigest() != certificate_id(server_certificate):
                raise RuntimeError('Frame identity does not match USB enrollment')
            connection.sendall(REQUEST)
            data = bytearray();deadline = time.monotonic() + 15
            while True:
                connection.settimeout(max(0.01, deadline - time.monotonic()))
                chunk = connection.recv(min(65536, MAX_RESPONSE + 1 - len(data)))
                if not chunk:
                    break
                data.extend(chunk)
                if len(data) > MAX_RESPONSE or time.monotonic() > deadline:
                    raise RuntimeError('Diagnostic response exceeded its limit')
    value = json.loads(data)
    if not isinstance(value, dict) or value.get('schema') != 1 or not isinstance(value.get('events'), list):
        raise RuntimeError('Invalid diagnostic response')
    if value.get('error'):
        raise RuntimeError('The frame could not collect its diagnostic journal; USB maintenance is required.')
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=DEFAULT, help='Private computer identity directory')
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('enroll', help='Physically enroll this computer over the verified USB frame console')
    commands.add_parser('status', help='List enrolled computer fingerprints over USB')
    revoke = commands.add_parser('revoke', help='Revoke this computer or all computers over USB')
    revoke.add_argument('--all', action='store_true')
    revoke.add_argument('--fingerprint')
    logs = commands.add_parser('logs', help='Read sanitized diagnostics over mutually authenticated Wi-Fi TLS')
    logs.add_argument('--host', help='Frame IPv4 address if DHCP changed it since USB enrollment')
    logs.add_argument('--output', type=Path, help='Save to a new private JSON file')
    args = parser.parse_args();os.umask(0o077)
    if not ssl.HAS_TLSv1_3:
        raise RuntimeError('Use Python 3.10+ built with OpenSSL and TLS 1.3; see linux/diagnostics/README.md.')
    directory = args.directory
    if args.command == 'enroll':
        key, cert = identity(directory)
        result = usb_operation({'operation': 'enroll', 'certificate': cert.read_text(encoding='ascii')})
        pem = result.get('server_certificate')
        if not isinstance(pem, str):
            raise RuntimeError('USB did not return a frame certificate')
        # Preserve the established frame pin; do not silently trust a replacement device.
        pin = directory / 'frame.pem'
        if pin.exists() and certificate_id(pin.read_text()) != certificate_id(pem):
            raise RuntimeError('Different frame identity; inspect the saved pin before replacing it')
        pin.write_text(pem);pin.chmod(0o600)
        (directory / 'connection.json').write_text(json.dumps({'hosts': result['hosts']}))
        print('Computer enrolled. Private key stays on this Mac. Wi-Fi diagnostic reading is enabled.')
        print('Computer fingerprint:', certificate_id(cert.read_text()))
    elif args.command == 'status':
        value = usb_operation({'operation': 'status'})
        print(json.dumps({'enrolled': value['enrolled'], 'hosts': value['hosts']}, indent=2))
    elif args.command == 'revoke':
        if args.all and args.fingerprint:
            parser.error('Choose --all or --fingerprint')
        fingerprint = 'all' if args.all else args.fingerprint or certificate_id((directory / 'computer.pem').read_text())
        value = usb_operation({'operation': 'revoke', 'fingerprint': fingerprint})
        print('Revoked. Remaining enrolled computers:', len(value['enrolled']))
    else:
        key, cert = identity(directory)
        hosts = json.loads((directory / 'connection.json').read_text()).get('hosts', [])
        host = args.host or (hosts[0] if hosts else None)
        if host is None or not isinstance(ipaddress.ip_address(host), ipaddress.IPv4Address):
            raise RuntimeError('Provide the frame IPv4 address with --host')
        value = fetch(host, cert, key, (directory / 'frame.pem').read_text(encoding='ascii'))
        data = json.dumps(value, indent=2) + '\n'
        if args.output:
            with args.output.open('x') as destination:
                destination.write(data)
            print('Private diagnostics saved:', args.output)
        else:
            print(data, end='')


if __name__ == '__main__':
    try:
        main()
    except RuntimeError as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
    except (Exception, KeyboardInterrupt):
        print('Diagnostic operation failed. Check USB enrollment, the frame address, certificate validity and private file permissions.', file=sys.stderr)
        sys.exit(1)
