"""Strict pure replay of the shared-cut downstream virtual-body journal.

Opaque saved operands and explicit IDs only: no IO, clocks, target resolution,
external parent join, timing sums, execution association or simulation proof.
Consumer-owned empty lanes are inventory, not fabricated virtual admissions.
"""
import copy
import hashlib
import re

import mcla_consumer_detail_decode as parent
from mcla_phase_source_join import _parse

need, exact, uint = parent.need, parent.exact, parent.uint
MAX_BYTES, U32, QMAX = parent.MAX_BYTES, parent.U32, parent.QMAX
TARGET, CALLER = 0x8258D1A0, 0x822C78C0
CALL_LRS = (0x8258D1E8, 0x8258D1FC, 0x8258D214)
FALSE_FLAGS = (parent.FALSE_FLAGS - {'resolved_child_target_proven'}) | {'resolved_virtual_target_proven'}
HEALTH, COUNTS = parent.HEALTH, parent.COUNTS
LIMITATIONS = {'binding_observed_before_event_qpc', 'unobserved_ancestry_possible',
              'shared_consumer_clock_and_cut', 'consumer_lane_inventory'}
TOP = (parent.TOP - parent.FALSE_FLAGS) | FALSE_FLAGS | LIMITATIONS
ENTRY = {'invocation', 'parent_entry', 'parent_child', 'actual_target', 'caller_lr', 'r3', 'f1_bits'}
ACCESS = {'status', 'entry', 'child', 'child_index'}
META = (parent.META - {'observed_consumer'}) | {'observed_child'}
CALL = {'caller_lr', 'r3', 'f1_bits', 'indirect_ctr', 'requested_target', 'state'}
FRAME = {'entry', 'model_parent', 'stage', 'returned_calls', 'calls'}
LANE = (parent.LANE - {'router_thread_epoch'}) | {'parent_detail_thread_epoch', 'parent_router_thread_epoch'}


def _zero_parent():
    return dict(invocation={'thread_epoch': 0, 'serial': 0}, router={'thread_epoch': 0, 'serial': 0},
        update={'phase_call_id': 0, 'thread_epoch': 0, 'native_thread_id': 0},
        actual_target=0, caller_lr=0, r3=0, f1_bits=0)


def _zero_call():
    return dict(caller_lr=0, r3=0, f1_bits=0, indirect_ctr=0, requested_target=0, state=0)


def _child(c):
    exact(c, parent.CHILD, 'exact parent child scalar')
    for k in ('caller_lr', 'r3', 'f1_bits', 'indirect_ctr'): uint(c[k])
    uint(c['requested_target'], U32); uint(c['state'], 3); parent._bool(c['indirect'])
    if c['state'] == 0: need(c == parent._zero_child(), 'absent parent child is zero')


def _child_bound(c):
    return (c['state'] == 1 and c['indirect'] and c['caller_lr'] == CALLER and
            c['requested_target'] == TARGET and c['indirect_ctr'] & U32 == TARGET)


def _entry(e, admitted=False):
    exact(e, ENTRY, 'exact virtual entry')
    parent._id(e['invocation'], admitted); parent._entry(e['parent_entry'], admitted)
    _child(e['parent_child']); uint(e['actual_target'], U32)
    for k in ('caller_lr', 'r3', 'f1_bits'): uint(e[k])
    if admitted:
        need(_child_bound(e['parent_child']) and e['actual_target'] == TARGET and e['caller_lr'] == CALLER and
             all(e[k] == e['parent_child'][k] for k in ('r3', 'f1_bits')), 'exact active child0 entry operands')


def _access(a):
    exact(a, ACCESS, 'exact child accessor')
    uint(a['status'], 8); parent._entry(a['entry']); _child(a['child']); uint(a['child_index'], 255)
    if a['status'] != 8:
        need(a['entry'] == _zero_parent() and a['child'] == parent._zero_child() and a['child_index'] == 0,
             'invalid child accessor contains only status')


def _bound(a, tid):
    p = a['entry']
    return (a['status'] == 8 and a['child_index'] == 0 and all(parent._id(p['invocation'])) and
            all(parent._id(p['router'])) and all(parent._binding(p['update'])) and
            p['update']['native_thread_id'] == tid and p['actual_target'] == parent.TARGET and
            p['caller_lr'] == parent.CALLER and _child_bound(a['child']))


def _same(a, e, tid):
    return (_bound(a, tid) and a['entry'] == e['parent_entry'] and a['child'] == e['parent_child'])


def _meta(m):
    exact(m, META, 'exact virtual metadata'); uint(m['qpc']); uint(m['native_thread_id'], U32, 1)
    for k in parent.META_FLAGS: parent._bool(m[k])
    _access(m['observed_child'])


def _frame(f):
    exact(f, FRAME, 'exact virtual frame'); _entry(f['entry'], True); parent._id(f['model_parent'])
    uint(f['stage'], 3); uint(f['returned_calls'], 3)
    need(type(f['calls']) is list and len(f['calls']) == 3, 'exact three virtual slots')
    for i, c in enumerate(f['calls']):
        exact(c, CALL, 'exact virtual call')
        for k in ('caller_lr', 'r3', 'f1_bits', 'indirect_ctr'): uint(c[k])
        uint(c['requested_target'], U32); uint(c['state'], 3)
        if c['state'] == 0: need(c == _zero_call(), 'absent virtual call is zero')
        else:
            need(c['caller_lr'] == CALL_LRS[i] and c['requested_target'] == c['indirect_ctr'] & U32,
                 'exact virtual callsite and raw low32 target')


def _rejections(data):
    ids, last = set(), 0
    for r in data['rejected_entries']:
        exact(r, {'reason', 'entry', 'meta'}, 'exact virtual rejection')
        reason = uint(r['reason'], 5); e, m = r['entry'], r['meta']; _entry(e); _meta(m)
        last = parent._clock(m, data, last); a, tid = m['observed_child'], m['native_thread_id']
        need(e['parent_entry'] == a['entry'] and e['parent_child'] == a['child'], 'rejected parent snapshot retained')
        need(m['lexical_top'] and m['context_nonnull'], 'constructor lexical/context metadata')
        if reason != 5: need(parent._id(e['invocation']) == (0, 0), 'no fabricated pre-model ID')
        else:
            iid = parent._id(e['invocation'], True); need(iid not in ids, 'unique rejected allocation ID'); ids.add(iid)
        bound = _bound(a, tid)
        matches = (e['actual_target'] == TARGET and e['caller_lr'] == CALLER and
                   all(e[k] == a['child'][k] for k in ('r3', 'f1_bits')))
        if reason == 0:
            need(not m['clock_valid'] and not m['binding_matches'] and not m['lane_matches'] and
                 not m['parent_admitted'], 'clock rejection before admission')
        else:
            need(m['clock_valid'] and data['begin_qpc'] <= m['qpc'] < data['end_qpc_exclusive'], 'rejected window entry')
            if reason == 1:
                # Shared parent failure can reject an otherwise bound snapshot.
                need((not bound or not m['base_nonnull'] or data['observer_failed']) and
                     not m['binding_matches'] and not m['lane_matches'] and not m['parent_admitted'], 'accessor/shared-health rejection')
            else:
                need(bound and m['base_nonnull'], 'later rejection requires bound child/base')
                if reason == 2:
                    need(not matches and not m['binding_matches'] and not m['lane_matches'] and
                         not m['parent_admitted'], 'entry mismatch rejection')
                else:
                    need(matches and m['binding_matches'] and m['lane_matches'] is (reason >= 4), 'post-match rejection stage')
                    if reason == 3: need(not m['parent_admitted'], 'no sampled parent before valid lane')
    return ids


def _replay(lane, data, issues, parents, routers):
    stack, histories, lexical = [], {}, {}
    last_id = last_detail = last_router = last_qpc = 0
    entries = exceptions = mismatches = 0; completed, own_ids = [], set(); phase_epoch = None
    for event in lane['events']:
        k, m, f = event['event'], event['meta'], event['frame']; e = f['entry']; p = e['parent_entry']
        iid, pid, rid = parent._id(e['invocation'], True), parent._id(p['invocation'], True), parent._id(p['router'], True)
        tid = lane['native_thread_id']
        need(iid[0] == lane['thread_epoch'] and pid[0] == lane['parent_detail_thread_epoch'] == lane['thread_epoch'] and
             rid[0] == lane['parent_router_thread_epoch'], 'explicit shared consumer lane, separate identity namespaces')
        need(m['native_thread_id'] == tid and m['lane_matches'] and m['lexical_top'] and
             m['context_nonnull'] and m['base_nonnull'] and p['update']['native_thread_id'] == tid, 'admitted native lane/context/base')
        if phase_epoch is None: phase_epoch = p['update']['thread_epoch']
        need(p['update']['thread_epoch'] == phase_epoch, 'stable Update thread lifetime')
        last_qpc = parent._clock(m, data, last_qpc)
        matches = _same(m['observed_child'], e, tid)
        need(m['binding_matches'] is matches, 'full saved parent entry and child comparison')
        if k == 0:
            need(iid[1] > last_id and pid[1] > last_detail and rid[1] > last_router and
                 pid not in parents and rid not in routers and len(stack) < 8, 'strict unique own/parent serials and bounded depth')
            if not data['counts']['dropped_entries'] and not data['counts']['observation_errors']:
                need(iid[1] == last_id + 1, 'healthy allocation serial ordinal')
            need(matches and m['clock_valid'] and data['begin_qpc'] <= m['qpc'] < data['end_qpc_exclusive'], 'valid admitted entry')
            need(not m['parent_admitted'] or bool(stack), 'admitted lexical parent requires model stack')
            wanted = copy.deepcopy(f); wanted.update(model_parent=stack[-1]['entry']['invocation'] if stack else
                {'thread_epoch': 0, 'serial': 0}, stage=0, returned_calls=0, calls=[_zero_call() for _ in range(3)])
            need(f == wanted, 'exact initialized virtual frame')
            stack.append(copy.deepcopy(f)); histories[iid] = []; lexical[iid] = m['parent_admitted']
            last_id, last_detail, last_router = iid[1], pid[1], rid[1]
            entries += 1; own_ids.add(iid); parents.add(pid); routers.add(rid)
        else:
            need(stack and stack[-1]['entry']['invocation'] == e['invocation'], 'LIFO exact virtual identity')
            need(m['parent_admitted'] is lexical[iid], 'immutable lexical parent flag')
            mismatches += int(not matches)
            wanted = copy.deepcopy(stack[-1]); stage, call = wanted['stage'], wanted['returned_calls']
            if k == 1:
                need(stage in (0, 2) and call < 3 and f['calls'][call]['state'] == 1, 'ordered active virtual pre')
                wanted['calls'][call] = copy.deepcopy(f['calls'][call]); wanted['stage'] = 1
            elif k in (2, 3):
                need(stage == 1 and call < 3 and wanted['calls'][call]['state'] == 1, 'matching active virtual end')
                wanted['calls'][call]['state'] = 2 if k == 2 else 3; wanted['stage'] = 2 if k == 2 else 3
                if k == 2: wanted['returned_calls'] += 1
            elif k == 4: need(stage == 2 and call == 3, 'normal exit requires three returns')
            elif k == 5: exceptions += 1
            need(f == wanted, 'full detached transition snapshot exact'); stack[-1] = wanted
            if k in (4, 5):
                completed.append({'frame': copy.deepcopy(f), 'normal_return': k == 4,
                                  'events': histories[iid] + [copy.deepcopy(event)]}); stack.pop()
        histories[iid].append(copy.deepcopy(event))
    need(lane['pending'] == stack and lane['pending_count'] == len(stack), 'immutable exact pending stack')
    if stack: issues.add('PENDING_VIRTUAL_INVOCATIONS')
    if lane['model_fault']: issues.add('MODEL_FAULT')
    need(bool(lane['parent_detail_thread_epoch']) is bool(entries) and
         bool(lane['parent_router_thread_epoch']) is bool(entries), 'bound parent epochs only after successful entry')
    return (entries, exceptions, mismatches), completed, own_ids, phase_epoch


def _evaluate(completed, issues):
    if issues: return []
    return [{'invocation': copy.deepcopy(r['frame']['entry']['invocation']),
             'parent_detail_invocation': copy.deepcopy(r['frame']['entry']['parent_entry']['invocation']),
             'parent_router_invocation': copy.deepcopy(r['frame']['entry']['parent_entry']['router']),
             'update_binding': copy.deepcopy(r['frame']['entry']['parent_entry']['update']),
             'frame': copy.deepcopy(r['frame']), 'entry_qpc': r['events'][0]['meta']['qpc'],
             'exit_qpc': r['events'][-1]['meta']['qpc'],
             'interpretation': 'SCALAR_REPLAY_ONLY_NOT_PARENT_ASSOCIATION_RESOLVED_TARGET_OR_SIMULATION_PROOF'}
            for r in completed if r['normal_return']]


def validate_virtual_detail(value, token):
    """Validate immutable JSON bytes fully before candidate evaluation."""
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token) and token != '0'*32, 'nonzero session token')
    d = _parse(value, MAX_BYTES); exact(d, TOP, 'exact virtual detail schema')
    need(d['schema'] == 'mcla-virtual-detail-v1' and d['token'] == token and
         d['clock'] == 'win32-query-performance-counter', 'schema/session/clock')
    for k in FALSE_FLAGS: need(d[k] is False, 'all proof flags false')
    for k in HEALTH: parent._bool(d[k])
    for k in LIMITATIONS: need(d[k] is True, 'shared-cut and ancestry limitations retained')
    for k, n in (('thread_capacity', 8), ('events_per_thread_capacity', 16384), ('depth_capacity', 8), ('rejected_entry_capacity', 256)):
        need(uint(d[k]) == n, 'fixed native capacities')
    begin, end, freq = uint(d['begin_qpc'], QMAX, 1), uint(d['end_qpc_exclusive'], QMAX, 1), uint(d['qpc_frequency'], 10**10, 1)
    need(begin < end and end-begin <= 12*freq, 'bounded configured window'); uint(d['freeze_qpc'])
    need(not d['clock_failed'] or d['observer_failed'], 'sticky clock failure')
    need(begin <= d['freeze_qpc'] <= QMAX or d['clock_failed'], 'trusted shared cut or explicitly failed clock')
    c = d['counts']; exact(c, COUNTS, 'exact virtual counters')
    for v in c.values(): uint(v)
    lanes, rejected = d['threads'], d['rejected_entries']
    need(type(lanes) is list and len(lanes) <= 8 and type(rejected) is list and len(rejected) <= 256, 'bounded inventories')
    need(c['thread_lifetimes'] == len(lanes) and c['rejected_entries'] == len(rejected), 'stored inventory counts')
    for i, lane in enumerate(lanes):
        exact(lane, LANE, 'exact consumer-owned virtual lane')
        need(uint(lane['thread_epoch'], 8, 1) == i+1, 'allocation ordered consumer lane epochs')
        uint(lane['native_thread_id'], U32, 1); uint(lane['parent_detail_thread_epoch']); uint(lane['parent_router_thread_epoch']); uint(lane['model_fault'], 4)
        need(type(lane['events']) is list and uint(lane['event_count'], 16384) == len(lane['events']), 'bounded event inventory')
        need(type(lane['pending']) is list and uint(lane['pending_count'], 8) == len(lane['pending']), 'bounded pending inventory')
        if lane['model_fault'] == 3: need(lane['event_count'] == 16384, 'capacity fault requires full event storage')
        if lane['model_fault'] == 4: need(lane['pending_count'] == 8, 'depth fault requires full stack')
        for ev in lane['events']:
            exact(ev, {'event', 'meta', 'frame'}, 'exact virtual event'); uint(ev['event'], 5); _meta(ev['meta']); _frame(ev['frame'])
        for f in lane['pending']: _frame(f)
    rejected_ids = _rejections(d)
    issues = {k.upper() for k in HEALTH if d[k]}
    for k in ('dropped_entries', 'binding_mismatches', 'observation_errors', 'original_exceptions', 'rejection_overflow'):
        if c[k]: issues.add(k.upper())
    if c['dropped_entries'] or c['binding_mismatches'] or c['observation_errors'] or d['counter_saturated']:
        need(d['observer_failed'], 'virtual faults require shared observer failure')
    total, completed, ids, routers, parents, router_epochs, phase_epochs = [0, 0, 0], [], set(), set(), set(), set(), set()
    for lane in lanes:
        if lane['model_fault']: need(d['observer_failed'], 'model fault requires shared failure')
        if lane['parent_router_thread_epoch']:
            pe = lane['parent_router_thread_epoch']; need(pe not in router_epochs, 'parent router lifetime cannot alias lanes'); router_epochs.add(pe)
        n, rows, own, pe = _replay(lane, d, issues, parents, routers)
        if pe is not None: need(pe not in phase_epochs, 'Update lifetime cannot alias lanes'); phase_epochs.add(pe)
        need(not own & rejected_ids, 'rejected allocation cannot also be admitted')
        ids.update(own); completed.extend(rows); total = [a+b for a,b in zip(total,n)]
    last = {lane['thread_epoch']: max((i[1] for i in ids if i[0] == lane['thread_epoch']), default=0) for lane in lanes}
    for row in rejected:
        if row['reason'] not in (4, 5): continue
        p, m = row['entry']['parent_entry'], row['meta']; epoch = p['invocation']['thread_epoch']
        need(1 <= epoch <= len(lanes) and lanes[epoch-1]['native_thread_id'] == m['native_thread_id'], 'serial/model rejection owns consumer lane')
        if row['reason'] == 5:
            own_epoch, serial = parent._id(row['entry']['invocation'], True)
            need(own_epoch == epoch and lanes[epoch-1]['model_fault'] != 0 and serial > last[epoch], 'monotonic rejected allocation on failed lane')
            last[epoch] = serial
    if not d['counter_saturated']:
        need(c['observed_entries'] == c['admitted_entries']+c['outside_before_entries']+c['outside_after_entries']+c['dropped_entries'], 'entry partition')
        need(c['admitted_entries'] == total[0] and c['original_exceptions'] == total[1], 'admitted/exception counters equal stored events')
        need(c['dropped_entries'] == c['rejected_entries']+c['rejection_overflow'], 'rejection conservation')
        need(total[2] <= c['binding_mismatches'] and c['observation_errors'] >= c['dropped_entries']+c['binding_mismatches'], 'each virtual rejection/mismatch records an error')
    if c['rejection_overflow']: need(len(rejected) == 256, 'overflow requires full rejection inventory')
    if not total[0]: issues.add('EMPTY_VIRTUAL_INVENTORY')
    candidates = _evaluate(completed, issues)
    result = dict.fromkeys(FALSE_FLAGS | {'causal_join_proven', 'source_virtual_join_proven'}, False)
    result.update(schema='mcla-virtual-detail-analysis-v1', token=token,
        status='VIRTUAL_DETAIL_DIAGNOSTIC_INCONCLUSIVE' if issues else 'VIRTUAL_DETAIL_CANDIDATES_REVIEW_PENDING',
        raw_sha256=hashlib.sha256(value).hexdigest(), virtual_detail_inventory=copy.deepcopy(d),
        completed_invocations=copy.deepcopy(completed), candidate_records=candidates,
        candidate_blocking_issues=sorted(issues), pending_invocations=sum(l['pending_count'] for l in lanes),
        physical_acceptance_pending=True, interpretation='SHARED_CUT_SCALAR_REPLAY_NO_EXTERNAL_PARENT_JOIN_OR_EXECUTION_PROOF')
    return result
