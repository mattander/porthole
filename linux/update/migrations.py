"""Declarative, additive credential delivery. No secrets, paths or commands in plans."""
from dataclasses import dataclass
from pathlib import Path
import re

from update_common import Rejected, parse_json

MAX_PLAN = 4096
MAX_CREDENTIALS = 16
CREDENTIAL = re.compile(r'porthole-[a-z][a-z0-9-]{0,47}')
DROP_IN = Path('/etc/systemd/system/porthole-calendar.service.d/credentials.conf')


@dataclass(frozen=True)
class CredentialPlan:
    names: tuple[str, ...]

    @classmethod
    def parse(cls, raw: bytes) -> 'CredentialPlan':
        value = parse_json(raw, MAX_PLAN)
        if set(value) != {'schema', 'add_credentials'} or type(value['schema']) is not int or value['schema'] != 1:
            raise Rejected('archive')
        names = value['add_credentials']
        if not isinstance(names, list) or not 1 <= len(names) <= MAX_CREDENTIALS:
            raise Rejected('archive')
        if not all(isinstance(name, str) and CREDENTIAL.fullmatch(name) for name in names):
            raise Rejected('archive')
        if len(set(names)) != len(names):
            raise Rejected('archive')
        return cls(tuple(sorted(names)))

    def merge(self, previous: 'CredentialPlan') -> 'CredentialPlan':
        names = tuple(sorted(set(self.names) | set(previous.names)))
        if len(names) > MAX_CREDENTIALS:
            raise Rejected('storage')
        return CredentialPlan(names)

    def render(self) -> bytes:
        lines = ['# Managed by Porthole signed credential migrations.', '[Service]']
        for name in self.names:
            lines.extend([f'LoadCredentialEncrypted={name}', f'SetCredential={name}:{{}}'])
        return ('\n'.join(lines) + '\n').encode('ascii')

    @classmethod
    def from_drop_in(cls, raw: bytes) -> 'CredentialPlan':
        """Refuse an edited/foreign drop-in rather than overwriting local configuration."""
        try:
            lines = raw.decode('ascii').splitlines()
            names = tuple(line.removeprefix('LoadCredentialEncrypted=') for line in lines
                          if line.startswith('LoadCredentialEncrypted='))
            if not 1 <= len(names) <= MAX_CREDENTIALS or not all(CREDENTIAL.fullmatch(name) for name in names):
                raise Rejected('storage')
            plan = cls(names)
            if len(set(names)) != len(names) or plan.render() != raw:
                raise Rejected('storage')
            return plan
        except UnicodeError:
            raise Rejected('storage') from None
