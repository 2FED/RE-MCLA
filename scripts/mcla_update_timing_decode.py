"""Strict offline opaque timing inventory -> exact active-Update ID linkage.

No IO, clocks, FPR conversion, game units, sums, nearest-time matching, global
latest update, inferred steps or actual-consumer claim. Complete journals first
validate; only explicit phase IDs select groups. QPC/TID values reject edges.
The phase epoch is NOT the frame-handoff host tracker epoch. Raw pending rows
cannot be completed using the phase journal's later drain.
"""
import copy
import hashlib
import re

import mcla_frame_handoff_decode as handoff
import mcla_frame_handoff_closed_prefix as closed_prefix

MAX_BYTES, CAPACITY = 8*1024*1024, 4096
U32, U64, MAX_QPC = 2**32-1, 2**64-1, 2**63-1
TIMER_ADDRESS = 0x827D7500
UPDATE_CALLER = 0x821316A0
FALSE_FLAGS = {'native_admitted', 'coverage_proven', 'actual_consumer_proven',
    'state_coherence_proven', 'object_lifetime_proven', 'simulation_proven',
    'frame_to_step_mapping_proven', 'fps_proven'}
TOP_KEYS = FALSE_FLAGS | {'schema', 'token', 'clock', 'capacity', 'begin_qpc',
    'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc', 'observer_failed',
    'clock_failed', 'counts', 'records'}
COUNT_KEYS = {'observed_entries', 'admitted_entries', 'stored_entries',
    'completed_entries', 'pending_entries', 'outside_before_entries',
    'outside_after_entries', 'invalid_clock_entries', 'unbound_entries',
    'overflow_entries', 'read_failures', 'binding_mismatches', 'original_exceptions',
    'observation_errors', 'counter_saturated'}
ROW_KEYS = {'observation_id', 'kind', 'caller_lr', 'native_thread_id', 'entry_qpc',
    'exit_qpc', 'binding', 'expected_route', 'incoming', 'before', 'after',
    'completed', 'normal_return', 'exit_binding_matches'}
BINDING_KEYS = {'phase_call_id', 'thread_epoch', 'native_thread_id'}
INCOMING_KEYS = {'r3', 'r4', 'r5', 'f1_bits', 'f28_bits', 'f30_bits', 'f31_bits',
                 'loop_r23', 'loop_r24'}
STATE_KEYS = {'words', 'bytes', 'qword64', 'reciprocal_bits', 'count',
              'saved_unsplit_bits', 'split_state'}
TIMER_LRS = {0x822C21B4, 0x822C22E4}
SUBDIVISION_LRS = {0x822C230C, 0x822C246C, 0x822C267C}
ROUTER_SELECTORS = {0x822C24C4: 1, 0x822C273C: 4, 0x822C2768: 0,
                    0x822C27C0: 8, 0x822C2848: 2}
need, exact, uint, boolean = handoff.need, handoff.exact, handoff.uint, handoff.boolean


def _expected(row):
    incoming = row['incoming']; pointer = incoming['r3'] & U32
    if row['kind'] == 'timer':
        return row['caller_lr'] in TIMER_LRS and pointer == TIMER_ADDRESS
    if row['kind'] == 'subdivision':
        return row['caller_lr'] in SUBDIVISION_LRS and pointer == TIMER_ADDRESS
    return (pointer != 0 and row['caller_lr'] in ROUTER_SELECTORS and
            (incoming['r4'] & U32) == ROUTER_SELECTORS[row['caller_lr']])


def _state(value):
    if value is None: return
    exact(value, STATE_KEYS, 'exact opaque step-timer state')
    need(type(value['words']) is list and len(value['words']) == 18, 'exact old word allowlist')
    need(type(value['bytes']) is list and len(value['bytes']) == 3, 'exact old byte allowlist')
    for item in value['words']: uint(item, U32)
    for item in value['bytes']: uint(item, 255)
    uint(value['qword64'])
    for key in ('reciprocal_bits', 'count', 'saved_unsplit_bits'): uint(value[key], U32)
    uint(value['split_state'], 255)


def _metadata(data, token):
    """Validate ALL timing rows/types/counts before any extra evaluator."""
    exact(data, TOP_KEYS, 'exact update-timing schema')
    need(data['schema'] == 'mcla-update-timing-v1' and data['token'] == token and
         data['clock'] == 'win32-query-performance-counter', 'timing schema/session/clock')
    need(uint(data['capacity']) == CAPACITY, 'fixed timing capacity')
    for key in FALSE_FLAGS: need(data[key] is False, 'all timing proof flags false')
    boolean(data['observer_failed']); boolean(data['clock_failed'])
    begin = uint(data['begin_qpc'], MAX_QPC, 1)
    end = uint(data['end_qpc_exclusive'], MAX_QPC, 1)
    frequency = uint(data['qpc_frequency'], 10000000000, 1)
    need(begin < end and end-begin <= 12*frequency, 'bounded positive timing window')
    uint(data['freeze_qpc'])  # Invalid recorded cuts remain explicit raw evidence.
    counts = data['counts']; exact(counts, COUNT_KEYS, 'exact timing counters')
    for key in COUNT_KEYS - {'counter_saturated'}: uint(counts[key])
    boolean(counts['counter_saturated'])
    need(type(data['records']) is list and len(data['records']) <= CAPACITY, 'bounded timing records')
    ids, completed, unbound, mismatches, exceptions = set(), 0, 0, 0, 0
    required_read_failures, pending_unknown_read_failures = 0, 0
    issues, contexts = set(), set()
    for key in ('observer_failed', 'clock_failed'):
        if data[key]: issues.add(key.upper())
    for key in ('invalid_clock_entries', 'overflow_entries', 'read_failures',
                'binding_mismatches', 'original_exceptions', 'observation_errors', 'counter_saturated'):
        if counts[key]: issues.add(key.upper())
    clock_fault = data['clock_failed'] and data['observer_failed']
    need(not data['clock_failed'] or data['observer_failed'], 'clock failure requires observer failure')
    for key in ('invalid_clock_entries', 'overflow_entries', 'binding_mismatches',
                'observation_errors', 'counter_saturated'):
        if counts[key]: need(data['observer_failed'], 'sticky counter fault requires observer failure')
    if counts['invalid_clock_entries']: need(clock_fault, 'rejected entry clock fault flags')
    for row in data['records']:
        exact(row, ROW_KEYS, 'exact timing row')
        oid = uint(row['observation_id'], CAPACITY, 1)
        need(oid not in ids, 'duplicate timing observation ID'); ids.add(oid)
        need(type(row['kind']) is str and row['kind'] in {'timer', 'subdivision', 'router'}, 'timing kind')
        uint(row['caller_lr']); uint(row['native_thread_id'], U32, 1)
        uint(row['entry_qpc']); uint(row['exit_qpc'])
        for key in ('expected_route', 'completed', 'normal_return', 'exit_binding_matches'): boolean(row[key])
        exact(row['incoming'], INCOMING_KEYS, 'exact opaque incoming registers')
        for key, value in row['incoming'].items(): uint(value, U32 if key.startswith('loop_') else U64)
        if not (row['kind'] == 'subdivision' and row['caller_lr'] == 0x822C246C):
            need(row['incoming']['loop_r23'] == row['incoming']['loop_r24'] == 0, 'loop fields only at exact subdivision caller')
        need(row['expected_route'] is _expected(row), 'exact low32 route predicate')
        _state(row['before']); _state(row['after'])
        binding = row['binding']
        if binding is not None:
            exact(binding, BINDING_KEYS, 'exact phase binding')
            uint(binding['phase_call_id'], U64-1, 1); uint(binding['thread_epoch'], U64-1, 1)
            need(uint(binding['native_thread_id'], U32, 1) == row['native_thread_id'], 'binding current native thread')
        else:
            need(not row['exit_binding_matches'], 'unbound row cannot match exit binding')
        readable = binding is not None and row['expected_route']
        if not readable:
            need(row['before'] is None and row['after'] is None, 'no state reads on unbound/unknown route')
            unbound += 1
            contexts.add('UNBOUND_OR_UNKNOWN_ROUTE_ROWS')
        entry_valid = 0 < row['entry_qpc'] <= MAX_QPC
        if entry_valid: need(begin <= row['entry_qpc'] < end, 'stored timing entry inside configured window')
        else: need(clock_fault, 'invalid recorded entry requires both clock fault flags')
        if row['completed']:
            completed += 1; exceptions += int(not row['normal_return'])
            valid_exit = entry_valid and row['entry_qpc'] <= row['exit_qpc'] <= MAX_QPC
            if not valid_exit: need(clock_fault, 'invalid completed clock requires both fault flags')
            if binding is not None and not row['exit_binding_matches']: mismatches += 1
            if not row['exit_binding_matches']:
                need(row['after'] is None, 'mismatched/unbound exit has no after state')
            if readable:
                required_read_failures += int(row['before'] is None)
                required_read_failures += int(row['exit_binding_matches'] and row['after'] is None)
        else:
            need(row['exit_qpc'] == 0 and not row['normal_return'] and not row['exit_binding_matches'] and
                 row['after'] is None, 'pending timing outcome not backfilled')
            pending_unknown_read_failures += int(readable and row['before'] is None)
            contexts.add('PENDING_TIMING_ROWS')
    stored = len(ids)
    need(ids == set(range(1, stored+1)), 'first stored admission ordinals are complete')
    need(counts['stored_entries'] == stored and counts['completed_entries'] == completed and
         counts['pending_entries'] == stored-completed, 'stored/completed/pending conservation')
    if not counts['counter_saturated']:
        need(counts['admitted_entries'] == stored+counts['overflow_entries'], 'admitted/storage/overflow conservation')
        need(counts['observed_entries'] == counts['admitted_entries'] + counts['outside_before_entries'] +
             counts['outside_after_entries'] + counts['invalid_clock_entries'], 'observed entry partition')
        need(unbound <= counts['unbound_entries'] <= unbound+counts['overflow_entries'], 'stored/overflow unbound bounds')
        if not counts['overflow_entries']: need(counts['unbound_entries'] == unbound, 'exact stored unbound counter')
        need(counts['binding_mismatches'] == mismatches and counts['original_exceptions'] == exceptions,
             'completed binding/exception counters')
        need(required_read_failures <= counts['read_failures'] <=
             required_read_failures+pending_unknown_read_failures, 'read failures match published attempts only')
    need(not counts['overflow_entries'] or stored == CAPACITY, 'overflow requires full storage')
    # IDs order entry sampling under the native observer mutex; never clock-sort
    # or fit timestamps. Exit order can differ because original calls overlap.
    last_entry = None
    for row in sorted(data['records'], key=lambda row: row['observation_id']):
        if last_entry is not None and row['entry_qpc'] < last_entry:
            need(clock_fault, 'entry admission clock regression requires fault flags')
        last_entry = row['entry_qpc']
    return issues, contexts


def _cross_references(data, closed):
    phase = closed['phase_inventory']
    for key in ('begin_qpc', 'end_qpc_exclusive', 'qpc_frequency'):
        need(data[key] == phase[key], 'exact shared timing/phase window')
    hand_cut = closed['original_analysis']['handoff_inventory']['freeze_qpc']
    cut = data['freeze_qpc']; flagged = data['clock_failed'] and data['observer_failed']
    need((0 < cut <= MAX_QPC and cut >= max(hand_cut, data['end_qpc_exclusive'])) or flagged,
         'separate timing cut after handoff/end or explicit clock failure')
    scopes = {row['call_id']: row for row in phase['scopes']}
    for row in data['records']:
        if row['exit_qpc'] > cut or row['entry_qpc'] > cut:
            need(flagged, 'record beyond cut requires explicit clock failure')
        binding = row['binding']
        if binding is None: continue
        need(binding['phase_call_id'] in scopes, 'unknown positive timing phase ID')
        p = scopes[binding['phase_call_id']]
        need(p['kind'] == 'update', 'timing binding must reference actual Update inventory')
        need(binding['thread_epoch'] == p['thread_epoch'] and
             binding['native_thread_id'] == p['native_thread_id'], 'exact phase epoch/native thread binding')
        if 0 < row['entry_qpc'] <= MAX_QPC:
            need(p['entry_qpc'] <= row['entry_qpc'], 'timing entry contained in explicit phase')
            if p['completed'] and not p['invalid_bracket']:
                need(row['entry_qpc'] <= p['exit_qpc'] or flagged,
                     'timing entry cannot follow completed phase exit without clock fault')
        if row['completed'] and row['entry_qpc'] <= row['exit_qpc'] <= MAX_QPC and row['exit_qpc'] > 0:
            if p['completed'] and not p['invalid_bracket']:
                need(row['exit_qpc'] <= p['exit_qpc'], 'timing exit contained in explicit phase')
    return scopes


def _timing_join(data, closed, phase_scopes, global_issues, contexts):
    """Candidate-only grouping after EVERY metadata/cross-reference validates."""
    by_phase, unbound, unknown, pending = {}, [], [], []
    for row in data['records']:
        oid = row['observation_id']
        if row['binding'] is None: unbound.append(oid)
        else: by_phase.setdefault(row['binding']['phase_call_id'], []).append(row)
        if not row['expected_route']: unknown.append(oid)
        if not row['completed']: pending.append(oid)
    selected = {row['update_phase_call_id'] for row in closed['candidate_records']}
    inventory, healthy = [], {}
    for pid in sorted(set(by_phase) | selected):
        rows = by_phase.get(pid, []); p = phase_scopes[pid]; issues = set()
        if not rows: issues.add('NO_BOUND_TIMING_ROWS_FOR_SELECTED_UPDATE')
        if p['caller_lr'] != UPDATE_CALLER: issues.add('UNKNOWN_UPDATE_CALLER_ROUTE')
        if not p['completed'] or p['original_exception'] or p['invalid_bracket']:
            issues.add('UPDATE_PHASE_NOT_POSITIVELY_COMPLETE_NORMAL')
        if not p['context_nonnull'] or not p['base_nonnull']: issues.add('UPDATE_PHASE_LANE_UNAVAILABLE')
        if any(not row['expected_route'] for row in rows): issues.add('BOUND_UNKNOWN_ROUTE_ROWS')
        if any(not row['completed'] for row in rows): issues.add('BOUND_PENDING_ROWS_NO_LATER_COMPLETION')
        if any(row['completed'] and (not row['normal_return'] or not row['exit_binding_matches'] or
               row['before'] is None or row['after'] is None) for row in rows):
            issues.add('BOUND_ROWS_NOT_POSITIVELY_COMPLETE_AVAILABLE')
        if pid not in selected: issues.add('UPDATE_NOT_SELECTED_BY_CLOSED_SOURCE_CHAIN')
        value = {'phase_call_id': pid, 'phase_thread_epoch': p['thread_epoch'],
                 'native_thread_id': p['native_thread_id'],
                 'observation_ids': [row['observation_id'] for row in rows],
                 'diagnostic_issues': sorted(issues), 'selected_by_closed_chain': pid in selected}
        inventory.append(value)
        if pid in selected and not issues and not global_issues: healthy[pid] = value
        contexts.update('update:'+str(pid)+':'+issue for issue in issues)
    candidates = []
    for candidate in closed['candidate_records']:
        group = healthy.get(candidate['update_phase_call_id'])
        if group is not None:
            value = copy.deepcopy(candidate); value['timing_observation_ids'] = list(group['observation_ids'])
            value['timing_phase_thread_epoch'] = group['phase_thread_epoch']
            value['timing_native_thread_id'] = group['native_thread_id']; candidates.append(value)
    if not candidates: contexts.add('NO_SELECTED_COMPLETE_TIMING_GROUP')
    return {'update_inventory': inventory, 'grouped_update_records': list(healthy.values()),
            'candidate_records': candidates, 'unbound_observation_ids': unbound,
            'unknown_route_observation_ids': unknown, 'pending_observation_ids': pending}


def validate_update_timing(timing_raw, handoff_raw, prefix_raw, phase_raw, swap_raw, source_receipt, token):
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token) and token != '0'*32, 'expected nonzero session token')
    source = copy.deepcopy(source_receipt)
    data = handoff._parse(timing_raw, MAX_BYTES)
    global_issues, contexts = _metadata(data, token)
    closed = closed_prefix.validate_closed_handoffs(handoff_raw, prefix_raw, phase_raw, swap_raw, source, token)
    phase_scopes = _cross_references(data, closed)
    global_issues.update('closed:'+issue for issue in closed['candidate_blocking_issues'])
    contexts.update('closed:'+issue for issue in closed['retained_context_issues'])
    grouped = _timing_join(data, closed, phase_scopes, global_issues, contexts)
    result = dict.fromkeys(FALSE_FLAGS | {'update_causality_proven', 'causal_join_proven',
                                       'display_proven', 'source_timing_join_proven'}, False)
    result.update(schema='mcla-update-timing-analysis-v1', token=token,
        status='UPDATE_TIMING_DIAGNOSTIC_INCONCLUSIVE' if global_issues or contexts else
               'UPDATE_TIMING_DIAGNOSTIC_REVIEW_PENDING',
        timing_raw_sha256=hashlib.sha256(timing_raw).hexdigest(), timing_inventory=copy.deepcopy(data),
        closed_handoff_analysis=copy.deepcopy(closed), candidate_blocking_issues=sorted(global_issues),
        retained_context_issues=sorted(contexts), physical_acceptance_pending=True,
        interpretation='OPAQUE_OBSERVATION_IDS_IN_EXACT_UPDATE_SCOPE_NOT_ACTUAL_CONSUMER_OR_SIMULATION_PROOF')
    result.update(copy.deepcopy(grouped))
    return result
