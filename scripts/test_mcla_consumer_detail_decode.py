"""Pure synthetic scalar fixtures; never native execution, IO or private data."""
import copy
import unittest
from unittest import mock

import mcla_consumer_detail_decode as decoder
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def fixture(nonzero=True):
    entry=dict(invocation={'thread_epoch':1,'serial':1},router={'thread_epoch':17,'serial':20},
        update={'phase_call_id':99,'thread_epoch':23,'native_thread_id':100},actual_target=decoder.TARGET,
        caller_lr=decoder.CALLER,r3=0xffffffff82900000,f1_bits=0x7ff80000deadbeef)
    access=dict(status=9,invocation=copy.deepcopy(entry['router']),update=copy.deepcopy(entry['update']),
        r3=entry['r3'],f1_bits=entry['f1_bits'],ctr=0xffffffff00000000|decoder.TARGET,requested_target_u32=decoder.TARGET)
    frame=dict(entry=entry,model_parent={'thread_epoch':0,'serial':0},stage=0,branch_r11=0,
        branch_observed=False,branch_nonzero=False,returned_children=0,children=[decoder._zero_child() for _ in range(3)])
    events=[]
    def append(kind):
        meta=dict.fromkeys(decoder.META_FLAGS,True);meta['parent_admitted']=False
        meta.update(qpc=110+len(events),native_thread_id=100,observed_consumer=copy.deepcopy(access))
        events.append(dict(event=kind,meta=meta,frame=copy.deepcopy(frame)))
    append(0)
    frame.update(stage=1,branch_r11=0xffffffff00000100 if nonzero else 0xffffffff00000000,
                 branch_observed=True,branch_nonzero=nonzero);append(1)
    if nonzero:
        for index in range(3):
            frame['children'][index]=dict(caller_lr=decoder.CHILD_LRS[index],r3=10+index,
                f1_bits=0x8000000000000000 if index==1 else 0x7ff8000012345678,
                indirect_ctr=0xaabbccdd82300000 if index==0 else 0,
                requested_target=0x82300000 if index==0 else decoder.DIRECT_TARGETS[index],state=1,indirect=index==0)
            frame['stage']=2;append(2)
            frame['children'][index]['state']=2;frame.update(stage=3,returned_children=index+1);append(3)
    append(5)
    lane=dict(thread_epoch=1,native_thread_id=100,router_thread_epoch=17,model_fault=0,
        event_count=len(events),pending_count=0,events=events,pending=[])
    data=dict.fromkeys(decoder.FALSE_FLAGS|decoder.HEALTH,False)
    data.update(schema='mcla-consumer-detail-v1',token=TOKEN,clock='win32-query-performance-counter',
        begin_qpc=100,end_qpc_exclusive=200,qpc_frequency=1000,freeze_qpc=210,
        thread_capacity=8,events_per_thread_capacity=16384,depth_capacity=8,rejected_entry_capacity=256,
        counts=dict.fromkeys(decoder.COUNTS,0),threads=[lane],rejected_entries=[],
        binding_observed_before_event_qpc=True,unobserved_ancestry_possible=True)
    data['counts'].update(observed_entries=1,admitted_entries=1,thread_lifetimes=1)
    return data


def decode(data):return decoder.validate_consumer_detail(raw(data),TOKEN)


def rejection(reason):
    d=fixture();e=copy.deepcopy(d['threads'][0]['events'][0]['frame']['entry']);e['invocation']={'thread_epoch':0,'serial':0}
    m=copy.deepcopy(d['threads'][0]['events'][0]['meta']);m.update(qpc=120,binding_matches=False,lane_matches=False)
    if reason==0:m.update(qpc=0,clock_valid=False)
    elif reason==1:m['base_nonnull']=False
    elif reason==2:e['r3']+=1
    elif reason in (3,4,5):
        m['binding_matches']=True;m['lane_matches']=reason>=4
        if reason==5:e['invocation']={'thread_epoch':1,'serial':2}
    return dict(reason=reason,entry=e,meta=m)


class ConsumerDetailTests(unittest.TestCase):
    def reject(self,data):
        with mock.patch.object(decoder,'_evaluate',side_effect=AssertionError('premature candidates')) as evaluate:
            with self.assertRaises(ValueError):decode(data)
        evaluate.assert_not_called()

    def test_both_complete_routes_detached_noio_and_no_proof(self):
        for nonzero in (False,True):
            d=fixture(nonzero);before=copy.deepcopy(d)
            with mock.patch('builtins.open',side_effect=AssertionError('no IO')):r=decode(d)
            self.assertEqual(d,before);self.assertEqual(len(r['candidate_records']),1)
            f=r['candidate_records'][0]['frame'];self.assertEqual(f['branch_nonzero'],nonzero)
            self.assertEqual(f['returned_children'],3 if nonzero else 0)
            for k in decoder.FALSE_FLAGS:self.assertIs(r[k],False)
            self.assertNotIn('PASS',r['status'])
            f['entry']['r3']=99;self.assertNotEqual(r['consumer_detail_inventory']['threads'][0]['events'][0]['frame']['entry']['r3'],99)
        d=decode(fixture())['candidate_records'][0]['frame']
        self.assertEqual(d['children'][0]['indirect_ctr'],0xaabbccdd82300000)
        self.assertEqual(d['children'][1]['f1_bits'],0x8000000000000000)
        self.assertEqual(d['children'][2]['f1_bits'],0x7ff8000012345678)

    def test_pending_at_every_stage_retained_no_completion(self):
        for n in range(1,9):
            d=fixture();l=d['threads'][0];l['events']=l['events'][:n]
            l.update(event_count=n,pending_count=1,pending=[copy.deepcopy(l['events'][-1]['frame'])])
            r=decode(d);self.assertEqual(r['candidate_records'],[]);self.assertEqual(r['pending_invocations'],1)
            self.assertIn('PENDING_DETAIL_INVOCATIONS',r['candidate_blocking_issues'])

    def test_exception_before_branch_or_earlier_unobserved_call(self):
        for n in (1,2):
            d=fixture();l=d['threads'][0];l['events']=l['events'][:n]
            e=copy.deepcopy(l['events'][-1]);e['event']=6;e['meta']['qpc']+=1
            l['events'].append(e);l['event_count']=n+1;d['counts']['original_exceptions']=1
            r=decode(d);self.assertEqual(r['candidate_records'],[])
            self.assertFalse(r['completed_invocations'][0]['normal_return'])
            self.assertTrue(all(c['state']==0 for c in r['completed_invocations'][0]['frame']['children']))

    def test_each_child_exception_preserved(self):
        for child in range(3):
            d=fixture();l=d['threads'][0];l['events']=l['events'][:3+2*child]
            e=copy.deepcopy(l['events'][-1]);e['event']=4;e['meta']['qpc']+=1
            e['frame']['stage']=4;e['frame']['children'][child]['state']=3
            end=copy.deepcopy(e);end['event']=6;end['meta']['qpc']+=1
            l['events'] += [e,end];l['event_count']=len(l['events']);d['counts']['original_exceptions']=1
            self.assertEqual(decode(d)['candidate_records'],[])

    def test_nested_children_and_exact_parent(self):
        d=fixture();l=d['threads'][0];outer=l['events'];inner=copy.deepcopy(fixture(False)['threads'][0]['events'])
        for e in inner:
            e['frame']['entry']['invocation']['serial']=2;e['frame']['entry']['router']['serial']=21
            e['meta']['observed_consumer']['invocation']['serial']=21
            e['frame']['model_parent']={'thread_epoch':1,'serial':1};e['meta']['parent_admitted']=True
        l['events']=outer[:3]+inner+outer[3:]
        for i,e in enumerate(l['events']):e['meta']['qpc']=110+i
        l['event_count']=len(l['events']);d['counts'].update(observed_entries=2,admitted_entries=2)
        r=decode(d);self.assertEqual([c['invocation']['serial'] for c in r['candidate_records']],[2,1])

    def test_sequential_reused_router_id_cannot_be_admitted(self):
        for detail_serial,router_serial in ((2,20),(1,21),(2,19)):
            d=fixture();l=d['threads'][0];extra=copy.deepcopy(l['events'])
            for e in extra:
                e['meta']['qpc']+=20;e['frame']['entry']['invocation']['serial']=detail_serial
                e['frame']['entry']['router']['serial']=router_serial;e['meta']['observed_consumer']['invocation']['serial']=router_serial
            l['events']+=extra;l['event_count']+=len(extra);d['counts'].update(observed_entries=2,admitted_entries=2)
            self.reject(d)

    def test_second_thread_independent_namespaces_and_reuse_rejection(self):
        d=fixture();l=copy.deepcopy(d['threads'][0]);l.update(thread_epoch=2,native_thread_id=200,router_thread_epoch=18)
        for e in l['events']:
            e['frame']['entry']['invocation']['thread_epoch']=2;e['frame']['entry']['router']['thread_epoch']=18
            e['frame']['entry']['update'].update(thread_epoch=24,native_thread_id=200)
            e['meta']['native_thread_id']=200;e['meta']['observed_consumer']['invocation']['thread_epoch']=18
            e['meta']['observed_consumer']['update'].update(thread_epoch=24,native_thread_id=200)
        d['threads'].append(l);d['counts'].update(observed_entries=2,admitted_entries=2,thread_lifetimes=2)
        self.assertEqual(len(decode(d)['candidate_records']),2)
        l['router_thread_epoch']=17;self.reject(d)

    def test_unknown_target_or_caller_or_wrong_thread_rejected(self):
        for key,value in (('actual_target',1),('caller_lr',0x822C5304)):
            d=fixture()
            for e in d['threads'][0]['events']:e['frame']['entry'][key]=value
            self.reject(d)
        d=fixture();d['threads'][0]['events'][-1]['meta']['native_thread_id']=999;self.reject(d)

    def test_branch_low32_and_all_snapshot_transitions_validated(self):
        edits=(lambda e:e[1]['frame'].update(branch_nonzero=False),lambda e:e[1]['frame'].update(branch_r11=0xffffffff00000000),
            lambda e:e[3]['frame'].update(returned_children=2),lambda e:e[-1]['frame']['entry'].update(r3=2),
            lambda e:e[3].update(event=5),lambda e:e[4]['frame']['children'][0].update(r3=123))
        for edit in edits:d=fixture();edit(d['threads'][0]['events']);self.reject(d)

    def test_child_route_targets_not_derived_from_ctr_for_direct_calls(self):
        for child,key,value in ((0,'requested_target',0),(1,'indirect_ctr',99),(1,'requested_target',0x82300000),
                                (2,'caller_lr',decoder.CHILD_LRS[1]),(2,'indirect',True)):
            d=fixture();d['threads'][0]['events'][-1]['frame']['children'][child][key]=value;self.reject(d)

    def test_valid_accessor_saved_operand_change_rejected(self):
        for key in ('r3','f1_bits','ctr'):
            d=fixture();d['threads'][0]['events'][-1]['meta']['observed_consumer'][key]^=1<<40;self.reject(d)
            d['threads'][0]['events'][-1]['meta']['binding_matches']=False
            d['observer_failed']=True;d['counts']['binding_mismatches']=1
            self.assertEqual(decode(d)['candidate_records'],[])

    def test_rejected_model_attempt_binds_existing_failed_lane_and_monotonic_id(self):
        base=fixture();base['observer_failed']=True;base['threads'][0]['model_fault']=1
        base['rejected_entries']=[rejection(5)]
        base['counts'].update(observed_entries=2,dropped_entries=1,rejected_entries=1)
        for edit in (lambda d:d['threads'][0].update(model_fault=0),
                     lambda d:d['rejected_entries'][0]['entry']['invocation'].update(thread_epoch=2),
                     lambda d:d['threads'][0].update(native_thread_id=101)):
            d=copy.deepcopy(base);edit(d);self.reject(d)
        d=copy.deepcopy(base);extra=copy.deepcopy(d['rejected_entries'][0])
        extra['entry']['invocation']['serial']=3;d['rejected_entries'].append(extra)
        d['counts'].update(observed_entries=3,dropped_entries=2,rejected_entries=2)
        self.assertEqual(decode(d)['candidate_records'],[])
        d['rejected_entries'].reverse();self.reject(d)

    def test_failed_empty_lane_remains_explicit_without_fabricated_router_epoch(self):
        d=fixture();lane=d['threads'][0]
        lane.update(events=[],event_count=0,router_thread_epoch=0,model_fault=1)
        d['observer_failed']=True;d['rejected_entries']=[rejection(5)]
        d['rejected_entries'][0]['entry']['invocation']['serial']=1
        d['counts'].update(admitted_entries=0,dropped_entries=1,rejected_entries=1)
        result=decode(d)
        self.assertEqual(result['candidate_records'],[])
        self.assertIn('EMPTY_DETAIL_INVENTORY',result['candidate_blocking_issues'])

    def test_invalid_accessor_status_clears_all_fields_and_blocks(self):
        d=fixture();m=d['threads'][0]['events'][-1]['meta'];m['binding_matches']=False
        m['observed_consumer']=dict(status=3,invocation={'thread_epoch':0,'serial':0},
            update={'phase_call_id':0,'thread_epoch':0,'native_thread_id':0},r3=0,f1_bits=0,ctr=0,requested_target_u32=0)
        d['observer_failed']=True;d['counts']['binding_mismatches']=1
        self.assertEqual(decode(d)['candidate_records'],[])
        m['observed_consumer']['r3']=1;self.reject(d)

    def test_valid_clock_bad_cut_retained_without_rebasing(self):
        for cut in (0,115,decoder.QMAX+1):
            d=fixture();d.update(freeze_qpc=cut,clock_failed=True,observer_failed=True)
            r=decode(d);self.assertEqual(r['consumer_detail_inventory']['freeze_qpc'],cut);self.assertEqual(r['candidate_records'],[])
            d['clock_failed']=False;self.reject(d)

    def test_valid_clock_still_rejects_range_regression_without_bad_cut_excuse(self):
        for tick in (0,109,decoder.QMAX+1):
            d=fixture();d.update(freeze_qpc=0,clock_failed=True,observer_failed=True)
            d['threads'][0]['events'][-1]['meta']['qpc']=tick;self.reject(d)
        d=fixture();d['threads'][0]['events'][-1]['meta'].update(qpc=0,clock_valid=False)
        self.reject(d);d.update(clock_failed=True,observer_failed=True)
        self.assertEqual(decode(d)['candidate_records'],[])

    def test_all_rejection_reasons_are_preserved_globally_blocked(self):
        for reason in range(6):
            d=fixture();d['rejected_entries']=[rejection(reason)];d['counts'].update(observed_entries=2,dropped_entries=1,rejected_entries=1)
            d['observer_failed']=True
            if reason==0:d['clock_failed']=True
            if reason==5:d['threads'][0]['model_fault']=1
            r=decode(d);self.assertEqual(r['candidate_records'],[]);self.assertEqual(r['consumer_detail_inventory']['rejected_entries'],d['rejected_entries'])

    def test_rejection_reason_contradictions_and_counters_rejected(self):
        for reason in range(6):
            d=fixture();d['observer_failed']=True;bad=rejection(reason);d['rejected_entries']=[bad]
            d['counts'].update(observed_entries=2,dropped_entries=1,rejected_entries=1)
            if reason==0:d['clock_failed']=True;bad['meta']['clock_valid']=True
            elif reason==1:bad['meta']['base_nonnull']=True
            elif reason==2:bad['entry']['r3']=bad['meta']['observed_consumer']['r3']
            else:bad['meta']['binding_matches']=False
            self.reject(d)
        d=fixture();d['counts']['rejection_overflow']=1;self.reject(d)

    def test_pending_stack_and_model_capacity_fault_rejected_if_fabricated(self):
        d=fixture();l=d['threads'][0];l['pending']=[copy.deepcopy(l['events'][0]['frame'])];l['pending_count']=1;self.reject(d)
        for fault in (3,4):
            d=fixture();d['observer_failed']=True;d['threads'][0]['model_fault']=fault;self.reject(d)

    def test_entire_tail_validates_before_evaluator(self):
        for edit in (lambda d:d['threads'][0]['events'][-1]['meta'].update(qpc=True),
            lambda d:d['threads'][0]['events'][-1].update(extra=0),lambda d:d['counts'].update(admitted_entries=2),
            lambda d:d.update(native_admitted=True),lambda d:d.update(thread_capacity=9)):
            d=fixture();edit(d);self.reject(d)

    def test_raw_bounds_duplicate_keys_float_session(self):
        for bad in (b'',bytearray(raw(fixture())),b'{"a":1,"a":2}',b'{"a":1.5}',b'\xff',b'['*2000+b']'*2000):
            with self.assertRaises(ValueError):decoder.validate_consumer_detail(bad,TOKEN)
        d=fixture();d['token']='1'*32;self.reject(d)


if __name__=='__main__':unittest.main()
