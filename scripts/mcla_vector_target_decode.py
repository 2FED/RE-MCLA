"""Pure strict actual-vector-target journal replay; no external parent join or IO."""
import copy
import hashlib
import re

import mcla_work_emission_decode as work
from mcla_phase_source_join import _parse

parent = work.parent
need, exact, uint = parent.need, parent.exact, parent.uint
U32, QMAX, MAX_BYTES = parent.U32, parent.QMAX, 128*1024*1024
TARGET, CALLER = 0x8259F300, 0x825886F4
CALLERS = [0x8259F334, 0x8259F34C, 0x8259F364, 0x8259F36C,
           0x8259F474, 0x8259F48C, 0x8259F4B4]
READY, AFTER = [1, 2, 3, 4, 7, 8, 11], [2, 3, 4, 5, 8, 9, 12]
FALSE_FLAGS = {'native_admitted', 'source_target_join_proven', 'resolved_target_proven',
    'task_completion_proven', 'simulation_proven', 'fps_proven', 'display_proven',
    'coverage_proven', 'causal_join_proven', 'work_semantics_proven', 'total_work_proven'}
HEALTH, COUNTS = parent.HEALTH, parent.COUNTS
LIMITATIONS = {'binding_observed_before_event_qpc', 'operands_observed_before_event_qpc',
              'independent_cut', 'unobserved_ancestry_possible'}
TOP = FALSE_FLAGS | HEALTH | LIMITATIONS | {'schema', 'token', 'clock', 'begin_qpc',
    'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc', 'thread_capacity',
    'entries_per_thread_capacity', 'events_per_thread_capacity', 'depth_capacity',
    'rejected_entry_capacity', 'json_limit_bytes', 'vector_lane_order', 'counts',
    'threads', 'rejected_entries'}
ENTRY = {'invocation', 'work_entry', 'vector_pre', 'parent_event_index',
         'actual_target', 'caller_lr', 'r3', 'v1'}
META_FLAGS = parent.META_FLAGS
META = META_FLAGS | {'qpc', 'native_thread_id', 'accessor_status'}
LANE = {'thread_epoch', 'native_thread_id', 'parent_work_thread_epoch', 'model_fault',
        'entry_count', 'event_count', 'pending_count', 'entries', 'events', 'pending'}
FRAME = {'entry_index', 'model_parent', 'stage', 'active', 'counter_loaded'}
EVENT = {'invocation', 'entry_index', 'event', 'site', 'fields', 'vector', 'meta'}


def _array(value, length, maximum):
    need(type(value) is list and len(value) == length, 'exact scalar array')
    for item in value: uint(item, maximum)


def _entry(e, admitted=False):
    exact(e, ENTRY, 'exact vector target entry'); parent._id(e['invocation'], admitted)
    work._entry(e['work_entry'], admitted)
    p = e['vector_pre']; exact(p, {'fields', 'vector'}, 'exact saved vector call')
    _array(p['fields'], 10, 2**64-1); _array(p['vector'], 4, U32); _array(e['v1'], 4, U32)
    uint(e['parent_event_index'], U32); uint(e['actual_target'], U32)
    uint(e['caller_lr']); uint(e['r3'])
    if admitted:
        need(e['actual_target'] == TARGET and e['caller_lr'] == CALLER and
             p['fields'][0] == CALLER and p['fields'][4] & U32 == TARGET and
             p['fields'][1] < 65535 and not any(p['fields'][5:]) and
             e['r3'] == p['fields'][2] and e['v1'] == p['vector'] and
             0 < e['parent_event_index'] < 65536, 'actual body and exact saved vector operands')


def _meta(m):
    exact(m, META, 'exact target metadata'); uint(m['qpc'])
    uint(m['native_thread_id'], U32, 1); uint(m['accessor_status'], 8)
    for key in META_FLAGS: parent._bool(m[key])


def _new_frame(index, model_parent):
    return dict(entry_index=index, model_parent=copy.deepcopy(model_parent), stage=0,
                active=0, counter_loaded=0)


def _step(f, ev, entry):
    k, site, a, v, stage = ev['event'], ev['site'], ev['fields'], ev['vector'], f['stage']
    used = {1: 1, 2: 3, 5: 2, 6: 1, 7: 2, 8: 3}.get(k, 0)
    need(not any(a[used:]) and (k in (1, 2) or v == [0]*4), 'unused payload is zero')
    if k not in (2, 3, 4): need(site == 0, 'site only meaningful for call events')
    address = entry['r3'] & U32
    if k == 1:
        need(stage == 0 and a[0] == ((address+400) & U32 & ~15), 'initial original vector load address')
        f['stage'] = 1
    elif k == 2:
        need(stage == READY[site] and a[0] == CALLERS[site] and a[1] == entry['r3'], 'exact original call order and arguments')
        if site in (0, 1, 2, 5): need(v == entry['v1'], 'original restored V1 lanes')
        f.update(active=site, stage=13)
    elif k in (3, 4):
        need(stage == 13 and f['active'] == site, 'return or exception requires same active call')
        f['stage'] = 14 if k == 4 else AFTER[site]
    elif k == 5:
        need(stage == 5 and a[0] == (address+657) & U32 and a[1] <= 1, 'original flag store')
        f['stage'] = 6
    elif k == 6:
        need(stage == 6 and a[0] in (0, 16), 'exact masked slot100 gate')
        f['stage'] = 8 if a[0] else 7
    elif k == 7:
        need(stage == 9 and a[0] == (address+664) & U32 and a[1] <= U32, 'original u32 counter load')
        f.update(counter_loaded=a[1], stage=10)
    elif k == 8:
        need(stage == 10 and a[0] == (address+664) & U32 and a[1] == f['counter_loaded'] and
             a[2] == a[1]+1, 'original wide increment and low32 store')
        low = a[2] & U32; f['stage'] = 11 if 32 < low < 2**31 else 12
    elif k == 9: need(stage == 12, 'normal return requires complete body')
    elif k == 10: pass  # Unobserved original memory operations can throw at any prefix.
    else: need(False, 'entry replayed separately')


def _clock(meta, d, last, issues, entry=False, rejection=False):
    q = meta['qpc']
    if not meta['clock_valid']:
        need(d['clock_failed'] and d['observer_failed'], 'invalid clock requires sticky failures')
        return last
    need(0 < q <= QMAX and q >= last, 'monotonic original clock')
    if not rejection: need(q >= d['begin_qpc'], 'stored row begins in window')
    if entry: need(q < d['end_qpc_exclusive'], 'entry before configured horizon')
    if not d['clock_failed']:
        need(q <= d['freeze_qpc'], 'row cannot follow trusted immutable cut')
        if q == d['freeze_qpc']: issues.add('CUT_BOUNDARY_AMBIGUOUS')
    return q


def _matched_entry(e, m):
    temporary = copy.deepcopy(e); temporary['invocation'] = dict(thread_epoch=1, serial=1)
    _entry(temporary, True)
    need(m['accessor_status'] == 8 and m['base_nonnull'] and
         e['work_entry']['virtual_entry']['parent_entry']['update']['native_thread_id'] == m['native_thread_id'],
         'matched accessor has exact thread and base')


def _zero_work():
    v = dict(invocation=dict(thread_epoch=0, serial=0), parent_entry=work.virtual._zero_parent(),
             parent_child=parent._zero_child(), actual_target=0, caller_lr=0, r3=0, f1_bits=0)
    return dict(invocation=dict(thread_epoch=0, serial=0), virtual_entry=v,
                active_call0=work.virtual._zero_call(), actual_target=0, caller_lr=0, r3=0, f1_bits=0)


def _rejections(d, issues):
    ids, last = set(), 0
    for r in d['rejected_entries']:
        exact(r, {'reason', 'entry', 'meta'}, 'exact rejected body')
        reason, e, m = r['reason'], r['entry'], r['meta']
        uint(reason, 4); _entry(e); _meta(m)
        if m['accessor_status'] != 8:
            need(e['work_entry'] == _zero_work() and e['vector_pre'] == dict(fields=[0]*10, vector=[0]*4) and
                 e['parent_event_index'] == 0, 'invalid accessor supplies no stale association')
        else:
            work._entry(e['work_entry'], True)
            p = e['vector_pre']['fields']
            need(p[0] == CALLER and p[1] < 65535 and not any(p[5:]) and
                 0 < e['parent_event_index'] < 65536, 'valid accessor retains a genuine vector call shape')
        last = _clock(m, d, last, issues, rejection=True)
        need(m['context_nonnull'] and m['lexical_top'], 'constructor context and lexical top')
        if reason == 4:
            iid = parent._id(e['invocation'], True); need(iid not in ids, 'unique rejected own ID'); ids.add(iid)
        else: need(parent._id(e['invocation']) == (0, 0), 'unallocated rejection ID is zero')
        if reason == 0:
            need(not m['clock_valid'] and not m['lane_matches'] and not m['parent_admitted'], 'clock rejection before lane')
            if m['binding_matches']: _matched_entry(e, m)
            continue
        need(m['clock_valid'] and d['begin_qpc'] <= m['qpc'] < d['end_qpc_exclusive'], 'in-window rejected body')
        if reason == 1:
            need(not m['lane_matches'] and not m['parent_admitted'], 'binding or shared-health rejection before lane')
            if m['binding_matches']: _matched_entry(e, m)
        else:
            need(m['binding_matches'] and m['lane_matches'] is (reason >= 3), 'post-binding rejection stage')
            _matched_entry(e, m)
            if reason == 2: need(not m['parent_admitted'], 'lane rejection has no model parent')
    return ids


def _replay(lane, d, issues):
    stack, histories, lexical, completed, ids = [], {}, {}, [], set()
    entered = exceptions = 0; last_qpc = last_serial = last_work = last_event = 0
    previous = None
    for ev in lane['events']:
        index, k, m = ev['entry_index'], ev['event'], ev['meta']; e = lane['entries'][index]
        own = parent._id(e['invocation'], True); wid = parent._id(e['work_entry']['invocation'], True)
        need(ev['invocation'] == e['invocation'] and own[0] == lane['thread_epoch'] and
             wid[0] == lane['parent_work_thread_epoch'], 'exact event index and stable work lifetime')
        need(m['native_thread_id'] == lane['native_thread_id'] ==
             e['work_entry']['virtual_entry']['parent_entry']['update']['native_thread_id'] and
             all(m[x] for x in ('lane_matches', 'lexical_top', 'context_nonnull', 'base_nonnull')),
             'stored exact native lane and lexical context')
        if not m['binding_matches']:
            need(d['observer_failed'] and d['counts']['binding_mismatches'], 'binding failure explicitly sticky')
            issues.add('BINDING_MISMATCHES')
        else: need(m['accessor_status'] == 8, 'matched accessor status valid')
        last_qpc = _clock(m, d, last_qpc, issues, entry=k == 0)
        if k == 0:
            need(ev['fields'] == [0]*4 and ev['vector'] == [0]*4 and ev['site'] == 0, 'entry has no sampled payload')
            need(index == entered and len(stack) < 8 and m['binding_matches'] and m['clock_valid'], 'ordered admitted entry')
            need(own[1] > last_serial and wid[1] >= last_work and e['parent_event_index'] > last_event, 'strict own IDs and parent event order')
            if previous and wid[1] == last_work:
                need(e['work_entry'] == previous['work_entry'] and e['vector_pre']['fields'][1] < previous['vector_pre']['fields'][1],
                     'same work entry and strictly decreasing selected ordinal')
            if not d['counts']['dropped_entries'] and not d['counts']['observation_errors']:
                need(own[1] == last_serial+1, 'healthy allocation ordinal')
            need(not m['parent_admitted'] or bool(stack), 'lexical admitted parent requires stack')
            stack.append(_new_frame(index, lane['entries'][stack[-1]['entry_index']]['invocation'] if stack else dict(thread_epoch=0, serial=0)))
            histories[own] = []; lexical[own] = m['parent_admitted']; ids.add(own)
            entered += 1; last_serial, last_work, last_event = own[1], wid[1], e['parent_event_index']; previous = e
        else:
            need(stack and stack[-1]['entry_index'] == index and m['parent_admitted'] is lexical[own], 'exact LIFO and immutable lexical flag')
            _step(stack[-1], ev, e)
            if k in (9, 10):
                completed.append(dict(entry=copy.deepcopy(e), frame=copy.deepcopy(stack[-1]),
                    normal_return=k == 9, events=histories[own]+[copy.deepcopy(ev)]))
                stack.pop(); exceptions += int(k == 10)
        histories[own].append(copy.deepcopy(ev))
    need(entered == len(lane['entries']) and stack == lane['pending'], 'all entries admitted once and exact pending replay')
    need(bool(lane['parent_work_thread_epoch']) is bool(entered), 'work epoch bound by first entry')
    if stack: issues.add('PENDING_VECTOR_TARGET_INVOCATIONS')
    return entered, exceptions, completed, ids


def _evaluate(completed, issues):
    if issues: return []
    return [dict(entry=copy.deepcopy(r['entry']), frame=copy.deepcopy(r['frame']),
        events=copy.deepcopy(r['events']), invocation=copy.deepcopy(r['entry']['invocation']),
        entry_qpc=r['events'][0]['meta']['qpc'], exit_qpc=r['events'][-1]['meta']['qpc'],
        interpretation='REGISTER_ONLY_BODY_REPLAY_NO_EXTERNAL_PARENT_JOIN_OR_SIMULATION_PROOF')
        for r in completed if r['normal_return']]


def validate_vector_target(value, token):
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token) and token != '0'*32, 'nonzero token')
    d = _parse(value, MAX_BYTES); exact(d, TOP, 'exact vector target schema')
    need(d['schema'] == 'mcla-vector-target-v1' and d['token'] == token and d['clock'] == 'win32-query-performance-counter', 'schema/session/clock')
    for key in FALSE_FLAGS: need(d[key] is False, 'all proof flags false')
    for key in HEALTH: parent._bool(d[key])
    for key in LIMITATIONS: need(d[key] is True, 'explicit observation limitations')
    need(d['vector_lane_order'] == 'PPCContext.u32[0,1,2,3]_HOST_STORAGE_ORDER_NO_BYTE_SWAP', 'raw host vector lane order')
    for key, maximum in (('thread_capacity', 8), ('entries_per_thread_capacity', 4096), ('events_per_thread_capacity', 65536),
                         ('depth_capacity', 8), ('rejected_entry_capacity', 256), ('json_limit_bytes', MAX_BYTES)):
        need(uint(d[key]) == maximum, 'fixed native limit')
    begin, end, freq = uint(d['begin_qpc'], QMAX, 1), uint(d['end_qpc_exclusive'], QMAX, 1), uint(d['qpc_frequency'], 10**10, 1)
    need(begin < end and end-begin <= 12*freq, 'bounded observation window'); uint(d['freeze_qpc'])
    need(not d['clock_failed'] or d['observer_failed'], 'sticky clock failure')
    need(begin <= d['freeze_qpc'] <= QMAX or d['clock_failed'], 'actual trusted cut or recorded clock failure')
    c = d['counts']; exact(c, COUNTS, 'exact counters')
    for v in c.values(): uint(v)
    lanes, rejected = d['threads'], d['rejected_entries']
    need(type(lanes) is list and len(lanes) <= 8 and type(rejected) is list and len(rejected) <= 256, 'bounded complete inventories')
    need(c['thread_lifetimes'] == len(lanes) and c['rejected_entries'] == len(rejected), 'inventory counts')
    issues = {key.upper() for key in HEALTH if d[key]}
    for key in ('dropped_entries', 'binding_mismatches', 'observation_errors', 'original_exceptions', 'rejection_overflow'):
        if c[key]: issues.add(key.upper())
    if d['counter_saturated'] or any(c[key] for key in ('dropped_entries', 'binding_mismatches', 'observation_errors')):
        need(d['observer_failed'], 'recorded faults require sticky failure')
    for i, lane in enumerate(lanes):
        exact(lane, LANE, 'exact target lane'); need(uint(lane['thread_epoch'], 8, 1) == i+1, 'ordered own lifetime')
        uint(lane['native_thread_id'], U32, 1); uint(lane['parent_work_thread_epoch']); uint(lane['model_fault'], 5)
        for name, maximum, count in (('entries', 4096, 'entry_count'), ('events', 65536, 'event_count'), ('pending', 8, 'pending_count')):
            need(type(lane[name]) is list and uint(lane[count], maximum) == len(lane[name]), 'exact bounded array count')
        for e in lane['entries']: _entry(e, True)
        for ev in lane['events']:
            exact(ev, EVENT, 'exact target event'); parent._id(ev['invocation'], True)
            uint(ev['entry_index'], 4095); need(ev['entry_index'] < len(lane['entries']), 'event entry exists')
            uint(ev['event'], 10); uint(ev['site'], 6); _meta(ev['meta'])
            _array(ev['fields'], 4, 2**64-1); _array(ev['vector'], 4, U32)
        for f in lane['pending']:
            exact(f, FRAME, 'exact pending frame'); uint(f['entry_index'], 4095); parent._id(f['model_parent'])
            uint(f['stage'], 14); uint(f['active'], 6); uint(f['counter_loaded'], U32)
        if not lane['entries']: need(lane['model_fault'] and d['observer_failed'], 'empty lane requires failed initial admission')
        if lane['model_fault']:
            need(d['observer_failed'], 'model failure sticky'); issues.add('MODEL_FAULT')
        for fault, count, maximum in ((3, 'entry_count', 4096), (4, 'event_count', 65536), (5, 'pending_count', 8)):
            if lane['model_fault'] == fault: need(lane[count] == maximum, 'capacity failure at exact bound')
    rejected_ids = _rejections(d, issues)
    admitted = exceptions = 0; completed = []; ids = set(); epochs = set()
    for lane in lanes:
        epoch = lane['parent_work_thread_epoch']
        if epoch: need(epoch not in epochs, 'work lifetime cannot alias lanes'); epochs.add(epoch)
        n, x, rows, own = _replay(lane, d, issues)
        need(not own & rejected_ids, 'rejected identity never admitted')
        admitted += n; exceptions += x; completed.extend(rows); ids.update(own)
    last_rejected = {lane['thread_epoch']: max((x[1] for x in ids if x[0] == lane['thread_epoch']), default=0) for lane in lanes}
    for r in rejected:
        if r['reason'] < 3: continue
        matches = [lane for lane in lanes if lane['native_thread_id'] == r['meta']['native_thread_id']]
        need(matches, 'post-lane rejection requires lane inventory')
        if r['reason'] == 4:
            epoch, serial = parent._id(r['entry']['invocation'], True)
            matches = [lane for lane in matches if lane['thread_epoch'] == epoch]
            need(len(matches) == 1 and matches[0]['model_fault'] and serial > last_rejected[epoch], 'model rejection after admitted IDs')
            # _rejections already checked every first-Enter identity operand,
            # and this match supplies the model's own epoch/positive serial.
            # With no admitted entry, the model has no prior work/event/ordinal
            # identity to contradict those operands. An identity fault here is
            # impossible, unlike a duplicate/reversal after a real admission.
            need(matches[0]['entry_count'] != 0 or matches[0]['model_fault'] != 1,
                 'valid first entry cannot identity-fail on a fresh lane')
            last_rejected[epoch] = serial
    if not d['counter_saturated']:
        need(c['observed_entries'] == c['admitted_entries']+c['outside_before_entries']+c['outside_after_entries']+c['dropped_entries'], 'observed partition')
        need(c['admitted_entries'] == admitted and c['original_exceptions'] == exceptions, 'replayed counters')
        need(c['dropped_entries'] == len(rejected)+c['rejection_overflow'], 'rejection partition')
        need(c['observation_errors'] >= c['dropped_entries']+c['binding_mismatches'], 'error conservation')
        need(c['binding_mismatches'] >= sum(not ev['meta']['binding_matches'] for lane in lanes for ev in lane['events']),
             'recorded mismatched rows require counted mismatches')
    if c['rejection_overflow']: need(len(rejected) == 256, 'rejected headers full at overflow')
    if not admitted: issues.add('EMPTY_VECTOR_TARGET_INVENTORY')
    candidates = _evaluate(completed, issues)
    result = dict.fromkeys(FALSE_FLAGS | {'actual_execution_association_proven', 'frame_to_step_mapping_proven'}, False)
    result.update(schema='mcla-vector-target-analysis-v1', token=token,
        status='VECTOR_TARGET_DIAGNOSTIC_INCONCLUSIVE' if issues else 'VECTOR_TARGET_CANDIDATES_REVIEW_PENDING',
        raw_sha256=hashlib.sha256(value).hexdigest(), vector_target_inventory=copy.deepcopy(d),
        completed_invocations=copy.deepcopy(completed), candidate_records=candidates,
        candidate_blocking_issues=sorted(issues), pending_invocations=sum(lane['pending_count'] for lane in lanes),
        physical_acceptance_pending=True,
        interpretation='REGISTER_ONLY_BODY_REPLAY_NO_EXTERNAL_PARENT_JOIN_OR_SIMULATION_PROOF')
    return result
