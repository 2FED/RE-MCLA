"""Strict pure decoder of bounded native timer observations, not simulation.

Expected token/window/frequency come from separately validated source-scout
metadata. Process/binary ownership is the caller's separate gate. Opaque state
bits remain opaque, including IEEE special values. No IO, real clocks, fitting,
delta sums, rates, game-memory reads or object-lifetime inference occur here.
"""
import hashlib
import json
import re

MAX_BYTES = 4 * 1024 * 1024
CAPACITY = 1024
U32, U64, MAX_QPC = 2**32 - 1, 2**64 - 1, 2**63 - 1
HOOK_ADDRESS = 0x821BDA90
FALSE_FLAGS = {'state_coherence_proven', 'object_lifetime_proven',
               'simulation_proven', 'fps_proven', 'frame_to_step_mapping_proven',
               'native_admitted'}
COUNT_FIELDS = {'observed_entries', 'admitted_entries', 'stored_entries',
                'completed_entries', 'overflow_entries', 'outside_before_entries',
                'outside_after_entries'}
COUNT_KEYS = COUNT_FIELDS | {'counter_saturated', 'observer_failed'}
TOP_KEYS = FALSE_FLAGS | {'schema', 'token', 'status', 'clock', 'thread_identity_domain',
                         'hook_address', 'capacity', 'begin_qpc', 'end_qpc_exclusive',
                         'qpc_frequency', 'counts', 'records'}
RECORD_KEYS = {'entry_ordinal', 'callback_issues', 'timer_address', 'caller_address',
               'native_thread_id', 'entry_qpc', 'exit_qpc', 'incoming_r5_low_byte',
               'incoming_f1_bits', 'before', 'after'}


def need(ok, reason):
    if not ok:
        raise ValueError(reason)


def exact(value, keys, label):
    need(type(value) is dict and set(value) == keys, label)


def uint(value, high=U64, low=0):
    need(type(value) is int and low <= value <= high, 'bounded exact unsigned integer')
    return value


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def _integer(text):
    need(len(text) <= 20 and text.isascii() and text.isdecimal(), 'native unsigned JSON integer')
    return int(text)


def _no_float(_):
    raise ValueError('floating/NaN/Infinity JSON value unsupported')


def _state(value):
    if value is None:
        return None
    exact(value, {'words', 'bytes', 'qword64'}, 'exact opaque state schema')
    for key, count, limit in (('words', 18, U32), ('bytes', 3, 255)):
        values = value[key]
        need(type(values) is list and len(values) == count, 'exact opaque state field count')
        for item in values:
            uint(item, limit)
    uint(value['qword64'])
    return {'words': list(value['words']), 'bytes': list(value['bytes']),
            'qword64': value['qword64']}


def _counts(value, records):
    exact(value, COUNT_KEYS, 'exact counts schema')
    for key in COUNT_FIELDS:
        uint(value[key])
    for key in ('counter_saturated', 'observer_failed'):
        need(type(value[key]) is bool, 'exact diagnostic count flag')
    need(not value['counter_saturated'] or any(value[key] == U64 for key in COUNT_FIELDS),
         'saturated flag requires a saturated counter')
    stored, admitted, completed = (value[key] for key in
                                  ('stored_entries', 'admitted_entries', 'completed_entries'))
    need(stored == len(records) <= CAPACITY and stored <= completed <= admitted,
         'retained/completed/admitted counts')
    def total(*keys):
        summed = sum(value[key] for key in keys)
        return min(U64, summed) if value['counter_saturated'] else summed
    need(value['observed_entries'] == total('admitted_entries', 'overflow_entries'),
         'observed versus admitted/overflow counts')
    need(completed == total('stored_entries', 'outside_before_entries', 'outside_after_entries'),
         'completed versus retained/outside counts')
    need(value['observer_failed'] or admitted == completed,
         'successful freeze must drain all admissions')
    return dict(value)


def decode(data, *, token, begin_qpc, end_qpc, qpc_frequency):
    """Decode native Serialize bytes and return a JSON-serializable diagnostic.

    Raw completion-order rows are retained. Cross-call analysis uses only the
    explicitly supplied unique entry ordinal, never sorts/fits clocks. Zero,
    backwards or beyond-signed-QPC observations and absent state are retained as
    evidence failures. Native integer overflow/unknown bits/schema errors reject.
    """
    need(type(data) is bytes and 0 < len(data) <= MAX_BYTES, 'bounded immutable receipt bytes')
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token), 'expected token shape')
    uint(begin_qpc, MAX_QPC, 1); uint(end_qpc, MAX_QPC, 1)
    uint(qpc_frequency, 10**10, 10)
    need(end_qpc == begin_qpc + 91 * qpc_frequency // 10,
         'exact shared91-slot observation window')
    try:
        value = json.loads(data.decode('utf-8'), object_pairs_hook=_pairs,
                           parse_int=_integer, parse_float=_no_float, parse_constant=_no_float)
    except (UnicodeError, RecursionError) as error:
        raise ValueError('invalid bounded JSON encoding/nesting') from error
    exact(value, TOP_KEYS, 'exact timer receipt schema')
    need(value['schema'] == 'mcla-timer-observer-v1' and value['token'] == token and
         value['status'] == 'TIMER_OBSERVATIONS_REVIEW_PENDING', 'schema/token/status binding')
    need(value['clock'] == 'win32-query-performance-counter' and
         value['thread_identity_domain'] == 'win32-native-thread-id-reuse-possible',
         'exact clock/native thread interpretation')
    for key, expected in (('hook_address', HOOK_ADDRESS), ('capacity', CAPACITY),
                          ('begin_qpc', begin_qpc), ('end_qpc_exclusive', end_qpc),
                          ('qpc_frequency', qpc_frequency)):
        need(type(value[key]) is int and value[key] == expected, 'exact declared integer/window binding')
    need(all(value[key] is False for key in FALSE_FLAGS), 'unsupported proven/admission claim')
    records = value['records']
    need(type(records) is list and len(records) <= CAPACITY, 'bounded native record array')
    counts = _counts(value['counts'], records)
    reasons = set()
    if counts['counter_saturated']: reasons.add('COUNTER_SATURATED')
    if counts['observer_failed']: reasons.add('OBSERVER_FAILED_OR_INCOMPLETE_PUBLICATION')
    if counts['overflow_entries']: reasons.add('STORAGE_OR_RESERVATION_OVERFLOW')
    if not records: reasons.add('NO_RETAINED_TIMER_OBSERVATIONS')
    rows, seen_ordinals = [], set()
    callback_mask = 0
    for record in records:
        exact(record, RECORD_KEYS, 'exact raw timer record schema')
        ordinal = uint(record['entry_ordinal'], counts['observed_entries'], 1)
        need(ordinal not in seen_ordinals, 'duplicate entry ordinal')
        seen_ordinals.add(ordinal)
        mask = uint(record['callback_issues'], 31)
        for key in ('timer_address', 'caller_address', 'native_thread_id'):
            uint(record[key], U32)
        entry, exit_ = uint(record['entry_qpc']), uint(record['exit_qpc'])
        uint(record['incoming_r5_low_byte'], 255); uint(record['incoming_f1_bits'])
        before, after = _state(record['before']), _state(record['after'])
        need(not mask & 1 or entry == 0, 'failed entry clock must be cleared')
        need(not mask & 2 or exit_ == 0, 'failed exit clock must be cleared')
        need(not mask & 4 or before is None, 'failed before-state must be cleared')
        need(not mask & 8 or after is None, 'failed after-state must be cleared')
        callback_mask |= mask
        issues = set()
        clock_valid = 0 < entry <= exit_ <= MAX_QPC
        identity_valid = all(record[key] != 0 for key in ('timer_address', 'caller_address', 'native_thread_id'))
        if not clock_valid: issues.add('INVALID_OUTER_CLOCK_WITNESS')
        if not identity_valid: issues.add('MISSING_OBSERVED_IDENTITY')
        if before is None or after is None: issues.add('MISSING_OPAQUE_STATE')
        if mask: issues.add('CALLBACK_FAILURE_OR_ORIGINAL_UNWIND')
        inside = clock_valid and entry >= begin_qpc and exit_ < end_qpc
        if clock_valid:
            if entry < begin_qpc <= exit_: issues.add('STRADDLES_OR_TOUCHES_WINDOW_BEGIN')
            if entry < end_qpc <= exit_: issues.add('STRADDLES_OR_TOUCHES_WINDOW_END')
            outside = exit_ < begin_qpc or entry >= end_qpc
            need(not (outside and identity_valid and before is not None and after is not None and mask == 0),
                 'healthy wholly-outside call should be counted, not retained')
            if outside: issues.add('RETAINED_DIAGNOSTIC_OUTSIDE_WINDOW')
        row = {key: record[key] for key in RECORD_KEYS - {'before', 'after'}}
        row.update(before=before, after=after, outer_qpc_witness=[entry, exit_],
                   raw_outer_elapsed_ticks=exit_ - entry if clock_valid else None,
                   clock_witness_valid=clock_valid, wholly_inside_declared_window=inside,
                   diagnostic_issues=issues)
        rows.append(row)

    # No clock sorting or monotonic repair. Completion-order output remains
    # untouched; admission order is an independently recorded ordinal domain.
    ordered = sorted(range(len(rows)), key=lambda index: rows[index]['entry_ordinal'])
    previous_valid = None
    overlap_pairs = 0
    for position, index in enumerate(ordered):
        row = rows[index]
        if not row['clock_witness_valid']: continue
        if previous_valid is not None:
            if row['entry_qpc'] < previous_valid['entry_qpc']:
                row['diagnostic_issues'].add('ENTRY_CLOCK_ORDER_REGRESSED')
            if row['exit_qpc'] < previous_valid['exit_qpc']:
                row['diagnostic_issues'].add('EXIT_CLOCK_ORDER_REGRESSED')
        previous_valid = row
        for old_position in range(position):
            old_index = ordered[old_position]
            old = rows[old_index]
            if not old['clock_witness_valid']: continue
            if row['entry_qpc'] <= old['exit_qpc'] and old['entry_qpc'] <= row['exit_qpc']:
                label = 'OUTER_WITNESSES_OVERLAP_OR_TOUCH'
                row['diagnostic_issues'].add(label); old['diagnostic_issues'].add(label)
                if row['timer_address'] and row['timer_address'] == old['timer_address']:
                    label = 'SHARED_ADDRESS_WITNESSES_OVERLAP_OR_TOUCH'
                    row['diagnostic_issues'].add(label); old['diagnostic_issues'].add(label)
                # Count rather than persist the O(n^2) pair inventory.
                overlap_pairs += 1
    groups = {}
    for index in ordered:
        row = rows[index]
        identity = (row['timer_address'], row['caller_address'], row['native_thread_id'])
        group = groups.setdefault(identity, {'timer_address': identity[0], 'caller_address': identity[1],
                                            'native_thread_id': identity[2], 'record_indices': [],
                                            'adjacent_outer_witness_differences': []})
        if group['record_indices']:
            old = rows[group['record_indices'][-1]]
            bounds = ([row['entry_qpc'] - old['exit_qpc'], row['exit_qpc'] - old['entry_qpc']]
                      if row['clock_witness_valid'] and old['clock_witness_valid'] else None)
            group['adjacent_outer_witness_differences'].append({
                'from_entry_ordinal': old['entry_ordinal'], 'to_entry_ordinal': row['entry_ordinal'],
                'raw_interval_difference_ticks_bounds': bounds})
        group['record_indices'].append(index)
    for row in rows:
        row['diagnostic_issues'] = sorted(set(row['diagnostic_issues']))
        reasons.update(row['diagnostic_issues'])
    result = {'schema': 'mcla-timer-observation-decode-v1',
              'status': 'TIMER_OBSERVATIONS_DIAGNOSTIC_INCONCLUSIVE' if reasons else 'TIMER_OBSERVATIONS_DECODED_REVIEW_PENDING',
              'interpretation': 'OPAQUE_TIMER_CALL_OUTER_WITNESSES_NOT_SIMULATION_STEPS_OR_PIXEL_TIMESTAMPS',
              'token': token, 'begin_qpc': begin_qpc, 'end_qpc_exclusive': end_qpc,
              'qpc_frequency': qpc_frequency, 'receipt_sha256': hashlib.sha256(data).hexdigest().upper(),
              'counts': counts, 'raw_callback_issues': callback_mask,
              'records': rows, 'admission_order_record_indices': ordered,
              'identity_summaries': list(groups.values()), 'overlap_or_touch_pairs': overlap_pairs,
              'diagnostic_issues': sorted(reasons),
              'limitations': ['Timer/caller/native-thread tuples do not establish object lifetimes or consumers.',
                              'State availability does not establish a coherent memory snapshot.',
                              'Outer spans include both snapshot attempts, original work and preemption.',
                              'Process/binary/session ownership requires separate supervisor review.']}
    result.update(dict.fromkeys(FALSE_FLAGS, False))
    return result
