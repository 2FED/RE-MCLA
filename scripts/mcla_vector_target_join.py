"""Pure exact-ID source/work-vector/actual-target association, never time search.

Immutable validators finish before relation evaluation. The parent event index
is lane-global, not an offset into a completed invocation's detached event list.
"""
import copy
import hashlib

import mcla_vector_target_decode as target_decoder
import mcla_work_emission_join as work_join

need = target_decoder.need
FALSE_FLAGS = target_decoder.FALSE_FLAGS | work_join.FALSE_FLAGS | {
    'actual_target_execution_proven', 'work_target_association_proven',
    'source_target_join_proven', 'total_work_inventory_proven',
    'actual_execution_association_proven', 'frame_to_step_mapping_proven'}


def _id(value):
    return value['thread_epoch'], value['serial']


def _pairs(work):
    """Index every complete parent using positions in its original lane."""
    raw_groups = {}
    for lane in work['work_emission_inventory']['threads']:
        for index, event in enumerate(lane['events']):
            raw_groups.setdefault(_id(event['invocation']), []).append((index, event))
    parents, pairs = {}, {}
    for row in work['completed_invocations']:
        pid = _id(row['entry']['invocation'])
        need(pid not in parents, 'unique completed work identity')
        parents[pid] = row
        positioned = raw_groups.get(pid, [])
        need([e for _, e in positioned] == row['events'], 'exact original lane event group')
        active = None
        for index, event in positioned:
            if event['event'] == 19:
                need(active is None, 'one active vector call per work body')
                active = (index, event)
            elif event['event'] in (20, 21):
                need(active is not None, 'vector termination needs exact active pre')
                if event['event'] == 20:
                    key = (pid, active[0])
                    need(key not in pairs, 'unique original vector pre index')
                    pairs[key] = dict(parent=row, pre=active[1], end=event)
                active = None
    return parents, pairs


def _expected(pair):
    return pair['pre']['fields'][4] & 0xffffffff == target_decoder.TARGET


def _relation(body, pair, target_cut, work_cut):
    entry = body['entry']; parent, pre, end = (pair[k] for k in ('parent', 'pre', 'end'))
    payload = dict(fields=pre['fields'], vector=pre['vector'])
    if (not body['normal_return'] or not parent['normal_return'] or
            entry['work_entry'] != parent['entry'] or entry['vector_pre'] != payload or
            entry['actual_target'] != target_decoder.TARGET or
            entry['caller_lr'] != target_decoder.CALLER or
            pre['fields'][0] != target_decoder.CALLER or
            entry['r3'] != pre['fields'][2] or entry['v1'] != pre['vector']):
        return 'TARGET_WORK_IDENTITY_OR_SAVED_OPERAND_MISMATCH'
    for event in body['events']:
        qpc = event['meta']['qpc']
        if (event['meta']['native_thread_id'] != pre['meta']['native_thread_id'] or
                not (pre['meta']['qpc'] <= qpc <= end['meta']['qpc'] and
                     qpc <= target_cut and qpc <= work_cut)):
            return 'TARGET_EVENTS_OUTSIDE_EXACT_VECTOR_PAIR_OR_BOTH_CUTS'
    return None


def _evaluate(target, joined, issues, contexts):
    if issues:
        return [], [], [], []
    work = joined['work_analysis']; parents, pairs = _pairs(work)
    bodies, own_ids, extras = {}, set(), []
    for row in target['completed_invocations']:
        entry = row['entry']; own = _id(entry['invocation'])
        need(own not in own_ids, 'unique completed target invocation'); own_ids.add(own)
        key = (_id(entry['work_entry']['invocation']), entry['parent_event_index'])
        bodies.setdefault(key, []).append(row)
        if key not in pairs or not _expected(pairs[key]):
            extras.append(copy.deepcopy(row))
    if extras:
        issues.add('ORPHAN_TARGET_WITHOUT_EXACT_APPLICABLE_VECTOR_PRE')
    if any(len(rows) != 1 for rows in bodies.values()):
        issues.add('DUPLICATE_TARGET_FOR_WORK_VECTOR_PRE')
    if issues:
        return [], [], extras, []
    target_cut = target['vector_target_inventory']['freeze_qpc']
    work_cut = work['work_emission_inventory']['freeze_qpc']
    # Contradictory unselected rows must not be hidden by source selection.
    for key, rows in bodies.items():
        reason = _relation(rows[0], pairs[key], target_cut, work_cut)
        if reason:
            issues.add(reason)
    if issues:
        return [], [], extras, []
    barrier = joined['frame_virtual_analysis']['frame_consumer_analysis'][
        'frame_router_analysis']['timing_frontier_analysis']['closed_handoff_analysis'][
        'closed_prefix_barrier_qpc_exclusive']
    accepted, excluded, inventory = [], [], []
    for source in joined['candidate_records']:
        pids = [_id(i) for i in source['work_emission_invocations']]
        need(len(pids) == len(set(pids)) and all(pid in parents for pid in pids),
             'unique complete selected work inventory')
        selected_pairs = [(key, pair) for key, pair in pairs.items() if key[0] in pids]
        expected = [(key, pair) for key, pair in selected_pairs if _expected(pair)]
        unknown = [dict(work_invocation=copy.deepcopy(pair['parent']['entry']['invocation']),
                        parent_event_index=key[1], vector_pre=copy.deepcopy(pair['pre']),
                        vector_return=copy.deepcopy(pair['end']))
                   for key, pair in selected_pairs if not _expected(pair)]
        chosen, groups, reason = [], [], None
        for key, pair in expected:
            rows = bodies.get(key, [])
            if len(rows) != 1:
                reason = 'MISSING_TARGET_FOR_APPLICABLE_WORK_VECTOR_PRE'; break
            body = rows[0]
            # An independently later work completion cannot backfill the cut.
            if pair['parent']['events'][-1]['meta']['qpc'] >= target_cut:
                reason = 'PARENT_WORK_COMPLETION_AT_OR_AFTER_TARGET_CUT'; break
            if any(event['meta']['qpc'] >= barrier for event in body['events']):
                reason = 'TARGET_EVENTS_TOUCH_OR_CROSS_SOURCE_CLOSED_FRONTIER'; break
            chosen.append(copy.deepcopy(body['entry']['invocation']))
            groups.append(dict(invocation=copy.deepcopy(body['entry']['invocation']),
                work_invocation=copy.deepcopy(body['entry']['work_entry']['invocation']),
                parent_event_index=key[1], vector_ordinal=pair['pre']['fields'][1],
                vector_pre=copy.deepcopy(pair['pre']), vector_return=copy.deepcopy(pair['end']),
                entry=copy.deepcopy(body['entry']), frame=copy.deepcopy(body['frame']),
                events=copy.deepcopy(body['events'])))
        expected_refs = [dict(work_invocation=copy.deepcopy(pair['parent']['entry']['invocation']),
                             parent_event_index=key[1], vector_ordinal=pair['pre']['fields'][1])
                         for key, pair in expected]
        inventory.append(dict(source_candidate=copy.deepcopy(source),
            expected_work_vector_calls=expected_refs, unknown_vector_calls=unknown,
            selected_target_invocations=chosen, compact_target_event_groups=groups,
            exclusion_reason=reason,
            interpretation='RAW_EXPLICIT_ID_INVENTORY_NOT_TOTAL_WORK_OR_SIMULATION'))
        if reason:
            contexts.add(reason)
            excluded.append(dict(source_candidate=copy.deepcopy(source), reason=reason,
                                 expected_target_count=len(expected)))
            continue
        if unknown:
            contexts.add('OTHER_VECTOR_TARGETS_RETAINED_UNKNOWN')
        if not expected:
            contexts.add('NO_APPLICABLE_TARGET_IN_SELECTED_WORK_GROUP')
        record = copy.deepcopy(source)
        record.update(vector_target_invocations=chosen, vector_target_group_count=len(chosen),
            expected_target_count=len(expected), unknown_vector_calls=unknown,
            compact_target_event_groups=groups,
            interpretation='EXACT_WORK_VECTOR_IDS_AND_RAW_OPERANDS_NOT_SIMULATION_OR_TOTAL_WORK')
        accepted.append(record)
    if not accepted:
        contexts.add('NO_COMPLETE_SOURCE_TARGET_GROUP')
    return accepted, excluded, extras, inventory


def validate_vector_target_join(target_raw, work_raw, virtual_raw, consumer_raw, router_raw,
                                timing_raw, handoff_raw, prefix_raw, phase_raw, swap_raw,
                                source_receipt, token):
    """Only immutable raw input; every validator finishes before this join."""
    target = target_decoder.validate_vector_target(target_raw, token)
    joined = work_join.validate_work_emission_join(work_raw, virtual_raw, consumer_raw,
        router_raw, timing_raw, handoff_raw, prefix_raw, phase_raw, swap_raw,
        copy.deepcopy(source_receipt), token)
    t = target['vector_target_inventory']; w = joined['work_analysis']['work_emission_inventory']
    for field in ('token', 'clock', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency'):
        need(t[field] == w[field], 'identical target/work session window and QPF')
    if not t['clock_failed'] and not w['clock_failed']:
        need(t['freeze_qpc'] <= w['freeze_qpc'], 'independent target cut precedes work cut')
    parents = {lane['thread_epoch']: lane for lane in w['threads']}
    for lane in t['threads']:
        epoch = lane['parent_work_thread_epoch']
        if epoch:
            need(epoch in parents and lane['native_thread_id'] == parents[epoch]['native_thread_id'],
                 'exact target-to-work lifetime and native thread')
    issues = {'target:'+issue for issue in target['candidate_blocking_issues']}
    issues.update('work_join:'+issue for issue in joined['candidate_blocking_issues'])
    contexts = {'target:'+issue for issue in target.get('retained_context_issues', [])}
    contexts.update('work_join:'+issue for issue in joined['retained_context_issues'])
    candidates, excluded, extras, inventory = _evaluate(target, joined, issues, contexts)
    hashes = copy.deepcopy(joined['raw_sha256']); hashes['vector_target'] = hashlib.sha256(target_raw).hexdigest()
    result = dict.fromkeys(FALSE_FLAGS, False)
    result.update(schema='mcla-vector-target-join-analysis-v1', token=token,
        status='VECTOR_TARGET_JOIN_DIAGNOSTIC_INCONCLUSIVE' if issues or contexts or not candidates else
               'VECTOR_TARGET_JOIN_CANDIDATES_REVIEW_PENDING', raw_sha256=hashes,
        vector_target_analysis=copy.deepcopy(target), frame_work_analysis=copy.deepcopy(joined),
        candidate_records=candidates, excluded_candidate_records=excluded,
        extraneous_target_records=extras, source_target_inventory=inventory,
        candidate_blocking_issues=sorted(issues), retained_context_issues=sorted(contexts),
        physical_acceptance_pending=True, prelude_unobserved=True,
        interpretation='EXPLICIT_IDS_NO_NEAREST_TIME_NO_BACKFILL_NO_F1_SUM_NO_TOTAL_WORK_PROOF')
    return result
