"""Rebuild a bounded allowlist from structured app journal records."""
import datetime
import json
import subprocess
from diagnostics_common import EVENTS, UNITS

PREFIX = 'PORTHOLE_DIAGNOSTIC '


def record(entry):
    if not isinstance(entry, dict) or entry.get('_SYSTEMD_UNIT') not in UNITS:
        return None
    message = entry.get('MESSAGE')
    if not isinstance(message, str) or len(message) > 256 or not message.startswith(PREFIX):
        return None
    try:
        value = json.loads(message[len(PREFIX):])
        if not isinstance(value, dict) or set(value) != {'v', 'event', 'http', 'network'}:
            return None
        if type(value['v']) is not int or value['v'] != 1 or value['event'] not in EVENTS:
            return None
        if type(value['http']) is not int or not 0 <= value['http'] <= 599:
            return None
        if type(value['network']) is not int or not 0 <= value['network'] <= 999:
            return None
        timestamp = entry.get('__REALTIME_TIMESTAMP')
        if not isinstance(timestamp, str) or not timestamp.isascii() or not timestamp.isdigit() or len(timestamp) > 18:
            return None
        when = datetime.datetime.fromtimestamp(int(timestamp) / 1000000, datetime.timezone.utc).isoformat()
        return {'time': when, 'service': entry['_SYSTEMD_UNIT'], 'event': value['event'],
                'http': value['http'], 'network': value['network']}
    except (ValueError, TypeError, OverflowError, OSError):
        return None


def collect():
    command = ['/usr/bin/journalctl', '--no-pager', '--output=json',
               '--output-fields=MESSAGE,__REALTIME_TIMESTAMP,_SYSTEMD_UNIT',
               '--since=-7days', '-n', '2000', '--grep=^PORTHOLE_DIAGNOSTIC \\{.{1,180}\\}$']
    for unit in UNITS:
        command.extend(['-u', unit])
    result = subprocess.run(command, capture_output=True, timeout=6)
    # journalctl returns 1 when the requested, filtered history has no entries.
    if result.returncode != 0 and not (result.returncode == 1 and not result.stdout and not result.stderr):
        raise RuntimeError('Diagnostic journal unavailable')
    if len(result.stdout) > 2097152:
        raise ValueError('Journal exceeds limit')
    records = []
    for line in result.stdout.splitlines():
        try:
            item = record(json.loads(line))
        except (ValueError, TypeError):
            continue
        if item is not None:
            records.append(item)
    services = {}
    for unit in UNITS:
        state = subprocess.run(['/usr/bin/systemctl', 'is-active', unit], capture_output=True, timeout=2)
        name = state.stdout.decode('ascii', 'replace').strip()
        services[unit] = name if name in ('active', 'inactive', 'failed', 'activating', 'deactivating') else 'unknown'
    return {'schema': 1, 'services': services, 'events': records, 'limit': 2000, 'window_days': 7}
