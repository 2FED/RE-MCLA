"""Synthetic exact-ID join tests; no native/private data or IO."""
import copy
import unittest
from unittest import mock

import mcla_frame_virtual_join as decoder
from test_mcla_frame_consumer_join import fixture as consumer_fixture, add_detail
from test_mcla_frame_router_join import add_router
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def body_events(parent, serial=1):
    vd = decoder.virtual_decoder
    pre = next(e for e in parent if e['event'] == 2 and e['frame']['returned_children'] == 0)
    pe = copy.deepcopy(pre['frame']['entry']); child = copy.deepcopy(pre['frame']['children'][0])
    entry = dict(invocation={'thread_epoch':1,'serial':serial}, parent_entry=pe, parent_child=child,
                 actual_target=decoder.TARGET, caller_lr=decoder.CALLER, r3=child['r3'], f1_bits=child['f1_bits'])
    frame = dict(entry=entry, model_parent={'thread_epoch':0,'serial':0}, stage=0,
                 returned_calls=0, calls=[vd._zero_call() for _ in range(3)])
    events = []
    def emit(kind):
        m = dict.fromkeys(vd.parent.META_FLAGS, True); m['parent_admitted'] = False
        m.update(qpc=pre['meta']['qpc'], native_thread_id=pe['update']['native_thread_id'],
                 observed_child=dict(status=8,entry=copy.deepcopy(pe),child=copy.deepcopy(child),child_index=0))
        events.append(dict(event=kind,meta=m,frame=copy.deepcopy(frame)))
    emit(0)
    for i in range(3):
        frame['calls'][i] = dict(caller_lr=vd.CALL_LRS[i], r3=300+i,
            f1_bits=0x8000000000000000 if i == 1 else 0x7ff8000012345678,
            indirect_ctr=0xabcdef0082300000+i*4, requested_target=0x82300000+i*4, state=1)
        frame['stage']=1;emit(1)
        frame['calls'][i]['state']=2;frame.update(stage=2,returned_calls=i+1);emit(2)
    emit(4)
    return events


def fixture():
    values=consumer_fixture(); consumer=values[0]
    for e in consumer['threads'][0]['events']:
        child=e['frame']['children'][0]
        if child['state']:
            child.update(indirect_ctr=0xabcdef0000000000|decoder.TARGET,requested_target=decoder.TARGET)
    vd=decoder.virtual_decoder
    data={k:copy.deepcopy(v) for k,v in consumer.items() if k not in vd.parent.FALSE_FLAGS}
    data.update(dict.fromkeys(vd.FALSE_FLAGS,False));data.update(dict.fromkeys(vd.LIMITATIONS,True))
    data['schema']='mcla-virtual-detail-v1'
    lane=data['threads'][0];lane.pop('router_thread_epoch')
    lane.update(parent_detail_thread_epoch=1,parent_router_thread_epoch=1,
                events=body_events(consumer['threads'][0]['events']))
    lane['event_count']=len(lane['events'])
    return (data,)+values


def decode(v):
    return decoder.validate_frame_virtual_join(*(raw(x) for x in v[:8]),v[8],TOKEN)


def analyses(v):
    return (decoder.virtual_decoder.validate_virtual_detail(raw(v[0]),TOKEN),
            decoder.consumer_join.validate_frame_consumer_join(*(raw(x) for x in v[1:8]),v[8],TOKEN))


class FrameVirtualTests(unittest.TestCase):
    def reject(self,v):
        with mock.patch.object(decoder,'_evaluate',side_effect=AssertionError('premature join')) as evaluate:
            with self.assertRaises(ValueError):decode(v)
        evaluate.assert_not_called()

    def test_full_raw_chain_immutable_detached_no_io_all_false(self):
        v=fixture();before=copy.deepcopy(v)
        with mock.patch('builtins.open',side_effect=AssertionError('no IO')):r=decode(v)
        self.assertEqual(v,before);self.assertEqual(len(r['candidate_records']),91)
        self.assertEqual(r['candidate_records'][0]['virtual_detail_invocations'],[{'thread_epoch':1,'serial':1}])
        a,b=analyses(v);self.assertEqual(r['virtual_analysis'],a);self.assertEqual(r['frame_consumer_analysis'],b)
        for k in decoder.FALSE_FLAGS:self.assertIs(r[k],False)
        self.assertNotIn('PASS',r['status']);self.assertNotIn('f1_sum',r)
        r['candidate_records'][0]['virtual_detail_invocations'][0]['serial']=100
        self.assertEqual(r['candidate_records'][1]['virtual_detail_invocations'][0]['serial'],1)

    def test_exact_shared_cut_window_clock_and_token(self):
        for field in ('begin_qpc','end_qpc_exclusive','qpc_frequency','freeze_qpc'):
            v=fixture();v[0][field]+=1;self.reject(v)
        v=fixture();v[0]['token']='1'*32;self.reject(v)

    def test_missing_body_rejects_entire_two_parent_group(self):
        v=fixture();add_router(v[2:]);add_detail(v[1:]);r=decode(v)
        self.assertEqual(r['candidate_records'],[]);self.assertEqual(len(r['excluded_candidate_records']),91)
        self.assertEqual(r['excluded_candidate_records'][0]['expected_virtual_body_count'],2)

    def test_two_bodies_then_one_bad_relation_cannot_be_filtered(self):
        v=fixture();add_router(v[2:]);add_detail(v[1:])
        lane=v[0]['threads'][0];lane['events']+=body_events(v[1]['threads'][0]['events'][9:],2)
        lane['event_count']=len(lane['events']);v[0]['counts'].update(observed_entries=2,admitted_entries=2)
        self.assertEqual(len(decode(v)['candidate_records']),91)
        for e in lane['events'][8:]:
            e['frame']['entry']['parent_entry']['r3']^=1<<40
            e['meta']['observed_child']['entry']['r3']^=1<<40
        r=decode(v);self.assertEqual(r['candidate_records'],[])
        self.assertEqual(r['excluded_candidate_records'][0]['expected_virtual_body_count'],2)

    def test_entire_parent_snapshot_and_child_full_operands(self):
        for which,key in (('parent','r3'),('parent','f1_bits'),('child','r3'),('child','f1_bits'),('child','indirect_ctr')):
            v=fixture()
            for e in v[0]['threads'][0]['events']:
                en=e['frame']['entry'];a=e['meta']['observed_child']
                if which=='parent':en['parent_entry'][key]^=1<<40;a['entry'][key]^=1<<40
                else:
                    en['parent_child'][key]^=1<<40;a['child'][key]^=1<<40
                    if key in ('r3','f1_bits'):en[key]^=1<<40
            self.assertEqual(decode(v)['candidate_records'],[])

    def test_orphan_and_unselected_contradiction_global(self):
        v=fixture()
        for e in v[0]['threads'][0]['events']:
            e['frame']['entry']['parent_entry']['invocation']['serial']=2
            e['meta']['observed_child']['entry']['invocation']['serial']=2
        r=decode(v);self.assertIn('ORPHAN_VIRTUAL_BODY_WITHOUT_APPLICABLE_PARENT_CHILD',r['candidate_blocking_issues'])
        a,b=analyses(fixture());b['candidate_records']=[]
        a['completed_invocations'][0]['frame']['entry']['parent_entry']['r3']+=1
        issues=set();r=decoder._evaluate(a,b,issues,set())
        self.assertEqual(r[0],[]);self.assertIn('UNSELECTED_VIRTUAL_PARENT_RELATION_INVALID',issues)

    def test_duplicate_actual_body_global_and_own_id_exact(self):
        a,b=analyses(fixture());extra=copy.deepcopy(a['completed_invocations'][0])
        extra['frame']['entry']['invocation']['serial']=2;a['completed_invocations'].append(extra)
        issues=set();self.assertEqual(decoder._evaluate(a,b,issues,set())[0],[])
        self.assertIn('DUPLICATE_VIRTUAL_BODY_FOR_PARENT_CHILD',issues)
        extra['frame']['entry']['invocation']['serial']=1
        with self.assertRaises(ValueError):decoder._evaluate(a,b,set(),set())

    def test_all_event_brackets_and_exclusive_frontier(self):
        for delta in (-1,1):
            v=fixture()
            for e in v[0]['threads'][0]['events']:e['meta']['qpc']+=delta
            r=decode(v);self.assertEqual(r['candidate_records'],[])
        a,b=analyses(fixture());tick=a['completed_invocations'][0]['events'][0]['meta']['qpc']
        b['frame_router_analysis']['timing_frontier_analysis']['closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive']=tick
        self.assertEqual(decoder._evaluate(a,b,set(),set())[0],[])

    def test_unrelated_malformed_tail_prevents_evaluation(self):
        for change in (lambda v:v[0]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v:v[3]['records'][-1]['incoming'].update(f1_bits=True),
                       lambda v:v[7]['records'][-1].update(slot=999)):
            v=fixture();change(v);self.reject(v)
        v=fixture()
        with self.assertRaises(ValueError):decoder.validate_frame_virtual_join(v[0],*(raw(x) for x in v[1:8]),v[8],TOKEN)

    def test_pending_and_global_failure_cannot_be_healed(self):
        for flag in ('observer_failed','disarmed_at_cut'):
            v=fixture();v[0][flag]=True;self.assertEqual(decode(v)['candidate_records'],[])
        v=fixture();lane=v[0]['threads'][0];lane['events'].pop();lane['event_count']-=1
        lane.update(pending_count=1,pending=[copy.deepcopy(lane['events'][-1]['frame'])])
        self.assertEqual(decode(v)['candidate_records'],[])

    def test_other_child_target_unknown_not_missing_body(self):
        v=fixture();add_router(v[2:]);add_detail(v[1:])
        for e in v[1]['threads'][0]['events'][9:]:
            c=e['frame']['children'][0]
            if c['state']:c.update(indirect_ctr=0xabcdef0082300000,requested_target=0x82300000)
        r=decode(v);self.assertEqual(len(r['candidate_records']),91)
        self.assertEqual(r['candidate_records'][0]['unknown_child0_parent_invocations'],[{'thread_epoch':1,'serial':2}])
        self.assertIn('OTHER_CHILD0_TARGETS_RETAINED_UNKNOWN',r['retained_context_issues'])

    def test_zero_route_has_no_expected_virtual_body(self):
        a,b=analyses(fixture());parent=copy.deepcopy(b['consumer_analysis']['completed_invocations'][0])
        parent['frame']['entry']['invocation']['serial']=2;parent['frame']['branch_nonzero']=False
        b['consumer_analysis']['completed_invocations'].append(parent)
        for s in b['candidate_records']:s['consumer_detail_invocations'].append({'thread_epoch':1,'serial':2})
        r=decoder._evaluate(a,b,set(),set());self.assertEqual(len(r[0]),91)
        self.assertEqual(r[0][0]['virtual_detail_group_count'],1)
        self.assertEqual(r[0][0]['zero_route_parent_invocations'],[{'thread_epoch':1,'serial':2}])

    def test_cross_lane_identity_including_empty_lane_checked(self):
        v=fixture();v[0]['threads'][0]['native_thread_id']=101
        for e in v[0]['threads'][0]['events']:
            e['meta']['native_thread_id']=101
            e['meta']['observed_child']['entry']['update']['native_thread_id']=101
            e['frame']['entry']['parent_entry']['update']['native_thread_id']=101
        self.reject(v)
        v=fixture();empty=copy.deepcopy(v[0]['threads'][0])
        empty.update(thread_epoch=2,native_thread_id=200,parent_detail_thread_epoch=0,parent_router_thread_epoch=0,
                     events=[],event_count=0,pending=[],pending_count=0)
        v[0]['threads'].append(empty);v[0]['counts']['thread_lifetimes']=2;self.reject(v)


if __name__=='__main__':unittest.main()
