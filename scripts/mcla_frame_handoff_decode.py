"""Bounded, offline explicit-ID handoff inventory; never native/causal proof.

The producer asserts coverage. This reader cannot establish that SDK admissions
or guest wrapper coverage were complete. Host thread epochs and phase-census
thread epochs are DIFFERENT namespaces. QPC/TID values only reject an explicit
ID edge; they never choose one. Pending/untracked/fault inventories are retained.
No IO, pixels, native APIs, time sampling, FIFO or latest-generation joins.
"""
import copy
import hashlib
import json
import re

import mcla_semaphore_credit_audit as v1
import mcla_semaphore_prefix_audit as prefix
import mcla_phase_source_join as phase_join
from mcla_history_limits import (HANDOFF_MAX_BYTES, HANDOFF_SCOPE_CAPACITY,
                                 HANDOFF_EVENT_CAPACITY, semaphore_capacity)

MAX_BYTES = HANDOFF_MAX_BYTES
FALSE_FLAGS = {'native_admitted', 'simulation_proven', 'update_causality_proven',
               'frame_to_step_mapping_proven', 'fps_proven'}
TOP_KEYS = FALSE_FLAGS | {'schema', 'token', 'qpc_frequency', 'observer_begin_qpc',
    'freeze_qpc', 'selected_cookie', 'selected_epoch', 'selected_owner',
    'selected_handle', 'observer_failed', 'coverage_claim_requested', 'lost_scopes',
    'lost_events', 'scopes', 'worker_events', 'operation_annotations', 'collector_counters'}
SCOPE_KEYS = {'phase_call_id', 'entry_qpc', 'exit_qpc', 'caller_lr', 'native_thread_id',
    'thread_epoch', 'serial', 'update_thread_epoch', 'update_serial', 'wait_operation',
    'object_epoch', 'kind', 'tracked', 'completed', 'normal_return'}
EVENT_KEYS = {'qpc', 'thread_epoch', 'worker_serial', 'native_thread_id', 'context',
    'base', 'owner', 'object_epoch', 'event', 'reason', 'helper', 'operation',
    'dispatch', 'phase_call_id', 'retired_operation', 'retirement_reason',
    'eligible', 'phase_bound'}
ANNOTATION_KEYS = {'operation_id', 'phase_call_id', 'caller_lr', 'native_thread_id', 'annotated'}
COUNTER_KEYS = {'failures', 'ignored_creations', 'ignored_operations',
                'annotation_mismatches', 'late_callbacks'}
need, exact, uint, boolean = v1.need, v1.exact, v1.uint, v1.boolean


def _parse(raw, bound):
    need(type(raw) is bytes and 0 < len(raw) <= bound, 'bounded immutable JSON bytes')
    try:
        return json.loads(raw.decode('utf-8'), object_pairs_hook=v1._pairs,
                          parse_int=v1._integer, parse_float=v1._no_float,
                          parse_constant=v1._no_float)
    except (UnicodeError, RecursionError) as error:
        raise ValueError('invalid or excessively nested JSON') from error


def _prefix_metadata(value, token, begin, cut, frequency):
    """Metadata-only prepass, using unchanged validators' schema primitives.

    The unchanged prefix.audit subsequently repeats these checks and performs
    its own sufficient-model evaluation. No candidate evaluator runs here.
    """
    exact(value, prefix.TOP_KEYS, 'exact prefix journal schema')
    need(value['schema'] == 'mcla-semaphore-prefix-journal-v2' and
         value['token'] == token and value['clock'] == 'win32-query-performance-counter',
         'prefix identity')
    for key, expected in (('observer_begin_qpc', begin), ('freeze_qpc', cut),
                          ('qpc_frequency', frequency)):
        uint(value[key]); need(value[key] == expected, 'prefix clock/capacity binding')
    capacity = semaphore_capacity(value['capacity'])
    for key in prefix.FALSE_FLAGS:
        need(value[key] is False, 'prefix proof flags false')
    for key in ('admission_inventory_complete', 'observer_failed', 'unknown_coverage_gap'):
        boolean(value[key])
    need(type(value['epochs']) is list and 0 < len(value['epochs']) <= v1.MAX_EPOCHS,
         'bounded nonempty prefix epochs')
    need(type(value['operations']) is list and len(value['operations']) <= capacity,
         'bounded prefix operations')
    need(type(value['loss_ranges']) is list and len(value['loss_ranges']) <= prefix.MAX_LOSS_RANGES,
         'bounded prefix losses')
    epochs, operations, intervals = {}, {}, []
    for row in value['epochs']:
        v1._epoch(row); eid = row['epoch_id']
        need(eid not in epochs, 'duplicate prefix epoch')
        epochs[eid] = row
        need(row['creation_qpc'] < cut, 'prefix creation before cut')
        if row['destroy_qpc'] is not None:
            need(row['creation_qpc'] < row['destroy_qpc'] <= cut, 'prefix destroy bracket')
    pending = 0
    for row in value['operations']:
        prefix._operation(row, epochs)
        oid = row['op_id']; need(oid not in operations, 'duplicate prefix operation')
        operations[oid] = row; epoch = epochs[row['epoch_id']]
        entry, exit_ = row['entry_qpc'], row['exit_qpc']
        intervals.append((oid, oid, entry))
        if entry is not None:
            need(begin <= entry <= cut and entry > epoch['creation_qpc'], 'prefix admission bracket')
            if epoch['destroy_qpc'] is not None:
                need(entry < epoch['destroy_qpc'], 'prefix admission before destruction')
        if exit_ is not None:
            need(exit_ <= cut, 'prefix completion before cut')
            if epoch['destroy_qpc'] is not None:
                need(exit_ < epoch['destroy_qpc'], 'prefix completion before destruction')
        if not row['completed']:
            pending += 1
            need(epoch['destroy_qpc'] is None, 'pending prefix object remains live')
    lost = 0
    for row in value['loss_ranges']:
        exact(row, prefix.LOSS_KEYS, 'exact prefix loss')
        first, last = uint(row['first_op_id'], low=1), uint(row['last_op_id'], low=1)
        need(first <= last and type(row['kind']) is str and row['kind'] in
             {'overflow', 'coverage_gap', 'unsupported_access'}, 'prefix loss range/kind')
        bound = row['entry_lower_bound_qpc']; prefix._clock(bound)
        if bound is not None:
            need(begin <= bound <= cut, 'prefix loss clock bracket')
        intervals.append((first, last, bound)); lost += last-first+1; uint(lost)
    counts = value['counts']; exact(counts, prefix.COUNT_KEYS, 'exact prefix counts')
    for count in counts.values(): uint(count)
    need(counts == {'observed_operations': len(operations)+lost,
                    'stored_operations': len(operations), 'completed_operations': len(operations)-pending,
                    'pending_operations': pending, 'lost_operations': lost}, 'prefix inventory accounting')
    next_id, previous_clock = 1, None
    for first, last, clock in sorted(intervals):
        need(first == next_id, 'prefix IDs/losses partition inventory'); next_id = last+1
        if clock is not None:
            need(previous_clock is None or clock >= previous_clock, 'prefix admission ID clock regression')
            previous_clock = clock
    need(next_id-1 == counts['observed_operations'], 'prefix final ID')
    return epochs, operations


def _validate_handoff(hand, phase, epochs, operations, token, capacity):
    capacity = semaphore_capacity(capacity)
    exact(hand, TOP_KEYS, 'exact handoff schema')
    need(hand['schema'] == 'mcla-frame-handoff-v1' and hand['token'] == token, 'handoff identity')
    for key in FALSE_FLAGS: need(hand[key] is False, 'handoff proof flags false')
    for key in ('observer_failed', 'coverage_claim_requested'): boolean(hand[key])
    for key in ('qpc_frequency', 'observer_begin_qpc', 'freeze_qpc'):
        uint(hand[key], v1.MAX_QPC, 1)
    begin, cut = hand['observer_begin_qpc'], hand['freeze_qpc']
    need(begin < cut and hand['qpc_frequency'] == phase['qpc_frequency'], 'handoff clock binding')
    # Configured phase horizon is NOT an actual freeze timestamp. The current
    # native producer waits for that horizon before Freeze; this generic reader
    # binds each retained handoff row to the actual cut rather than inventing a
    # header-to-cut equality. Phase Finish still follows collector Freeze.
    need(begin <= phase['begin_qpc'] < cut, 'phase start within observed handoff cut')
    for key in ('selected_cookie', 'selected_epoch'): uint(hand[key], low=1)
    for key in ('selected_owner', 'selected_handle'): uint(hand[key], v1.U32, 1)
    uint(hand['lost_scopes']); uint(hand['lost_events'])
    eid = hand['selected_epoch']; need(eid in epochs, 'selected epoch exists')
    selected = epochs[eid]
    need(selected['object_cookie'] == hand['selected_cookie'], 'selected cookie binding')
    need(len(epochs) == 1, 'single selected object inventory')
    need(selected['maximum_count'] == 32767, 'selected semaphore maximum')
    exact(hand['collector_counters'], COUNTER_KEYS, 'exact collector counters')
    for value in hand['collector_counters'].values(): uint(value)
    issues = set()
    if not hand['coverage_claim_requested']: issues.add('COVERAGE_NOT_REQUESTED_OR_ESTABLISHED')
    if hand['observer_failed']: issues.add('HANDOFF_OBSERVER_FAILED')
    if hand['lost_scopes'] or hand['lost_events']: issues.add('HANDOFF_INVENTORY_LOSS')
    if hand['collector_counters']['failures']: issues.add('COLLECTOR_FAILURES')
    if hand['collector_counters']['late_callbacks']: issues.add('LATE_CALLBACKS_AT_CUT')
    if hand['collector_counters']['annotation_mismatches']: issues.add('ANNOTATION_MISMATCHES')
    if selected['initial_count'] or selected['origin'] != 'fresh': issues.add('NONFRESH_OR_INITIAL_CREDITS')
    for name, limit in (('scopes', HANDOFF_SCOPE_CAPACITY),
                        ('worker_events', HANDOFF_EVENT_CAPACITY),
                        ('operation_annotations', capacity)):
        need(type(hand[name]) is list and len(hand[name]) <= limit, 'bounded '+name)
    phase_ids = {row['call_id']: row for row in phase['scopes']}
    scopes, invocation_ids, host_tids = {}, set(), {}
    for row in hand['scopes']:
        exact(row, SCOPE_KEYS, 'exact handoff scope')
        for key in SCOPE_KEYS - {'tracked', 'completed', 'normal_return'}:
            uint(row[key], v1.MAX_QPC if key in {'entry_qpc', 'exit_qpc'} else
                 v1.U32 if key == 'native_thread_id' else v1.U64)
        for key in ('tracked', 'completed', 'normal_return'): boolean(row[key])
        need(row['kind'] <= 3 and row['native_thread_id'] > 0, 'handoff scope kind/thread')
        pid = row['phase_call_id']; need(pid > 0 and pid in phase_ids and pid not in scopes,
                                         'unique existing handoff phase call ID')
        scopes[pid] = row; p = phase_ids[pid]
        need(row['native_thread_id'] == p['native_thread_id'] and row['caller_lr'] == p['caller_lr'],
             'handoff explicit phase identity mismatch')
        if row['tracked']:
            need(p['kind'] == ('update', 'coordinator', 'render', 'render')[row['kind']],
                 'handoff phase kind mismatch')
        if not row['completed']:
            need(row['exit_qpc'] == 0 and not row['normal_return'], 'pending handoff scope')
            issues.add('PENDING_HANDOFF_SCOPE')
        elif not row['normal_return']: issues.add('ABNORMAL_HANDOFF_RETURN')
        valid_clock = row['entry_qpc'] > 0 and (not row['completed'] or row['exit_qpc'] >= row['entry_qpc'])
        if not valid_clock or (row['completed'] and row['exit_qpc'] == 0):
            issues.add('INVALID_HANDOFF_SCOPE_CLOCK')
        else:
            need(begin <= row['entry_qpc'] <= cut, 'handoff entry bracket')
            need(p['entry_qpc'] <= row['entry_qpc'], 'handoff entry within explicit phase')
            if row['completed']:
                need(row['exit_qpc'] <= cut, 'handoff completion at cut')
                if p['completed'] and not p['invalid_bracket']:
                    need(row['exit_qpc'] <= p['exit_qpc'], 'handoff exit within explicit phase')
        if not row['tracked']:
            # Failed enter can retain the tracker's previous context. It is
            # opaque diagnostic content, NEVER a usable inherited identity.
            issues.add('UNTRACKED_HANDOFF_SCOPE'); continue
        identity = (row['thread_epoch'], row['serial'])
        need(all(identity) and identity not in invocation_ids, 'unique positive host invocation ID')
        invocation_ids.add(identity)
        te = identity[0]
        need(te not in host_tids or host_tids[te] == row['native_thread_id'], 'host epoch thread consistency')
        host_tids[te] = row['native_thread_id']
        update = (row['update_thread_epoch'], row['update_serial'])
        need(bool(update[0]) == bool(update[1]), 'complete host update ID pair')
        if row['kind'] == 0:
            need(update == (0, 0) and row['wait_operation'] == row['object_epoch'] == 0,
                 'update scope has no inherited credit')
        elif row['kind'] in (1, 2):
            need(all(update) and row['wait_operation'] == 0 and row['object_epoch'] == eid,
                 'coordinator/synchronous render context')
        else:
            need(update == (0, 0) and row['wait_operation'] > 0 and row['object_epoch'] == eid,
                 'worker render explicit wait context')
            need(row['wait_operation'] in operations and
                 operations[row['wait_operation']]['kind'] == 'wait', 'worker scope wait ID exists')
    annotations = {}
    for row in hand['operation_annotations']:
        exact(row, ANNOTATION_KEYS, 'exact operation annotation')
        for key in ANNOTATION_KEYS - {'annotated'}:
            uint(row[key], v1.U32 if key == 'native_thread_id' else v1.U64)
        boolean(row['annotated']); oid = row['operation_id']
        need(oid in operations and oid not in annotations, 'unique existing annotated operation')
        annotations[oid] = row; op = operations[oid]
        need(row['native_thread_id'] == op['native_thread_id'], 'annotation operation thread binding')
        need(op['epoch_id'] == eid, 'operation selected epoch')
        # Different guest handles may alias this same witnessed object epoch.
        # The selected creation handle is a producer claim, not an operation-ID
        # selector or evidence that every access reused that numeric handle.
        if not row['annotated']:
            need(row['phase_call_id'] == row['caller_lr'] == 0, 'unannotated operation has no context')
            issues.add('UNANNOTATED_OPERATION')
        if row['phase_call_id']:
            need(row['annotated'] and row['phase_call_id'] in phase_ids, 'annotation phase ID exists')
            p = phase_ids[row['phase_call_id']]
            need(p['native_thread_id'] == row['native_thread_id'], 'annotation phase thread binding')
            if op['entry_qpc'] is not None:
                need(p['entry_qpc'] <= op['entry_qpc'], 'operation admission in explicit phase')
            if op['completed'] and op['exit_qpc'] is not None and p['completed'] and not p['invalid_bracket']:
                need(op['exit_qpc'] <= p['exit_qpc'], 'operation completion in explicit phase')
    need(set(annotations) == set(operations), 'complete retained operation annotation inventory')
    keys, worker_clocks, consumed_operations = {}, {}, set()
    for row in hand['worker_events']:
        exact(row, EVENT_KEYS, 'exact worker event')
        for key in EVENT_KEYS - {'eligible', 'phase_bound'}:
            uint(row[key], v1.MAX_QPC if key == 'qpc' else
                 v1.U32 if key in {'native_thread_id', 'owner'} else v1.U64)
        boolean(row['eligible']); boolean(row['phase_bound'])
        need(row['event'] <= 10 and row['reason'] <= 20 and row['retirement_reason'] <= 20 and
             row['native_thread_id'] > 0, 'worker enum/thread bounds')
        if row['qpc'] == 0: issues.add('INVALID_WORKER_CLOCK')
        else: need(begin <= row['qpc'] <= cut, 'worker event at observed cut')
        if row['event'] == 10: issues.add('WORKER_OBSERVER_FAULT')
        if row['phase_call_id']:
            need(row['event'] == 6 and row['phase_call_id'] in phase_ids, 'worker render phase ID exists')
            p = phase_ids[row['phase_call_id']]
            need(p['kind'] == 'render' and p['native_thread_id'] == row['native_thread_id'],
                 'worker phase identity mismatch')
        need(row['phase_bound'] == (row['event'] == 6 and row['eligible'] and row['phase_call_id'] > 0),
             'explicit worker phase-bound flag')
        if row['phase_bound']:
            need(row['operation'] > 0 and (row['phase_call_id'] in scopes or hand['lost_scopes'] > 0),
                 'worker bound scope inventory')
        for key in ('operation', 'retired_operation'):
            if row[key]:
                need(row[key] in operations and operations[row[key]]['kind'] == 'wait',
                     'worker explicit operation ID exists as selected wait')
        if row['event'] == 6 and row['eligible']:
            need(row['operation'] > 0, 'eligible render has explicit wait ID')
            if row['operation'] in consumed_operations:
                issues.add('DUPLICATE_WORKER_WAIT_CONSUMPTION')
            consumed_operations.add(row['operation'])
        if row['thread_epoch'] and row['worker_serial']:
            key = (row['thread_epoch'], row['worker_serial'])
            identity = tuple(row[k] for k in ('native_thread_id', 'context', 'base', 'owner', 'object_epoch'))
            need(key not in keys or keys[key] == identity, 'worker lifetime key consistency')
            keys[key] = identity
            if row['qpc']:
                if key in worker_clocks and row['qpc'] < worker_clocks[key]:
                    issues.add('WORKER_CLOCK_REGRESSION')
                worker_clocks[key] = row['qpc']
        else:
            issues.add('UNTRACKED_WORKER_EVENT')
    return scopes, annotations, issues


def _evaluate(hand, prefix_result, joined, scopes, annotations, operations, issues):
    """Only explicit ID candidates, after ALL inventories have validated.

    This is a sufficient diagnostic chain, not a replay/coverage verifier for
    the native worker state machine. No source is reassigned on missing edges.
    """
    candidates, dispositions = [], []
    by_update = {(r['thread_epoch'], r['serial']): r for r in scopes.values()
                 if r['tracked'] and r['kind'] == 0}
    by_wait = {}
    for association in prefix_result['associations']:
        by_wait.setdefault(association['wait_op_id'], []).append(association)
    render_events, eligible, dispatches, wait_entries = {}, {}, {}, {}
    for event in hand['worker_events']:
        key = (event['thread_epoch'], event['worker_serial'])
        kind = event['event']
        if kind in (2, 4, 7, 8, 9, 10):
            eligible.pop(key, None); dispatches.pop(key, None)
        if kind == 2: wait_entries[key] = event
        if kind in (7, 8, 9, 10): wait_entries.pop(key, None)
        if kind == 3 and event['eligible']:
            eligible[key] = event
        if kind == 5:
            dispatches[key] = event
        if kind == 6:
            wait, dispatch = eligible.pop(key, None), dispatches.get(key)
            if event['phase_bound']:
                render_events.setdefault(event['phase_call_id'], []).append(
                    (event, wait, dispatch, wait_entries.get(key)))
    for source in joined['records']:
        disposition = 'UNBOUND_SOURCE_OR_PHASE'
        p = source['scope']; h = None if p is None else scopes.get(p['call_id'])
        candidate = None
        if h is not None and h['tracked'] and h['kind'] == 3 and h['completed'] and h['normal_return']:
            disposition = 'MISSING_OR_AMBIGUOUS_EXPLICIT_WORKER_EDGE'
            edges = render_events.get(h['phase_call_id'], [])
            associations = by_wait.get(h['wait_operation'], [])
            if len(edges) == len(associations) == 1:
                event, wait_event, dispatch, wait_entry = edges[0]; association = associations[0]
                wait_op = operations[h['wait_operation']]
                release = operations[association['release_op_id']]
                a = annotations[release['op_id']]; wa = annotations[wait_op['op_id']]
                coord = scopes.get(a['phase_call_id']); generation = association['context']['generation']
                update_id = (generation['thread_epoch'], generation['serial'])
                update = by_update.get(update_id)
                same = lambda e: e is not None and e['native_thread_id'] == h['native_thread_id'] and e['thread_epoch'] == h['thread_epoch']
                valid = (same(event) and same(wait_event) and same(dispatch) and same(wait_entry) and
                    event['eligible'] and wait_event['eligible'] and dispatch['eligible'] and
                    event['operation'] == wait_event['operation'] == dispatch['operation'] == h['wait_operation'] and
                    event['helper'] == wait_event['helper'] == dispatch['helper'] == wait_entry['helper'] > 0 and
                    wait_entry['operation'] == 0 and not wait_entry['eligible'] and
                    event['dispatch'] == dispatch['dispatch'] > 0 and
                    event['object_epoch'] == h['object_epoch'] == hand['selected_epoch'] and
                    event['owner'] == hand['selected_owner'] and
                    h['caller_lr'] == 0x82305F80 and p['incoming_r3'] == 0 and
                    source['callback'] is not None and
                    h['entry_qpc'] <= source['callback']['qpc'] <= h['exit_qpc'] and
                    event['qpc'] == h['entry_qpc'] and wa['annotated'] and wa['caller_lr'] == 0x82305EB8 and
                    wait_op['completed'] and wait_op['payload']['result'] == 'success' and
                    wait_op['native_thread_id'] == h['native_thread_id'] and
                    wait_op['entry_qpc'] is not None and wait_entry['qpc'] <= wait_op['entry_qpc'] and
                    wait_op['exit_qpc'] is not None and
                    wait_op['exit_qpc'] <= wait_event['qpc'] <= dispatch['qpc'] <= event['qpc'] and
                    a['annotated'] and a['caller_lr'] == 0x82305DDC and coord is not None and
                    coord['tracked'] and coord['kind'] == 1 and coord['object_epoch'] == hand['selected_epoch'] and
                    (coord['update_thread_epoch'], coord['update_serial']) == update_id and
                    coord['completed'] and coord['normal_return'] and
                    release['entry_qpc'] is not None and release['exit_qpc'] is not None and
                    coord['entry_qpc'] <= release['entry_qpc'] <= release['exit_qpc'] <= coord['exit_qpc'] and
                    update is not None and
                    update['completed'] and update['normal_return'] and
                    update['caller_lr'] == 0x821316A0 and coord['caller_lr'] == 0x821316B8 and
                    update['thread_epoch'] == coord['thread_epoch'] and
                    update['native_thread_id'] == coord['native_thread_id'] == release['native_thread_id'] and
                    update['exit_qpc'] <= coord['entry_qpc'])
                if valid:
                    disposition = 'EXPLICIT_ID_HANDOFF_CANDIDATE_REVIEW_PENDING'
                    candidate = {'slot': source['slot'], 'presenter_epoch': source['presenter_epoch'],
                        'acquired_sequence': source['acquired_sequence'], 'swap_token': source['swap_token'],
                        'callback_id': source['callback_id'], 'render_phase_call_id': h['phase_call_id'],
                        'wait_operation_id': wait_op['op_id'], 'release_operation_id': release['op_id'],
                        'coordinator_phase_call_id': coord['phase_call_id'],
                        'update_phase_call_id': update['phase_call_id'], 'host_update_id': dict(generation)}
                else:
                    disposition = 'INCOMPLETE_EXPLICIT_UPDATE_COORDINATOR_WORKER_CHAIN'
        if issues and candidate is not None:
            disposition = 'GLOBAL_HEALTH_OR_COVERAGE_INCONCLUSIVE'; candidate = None
        if candidate is not None: candidates.append(candidate)
        dispositions.append({'slot': source['slot'], 'disposition': disposition})
    return candidates, dispositions


def validate_frame_handoff(handoff_raw, prefix_raw, phase_raw, swap_raw, source_receipt, token):
    """Validate immutable journals plus detached source metadata; return JSON data.

    Coverage requests are producer assertions, NOT verified coverage. A missing
    update phase entry (including startup outside the phase window) stays
    inconclusive. This version deliberately suppresses combined candidates on
    any global health issue, even if an earlier PREFIX-v2 prefix was usable.
    """
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token), 'expected session token')
    source = copy.deepcopy(source_receipt)
    hand = _parse(handoff_raw, MAX_BYTES)
    pre = _parse(prefix_raw, prefix.MAX_BYTES)
    phase = _parse(phase_raw, 2*1024*1024)
    _parse(swap_raw, 128*1024)  # Reject lexical corruption before any evaluator.
    # Safe scalar header access only after exact handoff shape/type checks.
    exact(hand, TOP_KEYS, 'exact handoff schema')
    for key in ('observer_begin_qpc', 'freeze_qpc', 'qpc_frequency'):
        uint(hand[key], v1.MAX_QPC, 1)
    epochs, operations = _prefix_metadata(pre, token, hand['observer_begin_qpc'],
                                         hand['freeze_qpc'], hand['qpc_frequency'])
    # The existing join validates ALL source/phase/swap metadata before its own
    # ID linking. Handoff scalar/row metadata must validate before prefix.audit.
    # Phase schema is validated by it; guard the fields used in our prepass.
    need(type(phase) is dict and type(phase.get('scopes')) is list and len(phase['scopes']) <= 2048,
         'bounded phase scope inventory')
    required = {'call_id', 'kind', 'native_thread_id', 'caller_lr', 'entry_qpc',
                'exit_qpc', 'completed', 'invalid_bracket'}
    for row in phase['scopes']:
        need(type(row) is dict and required <= set(row), 'phase cross-reference fields')
        for key in ('call_id', 'native_thread_id', 'caller_lr', 'entry_qpc', 'exit_qpc'):
            uint(row[key])
        boolean(row['completed']); boolean(row['invalid_bracket'])
    for key in ('qpc_frequency', 'begin_qpc', 'end_qpc_exclusive'): uint(phase.get(key), v1.MAX_QPC, 1)
    scopes, annotations, issues = _validate_handoff(hand, phase, epochs, operations,
                                                 token, pre['capacity'])
    joined = phase_join.validate_phase_source_join(phase_raw, swap_raw, source, token)
    audited = prefix.audit(prefix_raw, token=token, observer_begin_qpc=hand['observer_begin_qpc'],
                           freeze_qpc=hand['freeze_qpc'], qpc_frequency=hand['qpc_frequency'])
    issues.update('phase_source:'+x for x in joined['diagnostic_issues'])
    issues.update('prefix:'+x for x in audited['diagnostic_issues'])
    if pre['counts']['pending_operations']: issues.add('PENDING_SEMAPHORE_OPERATIONS')
    if pre['counts']['lost_operations']: issues.add('SEMAPHORE_ADMISSION_LOSS')
    candidates, dispositions = _evaluate(hand, audited, joined, scopes, annotations, operations, issues)
    if not candidates: issues.add('NO_COMPLETE_EXPLICIT_ID_HANDOFF_CANDIDATE')
    result = dict.fromkeys(FALSE_FLAGS | {'render_proven', 'display_proven', 'causal_join_proven',
                          'object_identity_proven', 'object_lifetime_proven', 'coverage_proven'}, False)
    result.update(schema='mcla-frame-handoff-analysis-v1', token=token,
        status='FRAME_HANDOFF_DIAGNOSTIC_INCONCLUSIVE' if issues else 'FRAME_HANDOFF_CANDIDATES_REVIEW_PENDING',
        diagnostic_issues=sorted(issues), physical_acceptance_pending=True,
        unobserved_ancestry_possible=True, prefix_audit=audited, prefix_inventory=copy.deepcopy(pre),
        source_phase_analysis=joined,
        handoff_inventory=copy.deepcopy(hand), candidate_records=copy.deepcopy(candidates),
        source_dispositions=dispositions,
        handoff_raw_sha256=hashlib.sha256(handoff_raw).hexdigest(),
        interpretation='EXPLICIT_ID_DIAGNOSTIC_CHAIN_NOT_VERIFIED_UPDATE_OR_SIMULATION_LINEAGE')
    return result
