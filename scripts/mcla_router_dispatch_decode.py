"""Pure bounded router call-site journal replay, never consumer-internal proof.

All metadata, model transitions, pending snapshots and explicit phase references
validate before candidate evaluation. No IO, clocks, pixels, guest reads,
timestamp selection, repaired history or native admission.
"""
import copy
import hashlib
import json
import re

import mcla_phase_census_decode as phase_decoder
from mcla_phase_source_join import _parse

MAX_BYTES = 32 * 1024 * 1024
U64, U32, QMAX = 2**64-1, 2**32-1, 2**63-1
FALSE_FLAGS = {'native_admitted', 'coverage_proven', 'resolved_target_proven',
    'consumer_internals_proven', 'simulation_proven', 'state_coherence_proven',
    'object_lifetime_proven', 'frame_to_step_mapping_proven', 'fps_proven'}
HEALTH = {'observer_failed', 'clock_failed', 'counter_saturated', 'disarmed_at_cut'}
COUNTS = {'observed_entries', 'admitted_entries', 'outside_before_entries',
    'outside_after_entries', 'dropped_entries', 'unbound_entries',
    'unexpected_route_entries', 'binding_mismatches', 'observation_errors',
    'original_exceptions', 'thread_lifetimes'}
TOP = FALSE_FLAGS | HEALTH | {'schema', 'token', 'clock', 'begin_qpc',
    'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc', 'thread_capacity',
    'events_per_thread_capacity', 'depth_capacity', 'counts', 'threads',
    'binding_observed_before_event_qpc', 'unobserved_ancestry_possible'}
THREAD = {'thread_epoch', 'native_thread_id', 'model_fault', 'event_count',
          'pending_count', 'events', 'pending'}
EVENT_FLAGS = {'clock_valid', 'binding_matches', 'lane_matches', 'expected_route',
    'context_nonnull', 'base_nonnull', 'lexical_top', 'parent_admitted'}
EVENT = EVENT_FLAGS | {'event', 'qpc', 'native_thread_id', 'observed_binding', 'frame'}
BINDING = {'phase_call_id', 'thread_epoch', 'native_thread_id'}
ID = {'thread_epoch', 'serial'}
DISPATCH = {'r3', 'table', 'ctr', 'requested_target_u32', 'f1_bits', 'present'}
FRAME = {'invocation', 'model_parent', 'entry_binding', 'caller_lr', 'manager_r3',
    'selector_r4', 'entry_f1_bits', 'stage', 'predicate', 'consumer',
    'predicate_result_r3', 'predicate_result_low8', 'predicate_returned'}
ROUTES = {0x822C24C4: 1, 0x822C273C: 4, 0x822C2768: 0, 0x822C27C0: 8, 0x822C2848: 2}
need, exact, uint = phase_decoder.need, phase_decoder.exact, phase_decoder.uint


def _bool(value):
    need(type(value) is bool, 'exact boolean')


def _id(value, *, zero=False):
    exact(value, ID, 'exact router invocation ID')
    a, b = uint(value['thread_epoch']), uint(value['serial'])
    need((zero and a == b == 0) or (a > 0 and b > 0), 'whole positive or absent invocation ID')
    return a, b


def _binding(value):
    exact(value, BINDING, 'exact phase binding')
    a, b, c = uint(value['phase_call_id']), uint(value['thread_epoch']), uint(value['native_thread_id'], U32)
    need((a == b == c == 0) or (a > 0 and b > 0 and c > 0), 'whole positive or absent phase binding')
    return a != 0


def _dispatch(value):
    exact(value, DISPATCH, 'exact dispatch snapshot')
    for key in DISPATCH - {'present'}: uint(value[key], U32 if key == 'requested_target_u32' else U64)
    _bool(value['present'])
    need(value['requested_target_u32'] == value['ctr'] & U32, 'requested target is exact low32 CTR')
    if not value['present']:
        need(all(value[k] == 0 for k in DISPATCH - {'present'}), 'absent dispatch zeroed')


def _frame(value):
    exact(value, FRAME, 'exact detached model frame')
    _id(value['invocation']); _id(value['model_parent'], zero=True); _binding(value['entry_binding'])
    for key in ('caller_lr', 'manager_r3', 'selector_r4', 'entry_f1_bits', 'predicate_result_r3'): uint(value[key])
    uint(value['stage'], 6); uint(value['predicate_result_low8'], 255); _bool(value['predicate_returned'])
    _dispatch(value['predicate']); _dispatch(value['consumer'])
    need(value['predicate_result_low8'] == value['predicate_result_r3'] & 255, 'predicate result low byte')


def _expected(frame):
    return bool(frame['manager_r3'] & U32) and ROUTES.get(frame['caller_lr']) == frame['selector_r4'] & U32


def _zero_dispatch():
    return dict(r3=0, table=0, ctr=0, requested_target_u32=0, f1_bits=0, present=False)


def _phase(raw, source, token):
    data = _parse(raw, phase_decoder.MAX_BYTES)
    exact(data, phase_decoder.TOP_KEYS, 'exact phase v2 schema')
    need(data['schema'] == 'mcla-phase-census-v2', 'phase v2 required')
    callbacks = data['swap_callbacks']
    need(type(callbacks) is list and len(callbacks) <= 1024, 'bounded phase callbacks')
    seen = set()
    for row in callbacks:
        exact(row, phase_decoder.CALLBACK_KEYS | {'callback_id'}, 'exact v2 callback')
        cid = uint(row['callback_id'], 1024, 1)
        need(cid not in seen, 'unique callback ID'); seen.add(cid)
    need(seen == set(range(1, len(callbacks)+1)), 'complete callback ID inventory')
    old = copy.deepcopy(data); old['schema'] = 'mcla-phase-census-v1'
    for row in old['swap_callbacks']: del row['callback_id']
    audit = phase_decoder.validate_phase_census(json.dumps(old, separators=(',', ':')).encode(), source, token)
    return data, audit


def _phase_ref(binding, event, phases, issues):
    if not _binding(binding):
        return
    pid = binding['phase_call_id']
    need(pid in phases, 'unknown explicit phase ID')
    p = phases[pid]
    need(p['kind'] == 'update' and p['thread_epoch'] == binding['thread_epoch'] and
         p['native_thread_id'] == binding['native_thread_id'] == event['native_thread_id'],
         'exact Update phase/epoch/native thread')
    if event['clock_valid']:
        need(p['entry_qpc'] <= event['qpc'], 'event cannot precede explicit Update')
        if p['completed'] and not p['invalid_bracket']:
            need(event['qpc'] <= p['exit_qpc'], 'event cannot follow completed Update')
    if not p['completed'] or p['invalid_bracket'] or p['original_exception'] or not p['base_nonnull']:
        issues.add('UNHEALTHY_BOUND_UPDATE')
    if p['caller_lr'] != 0x821316A0: issues.add('UNKNOWN_UPDATE_CALLER')


def _replay_lane(lane, phases, data, issues):
    stack, last_serial, last_qpc = [], 0, 0
    entries, exits, mismatches, unbound, unexpected = 0, 0, 0, 0, 0
    terminal, histories, stable_meta = [], {}, {}
    for event in lane['events']:
        f = event['frame']; kind = event['event']; iid = _id(f['invocation'])
        need(iid[0] == lane['thread_epoch'], 'model invocation lane')
        need(event['native_thread_id'] == lane['native_thread_id'] and event['lane_matches'] and
             event['lexical_top'] and event['context_nonnull'], 'exact live native lane and lexical top')
        need(event['expected_route'] is _expected(f), 'exact route low32 predicate')
        if not event['base_nonnull']: issues.add('NULL_BASE')
        qpc = event['qpc']
        if event['clock_valid']:
            need(0 < qpc <= QMAX and qpc >= last_qpc, 'valid positive monotonic event clock')
            # A failed Freeze clock is retained verbatim, not repaired into an
            # ordering bound. Clock failure globally blocks every candidate;
            # earlier individually valid events still obey their own bounds.
            if not data['clock_failed']:
                need(qpc <= data['freeze_qpc'], 'valid event clock before trusted cut')
            last_qpc = qpc
        else:
            need(data['clock_failed'] and data['observer_failed'], 'invalid event clock requires sticky failure')
        _phase_ref(f['entry_binding'], event, phases, issues)
        _phase_ref(event['observed_binding'], event, phases, issues)
        matches = bool(event['observed_binding']['phase_call_id']) and event['observed_binding'] == f['entry_binding']
        need(event['binding_matches'] is matches, 'exact observed binding comparison')
        if kind == 0:
            need(iid[1] > last_serial and len(stack) < 8, 'strict serial and bounded nesting')
            if not data['counts']['dropped_entries'] and not data['counts']['observation_errors']:
                need(iid[1] == last_serial+1, 'healthy native lane has consecutive allocated serials')
            need(event['clock_valid'] and data['begin_qpc'] <= qpc < data['end_qpc_exclusive'], 'admitted entry inside window')
            need(event['observed_binding'] == f['entry_binding'], 'entry binding is observed binding')
            parent = stack[-1]['invocation'] if stack else {'thread_epoch': 0, 'serial': 0}
            need(not event['parent_admitted'] or bool(stack), 'admitted lexical parent requires model parent')
            wanted = copy.deepcopy(f)
            wanted.update(model_parent=parent, stage=0, predicate=_zero_dispatch(), consumer=_zero_dispatch(),
                          predicate_result_r3=0, predicate_result_low8=0, predicate_returned=False)
            need(f == wanted, 'exact new frame initialization')
            stack.append(copy.deepcopy(f)); last_serial = iid[1]; entries += 1
            unbound += int(not matches); unexpected += int(not event['expected_route'])
            stable_meta[iid] = (event['parent_admitted'], event['base_nonnull'])
            histories[iid] = []
        else:
            need(stack and stack[-1]['invocation'] == f['invocation'], 'top-only LIFO events')
            need(stable_meta[iid] == (event['parent_admitted'], event['base_nonnull']), 'immutable lexical metadata')
            wanted = copy.deepcopy(stack[-1]); stage = wanted['stage']
            if kind == 1:
                need(stage == 0 and f['predicate']['present'] and f['predicate']['f1_bits'] == 0, 'predicate pre transition')
                wanted.update(stage=1, predicate=copy.deepcopy(f['predicate']))
            elif kind == 2:
                need(stage == 1, 'predicate return transition')
                wanted.update(stage=2, predicate_result_r3=f['predicate_result_r3'],
                              predicate_result_low8=f['predicate_result_low8'], predicate_returned=True)
            elif kind in (3, 7):
                need(stage == (1 if kind == 3 else 4), 'dispatch exception transition'); wanted['stage'] = 6
            elif kind == 4:
                need(stage == 2 and wanted['predicate_result_low8'] == 0, 'skip requires predicate false'); wanted['stage'] = 3
            elif kind == 5:
                need(stage == 2 and wanted['predicate_result_low8'] != 0 and f['consumer']['present'], 'consumer pre requires predicate true')
                wanted.update(stage=4, consumer=copy.deepcopy(f['consumer']))
            elif kind == 6:
                need(stage == 4, 'consumer return transition'); wanted['stage'] = 5
            elif kind == 8:
                need(stage in (3, 5), 'normal exit requires skip or normal consumer return')
            elif kind == 9:
                exits += 1
            need(f == wanted, 'full snapshot matches exact model transition')
            stack[-1] = wanted
            mismatches += int(not matches)
            if kind in (8, 9):
                terminal.append((copy.deepcopy(f), kind, histories[iid] + [copy.deepcopy(event)])); stack.pop()
        histories[iid].append(copy.deepcopy(event))
    need(lane['pending'] == stack and lane['pending_count'] == len(stack), 'pending immutable model stack exact')
    if stack: issues.add('PENDING_ROUTER_INVOCATIONS')
    if lane['model_fault']: issues.add('MODEL_FAULT')
    return (entries, exits, mismatches, unbound, unexpected), terminal


def _evaluate(terminal, issues):
    if issues: return []
    result = []
    for frame, kind, events in terminal:
        if kind != 8 or frame['stage'] != 5: continue
        result.append({'invocation': copy.deepcopy(frame['invocation']),
            'update_binding': copy.deepcopy(frame['entry_binding']),
            'frame': copy.deepcopy(frame), 'entry_qpc': events[0]['qpc'],
            'consumer_pre_qpc': next(e['qpc'] for e in events if e['event'] == 5),
            'consumer_return_qpc': next(e['qpc'] for e in events if e['event'] == 6),
            'router_exit_qpc': events[-1]['qpc'],
            'interpretation': 'OBSERVED_NORMAL_DISPATCH_RETURN_NOT_RESOLVED_TARGET_OR_CONSUMER_INTERNALS'})
    return result


def validate_router_dispatch(router_raw, phase_raw, source_receipt, token):
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token) and token != '0'*32, 'nonzero session token')
    phase, audit = _phase(phase_raw, copy.deepcopy(source_receipt), token)
    data = _parse(router_raw, MAX_BYTES); exact(data, TOP, 'exact router journal schema')
    need(data['schema'] == 'mcla-router-dispatch-v1' and data['token'] == token and
         data['clock'] == 'win32-query-performance-counter', 'schema session and clock')
    for k in FALSE_FLAGS: need(data[k] is False, 'all proof flags false')
    for k in HEALTH: _bool(data[k])
    for k in ('binding_observed_before_event_qpc', 'unobserved_ancestry_possible'):
        need(data[k] is True, 'explicit observation limitations retained')
    need(uint(data['thread_capacity']) == 8 and uint(data['events_per_thread_capacity']) == 16384 and
         uint(data['depth_capacity']) == 8, 'exact native capacities')
    for k in ('begin_qpc', 'end_qpc_exclusive', 'qpc_frequency'):
        uint(data[k]); need(data[k] == phase[k], 'exact shared phase window')
    need(0 < data['begin_qpc'] < data['end_qpc_exclusive'] <= QMAX and
         0 < data['qpc_frequency'] <= 10**10 and data['end_qpc_exclusive']-data['begin_qpc'] <= 12*data['qpc_frequency'], 'bounded QPC window')
    uint(data['freeze_qpc'])
    need(not data['clock_failed'] or data['observer_failed'], 'sticky clock failure')
    need(data['begin_qpc'] <= data['freeze_qpc'] <= QMAX or data['clock_failed'], 'cut or explicit clock fault')
    counts = data['counts']; exact(counts, COUNTS, 'exact counters')
    for v in counts.values(): uint(v)
    lanes = data['threads']; need(type(lanes) is list and len(lanes) <= 8, 'bounded thread inventory')
    need(counts['thread_lifetimes'] == len(lanes), 'thread count')
    for i, lane in enumerate(lanes):
        exact(lane, THREAD, 'exact thread journal')
        need(uint(lane['thread_epoch'], 8, 1) == i+1, 'allocation ordered unique lane epoch')
        uint(lane['native_thread_id'], U32, 1); uint(lane['model_fault'], 4)
        need(type(lane['events']) is list and uint(lane['event_count'], 16384) == len(lane['events']), 'bounded exact event count')
        need(type(lane['pending']) is list and uint(lane['pending_count'], 8) == len(lane['pending']), 'bounded exact pending count')
        if lane['model_fault'] == 3:
            need(lane['event_count'] == 16384, 'capacity fault requires full event storage')
        if lane['model_fault'] == 4:
            need(lane['pending_count'] == 8, 'depth fault requires full lexical stack')
        for event in lane['events']:
            exact(event, EVENT, 'exact event metadata'); uint(event['event'], 9); uint(event['qpc']); uint(event['native_thread_id'], U32, 1)
            for k in EVENT_FLAGS: _bool(event[k])
            _binding(event['observed_binding']); _frame(event['frame'])
        for pending in lane['pending']: _frame(pending)
    issues = {'phase:'+s for s in audit['diagnostic_issues']}
    issues.update(k.upper() for k in HEALTH if data[k])
    for k in ('dropped_entries', 'unbound_entries', 'unexpected_route_entries', 'binding_mismatches', 'observation_errors', 'original_exceptions'):
        if counts[k]: issues.add(k.upper())
    if data['counter_saturated']: need(data['observer_failed'], 'saturation requires failure')
    for k in ('dropped_entries', 'binding_mismatches', 'observation_errors'):
        if counts[k]: need(data['observer_failed'], 'lost or failed observations require sticky failure')
    total = [0]*5; terminal = []; phases = {p['call_id']: p for p in phase['scopes']}
    phase_lanes, router_lanes = {}, {}
    for lane in lanes:
        for event in lane['events']:
            for binding in (event['observed_binding'], event['frame']['entry_binding']):
                if not binding['phase_call_id']: continue
                pe, router_epoch = binding['thread_epoch'], lane['thread_epoch']
                need(pe not in phase_lanes or phase_lanes[pe] == router_epoch, 'phase thread lifetime reused across router lanes')
                need(router_epoch not in router_lanes or router_lanes[router_epoch] == pe, 'router lane changed phase thread lifetime')
                phase_lanes[pe], router_lanes[router_epoch] = router_epoch, pe
        if lane['model_fault']: need(data['observer_failed'], 'model fault requires observer failure')
        n, completed = _replay_lane(lane, phases, data, issues)
        total = [a+b for a,b in zip(total,n)]; terminal.extend(completed)
    admitted, exceptions, mismatch, unbound, unexpected = total
    if not data['counter_saturated']:
        need(counts['observed_entries'] == counts['admitted_entries'] + counts['outside_before_entries'] + counts['outside_after_entries'] + counts['dropped_entries'], 'entry admission partition')
        need(counts['admitted_entries'] == admitted and counts['original_exceptions'] == exceptions, 'event counts equal counters')
        need(mismatch <= counts['binding_mismatches'] <= mismatch+counts['observation_errors'], 'binding mismatch failed-event bound')
        for key, n in (('unbound_entries', unbound), ('unexpected_route_entries', unexpected)):
            need(n <= counts[key] <= n+counts['dropped_entries'], 'admitted and failed-enter classification bound')
    if not admitted: issues.add('EMPTY_ROUTER_INVENTORY')
    candidates = _evaluate(terminal, issues)
    groups = {}
    for candidate in candidates:
        binding = candidate['update_binding']
        identity = (binding['phase_call_id'], binding['thread_epoch'], binding['native_thread_id'])
        group = groups.setdefault(identity, {'update_binding': copy.deepcopy(binding), 'invocations': []})
        group['invocations'].append(copy.deepcopy(candidate['invocation']))
    result = dict.fromkeys(FALSE_FLAGS | {'actual_consumer_proven', 'causal_join_proven', 'source_router_join_proven'}, False)
    result.update(schema='mcla-router-dispatch-analysis-v1', token=token,
        status='ROUTER_DISPATCH_DIAGNOSTIC_INCONCLUSIVE' if issues else 'ROUTER_DISPATCH_CANDIDATES_REVIEW_PENDING',
        raw_sha256=hashlib.sha256(router_raw).hexdigest(), phase_raw_sha256=hashlib.sha256(phase_raw).hexdigest(),
        router_inventory=copy.deepcopy(data), phase_inventory=copy.deepcopy(phase), phase_analysis=copy.deepcopy(audit),
        candidate_blocking_issues=sorted(issues), candidate_records=candidates,
        grouped_update_records=[groups[k] for k in sorted(groups)],
        completed_router_count=len(terminal), skipped_router_count=sum(k==8 and f['stage']==3 for f,k,_ in terminal),
        pending_router_count=sum(l['pending_count'] for l in lanes), physical_acceptance_pending=True,
        interpretation='EXACT_ID_ROUTER_DISPATCH_METADATA_NOT_CONSUMER_INTERNALS_SIMULATION_OR_FPS')
    return result
