"""Decode bounded graphics telemetry carried by the installed schema-1 exporter.

The old exporter is intentionally unchanged. Its allowed calendar_ready event is
reserved, in the kiosk unit only and with codes 560-577, as a numeric transport.
Normal calendar_ready records come from the calendar unit and keep their meaning.
"""
METRICS = {
    560: 'protocol', 561: 'platform', 562: 'qt_minor',
    563: 'screen_width_10px', 564: 'screen_height_10px', 565: 'refresh_hz',
    566: 'scroll_state', 567: 'scroll_duration_10ms', 568: 'scroll_moves',
    569: 'scroll_area_paints', 570: 'ui_gap_ms', 571: 'overshoot_px',
    572: 'qt_warning', 573: 'kernel_access', 574: 'kernel_fault_mask',
    575: 'kernel_fault_count', 576: 'gpu_driver', 577: 'alive_seconds',
}
PLATFORMS = {0: 'other', 1: 'wayland', 2: 'xcb', 3: 'offscreen'}
STATES = {0: 'inactive', 1: 'pressed', 2: 'dragging', 3: 'scrolling'}
KERNEL_ACCESS = {1: 'readable', 2: 'permission_denied', 3: 'reader_unavailable',
                 4: 'read_failed_or_bounded_out', 5: 'no_matching_records'}
WARNINGS = {1: 'timeout', 2: 'buffer', 3: 'egl_or_opengl', 4: 'backingstore_or_paint',
            5: 'protocol_or_connection', 6: 'allocation_or_memory', 7: 'other_graphics'}
GPU_DRIVERS = {0: 'unavailable', 1: 'lima', 2: 'other'}


def decode_graphics(value):
    graphics = []
    events = []
    for entry in value['events']:
        if not isinstance(entry, dict):
            events.append(entry)
            continue
        code, number = entry.get('http'), entry.get('network')
        if (entry.get('service') != 'porthole-kiosk.service' or entry.get('event') != 'calendar_ready'
                or type(code) is not int or code not in METRICS or type(number) is not int
                or not 0 <= number <= 999 or not isinstance(entry.get('time'), str)):
            events.append(entry)
            continue
        # Copy only explicit fields; never propagate unrelated raw response keys.
        result = {'time': entry['time'], 'metric': METRICS[code], 'value': number}
        names = {561: PLATFORMS, 566: STATES, 573: KERNEL_ACCESS, 576: GPU_DRIVERS}
        if code in names:
            result['meaning'] = names[code].get(number, 'unknown')
        elif code == 572:
            result['category'] = WARNINGS.get(number // 100, 'unknown')
            result['count'] = number % 100
        elif code == 574:
            result['faults'] = [name for bit, name in ((1, 'underflow_or_underrun'), (2, 'timeout'),
                (4, 'fault_or_hang'), (8, 'error_or_failure'), (16, 'reset')) if number & bit]
        graphics.append(result)
    if not graphics:
        return value
    return {**value, 'events': events, 'graphics': graphics, 'graphics_protocol': 1}
