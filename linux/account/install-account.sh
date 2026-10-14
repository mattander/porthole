#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
for path in /sys/class/block/mmcblk*/device/type; do test "$(cat "$path")" != MMC; done
sha256sum -c files.sha256
backup=/var/lib/porthole-install/before-persistent-account
if test -d "$backup"; then
    cmp -s "$backup/porthole" /usr/local/bin/porthole
    cmp -s "$backup/sway.conf" /etc/porthole/sway.conf
    cmp -s "$backup/porthole-kiosk.service" /etc/systemd/system/porthole-kiosk.service
    cmp -s "$backup/porthole-orientation.service" /etc/systemd/system/porthole-orientation.service
else
    install -d -m700 "$backup"
    cp -p /usr/local/bin/porthole /etc/porthole/sway.conf /etc/systemd/system/porthole-kiosk.service /etc/systemd/system/porthole-orientation.service "$backup/"
fi
rollback(){
    trap - ERR
    systemctl stop porthole-orientation.service porthole-kiosk.service || true
    systemctl disable --now porthole-calendar.service porthole-credentials.service || true
    install -m755 "$backup/porthole" /usr/local/bin/porthole
    cp -p "$backup/sway.conf" /etc/porthole/sway.conf
    cp -p "$backup/porthole-kiosk.service" "$backup/porthole-orientation.service" /etc/systemd/system/
    systemctl daemon-reload
    systemctl start porthole-kiosk.service porthole-orientation.service || true
    printf 'Account install stopped; previous dashboard restored.\n' >&2
}
trap rollback ERR
systemd-sysusers "$PWD/porthole-users.conf"
usermod -a -G network,video,input,render,seat porthole
install -d -m700 -o porthole -g porthole /home/porthole /home/porthole/.config /home/porthole/.config/porthole
if test -f /home/frame/.config/porthole/preferences.ini; then
    install -m600 -o porthole -g porthole /home/frame/.config/porthole/preferences.ini /home/porthole/.config/porthole/preferences.ini
fi
install -d -m700 /etc/credstore.encrypted
systemd-creds setup
install -m755 credential-vault.py /usr/local/lib/porthole/credential-vault.py
install -m644 porthole-calendar.service porthole-credentials.service /etc/systemd/system/
install -m755 bin/porthole-calendar /usr/local/bin/porthole-calendar
python - <<'PY'
from pathlib import Path
for name in ('porthole-kiosk','porthole-orientation'):
 p=Path('/etc/systemd/system/'+name+'.service');s=p.read_text();assert 'User=frame' in s
 s=s.replace('User=frame','User=porthole').replace('Group=frame','Group=porthole')
 s=s.replace('[Service]','[Service]\nNoNewPrivileges=yes\nLimitCORE=0')
 if name=='porthole-kiosk':s=s.replace('After=porthole-display.service seatd.service','After=porthole-display.service seatd.service porthole-calendar.service')
 p.write_text(s)
p=Path('/etc/porthole/sway.conf');s=p.read_text();assert 'native-start --demo' in s
s=s.replace('native-start --demo','native-start');p.write_text(s)
PY
systemd-analyze verify /etc/systemd/system/porthole-calendar.service /etc/systemd/system/porthole-credentials.service /etc/systemd/system/porthole-kiosk.service /etc/systemd/system/porthole-orientation.service
systemctl stop porthole-orientation.service porthole-kiosk.service
install -m755 bin/porthole /usr/local/bin/porthole
systemctl daemon-reload
systemctl enable --now porthole-credentials.service porthole-calendar.service
systemctl start porthole-kiosk.service porthole-orientation.service
trap - ERR
printf 'PERSISTENT_ACCOUNT_APP_INSTALLED\n'
