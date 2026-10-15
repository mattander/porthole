#!/bin/bash
set -Eeuo pipefail
cd "$(dirname "$0")"
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
for p in /sys/class/block/mmcblk*/device/type; do test "$(cat "$p")" != MMC; done
test "$(uname -r)" = 7.2.10-1-armv7-ARCH
sha256sum -c files.sha256
test ! -e /etc/systemd/system/porthole-update.service
test ! -e /etc/porthole/update.json
test ! -e /etc/porthole/update-public.pem
openssl pkey -pubin -in update-public.pem -noout
python3 -I - <<'PY'
import sys
sys.path.insert(0,'.')
from update_common import load_config
load_config('update.json')
PY
rollback(){
    trap - ERR
    systemctl disable --now porthole-update-check.timer porthole-update-check.service porthole-update.service || true
    for unit in porthole-calendar porthole-kiosk; do rm -f "/etc/systemd/system/$unit.service.d/updates.conf"; done
    rm -f /etc/systemd/system/porthole-update.service /etc/systemd/system/porthole-update-check.service /etc/systemd/system/porthole-update-check.timer
    rm -f /etc/porthole/update.json /etc/porthole/update-public.pem
    systemctl daemon-reload
    systemctl start porthole-calendar porthole-kiosk porthole-orientation || true
    printf 'Updater bootstrap stopped; original app services restored.\n' >&2
}
trap rollback ERR
systemd-sysusers "$PWD/update-users.conf"
install -d -o root -g root -m755 /usr/local/lib/porthole/update /etc/porthole
install -d -o root -g porthole -m700 /var/lib/porthole-updater
install -d -o porthole-update -g porthole-update -m700 /var/lib/porthole-update-cache
install -o root -g root -m644 update_common.py update_fetch.py update_service.py migrations.py /usr/local/lib/porthole/update/
install -o root -g root -m644 update-public.pem /etc/porthole/update-public.pem
install -o root -g root -m644 update.json /etc/porthole/update.json
install -o root -g root -m644 porthole-update.service porthole-update-check.service porthole-update-check.timer /etc/systemd/system/
for unit in porthole-calendar porthole-kiosk; do
    install -d -m755 "/etc/systemd/system/$unit.service.d"
    test ! -e "/etc/systemd/system/$unit.service.d/updates.conf"
    printf '[Unit]\nRequires=porthole-update.service\nAfter=porthole-update.service\n' > "/etc/systemd/system/$unit.service.d/updates.conf"
done
systemd-analyze verify /etc/systemd/system/porthole-update.service /etc/systemd/system/porthole-update-check.service /etc/systemd/system/porthole-update-check.timer
systemctl daemon-reload
systemctl enable --now porthole-update.service porthole-update-check.timer
systemctl is-active --quiet porthole-update.service
trap - ERR
printf 'SIGNED_WIFI_UPDATER_BOOTSTRAPPED\n'
