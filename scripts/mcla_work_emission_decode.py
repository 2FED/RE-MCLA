"""Strict pure replay of the compact selected 82588490 emission journal.

No IO, clocks, floating interpretation, external parent join, task completion
or simulation proof. Full ancestry is recorded inventory, not independently
verified execution. The original 825753B0 prelude is explicitly unobserved.
"""
import copy
import hashlib
import re

import mcla_virtual_detail_decode as virtual
from mcla_phase_source_join import _parse

parent = virtual.parent
need, exact, uint = parent.need, parent.exact, parent.uint
U32, QMAX, MAX_BYTES = parent.U32, parent.QMAX, 64 * 1024 * 1024
BODY, TAIL, CALLER = 0x82588490, 0x82741010, 0x8258D1E8
SITES = {3: 0x825885AC, 6: 0x825885CC, 11: 0x8258866C,
         15: 0x82588694, 19: 0x825886F4}
FALSE_FLAGS = {'native_admitted', 'source_work_join_proven', 'resolved_target_proven',
    'task_completion_proven', 'simulation_proven', 'fps_proven', 'display_proven',
    'coverage_proven', 'causal_join_proven', 'work_semantics_proven'}
HEALTH = parent.HEALTH
LIMITATIONS = {'binding_observed_before_event_qpc', 'independent_cut',
              'unobserved_ancestry_possible', 'prelude_unobserved'}
COUNTS = parent.COUNTS | {'tail_scopes', 'tail_rejected'}
TOP = FALSE_FLAGS | HEALTH | LIMITATIONS | {'schema', 'token', 'clock', 'begin_qpc',
    'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc', 'thread_capacity',
    'entries_per_thread_capacity', 'events_per_thread_capacity', 'depth_capacity',
    'rejected_entry_capacity', 'json_limit_bytes', 'vector_lane_order', 'counts',
    'threads', 'rejected_entries'}
ENTRY = {'invocation', 'virtual_entry', 'active_call0', 'actual_target', 'caller_lr', 'r3', 'f1_bits'}
META_FLAGS = parent.META_FLAGS | {'tail_present', 'tail_matches', 'tail_fresh'}
META = META_FLAGS | {'qpc', 'native_thread_id', 'accessor_status'}
LANE = {'thread_epoch', 'native_thread_id', 'parent_virtual_thread_epoch',
    'parent_detail_thread_epoch', 'parent_router_thread_epoch', 'model_fault',
    'entry_count', 'event_count', 'pending_count', 'entries', 'events', 'pending'}
FRAME_BOOLS = {'main_observed', 'scalar_count_observed', 'mode_observed',
               'job_bound_observed', 'vector_count_observed'}
FRAME_NUMBERS = {'main_raw', 'mode_raw', 'scalar_count', 'scalar_upper', 'scalar_index',
    'scalar_return', 'next_job', 'job_bound', 'job_return', 'vector_count', 'next_vector',
    'scalar_emissions', 'cleanup_emissions', 'job_emissions', 'wait_emissions', 'vector_emissions'}
FRAME = FRAME_BOOLS | FRAME_NUMBERS | {'entry_index', 'model_parent', 'stage'}
EVENT = {'invocation', 'entry_index', 'event', 'fields', 'vector', 'meta'}


def _entry(e, admitted=False):
    exact(e, ENTRY, 'exact immutable work entry')
    parent._id(e['invocation'], admitted); virtual._entry(e['virtual_entry'], admitted)
    c = e['active_call0']; exact(c, virtual.CALL, 'exact active virtual call0')
    for k in ('caller_lr', 'r3', 'f1_bits', 'indirect_ctr'): uint(c[k])
    uint(c['requested_target'], U32); uint(c['state'], 3)
    if not c['state']: need(c == virtual._zero_call(), 'absent call is wholly zero')
    uint(e['actual_target'], U32)
    for k in ('caller_lr', 'r3', 'f1_bits'): uint(e[k])
    if admitted:
        need(c['state'] == 1 and c['caller_lr'] == CALLER and c['requested_target'] == TAIL and
             c['indirect_ctr'] & U32 == TAIL and e['actual_target'] == BODY and
             e['caller_lr'] == CALLER and all(e[k] == c[k] for k in ('r3', 'f1_bits')),
             'actual body and immutable active call0 operands')


def _meta(m):
    exact(m, META, 'exact compact event metadata')
    uint(m['qpc']); uint(m['native_thread_id'], U32, 1); uint(m['accessor_status'], 8)
    for k in META_FLAGS: parent._bool(m[k])
    need(not m['tail_matches'] or m['tail_present'], 'matching tail is present')
    need(not m['tail_fresh'] or m['tail_present'], 'fresh tail is present')


def _frame(f):
    exact(f, FRAME, 'exact pending compact frame')
    uint(f['entry_index'], 2047); parent._id(f['model_parent']); uint(f['stage'], 16)
    for k in FRAME_BOOLS: parent._bool(f[k])
    for k in FRAME_NUMBERS: uint(f[k])


def _new_frame(index, model_parent):
    f = dict.fromkeys(FRAME_NUMBERS, 0); f.update(dict.fromkeys(FRAME_BOOLS, False))
    f.update(entry_index=index, model_parent=copy.deepcopy(model_parent), stage=0)
    return f


def _step(f, event):
    """Replay only raw scalar transitions; no observer/native work is invoked."""
    k, a, s = event['event'], event['fields'], f['stage']
    used = {1: 1, 2: 1, 3: 6, 4: 1, 6: 3, 9: 1, 10: 1, 11: 9,
            12: 1, 14: 1, 15: 4, 18: 1, 19: 5}.get(k, 0)
    need(all(v == 0 for v in a[used:]) and (k == 19 or event['vector'] == [0]*4),
         'unused payload fields are zero, not sampled operands')
    if k in SITES: need(a[0] == SITES[k], 'exact emission callsite')
    if k == 1:
        need(s == 0 and a[0] <= 255, 'main loaded u8 gate')
        f.update(main_observed=True, main_raw=a[0], stage=1 if a[0] else 15)
    elif k == 2:
        need(s == 1 and a[0] <= 65535, 'scalar loaded u16 count')
        f.update(scalar_count_observed=True, scalar_count=a[0], scalar_upper=a[0], stage=2)
    elif k == 3:
        need(s == 2 and a[1] < f['scalar_upper'], 'filtered scalar indices strictly decrease')
        f.update(scalar_index=a[1], scalar_upper=a[1], stage=3)
        f['scalar_emissions'] += 1
    elif k == 4:
        need(s == 3, 'scalar return requires active emission')
        f.update(scalar_return=a[0], stage=4 if a[0] & 255 == 0 else 2)
    elif k in (5, 8, 13, 17, 21):
        need(s == {5: 3, 8: 5, 13: 8, 17: 11, 21: 14}[k], 'exception requires active emission')
        f['stage'] = 16
    elif k == 6:
        need(s == 4, 'cleanup only after low-byte-zero scalar return')
        f['cleanup_emissions'] += 1; f['stage'] = 5
    elif k == 7:
        need(s == 5, 'cleanup return requires active emission'); f['stage'] = 2
    elif k == 9:
        need(s == 2 and a[0] <= 255, 'mode gate after scalar branch')
        f.update(mode_observed=True, mode_raw=a[0], stage=12 if a[0] else 6)
    elif k in (10, 14):
        need(s == (6 if k == 10 else 9) and a[0] <= U32, 'exact initial/reloaded raw u32 job bound')
        bound = a[0] - 2**32 if a[0] & 2**31 else a[0]
        f.update(job_bound_observed=True, job_bound=a[0],
                 stage=7 if bound > 0 and f['next_job'] < bound else 10)
    elif k == 11:
        need(s == 7 and a[1] == f['next_job'] and a[8] <= U32, 'ordinal job and raw descriptor f32 bits')
        f['job_emissions'] += 1; f['stage'] = 8
    elif k == 12:
        need(s == 8, 'job return requires active emission')
        f.update(job_return=a[0], next_job=f['next_job']+1, stage=9)
    elif k == 15:
        need(s == 10 and a[1] == f['job_bound'] and a[3] == 1, 'mandatory wait with last raw bound and r5=1')
        f['wait_emissions'] += 1; f['stage'] = 11
    elif k == 16:
        need(s == 11, 'wait return requires active emission'); f['stage'] = 15
    elif k == 18:
        need(s == 12 and a[0] <= 65535, 'vector loaded u16 count')
        f.update(vector_count_observed=True, vector_count=a[0], next_vector=max(0, a[0]-1),
                 stage=13 if a[0] else 15)
    elif k == 19:
        need(s == 13 and a[1] == f['next_vector'], 'exact reverse vector index')
        f['vector_emissions'] += 1; f['stage'] = 14
    elif k == 20:
        need(s == 14, 'vector return requires active emission')
        f['stage'] = 13 if f['next_vector'] else 15
        f['next_vector'] = max(0, f['next_vector']-1)
    elif k == 22: need(s == 15, 'normal body exit requires selected branch metadata complete')
    elif k == 23: pass  # Original unobserved work may throw at any valid prefix.
    else: need(False, 'entry handled only by admission replay')


def _clock(m, d, last):
    q = m['qpc']
    if m['clock_valid']:
        need(d['begin_qpc'] <= q <= QMAX and q >= last, 'positive in-window-or-straddling monotonic QPC')
        if not d['clock_failed']: need(q <= d['freeze_qpc'], 'event before actual trusted cut')
        return q
    need(d['clock_failed'] and d['observer_failed'], 'invalid recorded clock requires both failure flags')
    return last


def _rejections(d):
    ids, last = set(), 0
    for r in d['rejected_entries']:
        reason, e, m = r['reason'], r['entry'], r['meta']
        if m['accessor_status'] != 8:
            zero_virtual = dict(invocation={'thread_epoch': 0, 'serial': 0},
                parent_entry=virtual._zero_parent(), parent_child=parent._zero_child(),
                actual_target=0, caller_lr=0, r3=0, f1_bits=0)
            need(e['virtual_entry'] == zero_virtual and e['active_call0'] == virtual._zero_call(),
                 'invalid accessor retains status only, no stale parent fields')
        # Clock rejection may be outside the configured window, unlike stored rows.
        if m['clock_valid']:
            need(0 < m['qpc'] <= QMAX and m['qpc'] >= last, 'monotonic rejection clock')
            if not d['clock_failed']: need(m['qpc'] <= d['freeze_qpc'], 'rejection before trusted cut')
            last = m['qpc']
        else: need(d['clock_failed'] and d['observer_failed'], 'rejection failed clock retained')
        need(m['context_nonnull'] and m['lexical_top'], 'constructor lexical context')
        if reason == 5:
            iid = parent._id(e['invocation'], True); need(iid not in ids, 'unique rejected own ID'); ids.add(iid)
        else: need(parent._id(e['invocation']) == (0, 0), 'no own ID before model admission')
        if reason == 0:
            need(not m['clock_valid'] and not m['binding_matches'] and not m['lane_matches'] and
                 not m['parent_admitted'], 'clock rejection precedes binding and lane')
            continue
        need(m['clock_valid'] and d['begin_qpc'] <= m['qpc'] < d['end_qpc_exclusive'], 'rejected in-window body')
        if reason == 1:
            need((not m['tail_matches'] or not m['tail_fresh']) and not m['binding_matches'] and
                 not m['lane_matches'] and not m['parent_admitted'], 'tail rejection before binding')
        else:
            need(m['tail_matches'] and m['tail_fresh'], 'later rejection has fresh exact tail')
            if reason == 2:
                need(not m['lane_matches'] and not m['parent_admitted'], 'binding/shared-health rejection before lane')
                if m['binding_matches']: _matched_entry(e, m)
            else:
                need(m['binding_matches'] and m['lane_matches'] is (reason >= 4), 'post-binding lane rejection stage')
                _matched_entry(e, m)
                if reason == 3: need(not m['parent_admitted'], 'lane rejection has no admitted parent')
    return ids


def _matched_entry(e, m):
    # A rejection before own allocation can still carry a full positive parent.
    temporary = copy.deepcopy(e); temporary['invocation'] = {'thread_epoch': 1, 'serial': 1}
    _entry(temporary, True)
    need(m['accessor_status'] == 8 and m['base_nonnull'] and
         e['virtual_entry']['parent_entry']['update']['native_thread_id'] == m['native_thread_id'],
         'positive matched accessor identity and base')


def _replay(lane, d, issues, parent_ids):
    stack, histories, lexical, completed, ids = [], {}, {}, [], set()
    last_serials, last_clock, entered, exceptions = [0]*4, 0, 0, 0
    phase_epoch = None
    for ev in lane['events']:
        k, m, index = ev['event'], ev['meta'], ev['entry_index']; e = lane['entries'][index]
        v, p = e['virtual_entry'], e['virtual_entry']['parent_entry']
        own, vid, pid, rid = [parent._id(x, True) for x in
                            (e['invocation'], v['invocation'], p['invocation'], p['router'])]
        need(ev['invocation'] == e['invocation'] and own[0] == lane['thread_epoch'] and
             vid[0] == lane['parent_virtual_thread_epoch'] and pid[0] == lane['parent_detail_thread_epoch'] and
             rid[0] == lane['parent_router_thread_epoch'], 'exact entry index and separate stable parent lifetimes')
        need(m['native_thread_id'] == lane['native_thread_id'] == p['update']['native_thread_id'] and
             m['lane_matches'] and m['lexical_top'] and m['context_nonnull'] and m['base_nonnull'] and
             m['tail_present'] and m['tail_matches'] and m['binding_matches'] and m['accessor_status'] == 8,
             'stored native lane/context/base and lexical exact tail')
        if phase_epoch is None: phase_epoch = p['update']['thread_epoch']
        need(p['update']['thread_epoch'] == phase_epoch, 'stable explicit Update lifetime')
        last_clock = _clock(m, d, last_clock)
        if k == 0:
            need(ev['fields'] == [0]*10 and ev['vector'] == [0]*4, 'entry has no manufactured payload')
            need(index == entered and len(stack) < 8 and m['binding_matches'] and m['clock_valid'] and
                 m['qpc'] < d['end_qpc_exclusive'] and m['tail_fresh'], 'ordered admitted entry and fresh tail')
            serials = [i[1] for i in (own, vid, pid, rid)]
            need(all(a > b for a, b in zip(serials, last_serials)), 'strict nonwrapping own and parent serials')
            for namespace, ident in enumerate((vid, pid, rid)):
                need(ident not in parent_ids[namespace], 'one body per parent call0 and no parent alias across lanes')
                parent_ids[namespace].add(ident)
            if not d['counts']['dropped_entries'] and not d['counts']['observation_errors']:
                need(own[1] == last_serials[0]+1, 'healthy allocation ordinal')
            need(not m['parent_admitted'] or bool(stack), 'admitted lexical parent needs model stack')
            f = _new_frame(index, lane['entries'][stack[-1]['entry_index']]['invocation'] if stack else
                           {'thread_epoch': 0, 'serial': 0})
            stack.append(f); histories[own] = []; lexical[own] = m['parent_admitted']
            ids.add(own); entered += 1; last_serials = serials
        else:
            need(stack and stack[-1]['entry_index'] == index and not m['tail_fresh'] and
                 m['parent_admitted'] is lexical[own], 'exact LIFO identity and immutable lexical flag')
            _step(stack[-1], ev)
            if k in (22, 23):
                completed.append(dict(entry=copy.deepcopy(e), frame=copy.deepcopy(stack[-1]),
                    normal_return=k == 22, events=histories[own]+[copy.deepcopy(ev)]))
                stack.pop(); exceptions += int(k == 23)
        histories[own].append(copy.deepcopy(ev))
    need(entered == len(lane['entries']), 'all immutable entries have exactly one admission event')
    need(lane['pending'] == stack, 'exact detached pending frame replay')
    if stack: issues.add('PENDING_WORK_INVOCATIONS')
    for k in ('parent_virtual_thread_epoch', 'parent_detail_thread_epoch', 'parent_router_thread_epoch'):
        need(bool(lane[k]) is bool(entered), 'parent epoch binds only after successful admission')
    return (entered, exceptions), completed, ids, phase_epoch


def _evaluate(completed, issues):
    if issues: return []
    return [dict(invocation=copy.deepcopy(r['entry']['invocation']), entry=copy.deepcopy(r['entry']),
        frame=copy.deepcopy(r['frame']), events=copy.deepcopy(r['events']),
        entry_qpc=r['events'][0]['meta']['qpc'], exit_qpc=r['events'][-1]['meta']['qpc'],
        interpretation='SELECTED_SCALAR_REPLAY_ONLY_NO_EXTERNAL_PARENT_JOIN_OR_TASK_COMPLETION')
        for r in completed if r['normal_return']]


def validate_work_emission(value, token):
    """Validate every immutable metadata row before candidate evaluation."""
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token) and token != '0'*32, 'nonzero token')
    d = _parse(value, MAX_BYTES); exact(d, TOP, 'exact work emission schema')
    need(d['schema'] == 'mcla-work-emission-v1' and d['token'] == token and
         d['clock'] == 'win32-query-performance-counter', 'schema/session/clock')
    for k in FALSE_FLAGS: need(d[k] is False, 'all proof flags false')
    for k in HEALTH: parent._bool(d[k])
    for k in LIMITATIONS: need(d[k] is True, 'independent cut and prelude limitations retained')
    need(d['vector_lane_order'] == 'PPCContext.v1.u32[0,1,2,3]_HOST_STORAGE_ORDER_NO_BYTE_SWAP', 'raw vector storage order')
    for k, n in (('thread_capacity', 8), ('entries_per_thread_capacity', 2048),
                 ('events_per_thread_capacity', 65536), ('depth_capacity', 8),
                 ('rejected_entry_capacity', 256), ('json_limit_bytes', MAX_BYTES)):
        need(uint(d[k]) == n, 'fixed native capacity')
    begin, end, freq = uint(d['begin_qpc'], QMAX, 1), uint(d['end_qpc_exclusive'], QMAX, 1), uint(d['qpc_frequency'], 10**10, 1)
    need(begin < end and end-begin <= 12*freq, 'bounded configured window'); uint(d['freeze_qpc'])
    need(not d['clock_failed'] or d['observer_failed'], 'clock failure is sticky observer failure')
    need(begin <= d['freeze_qpc'] <= QMAX or d['clock_failed'], 'valid actual cut or explicit failed clock')
    c = d['counts']; exact(c, COUNTS, 'exact native counters')
    for v in c.values(): uint(v)
    lanes, rejected = d['threads'], d['rejected_entries']
    need(type(lanes) is list and len(lanes) <= 8 and type(rejected) is list and len(rejected) <= 256, 'bounded inventories')
    need(c['thread_lifetimes'] == len(lanes) and c['rejected_entries'] == len(rejected), 'inventory counts')
    for i, lane in enumerate(lanes):
        exact(lane, LANE, 'exact independent work lane')
        need(uint(lane['thread_epoch'], 8, 1) == i+1, 'allocation ordered independent own epoch')
        uint(lane['native_thread_id'], U32, 1)
        for k in ('parent_virtual_thread_epoch', 'parent_detail_thread_epoch', 'parent_router_thread_epoch'): uint(lane[k])
        uint(lane['model_fault'], 5)
        for name, high in (('entries', 2048), ('events', 65536), ('pending', 8)):
            count = {'entries': 'entry_count', 'events': 'event_count', 'pending': 'pending_count'}[name]
            need(type(lane[name]) is list and uint(lane[count], high) == len(lane[name]), 'bounded stored array count')
        for e in lane['entries']: _entry(e, True)
        for ev in lane['events']:
            exact(ev, EVENT, 'exact compact event'); parent._id(ev['invocation'], True)
            uint(ev['entry_index'], 2047); need(ev['entry_index'] < len(lane['entries']), 'entry index exists')
            uint(ev['event'], 23); _meta(ev['meta'])
            for key, length, high in (('fields', 10, 2**64-1), ('vector', 4, U32)):
                need(type(ev[key]) is list and len(ev[key]) == length, 'exact compact payload length')
                for v in ev[key]: uint(v, high)
        for f in lane['pending']: _frame(f)
        # Claim and the first Enter run under the same native mutex. A cut
        # cannot observe a newly claimed healthy lane before its entry. A
        # failed initial Enter may leave an empty lane, but never a healthy one.
        if not lane['entries']:
            need(lane['model_fault'] != 0 and d['observer_failed'],
                 'empty claimed lane requires failed initial model admission')
        if lane['model_fault'] == 3: need(lane['entry_count'] == 2048, 'entry capacity fault requires full entries')
        if lane['model_fault'] == 4: need(lane['event_count'] == 65536, 'event capacity fault requires full events')
        if lane['model_fault'] == 5: need(lane['pending_count'] == 8, 'depth fault requires full stack')
    for r in rejected:
        exact(r, {'reason', 'entry', 'meta'}, 'exact rejected body'); uint(r['reason'], 5); _entry(r['entry']); _meta(r['meta'])
    rejected_ids = _rejections(d)
    issues = {k.upper() for k in HEALTH if d[k]}
    for k in ('dropped_entries', 'binding_mismatches', 'observation_errors', 'original_exceptions', 'rejection_overflow'):
        if c[k]: issues.add(k.upper())
    if any(c[k] for k in ('dropped_entries', 'binding_mismatches', 'observation_errors')) or d['counter_saturated']:
        need(d['observer_failed'], 'recorded faults require observer failure')
    total, completed, ids, namespaces, epochs = [0]*2, [], set(), [set() for _ in range(3)], [set() for _ in range(4)]
    for lane in lanes:
        if lane['model_fault']:
            need(d['observer_failed'], 'model failure requires observer failure'); issues.add('MODEL_FAULT')
        for j, k in enumerate(('parent_virtual_thread_epoch', 'parent_detail_thread_epoch', 'parent_router_thread_epoch')):
            if lane[k]: need(lane[k] not in epochs[j], 'parent lifetime cannot alias lanes'); epochs[j].add(lane[k])
        n, rows, own, phase_epoch = _replay(lane, d, issues, namespaces)
        if phase_epoch is not None: need(phase_epoch not in epochs[3], 'Update lifetime cannot alias lanes'); epochs[3].add(phase_epoch)
        need(not own & rejected_ids, 'rejected own identity never admitted'); ids.update(own); completed.extend(rows)
        total = [a+b for a, b in zip(total, n)]
    rejected_last = {l['thread_epoch']: max((i[1] for i in ids if i[0] == l['thread_epoch']), default=0) for l in lanes}
    for r in rejected:
        if r['reason'] not in (4, 5): continue
        m, e = r['meta'], r['entry']
        matches = [l for l in lanes if l['native_thread_id'] == m['native_thread_id']]
        need(matches, 'post-lane rejection has recorded native thread inventory')
        if r['reason'] == 5:
            epoch, serial = parent._id(e['invocation'], True)
            matches = [l for l in matches if l['thread_epoch'] == epoch]
            need(len(matches) == 1 and matches[0]['model_fault'] and serial > rejected_last[epoch],
                 'allocated model rejection on explicit failed lane after admissions')
            rejected_last[epoch] = serial
    if not d['counter_saturated']:
        need(c['observed_entries'] == c['admitted_entries']+c['outside_before_entries']+c['outside_after_entries']+c['dropped_entries'], 'observed entry partition')
        need(c['admitted_entries'] == total[0] and c['original_exceptions'] == total[1], 'admission and exception counts match replay')
        need(c['dropped_entries'] == c['rejected_entries']+c['rejection_overflow'], 'rejection partition')
        need(c['observation_errors'] >= c['dropped_entries']+c['binding_mismatches'], 'rejection/mismatch error conservation')
        need(c['tail_rejected'] <= c['tail_scopes'] and c['admitted_entries'] <= c['tail_scopes']-c['tail_rejected'], 'admission consumes a counted eligible fresh tail')
    if c['rejection_overflow']: need(len(rejected) == 256, 'overflow requires full stored rejection inventory')
    if not total[0]: issues.add('EMPTY_WORK_INVENTORY')
    candidates = _evaluate(completed, issues)
    result = dict.fromkeys(FALSE_FLAGS | {'actual_execution_association_proven', 'frame_to_step_mapping_proven'}, False)
    result.update(schema='mcla-work-emission-analysis-v1', token=token,
        status='WORK_EMISSION_DIAGNOSTIC_INCONCLUSIVE' if issues else 'WORK_EMISSION_CANDIDATES_REVIEW_PENDING',
        raw_sha256=hashlib.sha256(value).hexdigest(), work_emission_inventory=copy.deepcopy(d),
        completed_invocations=copy.deepcopy(completed), candidate_records=candidates,
        candidate_blocking_issues=sorted(issues), pending_invocations=sum(l['pending_count'] for l in lanes),
        retained_context_issues=['TAIL_ELIGIBILITY_REJECTIONS_OUTSIDE_PARENT_WINDOW_POSSIBLE'] if c['tail_rejected'] else [],
        prelude_unobserved=True, physical_acceptance_pending=True,
        interpretation='REGISTER_ONLY_SELECTED_WORK_REPLAY_NO_EXTERNAL_PARENT_JOIN_OR_TOTAL_WORK_PROOF')
    return result
