#!/bin/bash
# Trusted USB bootstrap. Does not run through the signed two-binary app updater.
set -Eeuo pipefail
cd "$(dirname "$0")"
test "$(id -u)" = 0
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
test "$(uname -r)" = 7.2.10-1-armv7-ARCH
for device in /sys/class/block/mmcblk*/device/type; do test "$(cat "$device")" != MMC; done
sha256sum -c files.sha256
test ! -e /etc/systemd/system/porthole-diagnostics.service
test ! -e /usr/local/lib/porthole/diagnostics
test ! -e /etc/porthole/diagnostics
backup=/var/lib/porthole-diagnostics-bootstrap
test ! -e "$backup"
install -d -m700 "$backup"
cp -p /usr/local/lib/porthole/firewall-start "$backup/firewall-start"
rollback(){
    trap - ERR
    systemctl disable --now porthole-diagnostics.service || true
    cp -p "$backup/firewall-start" /usr/local/lib/porthole/firewall-start
    /usr/local/lib/porthole/firewall-start
    rm -f /etc/systemd/system/porthole-diagnostics.service
    systemctl daemon-reload
    printf 'Diagnostic bootstrap rolled back; evidence preserved.\n' >&2
}
trap rollback ERR
systemd-sysusers "$PWD/porthole-diagnostics-users.conf"
install -d -o root -g root -m755 /usr/local/lib/porthole/diagnostics /etc/porthole/diagnostics
install -d -o root -g porthole-diagnostics -m750 /etc/porthole/diagnostics/clients
install -o root -g root -m644 diagnostics_common.py export_diagnostics.py diagnostics_server.py diagnostics_admin.py /usr/local/lib/porthole/diagnostics/
install -o root -g root -m755 firewall-start /usr/local/lib/porthole/firewall-start
install -o root -g root -m644 porthole-diagnostics.service /etc/systemd/system/
systemd-analyze verify /etc/systemd/system/porthole-diagnostics.service
systemctl daemon-reload
/usr/local/lib/porthole/firewall-start
# No keys, enrollment, open port or running reader are created by installation.
trap - ERR
printf 'DIAGNOSTIC_BOOTSTRAP_READY\n'
