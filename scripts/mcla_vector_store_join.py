"""Pure exact-ID source/target-slot28/actual-store association, never time search.

Immutable validators finish before relation evaluation. The parent event index
is lane-global, not an offset into a completed invocation's detached event list.
"""
import copy
import hashlib

import mcla_vector_store_decode as store_decoder
import mcla_vector_target_join as target_join

need = store_decoder.need
FALSE_FLAGS = store_decoder.FALSE_FLAGS | target_join.FALSE_FLAGS | {
    'actual_target_execution_proven', 'work_target_association_proven',
    'source_target_join_proven', 'total_work_inventory_proven',
    'actual_execution_association_proven', 'frame_to_step_mapping_proven',
    'source_store_join_proven', 'target_store_association_proven', 'arithmetic_proven', 'state_units_proven'}


def _id(value):
    return value['thread_epoch'], value['serial']


def _pairs(target):
    """Index every complete parent using positions in its original lane."""
    raw_groups = {}
    for lane in target['vector_target_inventory']['threads']:
        for index, event in enumerate(lane['events']):
            raw_groups.setdefault(_id(event['invocation']), []).append((index, event))
    parents, pairs = {}, {}
    for row in target['completed_invocations']:
        pid = _id(row['entry']['invocation'])
        need(pid not in parents, 'unique completed target identity')
        parents[pid] = row
        positioned = raw_groups.get(pid, [])
        need([e for _, e in positioned] == row['events'], 'exact original lane event group')
        active = None
        for index, event in positioned:
            if event['event'] == 2:
                need(active is None, 'one active inner call per target body')
                active = (index, event)
            elif event['event'] in (3, 4):
                need(active is not None, 'inner termination needs exact active pre')
                need(event['site'] == active[1]['site'], 'exact matching inner-call termination site')
                if event['event'] == 3:
                    key = (pid, active[0])
                    need(key not in pairs, 'unique original inner call pre index')
                    pairs[key] = dict(parent=row, pre=active[1], end=event)
                active = None
    return parents, pairs


def _expected(pair):
    return (pair['pre']['site'] == 2 and
            pair['pre']['fields'][2] & 0xffffffff == store_decoder.TARGET)


def _relation(body, pair, store_cut, target_cut):
    entry = body['entry']; parent, pre, end = (pair[k] for k in ('parent', 'pre', 'end'))
    payload = dict(fields=pre['fields'], vector=pre['vector'])
    if (not body['normal_return'] or not parent['normal_return'] or
            entry['target_entry'] != parent['entry'] or entry['slot28_pre'] != payload or
            entry['actual_target'] != store_decoder.TARGET or
            entry['caller_lr'] != store_decoder.CALLER or
            pre['fields'][0] != store_decoder.CALLER or
            entry['r3'] != pre['fields'][1] or entry['v1'] != pre['vector']):
        return 'STORE_TARGET_IDENTITY_OR_SAVED_OPERAND_MISMATCH'
    for event in body['events']:
        qpc = event['meta']['qpc']
        if (event['meta']['native_thread_id'] != pre['meta']['native_thread_id'] or
                not (pre['meta']['qpc'] <= qpc <= end['meta']['qpc'] and
                     qpc <= store_cut and qpc <= target_cut)):
            return 'STORE_EVENTS_OUTSIDE_EXACT_SLOT28_PAIR_OR_BOTH_CUTS'
    return None


def _evaluate(store, joined, issues, contexts):
    if issues:
        return [], [], [], []
    target = joined['vector_target_analysis']; parents, pairs = _pairs(target)
    bodies, own_ids, extras = {}, set(), []
    for row in store['completed_invocations']:
        entry = row['entry']; own = _id(entry['invocation'])
        need(own not in own_ids, 'unique completed store invocation'); own_ids.add(own)
        key = (_id(entry['target_entry']['invocation']), entry['parent_event_index'])
        bodies.setdefault(key, []).append(row)
        if key not in pairs or not _expected(pairs[key]):
            extras.append(copy.deepcopy(row))
    if extras:
        issues.add('ORPHAN_STORE_WITHOUT_EXACT_APPLICABLE_SLOT28_PRE')
    if any(len(rows) != 1 for rows in bodies.values()):
        issues.add('DUPLICATE_STORE_FOR_TARGET_SLOT28_PRE')
    if issues:
        return [], [], extras, []
    store_cut = store['vector_store_inventory']['freeze_qpc']
    target_cut = target['vector_target_inventory']['freeze_qpc']
    # Contradictory unselected rows must not be hidden by source selection.
    for key, rows in bodies.items():
        reason = _relation(rows[0], pairs[key], store_cut, target_cut)
        if reason:
            issues.add(reason)
    if issues:
        return [], [], extras, []
    barrier = joined['frame_work_analysis']['frame_virtual_analysis']['frame_consumer_analysis'][
        'frame_router_analysis']['timing_frontier_analysis']['closed_handoff_analysis'][
        'closed_prefix_barrier_qpc_exclusive']
    accepted, excluded, inventory = [], [], []
    for source in joined['candidate_records']:
        pids = [_id(i) for i in source['vector_target_invocations']]
        need(len(pids) == len(set(pids)) and all(pid in parents for pid in pids),
             'unique complete selected target inventory')
        selected_pairs = [(key, pair) for key, pair in pairs.items() if key[0] in pids]
        expected = [(key, pair) for key, pair in selected_pairs if _expected(pair)]
        unknown = [dict(target_invocation=copy.deepcopy(pair['parent']['entry']['invocation']),
                        parent_event_index=key[1], call_pre=copy.deepcopy(pair['pre']),
                        call_return=copy.deepcopy(pair['end']))
                   for key, pair in selected_pairs if not _expected(pair)]
        chosen, groups, reason = [], [], None
        for key, pair in expected:
            rows = bodies.get(key, [])
            if len(rows) != 1:
                reason = 'MISSING_STORE_FOR_APPLICABLE_TARGET_SLOT28_PRE'; break
            body = rows[0]
            # An independently later target completion cannot backfill the cut.
            if pair['parent']['events'][-1]['meta']['qpc'] >= store_cut:
                reason = 'PARENT_TARGET_COMPLETION_AT_OR_AFTER_STORE_CUT'; break
            if any(event['meta']['qpc'] >= barrier for event in body['events']):
                reason = 'STORE_EVENTS_TOUCH_OR_CROSS_SOURCE_CLOSED_FRONTIER'; break
            chosen.append(copy.deepcopy(body['entry']['invocation']))
            groups.append(dict(invocation=copy.deepcopy(body['entry']['invocation']),
                target_invocation=copy.deepcopy(body['entry']['target_entry']['invocation']),
                parent_event_index=key[1],
                slot28_pre=copy.deepcopy(pair['pre']), slot28_return=copy.deepcopy(pair['end']),
                entry=copy.deepcopy(body['entry']), frame=copy.deepcopy(body['frame']),
                events=copy.deepcopy(body['events'])))
        expected_refs = [dict(target_invocation=copy.deepcopy(pair['parent']['entry']['invocation']),
                             parent_event_index=key[1])
                         for key, pair in expected]
        inventory.append(dict(source_candidate=copy.deepcopy(source),
            expected_target_slot28_calls=expected_refs, unknown_target_calls=unknown,
            selected_store_invocations=chosen, compact_store_event_groups=groups,
            exclusion_reason=reason,
            interpretation='RAW_EXPLICIT_ID_INVENTORY_NOT_TOTAL_WORK_OR_SIMULATION'))
        if reason:
            contexts.add(reason)
            excluded.append(dict(source_candidate=copy.deepcopy(source), reason=reason,
                                 expected_store_count=len(expected)))
            continue
        if unknown:
            contexts.add('OTHER_TARGET_INNER_CALLS_RETAINED_UNKNOWN')
        if not expected:
            contexts.add('NO_APPLICABLE_STORE_IN_SELECTED_TARGET_GROUP')
        record = copy.deepcopy(source)
        record.update(vector_store_invocations=chosen, vector_store_group_count=len(chosen),
            expected_store_count=len(expected), unknown_target_calls=unknown,
            compact_store_event_groups=groups,
            interpretation='EXACT_TARGET_SLOT28_IDS_AND_RAW_STORE_OPERANDS_NOT_PHYSICAL_OR_TOTAL_WORK')
        accepted.append(record)
    if not accepted:
        contexts.add('NO_COMPLETE_SOURCE_STORE_GROUP')
    return accepted, excluded, extras, inventory


def validate_vector_store_join(store_raw, target_raw, work_raw, virtual_raw, consumer_raw,
                               router_raw, timing_raw, handoff_raw, prefix_raw, phase_raw,
                               swap_raw, source_receipt, token):
    """Validate all immutable raw inputs before evaluating exact relations."""
    store = store_decoder.validate_vector_store(store_raw, token)
    joined = target_join.validate_vector_target_join(target_raw, work_raw, virtual_raw,
        consumer_raw, router_raw, timing_raw, handoff_raw, prefix_raw, phase_raw, swap_raw,
        copy.deepcopy(source_receipt), token)
    s = store['vector_store_inventory']; t = joined['vector_target_analysis']['vector_target_inventory']
    for field in ('token', 'clock', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency'):
        need(s[field] == t[field], 'identical store/target session window and QPF')
    if not s['clock_failed'] and not t['clock_failed']:
        need(s['freeze_qpc'] <= t['freeze_qpc'], 'independent store cut precedes target cut')
    parents = {lane['thread_epoch']: lane for lane in t['threads']}
    for lane in s['threads']:
        epoch = lane['parent_target_thread_epoch']
        if epoch:
            need(epoch in parents and lane['native_thread_id'] == parents[epoch]['native_thread_id'],
                 'exact store-to-target lifetime and native thread')
    issues = {'store:'+issue for issue in store['candidate_blocking_issues']}
    issues.update('target_join:'+issue for issue in joined['candidate_blocking_issues'])
    contexts = {'store:'+issue for issue in store.get('retained_context_issues', [])}
    contexts.update('target_join:'+issue for issue in joined['retained_context_issues'])
    candidates, excluded, extras, inventory = _evaluate(store, joined, issues, contexts)
    hashes = copy.deepcopy(joined['raw_sha256']); hashes['vector_store'] = hashlib.sha256(store_raw).hexdigest()
    result = dict.fromkeys(FALSE_FLAGS, False)
    result.update(schema='mcla-vector-store-join-analysis-v1', token=token,
        status='VECTOR_STORE_JOIN_DIAGNOSTIC_INCONCLUSIVE' if issues or contexts or not candidates else
               'VECTOR_STORE_JOIN_CANDIDATES_REVIEW_PENDING', raw_sha256=hashes,
        vector_store_analysis=copy.deepcopy(store), frame_target_analysis=copy.deepcopy(joined),
        candidate_records=candidates, excluded_candidate_records=excluded,
        extraneous_store_records=extras, source_store_inventory=inventory,
        candidate_blocking_issues=sorted(issues), retained_context_issues=sorted(contexts),
        physical_acceptance_pending=True, prelude_unobserved=True,
        interpretation='EXPLICIT_IDS_NO_NEAREST_TIME_NO_BACKFILL_NO_ARITHMETIC_OR_PHYSICAL_PROOF')
    return result
