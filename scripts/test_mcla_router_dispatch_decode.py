"""Synthetic scalar journals only: no native process, pixels or file writes."""
import copy
import unittest
from unittest import mock

import mcla_router_dispatch_decode as decoder
from test_mcla_phase_census_decode import fixture as phase_fixture, raw, scope, recount
from test_mcla_game_source_scout_decode import TOKEN


def fixture():
    phase, source = phase_fixture()
    phase['schema'] = 'mcla-phase-census-v2'
    for i, r in enumerate(phase['swap_callbacks']): r['callback_id'] = i+1
    binding = {'phase_call_id': 1, 'thread_epoch': 1, 'native_thread_id': 100}
    frame = {'invocation': {'thread_epoch': 1, 'serial': 1},
        'model_parent': {'thread_epoch': 0, 'serial': 0}, 'entry_binding': binding,
        'caller_lr': 0x822C24C4, 'manager_r3': 0xffffffff82900000,
        'selector_r4': 0xffffffff00000001, 'entry_f1_bits': 0x7ff80000deadbeef,
        'stage': 0, 'predicate': decoder._zero_dispatch(), 'consumer': decoder._zero_dispatch(),
        'predicate_result_r3': 0, 'predicate_result_low8': 0, 'predicate_returned': False}
    events = []
    def event(kind):
        e = dict.fromkeys(decoder.EVENT_FLAGS, True); e['parent_admitted'] = False
        e.update(event=kind, qpc=phase['begin_qpc']+20+len(events), native_thread_id=100,
                 observed_binding=copy.deepcopy(binding), frame=copy.deepcopy(frame))
        events.append(e)
    event(0)
    frame.update(stage=1, predicate=dict(r3=11, table=21, ctr=0xffffffff82200000,
        requested_target_u32=0x82200000, f1_bits=0, present=True)); event(1)
    frame.update(stage=2, predicate_returned=True, predicate_result_r3=0xffffffff00000080,
                 predicate_result_low8=128); event(2)
    frame.update(stage=4, consumer=dict(r3=12, table=22, ctr=0xffffffff82300000,
        requested_target_u32=0x82300000, f1_bits=0x8000000000000000, present=True)); event(5)
    frame['stage'] = 5; event(6); event(8)
    lane = dict(thread_epoch=1, native_thread_id=100, model_fault=0, event_count=len(events),
                pending_count=0, events=events, pending=[])
    data = dict.fromkeys(decoder.FALSE_FLAGS | decoder.HEALTH, False)
    data.update(schema='mcla-router-dispatch-v1', token=TOKEN, clock='win32-query-performance-counter',
        begin_qpc=phase['begin_qpc'], end_qpc_exclusive=phase['end_qpc_exclusive'],
        qpc_frequency=phase['qpc_frequency'], freeze_qpc=phase['end_qpc_exclusive']+10,
        thread_capacity=8, events_per_thread_capacity=16384, depth_capacity=8,
        counts=dict.fromkeys(decoder.COUNTS, 0), threads=[lane],
        binding_observed_before_event_qpc=True, unobserved_ancestry_possible=True)
    data['counts'].update(observed_entries=1, admitted_entries=1, thread_lifetimes=1)
    return data, phase, source


def decode(values):
    d, p, s = values
    return decoder.validate_router_dispatch(raw(d), raw(p), s, TOKEN)


class RouterDecodeTests(unittest.TestCase):
    def reject(self, values):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature candidates')) as evaluate:
            with self.assertRaises(ValueError): decode(values)
        evaluate.assert_not_called()

    def test_normal_exact_lowword_and_changed_objects_detached_no_io(self):
        values = fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('IO forbidden')):
            result = decode(values)
        self.assertEqual(values, before)
        self.assertEqual(len(result['candidate_records']), 1)
        c = result['candidate_records'][0]
        self.assertEqual(c['frame']['predicate']['r3'], 11)
        self.assertEqual(c['frame']['consumer']['r3'], 12)
        self.assertEqual(c['frame']['consumer']['f1_bits'], 0x8000000000000000)
        self.assertEqual(c['update_binding']['phase_call_id'], 1)
        for k in decoder.FALSE_FLAGS | {'actual_consumer_proven', 'causal_join_proven'}:
            self.assertIs(result[k], False)
        self.assertNotIn('PASS', result['status'])
        c['frame']['predicate']['r3'] = 99
        self.assertEqual(result['router_inventory']['threads'][0]['events'][1]['frame']['predicate']['r3'], 11)

    def test_skip_is_not_consumer_candidate(self):
        values = fixture(); lane = values[0]['threads'][0]
        lane['events'] = lane['events'][:3]
        f = lane['events'][2]['frame']; f.update(predicate_result_r3=0xffffffffffffff00, predicate_result_low8=0)
        skip = copy.deepcopy(lane['events'][-1]); skip.update(event=4, qpc=skip['qpc']+1); skip['frame']['stage']=3
        end=copy.deepcopy(skip); end.update(event=8,qpc=end['qpc']+1)
        lane['events'] += [skip,end];lane['event_count']=5
        result=decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertEqual(result['skipped_router_count'], 1)
        self.assertEqual(result['candidate_blocking_issues'], [])

    def test_pending_kept_without_backfill(self):
        for n in (1, 2, 3, 4, 5):
            v=fixture(); l=v[0]['threads'][0]; l['events']=l['events'][:n]
            l.update(event_count=n, pending_count=1, pending=[copy.deepcopy(l['events'][-1]['frame'])])
            r=decode(v);self.assertEqual(r['candidate_records'],[])
            self.assertIn('PENDING_ROUTER_INVOCATIONS',r['candidate_blocking_issues'])
            self.assertEqual(r['pending_router_count'],1)

    def test_exception_is_not_normal_dispatch(self):
        for pre, kind in ((2,3),(4,7)):
            v=fixture();l=v[0]['threads'][0];l['events']=l['events'][:pre]
            e=copy.deepcopy(l['events'][-1]);e.update(event=kind,qpc=e['qpc']+1);e['frame']['stage']=6
            x=copy.deepcopy(e);x.update(event=9,qpc=x['qpc']+1)
            l['events'] += [e,x];l['event_count']=len(l['events']);v[0]['counts']['original_exceptions']=1
            r=decode(v);self.assertEqual(r['candidate_records'],[]);self.assertIn('ORIGINAL_EXCEPTIONS',r['candidate_blocking_issues'])

    def test_false_completions_wrong_transitions_or_snapshot_reject(self):
        for mutate in (lambda e:e[1].update(event=6),lambda e:e[2]['frame'].update(predicate_result_low8=0),
            lambda e:e[3]['frame']['predicate'].update(r3=999),lambda e:e[5]['frame'].update(stage=3),
            lambda e:e[4]['frame']['entry_binding'].update(phase_call_id=2),
            lambda e:e[2]['frame']['consumer'].update(present=True)):
            v=fixture();mutate(v[0]['threads'][0]['events']);self.reject(v)

    def test_unknown_or_wrong_phase_lane_rejected(self):
        for key,value in (('phase_call_id',999),('phase_call_id',2),('thread_epoch',2),('native_thread_id',101)):
            v=fixture()
            for e in v[0]['threads'][0]['events']:
                e['frame']['entry_binding'][key]=value;e['observed_binding'][key]=value
            self.reject(v)

    def test_wrong_native_thread_or_false_metadata_rejected(self):
        for key,value in (('native_thread_id',101),('lane_matches',False),('lexical_top',False),
                          ('binding_matches',False),('expected_route',False),('parent_admitted',True)):
            v=fixture();v[0]['threads'][0]['events'][-1][key]=value;self.reject(v)

    def test_qpc_outside_update_regression_or_cut_rejected(self):
        for tick in (0, 2**63, -1):
            v=fixture();v[0]['threads'][0]['events'][-1]['qpc']=tick;self.reject(v)
        for offset in (9, 101):
            v=fixture();v[0]['threads'][0]['events'][-1]['qpc']=v[1]['begin_qpc']+offset;self.reject(v)
        v=fixture();v[0]['freeze_qpc']=v[1]['begin_qpc']+22;self.reject(v)

    def test_flagged_clock_fault_retained_no_candidates(self):
        v=fixture();v[0].update(clock_failed=True,observer_failed=True)
        v[0]['threads'][0]['events'][2].update(qpc=0,clock_valid=False)
        r=decode(v);self.assertEqual(r['candidate_records'],[]);self.assertIn('CLOCK_FAILED',r['candidate_blocking_issues'])

    def test_failed_cut_keeps_raw_valid_history_without_rebasing_or_candidates(self):
        for cut in (0, decoder.QMAX+1, fixture()[0]['begin_qpc']+22):
            with self.subTest(cut=cut):
                v=fixture();v[0].update(freeze_qpc=cut,clock_failed=True,observer_failed=True)
                before=copy.deepcopy(v);result=decode(v)
                self.assertEqual(v,before)
                self.assertEqual(result['router_inventory'],v[0])
                self.assertEqual(result['router_inventory']['freeze_qpc'],cut)
                self.assertEqual(result['candidate_records'],[])
                self.assertEqual(result['grouped_update_records'],[])
                self.assertIn('CLOCK_FAILED',result['candidate_blocking_issues'])
                self.assertIn('OBSERVER_FAILED',result['candidate_blocking_issues'])

    def test_failed_cut_requires_both_flags_and_does_not_relax_valid_event_clocks(self):
        for cut in (0, decoder.QMAX+1, fixture()[0]['begin_qpc']+22):
            for clock,observer in ((False,False),(False,True),(True,False)):
                v=fixture();v[0].update(freeze_qpc=cut,clock_failed=clock,observer_failed=observer)
                self.reject(v)
        for tick in (0,decoder.QMAX+1,fixture()[0]['begin_qpc']+19):
            v=fixture();v[0].update(freeze_qpc=0,clock_failed=True,observer_failed=True)
            v[0]['threads'][0]['events'][-1]['qpc']=tick
            self.reject(v)

    def test_global_faults_do_not_heal(self):
        for flag in decoder.HEALTH:
            v=fixture();v[0][flag]=True
            if flag in ('counter_saturated','clock_failed'):v[0]['observer_failed']=True
            r=decode(v);self.assertEqual(r['candidate_records'],[])
        v=fixture();v[0]['counts'].update(observation_errors=1);v[0]['observer_failed']=True
        self.assertEqual(decode(v)['candidate_records'],[])

    def test_unbound_and_unknown_routes_are_explicit_global(self):
        v=fixture();v[0]['counts'].update(unbound_entries=1,binding_mismatches=5);v[0]['observer_failed']=True
        for e in v[0]['threads'][0]['events']:
            e['observed_binding']=dict.fromkeys(decoder.BINDING,0)
            e['frame']['entry_binding']=dict.fromkeys(decoder.BINDING,0);e['binding_matches']=False
        r=decode(v);self.assertEqual(r['candidate_records'],[]);self.assertIn('UNBOUND_ENTRIES',r['candidate_blocking_issues'])
        v=fixture();v[0]['counts']['unexpected_route_entries']=1
        for e in v[0]['threads'][0]['events']:e['expected_route']=False;e['frame']['caller_lr']=123
        self.assertEqual(decode(v)['candidate_records'],[])

    def test_counts_caps_types_and_terminal_metadata_before_evaluation(self):
        mutations=(lambda d:d['counts'].update(admitted_entries=2),lambda d:d.update(thread_capacity=9),
            lambda d:d['threads'][0].update(event_count=5),lambda d:d['threads'][0]['events'][-1].update(qpc=True),
            lambda d:d['threads'][0]['events'][-1]['frame']['consumer'].update(requested_target_u32=0),
            lambda d:d['threads'][0]['events'][-1].update(extra=0),lambda d:d.update(native_admitted=True))
        for mutate in mutations:v=fixture();mutate(v[0]);self.reject(v)

    def test_model_failure_and_dropped_entries_remain_global(self):
        v=fixture();d=v[0];d['observer_failed']=True;d['counts'].update(observed_entries=2,dropped_entries=1)
        d['threads'][0]['model_fault']=1
        r=decode(v);self.assertEqual(r['candidate_records'],[]);self.assertIn('MODEL_FAULT',r['candidate_blocking_issues'])

    def test_pending_stack_corruption_rejected(self):
        v=fixture();l=v[0]['threads'][0];l['pending']=[copy.deepcopy(l['events'][0]['frame'])];l['pending_count']=1
        self.reject(v)

    def test_same_phase_lifetime_cannot_be_reused_by_second_router_lane(self):
        v=fixture();d=v[0];l=copy.deepcopy(d['threads'][0]);l['thread_epoch']=2
        for e in l['events']:e['frame']['invocation']['thread_epoch']=2
        d['threads'].append(l);d['counts'].update(thread_lifetimes=2,observed_entries=2,admitted_entries=2)
        self.reject(v)

    def test_nested_complete_dispatches_keep_distinct_lexical_ids(self):
        v=fixture();d=v[0];l=d['threads'][0];outer=l['events'];child=copy.deepcopy(outer)
        for e in child:
            e['frame']['invocation']['serial']=2
            e['frame']['model_parent']={'thread_epoch':1,'serial':1}
            e['parent_admitted']=True
        l['events']=outer[:2]+child+outer[2:]
        for i,e in enumerate(l['events']):e['qpc']=d['begin_qpc']+20+i
        l['event_count']=12;d['counts'].update(observed_entries=2,admitted_entries=2)
        result=decode(v)
        self.assertEqual(len(result['candidate_records']),2)
        self.assertEqual(len(result['grouped_update_records']),1)
        self.assertEqual(len(result['grouped_update_records'][0]['invocations']),2)
        self.assertEqual([r['invocation']['serial'] for r in result['candidate_records']],[2,1])
        self.assertEqual(result['candidate_records'][0]['frame']['model_parent']['serial'],1)

    def test_two_independent_thread_lifetimes_are_not_same_numeric_epoch_namespace(self):
        v=fixture();d,p,_=v;other=copy.deepcopy(d['threads'][0]);other.update(thread_epoch=2,native_thread_id=200)
        p['scopes'].append(scope(4,'update',p['begin_qpc']+10,p['begin_qpc']+100,epoch=17,tid=200));recount(p)
        for e in other['events']:
            e['native_thread_id']=200;e['frame']['invocation']['thread_epoch']=2
            for b in (e['observed_binding'],e['frame']['entry_binding']):
                b.update(phase_call_id=4,thread_epoch=17,native_thread_id=200)
        d['threads'].append(other);d['counts'].update(thread_lifetimes=2,observed_entries=2,admitted_entries=2)
        self.assertEqual(len(decode(v)['candidate_records']),2)

    def test_entry_without_dispatch_can_exceptionally_exit(self):
        v=fixture();d=v[0];l=d['threads'][0];end=copy.deepcopy(l['events'][0]);end.update(event=9,qpc=end['qpc']+1)
        l.update(events=[l['events'][0],end],event_count=2);d['counts']['original_exceptions']=1
        r=decode(v);self.assertEqual(r['completed_router_count'],1);self.assertEqual(r['candidate_records'],[])

    def test_null_base_or_phase_health_remains_global(self):
        v=fixture()
        for e in v[0]['threads'][0]['events']:e['base_nonnull']=False
        self.assertIn('NULL_BASE',decode(v)['candidate_blocking_issues'])
        v=fixture();v[1]['observer_failed']=True
        self.assertIn('phase:observer_failed',decode(v)['candidate_blocking_issues'])

    def test_returns_may_cross_observation_end_if_contained_in_phase_before_cut(self):
        v=fixture();d,p,_=v;p['scopes'][0]['exit_qpc']=d['end_qpc_exclusive']+4
        # Avoid impossible same-lane overlapping sibling phase scopes.
        p['scopes']=p['scopes'][:1];p['swap_callbacks']=[];recount(p)
        for i,e in enumerate(d['threads'][0]['events']):e['qpc']=d['end_qpc_exclusive']-2+i
        r=decode(v);self.assertEqual(r['completed_router_count'],1)
        self.assertIn('phase:empty_callback_inventory',r['candidate_blocking_issues'])

    def test_zero_event_failed_lane_is_retained_not_invented_entry(self):
        v=fixture();d=v[0];l=d['threads'][0];l.update(events=[],event_count=0,model_fault=1)
        d['observer_failed']=True;d['counts'].update(admitted_entries=0,dropped_entries=1)
        r=decode(v);self.assertEqual(r['candidate_records'],[]);self.assertIn('EMPTY_ROUTER_INVENTORY',r['candidate_blocking_issues'])

    def test_native_serial_gaps_and_false_capacity_causes_rejected(self):
        v=fixture()
        for e in v[0]['threads'][0]['events']:e['frame']['invocation']['serial']=2
        self.reject(v)
        for fault in (3,4):
            v=fixture();v[0]['observer_failed']=True;v[0]['threads'][0]['model_fault']=fault;self.reject(v)

    def test_phase_tail_and_callback_ids_validated_before_candidates(self):
        for mutate in (lambda p:p['swap_callbacks'][-1].update(callback_id=2),
                       lambda p:p['scopes'][-1].update(call_id=999)):
            v=fixture();mutate(v[1]);self.reject(v)

    def test_raw_json_limits_duplicates_float_and_session(self):
        d,p,s=fixture()
        for b in (b'',bytearray(raw(d)),b'{"a":1,"a":2}',b'{"a":1.2}',b'\xff',b'['*2000+b']'*2000):
            with self.assertRaises(ValueError):decoder.validate_router_dispatch(b,raw(p),s,TOKEN)
        d['token']='1'*32;self.reject((d,p,s))


if __name__=='__main__':unittest.main()
