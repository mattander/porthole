#!/usr/bin/env python3
"""Serve the offline dashboard only on localhost, without request logging."""
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory="/opt/porthole/web", **kwargs)

    def list_directory(self, path):
        self.send_error(404)
        return None

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    ThreadingHTTPServer(("127.0.0.1", 8765), Handler).serve_forever()
