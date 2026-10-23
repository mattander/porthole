#!/usr/bin/env python3
"""Root-only physical USB enrollment; never accepts remote administration."""
import base64
import json
import os
from pathlib import Path
import re
import ssl
import subprocess
import sys
import tempfile
import termios

sys.path.insert(0, str(Path(__file__).resolve().parent))
from diagnostics_common import certificate_id

ROOT = Path('/etc/porthole/diagnostics')
SERVICE = 'porthole-diagnostics.service'


def validate_certificate(pem):
    if not isinstance(pem, str) or not pem.isascii() or len(pem) > 2048:
        raise ValueError('Invalid public certificate')
    if not re.fullmatch(r'-----BEGIN CERTIFICATE-----\n[A-Za-z0-9+/=\n]+\n-----END CERTIFICATE-----\n?', pem):
        raise ValueError('Expected one public certificate')
    with tempfile.NamedTemporaryFile() as temporary:
        temporary.write(pem.encode('ascii'));temporary.flush()
        subprocess.run(['/usr/bin/openssl', 'x509', '-in', temporary.name, '-noout', '-checkend', '86400'],
                       check=True, capture_output=True, timeout=3)
        purposes = subprocess.check_output(['/usr/bin/openssl', 'x509', '-in', temporary.name, '-noout', '-purpose'], timeout=3)
        if b'SSL client : Yes' not in purposes.splitlines():
            raise ValueError('Certificate does not permit client authentication')
        # Parse through the TLS library too. TLS verifies key possession and certificate
        # signatures at connection time; the server additionally pins this exact DER.
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_verify_locations(cadata=pem)
    return certificate_id(pem)


def server_identity():
    key, cert = ROOT / 'server-key.pem', ROOT / 'server.pem'
    if key.exists() != cert.exists():
        raise ValueError('Incomplete server identity requires USB maintenance')
    if not key.exists():
        subprocess.run(['/usr/bin/openssl', 'req', '-x509', '-newkey', 'ec', '-pkeyopt',
                        'ec_paramgen_curve:P-256', '-nodes', '-days', '3650',
                        '-subj', '/CN=porthole-diagnostics', '-addext', 'subjectAltName=DNS:porthole-diagnostics',
                        '-addext', 'basicConstraints=critical,CA:FALSE', '-addext', 'keyUsage=critical,digitalSignature',
                        '-addext', 'extendedKeyUsage=serverAuth', '-keyout', str(key), '-out', str(cert)],
                       check=True, capture_output=True, timeout=15)
        key.chmod(0o600);cert.chmod(0o644)
    return cert.read_text(encoding='ascii')


def apply(value):
    if not isinstance(value, dict):
        raise ValueError('Invalid operation')
    clients = ROOT / 'clients'
    operation = value.get('operation')
    if operation == 'enroll' and set(value) == {'operation', 'certificate'}:
        pem = value['certificate'];fingerprint = validate_certificate(pem)
        destination = clients / (fingerprint + '.pem')
        if not destination.exists() and len(list(clients.glob('*.pem'))) >= 8:
            raise ValueError('Eight computers already enrolled')
        server_identity()
        subprocess.run(['/usr/bin/systemctl', 'stop', SERVICE], check=True, timeout=15)
        with tempfile.NamedTemporaryFile(dir=clients, delete=False) as temporary:
            temporary.write(pem.encode('ascii'));temporary.flush();os.fsync(temporary.fileno())
            os.fchmod(temporary.fileno(), 0o644)
        os.replace(temporary.name, destination)
    elif operation == 'revoke' and set(value) == {'operation', 'fingerprint'}:
        fingerprint = value['fingerprint']
        if not isinstance(fingerprint, str) or not re.fullmatch(r'[0-9a-f]{64}|all', fingerprint):
            raise ValueError('Invalid computer fingerprint')
        subprocess.run(['/usr/bin/systemctl', 'stop', SERVICE], check=True, timeout=15)
        for item in clients.glob('*.pem') if fingerprint == 'all' else (clients / (fingerprint + '.pem'),):
            item.unlink(missing_ok=True)
    elif operation != 'status' or set(value) != {'operation'}:
        raise ValueError('Unknown operation')
    directory = os.open(clients, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(directory)
    finally:
        os.close(directory)
    enrolled = sorted(p.stem for p in clients.glob('*.pem'))
    if operation != 'status':
        # Stop first: revocation terminates established sessions and discards cached trust.
        subprocess.run(['/usr/bin/systemctl', 'stop', SERVICE], check=True, timeout=15)
        subprocess.run(['/usr/local/lib/porthole/firewall-start'], check=True, timeout=5)
        subprocess.run(['/usr/bin/systemctl', 'enable' if enrolled else 'disable', SERVICE], check=True, capture_output=True, timeout=10)
        if enrolled:
            subprocess.run(['/usr/bin/systemctl', 'start', SERVICE], check=True, timeout=15)
    addresses = json.loads(subprocess.check_output(['/usr/bin/ip', '-j', '-4', 'addr', 'show', 'dev', 'wlan0'], timeout=3))
    hosts = [a['local'] for interface in addresses for a in interface.get('addr_info', []) if a.get('family') == 'inet']
    return {'schema': 1, 'enrolled': enrolled, 'hosts': hosts,
            'server_certificate': (ROOT / 'server.pem').read_text(encoding='ascii') if enrolled else None}


def main():
    # sudo may interpose its own PTY; SUDO_TTY is overwritten by sudo with
    # the invoking terminal. The network service has no sudo or shell authority.
    physical_usb = sys.stdin.isatty() and (os.ttyname(0) == '/dev/ttyGS0' or os.environ.get('SUDO_TTY') == '/dev/ttyGS0')
    if os.geteuid() != 0 or not physical_usb:
        raise SystemExit('Use the physical USB enrollment tool')
    if len(sys.argv) != 2 or not re.fullmatch(r'PORTHOLE_DIAG_[0-9a-f]{32}', sys.argv[1]):
        raise SystemExit('Use the USB enrollment tool')
    if subprocess.check_output(['/usr/bin/findmnt', '-n', '-o', 'SOURCE', '/']).strip() != b'/dev/mmcblk0p2':
        raise SystemExit('Expected SD Linux root')
    if Path('/sys/class/block/mmcblk0/device/type').read_text().strip() != 'SD':
        raise SystemExit('Expected SD Linux root')
    for device in Path('/sys/class/block').glob('mmcblk*/device/type'):
        if device.read_text().strip() == 'MMC':
            raise SystemExit('Exposed internal storage')
    os.umask(0o077)
    old = termios.tcgetattr(0)
    try:
        quiet = termios.tcgetattr(0);quiet[3] &= ~termios.ECHO;termios.tcsetattr(0, termios.TCSANOW, quiet)
        print(sys.argv[1] + '_READY', flush=True)
        line = sys.stdin.buffer.readline(8193)
        if len(line) > 8192 or not line.endswith(b'\n'):
            raise ValueError('Bounded enrollment request required')
        result = apply(json.loads(line))
        print(sys.argv[1] + '_RESULT ' + base64.b64encode(json.dumps(result).encode()).decode(), flush=True)
    except Exception:
        print(sys.argv[1] + '_FAILED', flush=True)
        raise SystemExit(1)
    finally:
        termios.tcsetattr(0, termios.TCSANOW, old)


if __name__ == '__main__':
    main()
