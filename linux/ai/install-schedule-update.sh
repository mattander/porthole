#!/bin/bash
set -Eeuo pipefail
cd "$(dirname "$0")"
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
for path in /sys/class/block/mmcblk*/device/type; do test "$(cat "$path")" != MMC; done
sha256sum -c files.sha256
backup=/var/lib/porthole-install/before-briefing-schedule
test ! -e "$backup"
install -d -m700 "$backup"
cp -p /usr/local/bin/porthole /usr/local/bin/porthole-calendar "$backup/"
rollback(){
    trap - ERR
    systemctl stop porthole-kiosk porthole-calendar || true
    install -m755 "$backup/porthole" /usr/local/bin/porthole
    install -m755 "$backup/porthole-calendar" /usr/local/bin/porthole-calendar
    systemctl start porthole-calendar porthole-kiosk porthole-orientation || true
    printf 'Briefing installation stopped; previous binaries restored.\n' >&2
}
trap rollback ERR
systemctl stop porthole-kiosk porthole-calendar
install -m755 bin/porthole bin/porthole-calendar /usr/local/bin/
/usr/local/bin/porthole --self-check
systemctl start porthole-calendar porthole-kiosk porthole-orientation
for unit in porthole-calendar porthole-kiosk porthole-orientation; do systemctl is-active --quiet "$unit"; done
cmp bin/porthole /usr/local/bin/porthole
cmp bin/porthole-calendar /usr/local/bin/porthole-calendar
trap - ERR
printf 'BRIEFING_SCHEDULE_INSTALLED\n'
