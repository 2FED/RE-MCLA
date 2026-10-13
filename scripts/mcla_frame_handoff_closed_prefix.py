"""Separate, deliberately narrow closed-handoff refinement of recorded v1.

The original reader runs FIRST and is never modified. Only (a) valid tracked
pending worker-render scopes, and (b) unscoped nonsuppressed callbacks that are
absent from every source callback ID can become retained context. Nothing else
is relaxed. Explicit-ID chains must be wholly closed strictly before the first
pending render entry (or the real freeze when none is pending). No rows are
removed, re-timestamped, completed, donated or turned into native proof.
"""
import copy
import hashlib

import mcla_frame_handoff_decode as original

PENDING = 'PENDING_HANDOFF_SCOPE'
UNSCOPED = 'phase_source:phase:callbacks_without_observed_scope'
DERIVED = 'NO_COMPLETE_EXPLICIT_ID_HANDOFF_CANDIDATE'
FALSE_FLAGS = original.FALSE_FLAGS | {'render_proven', 'display_proven',
    'causal_join_proven', 'object_identity_proven', 'object_lifetime_proven',
    'coverage_proven', 'closed_prefix_proven'}


def _partition(analysis, phase):
    hand = analysis['handoff_inventory']
    blockers = set(analysis['diagnostic_issues'])
    retained = set()
    if DERIVED in blockers:
        blockers.remove(DERIVED); retained.add(DERIVED)
    pending = [row for row in hand['scopes'] if not row['completed']]
    phase_scopes = {row['call_id']: row for row in phase['scopes']}
    operations = {row['op_id']: row for row in analysis['prefix_inventory']['operations']}
    def valid_pending_row(row):
        if not (row['tracked'] and row['kind'] == 3 and row['entry_qpc'] > 0 and
                row['exit_qpc'] == 0 and not row['normal_return']):
            return False
        p = phase_scopes[row['phase_call_id']]
        wait = operations[row['wait_operation']]
        edge = _one([event for event in hand['worker_events']
                     if event['event'] == 6 and event['phase_call_id'] == row['phase_call_id']])
        return (row['caller_lr'] == 0x82305F80 and p['incoming_r3'] == 0 and
                p['context_nonnull'] and p['base_nonnull'] and wait['completed'] and
                wait['payload']['result'] == 'success' and wait['exit_qpc'] is not None and
                0 < wait['exit_qpc'] <= row['entry_qpc'] and
                wait['native_thread_id'] == row['native_thread_id'] and edge is not None and
                edge['eligible'] and edge['phase_bound'] and edge['qpc'] == row['entry_qpc'] and
                edge['operation'] == row['wait_operation'] and edge['object_epoch'] == row['object_epoch'] and
                edge['thread_epoch'] == row['thread_epoch'] and edge['native_thread_id'] == row['native_thread_id'] and
                edge['owner'] == hand['selected_owner'])
    valid_pending = bool(pending) and all(valid_pending_row(row) for row in pending)
    if PENDING in blockers and valid_pending:
        blockers.remove(PENDING); retained.add(PENDING)
    # The exact raw phase inventory, not just the selected source joins, proves
    # absence. A single captured or suppressed unscoped callback forbids this
    # relaxation for the WHOLE issue, not only that callback's source slot.
    captured_ids = {row['callback_id'] for row in analysis['source_phase_analysis']['records']
                    if row['callback_id'] is not None}
    unscoped = [row for row in phase['swap_callbacks'] if not row['has_scope']]
    safe_unscoped = bool(unscoped) and all(
        not row['suppressed_scope'] and row['callback_id'] not in captured_ids for row in unscoped)
    if UNSCOPED in blockers and safe_unscoped:
        blockers.remove(UNSCOPED); retained.add(UNSCOPED)
    barrier = min((row['entry_qpc'] for row in pending), default=hand['freeze_qpc']) if valid_pending else hand['freeze_qpc']
    return blockers, retained, pending, unscoped, barrier


def _one(rows):
    return rows[0] if len(rows) == 1 else None


def _closed_chain(candidate, hand, phase, swap, prefix_inventory, barrier, pending):
    """Reject an explicit candidate, never choose a replacement edge by time.

    The phase journal may finish after the immutable handoff cut. Its later
    completions cannot repair pending handoff rows or widen the prefix boundary.
    Dispatcher completion is selected by exact worker lifetime + dispatch ID.
    """
    scopes = {row['phase_call_id']: row for row in hand['scopes']}
    phase_scopes = {row['call_id']: row for row in phase['scopes']}
    operations = {row['op_id']: row for row in prefix_inventory['operations']}
    pending_phase = {row['phase_call_id'] for row in pending}
    pending_waits = {row['wait_operation'] for row in pending}
    phase_ids = [candidate[key] for key in ('update_phase_call_id',
                 'coordinator_phase_call_id', 'render_phase_call_id')]
    if set(phase_ids) & pending_phase or candidate['wait_operation_id'] in pending_waits:
        return 'PENDING_ID_CANNOT_BE_REUSED_OR_BACKFILLED', None
    clocks = []
    for pid in phase_ids:
        h, p = scopes[pid], phase_scopes[pid]
        if not (h['tracked'] and h['completed'] and h['normal_return'] and
                p['completed'] and not p['original_exception'] and not p['invalid_bracket'] and
                p['context_nonnull'] and p['base_nonnull']):
            return 'CHAIN_SCOPE_NOT_POSITIVELY_CLOSED', None
        clocks.extend((h['entry_qpc'], h['exit_qpc'], p['entry_qpc'], p['exit_qpc']))
    for oid in (candidate['release_operation_id'], candidate['wait_operation_id']):
        op = operations[oid]
        if not op['completed'] or op['entry_qpc'] is None or op['exit_qpc'] is None:
            return 'CHAIN_OPERATION_NOT_POSITIVELY_CLOSED', None
        clocks.extend((op['entry_qpc'], op['exit_qpc']))
    render = _one([row for row in hand['worker_events']
                   if row['event'] == 6 and row['phase_call_id'] == candidate['render_phase_call_id']])
    if render is None:
        return 'RENDER_EVENT_ID_NOT_UNIQUE', None
    key = (render['thread_epoch'], render['worker_serial'])
    same_worker = [row for row in hand['worker_events']
                   if (row['thread_epoch'], row['worker_serial']) == key]
    wait_entry = _one([row for row in same_worker if row['event'] == 2 and row['helper'] == render['helper']])
    wait_exit = _one([row for row in same_worker if row['event'] == 3 and row['helper'] == render['helper']
                      and row['operation'] == candidate['wait_operation_id']])
    dispatch_entry = _one([row for row in same_worker if row['event'] == 5 and row['dispatch'] == render['dispatch']])
    dispatch_exit = _one([row for row in same_worker if row['event'] == 7 and row['dispatch'] == render['dispatch']])
    if any(row is None for row in (wait_entry, wait_exit, dispatch_entry, dispatch_exit)):
        return 'EXACT_HELPER_OR_DISPATCH_COMPLETION_NOT_UNIQUE', None
    h = scopes[candidate['render_phase_call_id']]; p = phase_scopes[candidate['render_phase_call_id']]
    if not (dispatch_exit['reason'] == 0 and not dispatch_exit['eligible'] and
            dispatch_exit['phase_call_id'] == 0 and not dispatch_exit['phase_bound'] and
            dispatch_exit['retired_operation'] == 0 and dispatch_exit['retirement_reason'] == 0 and
            dispatch_exit['helper'] == render['helper'] and
            dispatch_exit['operation'] == candidate['wait_operation_id'] and
            dispatch_exit['native_thread_id'] == render['native_thread_id'] and
            max(h['exit_qpc'], p['exit_qpc']) <= dispatch_exit['qpc']):
        return 'DISPATCH_COMPLETION_NOT_CLOSED_NORMAL_MATCHING_CHAIN', None
    clocks.extend(row['qpc'] for row in (wait_entry, wait_exit, dispatch_entry, render, dispatch_exit))
    callback = _one([row for row in phase['swap_callbacks'] if row['callback_id'] == candidate['callback_id']])
    if callback is None or callback['active_scope_call_id'] != candidate['render_phase_call_id']:
        return 'CALLBACK_ID_NOT_EXACT_RENDER', None
    clocks.append(callback['qpc'])
    # Include the exact emission bracket and packet-consumption witness as
    # well; do not substitute a nearby binding from another source slot.
    binding = swap['records'][candidate['slot']]['binding']
    if binding is None or binding['snapshot'] is None:
        return 'EXACT_SOURCE_EMISSION_WITNESS_MISSING', None
    clocks.extend((binding['snapshot']['qpc_before'], binding['snapshot']['qpc_after'], binding['consume_qpc']))
    if not all(type(clock) is int and 0 < clock < barrier for clock in clocks):
        return 'CHAIN_TOUCHES_OR_CROSSES_STRICT_BARRIER', None
    return None, {'barrier_qpc_exclusive': barrier, 'chain_last_observed_qpc': max(clocks),
                  'worker_thread_epoch': key[0], 'worker_serial': key[1],
                  'helper_id': render['helper'], 'dispatch_id': render['dispatch']}


def _refine(analysis, phase, swap):
    blockers, retained, pending, unscoped, barrier = _partition(analysis, phase)
    hand = analysis['handoff_inventory']; pre = analysis['prefix_inventory']
    scopes = {row['phase_call_id']: row for row in hand['scopes']}
    annotations = {row['operation_id']: row for row in hand['operation_annotations']}
    operations = {row['op_id']: row for row in pre['operations']}
    candidates, dispositions = original._evaluate(hand, analysis['prefix_audit'],
        analysis['source_phase_analysis'], scopes, annotations, operations, blockers)
    accepted, excluded = [], []
    for candidate in candidates:
        reason, witness = _closed_chain(candidate, hand, phase, swap, pre, barrier, pending)
        if reason:
            excluded.append({'candidate': copy.deepcopy(candidate), 'reason': reason})
        else:
            value = copy.deepcopy(candidate); value['closed_chain_witness'] = witness
            accepted.append(value)
    accepted_slots = {row['slot'] for row in accepted}
    excluded_slots = {row['candidate']['slot']: row['reason'] for row in excluded}
    for row in dispositions:
        if row['slot'] in accepted_slots:
            row['disposition'] = 'STRICTLY_CLOSED_EXPLICIT_ID_CANDIDATE_REVIEW_PENDING'
        elif row['slot'] in excluded_slots:
            row['disposition'] = 'CLOSED_CHAIN_EXCLUDED:' + excluded_slots[row['slot']]
    captured_ids = {row['callback_id'] for row in analysis['source_phase_analysis']['records']}
    result = dict.fromkeys(FALSE_FLAGS, False)
    context = bool(retained)
    result.update(schema='mcla-frame-handoff-closed-prefix-analysis-v1', token=analysis['token'],
        status=('FRAME_HANDOFF_CLOSED_PREFIX_PARTIAL_INCONCLUSIVE' if context else
                'FRAME_HANDOFF_CLOSED_PREFIX_DIAGNOSTIC_INCONCLUSIVE' if blockers or not accepted or excluded else
                'FRAME_HANDOFF_CLOSED_PREFIX_CANDIDATES_REVIEW_PENDING'),
        candidate_blocking_issues=sorted(blockers), retained_context_issues=sorted(retained),
        excluded_candidate_records=excluded, candidate_records=accepted,
        candidate_source_dispositions=copy.deepcopy(dispositions),
        closed_prefix_barrier_qpc_exclusive=barrier,
        pending_handoff_scopes=copy.deepcopy(pending),
        uncaptured_unscoped_callback_ids=sorted(row['callback_id'] for row in unscoped
            if not row['suppressed_scope'] and row['callback_id'] not in captured_ids),
        original_analysis=copy.deepcopy(analysis), phase_inventory=copy.deepcopy(phase),
        swap_inventory=copy.deepcopy(swap), physical_acceptance_pending=True,
        unobserved_ancestry_possible=True,
        interpretation='STRICTLY_CLOSED_EXPLICIT_ID_CANDIDATES_WITH_ALL_ORIGINAL_CONTEXT_RETAINED_NOT_NATIVE_PROOF')
    return result


def validate_closed_handoffs(handoff_raw, prefix_raw, phase_raw, swap_raw, source_receipt, token):
    """Original full validation FIRST; no journal transformations or IO.

    All faults other than the two exact allowlisted contexts remain global
    blockers. An original no-candidate outcome is retained, not misrepresented
    as a semantic fault or erased after the refinement finds candidate chains.
    """
    source = copy.deepcopy(source_receipt)
    analysis = original.validate_frame_handoff(handoff_raw, prefix_raw, phase_raw,
                                              swap_raw, copy.deepcopy(source), token)
    # These immutable bytes have already passed every original metadata check.
    # Keep the exact phase inventory to classify unselected callbacks and later
    # phase completions, neither of which may repair an immutable handoff cut.
    phase = original._parse(phase_raw, 2*1024*1024)
    swap = original._parse(swap_raw, 128*1024)
    result = _refine(copy.deepcopy(analysis), phase, swap)
    result['source_inventory'] = source
    result['raw_receipt_sha256'] = {name: hashlib.sha256(value).hexdigest()
        for name, value in (('handoff', handoff_raw), ('prefix', prefix_raw),
                            ('phase', phase_raw), ('swap', swap_raw))}
    return result
