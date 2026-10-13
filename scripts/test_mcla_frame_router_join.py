"""Synthetic metadata only; no native execution, file IO, or private fixtures."""
import copy
import hashlib
import unittest
from unittest import mock

import mcla_frame_router_join as decoder
from test_mcla_router_dispatch_decode import fixture as router_fixture
from test_mcla_update_timing_decode import fixture as timing_fixture
from test_mcla_phase_census_decode import raw, scope, recount
from test_mcla_game_source_scout_decode import TOKEN


def fixture():
    timing, hand, prefix, phase, swap, source = timing_fixture()
    router = router_fixture()[0]
    for event in router['threads'][0]['events']:
        for binding in (event['observed_binding'], event['frame']['entry_binding']):
            binding.update(phase_call_id=2, thread_epoch=7, native_thread_id=100)
    return router,timing,hand,prefix,phase,swap,source


def decode(values):
    return decoder.validate_frame_router_join(*(raw(v) for v in values[:6]),values[6],TOKEN)


def add_router(values):
    data=values[0];lane=data['threads'][0];new=copy.deepcopy(lane['events'])
    for e in new:
        e['frame']['invocation']['serial']=2;e['qpc']+=10
    lane['events']+=new;lane['event_count']+=len(new)
    data['counts']['observed_entries']+=1;data['counts']['admitted_entries']+=1


class FrameRouterTests(unittest.TestCase):
    def reject_before_join(self,values):
        with mock.patch.object(decoder,'_evaluate',side_effect=AssertionError('premature join')) as evaluate:
            with self.assertRaises(ValueError):decode(values)
        evaluate.assert_not_called()

    def test_complete_group_preserves_both_baselines_and_false_claims(self):
        values=fixture();add_router(values);before=copy.deepcopy(values)
        with mock.patch('builtins.open',side_effect=AssertionError('no IO')):result=decode(values)
        self.assertEqual(values,before)
        self.assertEqual(len(result['candidate_records']),91)
        self.assertEqual(len(result['router_update_groups']),1)
        group=result['router_update_groups'][0]
        self.assertEqual([r['invocation']['serial'] for r in group['invocations']],[1,2])
        for c in result['candidate_records']:
            self.assertEqual(c['router_group_count'],2)
            self.assertEqual(c['router_update_binding'],group['update_binding'])
            self.assertEqual(c['timing_observation_ids'],list(range(1,11)))
            self.assertNotIn('timer_observation_id',group['invocations'][0])
        self.assertEqual(result['router_analysis'],decoder.router_decoder.validate_router_dispatch(raw(values[0]),raw(values[4]),values[6],TOKEN))
        self.assertEqual(result['timing_frontier_analysis'],decoder.timing_decoder.validate_update_timing_frontier(
            *(raw(v) for v in values[1:6]),values[6],TOKEN))
        for key in decoder.FALSE_FLAGS:self.assertIs(result[key],False)
        self.assertNotIn('PASS',result['status'])
        for name,value in zip(('router','timing','handoff','prefix','phase','swap'),values):
            self.assertEqual(result['raw_sha256'][name],hashlib.sha256(raw(value)).hexdigest())
        group['invocations'][0]['frame']['consumer']['r3']=999
        self.assertEqual(result['router_analysis']['candidate_records'][0]['frame']['consumer']['r3'],12)
        result['candidate_records'][0]['router_update_binding']['phase_call_id']=999
        self.assertEqual(result['candidate_records'][1]['router_update_binding']['phase_call_id'],2)

    def test_foreign_id_thread_epoch_or_native_thread_rejects_before_evaluator(self):
        for key,value in (('phase_call_id',999),('thread_epoch',99),('native_thread_id',123)):
            v=fixture()
            for e in v[0]['threads'][0]['events']:
                e['observed_binding'][key]=value;e['frame']['entry_binding'][key]=value
            self.reject_before_join(v)

    def test_equal_clocks_different_valid_update_never_match(self):
        v=fixture();phase=v[4];phase['scopes'].append(scope(4,'update',phase['begin_qpc']+10,phase['begin_qpc']+100,epoch=8,tid=200));recount(phase)
        lane=v[0]['threads'][0];lane['native_thread_id']=200
        for e in lane['events']:
            e['native_thread_id']=200
            for b in (e['observed_binding'],e['frame']['entry_binding']):b.update(phase_call_id=4,thread_epoch=8,native_thread_id=200)
        result=decode(v)
        self.assertEqual(result['candidate_records'],[])
        self.assertEqual(len(result['excluded_candidate_records']),91)
        self.assertIn('NO_COMPLETE_ROUTER_INVOCATIONS_FOR_EXACT_UPDATE',result['retained_context_issues'])

    def test_duplicate_invocation_journal_cannot_be_counted_twice(self):
        v=fixture();add_router(v)
        for e in v[0]['threads'][0]['events'][6:]:e['frame']['invocation']['serial']=1
        self.reject_before_join(v)

    def test_pending_or_global_router_failure_blocks_every_source(self):
        v=fixture();lane=v[0]['threads'][0];lane['events']=lane['events'][:4]
        lane.update(event_count=4,pending_count=1,pending=[copy.deepcopy(lane['events'][-1]['frame'])])
        r=decode(v);self.assertEqual(r['candidate_records'],[])
        self.assertIn('router:PENDING_ROUTER_INVOCATIONS',r['candidate_blocking_issues'])
        for key in ('observer_failed','disarmed_at_cut'):
            v=fixture();v[0][key]=True;r=decode(v)
            self.assertEqual(r['candidate_records'],[]);self.assertIn('router:'+key.upper(),r['candidate_blocking_issues'])

    def test_timing_failure_and_pending_not_repaired_by_healthy_router(self):
        v=fixture();v[1]['observer_failed']=True
        r=decode(v);self.assertEqual(r['candidate_records'],[]);self.assertIn('timing:OBSERVER_FAILED',r['candidate_blocking_issues'])

    def test_all_unrelated_metadata_validated_before_new_evaluator(self):
        mutations=(lambda v:v[1]['records'][-1]['incoming'].update(f1_bits=True),
            lambda v:v[2]['scopes'][-1].update(phase_call_id=999),
            lambda v:v[3]['operations'][-1].update(op_id=999),
            lambda v:v[4]['swap_callbacks'][-1].update(callback_id=9999),
            lambda v:v[5]['records'][-1].update(slot=999),
            lambda v:v[0]['threads'][0]['events'][-1].update(qpc=True))
        for mutation in mutations:v=fixture();mutation(v);self.reject_before_join(v)

    def test_no_router_completion_is_explicit_not_invented_from_timing(self):
        v=fixture();r=v[0];r['threads']=[];r['counts']=dict.fromkeys(decoder.router_decoder.COUNTS,0)
        result=decode(v);self.assertEqual(result['candidate_records'],[])
        self.assertIn('router:EMPTY_ROUTER_INVENTORY',result['candidate_blocking_issues'])

    def analyses(self):
        v=fixture();add_router(v)
        router=decoder.router_decoder.validate_router_dispatch(raw(v[0]),raw(v[4]),v[6],TOKEN)
        timing=decoder.timing_decoder.validate_update_timing_frontier(*(raw(x) for x in v[1:6]),v[6],TOKEN)
        return router,timing

    def test_evaluator_rejects_equality_late_or_one_bad_invocation_without_filtering(self):
        # Focused defensive seam test: public API still accepts raw bytes only.
        for key in ('entry_qpc','consumer_pre_qpc','consumer_return_qpc','router_exit_qpc'):
            for delta in (0,1):
                router,timing=self.analyses();barrier=timing['closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive']
                router['candidate_records'][-1][key]=barrier+delta
                issues,contexts=set(),set();accepted,excluded=decoder._evaluate(router,timing,issues,contexts)
                self.assertEqual(accepted,[]);self.assertEqual(len(excluded),91)
                self.assertEqual(excluded[0]['matched_router_group_count'],2)
                self.assertIn('EXACT_UPDATE_ROUTER_GROUP_NOT_STRICTLY_BEFORE_FRONTIER',contexts)

    def test_evaluator_duplicate_id_rejected_even_if_bindings_differ(self):
        router,timing=self.analyses();router['candidate_records'][1]['invocation']=copy.deepcopy(router['candidate_records'][0]['invocation'])
        with self.assertRaises(ValueError):decoder._evaluate(router,timing,set(),set())

    def test_evaluator_exact_lifetime_and_tid_no_partial_identity_match(self):
        for key in ('timing_phase_thread_epoch','timing_native_thread_id'):
            router,timing=self.analyses();timing['candidate_records'][0][key]+=1
            accepted,excluded=decoder._evaluate(router,timing,set(),set())
            self.assertEqual(len(accepted),90);self.assertEqual(len(excluded),1)
            self.assertEqual(excluded[0]['reason'],'SELECTED_UPDATE_IDENTITY_OR_HEALTH_MISMATCH')

    def test_wrong_session_rejected_and_analysis_objects_not_accepted(self):
        v=fixture();v[0]['token']='1'*32;self.reject_before_join(v)
        v=fixture()
        with self.assertRaises(ValueError):decoder.validate_frame_router_join(v[0],*(raw(x) for x in v[1:6]),v[6],TOKEN)


if __name__=='__main__':unittest.main()
