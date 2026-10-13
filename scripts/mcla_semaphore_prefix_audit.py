"""Pure PREFIX-v2 sufficient audit; not native completeness or causality proof.

The supplied journal claims admission under one observer lock BEFORE each native
operation, with monotonically assigned IDs and admission-clock lower bounds.
Freeze is under that same lock while the title is alive. Epoch history_complete
means lifetime admission coverage through freeze, NOT drained or supported ops.
These native claims cannot be established by this offline reader.

IDs plus explicit loss intervals must partition the entire admission inventory.
There is no caller-selected prefix cutoff. A global barrier and closed component
boundaries derive it. Pending operations are retained, never completed on paper.
Only complete, zero-balance prefixes are projected into unchanged audit-v1. Such
projections are derived evidence, not original native journals or new timestamps.
"""
import copy
import hashlib
import json
import re

import mcla_semaphore_credit_audit as v1

MAX_BYTES = 2 * 1024 * 1024
CAPACITY = v1.CAPACITY
MAX_LOSS_RANGES = 4096
FALSE_FLAGS = v1.FALSE_FLAGS | {'prefix_history_proven', 'update_causality_proven'}
TOP_KEYS = FALSE_FLAGS | {
    'schema', 'token', 'clock', 'qpc_frequency', 'observer_begin_qpc',
    'freeze_qpc', 'capacity', 'admission_inventory_complete', 'observer_failed',
    'unknown_coverage_gap', 'counts', 'epochs', 'operations', 'loss_ranges'}
COUNT_KEYS = {'observed_operations', 'stored_operations', 'completed_operations',
              'pending_operations', 'lost_operations'}
LOSS_KEYS = {'first_op_id', 'last_op_id', 'entry_lower_bound_qpc', 'kind'}
OP_KEYS = v1.OP_KEYS | {'completed'}

need, exact, uint, boolean = v1.need, v1.exact, v1.uint, v1.boolean


def _clock(value):
    if value is not None:
        uint(value, v1.MAX_QPC, 1)


def _operation(row, epochs):
    """Full schema validation, including suffixes; no completion inference."""
    exact(row, OP_KEYS, 'exact prefix operation schema')
    boolean(row['completed'])
    _clock(row['entry_qpc'])
    _clock(row['exit_qpc'])
    payload = row['payload']
    # Reuse v1's payload/identity TYPE checks on a detached scratch object only.
    # Placeholders are never sent to audit(), returned as evidence, or admitted.
    scratch = copy.deepcopy(row)
    del scratch['completed']
    scratch['entry_qpc'] = row['entry_qpc'] or 1
    scratch['exit_qpc'] = row['exit_qpc'] or 1
    if not row['completed']:
        need(row['exit_qpc'] is None, 'pending operation has no exit timestamp')
        if row['kind'] == 'release':
            exact(payload, {'count', 'success', 'previous_count', 'context'},
                  'exact pending release schema')
            need(payload['success'] is None and payload['previous_count'] is None,
                 'pending release has no outcome or previous count')
            scratch['payload']['success'] = False
        elif row['kind'] == 'wait':
            exact(payload, {'result', 'timeout_kind'}, 'exact pending wait schema')
            need(payload['result'] is None, 'pending wait has no result')
            scratch['payload']['result'] = 'timeout'
    v1._operation(scratch, epochs)
    if row['completed'] and row['entry_qpc'] is not None and row['exit_qpc'] is not None:
        need(row['entry_qpc'] <= row['exit_qpc'], 'operation clock regression')
    if row['kind'] == 'release' and row['completed'] and payload['success']:
        need(payload['previous_count'] + payload['count'] <=
             epochs[row['epoch_id']]['maximum_count'], 'impossible successful release count')


def _unsupported(row):
    if row['kind'] not in {'release', 'wait'}:
        return True
    return (row['kind'] == 'wait' and row['completed'] and
            row['payload']['result'] not in {'success', 'timeout', 'apc', 'alerted'})


def _components(rows):
    """Inclusive transitive overlap/touch components; pending extends to infinity."""
    groups, end = [], None
    for row in sorted(rows, key=lambda r: (r['entry_qpc'], r['op_id'])):
        if end is None or row['entry_qpc'] > end:
            groups.append([row])
            end = row['exit_qpc'] if row['completed'] else v1.MAX_QPC
        else:
            groups[-1].append(row)
            end = max(end, row['exit_qpc'] if row['completed'] else v1.MAX_QPC)
    return groups


def _step(group, balance):
    """Return (balance, coverage restriction, contradiction), without FIFO."""
    if len(group) != 1:
        release = [r for r in group if r['kind'] == 'release']
        wait = [r for r in group if r['kind'] == 'wait']
        if (len(group) == 2 and len(release) == len(wait) == 1 and balance == 0 and
                release[0]['payload']['success'] is True and
                release[0]['payload']['count'] == 1 and
                release[0]['payload']['previous_count'] == 0 and
                wait[0]['payload']['result'] == 'success'):
            return 0, None, None
        return balance, 'OVERLAP_OUTSIDE_SINGLE_CREDIT_MODEL', None
    row, payload = group[0], group[0]['payload']
    if row['kind'] == 'release':
        if not payload['success']:
            return balance, None, None
        if payload['previous_count'] != balance:
            return balance, None, 'PREVIOUS_COUNT_CONTRADICTS_COMPLETE_PREFIX'
        updated = balance + payload['count']
        if updated > 1:
            return balance, 'BATCH_OR_COMPETING_CREDITS_OUTSIDE_MODEL', None
        return updated, None, None
    if payload['result'] == 'success':
        if balance == 0:
            return balance, None, 'SUCCESSFUL_WAIT_WITHOUT_PREFIX_CREDIT'
        return balance - 1, None, None
    if payload['result'] == 'timeout' and balance:
        return balance, None, 'TIMEOUT_WITH_AVAILABLE_PREFIX_CREDIT'
    return balance, None, None


def _projection(source, epoch, rows, end):
    """Truthful history through an observed exclusive boundary, NOT native data."""
    projected_epoch = copy.deepcopy(epoch)
    if projected_epoch['destroy_qpc'] is not None and projected_epoch['destroy_qpc'] >= end:
        projected_epoch['destroy_qpc'] = None  # Not yet destroyed at this prefix.
    projected = dict.fromkeys(v1.FALSE_FLAGS, False)
    projected.update(schema='mcla-semaphore-credit-journal-v1', token=source['token'],
                     clock=source['clock'], qpc_frequency=source['qpc_frequency'],
                     begin_qpc=source['observer_begin_qpc'], end_qpc_exclusive=end,
                     capacity=CAPACITY, history_complete=True, observer_failed=False,
                     counts={'observed_operations': len(rows), 'stored_operations': len(rows),
                             'completed_operations': len(rows), 'overflow_operations': 0},
                     epochs=[projected_epoch], operations=[])
    for row in rows:
        value = copy.deepcopy(row)
        del value['completed']
        projected['operations'].append(value)
    return json.dumps(projected, sort_keys=True, separators=(',', ':')).encode('ascii')


def audit(data, *, token, observer_begin_qpc, freeze_qpc, qpc_frequency):
    """Validate all metadata, then return candidate-only derived prefix evidence.

    Malformed/boundedness/impossible metadata raises ValueError. Honest unknown
    coverage or clocks yields no candidates. Unsupported later work is retained
    as unmodeled suffix, never represented as globally healthy/drained history.
    """
    need(type(data) is bytes and 0 < len(data) <= MAX_BYTES, 'bounded immutable journal')
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token), 'expected token')
    for value in (observer_begin_qpc, freeze_qpc, qpc_frequency):
        uint(value, v1.MAX_QPC, 1)
    need(observer_begin_qpc < freeze_qpc, 'positive frozen interval')
    try:
        source = json.loads(data.decode('utf-8'), object_pairs_hook=v1._pairs,
                            parse_int=v1._integer, parse_float=v1._no_float,
                            parse_constant=v1._no_float)
    except (UnicodeError, RecursionError) as error:
        raise ValueError('invalid or excessively nested JSON') from error
    exact(source, TOP_KEYS, 'exact prefix journal schema')
    need(source['schema'] == 'mcla-semaphore-prefix-journal-v2' and
         source['token'] == token and source['clock'] == 'win32-query-performance-counter',
         'prefix journal identity')
    for key, value in (('observer_begin_qpc', observer_begin_qpc), ('freeze_qpc', freeze_qpc),
                       ('qpc_frequency', qpc_frequency), ('capacity', CAPACITY)):
        uint(source[key]); need(source[key] == value, 'exact clock/capacity binding')
    for key in FALSE_FLAGS:
        need(source[key] is False, 'proof flags remain false')
    for key in ('admission_inventory_complete', 'observer_failed', 'unknown_coverage_gap'):
        boolean(source[key])
    need(type(source['epochs']) is list and 0 < len(source['epochs']) <= v1.MAX_EPOCHS,
         'bounded nonempty epochs')
    need(type(source['operations']) is list and len(source['operations']) <= CAPACITY,
         'bounded operations')
    need(type(source['loss_ranges']) is list and len(source['loss_ranges']) <= MAX_LOSS_RANGES,
         'bounded loss ranges')
    issues, epochs = set(), {}
    if not source['admission_inventory_complete']:
        issues.add('ADMISSION_INVENTORY_INCOMPLETE')
    if source['observer_failed']:
        issues.add('OBSERVER_FAILED')
    if source['unknown_coverage_gap']:
        issues.add('UNBOUNDED_COVERAGE_GAP')
    for epoch in source['epochs']:
        v1._epoch(epoch)
        eid = epoch['epoch_id']
        need(eid not in epochs, 'duplicate epoch ID')
        epochs[eid] = epoch
        created, destroyed = epoch['creation_qpc'], epoch['destroy_qpc']
        need(created < freeze_qpc, 'creation before freeze')
        if destroyed is not None:
            need(created < destroyed <= freeze_qpc, 'destruction bracket')
        if created <= observer_begin_qpc:
            issues.add('CREATION_NOT_STRICTLY_AFTER_OBSERVER_START')
        if epoch['origin'] != 'fresh' or not epoch['history_complete']:
            issues.add('FRESH_LIFETIME_ADMISSION_COVERAGE_REQUIRED')
        if epoch['initial_count']:
            issues.add('INITIAL_CREDITS_NOT_ATTRIBUTABLE')
    ordered_epochs = sorted(epochs.values(), key=lambda e: (e['object_cookie'], e['creation_qpc']))
    for a, b in zip(ordered_epochs, ordered_epochs[1:]):
        if a['object_cookie'] == b['object_cookie'] and (
                a['destroy_qpc'] is None or a['destroy_qpc'] >= b['creation_qpc']):
            issues.add('COOKIE_REUSE_WITHOUT_SEPARATE_WITNESSED_LIFETIMES')

    intervals, barriers, pending, ids = [], [], 0, set()
    for row in source['operations']:
        _operation(row, epochs)
        need(row['op_id'] not in ids, 'duplicate operation ID')
        ids.add(row['op_id'])
        entry, exit_ = row['entry_qpc'], row['exit_qpc']
        intervals.append((row['op_id'], row['op_id'], entry))
        if entry is None or (row['completed'] and exit_ is None):
            issues.add('UNKNOWN_OPERATION_CLOCK')
        epoch = epochs[row['epoch_id']]
        if entry is not None:
            need(observer_begin_qpc <= entry <= freeze_qpc and entry > epoch['creation_qpc'],
                 'operation admission within observed lifetime/window')
            if epoch['destroy_qpc'] is not None:
                need(entry < epoch['destroy_qpc'], 'admission before destruction')
        if exit_ is not None:
            need(exit_ <= freeze_qpc, 'completed operation cannot finish after freeze')
            if epoch['destroy_qpc'] is not None:
                need(exit_ < epoch['destroy_qpc'], 'completion before destruction')
        if not row['completed']:
            pending += 1
            need(epoch['destroy_qpc'] is None, 'destroyed object cannot have pending operation')
            barriers.append({'reason': 'PENDING_OPERATION', 'op_id': row['op_id'], 'qpc': entry})
        if _unsupported(row):
            barriers.append({'reason': 'UNSUPPORTED_OPERATION', 'op_id': row['op_id'], 'qpc': entry})
    lost = 0
    for loss in source['loss_ranges']:
        exact(loss, LOSS_KEYS, 'exact admission loss interval')
        first, last = uint(loss['first_op_id'], low=1), uint(loss['last_op_id'], low=1)
        need(first <= last, 'ordered loss IDs')
        need(type(loss['kind']) is str and loss['kind'] in
             {'overflow', 'coverage_gap', 'unsupported_access'}, 'loss kind')
        bound = loss['entry_lower_bound_qpc']; _clock(bound)
        if bound is None:
            issues.add('LOSS_WITHOUT_RELIABLE_ENTRY_LOWER_BOUND')
        else:
            need(observer_begin_qpc <= bound <= freeze_qpc, 'loss lower bound in window')
        intervals.append((first, last, bound)); lost += last - first + 1
        uint(lost)
        barriers.append({'reason': 'ADMISSION_LOSS_' + loss['kind'].upper(),
                         'first_op_id': first, 'last_op_id': last, 'qpc': bound})
    counts = source['counts']; exact(counts, COUNT_KEYS, 'exact inventory counters')
    for value in counts.values():
        uint(value)
    need(counts['stored_operations'] == len(ids) and counts['pending_operations'] == pending and
         counts['completed_operations'] == len(ids) - pending and counts['lost_operations'] == lost and
         counts['observed_operations'] == len(ids) + lost, 'complete admission accounting')
    next_id, last_clock = 1, None
    for first, last, clock in sorted(intervals):
        need(first == next_id, 'operation IDs/loss ranges must partition complete inventory')
        next_id = last + 1
        if clock is not None:
            need(last_clock is None or clock >= last_clock, 'nonmonotonic admission ID clocks')
            last_clock = clock
    need(next_id - 1 == counts['observed_operations'], 'exact final admission ID')

    # NO candidate processing occurs before the entire frozen metadata validates.
    barrier = min([freeze_qpc] + [b['qpc'] for b in barriers if b['qpc'] is not None])
    result = dict.fromkeys(FALSE_FLAGS, False)
    result.update(schema='mcla-semaphore-prefix-audit-v2', token=token,
                  original_receipt_sha256=hashlib.sha256(data).hexdigest().upper(),
                  freeze_qpc=freeze_qpc, global_barrier_qpc=barrier,
                  barrier_inventory=copy.deepcopy(barriers), loss_ranges=copy.deepcopy(source['loss_ranges']),
                  validated_operation_count=len(ids), original_counts=copy.deepcopy(counts),
                  associations=[], unique_credit_consumptions=[], prefixes=[],
                  excluded_operation_ids=sorted(ids), diagnostic_issues=[],
                  suffix_semantics='RETAINED_UNMODELED_NOT_ASSERTED_COUNTER_CONSISTENT',
                  projection_semantics='DERIVED_COMPLETE_PREFIX_NOT_ORIGINAL_NATIVE_RECEIPT')
    if not issues:
        for eid, epoch in epochs.items():
            rows = [row for row in source['operations'] if row['epoch_id'] == eid]
            groups = _components(rows)
            accepted, last_zero, balance = [], 0, 0
            stop_reason, stop_qpc = 'FROZEN_BARRIER', barrier
            for group in groups:
                start = min(r['entry_qpc'] for r in group)
                if start >= barrier or any(not r['completed'] or r['exit_qpc'] >= barrier for r in group):
                    stop_reason, stop_qpc = 'COMPONENT_TOUCHES_OR_CROSSES_BARRIER', min(start, barrier)
                    break
                balance, restriction, contradiction = _step(group, balance)
                if contradiction:
                    issues.add('EPOCH_' + str(eid) + ':' + contradiction)
                    stop_reason, stop_qpc = contradiction, start
                    break
                if restriction:
                    stop_reason, stop_qpc = restriction, start
                    break
                accepted.extend(group)
                if balance == 0:
                    last_zero = len(accepted)
            if last_zero < len(accepted):
                stop_qpc = min(stop_qpc, min(r['entry_qpc'] for r in accepted[last_zero:]))
                stop_reason = 'UNCONSUMED_TAIL_ROLLED_BACK_TO_ZERO_BALANCE'
            prefix_rows = accepted[:last_zero]
            retained_ids = {r['op_id'] for r in prefix_rows}
            summary = {'epoch_id': eid, 'end_qpc_exclusive': stop_qpc,
                       'prefix_eligible': bool(prefix_rows) and not issues,
                       'operation_ids': sorted(retained_ids),
                       'excluded_operation_ids': sorted(r['op_id'] for r in rows if r['op_id'] not in retained_ids),
                       'ending_count': 0, 'boundary_reason': stop_reason,
                       'derived_projection_sha256': None, 'candidate_count': 0}
            if prefix_rows and not issues:
                need(epoch['creation_qpc'] < stop_qpc and all(r['exit_qpc'] < stop_qpc for r in prefix_rows),
                     'strict observed closed prefix boundary')
                raw = _projection(source, epoch, prefix_rows, stop_qpc)
                checked = v1.audit(raw, token=token, begin_qpc=observer_begin_qpc,
                                   end_qpc=stop_qpc, qpc_frequency=qpc_frequency)
                unexpected = set(checked['diagnostic_issues']) - {'NO_UNAMBIGUOUS_UPDATE_WAIT_CANDIDATE'}
                need(not unexpected, 'derived complete prefix must satisfy unchanged v1 rules')
                summary['derived_projection_sha256'] = hashlib.sha256(raw).hexdigest().upper()
                summary['candidate_count'] = len(checked['associations'])
                result['associations'].extend(checked['associations'])
                result['unique_credit_consumptions'].extend(checked['unique_credit_consumptions'])
            result['prefixes'].append(summary)
        if not issues:
            retained = {oid for prefix in result['prefixes'] for oid in prefix['operation_ids']}
            result['excluded_operation_ids'] = sorted(ids - retained)
    if issues:
        result['associations'] = []; result['unique_credit_consumptions'] = []
        for prefix in result['prefixes']:
            prefix['prefix_eligible'] = False
            prefix['candidate_count'] = 0
        result['excluded_operation_ids'] = sorted(ids)
    result['diagnostic_issues'] = sorted(issues)
    result['status'] = ('SEMAPHORE_PREFIX_CANDIDATES_REVIEW_PENDING' if result['associations'] else
                        'SEMAPHORE_PREFIX_AUDIT_INCONCLUSIVE')
    return result
