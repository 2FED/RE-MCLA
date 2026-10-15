"""Strict pure replay of the selected vector load/compute/store witness.

No IO, floating arithmetic, units, external parent join or simulation/FPS proof.
Raw register equality proves only journal consistency, including zero change.
"""
import copy
import hashlib
import re

import mcla_vector_target_decode as target
from mcla_phase_source_join import _parse

parent = target.parent
need, exact, uint = parent.need, parent.exact, parent.uint
U32, QMAX, MAX_BYTES = target.U32, target.QMAX, 128 * 1024 * 1024
TARGET, TAIL, CALLER = 0x8259E768, 0x8259D030, 0x8259F364
FALSE_FLAGS, HEALTH, LIMITATIONS = target.FALSE_FLAGS, target.HEALTH, target.LIMITATIONS
UNSELECTED = {'unselected_absent_entries', 'unselected_wrong_stage_entries', 'unselected_other_target_entries'}
COUNTS = target.COUNTS | UNSELECTED | {'unselected_entries'}
TOP = target.TOP
ENTRY = {'invocation', 'target_entry', 'slot28_pre', 'parent_event_index',
         'actual_target', 'caller_lr', 'r3', 'v1'}
META_FLAGS, META = target.META_FLAGS, target.META
LANE = (target.LANE - {'parent_work_thread_epoch'}) | {'parent_target_thread_epoch'}
FRAME = {'entry_index', 'model_parent', 'stage', 'computed', 'scaled'}
EVENT = target.EVENT - {'site'}
_array, _clock = target._array, target._clock
INTERPRETATION = 'REGISTER_ONLY_STORE_REPLAY_NO_EXTERNAL_PARENT_JOIN_ARITHMETIC_OR_SIMULATION_PROOF'


def _saved_parent(e):
    target._entry(e['target_entry'], True)
    p, outer = e['slot28_pre'], e['target_entry']
    need(p['fields'][0] == CALLER and p['fields'][1] == outer['r3'] and
         p['fields'][2] & U32 == TARGET and p['fields'][3] == 0 and
         p['vector'] == outer['v1'] and 0 < e['parent_event_index'] < 65536,
         'exact saved selected Slot28 call')


def _entry(e, admitted=False):
    exact(e, ENTRY, 'exact vector store entry'); parent._id(e['invocation'], admitted)
    target._entry(e['target_entry'], admitted)
    p = e['slot28_pre']; exact(p, {'fields', 'vector'}, 'exact saved Slot28 payload')
    _array(p['fields'], 4, 2**64-1); _array(p['vector'], 4, U32); _array(e['v1'], 4, U32)
    uint(e['parent_event_index'], U32); uint(e['actual_target'], U32)
    uint(e['caller_lr']); uint(e['r3'])
    if admitted:
        _saved_parent(e)
        need(e['actual_target'] == TARGET and e['caller_lr'] == CALLER and
             e['r3'] == e['target_entry']['r3'] and e['v1'] == e['target_entry']['v1'],
             'actual store body matches selected parent operands')


def _meta(m):
    exact(m, META, 'exact store metadata'); uint(m['qpc'])
    uint(m['native_thread_id'], U32, 1); uint(m['accessor_status'], 9)
    for key in META_FLAGS: parent._bool(m[key])


def _new_frame(index, model_parent):
    return dict(entry_index=index, model_parent=copy.deepcopy(model_parent),
                stage=0, computed=[0]*4, scaled=[0]*4)


def _step(f, ev, entry):
    k, a, v, stage = ev['event'], ev['fields'], ev['vector'], f['stage']
    used = 1 if k in (1, 2, 4, 6) else 3 if k == 7 else 0
    need(not any(a[used:]) and (1 <= k <= 7 or v == [0]*4), 'unused payload is zero')
    if 1 <= k <= 7:
        need(stage == k-1, 'exact original load compute store tail order')
        if k in (1, 2, 4, 6):
            offset = {1: 256, 2: 272, 4: 288, 6: 256}[k]
            need(a[0] == ((entry['r3']+offset) & U32 & ~15), 'original aligned effective address')
        if k == 3: f['computed'] = copy.deepcopy(v)
        if k == 5: f['scaled'] = copy.deepcopy(v)
        if k == 6: need(v == f['computed'], 'successful store uses observed computed register')
        if k == 7:
            need(a[:3] == [CALLER, entry['r3'], TAIL] and v == f['scaled'], 'original tail arguments')
        f['stage'] = k
    elif k in (8, 9):
        need(stage == 7, 'tail return or exception requires active tail'); f['stage'] = k
    elif k == 10: need(stage == 8, 'normal return requires complete body and tail')
    elif k == 11: pass  # Original memory/tail execution may throw at any prefix.
    else: need(False, 'entry replayed separately')


def _thread(e):
    return e['target_entry']['work_entry']['virtual_entry']['parent_entry']['update']['native_thread_id']


def _matched_entry(e, m):
    temporary = copy.deepcopy(e); temporary['invocation'] = dict(thread_epoch=1, serial=1)
    _entry(temporary, True)
    need(m['accessor_status'] == 8 and m['base_nonnull'] and _thread(e) == m['native_thread_id'],
         'matched accessor has exact thread and base')


def _zero_target():
    return dict(invocation=dict(thread_epoch=0, serial=0), work_entry=target._zero_work(),
                vector_pre=dict(fields=[0]*10, vector=[0]*4), parent_event_index=0,
                actual_target=0, caller_lr=0, r3=0, v1=[0]*4)


def _rejections(d, issues):
    ids, last = set(), 0
    for r in d['rejected_entries']:
        exact(r, {'reason', 'entry', 'meta'}, 'exact rejected store body')
        reason, e, m = r['reason'], r['entry'], r['meta']
        uint(reason, 4); _entry(e); _meta(m)
        if m['accessor_status'] != 8:
            need(e['target_entry'] == _zero_target() and e['slot28_pre'] == dict(fields=[0]*4, vector=[0]*4) and
                 e['parent_event_index'] == 0, 'invalid accessor supplies no stale association')
        else: _saved_parent(e)
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
        need(m['accessor_status'] not in (0, 5, 9), 'valid-clock unselected context is not a rejection')
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
    entered = exceptions = 0; last_qpc = last_serial = last_target = last_event = 0
    for ev in lane['events']:
        index, k, m = ev['entry_index'], ev['event'], ev['meta']; e = lane['entries'][index]
        own = parent._id(e['invocation'], True); outer = parent._id(e['target_entry']['invocation'], True)
        need(ev['invocation'] == e['invocation'] and own[0] == lane['thread_epoch'] and
             outer[0] == lane['parent_target_thread_epoch'], 'exact event index and stable target lifetime')
        need(m['native_thread_id'] == lane['native_thread_id'] == _thread(e) and
             all(m[x] for x in ('lane_matches', 'lexical_top', 'context_nonnull', 'base_nonnull')),
             'stored exact native lane and lexical context')
        if not m['binding_matches']:
            need(d['observer_failed'] and d['counts']['binding_mismatches'], 'binding failure explicitly sticky')
            issues.add('BINDING_MISMATCHES')
        else: need(m['accessor_status'] == 8, 'matched accessor status valid')
        last_qpc = _clock(m, d, last_qpc, issues, entry=k == 0)
        if k == 0:
            need(ev['fields'] == [0]*4 and ev['vector'] == [0]*4, 'entry has no sampled payload')
            need(index == entered and len(stack) < 8 and m['binding_matches'] and m['clock_valid'], 'ordered admitted entry')
            need(own[1] > last_serial and outer[1] > last_target and e['parent_event_index'] > last_event,
                 'strict own IDs, parent target IDs and parent event order')
            if not d['counts']['dropped_entries'] and not d['counts']['observation_errors']:
                need(own[1] == last_serial+1, 'healthy allocation ordinal')
            need(not m['parent_admitted'] or bool(stack), 'lexical admitted parent requires stack')
            stack.append(_new_frame(index, lane['entries'][stack[-1]['entry_index']]['invocation'] if stack else dict(thread_epoch=0, serial=0)))
            histories[own] = []; lexical[own] = m['parent_admitted']; ids.add(own)
            entered += 1; last_serial, last_target, last_event = own[1], outer[1], e['parent_event_index']
        else:
            need(stack and stack[-1]['entry_index'] == index and m['parent_admitted'] is lexical[own],
                 'exact LIFO and immutable lexical flag')
            _step(stack[-1], ev, e)
            if k in (10, 11):
                completed.append(dict(entry=copy.deepcopy(e), frame=copy.deepcopy(stack[-1]),
                    normal_return=k == 10, events=histories[own]+[copy.deepcopy(ev)]))
                stack.pop(); exceptions += int(k == 11)
        histories[own].append(copy.deepcopy(ev))
    need(entered == len(lane['entries']) and stack == lane['pending'], 'all entries admitted once and exact pending replay')
    need(bool(lane['parent_target_thread_epoch']) is bool(entered), 'target epoch bound by first entry')
    if stack: issues.add('PENDING_VECTOR_STORE_INVOCATIONS')
    return entered, exceptions, completed, ids


def _evaluate(completed, issues):
    if issues: return []
    return [dict(entry=copy.deepcopy(r['entry']), frame=copy.deepcopy(r['frame']),
        events=copy.deepcopy(r['events']), invocation=copy.deepcopy(r['entry']['invocation']),
        entry_qpc=r['events'][0]['meta']['qpc'], exit_qpc=r['events'][-1]['meta']['qpc'],
        interpretation=INTERPRETATION) for r in completed if r['normal_return']]


def validate_vector_store(value, token):
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token) and token != '0'*32, 'nonzero token')
    d = _parse(value, MAX_BYTES); exact(d, TOP, 'exact vector store schema')
    need(d['schema'] == 'mcla-vector-store-v1' and d['token'] == token and
         d['clock'] == 'win32-query-performance-counter', 'schema/session/clock')
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
        exact(lane, LANE, 'exact store lane'); need(uint(lane['thread_epoch'], 8, 1) == i+1, 'ordered own lifetime')
        uint(lane['native_thread_id'], U32, 1); uint(lane['parent_target_thread_epoch']); uint(lane['model_fault'], 5)
        for name, maximum, count in (('entries', 4096, 'entry_count'), ('events', 65536, 'event_count'), ('pending', 8, 'pending_count')):
            need(type(lane[name]) is list and uint(lane[count], maximum) == len(lane[name]), 'exact bounded array count')
        for e in lane['entries']: _entry(e, True)
        for ev in lane['events']:
            exact(ev, EVENT, 'exact store event'); parent._id(ev['invocation'], True)
            uint(ev['entry_index'], 4095); need(ev['entry_index'] < len(lane['entries']), 'event entry exists')
            uint(ev['event'], 11); _meta(ev['meta'])
            _array(ev['fields'], 4, 2**64-1); _array(ev['vector'], 4, U32)
        for f in lane['pending']:
            exact(f, FRAME, 'exact pending frame'); uint(f['entry_index'], 4095); parent._id(f['model_parent'])
            uint(f['stage'], 9); _array(f['computed'], 4, U32); _array(f['scaled'], 4, U32)
        # Claim follows the complete Bound check. Its fresh positive-epoch
        # model has fixed nonzero capacity and receives a positive own serial;
        # the first Enter cannot fail identity/capacity/transition. Later model
        # failures retain that first admission. A wholly empty inventory is
        # valid, but a fabricated empty claimed lane is not native output.
        need(lane['entries'], 'a claimed store lane has an admitted first entry')
        if lane['model_fault']:
            need(d['observer_failed'] and c['observation_errors'], 'model failure sticky and counted'); issues.add('MODEL_FAULT')
        for fault, count, maximum in ((3, 'entry_count', 4096), (4, 'event_count', 65536), (5, 'pending_count', 8)):
            if lane['model_fault'] == fault: need(lane[count] == maximum, 'capacity failure at exact bound')
    rejected_ids = _rejections(d, issues)
    admitted = exceptions = 0; completed = []; ids = set(); epochs = set()
    for lane in lanes:
        epoch = lane['parent_target_thread_epoch']
        if epoch: need(epoch not in epochs, 'target lifetime cannot alias lanes'); epochs.add(epoch)
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
            need(matches[0]['entry_count'] != 0 or matches[0]['model_fault'] != 1,
                 'valid first entry cannot identity-fail on a fresh lane')
            last_rejected[epoch] = serial
    if not d['counter_saturated']:
        need(c['unselected_entries'] == sum(c[key] for key in UNSELECTED), 'unselected context partition')
        need(c['observed_entries'] == c['admitted_entries']+c['outside_before_entries']+
             c['outside_after_entries']+c['unselected_entries']+c['dropped_entries'], 'observed partition')
        need(c['admitted_entries'] == admitted and c['original_exceptions'] == exceptions, 'replayed counters')
        need(c['dropped_entries'] == len(rejected)+c['rejection_overflow'], 'rejection partition')
        need(c['observation_errors'] >= c['dropped_entries']+c['binding_mismatches'], 'error conservation')
        need(c['binding_mismatches'] >= sum(not ev['meta']['binding_matches'] for lane in lanes for ev in lane['events']),
             'recorded mismatched rows require counted mismatches')
    if c['rejection_overflow']: need(len(rejected) == 256, 'rejected headers full at overflow')
    if not admitted: issues.add('EMPTY_VECTOR_STORE_INVENTORY')
    candidates = _evaluate(completed, issues)
    result = dict.fromkeys(FALSE_FLAGS | {'actual_execution_association_proven', 'frame_to_step_mapping_proven',
        'arithmetic_proven', 'state_units_proven'}, False)
    result.update(schema='mcla-vector-store-analysis-v1', token=token,
        status='VECTOR_STORE_DIAGNOSTIC_INCONCLUSIVE' if issues else 'VECTOR_STORE_CANDIDATES_REVIEW_PENDING',
        raw_sha256=hashlib.sha256(value).hexdigest(), vector_store_inventory=copy.deepcopy(d),
        completed_invocations=copy.deepcopy(completed), candidate_records=candidates,
        candidate_blocking_issues=sorted(issues), pending_invocations=sum(lane['pending_count'] for lane in lanes),
        physical_acceptance_pending=True, interpretation=INTERPRETATION)
    return result
