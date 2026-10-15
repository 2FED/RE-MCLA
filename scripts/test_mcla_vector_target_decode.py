"""Synthetic target journal regression tests; no native execution or IO."""
import copy
import unittest
from unittest import mock

import mcla_vector_target_decode as decoder
from test_mcla_work_emission_decode import fixture as work_fixture
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def fixture(gate=0, loaded=32):
    work = copy.deepcopy(work_fixture()['threads'][0]['entries'][0])
    work['invocation'] = dict(thread_epoch=5, serial=10)
    vector = [0, 0x80000000, 0x7fc12345, 0xffffffff]
    e = dict(invocation=dict(thread_epoch=1, serial=1), work_entry=work,
        vector_pre=dict(fields=[decoder.CALLER, 3, 0x10000020, 0xffffffffffffffff, 0xabcdef008259f300]+[0]*5,
                        vector=copy.deepcopy(vector)),
        parent_event_index=5, actual_target=decoder.TARGET, caller_lr=decoder.CALLER,
        r3=0x10000020, v1=copy.deepcopy(vector))
    events = []
    def add(k, fields=(), site=0, lanes=None):
        meta = dict.fromkeys(decoder.META_FLAGS, True)
        meta.update(qpc=110+len(events), native_thread_id=100, accessor_status=8, parent_admitted=False)
        events.append(dict(invocation=copy.deepcopy(e['invocation']), entry_index=0,
            event=k, site=site, fields=list(fields)+[0]*(4-len(fields)), vector=list(lanes or [0]*4), meta=meta))
    def call(site):
        add(2, [decoder.CALLERS[site], e['r3'], 0xabcdef008259f300], site, vector)
        add(3, site=site)
    add(0); add(1, [((e['r3']+400) & decoder.U32) & ~15], lanes=vector)
    for site in range(4): call(site)
    add(5, [e['r3']+657, 1]); add(6, [gate])
    if gate == 0: call(4)
    call(5); add(7, [e['r3']+664, loaded]); add(8, [e['r3']+664, loaded, loaded+1])
    if 32 < (loaded+1) & decoder.U32 < 2**31: call(6)
    add(9)
    d = dict.fromkeys(decoder.FALSE_FLAGS, False)
    d.update(dict.fromkeys(decoder.HEALTH, False)); d.update(dict.fromkeys(decoder.LIMITATIONS, True))
    d.update(schema='mcla-vector-target-v1', token=TOKEN, clock='win32-query-performance-counter',
        begin_qpc=100, end_qpc_exclusive=250, qpc_frequency=100, freeze_qpc=1000,
        thread_capacity=8, entries_per_thread_capacity=4096, events_per_thread_capacity=65536,
        depth_capacity=8, rejected_entry_capacity=256, json_limit_bytes=decoder.MAX_BYTES,
        vector_lane_order='PPCContext.u32[0,1,2,3]_HOST_STORAGE_ORDER_NO_BYTE_SWAP',
        counts=dict.fromkeys(decoder.COUNTS, 0), rejected_entries=[])
    d['counts'].update(observed_entries=1, admitted_entries=1, thread_lifetimes=1)
    d['threads'] = [dict(thread_epoch=1, native_thread_id=100, parent_work_thread_epoch=5,
        model_fault=0, entry_count=1, event_count=len(events), pending_count=0, entries=[e], events=events, pending=[])]
    return d


def decode(d): return decoder.validate_vector_target(raw(d), TOKEN)


def prefix(d, n):
    lane = d['threads'][0]; lane['events'] = lane['events'][:n]; lane['event_count'] = n
    f = decoder._new_frame(0, dict(thread_epoch=0, serial=0))
    for ev in lane['events'][1:]: decoder._step(f, ev, lane['entries'][0])
    lane.update(pending_count=1, pending=[f]); return d


class VectorTargetTests(unittest.TestCase):
    def reject(self, d):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature evaluation')) as evaluate:
            with self.assertRaises(ValueError): decode(d)
        evaluate.assert_not_called()

    def test_detached_raw_body_no_io_no_proof(self):
        d = fixture(); before = copy.deepcopy(d)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')): r = decode(d)
        self.assertEqual(d, before); self.assertEqual(len(r['candidate_records']), 1)
        self.assertEqual(r['vector_target_inventory'], d)
        self.assertEqual(r['candidate_records'][0]['entry']['v1'], [0, 0x80000000, 0x7fc12345, 0xffffffff])
        self.assertNotIn('PASS', r['status'])
        for k in decoder.FALSE_FLAGS: self.assertIs(r[k], False)
        r['candidate_records'][0]['entry']['v1'][0] = 4
        self.assertEqual(r['vector_target_inventory'], d)

    def test_gate_counter_boundary_and_wrap(self):
        for gate in (0, 16):
            for loaded in (0, 31, 32, 0x7ffffffe, 0x7fffffff, decoder.U32):
                self.assertEqual(len(decode(fixture(gate, loaded))['candidate_records']), 1)

    def test_every_valid_pending_prefix(self):
        for n in range(1, len(fixture()['threads'][0]['events'])):
            r = decode(prefix(fixture(), n)); self.assertEqual(r['candidate_records'], [])
            self.assertIn('PENDING_VECTOR_TARGET_INVOCATIONS', r['candidate_blocking_issues'])

    def test_exception_after_every_prefix(self):
        for n in range(1, len(fixture()['threads'][0]['events'])):
            d = prefix(fixture(), n); lane = d['threads'][0]
            ev = copy.deepcopy(lane['events'][-1]); ev.update(event=10, site=0, fields=[0]*4, vector=[0]*4)
            ev['meta']['qpc'] += 1; lane['events'].append(ev)
            lane.update(event_count=n+1, pending_count=0, pending=[]); d['counts']['original_exceptions'] = 1
            self.assertEqual(decode(d)['candidate_records'], [])

    def test_call_exception_requires_active_site(self):
        d = prefix(fixture(), 3); lane = d['threads'][0]
        ev = copy.deepcopy(lane['events'][-1]); ev.update(event=4, fields=[0]*4, vector=[0]*4)
        ev['meta']['qpc'] += 1; lane['events'].append(ev); lane['event_count'] += 1
        lane['pending'][0]['stage'] = 14
        self.assertEqual(decode(d)['candidate_records'], [])
        ev['site'] = 1; self.reject(d)

    def test_empty_inventory_is_not_coverage(self):
        d = fixture(); d['threads'] = []; d['counts'] = dict.fromkeys(decoder.COUNTS, 0)
        self.assertIn('EMPTY_VECTOR_TARGET_INVENTORY', decode(d)['candidate_blocking_issues'])

    def test_faulted_valid_prefix_preserved(self):
        d = prefix(fixture(), 3); d['threads'][0]['model_fault'] = 2
        d['observer_failed'] = True; d['counts']['observation_errors'] = 1
        r = decode(d); self.assertIn('MODEL_FAULT', r['candidate_blocking_issues'])
        self.assertEqual(r['vector_target_inventory'], d)
        d['observer_failed'] = False; self.reject(d)

    def test_failed_binding_and_clock_retained(self):
        d = fixture(); m = d['threads'][0]['events'][2]['meta']; m['binding_matches'] = False; m['accessor_status'] = 4
        d['observer_failed'] = True; d['counts'].update(binding_mismatches=1, observation_errors=1)
        self.assertEqual(decode(d)['candidate_records'], [])
        m.update(clock_valid=False, qpc=0); d['clock_failed'] = True
        self.assertEqual(decode(d)['candidate_records'], [])
        d['clock_failed'] = False; self.reject(d)

    def test_cut_equal_retained_not_candidate_after_cut_rejected(self):
        d = fixture(); d['freeze_qpc'] = d['threads'][0]['events'][-1]['meta']['qpc']
        r = decode(d); self.assertIn('CUT_BOUNDARY_AMBIGUOUS', r['candidate_blocking_issues'])
        self.assertEqual(r['candidate_records'], [])
        d['freeze_qpc'] -= 1; self.reject(d)

    def test_after_window_exit_retains_context(self):
        d = fixture(); d['end_qpc_exclusive'] = 111
        self.assertEqual(len(decode(d)['candidate_records']), 1)
        d['threads'][0]['events'][0]['meta']['qpc'] = 111; self.reject(d)

    def test_exact_schema_and_bool_numeric(self):
        for path in ('begin_qpc', 'entries_per_thread_capacity'):
            d = fixture(); d[path] = True; self.reject(d)
        d = fixture(); d['extra'] = 0; self.reject(d)
        d = fixture(); del d['clock']; self.reject(d)
        d = fixture(); d['threads'][0]['events'][2]['fields'][1] = True; self.reject(d)

    def test_json_duplicates_nan_float_types_and_limit(self):
        for encoded in (b'{"x":0,"x":0}', b'{"x":NaN}', b'{"x":1.0}', b'[]', b'null', b'\xff'):
            with self.assertRaises(ValueError): decoder.validate_vector_target(encoded, TOKEN)
        with self.assertRaises(ValueError): decoder.validate_vector_target('{}', TOKEN)
        with mock.patch.object(decoder, 'MAX_BYTES', 1):
            with self.assertRaises(ValueError): decoder.validate_vector_target(raw(fixture()), TOKEN)

    def test_missing_reordered_duplicate_events(self):
        for mode in range(3):
            d = fixture(); lane = d['threads'][0]
            if mode == 0: del lane['events'][2]
            if mode == 1: lane['events'][2], lane['events'][3] = lane['events'][3], lane['events'][2]
            if mode == 2: lane['events'].insert(3, copy.deepcopy(lane['events'][2]))
            lane['event_count'] = len(lane['events']); self.reject(d)

    def test_exact_operands_vectors_unused_bits(self):
        for where in ('lr', 'object', 'vector', 'unused', 'gate', 'increment', 'address'):
            d = fixture(); events = d['threads'][0]['events']
            if where == 'lr': events[2]['fields'][0] ^= 4
            if where == 'object': events[2]['fields'][1] ^= 1
            if where == 'vector': events[2]['vector'][0] ^= 1
            if where == 'unused': events[3]['fields'][3] = 1
            if where == 'gate': next(e for e in events if e['event'] == 6)['fields'][0] = 1
            if where == 'increment': next(e for e in events if e['event'] == 8)['fields'][2] += 1
            if where == 'address': events[1]['fields'][0] += 16
            self.reject(d)

    def test_two_targets_allow_unobserved_ordinal_gap_not_duplicate(self):
        d = fixture(); lane = d['threads'][0]; second = copy.deepcopy(lane['entries'][0])
        second['invocation']['serial'] = 2; second['parent_event_index'] = 9; second['vector_pre']['fields'][1] = 1
        lane['entries'].append(second); more = copy.deepcopy(lane['events'])
        for ev in more: ev['entry_index'] = 1; ev['invocation']['serial'] = 2; ev['meta']['qpc'] += 50
        lane['events'] += more; lane.update(entry_count=2, event_count=len(lane['events']))
        d['counts'].update(observed_entries=2, admitted_entries=2)
        self.assertEqual(len(decode(d)['candidate_records']), 2)
        for key in ('event', 'ordinal', 'epoch'):
            bad = copy.deepcopy(d); e = bad['threads'][0]['entries'][1]
            if key == 'event': e['parent_event_index'] = 5
            if key == 'ordinal': e['vector_pre']['fields'][1] = 3
            if key == 'epoch': e['work_entry']['invocation']['thread_epoch'] = 6
            self.reject(bad)

    def test_rejected_model_attempt_preserved(self):
        d = fixture(); lane = d['threads'][0]; e = copy.deepcopy(lane['entries'][0]); e['invocation']['serial'] = 2
        m = copy.deepcopy(lane['events'][0]['meta']); m['qpc'] = 200
        d['rejected_entries'] = [dict(reason=4, entry=e, meta=m)]
        lane['model_fault'] = 1; d['observer_failed'] = True
        d['counts'].update(observed_entries=2, dropped_entries=1, rejected_entries=1, observation_errors=1)
        self.assertEqual(decode(d)['vector_target_inventory'], d)
        d['rejected_entries'][0]['entry']['invocation']['serial'] = 1; self.reject(d)

    def test_valid_first_entry_cannot_identity_fail_on_fresh_lane(self):
        d = fixture(); lane = d['threads'][0]
        entry = copy.deepcopy(lane['entries'][0]); meta = copy.deepcopy(lane['events'][0]['meta'])
        lane.update(entries=[], events=[], pending=[], entry_count=0, event_count=0,
                    pending_count=0, parent_work_thread_epoch=0, model_fault=1)
        d['rejected_entries'] = [dict(reason=4, entry=entry, meta=meta)]
        d['counts'].update(observed_entries=1, admitted_entries=0, dropped_entries=1,
                           rejected_entries=1, observation_errors=1, original_exceptions=0)
        d['observer_failed'] = True
        # Bound already accepted every parent/body field; positive own serial1
        # matches this new lane. First Enter has no prior identity to conflict.
        self.reject(d)


if __name__ == '__main__': unittest.main()
