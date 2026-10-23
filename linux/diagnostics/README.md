# Private Wi-Fi diagnostics

The optional reader returns only fixed diagnostic event codes, HTTP/Qt error
numbers, timestamps and the state of the two app services. It never exports raw
journal messages, credentials, account identifiers, Wi-Fi names, calendar/event
contents, generated briefing text, device serials or arbitrary files. It cannot
run commands, change settings, update software or enroll another computer.

## Trust and enrollment

Each computer generates its own P-256 private key and public client certificate.
The private key stays in `~/.config/porthole/diagnostics/` with mode 0600. Physical
USB enrollment installs only its public certificate on the verified SD Linux
frame and pins the frame's public certificate on the computer. Both ends verify
TLS certificates, and both additionally pin the exact enrolled certificate. A
DHCP address change does not change the frame identity. TLS 1.3 is required.

Use Python 3.10 or newer built with OpenSSL and TLS 1.3 support. macOS's Xcode
Python/LibreSSL may lack TLS 1.3; use an OpenSSL-based Python. No Python packages
are required. Certificates last ten years; renewal or replacement uses USB.
A copied computer private key confers the same read-only access until revoked.
This is computer-key approval, not hardware attestation.

Once the USB bootstrap is installed, connect the frame and run:

```sh
python3 linux/diagnostics/diagnostics.py enroll
python3 linux/diagnostics/diagnostics.py logs --output local/frame-diagnostics.json
```

The first command verifies the known PF1007L USB gadget, waits for the no-echo
handshake and installs the public certificate through a root helper restricted
to the physical `/dev/ttyGS0` console. Up to eight computer certificates can be
enrolled. Each computer must enroll locally; private keys should not be shared.
An existing frame pin is never silently replaced. No network enrollment exists.

The second command needs only Wi-Fi. It uses the address learned over USB. If
DHCP changed the address, supply `logs --host 192.168.1.100` with the actual frame
IPv4 address. Authentication still verifies the USB-pinned frame certificate;
another computer at the old address cannot impersonate the frame. Output files
are created privately and never overwrite existing files. Omit `--output` to
print the sanitized JSON.

```sh
python3 linux/diagnostics/diagnostics.py status
python3 linux/diagnostics/diagnostics.py revoke
python3 linux/diagnostics/diagnostics.py revoke --fingerprint COMPUTER_SHA256
python3 linux/diagnostics/diagnostics.py revoke --all
```

Status and revocation require USB. Revocation stops the process and established
sessions before reloading trust. Revoking the last computer disables the service
and removes its inbound firewall allowance. Another enrolled computer cannot
reverse revocation over Wi-Fi. The frame's server identity remains for safe
future enrollment. Keep USB available if a computer/key is lost.

## Network and service boundary

A dedicated unprivileged service listens on IPv4 TCP 9443. The firewall allows
new connections only on `wlan0`, only while enrolled certificates exist, with a
connection-rate limit. IPv6, SSH and other inbound ports remain closed. No router
port-forward, cloud relay, service discovery or public DNS is installed. The
service is reachable by routing to the frame's Wi-Fi interface; authentication
is by certificate rather than IP/MAC address. Keep the existing LAN/IoT network
boundary; certificate authentication is not a replacement for network isolation.

The protocol accepts exactly `PORTHOLE_DIAGNOSTICS_V1\n` through mutual TLS and
returns one JSON document, at most 512 KiB, then closes. Four concurrent workers,
five-second handshake/request deadlines and a thirty-second collection cache
bound resource use. There are no paths, parameters or remote operations. The
reader can access the system journal through its supplementary journal group,
but reconstructs an explicit allowlist from fixed app-generated records. Its
systemd sandbox hides household/credential stores, drops capabilities, prevents
writes and bounds memory/tasks. It has no access to the app control socket.

The export covers at most 2,000 structured events from the last seven days,
subject to the system journal's existing retention. Logs survive an app restart
and reboot. Successful source checks are sampled every fifteen minutes; request
failures and briefing outcomes are retained. Raw journals remain USB-only.

## USB bootstrap

Privileged services and firewall helpers cannot be installed by the signed
application-update channel. Stage these files in a private checksum-covered
bundle and transfer through the established trusted USB console:

- `diagnostics_common.py`, `export_diagnostics.py`, `diagnostics_server.py`,
  `diagnostics_admin.py`, `porthole-diagnostics.service`,
  `porthole-diagnostics-users.conf`, `install-diagnostics.sh`
- `firewall-start` from `linux/network/`
- `files.sha256` covering every bundle file except itself

Run `bash install-diagnostics.sh` as root on the verified frame. Installation
checks SD root, absence of exposed eMMC, pinned kernel and file hashes, and backs
up the previous firewall helper. It installs support without generating keys,
enrolling computers or opening the listener. Bootstrap failure restores the
previous firewall helper and disables/removes the service; evidence remains for
maintenance review. Do not retry over partial bootstrap files without review.
The native app and calendar recovery/logging binaries ship separately through
the normal signed Wi-Fi release.

```sh
python3 linux/diagnostics/diagnostics_tests.py
```

Tests cover allowlist reconstruction, malformed/injected records, public-only
certificate parsing, mutual TLS success, missing/unknown/revoked clients, wrong
server identity and rejection of command/file requests. Native tests cover
never-finishing discovery/fetch requests, operation deadlines, cancellation and
recovery on the same connector. Hardware checks additionally verify enrollment,
Wi-Fi retrieval, revocation, firewall closure and app state preservation.
