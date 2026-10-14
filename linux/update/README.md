# Signed Wi-Fi application updates

An installed PF1007L SD Linux frame can pull app releases over Wi-Fi. It checks
about hourly (up to five minutes of jitter), and Settings → Data → Software
updates provides **Check for updates** and **Install updates automatically**.
Pausing affects future automatic checks; an install already in progress finishes.
Manual checks work while paused and have a one-minute check cooldown.

No device discovery, MAC recording, inbound HTTP listener, SSH or router port
forward is needed. The frame makes outbound HTTPS requests for GitHub release
assets. The existing inbound firewall stays in place. No GitHub token is stored
on the frame; the release repository must be public.

## Scope and trust

Updates replace exactly `/usr/local/bin/porthole` and
`/usr/local/bin/porthole-calendar`. They preserve household configuration, display
preferences, encrypted credentials, Wi-Fi profiles, calendar selection, and the
AI cache/spending ledger. Kernel, drivers, bootloader, root update helpers, system
configuration and internal Android/eMMC are outside this app-update channel.

The publisher signs a bounded manifest with a local RSA signing key. A public
verification key is enrolled on the frame during bootstrap. The unprivileged
fetcher downloads only from fixed HTTPS GitHub asset hosts. The local root
installer independently verifies the signature, archive/file hashes, ARM ELF
headers, platform and pinned kernel. It accepts no caller-supplied paths,
commands, shell scripts, extra archive files, symlinks or traversal entries.
Manifest sequences must increase; failed/replayed sequences are held back.
Repository write access alone is insufficient without the matching signing key.
The publisher and enrolled key remain trusted to supply application code.

The installer backs up both old binaries and records a durable pending journal
before replacing either. Atomic file replacement, fsync and startup recovery
cover interrupted two-file installs. Post-install checks require a responsive
local calendar ping and a visible Qt window with a stable process. A failed
startup restores the previous pair automatically. The next service/boot startup
recovers an unfinished journal before dependent app services start. Corrupt
rollback evidence fails closed and requires maintenance.

## Bootstrap once on a compatible installed frame

This expects the already validated SD Linux runtime, service users, compositor,
working native app and network. It is not a kernel installer or generic OS image.
Dependencies are Python 3, OpenSSL, systemd, `setpriv`, and `swaymsg` from the
existing runtime. No Python packages are required.

On the publishing machine, create a key in ignored local storage:

```sh
python3 linux/update/publish.py keygen --directory local/update-signing
```

Keep `signing-private.pem` private (mode 0600), back it up securely, and retain it
for future releases. Enroll only `signing-public.pem`. Losing the signing key
requires trusted maintenance access to enroll a replacement; key rotation is not
an unsigned network operation.

Prepare a private bootstrap directory containing:

- `update_common.py`, `update_fetch.py`, `update_service.py`,
  `install-bootstrap.sh`, `update-users.conf`, and the three update service/timer
  files from this directory.
- `update-public.pem`: the public verification key, never the private key.
- `update.json`: copy `update.example.json` and set your public `owner/repository`.
- `files.sha256`: a SHA-256 manifest covering the bootstrap files.

Transfer and verify that directory through the existing trusted maintenance
connection, then run `bash install-bootstrap.sh` as root on the frame. The
installer checks SD root, absence of exposed eMMC, and the pinned kernel. It
creates an unprivileged downloader identity, installs the local-only update
service and hourly timer, and orders the app services after journal recovery.
Bootstrap failures remove the new dependencies and restore the app services.
Automatic updates start enabled; change that in Settings if desired.

The publisher's actual repository setting and keys belong outside the public
source tree. `update.example.json` is generic. Private keys and build outputs are
ignored by Git.

## Publish a new app release

Build and test both binaries against the frame's matching ARMv7 userspace first.
Keep the installed runtime/library ABI and pinned kernel compatible. Use a new
numeric three/four-part version for each release; the default sequence is the
publisher's current Unix time. Explicit sequences must increase monotonically.

```sh
python3 linux/update/publish.py release \
  --binaries local/native-build \
  --key local/update-signing/signing-private.pem \
  --version 2026.10.14.1 \
  --output local/release-2026.10.14.1
```

This creates a deterministic gzip/tar payload containing only the two binaries,
`porthole-manifest.json`, and `porthole-manifest.sig`. It rejects incompatible
ELF headers and validates the assembled archive before signing. No private
profile, credential, firmware, runtime library or installer is packaged.

To assemble and publish together, add `--publish --repo owner/repository
--target COMMIT_SHA` to that command, replacing the repository and commit with
your actual values. GitHub CLI must already be authenticated with release-write
access. Use a fresh output directory and release version each time. The helper
creates tag `vVERSION`, uploads the three assets, and marks the release Latest.
The private signing key is used locally and is never uploaded or stored in
GitHub Actions. Pushing source to `main` alone does not update the frame.

The fetcher uses GitHub's documented
[latest-asset download URL](https://docs.github.com/en/repositories/releasing-projects-on-github/linking-to-releases),
then gets the signature and archive from that manifest's exact version tag.
Mixed publications or network failures retain the current app and retry later.
After publication, the frame updates on its next permitted check, or use
**Check for updates**. The dashboard briefly restarts during installation.

## Services and verification

- `porthole-update.service`: local Unix IPC and privileged, network-disabled
  installer. Only the dashboard UID and downloader UID have their fixed operations.
- `porthole-update-check.service`: restricted downloader, without root privileges.
- `porthole-update-check.timer`: checks after startup and about hourly.

The private state is `/var/lib/porthole-updater/state.json`; rollback binaries and
journal stay in that root-private directory. Candidate files are bounded and
stored in the downloader's private state directory. The installer reads regular
files without following symlinks, snapshots bytes, and re-verifies them itself.
No raw network response, private calendar data, key or request URL is logged.

```sh
python3 linux/update/update-tests.py
```

Offline tests cover signature tampering, hashes, traversal, links, extra files,
duplicates, decompression limits, malformed metadata, HTTPS host restrictions,
peer operations, replay rejection, and recovery after a partial two-file write.
Native tests cover update IPC parsing and Settings control callbacks. Runtime
checks must additionally verify the installed sandbox, a real signed Wi-Fi
update, and automatic failed-startup rollback on the target hardware.

The privileged service explicitly retains only DAC override, identity switching,
and child-process termination capabilities. Ambient capability inheritance is
needed by its Python subprocesses; probes drop to the existing app identities
using `setpriv` without a PAM session. Network address families remain disabled
for the installer. The downloader has no capabilities. Core dumps are disabled
for both services.
