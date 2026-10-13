"""Pure explicit-ID source/consumer/downstream-body join; no IO or proof.

Both raw validator chains finish before evaluating any relation. Clock bounds
only reject an identity-selected relation, never select or replace a body.
"""
import copy
import hashlib

import mcla_virtual_detail_decode as virtual_decoder
import mcla_frame_consumer_join as consumer_join

need = consumer_join.need
TARGET = 0x8258D1A0
CALLER = 0x822C78C0
FALSE_FLAGS = consumer_join.FALSE_FLAGS | virtual_decoder.FALSE_FLAGS | {
    'source_virtual_join_proven', 'virtual_consumer_association_proven',
    'virtual_target_resolution_proven', 'consumer_internal_semantics_proven'}


def _id(value):
    return value['thread_epoch'], value['serial']


def _applicable(parent):
    f = parent['frame']
    c = f['children'][0]
    return f['branch_nonzero'] and c['indirect'] and c['requested_target'] == TARGET


def _bounds(parent):
    pre = [e for e in parent['events'] if e['event'] == 2 and e['frame']['returned_children'] == 0]
    end = [e for e in parent['events'] if e['event'] == 3 and e['frame']['returned_children'] == 1]
    need(len(pre) == len(end) == 1, 'one exact parent child0 pre/return pair')
    return pre[0], end[0]


def _mismatch(body, parent, cut, barrier):
    en = body['frame']['entry']
    pre, end = _bounds(parent)
    child = pre['frame']['children'][0]
    if (not body['normal_return'] or not parent['normal_return'] or
            en['parent_entry'] != parent['frame']['entry'] or
            en['parent_child'] != child or en['actual_target'] != TARGET or
            en['caller_lr'] != CALLER or en['r3'] != child['r3'] or
            en['f1_bits'] != child['f1_bits']):
        return 'VIRTUAL_PARENT_IDENTITY_OR_SAVED_OPERAND_MISMATCH'
    for event in body['events']:
        qpc = event['meta']['qpc']
        if not (pre['meta']['qpc'] <= qpc <= end['meta']['qpc'] and qpc <= cut and qpc < barrier):
            return 'VIRTUAL_EVENTS_OUTSIDE_PARENT_CHILD_CUT_OR_FRONTIER'
    return None


def _evaluate(virtual, joined, issues, contexts):
    if issues:
        return [], [], []
    parents = {}
    for row in joined['consumer_analysis']['completed_invocations']:
        pid = _id(row['frame']['entry']['invocation'])
        need(pid not in parents, 'unique complete consumer parent inventory')
        parents[pid] = row
    bodies, own_ids, extras = {}, set(), []
    for row in virtual['completed_invocations']:
        en = row['frame']['entry']; own = _id(en['invocation']); pid = _id(en['parent_entry']['invocation'])
        need(own not in own_ids, 'unique complete virtual invocation inventory')
        own_ids.add(own)
        bodies.setdefault(pid, []).append(row)
        if pid not in parents or not _applicable(parents[pid]):
            extras.append(copy.deepcopy(row))
    if extras:
        issues.add('ORPHAN_VIRTUAL_BODY_WITHOUT_APPLICABLE_PARENT_CHILD')
    if any(len(rows) != 1 for rows in bodies.values()):
        issues.add('DUPLICATE_VIRTUAL_BODY_FOR_PARENT_CHILD')
    if issues:
        return [], [], extras
    cut = virtual['virtual_detail_inventory']['freeze_qpc']
    frontier = joined['frame_router_analysis']['timing_frontier_analysis']['closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive']
    selected = {_id(i) for s in joined['candidate_records'] for i in s['consumer_detail_invocations']}
    for pid, rows in bodies.items():
        if pid not in selected and _mismatch(rows[0], parents[pid], cut, virtual_decoder.QMAX + 1):
            issues.add('UNSELECTED_VIRTUAL_PARENT_RELATION_INVALID')
    if issues:
        return [], [], extras
    accepted, excluded = [], []
    for source in joined['candidate_records']:
        pids = [_id(i) for i in source['consumer_detail_invocations']]
        need(len(pids) == len(set(pids)) and all(i in parents for i in pids), 'complete unique selected consumer inventory')
        expected = [i for i in pids if _applicable(parents[i])]
        unknown = [copy.deepcopy(parents[i]['frame']['entry']['invocation']) for i in pids
                   if parents[i]['frame']['branch_nonzero'] and i not in expected]
        zero = [copy.deepcopy(parents[i]['frame']['entry']['invocation']) for i in pids
                if not parents[i]['frame']['branch_nonzero']]
        chosen = []; reason = None
        for pid in expected:
            rows = bodies.get(pid, [])
            if len(rows) != 1:
                reason = 'MISSING_VIRTUAL_BODY_FOR_APPLICABLE_PARENT_CHILD'; break
            reason = _mismatch(rows[0], parents[pid], cut, frontier)
            if reason:
                break
            chosen.append(copy.deepcopy(rows[0]['frame']['entry']['invocation']))
        if reason:
            contexts.add(reason)
            excluded.append({'source_candidate': copy.deepcopy(source), 'reason': reason,
                             'expected_virtual_body_count': len(expected)})
            continue
        if unknown:
            contexts.add('OTHER_CHILD0_TARGETS_RETAINED_UNKNOWN')
        if not expected:
            contexts.add('NO_APPLICABLE_VIRTUAL_BODY_IN_SELECTED_GROUP')
        record = copy.deepcopy(source)
        record.update(virtual_detail_invocations=chosen, virtual_detail_group_count=len(chosen),
                      expected_virtual_body_count=len(expected), unknown_child0_parent_invocations=unknown,
                      zero_route_parent_invocations=zero,
                      interpretation='EXACT_PARENT_CHILD_IDS_AND_CONTAINMENT_NOT_RESOLVED_TARGET_OR_SIMULATION_PROOF')
        accepted.append(record)
    if not accepted:
        contexts.add('NO_COMPLETE_SOURCE_VIRTUAL_GROUP')
    return accepted, excluded, extras


def validate_frame_virtual_join(virtual_raw, consumer_raw, router_raw, timing_raw, handoff_raw,
                                prefix_raw, phase_raw, swap_raw, source_receipt, token):
    """Validate immutable raw bytes; return detached analyses and explicit IDs."""
    virtual = virtual_decoder.validate_virtual_detail(virtual_raw, token)
    joined = consumer_join.validate_frame_consumer_join(consumer_raw, router_raw, timing_raw,
        handoff_raw, prefix_raw, phase_raw, swap_raw, copy.deepcopy(source_receipt), token)
    v = virtual['virtual_detail_inventory']; c = joined['consumer_analysis']['consumer_detail_inventory']
    for field in ('token', 'clock', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc'):
        need(v[field] == c[field], 'identical virtual/consumer clock, window and shared cut')
    parent_lanes = {l['thread_epoch']: l for l in c['threads']}
    need(len(v['threads']) == len(parent_lanes), 'complete shared consumer lane inventory')
    for lane in v['threads']:
        need(lane['thread_epoch'] in parent_lanes and
             lane['native_thread_id'] == parent_lanes[lane['thread_epoch']]['native_thread_id'],
             'every shared lane including empty lanes has exact identity')
        epoch = lane['parent_detail_thread_epoch']
        if epoch:
            need(epoch in parent_lanes, 'virtual lane has exact consumer lifetime')
            parent = parent_lanes[epoch]
            need(lane['native_thread_id'] == parent['native_thread_id'] and
                 lane['parent_router_thread_epoch'] == parent['router_thread_epoch'],
                 'virtual lane matches parent native thread and router lifetime')
    issues = {'virtual:'+s for s in virtual['candidate_blocking_issues']}
    issues.update('consumer_join:'+s for s in joined['candidate_blocking_issues'])
    contexts = {'consumer_join:'+s for s in joined['retained_context_issues']}
    candidates, excluded, extras = _evaluate(virtual, joined, issues, contexts)
    hashes = copy.deepcopy(joined['raw_sha256']); hashes['virtual'] = hashlib.sha256(virtual_raw).hexdigest()
    result = dict.fromkeys(FALSE_FLAGS, False)
    result.update(schema='mcla-frame-virtual-join-analysis-v1', token=token,
        status='FRAME_VIRTUAL_JOIN_DIAGNOSTIC_INCONCLUSIVE' if issues or contexts or not candidates else
               'FRAME_VIRTUAL_JOIN_CANDIDATES_REVIEW_PENDING', raw_sha256=hashes,
        virtual_analysis=copy.deepcopy(virtual), frame_consumer_analysis=copy.deepcopy(joined),
        candidate_records=candidates, excluded_candidate_records=excluded, extraneous_virtual_records=extras,
        candidate_blocking_issues=sorted(issues), retained_context_issues=sorted(contexts),
        physical_acceptance_pending=True,
        interpretation='EXPLICIT_IDS_ONLY_NO_NEAREST_TIME_NO_F1_SUM_NO_RESOLUTION_OR_SIMULATION_PROOF')
    return result
