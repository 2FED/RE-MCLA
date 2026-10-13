"""Synthetic compact-journal regressions only; no native execution or IO."""
import copy
import unittest
from unittest import mock

import mcla_work_emission_decode as decoder
from test_mcla_virtual_detail_decode import fixture as virtual_fixture
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def trace(branch='vector'):
    scalar = [(0, []), (1, [1]), (2, [10]),
        (3, [decoder.SITES[3], 8, 3, 0x7ff8000012345678, 0x8000000000000000, 0xffffffff00000000]),
        (4, [0x101]), (3, [decoder.SITES[3], 2, 3, 4, 5, 6]), (4, [0x100]),
        (6, [decoder.SITES[6], 9, 0xffffffffDEADBEEF]), (7, [])]
    if branch == 'zero': return [(0, []), (1, [0]), (22, [])]
    if branch == 'job':
        return [(0, []), (1, [1]), (2, [0]), (9, [0]), (10, [1]),
            (11, [decoder.SITES[11], 0, 3, 4, 5, 6, 7, 8, 0x7fc01234]),
            (12, [0xffffffffffffffff]), (14, [3]),
            (11, [decoder.SITES[11], 1, 3, 4, 5, 6, 7, 8, 0x80000000]),
            (12, [9]), (14, [1]), (15, [decoder.SITES[15], 1, 4, 1]), (16, []), (22, [])]
    return scalar + [(9, [255]), (18, [2]),
        (19, [decoder.SITES[19], 1, 3, 0x7ff0000000000001, 0xffffffffDEADBEEF]), (20, []),
        (19, [decoder.SITES[19], 0, 3, 0x8000000000000000, 0xaabbccdd00000000]), (20, []), (22, [])]


def fixture(branch='vector'):
    old = virtual_fixture(); v = copy.deepcopy(old['threads'][0]['events'][0]['frame']['entry'])
    v['invocation'] = dict(thread_epoch=7, serial=11)
    v['parent_entry']['invocation'] = dict(thread_epoch=9, serial=21)
    call = dict(caller_lr=decoder.CALLER, r3=0xffffffffBDF9D620,
        f1_bits=0x8000000000000000, indirect_ctr=0xaabbccdd82741010,
        requested_target=decoder.TAIL, state=1)
    entry = dict(invocation=dict(thread_epoch=1, serial=1), virtual_entry=v, active_call0=call,
        actual_target=decoder.BODY, caller_lr=decoder.CALLER, r3=call['r3'], f1_bits=call['f1_bits'])
    events = []
    for k, fields in trace(branch):
        meta = dict.fromkeys(decoder.META_FLAGS, True); meta.update(parent_admitted=False,
            tail_fresh=k == 0, qpc=110+len(events), native_thread_id=100, accessor_status=8)
        events.append(dict(invocation=copy.deepcopy(entry['invocation']), entry_index=0,
            event=k, fields=fields+[0]*(10-len(fields)),
            vector=[0x7fc01234, 0x80000000, 0xffffffff, 0] if k == 19 else [0]*4, meta=meta))
    data = dict.fromkeys(decoder.FALSE_FLAGS, False)
    data.update(dict.fromkeys(decoder.HEALTH, False)); data.update(dict.fromkeys(decoder.LIMITATIONS, True))
    data.update(schema='mcla-work-emission-v1', token=TOKEN, clock='win32-query-performance-counter',
        begin_qpc=100, end_qpc_exclusive=250, qpc_frequency=100, freeze_qpc=1000,
        thread_capacity=8, entries_per_thread_capacity=2048, events_per_thread_capacity=65536,
        depth_capacity=8, rejected_entry_capacity=256, json_limit_bytes=decoder.MAX_BYTES,
        vector_lane_order='PPCContext.v1.u32[0,1,2,3]_HOST_STORAGE_ORDER_NO_BYTE_SWAP',
        counts=dict.fromkeys(decoder.COUNTS, 0), rejected_entries=[])
    data['counts'].update(observed_entries=1, admitted_entries=1, thread_lifetimes=1, tail_scopes=1)
    data['threads'] = [dict(thread_epoch=1, native_thread_id=100, parent_virtual_thread_epoch=7,
        parent_detail_thread_epoch=9, parent_router_thread_epoch=v['parent_entry']['router']['thread_epoch'],
        model_fault=0, entry_count=1, event_count=len(events), pending_count=0,
        entries=[entry], events=events, pending=[])]
    return data


def decode(d): return decoder.validate_work_emission(raw(d), TOKEN)


def prefix(d, n):
    lane = d['threads'][0]; lane['events'] = lane['events'][:n]; lane['event_count'] = n
    f = decoder._new_frame(0, dict(thread_epoch=0, serial=0))
    for ev in lane['events'][1:]: decoder._step(f, ev)
    lane.update(pending_count=1, pending=[f])
    return d


def rejection(d, reason):
    lane = d['threads'][0]; e, m = copy.deepcopy(lane['entries'][0]), copy.deepcopy(lane['events'][0]['meta'])
    e['invocation'] = dict(thread_epoch=0, serial=0)
    m.update(qpc=200, binding_matches=False, lane_matches=False, parent_admitted=False)
    if reason == 0: m.update(qpc=0, clock_valid=False)
    if reason == 1: m['tail_fresh'] = False
    if reason == 2: e['actual_target'] = 0
    if reason >= 3: m.update(binding_matches=True, lane_matches=reason >= 4)
    if reason == 5: e['invocation'] = dict(thread_epoch=1, serial=2); lane['model_fault'] = 1
    d['rejected_entries'] = [dict(reason=reason, entry=e, meta=m)]
    d['counts'].update(observed_entries=2, dropped_entries=1, rejected_entries=1, observation_errors=1, tail_scopes=2)
    d['observer_failed'] = True
    if reason == 0: d['clock_failed'] = True
    return d


class WorkEmissionTests(unittest.TestCase):
    def reject(self, d):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature evaluation')) as evaluate:
            with self.assertRaises(ValueError): decode(d)
        evaluate.assert_not_called()

    def test_vector_route_detached_opaque_noio_no_proof(self):
        d = fixture(); before = copy.deepcopy(d)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')): r = decode(d)
        self.assertEqual(d, before); self.assertEqual(len(r['candidate_records']), 1)
        f = r['completed_invocations'][0]['frame']
        self.assertEqual((f['scalar_emissions'], f['cleanup_emissions'], f['vector_emissions']), (2, 1, 2))
        self.assertEqual(f['stage'], 15); self.assertEqual(f['next_vector'], 0)
        events = r['candidate_records'][0]['events']
        self.assertEqual(events[3]['fields'][3], 0x7ff8000012345678)
        self.assertEqual(events[11]['vector'], [0x7fc01234, 0x80000000, 0xffffffff, 0])
        self.assertNotEqual(r['candidate_records'][0]['entry']['actual_target'], decoder.TAIL)
        self.assertIs(r['prelude_unobserved'], True)
        for k in decoder.FALSE_FLAGS: self.assertIs(r[k], False)
        self.assertNotIn('PASS', r['status']); self.assertNotIn('delta_sum', r)
        events[0]['fields'][0] = 8
        self.assertEqual(r['work_emission_inventory']['threads'][0]['events'][0]['fields'][0], 0)

    def test_dynamic_job_reload_and_wait(self):
        r = decode(fixture('job')); f = r['completed_invocations'][0]['frame']
        self.assertEqual((f['next_job'], f['job_bound'], f['job_emissions'], f['wait_emissions']), (2, 1, 2, 1))
        self.assertEqual(r['candidate_records'][0]['events'][5]['fields'][8], 0x7fc01234)

    def test_main_zero_not_zero_total_work(self):
        r = decode(fixture('zero')); self.assertEqual(len(r['candidate_records']), 1)
        self.assertFalse(r['completed_invocations'][0]['frame']['scalar_count_observed'])
        self.assertTrue(r['prelude_unobserved']); self.assertFalse(r['task_completion_proven'])

    def test_initial_signed_nonpositive_jobs_still_wait(self):
        for bound in (0, 0x80000000, decoder.U32):
            d = fixture('job'); lane = d['threads'][0]
            lane['events'] = lane['events'][:5]+lane['events'][-3:]
            lane['events'][4]['fields'][0] = bound
            lane['events'][5]['fields'][1] = bound
            for i, ev in enumerate(lane['events']): ev['meta']['qpc'] = 110+i
            lane['event_count'] = len(lane['events'])
            self.assertEqual(decode(d)['completed_invocations'][0]['frame']['job_emissions'], 0)

    def test_pending_every_valid_prefix(self):
        for branch in ('vector', 'job', 'zero'):
            for n in range(1, len(fixture(branch)['threads'][0]['events'])):
                r = decode(prefix(fixture(branch), n))
                self.assertEqual(r['candidate_records'], []); self.assertEqual(r['pending_invocations'], 1)
                self.assertIn('PENDING_WORK_INVOCATIONS', r['candidate_blocking_issues'])

    def test_original_exception_any_valid_prefix(self):
        for n in range(1, len(fixture()['threads'][0]['events'])):
            d = prefix(fixture(), n); lane = d['threads'][0]
            ev = copy.deepcopy(lane['events'][-1]); ev.update(event=23, fields=[0]*10, vector=[0]*4)
            ev['meta'].update(qpc=ev['meta']['qpc']+1, tail_fresh=False)
            lane['events'].append(ev); lane.update(event_count=n+1, pending_count=0, pending=[])
            d['counts']['original_exceptions'] = 1
            r = decode(d); self.assertFalse(r['completed_invocations'][0]['normal_return'])
            self.assertEqual(r['candidate_records'], [])

    def test_selected_emission_exceptions_not_fabricated_returns(self):
        for branch, pre, exceptional in (('vector', 3, 5), ('vector', 6, 8),
            ('job', 11, 13), ('job', 15, 17), ('vector', 19, 21)):
            d = fixture(branch); lane = d['threads'][0]
            i = next(i for i, ev in enumerate(lane['events']) if ev['event'] == pre)
            prefix(d, i+1); end = copy.deepcopy(lane['events'][-1])
            end.update(event=exceptional, fields=[0]*10, vector=[0]*4); end['meta']['qpc'] += 1
            exit_ev = copy.deepcopy(end); exit_ev['event'] = 23; exit_ev['meta']['qpc'] += 1
            lane['events'] += [end, exit_ev]; lane.update(event_count=i+3, pending_count=0, pending=[])
            d['counts']['original_exceptions'] = 1
            r = decode(d); self.assertEqual(r['completed_invocations'][0]['frame']['stage'], 16)

    def test_empty_lane_inventory(self):
        d = fixture(); d['counts'].update(observed_entries=0, admitted_entries=0, thread_lifetimes=0, tail_scopes=0)
        d['threads'] = []
        self.assertIn('EMPTY_WORK_INVENTORY', decode(d)['candidate_blocking_issues'])

    def test_healthy_empty_claimed_lane_cannot_leave_candidates(self):
        d = fixture('zero'); empty = copy.deepcopy(d['threads'][0])
        empty.update(thread_epoch=2, native_thread_id=101, entries=[], events=[],
            entry_count=0, event_count=0, parent_virtual_thread_epoch=0,
            parent_detail_thread_epoch=0, parent_router_thread_epoch=0)
        d['threads'].append(empty); d['counts']['thread_lifetimes'] = 2
        self.reject(d)
        # A global failure unrelated to this lane does not manufacture its fault.
        d['observer_failed'] = True
        self.reject(d)

    def test_failed_first_enter_retains_empty_claimed_lane(self):
        d = rejection(fixture('zero'), 5); lane = d['threads'][0]
        lane.update(entries=[], events=[], entry_count=0, event_count=0,
            parent_virtual_thread_epoch=0, parent_detail_thread_epoch=0,
            parent_router_thread_epoch=0)
        d['counts'].update(observed_entries=1, admitted_entries=0, tail_scopes=1)
        d['rejected_entries'][0]['entry']['invocation']['serial'] = 1
        r = decode(d)
        self.assertEqual(r['candidate_records'], [])
        self.assertIn('EMPTY_WORK_INVENTORY', r['candidate_blocking_issues'])
        self.assertEqual(r['work_emission_inventory']['threads'][0]['model_fault'], 1)
        self.assertEqual(len(r['work_emission_inventory']['rejected_entries']), 1)

    def test_nested_exact_lifo_and_separate_parent_namespaces(self):
        d = fixture(); lane = d['threads'][0]; inner = copy.deepcopy(fixture('zero')['threads'][0])
        entry = inner['entries'][0]; entry['invocation']['serial'] = 2
        entry['virtual_entry']['invocation']['serial'] += 1
        entry['virtual_entry']['parent_entry']['invocation']['serial'] += 1
        entry['virtual_entry']['parent_entry']['router']['serial'] += 1
        lane['entries'].append(entry); lane['entry_count'] = 2
        for ev in inner['events']:
            ev['invocation'] = copy.deepcopy(entry['invocation']); ev['entry_index'] = 1
            ev['meta']['parent_admitted'] = True
        lane['events'] = lane['events'][:4]+inner['events']+lane['events'][4:]
        for i, ev in enumerate(lane['events']): ev['meta']['qpc'] = 110+i
        lane['event_count'] = len(lane['events'])
        d['counts'].update(observed_entries=2, admitted_entries=2, tail_scopes=2)
        r = decode(d)
        self.assertEqual([r['invocation']['serial'] for r in r['candidate_records']], [2, 1])
        self.assertEqual(r['completed_invocations'][0]['frame']['model_parent'], dict(thread_epoch=1, serial=1))
        for namespace in ('virtual', 'detail', 'router', 'own'):
            bad = copy.deepcopy(d); e = bad['threads'][0]['entries'][1]
            if namespace == 'virtual': e['virtual_entry']['invocation']['serial'] -= 1
            if namespace == 'detail': e['virtual_entry']['parent_entry']['invocation']['serial'] -= 1
            if namespace == 'router': e['virtual_entry']['parent_entry']['router']['serial'] -= 1
            if namespace == 'own':
                e['invocation']['serial'] = 1
                for ev in bad['threads'][0]['events']:
                    if ev['entry_index'] == 1: ev['invocation']['serial'] = 1
            self.reject(bad)
        bad = copy.deepcopy(d); bad['threads'][0]['events'][5]['entry_index'] = 0; self.reject(bad)

    def test_parent_lifetime_alias_across_lanes(self):
        d = fixture('zero'); second = copy.deepcopy(d['threads'][0]); second['thread_epoch'] = 2
        entry = second['entries'][0]; entry['invocation']['thread_epoch'] = 2
        entry['virtual_entry']['invocation']['thread_epoch'] += 1
        entry['virtual_entry']['parent_entry']['invocation']['thread_epoch'] += 1
        entry['virtual_entry']['parent_entry']['router']['thread_epoch'] += 1
        entry['virtual_entry']['parent_entry']['update']['thread_epoch'] += 1
        second.update(parent_virtual_thread_epoch=8, parent_detail_thread_epoch=10,
            parent_router_thread_epoch=entry['virtual_entry']['parent_entry']['router']['thread_epoch'])
        for ev in second['events']: ev['invocation']['thread_epoch'] = 2
        d['threads'].append(second); d['counts'].update(observed_entries=2, admitted_entries=2, thread_lifetimes=2, tail_scopes=2)
        # Reused native TID alone does not imply the same lifetime namespace.
        self.assertEqual(len(decode(d)['candidate_records']), 2)
        for lane_key, path in (('parent_virtual_thread_epoch', ('virtual_entry', 'invocation')),
            ('parent_detail_thread_epoch', ('virtual_entry', 'parent_entry', 'invocation')),
            ('parent_router_thread_epoch', ('virtual_entry', 'parent_entry', 'router'))):
            bad = copy.deepcopy(d); l = bad['threads'][1]; l[lane_key] = bad['threads'][0][lane_key]
            target = l['entries'][0]
            for key in path: target = target[key]
            target['thread_epoch'] = l[lane_key]; self.reject(bad)

    def test_tail_rejected_census_alone_not_global_failure(self):
        d = fixture(); d['counts'].update(tail_scopes=100, tail_rejected=99)
        r = decode(d); self.assertEqual(len(r['candidate_records']), 1)
        self.assertTrue(r['retained_context_issues']); self.assertEqual(r['candidate_blocking_issues'], [])
        d['counts']['tail_rejected'] = 100; self.reject(d)

    def test_all_rejection_stages_retained(self):
        for reason in range(6):
            r = decode(rejection(fixture(), reason)); self.assertEqual(r['candidate_records'], [])
            self.assertEqual(r['work_emission_inventory']['rejected_entries'][0]['reason'], reason)

    def test_invalid_accessor_status_clears_all_parent_content(self):
        d = rejection(fixture(), 1); row = d['rejected_entries'][0]
        row['meta'].update(accessor_status=0, tail_present=False, tail_matches=False, tail_fresh=False)
        self.reject(d)
        row['entry']['virtual_entry'] = dict(invocation=dict(thread_epoch=0, serial=0),
            parent_entry=decoder.virtual._zero_parent(), parent_child=decoder.parent._zero_child(),
            actual_target=0, caller_lr=0, r3=0, f1_bits=0)
        row['entry']['active_call0'] = decoder.virtual._zero_call()
        self.assertEqual(decode(d)['candidate_records'], [])

    def test_rejected_allocation_never_admitted(self):
        d = rejection(fixture(), 5); d['rejected_entries'][0]['entry']['invocation']['serial'] = 1; self.reject(d)

    def test_multiple_failed_model_attempts_monotonic(self):
        d = rejection(fixture(), 5); second = copy.deepcopy(d['rejected_entries'][0])
        second['entry']['invocation']['serial'] = 3; second['meta']['qpc'] += 1
        d['rejected_entries'].append(second)
        d['counts'].update(observed_entries=3, dropped_entries=2, rejected_entries=2, observation_errors=2, tail_scopes=3)
        self.assertEqual(decode(d)['candidate_records'], [])
        second['entry']['invocation']['serial'] = 1; self.reject(d)

    def test_fault_health_retained_and_no_candidate(self):
        for k in decoder.HEALTH:
            d = fixture(); d[k] = True
            if k in ('clock_failed', 'counter_saturated'): d['observer_failed'] = True
            r = decode(d); self.assertEqual(r['candidate_records'], []); self.assertIn(k.upper(), r['candidate_blocking_issues'])
        for k in ('binding_mismatches', 'observation_errors'):
            d = fixture(); d['counts'][k] = 1; d['counts']['observation_errors'] = 1; d['observer_failed'] = True
            self.assertEqual(decode(d)['candidate_records'], [])

    def test_invalid_cut_requires_both_flags_and_is_raw(self):
        for cut in (0, 99, decoder.QMAX+1):
            d = fixture(); d['freeze_qpc'] = cut; self.reject(d)
            d['clock_failed'] = d['observer_failed'] = True
            r = decode(d); self.assertEqual(r['work_emission_inventory']['freeze_qpc'], cut)
            self.assertEqual(r['candidate_records'], [])
            d['observer_failed'] = False; self.reject(d)

    def test_clock_fault_events_retained_but_valid_clocks_monotonic(self):
        d = fixture(); d['threads'][0]['events'][3]['meta'].update(qpc=0, clock_valid=False)
        self.reject(d); d['clock_failed'] = d['observer_failed'] = True
        self.assertEqual(decode(d)['candidate_records'], [])
        d['threads'][0]['events'][4]['meta']['qpc'] = 100; self.reject(d)

    def test_straddling_exit_allowed_but_actual_cut_enforced(self):
        d = fixture(); ev = d['threads'][0]['events'][-1]; ev['meta']['qpc'] = 300
        self.assertEqual(len(decode(d)['candidate_records']), 1)
        d['freeze_qpc'] = 299; self.reject(d)

    def test_exact_unused_payload_and_widths(self):
        for event, field, value in ((0, 0, 1), (1, 0, 256), (2, 0, 65536), (3, 9, 1),
            (7, 0, 1), (9, 0, 256), (18, 0, 65536), (22, 0, 1)):
            d = fixture(); next(ev for ev in d['threads'][0]['events'] if ev['event'] == event)['fields'][field] = value
            self.reject(d)
        d = fixture('job'); d['threads'][0]['events'][5]['fields'][8] = 2**32; self.reject(d)
        d = fixture(); d['threads'][0]['events'][3]['vector'][0] = 1; self.reject(d)

    def test_scalar_filter_repetition_and_cleanup_required(self):
        d = fixture(); d['threads'][0]['events'][5]['fields'][1] = 8; self.reject(d)
        d = fixture(); d['threads'][0]['events'][6]['fields'][0] = 1; self.reject(d)

    def test_job_requires_reload_ordinal_and_last_bound_wait(self):
        for index, field, value in ((5, 1, 1), (7, 0, 0), (10, 0, 3), (11, 1, 0), (11, 3, 0)):
            d = fixture('job'); d['threads'][0]['events'][index]['fields'][field] = value; self.reject(d)
        d = fixture('job'); d['threads'][0]['events'][7]['event'] = 10; self.reject(d)

    def test_vector_exact_reverse_and_lr(self):
        d = fixture(); d['threads'][0]['events'][11]['fields'][1] = 0; self.reject(d)
        for k in decoder.SITES:
            d = fixture('job' if k in (11, 15) else 'vector')
            next(ev for ev in d['threads'][0]['events'] if ev['event'] == k)['fields'][0] += 1
            self.reject(d)

    def test_pending_frame_exact_including_observed_flags(self):
        d = prefix(fixture(), 10); d['threads'][0]['pending'][0]['scalar_upper'] += 1; self.reject(d)
        d = prefix(fixture(), 1); d['threads'][0]['pending'][0]['main_observed'] = True; self.reject(d)

    def test_full_ancestry_and_operand_identity(self):
        for key in ('r3', 'f1_bits', 'actual_target', 'caller_lr'):
            d = fixture(); d['threads'][0]['entries'][0][key] ^= 1; self.reject(d)
        d = fixture(); d['threads'][0]['entries'][0]['virtual_entry']['parent_entry']['update']['native_thread_id'] += 1; self.reject(d)
        d = fixture(); d['threads'][0]['parent_virtual_thread_epoch'] = 1; self.reject(d)

    def test_exact_metadata_stored_flags(self):
        for flag in ('binding_matches', 'tail_matches', 'tail_present', 'lane_matches', 'lexical_top', 'context_nonnull', 'base_nonnull'):
            d = fixture(); d['threads'][0]['events'][-1]['meta'][flag] = False; d['observer_failed'] = True; self.reject(d)
        d = fixture(); d['threads'][0]['events'][-1]['meta']['tail_fresh'] = True; self.reject(d)

    def test_counts_partitions_and_saturation(self):
        for key in ('observed_entries', 'admitted_entries', 'thread_lifetimes', 'original_exceptions', 'rejected_entries'):
            d = fixture(); d['counts'][key] += 1; self.reject(d)
        d = fixture(); d['counter_saturated'] = d['observer_failed'] = True
        d['counts']['observed_entries'] = 2**64-1
        self.assertEqual(decode(d)['candidate_records'], [])

    def test_capacity_fault_requires_actual_capacity(self):
        for fault in (3, 4, 5):
            d = fixture(); d['threads'][0]['model_fault'] = fault; d['observer_failed'] = True; self.reject(d)
        d = prefix(fixture(), 3); d['threads'][0]['model_fault'] = 1; d['observer_failed'] = True
        self.assertEqual(decode(d)['candidate_records'], [])

    def test_malformed_last_record_before_evaluation(self):
        for key, value in (('event', 24), ('entry_index', 1), ('fields', [0]*9), ('vector', [0]*3)):
            d = fixture(); d['threads'][0]['events'][-1][key] = value; self.reject(d)
        d = fixture(); d['threads'][0]['events'][-1]['meta']['unknown'] = 0; self.reject(d)
        d = fixture(); d['threads'][0]['entries'].append(copy.deepcopy(d['threads'][0]['entries'][0]))
        d['threads'][0]['entry_count'] = 2; self.reject(d)

    def test_exact_types_keys_and_proof_flags(self):
        for value in (True, -1, 1.0, 2**64):
            d = fixture(); d['threads'][0]['events'][-1]['fields'][0] = value; self.reject(d)
        for k in decoder.FALSE_FLAGS:
            d = fixture(); d[k] = True; self.reject(d)
        d = fixture(); d['prelude_unobserved'] = False; self.reject(d)
        d = fixture(); d['unexpected'] = 0; self.reject(d)
        d = fixture(); d['threads'][0]['events'][-1]['meta']['clock_valid'] = 1; self.reject(d)

    def test_duplicate_keys_nan_and_bytes_bound(self):
        for blob in (b'{"schema":0,"schema":1}', b'{"schema":NaN}'):
            with self.assertRaises(ValueError): decoder.validate_work_emission(blob, TOKEN)
        with self.assertRaises(ValueError): decoder.validate_work_emission(b' '*(decoder.MAX_BYTES+1), TOKEN)
        for token in ('0'*32, TOKEN.upper(), TOKEN+'0', True):
            with self.assertRaises(ValueError): decoder.validate_work_emission(raw(fixture()), token)


if __name__ == '__main__': unittest.main()
