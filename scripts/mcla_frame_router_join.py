"""Pure exact-Update-ID source/router metadata join, not simulation proof.

Both raw journal validators finish before this evaluator. A source may share an
Update with another source; this is not a one-frame/one-step assertion. Every
eligible router invocation for the selected Update is retained, never chosen by
nearest time or paired with a particular outer timer observation.
"""
import copy
import hashlib

import mcla_router_dispatch_decode as router_decoder
import mcla_update_timing_frontier as timing_decoder

FALSE_FLAGS = router_decoder.FALSE_FLAGS | {
    'actual_consumer_proven', 'causal_join_proven', 'source_router_join_proven',
    'source_timing_join_proven', 'update_causality_proven', 'display_proven',
    'closed_frontier_proven', 'timer_observation_to_router_invocation_proven'}
need = router_decoder.need


def _update_id(binding):
    return binding['phase_call_id'], binding['thread_epoch'], binding['native_thread_id']


def _invocation_id(candidate):
    return candidate['invocation']['thread_epoch'], candidate['invocation']['serial']


def _evaluate(router, timing, issues, contexts):
    """Only internally validated analyses reach this exact-ID evaluator."""
    if issues:
        return [], []
    closed = timing['closed_handoff_analysis']
    barrier = closed['closed_prefix_barrier_qpc_exclusive']
    phases = {p['call_id']: p for p in router['phase_inventory']['scopes']}
    by_update, ids = {}, set()
    for candidate in router['candidate_records']:
        iid = _invocation_id(candidate)
        need(iid not in ids, 'unique router invocation IDs across candidate inventory')
        ids.add(iid)
        by_update.setdefault(_update_id(candidate['update_binding']), []).append(candidate)
    accepted, excluded = [], []
    for source in timing['candidate_records']:
        identity = (source['update_phase_call_id'], source['timing_phase_thread_epoch'],
                    source['timing_native_thread_id'])
        matches = by_update.get(identity, [])
        reason = None
        phase = phases.get(identity[0])
        if not (phase and phase['kind'] == 'update' and phase['thread_epoch'] == identity[1] and
                phase['native_thread_id'] == identity[2] and phase['completed'] and
                not phase['original_exception'] and not phase['invalid_bracket'] and
                phase['context_nonnull'] and phase['base_nonnull']):
            reason = 'SELECTED_UPDATE_IDENTITY_OR_HEALTH_MISMATCH'
        elif not matches:
            reason = 'NO_COMPLETE_ROUTER_INVOCATIONS_FOR_EXACT_UPDATE'
        else:
            for candidate in matches:
                clocks = tuple(candidate[key] for key in
                    ('entry_qpc', 'consumer_pre_qpc', 'consumer_return_qpc', 'router_exit_qpc'))
                # Reject edges of an already ID-selected group; clocks never
                # choose a substitute or drop just the inconvenient invocation.
                if not (all(type(t) is int and 0 < t < barrier for t in clocks) and
                        phase['entry_qpc'] <= clocks[0] <= clocks[1] <= clocks[2] <= clocks[3] <= phase['exit_qpc'] < barrier):
                    reason = 'EXACT_UPDATE_ROUTER_GROUP_NOT_STRICTLY_BEFORE_FRONTIER'
                    break
        if reason:
            contexts.add(reason)
            excluded.append({'source_candidate': copy.deepcopy(source), 'reason': reason,
                             'matched_router_group_count': len(matches)})
            continue
        record = copy.deepcopy(source)
        record.update(router_update_binding={'phase_call_id': identity[0], 'thread_epoch': identity[1],
                                   'native_thread_id': identity[2]},
            router_group_count=len(matches), closed_frontier_qpc_exclusive=barrier,
            interpretation='SHARED_EXPLICIT_UPDATE_GROUP_NOT_TIMER_ROW_TO_ROUTER_OR_SIMULATION_STEP_MAPPING')
        accepted.append(record)
    if not accepted:
        contexts.add('NO_COMPLETE_SOURCE_ROUTER_UPDATE_GROUP')
    return accepted, excluded


def validate_frame_router_join(router_raw, timing_raw, handoff_raw, prefix_raw,
                              phase_raw, swap_raw, source_receipt, token):
    """Validate raw inventories, then join exact IDs; no IO or native action."""
    source = copy.deepcopy(source_receipt)
    router = router_decoder.validate_router_dispatch(router_raw, phase_raw, source, token)
    timing = timing_decoder.validate_update_timing_frontier(timing_raw, handoff_raw,
        prefix_raw, phase_raw, swap_raw, source, token)
    # Shared bytes are validated by both independent paths; this assertion also
    # guards future adapter changes from silently changing the phase namespace.
    need(router['phase_inventory'] == timing['closed_handoff_analysis']['phase_inventory'],
         'identical validated phase inventory')
    issues = {'router:'+s for s in router['candidate_blocking_issues']}
    issues.update('timing:'+s for s in timing['candidate_blocking_issues'])
    contexts = {'timing:'+s for s in timing['retained_context_issues']}
    candidates, excluded = _evaluate(router, timing, issues, contexts)
    # A normalized group stores each invocation once, even when many source
    # samples share one Update. Candidate.router_update_binding is its exact
    # key; no repeated potentially large frame snapshots per source sample.
    # These groups remain router inventory, not blanket frontier acceptance:
    # only an emitted source candidate references an accepted complete group.
    groups = {}
    for invocation in router['candidate_records']:
        identity = _update_id(invocation['update_binding'])
        group = groups.setdefault(identity, {'update_binding': copy.deepcopy(invocation['update_binding']), 'invocations': []})
        group['invocations'].append(copy.deepcopy(invocation))
    result = dict.fromkeys(FALSE_FLAGS, False)
    result.update(schema='mcla-frame-router-join-analysis-v1', token=token,
        status='FRAME_ROUTER_JOIN_DIAGNOSTIC_INCONCLUSIVE' if issues or contexts or not candidates else
               'FRAME_ROUTER_JOIN_CANDIDATES_REVIEW_PENDING',
        raw_sha256={name: hashlib.sha256(value).hexdigest() for name,value in (
            ('router',router_raw),('timing',timing_raw),('handoff',handoff_raw),
            ('prefix',prefix_raw),('phase',phase_raw),('swap',swap_raw))},
        router_analysis=copy.deepcopy(router), timing_frontier_analysis=copy.deepcopy(timing),
        candidate_records=candidates, excluded_candidate_records=excluded,
        router_update_groups=[groups[key] for key in sorted(groups)],
        candidate_blocking_issues=sorted(issues), retained_context_issues=sorted(contexts),
        physical_acceptance_pending=True,
        interpretation='EXPLICIT_UPDATE_GROUP_WITH_COMPLETED_ROUTER_DISPATCH_OBSERVATIONS_NO_CONSUMER_INTERNALS_OR_SIMULATION_PROOF')
    return result
