"""Separate v2: exact known pending coordinator suffix, never repaired history.

Full original and closed-prefix-v1 validation run first and are retained. Only
the finite coordinator route below extends v1's pending-worker classification.
All candidate clocks must be strictly before every pending outer-phase and
handoff entry. No pixels,
files, nearest-time association, rewritten journal or physical proof.
"""
import copy

import mcla_frame_handoff_closed_prefix as baseline

FALSE_FLAGS = baseline.FALSE_FLAGS | {'closed_frontier_proven'}


def _pending_phase_valid(row, phases):
    p = phases[row['phase_call_id']]
    return (not p['invalid_bracket'] and not p['original_exception'] and
            0 < p['entry_qpc'] <= row['entry_qpc'] and
            (not p['completed'] or row['entry_qpc'] <= p['exit_qpc']))


def _pending_render(row, hand, phases, operations):
    # Same sufficient route as v1; no extra relaxation for worker scopes.
    if not (row['tracked'] and row['kind'] == 3 and row['entry_qpc'] > 0 and
            row['exit_qpc'] == 0 and not row['normal_return'] and
            _pending_phase_valid(row, phases)):
        return False
    p = phases[row['phase_call_id']]
    wait = operations.get(row['wait_operation'])
    edge = baseline._one([e for e in hand['worker_events']
                          if e['event'] == 6 and e['phase_call_id'] == row['phase_call_id']])
    return bool(wait and edge and row['caller_lr'] == 0x82305F80 and
        p['incoming_r3'] == 0 and p['context_nonnull'] and p['base_nonnull'] and
        wait['completed'] and wait['payload'].get('result') == 'success' and
        wait['exit_qpc'] is not None and 0 < wait['exit_qpc'] <= row['entry_qpc'] and
        wait['native_thread_id'] == row['native_thread_id'] and edge['eligible'] and
        edge['phase_bound'] and edge['qpc'] == row['entry_qpc'] and
        edge['operation'] == row['wait_operation'] and edge['object_epoch'] == row['object_epoch'] and
        edge['thread_epoch'] == row['thread_epoch'] and edge['native_thread_id'] == row['native_thread_id'] and
        edge['owner'] == hand['selected_owner'])


def _pending_coordinator(row, hand, phases, operations):
    if not (row['tracked'] and row['kind'] == 1 and not row['completed'] and
            row['exit_qpc'] == 0 and not row['normal_return'] and row['entry_qpc'] > 0 and
            row['caller_lr'] == 0x821316B8 and row['object_epoch'] == hand['selected_epoch'] and
            row['wait_operation'] == 0 and _pending_phase_valid(row, phases)):
        return False, []
    p = phases[row['phase_call_id']]
    if not (p['kind'] == 'coordinator' and p['caller_lr'] == 0x821316B8 and
            p['context_nonnull'] and p['base_nonnull'] and
            (p['incoming_r3'] & 0xffffffff) == hand['selected_owner'] and
            not p['invalid_bracket'] and not p['original_exception'] and
            0 < p['entry_qpc'] <= row['entry_qpc'] <= hand['freeze_qpc'] and
            (not p['completed'] or row['entry_qpc'] <= p['exit_qpc'])):
        return False, []
    identity = (row['update_thread_epoch'], row['update_serial'])
    update = baseline._one([u for u in hand['scopes'] if u['kind'] == 0 and
                           (u['thread_epoch'], u['serial']) == identity])
    if update is None:
        return False, []
    # Tracker consumes pending_update at coordinator entry. An admitted
    # coordinator must be the next serial and the sole coordinator owner of
    # that update; explicit identity alone cannot legitimize reused credit.
    owners = [u for u in hand['scopes'] if u['kind'] == 1 and
              (u['update_thread_epoch'], u['update_serial']) == identity]
    if len(owners) != 1 or row['serial'] != update['serial'] + 1:
        return False, []
    up = phases[update['phase_call_id']]
    if not (update['tracked'] and update['completed'] and update['normal_return'] and
            update['caller_lr'] == 0x821316A0 and up['kind'] == 'update' and
            up['caller_lr'] == 0x821316A0 and up['completed'] and
            not up['original_exception'] and not up['invalid_bracket'] and
            up['context_nonnull'] and up['base_nonnull'] and
            update['thread_epoch'] == row['thread_epoch'] and
            update['native_thread_id'] == row['native_thread_id'] == p['native_thread_id'] == up['native_thread_id'] and
            up['thread_epoch'] == p['thread_epoch'] and
            0 < up['entry_qpc'] <= update['entry_qpc'] <= update['exit_qpc'] <= up['exit_qpc'] < p['entry_qpc']):
        return False, []
    owned = []
    for annotation in hand['operation_annotations']:
        if annotation['phase_call_id'] != row['phase_call_id']:
            continue
        op = operations[annotation['operation_id']]
        if not (annotation['annotated'] and annotation['caller_lr'] == 0x82305DDC and
                annotation['native_thread_id'] == row['native_thread_id'] == op['native_thread_id'] and
                op['epoch_id'] == row['object_epoch'] and op['kind'] == 'release' and op['completed'] and
                op['entry_qpc'] is not None and op['exit_qpc'] is not None and
                row['entry_qpc'] <= op['entry_qpc'] <= op['exit_qpc'] <= hand['freeze_qpc'] and
                (not p['completed'] or op['exit_qpc'] <= p['exit_qpc']) and
                op['payload']['context'] == {'kind': 'update', 'generation':
                    {'thread_epoch': identity[0], 'serial': identity[1]}}):
            return False, []
        owned.append(op['op_id'])
    return True, owned


def validate_closed_frontier(handoff_raw, prefix_raw, phase_raw, swap_raw, source_receipt, token):
    """Return detached candidate evidence; retain v1's exact original verdict."""
    old = baseline.validate_closed_handoffs(handoff_raw, prefix_raw, phase_raw, swap_raw,
                                          source_receipt, token)
    result = copy.deepcopy(old)
    result['v1_baseline_analysis'] = copy.deepcopy(old)
    analysis = old['original_analysis']
    hand, phase, swap = analysis['handoff_inventory'], old['phase_inventory'], old['swap_inventory']
    operations = {o['op_id']: o for o in analysis['prefix_inventory']['operations']}
    phases = {p['call_id']: p for p in phase['scopes']}
    pending = [h for h in hand['scopes'] if not h['completed']]
    classifications, coordinator_operations = [], []
    valid = bool(pending)
    for row in pending:
        render = _pending_render(row, hand, phases, operations)
        coord, owned = _pending_coordinator(row, hand, phases, operations)
        route = 'KNOWN_WORKER_RENDER' if render else 'KNOWN_MAIN_COORDINATOR' if coord else 'UNCLASSIFIED'
        classifications.append({'phase_call_id': row['phase_call_id'], 'route': route,
                                'owned_operation_ids': list(owned)})
        coordinator_operations.extend(owned)
        valid = valid and (render or coord)
    blockers = set(old['candidate_blocking_issues'])
    retained = set(old['retained_context_issues'])
    if baseline.PENDING in blockers and valid:
        blockers.remove(baseline.PENDING)
        retained.add(baseline.PENDING)
    elif pending and not valid:
        # v1 may already have accepted a worker suffix without the stronger
        # phase-bound checks. Its relaxation must not bypass this contract.
        blockers.add(baseline.PENDING)
        retained.discard(baseline.PENDING)
    # For all known pending routes use their EARLIEST entry, regardless of array
    # order. Later phase completion never changes handoff's immutable cut/state.
    barrier = min((min(p['entry_qpc'], phases[p['phase_call_id']]['entry_qpc'])
                   for p in pending), default=hand['freeze_qpc'])
    scopes = {h['phase_call_id']: h for h in hand['scopes']}
    annotations = {a['operation_id']: a for a in hand['operation_annotations']}
    candidates, dispositions = baseline.original._evaluate(hand, analysis['prefix_audit'],
        analysis['source_phase_analysis'], scopes, annotations, operations, blockers)
    accepted, excluded = [], []
    for candidate in candidates:
        if candidate['release_operation_id'] in coordinator_operations or candidate['wait_operation_id'] in coordinator_operations:
            reason, witness = 'PENDING_COORDINATOR_OPERATION_CANNOT_BE_REUSED', None
        else:
            reason, witness = baseline._closed_chain(candidate, hand, phase, swap,
                analysis['prefix_inventory'], barrier, pending)
        if reason:
            excluded.append({'candidate': copy.deepcopy(candidate), 'reason': reason})
        else:
            record = copy.deepcopy(candidate)
            record['closed_chain_witness'] = witness
            accepted.append(record)
    accepted_slots = {c['slot'] for c in accepted}
    excluded_slots = {c['candidate']['slot']: c['reason'] for c in excluded}
    for row in dispositions:
        if row['slot'] in accepted_slots:
            row['disposition'] = 'STRICTLY_CLOSED_EXPLICIT_ID_CANDIDATE_REVIEW_PENDING'
        elif row['slot'] in excluded_slots:
            row['disposition'] = 'CLOSED_CHAIN_EXCLUDED:' + excluded_slots[row['slot']]
    result.update(dict.fromkeys(FALSE_FLAGS, False))
    result.update(schema='mcla-frame-handoff-closed-frontier-analysis-v2',
        status=('FRAME_HANDOFF_CLOSED_FRONTIER_PARTIAL_INCONCLUSIVE' if retained else
                'FRAME_HANDOFF_CLOSED_FRONTIER_DIAGNOSTIC_INCONCLUSIVE' if blockers or not accepted or excluded else
                'FRAME_HANDOFF_CLOSED_FRONTIER_CANDIDATES_REVIEW_PENDING'),
        candidate_records=accepted, excluded_candidate_records=excluded,
        candidate_source_dispositions=dispositions,
        candidate_blocking_issues=sorted(blockers), retained_context_issues=sorted(retained),
        closed_prefix_barrier_qpc_exclusive=barrier,
        pending_scope_classifications=classifications,
        pending_coordinator_operation_ids=sorted(coordinator_operations),
        interpretation='KNOWN_PENDING_SUFFIX_EXCLUDED_STRICT_CLOSED_ID_CANDIDATES_NOT_NATIVE_PROOF')
    return result
