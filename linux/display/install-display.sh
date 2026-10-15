#!/bin/bash
# Trusted USB maintenance bootstrap; not a signed application migration.
set -Eeuo pipefail
cd "$(dirname "$0")"
test "$(id -u)" = 0
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
for device in /sys/class/block/mmcblk*/device/type; do test "$(cat "$device")" != MMC; done
test "$(uname -r)" = 7.2.10-1-armv7-ARCH
sha256sum -c files.sha256
getent group porthole > /dev/null
attribute=/sys/class/backlight/backlight/brightness
unit=/etc/systemd/system/porthole-backlight.service
test ! -e "$unit"
test -f "$attribute"
previous_group=$(stat -c %g "$attribute")
previous_mode=$(stat -c %a "$attribute")
rollback(){
    trap - ERR
    systemctl disable --now porthole-backlight.service || true
    rm -f "$unit"
    chgrp "$previous_group" "$attribute"
    chmod "$previous_mode" "$attribute"
    systemctl daemon-reload
    printf 'Backlight permissions restored.\n' >&2
}
trap rollback ERR
install -o root -g root -m644 porthole-backlight.service "$unit"
systemd-analyze verify "$unit"
systemctl daemon-reload
systemctl enable --now porthole-backlight.service
systemctl is-active --quiet porthole-backlight.service
sudo -n -u porthole test -w "$attribute"
test "$(stat -c %a "$attribute")" = 660
trap - ERR
printf 'BACKLIGHT_ACCESS_READY\n'
