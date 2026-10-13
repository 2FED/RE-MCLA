"""Strict offline replay of a bounded consumer-detail scalar journal.

validate_consumer_detail(raw_bytes, token) returns detached raw inventory and
normal/exceptional invocation summaries. Candidates require a wholly healthy
journal, but still do NOT prove native execution, router association, resolved
child targets, consumer internals, simulation, frames or FPS. No IO or clocks.
"""
import copy
import hashlib
import re

from mcla_phase_source_join import _parse
from mcla_phase_census_decode import need, exact, uint

MAX_BYTES = 32 * 1024 * 1024
U64, U32, QMAX = 2**64-1, 2**32-1, 2**63-1
TARGET, CALLER = 0x822C7870, 0x822C5328
CHILD_LRS = (0x822C78C0, 0x822C78CC, 0x822C78D8)
DIRECT_TARGETS = (0, 0x82588428, 0x82476238)
FALSE_FLAGS = {'native_admitted', 'coverage_proven', 'actual_execution_association_proven',
    'resolved_child_target_proven', 'consumer_internals_proven', 'simulation_proven',
    'state_coherence_proven', 'object_lifetime_proven', 'frame_to_step_mapping_proven', 'fps_proven'}
HEALTH = {'observer_failed', 'clock_failed', 'counter_saturated', 'disarmed_at_cut'}
COUNTS = {'observed_entries', 'admitted_entries', 'outside_before_entries', 'outside_after_entries',
    'dropped_entries', 'binding_mismatches', 'observation_errors', 'original_exceptions',
    'thread_lifetimes', 'rejected_entries', 'rejection_overflow'}
TOP = FALSE_FLAGS | HEALTH | {'schema', 'token', 'clock', 'begin_qpc', 'end_qpc_exclusive',
    'qpc_frequency', 'freeze_qpc', 'thread_capacity', 'events_per_thread_capacity',
    'depth_capacity', 'rejected_entry_capacity', 'counts', 'threads', 'rejected_entries',
    'binding_observed_before_event_qpc', 'unobserved_ancestry_possible'}
ID = {'thread_epoch', 'serial'}
BINDING = {'phase_call_id', 'thread_epoch', 'native_thread_id'}
ENTRY = {'invocation', 'router', 'update', 'actual_target', 'caller_lr', 'r3', 'f1_bits'}
ACCESS = {'status', 'invocation', 'update', 'r3', 'f1_bits', 'ctr', 'requested_target_u32'}
META_FLAGS = {'clock_valid', 'binding_matches', 'lane_matches', 'lexical_top',
              'context_nonnull', 'base_nonnull', 'parent_admitted'}
META = META_FLAGS | {'qpc', 'native_thread_id', 'observed_consumer'}
CHILD = {'caller_lr', 'r3', 'f1_bits', 'indirect_ctr', 'requested_target', 'state', 'indirect'}
FRAME = {'entry', 'model_parent', 'stage', 'branch_r11', 'branch_observed',
         'branch_nonzero', 'returned_children', 'children'}
LANE = {'thread_epoch', 'native_thread_id', 'router_thread_epoch', 'model_fault',
        'event_count', 'pending_count', 'events', 'pending'}


def _bool(value):
    need(type(value) is bool, 'exact boolean')


def _id(value, positive=False):
    exact(value, ID, 'exact invocation ID')
    a,b=uint(value['thread_epoch']),uint(value['serial'])
    if positive: need(a>0 and b>0, 'positive invocation identity')
    return a,b


def _binding(value, positive=False):
    exact(value, BINDING, 'exact Update binding')
    a,b,c=uint(value['phase_call_id']),uint(value['thread_epoch']),uint(value['native_thread_id'],U32)
    if positive:need(a>0 and b>0 and c>0, 'positive Update identity')
    return a,b,c


def _entry(value, admitted=False):
    exact(value, ENTRY, 'exact raw entry')
    _id(value['invocation'],admitted);_id(value['router'],admitted);_binding(value['update'],admitted)
    uint(value['actual_target'],U32)
    for key in ('caller_lr','r3','f1_bits'):uint(value[key])
    if admitted:need(value['actual_target']==TARGET and value['caller_lr']==CALLER, 'exact observed body/caller')


def _access(value):
    exact(value,ACCESS,'exact accessor observation')
    uint(value['status'],9);_id(value['invocation']);_binding(value['update'])
    for key in ('r3','f1_bits','ctr'):uint(value[key])
    uint(value['requested_target_u32'],U32)
    if value['status'] != 9:
        need(_id(value['invocation'])==(0,0) and _binding(value['update'])==(0,0,0) and
             all(value[k]==0 for k in ('r3','f1_bits','ctr','requested_target_u32')),
             'invalid accessor contains only status')


def _bound(a,tid):
    return (a['status']==9 and all(_id(a['invocation'])) and all(_binding(a['update'])) and
            a['update']['native_thread_id']==tid and a['requested_target_u32']==a['ctr']&U32)


def _same(a,entry,tid,saved=None):
    return (_bound(a,tid) and a['invocation']==entry['router'] and a['update']==entry['update'] and
            a['requested_target_u32']==TARGET and (saved is None or
            all(a[k]==saved[k] for k in ('r3','f1_bits','ctr'))))


def _meta(value):
    exact(value,META,'exact event metadata');uint(value['qpc']);uint(value['native_thread_id'],U32,1)
    for k in META_FLAGS:_bool(value[k])
    _access(value['observed_consumer'])


def _zero_child():
    return dict(caller_lr=0,r3=0,f1_bits=0,indirect_ctr=0,requested_target=0,state=0,indirect=False)


def _frame(value):
    exact(value,FRAME,'exact scalar frame');_entry(value['entry'],True);_id(value['model_parent'])
    uint(value['stage'],4);uint(value['branch_r11']);uint(value['returned_children'],3)
    _bool(value['branch_observed']);_bool(value['branch_nonzero'])
    need(type(value['children']) is list and len(value['children'])==3,'exact three child slots')
    for index,c in enumerate(value['children']):
        exact(c,CHILD,'exact child observation')
        for k in ('caller_lr','r3','f1_bits','indirect_ctr'):uint(c[k])
        uint(c['requested_target'],U32);uint(c['state'],3);_bool(c['indirect'])
        if c['state']==0:need(c==_zero_child(),'absent child is zeroed')
        else:
            need(c['caller_lr']==CHILD_LRS[index] and c['indirect'] is (index==0),'exact selected child route')
            if index==0:need(c['requested_target']==c['indirect_ctr']&U32,'indirect requested target low32')
            else:need(c['indirect_ctr']==0 and c['requested_target']==DIRECT_TARGETS[index], 'direct instruction target never CTR')


def _clock(meta,data,last):
    q=meta['qpc']
    if meta['clock_valid']:
        need(0<q<=QMAX and q>=last,'valid positive monotonic event QPC')
        if not data['clock_failed']:need(q<=data['freeze_qpc'],'valid event before trusted cut')
        return q
    need(data['clock_failed'] and data['observer_failed'],'invalid clock requires sticky failure')
    return last


def _rejections(data):
    rejected=data['rejected_entries'];last=0;detail_attempts=set()
    for row in rejected:
        exact(row,{'reason','entry','meta'},'exact rejected entry')
        reason=uint(row['reason'],5);e,m=row['entry'],row['meta'];_entry(e);_meta(m)
        last=_clock(m,data,last);a=m['observed_consumer'];tid=m['native_thread_id']
        need(e['router']==a['invocation'] and e['update']==a['update'],'rejected entry retains accessor identity')
        need(m['lexical_top'] and m['context_nonnull'],'constructor lexical/context observations')
        if reason!=5:need(_id(e['invocation'])==(0,0),'no detail ID fabricated before model admission')
        else:
            iid=_id(e['invocation'],True);need(iid not in detail_attempts,'unique rejected allocation attempt');detail_attempts.add(iid)
        bound=_bound(a,tid)
        entry_matches=(e['actual_target']==TARGET and e['caller_lr']==CALLER and
                       a['requested_target_u32']==TARGET and e['r3']==a['r3'] and e['f1_bits']==a['f1_bits'])
        if reason==0:
            need(not m['clock_valid'] and not m['binding_matches'] and not m['lane_matches'] and not m['parent_admitted'],'clock rejection before admission')
        else:
            need(m['clock_valid'] and data['begin_qpc']<=m['qpc']<data['end_qpc_exclusive'],'rejected configured-window entry')
            if reason==1:
                need((not bound or not m['base_nonnull']) and not m['binding_matches'] and not m['lane_matches'] and not m['parent_admitted'],'accessor rejection predicate')
            else:
                need(bound and m['base_nonnull'],'later rejection requires valid accessor/base')
                if reason==2:
                    need(not entry_matches and not m['binding_matches'] and not m['lane_matches'] and not m['parent_admitted'],'entry mismatch rejection predicate')
                else:
                    need(entry_matches and m['binding_matches'],'post-match rejection')
                    need(m['lane_matches'] is (reason>=4),'lane/serial/model rejection stage')
                    if reason<5:need(not m['parent_admitted'],'parent only sampled at model admission')
    return detail_attempts


def _replay(lane,data,issues,global_router_ids):
    stack=[];last_id=last_router=last_qpc=0;entries=exceptions=mismatches=0
    summaries=[];histories={};accesses={};lexical={};admitted_ids=set();phase_epoch=None
    for event in lane['events']:
        kind,m,f=event['event'],event['meta'],event['frame'];e=f['entry'];iid=_id(e['invocation'],True)
        rid=_id(e['router'],True);tid=lane['native_thread_id']
        need(iid[0]==lane['thread_epoch'] and rid[0]==lane['router_thread_epoch'],'exact independent detail/router lane epochs')
        need(m['native_thread_id']==tid and m['lane_matches'] and m['lexical_top'] and m['context_nonnull'] and m['base_nonnull'],'admitted lane/context/base/top')
        need(e['update']['native_thread_id']==tid,'entry binding native lane')
        pe=e['update']['thread_epoch']
        if phase_epoch is None:phase_epoch=pe
        need(pe==phase_epoch,'one phase thread lifetime per detail lane')
        last_qpc=_clock(m,data,last_qpc)
        matches=_same(m['observed_consumer'],e,tid,accesses.get(iid) if kind else None)
        need(m['binding_matches'] is matches,'exact accessor comparison')
        if kind==0:
            need(iid[1]>last_id and rid[1]>last_router and rid not in global_router_ids and len(stack)<8,'monotonic IDs, once per router, bounded nesting')
            if not data['counts']['dropped_entries'] and not data['counts']['observation_errors']:
                need(iid[1]==last_id+1,'healthy native detail allocation serials')
            need(m['clock_valid'] and data['begin_qpc']<=m['qpc']<data['end_qpc_exclusive'],'entry inside admitted window')
            a=m['observed_consumer']
            need(matches and e['r3']==a['r3'] and e['f1_bits']==a['f1_bits'],'exact entry saved dispatch operands')
            need(not m['parent_admitted'] or bool(stack),'admitted parent requires model parent')
            wanted=copy.deepcopy(f);wanted.update(model_parent=stack[-1]['entry']['invocation'] if stack else {'thread_epoch':0,'serial':0},
                stage=0,branch_r11=0,branch_observed=False,branch_nonzero=False,returned_children=0,children=[_zero_child() for _ in range(3)])
            need(f==wanted,'exact initialized frame')
            stack.append(copy.deepcopy(f));last_id=iid[1];last_router=rid[1];entries+=1
            global_router_ids.add(rid);admitted_ids.add(iid);accesses[iid]=copy.deepcopy(a);lexical[iid]=m['parent_admitted'];histories[iid]=[]
        else:
            need(stack and stack[-1]['entry']['invocation']==e['invocation'],'LIFO exact detail identity')
            need(m['parent_admitted'] is lexical[iid],'immutable lexical parent admission')
            if matches:need(m['observed_consumer']==accesses[iid],'saved active-consumer observation cannot change operands')
            mismatches+=int(not matches)
            wanted=copy.deepcopy(stack[-1]);stage=wanted['stage'];child=wanted['returned_children']
            if kind==1:
                need(stage==0,'branch observed once');wanted.update(stage=1,branch_r11=f['branch_r11'],branch_observed=True,branch_nonzero=bool(f['branch_r11']&U32))
            elif kind==2:
                need(stage in (1,3) and wanted['branch_observed'] and wanted['branch_nonzero'] and child<3,'ordered child pre after nonzero branch')
                need(f['children'][child]['state']==1,'selected child active');wanted['children'][child]=copy.deepcopy(f['children'][child]);wanted['stage']=2
            elif kind in (3,4):
                need(stage==2 and child<3 and wanted['children'][child]['state']==1,'matching active child end')
                wanted['children'][child]['state']=2 if kind==3 else 3;wanted['stage']=3 if kind==3 else 4
                if kind==3:wanted['returned_children']+=1
            elif kind==5:
                need(wanted['branch_observed'] and ((wanted['branch_nonzero'] and stage==3 and child==3) or
                     (not wanted['branch_nonzero'] and stage==1)),'normal exit requires exact selected route completed')
            elif kind==6:exceptions+=1
            need(f==wanted,'full scalar transition snapshot exact');stack[-1]=wanted
            if kind in (5,6):
                summaries.append({'frame':copy.deepcopy(f),'normal_return':kind==5,'events':histories[iid]+[copy.deepcopy(event)]});stack.pop()
        histories[iid].append(copy.deepcopy(event))
    need(lane['pending']==stack and lane['pending_count']==len(stack),'immutable pending stack exact')
    if stack:issues.add('PENDING_DETAIL_INVOCATIONS')
    if lane['model_fault']:issues.add('MODEL_FAULT')
    need(bool(lane['router_thread_epoch']) is bool(entries),'router lane epoch bound only by successful first entry')
    return (entries,exceptions,mismatches),summaries,admitted_ids,phase_epoch


def _evaluate(completed,issues):
    if issues:return []
    return [{'invocation':copy.deepcopy(row['frame']['entry']['invocation']),
             'router_invocation':copy.deepcopy(row['frame']['entry']['router']),
             'update_binding':copy.deepcopy(row['frame']['entry']['update']),
             'frame':copy.deepcopy(row['frame']), 'entry_qpc':row['events'][0]['meta']['qpc'],
             'exit_qpc':row['events'][-1]['meta']['qpc'],
             'interpretation':'NORMAL_BODY_ROUTE_METADATA_NOT_INDEPENDENT_EXECUTION_ASSOCIATION_OR_SIMULATION_PROOF'}
            for row in completed if row['normal_return']]


def validate_consumer_detail(value,token):
    """Return detached analysis of immutable raw JSON bytes; no external joins."""
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}',token) and token!='0'*32,'nonzero session token')
    data=_parse(value,MAX_BYTES);exact(data,TOP,'exact consumer detail schema')
    need(data['schema']=='mcla-consumer-detail-v1' and data['token']==token and data['clock']=='win32-query-performance-counter','schema/session/clock')
    for k in FALSE_FLAGS:need(data[k] is False,'all proof flags false')
    for k in HEALTH:_bool(data[k])
    for k in ('binding_observed_before_event_qpc','unobserved_ancestry_possible'):need(data[k] is True,'limitations retained')
    for k,n in (('thread_capacity',8),('events_per_thread_capacity',16384),('depth_capacity',8),('rejected_entry_capacity',256)):
        need(uint(data[k])==n,'fixed native capacity')
    begin,end,freq=uint(data['begin_qpc'],QMAX,1),uint(data['end_qpc_exclusive'],QMAX,1),uint(data['qpc_frequency'],10**10,1)
    need(begin<end and end-begin<=12*freq,'bounded configured window');uint(data['freeze_qpc'])
    need(not data['clock_failed'] or data['observer_failed'],'sticky clock failure')
    need(begin<=data['freeze_qpc']<=QMAX or data['clock_failed'],'real cut or flagged failure')
    counts=data['counts'];exact(counts,COUNTS,'exact counters')
    for v in counts.values():uint(v)
    lanes=data['threads'];need(type(lanes) is list and len(lanes)<=8,'bounded lanes')
    rejected=data['rejected_entries'];need(type(rejected) is list and len(rejected)<=256,'bounded rejection inventory')
    need(counts['thread_lifetimes']==len(lanes) and counts['rejected_entries']==len(rejected),'stored inventory counts')
    for i,lane in enumerate(lanes):
        exact(lane,LANE,'exact lane');need(uint(lane['thread_epoch'],8,1)==i+1,'allocation ordered lane epochs')
        uint(lane['native_thread_id'],U32,1);uint(lane['router_thread_epoch']);uint(lane['model_fault'],4)
        need(type(lane['events']) is list and uint(lane['event_count'],16384)==len(lane['events']),'bounded event count')
        need(type(lane['pending']) is list and uint(lane['pending_count'],8)==len(lane['pending']),'bounded pending count')
        if lane['model_fault']==3:need(lane['event_count']==16384,'capacity fault requires full storage')
        if lane['model_fault']==4:need(lane['pending_count']==8,'depth fault requires full stack')
        for event in lane['events']:
            exact(event,{'event','meta','frame'},'exact event');uint(event['event'],6);_meta(event['meta']);_frame(event['frame'])
        for f in lane['pending']:_frame(f)
    rejected_ids=_rejections(data)
    issues={k.upper() for k in HEALTH if data[k]}
    for k in ('dropped_entries','binding_mismatches','observation_errors','original_exceptions','rejection_overflow'):
        if counts[k]:issues.add(k.upper())
    if counts['dropped_entries'] or counts['binding_mismatches'] or counts['observation_errors'] or data['counter_saturated']:
        need(data['observer_failed'],'loss/errors require sticky observer failure')
    all_ids=set();router_ids=set();totals=[0,0,0];completed=[];router_epochs=set();phase_epochs=set()
    for lane in lanes:
        if lane['model_fault']:need(data['observer_failed'],'model fault requires failure')
        if lane['router_thread_epoch']:
            need(lane['router_thread_epoch'] not in router_epochs,'one router lifetime cannot alias detail lanes');router_epochs.add(lane['router_thread_epoch'])
        n,rows,ids,pe=_replay(lane,data,issues,router_ids)
        if pe is not None:need(pe not in phase_epochs,'one phase lifetime cannot alias detail lanes');phase_epochs.add(pe)
        need(not ids&rejected_ids,'rejected allocation cannot also be admitted')
        all_ids.update(ids);completed.extend(rows);totals=[a+b for a,b in zip(totals,n)]
    # Failed model attempts allocate IDs in the same lane before Enter rejects.
    # The sticky model fault prevents any later admitted entry on that lane.
    last_rejected={lane['thread_epoch']:max((i[1] for i in all_ids if i[0]==lane['thread_epoch']),default=0)
                   for lane in lanes}
    for row in rejected:
        if row['reason']!=5:continue
        epoch,serial=_id(row['entry']['invocation'],True)
        need(1<=epoch<=len(lanes),'rejected model attempt owns an existing lane')
        lane=lanes[epoch-1]
        need(lane['model_fault']!=0 and row['meta']['native_thread_id']==lane['native_thread_id'],
             'rejected model attempt preserves failed lane identity')
        need(serial>last_rejected[epoch],'rejected model allocation follows all admitted/prior attempts')
        last_rejected[epoch]=serial
    if not data['counter_saturated']:
        need(counts['observed_entries']==counts['admitted_entries']+counts['outside_before_entries']+counts['outside_after_entries']+counts['dropped_entries'],'entry partition')
        need(counts['admitted_entries']==totals[0] and counts['original_exceptions']==totals[1],'admitted/exception events equal counters')
        need(counts['dropped_entries']==counts['rejected_entries']+counts['rejection_overflow'],'rejection conservation')
        need(totals[2]<=counts['binding_mismatches']<=totals[2]+counts['observation_errors'],'failed model event mismatch bounds')
    if counts['rejection_overflow']:need(len(rejected)==256,'overflow requires full rejected storage')
    if not totals[0]:issues.add('EMPTY_DETAIL_INVENTORY')
    candidates=_evaluate(completed,issues)
    result=dict.fromkeys(FALSE_FLAGS|{'causal_join_proven','source_detail_join_proven'},False)
    result.update(schema='mcla-consumer-detail-analysis-v1',token=token,
        status='CONSUMER_DETAIL_DIAGNOSTIC_INCONCLUSIVE' if issues else 'CONSUMER_DETAIL_CANDIDATES_REVIEW_PENDING',
        raw_sha256=hashlib.sha256(value).hexdigest(),consumer_detail_inventory=copy.deepcopy(data),
        completed_invocations=copy.deepcopy(completed),candidate_records=candidates,
        candidate_blocking_issues=sorted(issues),pending_invocations=sum(l['pending_count'] for l in lanes),
        physical_acceptance_pending=True,interpretation='MODEL_REPLAY_ONLY_NO_EXTERNAL_ROUTER_PHASE_OR_SOURCE_ASSOCIATION')
    return result
