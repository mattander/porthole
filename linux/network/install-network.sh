#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
grep -qi '^SDIO_ID=C8A1:C08D$' /sys/bus/sdio/devices/*/uevent
systemctl disable --now sshd.service
install -d -m755 /etc/porthole /etc/iwd /usr/local/lib/porthole /etc/systemd/system/iwd.service.d /etc/systemd/resolved.conf.d
if test -e /etc/iwd/main.conf; then
    test ! -e /etc/iwd/main.conf.before-porthole
    cp -p /etc/iwd/main.conf /etc/iwd/main.conf.before-porthole
fi
install -m644 iwd-main.conf /etc/iwd/main.conf
install -m644 firewall.nft /etc/porthole/firewall.nft
install -m755 firewall-start wifi-start /usr/local/lib/porthole/
install -m644 porthole-firewall.service porthole-wifi.service /etc/systemd/system/
python - <<'PY'
from pathlib import Path
import secrets
p=Path('/etc/porthole/wifi-mac')
if not p.exists():
    address=bytearray(secrets.token_bytes(6));address[0]=(address[0]&0xfe)|2
    p.write_text(':'.join(f'{b:02x}' for b in address)+'\n');p.chmod(0o600)
Path('/etc/systemd/system/iwd.service.d/porthole.conf').write_text('[Unit]\nRequires=porthole-wifi.service\nAfter=porthole-wifi.service systemd-resolved.service\n')
Path('/etc/systemd/resolved.conf.d/porthole.conf').write_text('[Resolve]\nLLMNR=no\nMulticastDNS=no\n')
PY
install -d -m700 /var/lib/iwd
sh -n /usr/local/lib/porthole/wifi-start /usr/local/lib/porthole/firewall-start
nft -c -f /etc/porthole/firewall.nft
systemd-analyze verify /etc/systemd/system/porthole-firewall.service /etc/systemd/system/porthole-wifi.service
systemctl daemon-reload
systemctl restart systemd-resolved.service
systemctl enable --now porthole-firewall.service porthole-wifi.service iwd.service
printf 'LINUX_WIFI_PREPARED\n'
