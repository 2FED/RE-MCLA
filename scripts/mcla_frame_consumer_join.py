"""Pure exact-ID source/router/consumer-detail join. No IO or execution proof.

Both complete raw validator chains run before evaluation. Clocks reject an
already identity-selected relation; they never find or replace an invocation.
Groups are normalized inventory, not acceptance unless a source references one.
"""
import copy
import hashlib

import mcla_consumer_detail_decode as detail_decoder
import mcla_frame_router_join as router_join

need = detail_decoder.need
FALSE_FLAGS = detail_decoder.FALSE_FLAGS | router_join.FALSE_FLAGS | {
    'source_consumer_join_proven', 'consumer_router_association_proven',
    'child_target_resolved_proven', 'timer_observation_to_detail_invocation_proven'}


def _id(value):
    return value['thread_epoch'], value['serial']


def _update(value):
    return value['phase_call_id'], value['thread_epoch'], value['native_thread_id']


def _mismatch(detail, router, detail_cut, router_cut, barrier):
    entry = detail['frame']['entry']
    saved = detail['events'][0]['meta']['observed_consumer']
    dispatch = router['frame']['consumer']
    if (not detail['normal_return'] or entry['router'] != router['invocation'] or
        entry['update'] != router['update_binding'] or
        entry['actual_target'] != detail_decoder.TARGET or entry['caller_lr'] != detail_decoder.CALLER or
        dispatch['requested_target_u32'] != detail_decoder.TARGET or
        entry['r3'] != dispatch['r3'] or entry['f1_bits'] != dispatch['f1_bits'] or
        saved['r3'] != dispatch['r3'] or saved['f1_bits'] != dispatch['f1_bits'] or
        saved['ctr'] != dispatch['ctr']):
        return 'DETAIL_ROUTER_IDENTITY_OR_SAVED_OPERAND_MISMATCH'
    for event in detail['events']:
        qpc = event['meta']['qpc']
        if not (router['consumer_pre_qpc'] <= qpc <= router['consumer_return_qpc'] and
                qpc <= detail_cut and qpc <= router_cut and qpc < barrier):
            return 'DETAIL_EVENTS_OUTSIDE_ENCLOSING_DISPATCH_CUT_OR_FRONTIER'
    return None


def _evaluate(detail, joined, issues, contexts):
    # Global health damage cannot be repaired by a favorable subset.
    if issues:
        return [], [], []
    router_analysis = joined['router_analysis']
    routers = {}
    for row in router_analysis['candidate_records']:
        rid = _id(row['invocation'])
        need(rid not in routers, 'unique complete router invocation inventory')
        routers[rid] = row
    details, detail_ids = {}, set()
    extras = []
    for row in detail['completed_invocations']:
        entry = row['frame']['entry']; iid = _id(entry['invocation']); rid = _id(entry['router'])
        need(iid not in detail_ids, 'unique complete detail invocation inventory')
        detail_ids.add(iid)
        details.setdefault(rid, []).append(row)
        if rid not in routers or routers[rid]['frame']['consumer']['requested_target_u32'] != detail_decoder.TARGET:
            extras.append(copy.deepcopy(row))
    if extras:
        # An orphan may claim a different Update, so it cannot safely be hidden
        # by assigning it to just a convenient selected source group.
        issues.add('EXTRANEOUS_DETAIL_WITHOUT_MATCHING_TARGET_ROUTER')
        return [], [], extras
    groups = {_update(g['update_binding']): g for g in joined['router_update_groups']}
    barrier = joined['timing_frontier_analysis']['closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive']
    dcut = detail['consumer_detail_inventory']['freeze_qpc']
    rcut = router_analysis['router_inventory']['freeze_qpc']
    selected_updates = {_update(s['router_update_binding']) for s in joined['candidate_records']}
    # Also check complete detail inventory outside the selected source groups.
    # It is not accepted through the frontier, but cannot hide a contradictory
    # router relation merely because no selected source happens to reference it.
    for rid, rows in details.items():
        router = routers[rid]
        if _update(router['update_binding']) in selected_updates: continue
        if len(rows)!=1 or _mismatch(rows[0],router,dcut,rcut,detail_decoder.QMAX+1):
            issues.add('UNSELECTED_DETAIL_ROUTER_RELATION_INVALID')
    if issues:return [], [], extras
    accepted, excluded = [], []
    for source in joined['candidate_records']:
        group = groups[_update(source['router_update_binding'])]
        expected = [r for r in group['invocations'] if r['frame']['consumer']['requested_target_u32'] == detail_decoder.TARGET]
        unknown = [copy.deepcopy(r['invocation']) for r in group['invocations'] if r not in expected]
        reason = None if expected else 'NO_REQUESTED_TARGET_ROUTER_IN_SELECTED_UPDATE'
        selected = []
        for router in expected:
            matches = details.get(_id(router['invocation']), [])
            if len(matches) != 1:
                reason = 'MISSING_OR_DUPLICATE_DETAIL_FOR_TARGET_ROUTER'; break
            reason = _mismatch(matches[0], router, dcut, rcut, barrier)
            if reason: break
            selected.append(copy.deepcopy(matches[0]['frame']['entry']['invocation']))
        if reason:
            contexts.add(reason)
            excluded.append({'source_candidate': copy.deepcopy(source), 'reason': reason,
                'expected_target_router_count': len(expected), 'unknown_router_invocations': unknown})
            continue
        record = copy.deepcopy(source)
        record.update(consumer_detail_invocations=selected, consumer_detail_group_count=len(selected),
            unknown_router_invocations=unknown,
            interpretation='EXACT_ROUTER_ID_AND_UPDATE_METADATA_CHAIN_NOT_CONSUMER_INTERNALS_OR_SIMULATION_PROOF')
        if unknown: contexts.add('OTHER_ROUTER_TARGETS_RETAINED_UNKNOWN')
        accepted.append(record)
    if not accepted: contexts.add('NO_COMPLETE_SOURCE_CONSUMER_GROUP')
    return accepted, excluded, extras


def validate_frame_consumer_join(consumer_raw, router_raw, timing_raw, handoff_raw,
                                prefix_raw, phase_raw, swap_raw, source_receipt, token):
    """Return detached raw analyses and exact-ID groups; raw bytes only, no IO."""
    detail = detail_decoder.validate_consumer_detail(consumer_raw, token)
    joined = router_join.validate_frame_router_join(router_raw, timing_raw, handoff_raw,
        prefix_raw, phase_raw, swap_raw, copy.deepcopy(source_receipt), token)
    d = detail['consumer_detail_inventory']; r = joined['router_analysis']['router_inventory']
    for field in ('token', 'clock', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency'):
        need(d[field] == r[field], 'identical detail/router session window and clock')
    if not d['clock_failed'] and not r['clock_failed']:
        need(d['freeze_qpc'] <= r['freeze_qpc'], 'consumer cut precedes router cut')
    issues = {'consumer:'+s for s in detail['candidate_blocking_issues']}
    issues.update('router_join:'+s for s in joined['candidate_blocking_issues'])
    contexts = {'router_join:'+s for s in joined['retained_context_issues']}
    candidates, excluded, extras = _evaluate(detail, joined, issues, contexts)
    result = dict.fromkeys(FALSE_FLAGS, False)
    hashes = copy.deepcopy(joined['raw_sha256']); hashes['consumer'] = hashlib.sha256(consumer_raw).hexdigest()
    result.update(schema='mcla-frame-consumer-join-analysis-v1', token=token,
        status='FRAME_CONSUMER_JOIN_DIAGNOSTIC_INCONCLUSIVE' if issues or contexts or not candidates else
               'FRAME_CONSUMER_JOIN_CANDIDATES_REVIEW_PENDING', raw_sha256=hashes,
        consumer_analysis=copy.deepcopy(detail), frame_router_analysis=copy.deepcopy(joined),
        candidate_records=candidates, excluded_candidate_records=excluded,
        extraneous_detail_records=extras, candidate_blocking_issues=sorted(issues),
        retained_context_issues=sorted(contexts), physical_acceptance_pending=True,
        interpretation='EXPLICIT_IDS_AND_CONTAINMENT_ONLY_NO_NEAREST_TIME_NO_F1_SUM_NO_RESOLVED_CHILD_OR_SIMULATION_PROOF')
    return result
