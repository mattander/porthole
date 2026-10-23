#!/usr/bin/env python3
"""USB-enrolled, mutually authenticated TLS diagnostic reader."""
import hashlib
import os
from pathlib import Path
import socketserver
import ssl
import threading
import time
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from diagnostics_common import PORT, REQUEST, certificate_id, encode, tls_context
from export_diagnostics import collect


class DiagnosticServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address, context, approved, collector=collect):
        self.context = context
        self.approved = approved
        self.collector = collector
        self.slots = threading.BoundedSemaphore(4)
        self.collection_lock = threading.Lock()
        self.cached = None
        self.cached_at = 0.0
        super().__init__(address, DiagnosticRequest)

    def process_request(self, request, address):
        if not self.slots.acquire(blocking=False):
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, address)
        except Exception:
            self.slots.release()
            raise

    def process_request_thread(self, request, address):
        try:
            super().process_request_thread(request, address)
        finally:
            self.slots.release()

    def handle_error(self, request, address):
        # TLS errors, peer addresses and request bytes never enter the journal.
        pass

    def response(self):
        with self.collection_lock:
            if self.cached is None or time.monotonic() - self.cached_at >= 30:
                try:
                    self.cached = encode(self.collector())
                except Exception:
                    self.cached = encode({'schema': 1, 'services': {}, 'events': [], 'error': 'collection_unavailable'})
                self.cached_at = time.monotonic()
            return self.cached


class DiagnosticRequest(socketserver.BaseRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        try:
            with self.server.context.wrap_socket(self.request, server_side=True) as connection:
                peer = connection.getpeercert(binary_form=True)
                if hashlib.sha256(peer).hexdigest() not in self.server.approved:
                    return
                data = bytearray()
                # Fixed framing and a wall deadline prevent slow-drip resource retention.
                deadline = time.monotonic() + 5
                while len(data) < len(REQUEST):
                    connection.settimeout(max(0.01, deadline - time.monotonic()))
                    chunk = connection.recv(len(REQUEST) - len(data))
                    if not chunk or time.monotonic() > deadline:
                        return
                    data.extend(chunk)
                if bytes(data) != REQUEST:
                    return
                connection.sendall(self.server.response())
        except (OSError, ValueError, ssl.SSLError, TimeoutError):
            return


def main():
    credentials = Path(os.environ['CREDENTIALS_DIRECTORY'])
    clients = sorted(Path('/etc/porthole/diagnostics/clients').glob('*.pem'))
    if not 1 <= len(clients) <= 8:
        raise SystemExit('No enrolled diagnostic computers')
    certificates = [p.read_text(encoding='ascii') for p in clients]
    context = tls_context(True, credentials / 'server.pem', credentials / 'server-key.pem', ''.join(certificates))
    with DiagnosticServer(('0.0.0.0', PORT), context, frozenset(certificate_id(pem) for pem in certificates)) as server:
        server.serve_forever(poll_interval=0.5)


if __name__ == '__main__':
    main()
