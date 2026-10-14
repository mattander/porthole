#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
for path in /sys/class/block/mmcblk*/device/type; do test "$(cat "$path")" != MMC; done
test "$(uname -r)" = 7.2.10-1-armv7-ARCH
sha256sum -c files.sha256
python - <<'PY'
from pathlib import Path
import subprocess
expected=dict(line.split(' ',1) for line in Path('baseline.txt').read_text().splitlines())
actual=dict(line.split(' ',1) for line in subprocess.check_output(['pacman','-Q'],text=True).splitlines())
if actual != expected:raise SystemExit('Frame package baseline changed; review before installing.')
PY
packages=()
for package in packages/*.pkg.tar.*; do
    case "$package" in *.sig)continue;;esac
    packages+=("$package")
done
pacman -U --needed --noconfirm "${packages[@]}"
test "$(pacman -Q linux-armv7)" = 'linux-armv7 7.2.10-1'
install -m755 bin/porthole /usr/local/bin/porthole
/usr/local/bin/porthole --self-check
install -d -m700 /var/lib/porthole-install/native-browser-fallback
cp /etc/porthole/sway.conf /var/lib/porthole-install/native-browser-fallback/sway.conf
cp /etc/systemd/system/porthole-kiosk.service /var/lib/porthole-install/native-browser-fallback/porthole-kiosk.service
systemctl stop porthole-orientation.service porthole-kiosk.service
install -m755 native-start /usr/local/lib/porthole/native-start
python - <<'PY'
from pathlib import Path
p=Path('/etc/porthole/sway.conf');text=p.read_text();assert '/usr/local/lib/porthole/browser-start' in text
text=text.replace('/usr/local/lib/porthole/browser-start','/usr/local/lib/porthole/native-start --demo')
text+='\nfor_window [app_id="porthole"] fullscreen enable\n'
p.write_text(text)
p=Path('/etc/systemd/system/porthole-kiosk.service');text=p.read_text()
text=text.replace('Porthole Linux browser hardware test','Porthole fullscreen Linux dashboard')
text=text.replace(' porthole-web.service','')
p.write_text(text)
PY
systemctl daemon-reload
systemctl disable --now porthole-web.service
systemctl start porthole-kiosk.service porthole-orientation.service
printf 'NATIVE_INSTALL_COMPLETE\n'
