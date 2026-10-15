"""Pure metadata-only audit of a fixed scout's transported swap tokens.

The supervisor owns strict JSON parsing/128KiB limits, immutable input pins,
pixels and process ownership. This module neither reads images nor associates
nearby timestamps. A bound token is CPU-emission/resource provenance only;
the opaque timer words do not prove a simulation step or coherent state.
Statistics are explicitly non-quiescent descriptive samples, not acceptance.
"""
import re

from mcla_game_source_scout_decode import resolve_source_schedule

U32, U64, MAX_QPC = 2**32 - 1, 2**64 - 1, 2**63 - 1
SLOTS, CAPACITY = 91, 1024
TIMER_ADDRESS, STOCK_MAGIC = 0x827D7500, 0x53574150
FALSE_FLAGS = {'simulation_proven', 'frame_to_step_mapping_proven',
               'display_proven', 'fps_proven', 'statistics_quiescent'}
TOP_KEYS = FALSE_FLAGS | {'schema', 'token', 'clock', 'epoch', 'begin_qpc',
                         'end_qpc_exclusive', 'qpc_frequency', 'timer_address',
                         'statistics', 'records'}
STAT_KEYS = {'issued', 'matched', 'rejected', 'callback_failed', 'failed'}
ROW_KEYS = {'slot', 'presenter_epoch', 'acquired_sequence', 'binding'}
BINDING_KEYS = {'epoch', 'serial', 'stock', 'fetch', 'consume_qpc',
                'snapshot_result', 'snapshot'}
SNAPSHOT_KEYS = {'qpc_before', 'qpc_after', 'native_thread_id', 'guest_caller_lr',
                 'timer_address', 'timer_counter', 'timer_delta_bits'}
SNAPSHOT_RESULTS = {'available', 'unavailable', 'invalid_qpc', 'missing_identity'}


def need(ok, message):
    if not ok:
        raise ValueError(message)


def exact(value, keys, label):
    need(type(value) is dict and set(value) == keys, label)


def uint(value, high=U64, low=0):
    need(type(value) is int and low <= value <= high, 'bounded exact integer')
    return value


def words(value, count):
    need(type(value) is list and len(value) == count, 'exact stock/fetch word count')
    return [uint(word, U32) for word in value]


def _binding(value, *, epoch, begin, end, refresh_begin):
    exact(value, BINDING_KEYS, 'exact binding schema')
    need(uint(value['epoch'], low=1) == epoch, 'binding token epoch')
    serial = uint(value['serial'], CAPACITY, 1)
    stock, fetch = words(value['stock'], 4), words(value['fetch'], 6)
    need(stock[0] == STOCK_MAGIC and stock[2:] == [1280, 720], 'stock magic/source shape')
    consume = uint(value['consume_qpc'], MAX_QPC, 1)
    need(begin <= consume <= refresh_begin, 'packet consume outside issuance/source chronology')
    result = value['snapshot_result']
    need(type(result) is str and result in SNAPSHOT_RESULTS, 'known snapshot result')
    snapshot = value['snapshot']
    if result == 'available':
        exact(snapshot, SNAPSHOT_KEYS, 'exact available snapshot schema')
        before = uint(snapshot['qpc_before'], MAX_QPC, 1)
        after = uint(snapshot['qpc_after'], MAX_QPC, 1)
        for key in ('native_thread_id', 'guest_caller_lr', 'timer_address'):
            uint(snapshot[key], U32, 1)
        for key in ('timer_counter', 'timer_delta_bits'):
            uint(snapshot[key], U32)
        need(snapshot['timer_address'] == TIMER_ADDRESS, 'allowlisted timer address')
        need(begin <= before <= after < end and after <= consume,
             'snapshot window/packet chronology')
        snapshot = dict(snapshot)
    else:
        need(snapshot is None, 'unavailable snapshot must clear all raw fields')
    # Every returned container is detached; no caller-owned nested structures.
    return {'epoch': epoch, 'serial': serial, 'stock': stock, 'fetch': fetch,
            'consume_qpc': consume, 'snapshot_result': result, 'snapshot': snapshot}


def validate_swap_receipt(data, scout_receipt, expected_token):
    """Validate parsed exact dictionaries; return detached finite-profile rows.

    Scout validation is pure metadata only. The supervisor must separately
    validate its pixels/inventory, and must reject duplicate JSON keys before
    calling this function. Missing captures and unbound captures remain holes.
    Post-collection counters bound already captured tokens from below, but
    cannot establish complete coverage or timer-counter/rendered-frame equivalence.
    """
    need(type(expected_token) is str and re.fullmatch('[0-9a-f]{32}', expected_token),
         'expected lowercase32hex token')
    epoch = uint(int(expected_token[:16], 16), low=1)
    sequence = resolve_source_schedule(scout_receipt, token=expected_token)
    frequency = sequence['qpc_frequency']
    begin = sequence['journal_begin_qpc']
    end = sequence['schedule_end_qpc']
    slot_count = sequence['scheduled_slots']
    need(0 < end - begin <= 12 * frequency, 'bounded swap window')
    exact(data, TOP_KEYS, 'exact swap receipt schema')
    for key, expected in (('schema', 'mcla-game-swap-observer-v1'),
                          ('clock', 'win32-query-performance-counter'),
                          ('token', expected_token)):
        need(type(data[key]) is str and data[key] == expected, 'schema/clock/token binding')
    need(all(data[key] is False for key in FALSE_FLAGS), 'unsupported proof/quiescence claim')
    for key, expected, high in (('epoch', epoch, U64), ('begin_qpc', begin, MAX_QPC),
                                ('end_qpc_exclusive', end, MAX_QPC),
                                ('qpc_frequency', frequency, 10**10),
                                ('timer_address', TIMER_ADDRESS, U32)):
        need(uint(data[key], high, 1) == expected, 'shared exact window/epoch/timer')
    stats = data['statistics']
    exact(stats, STAT_KEYS, 'exact statistics schema')
    issued = uint(stats['issued'], CAPACITY)
    matched = uint(stats['matched'], CAPACITY)
    need(matched <= issued, 'matched count exceeds issued')
    rejected = uint(stats['rejected']); callback_failed = uint(stats['callback_failed'])
    need(callback_failed <= issued + rejected, 'callback failures exceed attempted issuance')
    need(type(stats['failed']) is bool, 'exact failed boolean')
    records = data['records']
    need(type(records) is list and len(records) == slot_count, 'exact profile swap records')
    rows, sources, tokens = [], {}, {}
    counts = {'bound': 0, 'unbound': 0, 'missing': 0, 'available_snapshots': 0,
              'unavailable_snapshots': 0, 'duplicate_sources': 0}
    previous_serial = None
    for index, (record, scout_row) in enumerate(zip(records, sequence['slots'])):
        exact(record, ROW_KEYS, 'exact swap row schema')
        need(uint(record['slot'], slot_count - 1) == index, 'ordered unique swap slots')
        capture = scout_row['capture']
        if capture is None:
            need(all(record[key] is None for key in ROW_KEYS - {'slot'}),
                 'missing scout capture retains stale swap identity/binding')
            counts['missing'] += 1
            rows.append({'slot': index, 'presenter_epoch': None,
                         'acquired_sequence': None, 'binding': None})
            continue
        identity = (uint(record['presenter_epoch'], low=1),
                    uint(record['acquired_sequence'], low=1))
        need(identity == (capture['presenter_epoch'], capture['acquired_sequence']),
             'exact captured source identity')
        binding = record['binding']
        if binding is not None:
            binding = _binding(binding, epoch=epoch, begin=begin, end=end,
                               refresh_begin=capture['refresh_cpu_begin_qpc'])
            need(binding['serial'] <= issued, 'captured token exceeds issued count')
            counts['bound'] += 1
            counts['available_snapshots' if binding['snapshot'] is not None
                   else 'unavailable_snapshots'] += 1
        else:
            counts['unbound'] += 1
        if identity in sources:
            need(binding == sources[identity], 'duplicate source changed binding including null')
            counts['duplicate_sources'] += 1
        else:
            if binding is not None:
                token = (epoch, binding['serial'])
                need(token not in tokens, 'distinct source reused swap token')
                need(previous_serial is None or binding['serial'] >= previous_serial,
                     'token serial regressed across increasing bound sources')
                tokens[token] = identity
                previous_serial = binding['serial']
            sources[identity] = binding
        rows.append({'slot': index, 'presenter_epoch': identity[0],
                     'acquired_sequence': identity[1], 'binding': binding})
    need(len(tokens) <= matched, 'captured unique tokens exceed matched count')
    issues = []
    for key in ('rejected', 'callback_failed', 'failed'):
        if stats[key]: issues.append('statistics_' + key)
    for key in ('missing', 'unbound', 'unavailable_snapshots'):
        if counts[key]: issues.append(key)
    result = {'schema': 'mcla-game-swap-decode-v1', 'token': expected_token,
              'status': 'SWAP_BINDING_REVIEW_PENDING', 'epoch': epoch,
              'begin_qpc': begin, 'end_qpc_exclusive': end, 'qpc_frequency': frequency,
              'timer_address': TIMER_ADDRESS, 'statistics': dict(stats),
              'counts': counts, 'unique_sources': len(sources),
              'unique_bound_tokens': len(tokens), 'records': rows,
              'diagnostic_inconclusive': bool(issues), 'diagnostic_issues': issues,
              'interpretation': 'CPU_SWAP_EMISSION_RESOURCE_BINDING_NOT_SIMULATION_DISPLAY_OR_FPS',
              'native_admitted': False, 'scene_admitted': False, 'cycle_timing_proven': False}
    result.update(dict.fromkeys(FALSE_FLAGS, False))
    return result
