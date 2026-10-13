"""Synthetic exact-ID source/virtual/work relations; no native or private IO."""
import copy
import unittest
from unittest import mock

import mcla_work_emission_join as decoder
from test_mcla_work_emission_decode import fixture as work_fixture
from test_mcla_frame_virtual_join import fixture as virtual_fixture, body_events
from test_mcla_frame_consumer_join import add_detail
from test_mcla_frame_router_join import add_router
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def set_call0_tail(virtual):
    for ev in virtual['threads'][0]['events']:
        call = ev['frame']['calls'][0]
        if call['state']:
            call.update(requested_target=decoder.work_decoder.TAIL,
                        indirect_ctr=0xaabbccdd00000000 | decoder.work_decoder.TAIL)


def work_body(parent_events, serial=1, index=0, branch='vector'):
    pre = next(e for e in parent_events if e['event'] == 1 and e['frame']['returned_calls'] == 0)
    e = dict(invocation=dict(thread_epoch=1, serial=serial),
        virtual_entry=copy.deepcopy(pre['frame']['entry']), active_call0=copy.deepcopy(pre['frame']['calls'][0]),
        actual_target=decoder.work_decoder.BODY, caller_lr=decoder.work_decoder.CALLER,
        r3=pre['frame']['calls'][0]['r3'], f1_bits=pre['frame']['calls'][0]['f1_bits'])
    events = copy.deepcopy(work_fixture(branch)['threads'][0]['events'])
    for ev in events:
        ev.update(invocation=copy.deepcopy(e['invocation']), entry_index=index)
        ev['meta'].update(qpc=pre['meta']['qpc'], native_thread_id=pre['meta']['native_thread_id'])
    return e, events


def fixture(branch='vector'):
    values = virtual_fixture(); virtual = values[0]; set_call0_tail(virtual)
    work = work_fixture(branch); lane = work['threads'][0]
    e, events = work_body(virtual['threads'][0]['events'], branch=branch)
    lane.update(native_thread_id=events[0]['meta']['native_thread_id'],
        parent_virtual_thread_epoch=e['virtual_entry']['invocation']['thread_epoch'],
        parent_detail_thread_epoch=e['virtual_entry']['parent_entry']['invocation']['thread_epoch'],
        parent_router_thread_epoch=e['virtual_entry']['parent_entry']['router']['thread_epoch'],
        entries=[e], events=events, event_count=len(events))
    for field in ('token', 'clock', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc'):
        work[field] = virtual[field]
    return (work,)+values


def decode(values):
    return decoder.validate_work_emission_join(*(raw(x) for x in values[:9]), values[9], TOKEN)


def analyses(values):
    return (decoder.work_decoder.validate_work_emission(raw(values[0]), TOKEN),
        decoder.virtual_join.validate_frame_virtual_join(*(raw(x) for x in values[1:9]), values[9], TOKEN))


def add_second_virtual(values, with_work=False):
    # Build a second explicit router/consumer/virtual chain in the same Update.
    add_router(values[3:]); add_detail(values[2:])
    lane = values[1]['threads'][0]
    lane['events'] += body_events(values[2]['threads'][0]['events'][9:], 2)
    lane['event_count'] = len(lane['events']); values[1]['counts'].update(observed_entries=2, admitted_entries=2)
    set_call0_tail(values[1])
    if with_work:
        wlane = values[0]['threads'][0]; e, events = work_body(lane['events'][8:], 2, 1, 'job')
        wlane['entries'].append(e); wlane['events'] += events
        wlane.update(entry_count=2, event_count=len(wlane['events']))
        values[0]['counts'].update(observed_entries=2, admitted_entries=2, tail_scopes=2)


class WorkEmissionJoinTests(unittest.TestCase):
    def reject(self, values):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature join')) as evaluate:
            with self.assertRaises(ValueError): decode(values)
        evaluate.assert_not_called()

    def test_complete_raw_chain_preserved_detached_noio_falseproof(self):
        values = fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')): result = decode(values)
        self.assertEqual(values, before); self.assertEqual(len(result['candidate_records']), 91)
        w, v = analyses(values)
        self.assertEqual(result['work_analysis'], w); self.assertEqual(result['frame_virtual_analysis'], v)
        record = result['candidate_records'][0]
        self.assertEqual(record['work_emission_invocations'], [dict(thread_epoch=1, serial=1)])
        self.assertEqual(record['expected_work_body_count'], 1)
        self.assertEqual(record['compact_work_event_groups'][0]['frame']['scalar_emissions'], 2)
        self.assertEqual(len(result['source_work_inventory']), 91)
        self.assertIs(result['prelude_unobserved'], True)
        for k in decoder.FALSE_FLAGS: self.assertIs(result[k], False)
        self.assertNotIn('PASS', result['status']); self.assertNotIn('f1_sum', result)
        record['compact_work_event_groups'][0]['entry']['r3'] = 9
        self.assertNotEqual(result['candidate_records'][1]['compact_work_event_groups'][0]['entry']['r3'], 9)

    def test_all_branch_replays_keep_opaque_emissions(self):
        for branch in ('vector', 'job', 'zero'):
            r = decode(fixture(branch)); self.assertEqual(len(r['candidate_records']), 91)
            self.assertEqual(r['candidate_records'][0]['work_emission_group_count'], 1)

    def test_shared_headers_strict_and_independent_cut_order(self):
        for field in ('token', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency'):
            v = fixture(); v[0][field] = '1'*32 if field == 'token' else v[0][field]+1; self.reject(v)
        v = fixture(); v[0]['freeze_qpc'] += 1; self.reject(v)
        v = fixture(); tick = v[0]['threads'][0]['events'][0]['meta']['qpc']; v[0]['freeze_qpc'] = tick
        self.assertEqual(decode(v)['candidate_records'], [])

    def test_failed_cut_is_retained_globally_inconclusive(self):
        v = fixture(); v[0].update(freeze_qpc=0, observer_failed=True, clock_failed=True)
        r = decode(v); self.assertEqual(r['candidate_records'], [])
        self.assertEqual(r['work_analysis']['work_emission_inventory']['freeze_qpc'], 0)

    def test_missing_body_excludes_whole_two_parent_source(self):
        v = fixture(); add_second_virtual(v)
        r = decode(v); self.assertEqual(r['candidate_records'], [])
        self.assertEqual(len(r['excluded_candidate_records']), 91)
        self.assertEqual(r['excluded_candidate_records'][0]['expected_work_body_count'], 2)
        self.assertEqual(r['candidate_blocking_issues'], [])
        self.assertEqual(r['source_work_inventory'][0]['exclusion_reason'], 'MISSING_WORK_BODY_FOR_APPLICABLE_VIRTUAL_CALL0')

    def test_complete_two_body_source_and_one_contradiction_global(self):
        v = fixture(); add_second_virtual(v, True)
        self.assertEqual(decode(v)['candidate_records'][0]['work_emission_group_count'], 2)
        v[0]['threads'][0]['entries'][1]['virtual_entry']['parent_entry']['r3'] ^= 1 << 40
        r = decode(v); self.assertEqual(r['candidate_records'], [])
        self.assertIn('WORK_VIRTUAL_IDENTITY_OR_SAVED_OPERAND_MISMATCH', r['candidate_blocking_issues'])

    def test_full_saved_ancestry_call_and_upper_bits(self):
        for kind, key in (('parent', 'r3'), ('parent', 'f1_bits'), ('call', 'r3'),
                          ('call', 'f1_bits'), ('call', 'indirect_ctr')):
            v = fixture(); e = v[0]['threads'][0]['entries'][0]
            if kind == 'parent': e['virtual_entry']['parent_entry'][key] ^= 1 << 40
            else:
                e['active_call0'][key] ^= 1 << 40
                if key in ('r3', 'f1_bits'): e[key] ^= 1 << 40
            r = decode(v); self.assertEqual(r['candidate_records'], [])
            self.assertIn('WORK_VIRTUAL_IDENTITY_OR_SAVED_OPERAND_MISMATCH', r['candidate_blocking_issues'])

    def test_orphan_global_even_if_source_does_not_select_it(self):
        v = fixture(); v[0]['threads'][0]['entries'][0]['virtual_entry']['invocation']['serial'] += 1
        r = decode(v); self.assertEqual(r['candidate_records'], [])
        self.assertIn('ORPHAN_WORK_BODY_WITHOUT_APPLICABLE_VIRTUAL_CALL0', r['candidate_blocking_issues'])
        self.assertEqual(len(r['extraneous_work_records']), 1)

    def test_duplicate_body_and_duplicate_own_id_failclosed(self):
        w, v = analyses(fixture()); extra = copy.deepcopy(w['completed_invocations'][0])
        extra['entry']['invocation']['serial'] = 2; w['completed_invocations'].append(extra)
        issues = set(); self.assertEqual(decoder._evaluate(w, v, issues, set())[0], [])
        self.assertIn('DUPLICATE_WORK_BODY_FOR_VIRTUAL_CALL0', issues)
        extra['entry']['invocation']['serial'] = 1
        with self.assertRaises(ValueError): decoder._evaluate(w, v, set(), set())

    def test_unselected_contradiction_cannot_be_hidden(self):
        w, v = analyses(fixture()); v['candidate_records'] = []
        w['completed_invocations'][0]['entry']['virtual_entry']['parent_entry']['r3'] += 1
        issues = set(); self.assertEqual(decoder._evaluate(w, v, issues, set())[0], [])
        self.assertIn('WORK_VIRTUAL_IDENTITY_OR_SAVED_OPERAND_MISMATCH', issues)

    def test_every_event_inside_exact_call0_pair(self):
        for delta in (-1, 1):
            values = fixture()
            for ev in values[0]['threads'][0]['events']: ev['meta']['qpc'] += delta
            r = decode(values); self.assertEqual(r['candidate_records'], [])
            self.assertIn('WORK_EVENTS_OUTSIDE_EXACT_CALL0_OR_BOTH_CUTS', r['candidate_blocking_issues'])

    def test_strict_source_frontier_touch_and_cross(self):
        for delta in (0, -1):
            w, v = analyses(fixture()); tick = w['completed_invocations'][0]['events'][0]['meta']['qpc']
            v['frame_consumer_analysis']['frame_router_analysis']['timing_frontier_analysis'][
                'closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive'] = tick+delta
            accepted, excluded, extras, inventory = decoder._evaluate(w, v, set(), set())
            self.assertEqual(accepted, []); self.assertEqual(len(excluded), 91); self.assertEqual(extras, [])
            self.assertEqual(inventory[0]['exclusion_reason'], 'WORK_EVENTS_TOUCH_OR_CROSS_SOURCE_CLOSED_FRONTIER')

    def test_later_parent_normal_completion_never_backfills(self):
        w, v = analyses(fixture()); tick = w['completed_invocations'][0]['events'][0]['meta']['qpc']
        w['work_emission_inventory']['freeze_qpc'] = tick
        v['virtual_analysis']['completed_invocations'][0]['events'][-1]['meta']['qpc'] = tick+1
        accepted, excluded, _, _ = decoder._evaluate(w, v, set(), set())
        self.assertEqual(accepted, []); self.assertEqual(len(excluded), 91)
        self.assertEqual(excluded[0]['reason'], 'PARENT_VIRTUAL_COMPLETION_AT_OR_AFTER_WORK_CUT')

    def test_equal_qpc_parent_completion_is_not_order_proof(self):
        w, v = analyses(fixture())
        tick = w['completed_invocations'][0]['events'][0]['meta']['qpc']
        w['work_emission_inventory']['freeze_qpc'] = tick
        v['virtual_analysis']['completed_invocations'][0]['events'][-1]['meta']['qpc'] = tick
        accepted, excluded, _, _ = decoder._evaluate(w, v, set(), set())
        self.assertEqual(accepted, [])
        self.assertEqual(len(excluded), 91)

    def test_pending_or_fault_never_healed_by_other_chain(self):
        for flag in ('observer_failed', 'disarmed_at_cut'):
            values = fixture(); values[0][flag] = True; self.assertEqual(decode(values)['candidate_records'], [])
            values = fixture(); values[1][flag] = True; self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); lane = values[0]['threads'][0]; lane['events'].pop(); lane['event_count'] -= 1
        f = decoder.work_decoder._new_frame(0, dict(thread_epoch=0, serial=0))
        for ev in lane['events'][1:]: decoder.work_decoder._step(f, ev)
        lane.update(pending_count=1, pending=[f])
        self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); lane = values[1]['threads'][0]; lane['events'].pop(); lane['event_count'] -= 1
        lane.update(pending_count=1, pending=[copy.deepcopy(lane['events'][-1]['frame'])])
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_other_call0_requested_target_unknown_not_missing(self):
        w, v = analyses(fixture()); parent = copy.deepcopy(v['virtual_analysis']['completed_invocations'][0])
        parent['frame']['entry']['invocation']['serial'] = 2
        parent['frame']['calls'][0]['requested_target'] = 0x82300000
        v['virtual_analysis']['completed_invocations'].append(parent)
        for source in v['candidate_records']: source['virtual_detail_invocations'].append(dict(thread_epoch=1, serial=2))
        contexts = set(); accepted, _, _, _ = decoder._evaluate(w, v, set(), contexts)
        self.assertEqual(len(accepted), 91); self.assertEqual(accepted[0]['expected_work_body_count'], 1)
        self.assertIn('OTHER_VIRTUAL_CALL0_TARGETS_RETAINED_UNKNOWN', contexts)

    def test_full_raw_chain_malformed_last_row_before_join(self):
        for change in (lambda v: v[0]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v: v[1]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v: v[4]['records'][-1]['incoming'].update(f1_bits=True),
                       lambda v: v[8]['records'][-1].update(slot=999)):
            values = fixture(); change(values); self.reject(values)
        values = fixture()
        with self.assertRaises(ValueError):
            decoder.validate_work_emission_join(values[0], *(raw(x) for x in values[1:9]), values[9], TOKEN)

    def test_parent_lane_mismatch_rejected_before_evaluation(self):
        values = fixture(); lane = values[0]['threads'][0]; lane['parent_virtual_thread_epoch'] = 2
        lane['entries'][0]['virtual_entry']['invocation']['thread_epoch'] = 2
        self.reject(values)


if __name__ == '__main__': unittest.main()
