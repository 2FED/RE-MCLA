"""Pure synthetic join regressions; no private artifacts or native actions."""
import copy
import unittest
from unittest import mock

import mcla_frame_consumer_join as decoder
from test_mcla_consumer_detail_decode import fixture as detail_fixture
from test_mcla_frame_router_join import fixture as router_fixture, add_router
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def fixture():
    values = router_fixture(); router=values[0]
    for e in router['threads'][0]['events']:
        if e['frame']['consumer']['present']:
            e['frame']['consumer'].update(ctr=0xffffffff822C7870, requested_target_u32=decoder.detail_decoder.TARGET)
    detail=detail_fixture(); lane=detail['threads'][0]; lane['router_thread_epoch']=1
    for key in ('begin_qpc','end_qpc_exclusive','qpc_frequency','freeze_qpc'):detail[key]=router[key]
    dispatch=router['threads'][0]['events'][-1]['frame']['consumer']
    binding=router['threads'][0]['events'][-1]['frame']['entry_binding']
    tick=router['threads'][0]['events'][3]['qpc']
    for e in lane['events']:
        en=e['frame']['entry'];en.update(router={'thread_epoch':1,'serial':1},update=copy.deepcopy(binding),
            r3=dispatch['r3'],f1_bits=dispatch['f1_bits'])
        a=e['meta']['observed_consumer'];a.update(invocation=copy.deepcopy(en['router']),update=copy.deepcopy(binding),
            r3=en['r3'],f1_bits=en['f1_bits'],ctr=dispatch['ctr'])
        e['meta']['qpc']=tick
    return (detail,)+values


def decode(values):
    return decoder.validate_frame_consumer_join(*(raw(v) for v in values[:7]),values[7],TOKEN)


def add_detail(values):
    data=values[0];lane=data['threads'][0];extra=copy.deepcopy(lane['events'])
    for e in extra:
        e['frame']['entry']['invocation']['serial']=2;e['frame']['entry']['router']['serial']=2
        e['meta']['observed_consumer']['invocation']['serial']=2;e['meta']['qpc']+=10
    lane['events']+=extra;lane['event_count']+=len(extra)
    data['counts']['observed_entries']+=1;data['counts']['admitted_entries']+=1


class FrameConsumerTests(unittest.TestCase):
    def test_healthy_cut_order_required_failed_clock_retained(self):
        v=fixture();v[0]['freeze_qpc']=v[1]['freeze_qpc']+1
        with self.assertRaisesRegex(ValueError, 'consumer cut precedes router cut'):decode(v)
        v[0]['clock_failed']=True;v[0]['observer_failed']=True
        self.assertEqual(decode(v)['candidate_records'],[])

    def reject(self,values):
        with mock.patch.object(decoder,'_evaluate',side_effect=AssertionError('premature join')) as evaluate:
            with self.assertRaises(ValueError):decode(values)
        evaluate.assert_not_called()

    def test_complete_exact_chain_immutable_detached_noio_falseproofs(self):
        v=fixture();before=copy.deepcopy(v)
        with mock.patch('builtins.open',side_effect=AssertionError('no IO')):r=decode(v)
        self.assertEqual(v,before);self.assertEqual(len(r['candidate_records']),91)
        self.assertEqual(r['candidate_records'][0]['consumer_detail_invocations'],[{'thread_epoch':1,'serial':1}])
        self.assertEqual(r['consumer_analysis'],decoder.detail_decoder.validate_consumer_detail(raw(v[0]),TOKEN))
        self.assertEqual(r['frame_router_analysis'],decoder.router_join.validate_frame_router_join(
            *(raw(x) for x in v[1:7]),v[7],TOKEN))
        for key in decoder.FALSE_FLAGS:self.assertIs(r[key],False)
        self.assertNotIn('PASS',r['status']);self.assertNotIn('delta_sum',r)
        r['candidate_records'][0]['consumer_detail_invocations'][0]['serial']=9
        self.assertEqual(r['candidate_records'][1]['consumer_detail_invocations'][0]['serial'],1)

    def test_missing_one_of_two_targets_excludes_entire_group(self):
        v=fixture();add_router(v[1:]);r=decode(v)
        self.assertEqual(r['candidate_records'],[]);self.assertEqual(len(r['excluded_candidate_records']),91)
        self.assertEqual(r['excluded_candidate_records'][0]['expected_target_router_count'],2)

    def test_two_complete_details_then_one_bad_relation_rejects_whole_group(self):
        v=fixture();add_router(v[1:]);add_detail(v)
        r=decode(v);self.assertEqual(len(r['candidate_records']),91)
        self.assertEqual(r['candidate_records'][0]['consumer_detail_group_count'],2)
        self.assertEqual([x['serial'] for x in r['candidate_records'][0]['consumer_detail_invocations']],[1,2])
        for e in v[0]['threads'][0]['events'][9:]:
            e['frame']['entry']['r3']+=1;e['meta']['observed_consumer']['r3']+=1
        r=decode(v);self.assertEqual(r['candidate_records'],[])
        self.assertEqual(r['excluded_candidate_records'][0]['expected_target_router_count'],2)

    def test_other_target_kept_unknown_not_target_coverage(self):
        v=fixture();add_router(v[1:])
        for e in v[1]['threads'][0]['events'][6:]:
            if e['frame']['consumer']['present']:e['frame']['consumer'].update(ctr=0xffffffff82300000,requested_target_u32=0x82300000)
        r=decode(v);self.assertEqual(len(r['candidate_records']),91)
        self.assertEqual(r['candidate_records'][0]['unknown_router_invocations'],[{'thread_epoch':1,'serial':2}])
        self.assertIn('OTHER_ROUTER_TARGETS_RETAINED_UNKNOWN',r['retained_context_issues'])

    def test_no_target_and_orphan_detail_not_vacuous_success(self):
        v=fixture()
        for e in v[1]['threads'][0]['events']:
            if e['frame']['consumer']['present']:e['frame']['consumer'].update(ctr=0xffffffff82300000,requested_target_u32=0x82300000)
        r=decode(v);self.assertEqual(r['candidate_records'],[])
        self.assertIn('EXTRANEOUS_DETAIL_WITHOUT_MATCHING_TARGET_ROUTER',r['candidate_blocking_issues'])

    def test_duplicate_actual_entry_for_router_rejected_before_join(self):
        v=fixture();add_router(v[1:]);add_detail(v)
        for e in v[0]['threads'][0]['events'][9:]:
            e['frame']['entry']['router']['serial']=1;e['meta']['observed_consumer']['invocation']['serial']=1
        self.reject(v)

    def test_saved_operands_match_full_bits_not_float_or_lowword(self):
        for key in ('r3','f1_bits','ctr'):
            v=fixture()
            for e in v[0]['threads'][0]['events']:
                e['meta']['observed_consumer'][key]^=1<<40
                if key!='ctr':e['frame']['entry'][key]^=1<<40
            r=decode(v);self.assertEqual(r['candidate_records'],[])
            self.assertEqual(len(r['excluded_candidate_records']),91)

    def test_exact_update_epoch_tid_and_router_id_not_nearest_time(self):
        for key in ('phase_call_id','thread_epoch'):
            v=fixture()
            for e in v[0]['threads'][0]['events']:
                e['frame']['entry']['update'][key]+=1;e['meta']['observed_consumer']['update'][key]+=1
            self.assertEqual(decode(v)['candidate_records'],[])
        v=fixture();lane=v[0]['threads'][0];lane['native_thread_id']=200
        for e in lane['events']:
            e['meta']['native_thread_id']=200;e['meta']['observed_consumer']['update']['native_thread_id']=200
            e['frame']['entry']['update']['native_thread_id']=200
        self.assertEqual(decode(v)['candidate_records'],[])
        v=fixture()
        for e in v[0]['threads'][0]['events']:
            e['frame']['entry']['router']['serial']=2;e['meta']['observed_consumer']['invocation']['serial']=2
        self.assertEqual(decode(v)['candidate_records'],[])

    def test_all_detail_events_inside_dispatch_not_just_entry_exit(self):
        for delta in (-1,2):
            v=fixture()
            for e in v[0]['threads'][0]['events']:e['meta']['qpc']+=delta
            r=decode(v);self.assertEqual(r['candidate_records'],[])
            self.assertIn('DETAIL_EVENTS_OUTSIDE_ENCLOSING_DISPATCH_CUT_OR_FRONTIER',r['retained_context_issues'])

    def test_pending_global_failure_and_rejected_damage_never_repaired(self):
        for flag in ('observer_failed','disarmed_at_cut'):
            v=fixture();v[0][flag]=True;r=decode(v);self.assertEqual(r['candidate_records'],[])
        v=fixture();lane=v[0]['threads'][0];lane['events'].pop();lane['event_count']-=1
        lane.update(pending_count=1,pending=[copy.deepcopy(lane['events'][-1]['frame'])])
        self.assertEqual(decode(v)['candidate_records'],[])
        v=fixture();v[2]['observer_failed']=True;self.assertEqual(decode(v)['candidate_records'],[])

    def test_shared_window_frequency_and_token_required(self):
        for key in ('begin_qpc','end_qpc_exclusive','qpc_frequency'):
            v=fixture();v[0][key]+=1;self.reject(v)
        v=fixture();v[0]['token']='1'*32;self.reject(v)

    def test_all_raw_metadata_validated_before_evaluation(self):
        for edit in (lambda v:v[0]['threads'][0]['events'][-1]['meta'].update(qpc=True),
            lambda v:v[2]['records'][-1]['incoming'].update(f1_bits=True),
            lambda v:v[6]['records'][-1].update(slot=999)):
            v=fixture();edit(v);self.reject(v)
        v=fixture()
        with self.assertRaises(ValueError):decoder.validate_frame_consumer_join(v[0],*(raw(x) for x in v[1:7]),v[7],TOKEN)

    def test_evaluator_rejects_cut_frontier_and_duplicates(self):
        v=fixture();detail=decoder.detail_decoder.validate_consumer_detail(raw(v[0]),TOKEN)
        joined=decoder.router_join.validate_frame_router_join(*(raw(x) for x in v[1:7]),v[7],TOKEN)
        tick=detail['completed_invocations'][0]['events'][0]['meta']['qpc']
        for which in ('detail','router','frontier'):
            d,j=copy.deepcopy(detail),copy.deepcopy(joined)
            if which=='detail':d['consumer_detail_inventory']['freeze_qpc']=tick-1
            elif which=='router':j['router_analysis']['router_inventory']['freeze_qpc']=tick-1
            else:j['timing_frontier_analysis']['closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive']=tick
            a,x,_=decoder._evaluate(d,j,set(),set());self.assertEqual(a,[]);self.assertEqual(len(x),91)
        d=copy.deepcopy(detail);d['completed_invocations'].append(copy.deepcopy(d['completed_invocations'][0]))
        with self.assertRaises(ValueError):decoder._evaluate(d,joined,set(),set())

    def test_unselected_relation_cannot_hide_mismatch(self):
        v=fixture();detail=decoder.detail_decoder.validate_consumer_detail(raw(v[0]),TOKEN)
        joined=decoder.router_join.validate_frame_router_join(*(raw(x) for x in v[1:7]),v[7],TOKEN)
        joined['candidate_records']=[]
        detail['completed_invocations'][0]['frame']['entry']['r3']+=1
        issues=set();accepted,_,_=decoder._evaluate(detail,joined,issues,set())
        self.assertEqual(accepted,[]);self.assertIn('UNSELECTED_DETAIL_ROUTER_RELATION_INVALID',issues)


if __name__=='__main__':unittest.main()
