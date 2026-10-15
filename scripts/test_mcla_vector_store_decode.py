"""Synthetic strict store journal tests. No native execution or file writes."""
import copy
import unittest
from unittest import mock

import mcla_vector_store_decode as decoder
from test_mcla_vector_target_decode import fixture as target_fixture
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def fixture(zero_change=False):
    outer = copy.deepcopy(target_fixture()['threads'][0]['entries'][0])
    outer['invocation'] = dict(thread_epoch=7, serial=15)
    e = dict(invocation=dict(thread_epoch=1, serial=1), target_entry=outer,
        slot28_pre=dict(fields=[decoder.CALLER, outer['r3'], 0xabcdef008259e768, 0], vector=copy.deepcopy(outer['v1'])),
        parent_event_index=6, actual_target=decoder.TARGET, caller_lr=decoder.CALLER,
        r3=outer['r3'], v1=copy.deepcopy(outer['v1']))
    old = [0, 0x80000000, 0x7fc12345, 0xffffffff]
    computed = old if zero_change else [0x7f800000, 0xff800000, 1, 0x80000001]
    scaled = [0x3f800000, 0x3f000000, 0, 1]
    events = []
    def add(k, fields=(), lanes=None):
        meta = dict.fromkeys(decoder.META_FLAGS, True)
        meta.update(qpc=110+len(events), native_thread_id=100, accessor_status=8, parent_admitted=False)
        events.append(dict(invocation=copy.deepcopy(e['invocation']), entry_index=0, event=k,
            fields=list(fields)+[0]*(4-len(fields)), vector=list(lanes if lanes is not None else [0]*4), meta=meta))
    address = e['r3'] & decoder.U32
    add(0); add(1, [(address+256) & ~15], old); add(2, [(address+272) & ~15], [0]*4)
    add(3, lanes=computed); add(4, [(address+288) & ~15], old); add(5, lanes=scaled)
    add(6, [(address+256) & ~15], computed); add(7, [decoder.CALLER, e['r3'], decoder.TAIL], scaled)
    add(8); add(10)
    d = dict.fromkeys(decoder.FALSE_FLAGS, False)
    d.update(dict.fromkeys(decoder.HEALTH, False)); d.update(dict.fromkeys(decoder.LIMITATIONS, True))
    d.update(schema='mcla-vector-store-v1', token=TOKEN, clock='win32-query-performance-counter',
        begin_qpc=100, end_qpc_exclusive=250, qpc_frequency=100, freeze_qpc=1000,
        thread_capacity=8, entries_per_thread_capacity=4096, events_per_thread_capacity=65536,
        depth_capacity=8, rejected_entry_capacity=256, json_limit_bytes=decoder.MAX_BYTES,
        vector_lane_order='PPCContext.u32[0,1,2,3]_HOST_STORAGE_ORDER_NO_BYTE_SWAP',
        counts=dict.fromkeys(decoder.COUNTS, 0), rejected_entries=[])
    d['counts'].update(observed_entries=1, admitted_entries=1, thread_lifetimes=1)
    d['threads'] = [dict(thread_epoch=1, native_thread_id=100, parent_target_thread_epoch=7,
        model_fault=0, entry_count=1, event_count=len(events), pending_count=0, entries=[e], events=events, pending=[])]
    return d


def decode(d): return decoder.validate_vector_store(raw(d), TOKEN)


def prefix(d, n):
    lane = d['threads'][0]; lane['events'] = lane['events'][:n]; lane['event_count'] = n
    f = decoder._new_frame(0, dict(thread_epoch=0, serial=0))
    for ev in lane['events'][1:]: decoder._step(f, ev, lane['entries'][0])
    lane.update(pending_count=1, pending=[f]); return d


def second(d):
    lane = d['threads'][0]; entry = copy.deepcopy(lane['entries'][0])
    entry['invocation']['serial'] = 2; entry['target_entry']['invocation']['serial'] = 19
    entry['parent_event_index'] = 35
    lane['entries'].append(entry); more = copy.deepcopy(lane['events'])
    for ev in more: ev['entry_index'] = 1; ev['invocation']['serial'] = 2; ev['meta']['qpc'] += 50
    lane['events'] += more; lane.update(entry_count=2, event_count=len(lane['events']))
    d['counts'].update(observed_entries=2, admitted_entries=2)
    return d


def rejection(d, reason=4, status=8):
    lane = d['threads'][0]; e = copy.deepcopy(lane['entries'][0]); m = copy.deepcopy(lane['events'][0]['meta'])
    e['invocation'] = dict(thread_epoch=1, serial=2) if reason == 4 else dict(thread_epoch=0, serial=0)
    m.update(qpc=200, accessor_status=status, lane_matches=reason >= 3)
    if status != 8:
        e.update(target_entry=decoder._zero_target(), slot28_pre=dict(fields=[0]*4, vector=[0]*4), parent_event_index=0)
        m['binding_matches'] = False
    d['rejected_entries'] = [dict(reason=reason, entry=e, meta=m)]
    d['observer_failed'] = True
    d['counts'].update(observed_entries=2, dropped_entries=1, rejected_entries=1, observation_errors=1)
    if reason == 4: lane['model_fault'] = 1
    if reason == 0: m.update(qpc=0, clock_valid=False); d['clock_failed'] = True
    return d


class VectorStoreTests(unittest.TestCase):
    def reject(self, d):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature evaluation')) as evaluate:
            with self.assertRaises(ValueError): decode(d)
        evaluate.assert_not_called()

    def test_detached_raw_witness_no_io_or_proof(self):
        d = fixture(); before = copy.deepcopy(d)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')): r = decode(d)
        self.assertEqual(d, before); self.assertEqual(len(r['candidate_records']), 1)
        self.assertEqual(r['vector_store_inventory'], d); self.assertNotIn('PASS', r['status'])
        for k in decoder.FALSE_FLAGS | {'arithmetic_proven', 'state_units_proven', 'frame_to_step_mapping_proven'}:
            self.assertIs(r[k], False)
        r['candidate_records'][0]['entry']['v1'][0] = 7
        self.assertEqual(r['vector_store_inventory'], d)

    def test_zero_change_and_arbitrary_float_bits_remain_valid_raw_witness(self):
        for unchanged in (False, True):
            r = decode(fixture(unchanged)); self.assertEqual(len(r['candidate_records']), 1)
            self.assertIs(r['arithmetic_proven'], False)
        # Rate zero but a changed computed vector is still a valid raw witness:
        # arithmetic is deliberately not inferred by this structural decoder.
        d = fixture(); self.assertEqual(d['threads'][0]['events'][2]['vector'], [0]*4)
        self.assertNotEqual(d['threads'][0]['events'][1]['vector'], d['threads'][0]['events'][3]['vector'])
        self.assertEqual(len(decode(d)['candidate_records']), 1)

    def test_every_valid_pending_prefix(self):
        for n in range(1, 10):
            r = decode(prefix(fixture(), n)); self.assertEqual(r['candidate_records'], [])
            self.assertIn('PENDING_VECTOR_STORE_INVOCATIONS', r['candidate_blocking_issues'])

    def test_exception_after_every_prefix(self):
        for n in range(1, 10):
            d = prefix(fixture(), n); lane = d['threads'][0]
            ev = copy.deepcopy(lane['events'][-1]); ev.update(event=11, fields=[0]*4, vector=[0]*4)
            ev['meta']['qpc'] += 1; lane['events'].append(ev)
            lane.update(event_count=n+1, pending_count=0, pending=[]); d['counts']['original_exceptions'] = 1
            r = decode(d); self.assertEqual(r['candidate_records'], [])
            self.assertFalse(r['completed_invocations'][0]['normal_return'])

    def test_tail_exception_requires_active_tail_and_exceptional_exit(self):
        d = fixture(); lane = d['threads'][0]
        lane['events'][8]['event'] = 9; lane['events'][9]['event'] = 11
        d['counts']['original_exceptions'] = 1
        self.assertEqual(decode(d)['candidate_records'], [])
        bad = copy.deepcopy(d); bad['threads'][0]['events'][9]['event'] = 10; self.reject(bad)
        bad = prefix(fixture(), 7); bad['threads'][0]['events'][-1]['event'] = 9; self.reject(bad)
        d = prefix(d, 9); d['counts']['original_exceptions'] = 0
        self.assertEqual(d['threads'][0]['pending'][0]['stage'], 9)
        self.assertEqual(decode(d)['pending_invocations'], 1)

    def test_missing_reordered_duplicate_events(self):
        for mode in range(3):
            d = fixture(); lane = d['threads'][0]
            if mode == 0: del lane['events'][2]
            if mode == 1: lane['events'][2], lane['events'][3] = lane['events'][3], lane['events'][2]
            if mode == 2: lane['events'].insert(3, copy.deepcopy(lane['events'][2]))
            lane['event_count'] = len(lane['events']); self.reject(d)

    def test_original_addresses_store_bits_tail_arguments_and_unused_payload(self):
        for event in (1, 2, 4, 6):
            d = fixture(); d['threads'][0]['events'][event]['fields'][0] += 1; self.reject(d)
        for event, field in ((6, 'vector'), (7, 'vector')):
            d = fixture(); d['threads'][0]['events'][event][field][0] ^= 1; self.reject(d)
        for i in range(3):
            d = fixture(); d['threads'][0]['events'][7]['fields'][i] ^= 1; self.reject(d)
        for event in range(10):
            d = fixture(); d['threads'][0]['events'][event]['fields'][3] = 1; self.reject(d)
        for event in (0, 8, 9):
            d = fixture(); d['threads'][0]['events'][event]['vector'][0] = 1; self.reject(d)

    def test_exact_parent_saved_call_and_body_binding(self):
        for path in ('actual_target', 'caller_lr', 'r3', 'v1', 'parent_event_index'):
            d = fixture(); e = d['threads'][0]['entries'][0]
            if path == 'v1': e[path][0] ^= 1
            elif path == 'parent_event_index': e[path] = 0
            else: e[path] ^= 1
            self.reject(d)
        for i in range(4):
            d = fixture(); d['threads'][0]['entries'][0]['slot28_pre']['fields'][i] ^= 1; self.reject(d)
        d = fixture(); d['threads'][0]['entries'][0]['slot28_pre']['vector'][0] ^= 1; self.reject(d)
        # Upper CTR bits are inventory; target resolution is explicitly low32.
        d = fixture(); d['threads'][0]['entries'][0]['slot28_pre']['fields'][2] ^= 1 << 40
        self.assertEqual(len(decode(d)['candidate_records']), 1)

    def test_parent_serial_and_event_index_strictly_increase(self):
        d = second(fixture()); self.assertEqual(len(decode(d)['candidate_records']), 2)
        for key in ('parent', 'event', 'epoch', 'own'):
            bad = copy.deepcopy(d); e = bad['threads'][0]['entries'][1]
            if key == 'parent': e['target_entry']['invocation']['serial'] = 15
            if key == 'event': e['parent_event_index'] = 6
            if key == 'epoch': e['target_entry']['invocation']['thread_epoch'] = 8
            if key == 'own':
                e['invocation']['serial'] = 1
                for ev in bad['threads'][0]['events'][10:]: ev['invocation']['serial'] = 1
            self.reject(bad)

    def test_multilane_target_lifetimes_cannot_alias(self):
        d = fixture(); lane = copy.deepcopy(d['threads'][0]); lane.update(thread_epoch=2, native_thread_id=101, parent_target_thread_epoch=8)
        lane['entries'][0]['invocation']['thread_epoch'] = 2
        outer = lane['entries'][0]['target_entry']; outer['invocation']['thread_epoch'] = 8
        outer['work_entry']['virtual_entry']['parent_entry']['update']['native_thread_id'] = 101
        for ev in lane['events']: ev['invocation']['thread_epoch'] = 2; ev['meta']['native_thread_id'] = 101
        d['threads'].append(lane); d['counts'].update(observed_entries=2, admitted_entries=2, thread_lifetimes=2)
        self.assertEqual(len(decode(d)['candidate_records']), 2)
        lane['parent_target_thread_epoch'] = 7; outer['invocation']['thread_epoch'] = 7; self.reject(d)

    def test_unselected_context_has_separate_partition_not_failure(self):
        d = fixture(); d['counts'].update(observed_entries=7, unselected_entries=6,
            unselected_absent_entries=1, unselected_wrong_stage_entries=2, unselected_other_target_entries=3)
        r = decode(d); self.assertEqual(len(r['candidate_records']), 1)
        self.assertFalse(r['coverage_proven']); self.assertEqual(r['candidate_blocking_issues'], [])
        d['counts']['unselected_entries'] = 5; self.reject(d)

    def test_empty_inventory_is_not_coverage_even_with_unselected(self):
        d = fixture(); d['threads'] = []; d['counts'] = dict.fromkeys(decoder.COUNTS, 0)
        d['counts'].update(observed_entries=1, unselected_entries=1, unselected_absent_entries=1)
        self.assertIn('EMPTY_VECTOR_STORE_INVENTORY', decode(d)['candidate_blocking_issues'])

    def test_faulted_prefix_preserved_and_fault_contradictions_rejected(self):
        d = prefix(fixture(), 4); d['threads'][0]['model_fault'] = 2
        d['observer_failed'] = True; d['counts']['observation_errors'] = 1
        r = decode(d); self.assertEqual(r['candidate_records'], []); self.assertIn('MODEL_FAULT', r['candidate_blocking_issues'])
        for failure in ('sticky', 'count', 'capacity'):
            bad = copy.deepcopy(d)
            if failure == 'sticky': bad['observer_failed'] = False
            if failure == 'count': bad['counts']['observation_errors'] = 0
            if failure == 'capacity': bad['threads'][0]['model_fault'] = 3
            self.reject(bad)

    def test_pending_saved_computed_scaled_and_parent_are_exact(self):
        for field in ('computed', 'scaled', 'model_parent', 'stage', 'entry_index'):
            d = prefix(fixture(), 8); f = d['threads'][0]['pending'][0]
            if field in ('computed', 'scaled'): f[field][0] ^= 1
            elif field == 'model_parent': f[field]['serial'] = 1
            else: f[field] += 1
            self.reject(d)

    def test_failed_binding_and_clock_retained_not_candidates(self):
        d = fixture(); m = d['threads'][0]['events'][2]['meta']; m.update(binding_matches=False, accessor_status=4)
        d['observer_failed'] = True; d['counts'].update(binding_mismatches=1, observation_errors=1)
        self.assertEqual(decode(d)['candidate_records'], [])
        m.update(clock_valid=False, qpc=0); d['clock_failed'] = True
        self.assertEqual(decode(d)['candidate_records'], [])
        d['clock_failed'] = False; self.reject(d)

    def test_cut_and_clock_order(self):
        d = fixture(); d['freeze_qpc'] = 119
        self.assertIn('CUT_BOUNDARY_AMBIGUOUS', decode(d)['candidate_blocking_issues'])
        d['freeze_qpc'] = 118; self.reject(d)
        d = fixture(); d['threads'][0]['events'][3]['meta']['qpc'] = 110; self.reject(d)
        d = fixture(); d['end_qpc_exclusive'] = 111
        self.assertEqual(len(decode(d)['candidate_records']), 1)
        d['threads'][0]['events'][0]['meta']['qpc'] = 111; self.reject(d)

    def test_rejected_model_and_pre_lane_binding_preserved(self):
        for reason, status in ((4, 8), (1, 6), (2, 8), (0, 0)):
            d = rejection(fixture(), reason, status)
            self.assertEqual(decode(d)['vector_store_inventory'], d)
        d = rejection(fixture()); d['rejected_entries'][0]['entry']['invocation']['serial'] = 1; self.reject(d)
        d = rejection(fixture(), 1, 6); d['rejected_entries'][0]['entry']['target_entry']['r3'] = 1; self.reject(d)

    def test_unselected_status_cannot_be_valid_clock_rejection(self):
        for status in (0, 5, 9):
            self.reject(rejection(fixture(), 1, status))
            self.assertEqual(decode(rejection(fixture(), 0, status))['candidate_records'], [])

    def test_valid_first_entry_cannot_identity_fail_on_fresh_lane(self):
        d = rejection(fixture()); lane = d['threads'][0]
        lane.update(entries=[], events=[], pending=[], entry_count=0, event_count=0,
                    pending_count=0, parent_target_thread_epoch=0)
        d['counts'].update(observed_entries=1, admitted_entries=0)
        self.reject(d)

    def test_empty_claimed_lane_cannot_invent_identity_or_transition_fault(self):
        # Claim happens only after Bound checked every parent/body operand.
        # Its fresh positive-epoch, fixed-capacity model must accept the first
        # positive serial. No event can transition-fail before that admission.
        for fault in (1, 2):
            with self.subTest(fault=fault):
                d = fixture(); lane = d['threads'][0]
                lane.update(entries=[], events=[], pending=[], entry_count=0,
                    event_count=0, pending_count=0, parent_target_thread_epoch=0, model_fault=fault)
                d['observer_failed'] = True
                d['counts'].update(observed_entries=0, admitted_entries=0, observation_errors=1)
                self.reject(d)

    def test_counter_conservation_and_rejection_overflow(self):
        for field in ('observed_entries', 'admitted_entries', 'original_exceptions', 'rejected_entries', 'thread_lifetimes'):
            d = fixture(); d['counts'][field] += 1; self.reject(d)
        d = rejection(fixture()); d['counts']['observation_errors'] = 0; self.reject(d)
        d = fixture(); d['counts'].update(observed_entries=2, dropped_entries=1, observation_errors=1, rejection_overflow=1)
        d['observer_failed'] = True; self.reject(d)

    def test_saturation_is_sticky_and_suppresses_candidates(self):
        d = fixture(); d['counter_saturated'] = True; self.reject(d)
        d['observer_failed'] = True; d['counts']['observed_entries'] = 2**64-1
        self.assertEqual(decode(d)['candidate_records'], [])

    def test_exact_shapes_numeric_bool_and_flags(self):
        for key in ('begin_qpc', 'entries_per_thread_capacity'):
            d = fixture(); d[key] = True; self.reject(d)
        d = fixture(); d['extra'] = 0; self.reject(d)
        d = fixture(); del d['clock']; self.reject(d)
        d = fixture(); d['threads'][0]['events'][1]['fields'][0] = True; self.reject(d)
        d = fixture(); d['threads'][0]['entries'][0]['v1'][0] = 2**32; self.reject(d)
        d = fixture(); d['threads'][0]['events'][1]['meta']['accessor_status'] = 10; self.reject(d)
        for key in decoder.FALSE_FLAGS:
            d = fixture(); d[key] = True; self.reject(d)
        for key in decoder.LIMITATIONS:
            d = fixture(); d[key] = False; self.reject(d)

    def test_json_duplicates_nonintegers_and_limits(self):
        for encoded in (b'{"x":0,"x":0}', b'{"x":NaN}', b'{"x":1.0}', b'[]', b'null', b'\xff'):
            with self.assertRaises(ValueError): decoder.validate_vector_store(encoded, TOKEN)
        with self.assertRaises(ValueError): decoder.validate_vector_store('{}', TOKEN)
        with mock.patch.object(decoder, 'MAX_BYTES', 1):
            with self.assertRaises(ValueError): decoder.validate_vector_store(raw(fixture()), TOKEN)
        for token in ('0'*32, 'A'*32, TOKEN+'0', True):
            with self.assertRaises(ValueError): decoder.validate_vector_store(raw(fixture()), token)


if __name__ == '__main__': unittest.main()
