#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
test "$(findmnt -n -o SOURCE /)" = /dev/mmcblk0p2
test "$(cat /sys/class/block/mmcblk0/device/type)" = SD
for path in /sys/class/block/mmcblk*/device/type; do test "$(cat "$path")" != MMC; done
sha256sum -c files.sha256
backup=/var/lib/porthole-install/before-ai-briefing
test ! -e "$backup"
install -d -m700 "$backup"
cp -p /usr/local/bin/porthole /usr/local/bin/porthole-calendar /usr/local/lib/porthole/credential-vault.py /etc/systemd/system/porthole-calendar.service "$backup/"
if test -f /usr/local/lib/porthole/import-openai-key.py; then cp -p /usr/local/lib/porthole/import-openai-key.py "$backup/"; fi
rollback(){
    trap - ERR
    systemctl stop porthole-kiosk.service porthole-calendar.service porthole-credentials.service || true
    install -m755 "$backup/porthole" /usr/local/bin/porthole
    install -m755 "$backup/porthole-calendar" /usr/local/bin/porthole-calendar
    install -m755 "$backup/credential-vault.py" /usr/local/lib/porthole/credential-vault.py
    install -m644 "$backup/porthole-calendar.service" /etc/systemd/system/porthole-calendar.service
    if test -f "$backup/import-openai-key.py"; then install -m755 "$backup/import-openai-key.py" /usr/local/lib/porthole/import-openai-key.py; else rm -f /usr/local/lib/porthole/import-openai-key.py; fi
    systemctl daemon-reload
    systemctl start porthole-credentials.service porthole-calendar.service porthole-kiosk.service porthole-orientation.service || true
    printf 'AI installation stopped; previous app and calendar service restored.\n' >&2
}
trap rollback ERR
python - <<'PY'
from pathlib import Path
for name in ('credential-vault.py','import-openai-key.py'):compile(Path(name).read_text(),name,'exec')
PY
systemctl stop porthole-kiosk.service porthole-calendar.service porthole-credentials.service
install -m755 bin/porthole bin/porthole-calendar /usr/local/bin/
install -m755 credential-vault.py import-openai-key.py /usr/local/lib/porthole/
install -m644 porthole-calendar.service /etc/systemd/system/
systemd-analyze verify /etc/systemd/system/porthole-calendar.service /etc/systemd/system/porthole-credentials.service
systemctl daemon-reload
systemctl start porthole-credentials.service porthole-calendar.service porthole-kiosk.service porthole-orientation.service
for unit in porthole-credentials.service porthole-calendar.service porthole-kiosk.service porthole-orientation.service; do systemctl is-active --quiet "$unit"; done
trap - ERR
printf 'AI_BRIEFING_INSTALLED_NO_KEY_OR_PAID_REQUEST\n'
