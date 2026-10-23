"""Fixed, read-only diagnostic protocol. No calendar data or remote commands."""
import hashlib
import json
import ssl

PORT = 9443
REQUEST = b'PORTHOLE_DIAGNOSTICS_V1\n'
MAX_RESPONSE = 524288
EVENTS = frozenset('''calendar_request_timeout calendar_operation_timeout
calendar_request_failed calendar_response_invalid calendar_ready calendar_fetch_complete
upcoming_unavailable upcoming_ready upcoming_selection_missing
briefing_waiting_calendar briefing_waiting_weather briefing_request_started
briefing_request_failed briefing_reply_invalid briefing_complete'''.split())
UNITS = ('porthole-calendar.service', 'porthole-kiosk.service')


def certificate_id(pem):
    return hashlib.sha256(ssl.PEM_cert_to_DER_cert(pem)).hexdigest()


def encode(value):
    data = json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode() + b'\n'
    if len(data) > MAX_RESPONSE:
        raise ValueError('Diagnostic response exceeds limit')
    return data


def tls_context(server, certificate, key, trust):
    # Construct explicitly: never enable key logging via SSLKEYLOGFILE.
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER if server else ssl.PROTOCOL_TLS_CLIENT)
    context.minimum_version = ssl.TLSVersion.TLSv1_3
    context.verify_mode = ssl.CERT_REQUIRED
    context.load_verify_locations(cadata=trust)
    context.load_cert_chain(certificate, key)
    return context
