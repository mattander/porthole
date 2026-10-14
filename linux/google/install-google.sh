#!/bin/bash
# Trusted, once-only USB maintenance; binary updates continue over signed Wi-Fi releases.
set -euo pipefail
[[ $(id -u) == 0 ]]
[[ $(findmnt -n -o SOURCE /) == /dev/mmcblk0p2 ]]
[[ $(cat /sys/class/block/mmcblk0/device/type) == SD ]]
for device in /sys/class/block/mmcblk*/device/type; do [[ $(cat "$device") != MMC ]]; done
if [[ -f SHA256SUMS ]]; then sha256sum -c SHA256SUMS; fi
backup=/var/lib/porthole-google-bootstrap/previous
install -d -m700 "$backup"
if [[ -f "$backup/credential-vault.py" ]]; then
  if cmp -s google_bundle.py /usr/local/lib/porthole/google_bundle.py &&
     cmp -s import-google.py /usr/local/lib/porthole/import-google.py &&
     cmp -s credential-vault.py /usr/local/lib/porthole/credential-vault.py &&
     cmp -s porthole-calendar.service /etc/systemd/system/porthole-calendar.service; then
    printf 'GOOGLE_CREDENTIAL_SUPPORT_ALREADY_INSTALLED\n'
    exit 0
  fi
  printf 'Previous bootstrap evidence exists; review it before retrying.\n' >&2
  exit 1
fi
cp -p /usr/local/lib/porthole/credential-vault.py "$backup/credential-vault.py"
cp -p /etc/systemd/system/porthole-calendar.service "$backup/porthole-calendar.service"
rollback() {
  cp -p "$backup/credential-vault.py" /usr/local/lib/porthole/credential-vault.py
  cp -p "$backup/porthole-calendar.service" /etc/systemd/system/porthole-calendar.service
  systemctl daemon-reload
  systemctl restart porthole-credentials.service porthole-calendar.service || true
}
trap rollback ERR
install -d -m755 /usr/local/lib/porthole
install -o root -g root -m644 google_bundle.py /usr/local/lib/porthole/google_bundle.py
install -o root -g root -m755 import-google.py /usr/local/lib/porthole/import-google.py
install -o root -g root -m755 credential-vault.py /usr/local/lib/porthole/credential-vault.py
install -o root -g root -m644 porthole-calendar.service /etc/systemd/system/porthole-calendar.service
systemctl daemon-reload
systemctl restart porthole-credentials.service porthole-calendar.service
trap - ERR
printf 'GOOGLE_CREDENTIAL_SUPPORT_INSTALLED\n'
