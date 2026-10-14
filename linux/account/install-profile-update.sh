#!/bin/bash
set -Eeuo pipefail
cd "$(dirname "$0")"
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
for path in /sys/class/block/mmcblk*/device/type; do test "$(cat "$path")" != MMC; done
sha256sum -c files.sha256
backup=/var/lib/porthole-install/before-private-profile
profile=/var/lib/porthole-calendar/profile.json
test ! -e "$backup"
install -d -m700 "$backup"
cp -p /usr/local/bin/porthole /usr/local/bin/porthole-calendar "$backup/"
if test -e "$profile"; then cp -p "$profile" "$backup/profile.json"; fi
rollback(){
    trap - ERR
    systemctl stop porthole-kiosk porthole-calendar || true
    install -m755 "$backup/porthole" /usr/local/bin/porthole
    install -m755 "$backup/porthole-calendar" /usr/local/bin/porthole-calendar
    if test -e "$backup/profile.json"; then install -o porthole-calendar -g porthole -m600 "$backup/profile.json" "$profile"; else rm -f "$profile"; fi
    systemctl start porthole-calendar porthole-kiosk porthole-orientation || true
    printf 'Profile update stopped; previous binaries and settings restored.\n' >&2
}
trap rollback ERR
systemctl stop porthole-kiosk porthole-calendar
install -m755 bin/porthole bin/porthole-calendar /usr/local/bin/
# Parse the sealed profile before provisioning it; no profile value is logged.
bin/porthole-calendar --check-profile "$PWD/profile.json"
install -o porthole-calendar -g porthole -m600 profile.json "$profile"
/usr/local/bin/porthole --self-check
systemctl start porthole-calendar porthole-kiosk porthole-orientation
for unit in porthole-calendar porthole-kiosk porthole-orientation; do systemctl is-active --quiet "$unit"; done
cmp bin/porthole /usr/local/bin/porthole
cmp bin/porthole-calendar /usr/local/bin/porthole-calendar
cmp profile.json "$profile"
trap - ERR
printf 'PRIVATE_PROFILE_INSTALLED\n'
