"""Pure synthetic virtual-journal regressions; no game, private data or IO."""
import copy
import unittest
from unittest import mock

import mcla_virtual_detail_decode as decoder
from test_mcla_consumer_detail_decode import fixture as parent_fixture
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def fixture():
    old = parent_fixture(); parent_entry = copy.deepcopy(old['threads'][0]['events'][0]['frame']['entry'])
    parent_entry['invocation']['serial'] = 5
    child = dict(caller_lr=decoder.CALLER, r3=0xffffffffBDF9D620, f1_bits=0x7ff80000deadbeef,
                 indirect_ctr=0xaabbccdd8258D1A0, requested_target=decoder.TARGET, state=1, indirect=True)
    entry = dict(invocation={'thread_epoch': 1, 'serial': 1}, parent_entry=parent_entry,
                 parent_child=child, actual_target=decoder.TARGET, caller_lr=decoder.CALLER,
                 r3=child['r3'], f1_bits=child['f1_bits'])
    access = dict(status=8, entry=copy.deepcopy(parent_entry), child=copy.deepcopy(child), child_index=0)
    f = dict(entry=entry, model_parent={'thread_epoch': 0, 'serial': 0}, stage=0,
             returned_calls=0, calls=[decoder._zero_call() for _ in range(3)])
    events = []
    def append(kind):
        meta = dict.fromkeys(decoder.parent.META_FLAGS, True); meta['parent_admitted'] = False
        meta.update(qpc=110+len(events), native_thread_id=100, observed_child=copy.deepcopy(access))
        events.append(dict(event=kind, meta=meta, frame=copy.deepcopy(f)))
    append(0)
    for i in range(3):
        ctr = 0 if i == 1 else 0xaabbccddFFFFFFFF-i
        f['calls'][i] = dict(caller_lr=decoder.CALL_LRS[i], r3=0xffffffff00000020+i,
            f1_bits=0x8000000000000000 if i == 1 else 0x7ff8000012345678,
            indirect_ctr=ctr, requested_target=ctr & decoder.U32, state=1)
        f['stage'] = 1; append(1)
        f['calls'][i]['state'] = 2; f.update(stage=2, returned_calls=i+1); append(2)
    append(4)
    data = {k: v for k, v in old.items() if k not in decoder.parent.FALSE_FLAGS}
    data.update(dict.fromkeys(decoder.FALSE_FLAGS, False)); data.update(dict.fromkeys(decoder.LIMITATIONS, True))
    data['schema'] = 'mcla-virtual-detail-v1'
    data['threads'] = [dict(thread_epoch=1, native_thread_id=100, parent_detail_thread_epoch=1,
        parent_router_thread_epoch=17, model_fault=0, event_count=8, pending_count=0, events=events, pending=[])]
    return data


def decode(d): return decoder.validate_virtual_detail(raw(d), TOKEN)


def rejection(reason):
    d = fixture(); ev = d['threads'][0]['events'][0]
    e, m = copy.deepcopy(ev['frame']['entry']), copy.deepcopy(ev['meta'])
    e['invocation'] = {'thread_epoch': 0, 'serial': 0}; m.update(qpc=120, binding_matches=False, lane_matches=False)
    if reason == 0: m.update(clock_valid=False, qpc=0)
    if reason == 1: m['base_nonnull'] = False
    if reason == 2: e['r3'] += 1
    if reason >= 3: m.update(binding_matches=True, lane_matches=reason >= 4)
    if reason == 5: e['invocation'] = {'thread_epoch': 1, 'serial': 2}
    return dict(reason=reason, entry=e, meta=m)


class VirtualDetailTests(unittest.TestCase):
    def reject(self, d):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature evaluation')) as evaluate:
            with self.assertRaises(ValueError): decode(d)
        evaluate.assert_not_called()

    def test_full_route_opaque_operands_detached_noio_no_proofs(self):
        d = fixture(); before = copy.deepcopy(d)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')): r = decode(d)
        self.assertEqual(d, before); self.assertEqual(len(r['candidate_records']), 1)
        self.assertEqual(r['candidate_records'][0]['parent_detail_invocation']['serial'], 5)
        f = r['candidate_records'][0]['frame']
        self.assertEqual(f['calls'][1]['f1_bits'], 0x8000000000000000)
        self.assertNotEqual(f['calls'][1]['f1_bits'], f['entry']['f1_bits'])
        self.assertEqual(f['calls'][0]['requested_target'], decoder.U32)
        self.assertEqual(f['calls'][1]['requested_target'], 0)
        self.assertEqual(f['calls'][0]['indirect_ctr'], 0xaabbccddFFFFFFFF)
        for k in decoder.FALSE_FLAGS: self.assertIs(r[k], False)
        self.assertNotIn('PASS', r['status']); self.assertNotIn('delta_sum', r)
        f['entry']['parent_entry']['r3'] = 9
        self.assertNotEqual(r['virtual_detail_inventory']['threads'][0]['events'][0]['frame']['entry']['parent_entry']['r3'], 9)

    def test_pending_every_stage_preserved(self):
        for n in range(1, 8):
            d = fixture(); lane = d['threads'][0]; lane['events'] = lane['events'][:n]
            lane.update(event_count=n, pending_count=1, pending=[copy.deepcopy(lane['events'][-1]['frame'])])
            r = decode(d); self.assertEqual(r['candidate_records'], [])
            self.assertEqual(r['pending_invocations'], 1); self.assertIn('PENDING_VIRTUAL_INVOCATIONS', r['candidate_blocking_issues'])

    def test_exception_any_valid_stage_without_invented_calls(self):
        for n in range(1, 8):
            d = fixture(); lane = d['threads'][0]; lane['events'] = lane['events'][:n]
            end = copy.deepcopy(lane['events'][-1]); end['event'] = 5; end['meta']['qpc'] += 1
            lane['events'].append(end); lane['event_count'] = n+1; d['counts']['original_exceptions'] = 1
            r = decode(d); self.assertEqual(r['candidate_records'], [])
            self.assertFalse(r['completed_invocations'][0]['normal_return'])
            self.assertEqual(r['completed_invocations'][0]['frame'], end['frame'])

    def test_each_virtual_exception_preserved(self):
        for c in range(3):
            d = fixture(); lane = d['threads'][0]; lane['events'] = lane['events'][:2+2*c]
            e = copy.deepcopy(lane['events'][-1]); e['event'] = 3; e['meta']['qpc'] += 1
            e['frame']['stage'] = 3; e['frame']['calls'][c]['state'] = 3
            end = copy.deepcopy(e); end['event'] = 5; end['meta']['qpc'] += 1
            lane['events'] += [e, end]; lane['event_count'] = len(lane['events']); d['counts']['original_exceptions'] = 1
            r = decode(d); self.assertEqual(r['candidate_records'], [])
            self.assertEqual(r['completed_invocations'][0]['frame']['returned_calls'], c)

    def test_nested_distinct_parent_and_exact_lifo(self):
        d = fixture(); lane = d['threads'][0]; outer = lane['events']; inner = copy.deepcopy(outer)
        for e in inner:
            f, m = e['frame'], e['meta']; f['entry']['invocation']['serial'] = 2
            f['entry']['parent_entry']['invocation']['serial'] = 6; f['entry']['parent_entry']['router']['serial'] = 21
            m['observed_child']['entry'] = copy.deepcopy(f['entry']['parent_entry'])
            f['model_parent'] = {'thread_epoch': 1, 'serial': 1}; m['parent_admitted'] = True
        lane['events'] = outer[:2] + inner + outer[2:]
        for i, e in enumerate(lane['events']): e['meta']['qpc'] = 110+i
        lane['event_count'] = 16; d['counts'].update(observed_entries=2, admitted_entries=2)
        self.assertEqual([r['invocation']['serial'] for r in decode(d)['candidate_records']], [2, 1])
        lane['events'][3]['frame']['entry']['invocation']['serial'] = 1; self.reject(d)

    def test_empty_consumer_owned_lanes_and_no_lanes_not_admission(self):
        for present in (False, True):
            d = fixture(); d['counts'].update(observed_entries=0, admitted_entries=0, thread_lifetimes=int(present))
            if present: d['threads'][0].update(events=[], event_count=0, parent_detail_thread_epoch=0, parent_router_thread_epoch=0)
            else: d['threads'] = []
            r = decode(d); self.assertEqual(r['candidate_records'], []); self.assertIn('EMPTY_VIRTUAL_INVENTORY', r['candidate_blocking_issues'])
            if present: d['threads'][0]['parent_detail_thread_epoch'] = 1; self.reject(d)

    def test_shared_parent_only_health_is_retained(self):
        d = fixture(); d['observer_failed'] = True
        r = decode(d); self.assertEqual(r['virtual_detail_inventory']['counts']['observation_errors'], 0)
        self.assertEqual(r['candidate_records'], []); self.assertIn('OBSERVER_FAILED', r['candidate_blocking_issues'])

    def test_full_parent_snapshot_drift_never_accepted(self):
        for which, key in (('entry', 'r3'), ('entry', 'f1_bits'), ('child', 'r3'), ('child', 'f1_bits'), ('child', 'indirect_ctr')):
            d = fixture(); m = d['threads'][0]['events'][-1]['meta']; m['observed_child'][which][key] ^= 1<<40
            self.reject(d); m['binding_matches'] = False; d['observer_failed'] = True
            d['counts'].update(binding_mismatches=1, observation_errors=1)
            self.assertEqual(decode(d)['candidate_records'], [])

    def test_invalid_accessor_status_clears_every_payload_field(self):
        d = fixture(); m = d['threads'][0]['events'][-1]['meta']
        m.update(binding_matches=False, observed_child=dict(status=3, entry=decoder._zero_parent(), child=decoder.parent._zero_child(), child_index=0))
        d['observer_failed'] = True; d['counts'].update(binding_mismatches=1, observation_errors=1)
        self.assertEqual(decode(d)['candidate_records'], [])
        m['observed_child']['entry']['r3'] = 1; self.reject(d)

    def test_parent_identity_route_child0_and_entry_bits_strict(self):
        for edit in (lambda e:e['parent_entry']['update'].update(phase_call_id=0),
            lambda e:e['parent_entry'].update(actual_target=decoder.TARGET),
            lambda e:e['parent_child'].update(state=2), lambda e:e['parent_child'].update(indirect=False),
            lambda e:e['parent_child'].update(caller_lr=decoder.CALL_LRS[0]),
            lambda e:e['parent_child'].update(requested_target=0), lambda e:e.update(actual_target=0),
            lambda e:e.update(caller_lr=decoder.CALL_LRS[0]), lambda e:e.update(r3=1), lambda e:e.update(f1_bits=1)):
            d = fixture(); edit(d['threads'][0]['events'][-1]['frame']['entry']); self.reject(d)
        d = fixture(); d['threads'][0]['events'][-1]['meta']['observed_child']['child_index'] = 1; self.reject(d)

    def test_duplicate_or_reordered_own_parent_and_router_ids(self):
        for field, value in (('own', 1), ('parent', 5), ('router', 20)):
            d = fixture(); lane = d['threads'][0]; extra = copy.deepcopy(lane['events'])
            for e in extra:
                f, m = e['frame'], e['meta']; f['entry']['invocation']['serial'] = value if field == 'own' else 2
                f['entry']['parent_entry']['invocation']['serial'] = value if field == 'parent' else 6
                f['entry']['parent_entry']['router']['serial'] = value if field == 'router' else 21
                m['observed_child']['entry'] = copy.deepcopy(f['entry']['parent_entry']); m['qpc'] += 20
            lane['events'] += extra; lane['event_count'] = 16; d['counts'].update(observed_entries=2, admitted_entries=2)
            self.reject(d)

    def test_multilane_independent_lifetimes_not_tid_inference(self):
        d = fixture(); lane = copy.deepcopy(d['threads'][0]); lane.update(thread_epoch=2, native_thread_id=200,
            parent_detail_thread_epoch=2, parent_router_thread_epoch=18)
        for e in lane['events']:
            f, m = e['frame'], e['meta']; f['entry']['invocation']['thread_epoch'] = 2
            p = f['entry']['parent_entry']; p['invocation']['thread_epoch'] = 2; p['router']['thread_epoch'] = 18
            p['update'].update(thread_epoch=24, native_thread_id=200)
            m.update(native_thread_id=200); m['observed_child']['entry'] = copy.deepcopy(p)
        d['threads'].append(lane); d['counts'].update(observed_entries=2, admitted_entries=2, thread_lifetimes=2)
        self.assertEqual(len(decode(d)['candidate_records']), 2)
        lane['parent_router_thread_epoch'] = 17; self.reject(d)

    def test_snapshot_transitions_all_tail_fields_and_pending_exact(self):
        edits = (lambda l:l['events'][2]['frame'].update(returned_calls=2),
            lambda l:l['events'][3]['frame']['calls'][0].update(r3=0), lambda l:l['events'][1].update(event=4),
            lambda l:l['events'][2]['frame'].update(stage=3), lambda l:l['events'][-1]['frame']['calls'][1].update(f1_bits=0),
            lambda l:l.update(pending=[copy.deepcopy(l['events'][0]['frame'])], pending_count=1))
        for edit in edits: d = fixture(); edit(d['threads'][0]); self.reject(d)

    def test_unknown_requested_virtual_targets_are_raw_not_guessed(self):
        d = fixture()
        for e in d['threads'][0]['events']:
            c = e['frame']['calls'][1]
            if c['state']: c.update(indirect_ctr=0xffffffffDEADBEEF, requested_target=0xDEADBEEF)
        self.assertEqual(len(decode(d)['candidate_records']), 1)
        d['threads'][0]['events'][-1]['frame']['calls'][1]['requested_target'] = 1; self.reject(d)

    def test_failed_cut_and_invalid_clocks_remain_raw_globally_blocking(self):
        for cut in (0, 115, decoder.QMAX+1):
            d = fixture(); d.update(freeze_qpc=cut, clock_failed=True, observer_failed=True)
            r = decode(d); self.assertEqual(r['virtual_detail_inventory']['freeze_qpc'], cut); self.assertEqual(r['candidate_records'], [])
            d['clock_failed'] = False; self.reject(d)
        d = fixture(); d['threads'][0]['events'][-1]['meta'].update(qpc=0, clock_valid=False)
        self.reject(d); d.update(clock_failed=True, observer_failed=True)
        self.assertEqual(decode(d)['candidate_records'], [])
        for tick in (0, 109, decoder.QMAX+1):
            d = fixture(); d.update(clock_failed=True, observer_failed=True, freeze_qpc=0)
            d['threads'][0]['events'][-1]['meta']['qpc'] = tick; self.reject(d)

    def test_all_rejection_stages_preserved_no_fabricated_admission(self):
        for reason in range(6):
            d = fixture(); d['observer_failed'] = True; d['rejected_entries'] = [rejection(reason)]
            d['counts'].update(observed_entries=2, dropped_entries=1, rejected_entries=1, observation_errors=1)
            if reason == 0: d['clock_failed'] = True
            if reason == 5: d['threads'][0]['model_fault'] = 1
            self.assertEqual(decode(d)['candidate_records'], [])
        d = fixture(); d['observer_failed'] = True; r = rejection(1); r['meta']['base_nonnull'] = True
        d['rejected_entries'] = [r]; d['counts'].update(observed_entries=2, dropped_entries=1, rejected_entries=1, observation_errors=1)
        self.assertEqual(decode(d)['candidate_records'], [])  # Shared parent already failed.
        d = fixture(); d['observer_failed'] = True; r = rejection(4); r['meta']['parent_admitted'] = True
        d['rejected_entries'] = [r]; d['counts'].update(observed_entries=2, dropped_entries=1, rejected_entries=1, observation_errors=1)
        self.assertEqual(decode(d)['candidate_records'], [])  # Parent sampled before serial-exhaustion check.

    def test_rejected_model_id_lifetime_order_and_failed_empty_lane(self):
        d = fixture(); d['observer_failed'] = True; d['threads'][0]['model_fault'] = 1
        d['rejected_entries'] = [rejection(5)]; d['counts'].update(observed_entries=2, dropped_entries=1, rejected_entries=1, observation_errors=1)
        for edit in (lambda d:d['threads'][0].update(model_fault=0),
                     lambda d:d['rejected_entries'][0]['entry']['invocation'].update(serial=1),
                     lambda d:d['rejected_entries'][0]['entry']['invocation'].update(thread_epoch=2)):
            bad = copy.deepcopy(d); edit(bad); self.reject(bad)
        lane = d['threads'][0]; lane.update(events=[], event_count=0, parent_detail_thread_epoch=0, parent_router_thread_epoch=0)
        d['counts'].update(observed_entries=1, admitted_entries=0); d['rejected_entries'][0]['entry']['invocation']['serial'] = 1
        self.assertEqual(decode(d)['candidate_records'], [])

    def test_counter_relations_and_shared_sticky_fault(self):
        for key in ('observed_entries', 'admitted_entries', 'original_exceptions', 'thread_lifetimes', 'rejected_entries', 'rejection_overflow'):
            d = fixture(); d['counts'][key] += 1; self.reject(d)
        d = fixture(); d['counts']['binding_mismatches'] = 1; self.reject(d)
        d['observer_failed'] = True; self.reject(d)  # Every mismatch has its own VirtualError.
        d['counts']['observation_errors'] = 1; self.assertEqual(decode(d)['candidate_records'], [])
        d = fixture(); d['counter_saturated'] = True; self.reject(d)
        d['observer_failed'] = True; d['counts']['observed_entries'] = 2**64-1
        self.assertEqual(decode(d)['candidate_records'], [])

    def test_rejection_overflow_requires_full_stored_inventory_and_conservation(self):
        d = fixture(); d['observer_failed'] = True
        d['rejected_entries'] = [copy.deepcopy(rejection(1)) for _ in range(256)]
        d['counts'].update(observed_entries=258, dropped_entries=257, rejected_entries=256,
                           rejection_overflow=1, observation_errors=257)
        self.assertEqual(decode(d)['candidate_records'], [])
        d['rejected_entries'].pop(); d['counts'].update(rejected_entries=255, dropped_entries=256, observed_entries=257)
        self.reject(d)

    def test_model_faults_require_flags_and_capacity_evidence(self):
        for fault in (1, 2):
            d = fixture(); d['threads'][0]['model_fault'] = fault; self.reject(d)
            d['observer_failed'] = True; self.assertEqual(decode(d)['candidate_records'], [])
        for fault in (3, 4):
            d = fixture(); d['observer_failed'] = True; d['threads'][0]['model_fault'] = fault; self.reject(d)

    def test_full_metadata_tail_before_evaluator_and_exact_bool_types(self):
        for edit in (lambda d:d['threads'][0]['events'][-1]['meta'].update(qpc=True),
            lambda d:d['threads'][0]['events'][-1]['frame']['calls'][-1].update(extra=0),
            lambda d:d['threads'][0].update(parent_detail_thread_epoch=True), lambda d:d.update(native_admitted=True),
            lambda d:d.update(shared_consumer_clock_and_cut=False), lambda d:d['counts'].update(observation_errors=True)):
            d = fixture(); edit(d); self.reject(d)

    def test_strict_bytes_json_types_duplicate_keys_and_session(self):
        for value in (b'', bytearray(raw(fixture())), b'{"x":1,"x":2}', b'{"x":NaN}', b'{"x":1.0}', b'\xff',
                      b'['*2000+b']'*2000, b' '*(decoder.MAX_BYTES+1)):
            with self.assertRaises(ValueError): decoder.validate_virtual_detail(value, TOKEN)
        for token in ('0'*32, 'F'*32, True):
            with self.assertRaises(ValueError): decoder.validate_virtual_detail(raw(fixture()), token)
        d = fixture(); d['token'] = '1'*32; self.reject(d)


if __name__ == '__main__': unittest.main()
