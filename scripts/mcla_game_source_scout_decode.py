"""Pure, streaming decoder for one fixed passive91-slot source scout.

All metadata and the exact BMP inventory pass before the first injected read.
Only current/previous RGB and current encoded BMP are held, never91 images.
No IO, native admission, scene selection, retry, cycle extraction or FPS claim.
The supervisor separately proves immutable inputs, process ownership and normal
cleanup. Native buffer-limit fields describe its diagnostic buffers, not total
game/process memory and not a measured memory-usage result.
"""
import hashlib
import re
import struct

SLOTS, HZ, DELAY_SECONDS = 91, 10, 40
WIDTH, HEIGHT = 1280, 720
MAX_QPC = 2**63 - 1
BMP_STRIDE = WIDTH * 3
BMP_BYTES = 54 + BMP_STRIDE * HEIGHT
BUFFER_LIMIT = 536870912
MAX_RAW_BYTES = 335462400
CAPTURE_KEYS = {'presenter_epoch', 'acquired_sequence', 'qpc_frequency',
                'refresh_cpu_begin_qpc', 'refresh_cpu_end_qpc',
                'acquire_before_qpc', 'acquire_after_qpc',
                'copy_submit_before_qpc', 'copy_completion_observed_qpc',
                'pixels_ready_qpc', 'width', 'height', 'stride'}
SLOT_KEYS = {'slot', 'due_qpc', 'observed_qpc', 'status', 'reason',
             'call_before_qpc', 'call_after_qpc', 'capture',
             'image_file', 'image_state', 'image_bytes_written'}
FALSE_FLAGS = {'native_admitted', 'display_proven', 'simulation_proven', 'fps_proven',
               'scene_admitted', 'cycle_timing_proven', 'legacy_cycle_transfer_eligible',
               'input_injected', 'desktop_capture', 'source_binding_independently_reviewed'}
TOP_KEYS = FALSE_FLAGS | {'schema', 'token', 'status', 'reason',
                        'readiness_sequence_not_pixel_identity', 'readiness_wait_ms',
                        'readiness_observed_qpc', 'scout_delay_seconds',
                        'diagnostic_buffer_limit_bytes', 'maximum_raw_capture_bytes',
                        'clock', 'pixel_format', 'file_format', 'interpretation', 'sequence'}
SEQUENCE_KEYS = {'begin_qpc', 'qpc_frequency', 'scheduled_slots', 'schedule_hz', 'width', 'height', 'slots'}
FAILED_REASONS = {'API_FAILED', 'API_UNSUPPORTED', 'METADATA_INVALID', 'STOPPED_AFTER_CAPTURE'}
COMPLETE = 'SOURCE_SCOUT_CENSUS_REVIEW_PENDING'
INCOMPLETE = 'SOURCE_SCOUT_CENSUS_INCOMPLETE_REVIEW_PENDING'
REASON = 'FIXED_DELAY_AND_SLOTS_NO_DISPLAY_SIMULATION_OR_TIMING_CLAIM'
INTERPRETATION = 'CPU_REFRESH_AND_READBACK_WITNESSES_NOT_PIXEL_AGE_OR_SIMULATION_TIME'
NATIVE_ADMITTED = False


def need(ok, reason):
    if not ok:
        raise ValueError(reason)


def exact(value, keys, label):
    need(type(value) is dict and set(value) == keys, label)


def uint(value, low=1, high=MAX_QPC):
    need(type(value) is int and low <= value <= high, 'bounded exact integer')
    return value


def _metadata(receipt, token):
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token), 'expected token shape')
    exact(receipt, TOP_KEYS, 'exact native receipt schema')
    need(receipt['schema'] == 'mcla-guest-source-scout-v1' and
         type(receipt['token']) is str and receipt['token'] == token, 'native schema/token binding')
    need(type(receipt['status']) is str and receipt['status'] in (COMPLETE, INCOMPLETE),
         'native failed or unknown terminal status')
    need(receipt['reason'] == REASON, 'native reason')
    need(all(receipt[key] is False for key in FALSE_FLAGS), 'unsupported native admission claim')
    need(receipt['clock'] == 'win32-query-performance-counter' and receipt['pixel_format'] == 'R8G8B8X8' and
         receipt['file_format'] == '24bit-BGR-bottom-up-BMP' and receipt['interpretation'] == INTERPRETATION,
         'clock/pixel/file interpretation')
    for key, expected in (('scout_delay_seconds', DELAY_SECONDS),
                          ('diagnostic_buffer_limit_bytes', BUFFER_LIMIT),
                          ('maximum_raw_capture_bytes', MAX_RAW_BYTES)):
        need(type(receipt[key]) is int and receipt[key] == expected, 'fixed scout delay/buffer contract')
    readiness = uint(receipt['readiness_sequence_not_pixel_identity'], 60, 2**64-1)
    uint(receipt['readiness_wait_ms'], 0, 89999)
    anchor = uint(receipt['readiness_observed_qpc'])
    sequence = receipt['sequence']
    exact(sequence, SEQUENCE_KEYS, 'exact sequence schema')
    begin = uint(sequence['begin_qpc'])
    frequency = uint(sequence['qpc_frequency'], 10, 10**10)
    need(begin == uint(anchor + DELAY_SECONDS * frequency), 'exact fixed delay from readiness observation')
    end = uint(begin + SLOTS * frequency // HZ)
    need(type(sequence['scheduled_slots']) is int and sequence['scheduled_slots'] == SLOTS and
         type(sequence['schedule_hz']) is int and sequence['schedule_hz'] == HZ,
         'fixed91 absolute100ms slots')
    need(type(sequence['width']) is int and sequence['width'] == WIDTH and
         type(sequence['height']) is int and sequence['height'] == HEIGHT, 'fixed1280x720 source')
    slots = sequence['slots']
    need(type(slots) is list and len(slots) == SLOTS, 'exact91 native slots')
    frozen, names = [], []
    # A stop while waiting the40s delay may precede begin; calls may not.
    previous_observed = anchor
    terminal = None
    epoch = None
    previous_source = None
    collected = 0
    for index, slot in enumerate(slots):
        exact(slot, SLOT_KEYS, 'exact native slot schema')
        need(type(slot['slot']) is int and slot['slot'] == index, 'ordered unique slots')
        due = begin + index * frequency // HZ
        next_due = begin + (index + 1) * frequency // HZ
        need(type(slot['due_qpc']) is int and slot['due_qpc'] == due, 'exact absolute slot schedule')
        observed = uint(slot['observed_qpc'])
        need(observed >= previous_observed, 'observation journal regression')
        status, reason = slot['status'], slot['reason']
        need(type(status) is str and status in ('COLLECTED', 'FAILED', 'MISSED_DEADLINE', 'NOT_ATTEMPTED'),
             'known slot status')
        if terminal is not None:
            need(status == 'NOT_ATTEMPTED' and reason == terminal, 'terminal suffix cannot retry')
        elif status == 'NOT_ATTEMPTED':
            need(reason in ('STOPPED', 'BUFFER_BUDGET_EXHAUSTED'), 'known initial unattempted reason')
            if reason == 'BUFFER_BUDGET_EXHAUSTED':
                need(due <= observed < next_due, 'buffer budget guard requires still-open slot')
                terminal = 'PREVIOUS_TERMINAL_FAILURE'
            else:
                terminal = 'PREVIOUS_STOP'
        name = f'slot-{index:02d}.bmp'
        need(type(slot['image_file']) is str and slot['image_file'] == name, 'fixed slot basename')
        image_bytes = uint(slot['image_bytes_written'], 0, BMP_BYTES)
        # Only validated scalar fields survive callback mutation of the receipt.
        row = {'slot': index, 'due_qpc': due, 'observed_qpc': observed, 'status': status,
               'reason': reason, 'capture': None, 'call_before_qpc': None, 'call_after_qpc': None}
        if status in ('NOT_ATTEMPTED', 'MISSED_DEADLINE'):
            need(slot['call_before_qpc'] is None and slot['call_after_qpc'] is None and slot['capture'] is None,
                 'unattempted slot contains stale capture')
            if status == 'MISSED_DEADLINE':
                need(reason == 'SLOT_WINDOW_ELAPSED' and observed >= next_due, 'miss requires expired slot')
        else:
            before, after = uint(slot['call_before_qpc']), uint(slot['call_after_qpc'])
            need(due <= before < next_due and before >= previous_observed and before <= after == observed,
                 'external call bracket and schedule')
            row.update(call_before_qpc=before, call_after_qpc=after)
            if status == 'FAILED':
                need(type(reason) is str and reason in FAILED_REASONS, 'known failed-call reason')
                need(slot['capture'] is None, 'failed call retains stale metadata')
                terminal = 'PREVIOUS_TERMINAL_FAILURE'
            else:
                need(reason is None, 'collected call has failure reason')
                capture = slot['capture']
                exact(capture, CAPTURE_KEYS, 'exact paired metadata schema')
                for key, value in capture.items():
                    uint(value, high=2**64-1 if key in ('presenter_epoch', 'acquired_sequence') else MAX_QPC)
                need(capture['qpc_frequency'] == frequency and
                     (capture['width'], capture['height'], capture['stride']) == (WIDTH, HEIGHT, WIDTH * 4),
                     'fixed paired QPF/shape/stride')
                pub_begin, pub_end = capture['refresh_cpu_begin_qpc'], capture['refresh_cpu_end_qpc']
                acq_before, acq_after = capture['acquire_before_qpc'], capture['acquire_after_qpc']
                submit, complete = capture['copy_submit_before_qpc'], capture['copy_completion_observed_qpc']
                ready = capture['pixels_ready_qpc']
                need(pub_begin <= pub_end <= acq_after and
                     before <= acq_before <= acq_after <= submit <= complete <= ready <= after,
                     'paired provenance enclosed by external call')
                if epoch is None:
                    epoch = capture['presenter_epoch']
                need(capture['presenter_epoch'] == epoch, 'presenter epoch changed')
                source = capture['acquired_sequence']
                need(source >= readiness and (previous_source is None or source >= previous_source[0]),
                     'source sequence regression or preceding readiness')
                if previous_source is not None:
                    if source == previous_source[0]:
                        need((pub_begin, pub_end) == previous_source[1:], 'duplicate source changed publication')
                    else:
                        need(pub_begin >= previous_source[2], 'new source publication reordered')
                previous_source = (source, pub_begin, pub_end)
                row['capture'] = dict(capture)
                need(slot['image_state'] == 'complete' and image_bytes == BMP_BYTES, 'collected image must be complete')
                names.append(name)
                collected += 1
        if status != 'COLLECTED':
            need(slot['image_state'] == 'absent' and image_bytes == 0, 'uncollected image must be absent')
        frozen.append(row)
        previous_observed = observed
    need(receipt['status'] == (COMPLETE if collected == SLOTS else INCOMPLETE), 'terminal status versus slot coverage')
    return tuple(frozen), tuple(names), begin, end, frequency, anchor


def validate_metadata(receipt, *, token):
    """Metadata-only live terminal validation; original receipt returned, no IO."""
    _metadata(receipt, token)
    return receipt


def _bmp_rgb(data):
    need(type(data) is bytes and len(data) == BMP_BYTES, 'canonical bounded immutable BMP bytes')
    need(data[:2] == b'BM' and struct.unpack_from('<IHHI', data, 2) == (BMP_BYTES, 0, 0, 54),
         'canonical BMP file header')
    need(struct.unpack_from('<IiiHHIIiiII', data, 14) ==
         (40, WIDTH, HEIGHT, 1, 24, 0, BMP_STRIDE * HEIGHT, 0, 0, 0, 0), 'canonical BMP DIB header')
    rgb = bytearray(WIDTH * HEIGHT * 3)
    for y in range(HEIGHT):
        source = 54 + (HEIGHT - 1 - y) * BMP_STRIDE
        row = data[source:source + BMP_STRIDE]
        target = y * BMP_STRIDE
        for channel in range(3):
            rgb[target + channel:target + BMP_STRIDE:3] = row[2 - channel::3]
    return bytes(rgb)


def decode(receipt, *, token, image_names, read_image):
    """Post-exit streaming census; callback reads each collected slot once in order.

    Complete metadata/inventory gates precede every image read. Pixel failure
    rejects the result, never replaces a frame. Only the preceding collected
    RGB is retained; monotonic source IDs make this sufficient for duplicates,
    even across missed slots. Returned arrays contain scalars/hashes, no pixels.
    """
    rows, wanted, begin, end, frequency, anchor = _metadata(receipt, token)
    need(type(image_names) in (list, tuple) and len(image_names) <= SLOTS and
         all(type(name) is str for name in image_names), 'bounded BMP-only inventory')
    names = tuple(image_names)
    need(len(set(names)) == len(names) and set(names) == set(wanted), 'exact unique BMP-only inventory')
    need(callable(read_image), 'injected image reader required')
    counts = {key: 0 for key in ('COLLECTED', 'FAILED', 'MISSED_DEADLINE', 'NOT_ATTEMPTED')}
    images, observations, windows, calls = [], [], [], []
    previous_rgb = None
    previous_source = None
    duplicates = unique = 0
    for row in rows:
        index, status = row['slot'], row['status']
        counts[status] += 1
        observation = {key: row[key] for key in ('slot', 'due_qpc', 'observed_qpc', 'status', 'reason')}
        observation.update(source_identity=None, rgb_sha256=None)
        before, after = row['call_before_qpc'], row['call_after_qpc']
        calls.append(None if before is None else {'slot': index, 'before_qpc': before,
                     'after_qpc': after, 'call_ticks': after - before})
        if status != 'COLLECTED':
            windows.append(None)
        else:
            name = f'slot-{index:02d}.bmp'
            raw = read_image(name)
            rgb = _bmp_rgb(raw)
            p = row['capture']
            source = (p['presenter_epoch'], p['acquired_sequence'])
            if source == previous_source:
                need(rgb == previous_rgb, 'same source identity changed RGB')
                duplicates += 1
            else:
                unique += 1
            previous_source, previous_rgb = source, rgb
            images.append({'slot': index, 'name': name, 'bytes': len(raw),
                           'sha256': hashlib.sha256(raw).hexdigest().upper()})
            observation.update(source_identity=source, rgb_sha256=hashlib.sha256(rgb).hexdigest().upper())
            windows.append({'slot': index, 'source_identity': source,
                            'refresh_cpu_begin_qpc': p['refresh_cpu_begin_qpc'],
                            'refresh_cpu_end_qpc': p['refresh_cpu_end_qpc'],
                            'pixels_ready_qpc': p['pixels_ready_qpc'], 'external_after_qpc': after,
                            'refresh_to_pixels_ready_ticks': p['pixels_ready_qpc'] - p['refresh_cpu_begin_qpc'],
                            'refresh_to_external_after_ticks': after - p['refresh_cpu_begin_qpc'],
                            'copy_submit_to_completion_observed_ticks':
                            p['copy_completion_observed_qpc'] - p['copy_submit_before_qpc']})
            del raw, rgb
        observations.append(observation)
    census = {'schema': 'mcla-guest-source-scout-census-v1', 'status': COMPLETE if counts['COLLECTED'] == SLOTS else INCOMPLETE,
              'scheduled_slots': SLOTS, 'schedule_hz': HZ, 'begin_qpc': begin, 'schedule_end_qpc': end,
              'qpc_frequency': frequency, 'readiness_observed_qpc': anchor, 'scout_delay_seconds': DELAY_SECONDS,
              'width': WIDTH, 'height': HEIGHT, 'counts': counts,
              'attempted_calls': counts['COLLECTED'] + counts['FAILED'],
              'all_slots_collected': counts['COLLECTED'] == SLOTS,
              'duplicate_sources': duplicates, 'unique_sources': unique,
              'observations': tuple(observations), 'source_witness_spans': tuple(windows),
              'external_call_costs': tuple(calls), 'interpretation': INTERPRETATION}
    census.update(dict.fromkeys(FALSE_FLAGS, False))
    result = {'schema': 'mcla-game-source-scout-decode-v1', 'status': 'SOURCE_SCOUT_DECODED_REVIEW_PENDING',
              'images': tuple(images), 'census': census}
    result.update(dict.fromkeys(FALSE_FLAGS, False))
    return result
