"""Pure exact-ID source/virtual-call0/selected-work join.

All immutable raw validator chains finish before relation evaluation. Clock
brackets reject explicit identity-selected relations; no nearest-time, later
completion backfill, scalar arithmetic or task/simulation acceptance exists.
"""
import copy
import hashlib

import mcla_work_emission_decode as work_decoder
import mcla_frame_virtual_join as virtual_join

need = work_decoder.need
FALSE_FLAGS = work_decoder.FALSE_FLAGS | virtual_join.FALSE_FLAGS | {
    'work_virtual_association_proven', 'actual_work_execution_proven',
    'total_work_inventory_proven', 'frame_to_step_mapping_proven'}


def _id(value):
    return value['thread_epoch'], value['serial']


def _applicable(parent):
    call = parent['frame']['calls'][0]
    return (call['state'] == 2 and call['caller_lr'] == work_decoder.CALLER and
            call['requested_target'] == work_decoder.TAIL)


def _bounds(parent):
    pre = [e for e in parent['events'] if e['event'] == 1 and e['frame']['returned_calls'] == 0]
    end = [e for e in parent['events'] if e['event'] == 2 and e['frame']['returned_calls'] == 1]
    need(len(pre) == len(end) == 1, 'one exact virtual call0 pre/normal-return pair')
    return pre[0], end[0]


def _relation(body, parent, work_cut, parent_cut):
    """Contradictions anywhere are global; never hide an unselected bad row."""
    e = body['entry']; pre, end = _bounds(parent); call = pre['frame']['calls'][0]
    if (not body['normal_return'] or not parent['normal_return'] or
            e['virtual_entry'] != parent['frame']['entry'] or e['active_call0'] != call or
            e['actual_target'] != work_decoder.BODY or e['caller_lr'] != work_decoder.CALLER or
            call['requested_target'] != work_decoder.TAIL or
            any(e[k] != call[k] for k in ('r3', 'f1_bits'))):
        return 'WORK_VIRTUAL_IDENTITY_OR_SAVED_OPERAND_MISMATCH'
    for event in body['events']:
        q = event['meta']['qpc']
        if (event['meta']['native_thread_id'] != pre['meta']['native_thread_id'] or
                not (pre['meta']['qpc'] <= q <= end['meta']['qpc'] and q <= work_cut and q <= parent_cut)):
            return 'WORK_EVENTS_OUTSIDE_EXACT_CALL0_OR_BOTH_CUTS'
    return None


def _evaluate(work, joined, issues, contexts):
    if issues: return [], [], [], []
    parents = {}
    for row in joined['virtual_analysis']['completed_invocations']:
        pid = _id(row['frame']['entry']['invocation'])
        need(pid not in parents, 'unique completed virtual inventory'); parents[pid] = row
    bodies, own_ids, extras = {}, set(), []
    for row in work['completed_invocations']:
        own, pid = _id(row['entry']['invocation']), _id(row['entry']['virtual_entry']['invocation'])
        need(own not in own_ids, 'unique complete work invocation'); own_ids.add(own)
        bodies.setdefault(pid, []).append(row)
        if pid not in parents or not _applicable(parents[pid]): extras.append(copy.deepcopy(row))
    if extras: issues.add('ORPHAN_WORK_BODY_WITHOUT_APPLICABLE_VIRTUAL_CALL0')
    if any(len(rows) != 1 for rows in bodies.values()): issues.add('DUPLICATE_WORK_BODY_FOR_VIRTUAL_CALL0')
    if issues: return [], [], extras, []
    work_cut = work['work_emission_inventory']['freeze_qpc']
    parent_cut = joined['virtual_analysis']['virtual_detail_inventory']['freeze_qpc']
    for pid, rows in bodies.items():
        reason = _relation(rows[0], parents[pid], work_cut, parent_cut)
        if reason: issues.add(reason)
    if issues: return [], [], extras, []
    barrier = joined['frame_consumer_analysis']['frame_router_analysis']['timing_frontier_analysis'][
        'closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive']
    accepted, excluded, inventory = [], [], []
    for source in joined['candidate_records']:
        pids = [_id(i) for i in source['virtual_detail_invocations']]
        need(len(pids) == len(set(pids)) and all(i in parents for i in pids), 'unique complete selected virtual inventory')
        expected = [i for i in pids if _applicable(parents[i])]
        unknown = [copy.deepcopy(parents[i]['frame']['entry']['invocation']) for i in pids if i not in expected]
        chosen, event_groups, reason = [], [], None
        for pid in expected:
            rows = bodies.get(pid, [])
            if len(rows) != 1:
                reason = 'MISSING_WORK_BODY_FOR_APPLICABLE_VIRTUAL_CALL0'; break
            parent = parents[pid]; body = rows[0]
            # A later parent normal exit must not rescue a prefix still pending
            # at the independently observed work cut, even if call0 ended first.
            # Equal ticks cannot order an independently frozen parent journal.
            if parent['events'][-1]['meta']['qpc'] >= work_cut:
                reason = 'PARENT_VIRTUAL_COMPLETION_AT_OR_AFTER_WORK_CUT'; break
            if any(e['meta']['qpc'] >= barrier for e in body['events']):
                reason = 'WORK_EVENTS_TOUCH_OR_CROSS_SOURCE_CLOSED_FRONTIER'; break
            chosen.append(copy.deepcopy(body['entry']['invocation']))
            event_groups.append(dict(invocation=copy.deepcopy(body['entry']['invocation']),
                virtual_invocation=copy.deepcopy(body['entry']['virtual_entry']['invocation']),
                entry=copy.deepcopy(body['entry']), frame=copy.deepcopy(body['frame']),
                events=copy.deepcopy(body['events'])))
        inv = dict(source_candidate=copy.deepcopy(source),
            expected_virtual_call0_invocations=[copy.deepcopy(parents[i]['frame']['entry']['invocation']) for i in expected],
            unknown_virtual_call0_invocations=unknown, selected_work_invocations=chosen,
            compact_work_event_groups=event_groups, exclusion_reason=reason,
            interpretation='RAW_EXPLICIT_ID_INVENTORY_NOT_TOTAL_WORK_OR_TASK_COMPLETION')
        inventory.append(inv)
        if reason:
            contexts.add(reason)
            excluded.append(dict(source_candidate=copy.deepcopy(source), reason=reason,
                expected_work_body_count=len(expected)))
            continue
        if unknown: contexts.add('OTHER_VIRTUAL_CALL0_TARGETS_RETAINED_UNKNOWN')
        if not expected: contexts.add('NO_APPLICABLE_WORK_BODY_IN_SELECTED_GROUP')
        record = copy.deepcopy(source)
        record.update(work_emission_invocations=chosen, work_emission_group_count=len(chosen),
            expected_work_body_count=len(expected), unknown_virtual_call0_invocations=unknown,
            compact_work_event_groups=event_groups,
            interpretation='EXACT_VIRTUAL_CALL0_IDS_AND_RAW_OPERANDS_NOT_TOTAL_WORK_SIMULATION_OR_TASK_COMPLETION')
        accepted.append(record)
    if not accepted: contexts.add('NO_COMPLETE_SOURCE_WORK_GROUP')
    return accepted, excluded, extras, inventory


def validate_work_emission_join(work_raw, virtual_raw, consumer_raw, router_raw, timing_raw,
                               handoff_raw, prefix_raw, phase_raw, swap_raw, source_receipt, token):
    """Raw immutable input only; validate all inventories before any join."""
    work = work_decoder.validate_work_emission(work_raw, token)
    joined = virtual_join.validate_frame_virtual_join(virtual_raw, consumer_raw, router_raw,
        timing_raw, handoff_raw, prefix_raw, phase_raw, swap_raw, copy.deepcopy(source_receipt), token)
    w = work['work_emission_inventory']; v = joined['virtual_analysis']['virtual_detail_inventory']
    for field in ('token', 'clock', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency'):
        need(w[field] == v[field], 'identical work/virtual session window and QPF')
    if not w['clock_failed'] and not v['clock_failed']:
        need(w['freeze_qpc'] <= v['freeze_qpc'], 'independent work cut precedes shared virtual/consumer cut')
    parent_lanes = {l['thread_epoch']: l for l in v['threads']}
    for lane in w['threads']:
        epoch = lane['parent_virtual_thread_epoch']
        if epoch:
            need(epoch in parent_lanes, 'work lane names exact virtual lifetime')
            p = parent_lanes[epoch]
            need(lane['native_thread_id'] == p['native_thread_id'] and
                 lane['parent_detail_thread_epoch'] == p['parent_detail_thread_epoch'] and
                 lane['parent_router_thread_epoch'] == p['parent_router_thread_epoch'],
                 'separate work namespace matches exact parent lane and ancestry lifetimes')
    issues = {'work:'+s for s in work['candidate_blocking_issues']}
    issues.update('virtual_join:'+s for s in joined['candidate_blocking_issues'])
    contexts = {'work:'+s for s in work['retained_context_issues']}
    contexts.update('virtual_join:'+s for s in joined['retained_context_issues'])
    candidates, excluded, extras, inventory = _evaluate(work, joined, issues, contexts)
    hashes = copy.deepcopy(joined['raw_sha256']); hashes['work'] = hashlib.sha256(work_raw).hexdigest()
    result = dict.fromkeys(FALSE_FLAGS, False)
    result.update(schema='mcla-work-emission-join-analysis-v1', token=token,
        status='WORK_EMISSION_JOIN_DIAGNOSTIC_INCONCLUSIVE' if issues or contexts or not candidates else
               'WORK_EMISSION_JOIN_CANDIDATES_REVIEW_PENDING', raw_sha256=hashes,
        work_analysis=copy.deepcopy(work), frame_virtual_analysis=copy.deepcopy(joined),
        candidate_records=candidates, excluded_candidate_records=excluded, extraneous_work_records=extras,
        source_work_inventory=inventory, candidate_blocking_issues=sorted(issues),
        retained_context_issues=sorted(contexts), prelude_unobserved=True,
        physical_acceptance_pending=True,
        interpretation='EXPLICIT_IDS_ONLY_NO_NEAREST_TIME_NO_BACKFILL_NO_F1_SUM_NO_TOTAL_WORK_PROOF')
    return result
