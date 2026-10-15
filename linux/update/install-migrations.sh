#!/bin/bash
# Trusted, once-only maintenance upgrade for an already enrolled signed updater.
set -Eeuo pipefail
umask 077
cd "$(dirname "$0")"
test "$(id -u)" = 0
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
for device in /sys/class/block/mmcblk*/device/type; do test "$(cat "$device")" != MMC; done
test "$(uname -r)" = 7.2.10-1-armv7-ARCH
sha256sum -c files.sha256
test -f /etc/porthole/update-public.pem
test -f /etc/porthole/update.json
test -d /etc/systemd/system/porthole-calendar.service.d

library=/usr/local/lib/porthole/update
backup=/var/lib/porthole-migration-bootstrap
files=(update_common.py update_fetch.py update_service.py)
if test -d "$backup"; then
    if test -f "$backup/complete" && cmp -s porthole-update.service /etc/systemd/system/porthole-update.service; then
        matches=true
        for file in "${files[@]}" migrations.py; do
            if ! cmp -s "$file" "$library/$file"; then matches=false; fi
        done
        if test "$matches" = true; then
            printf 'SIGNED_CREDENTIAL_MIGRATIONS_ALREADY_INSTALLED\n'
            exit 0
        fi
    fi
    printf 'Migration bootstrap evidence already exists; review it before retrying.\n' >&2
    exit 1
fi
test ! -e /etc/systemd/system/porthole-calendar.service.d/credentials.conf
test ! -e "$library/migrations.py"
for file in "${files[@]}"; do test -f "$library/$file"; done
test -f /etc/systemd/system/porthole-update.service
python3 -I - <<'PY'
import sys
sys.path.insert(0, '.')
from update_common import load_config
from migrations import CredentialPlan
load_config('/etc/porthole/update.json')
CredentialPlan.parse(b'{"schema":1,"add_credentials":["porthole-google"]}')
PY
was_timer=$(systemctl is-active porthole-update-check.timer || true)
restart_updater() {
    systemctl start porthole-update.service
    if test "$was_timer" = active; then systemctl start porthole-update-check.timer; fi
}
# Stop checks; an interrupted app transaction must recover with the old updater.
systemctl stop porthole-update-check.timer porthole-update-check.service porthole-update.service
if test -e /var/lib/porthole-updater/pending.json; then
    restart_updater
    printf 'An app transaction needs recovery. Retry after the updater recovers it.\n' >&2
    exit 1
fi
trap 'restart_updater || true' ERR
install -d -o root -g root -m700 "$backup"
for file in "${files[@]}"; do cp -p "$library/$file" "$backup/$file"; done
cp -p /etc/systemd/system/porthole-update.service "$backup/porthole-update.service"
rollback() {
    trap - ERR
    systemctl stop porthole-update.service || true
    for file in "${files[@]}"; do cp -p "$backup/$file" "$library/$file"; done
    cp -p "$backup/porthole-update.service" /etc/systemd/system/porthole-update.service
    rm -f "$library/migrations.py"
    systemctl daemon-reload
    restart_updater || true
    printf 'Migration bootstrap stopped; previous updater restored.\n' >&2
}
trap rollback ERR
install -o root -g root -m644 "${files[@]}" migrations.py "$library/"
install -o root -g root -m644 porthole-update.service /etc/systemd/system/
systemd-analyze verify /etc/systemd/system/porthole-update.service
systemctl daemon-reload
restart_updater
systemctl is-active --quiet porthole-update.service
touch "$backup/complete"
trap - ERR
printf 'SIGNED_CREDENTIAL_MIGRATIONS_BOOTSTRAPPED\n'
