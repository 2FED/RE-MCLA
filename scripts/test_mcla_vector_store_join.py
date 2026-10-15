"""Complete synthetic raw chain to exact slot28 state-store witness. No IO."""
import copy
import unittest
from unittest import mock

import mcla_vector_store_join as decoder
from test_mcla_vector_store_decode import fixture as store_fixture, prefix as store_prefix
from test_mcla_vector_target_join import fixture as target_fixture
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def fixture(second_work=False, unknown=False):
    values = target_fixture(second_work=second_work)
    target = values[0]; store = store_fixture(); lane = store['threads'][0]
    lane.update(entries=[], events=[], parent_target_thread_epoch=1,
                native_thread_id=target['threads'][0]['native_thread_id'])
    for field in ('token', 'clock', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc'):
        store[field] = target[field]
    for index, pre in enumerate(target['threads'][0]['events']):
        if pre['event'] != 2 or pre['site'] != 2:
            continue
        if unknown and pre['entry_index'] == 1:
            pre['fields'][2] = 0xabcdef0012345678
            continue
        pre['fields'][2] = 0xabcdef0000000000 | decoder.store_decoder.TARGET
        part = store_fixture()['threads'][0]
        entry = part['entries'][0]; own_index = len(lane['entries'])
        entry.update(invocation=dict(thread_epoch=1, serial=own_index+1),
            target_entry=copy.deepcopy(target['threads'][0]['entries'][pre['entry_index']]),
            parent_event_index=index, slot28_pre=dict(fields=copy.deepcopy(pre['fields']),
            vector=copy.deepcopy(pre['vector'])), r3=pre['fields'][1], v1=copy.deepcopy(pre['vector']))
        for event in part['events']:
            event.update(invocation=copy.deepcopy(entry['invocation']), entry_index=own_index)
            event['meta'].update(qpc=pre['meta']['qpc'], native_thread_id=pre['meta']['native_thread_id'])
            if event['event'] in (1, 2, 4, 6):
                offset = {1: 256, 2: 272, 4: 288, 6: 256}[event['event']]
                event['fields'][0] = ((entry['r3']+offset) & 0xffffffff) & ~15
            if event['event'] == 7:
                event['fields'][1] = entry['r3']
        lane['entries'].append(entry); lane['events'] += part['events']
    count = len(lane['entries'])
    lane.update(entry_count=count, event_count=len(lane['events']))
    store['counts'].update(observed_entries=count, admitted_entries=count)
    return (store,)+values


def decode(values):
    return decoder.validate_vector_store_join(*(raw(x) for x in values[:11]), values[11], TOKEN)


def analyses(values):
    return (decoder.store_decoder.validate_vector_store(raw(values[0]), TOKEN),
            decoder.target_join.validate_vector_target_join(*(raw(x) for x in values[1:11]), values[11], TOKEN))


def keep_first_store(values):
    lane = values[0]['threads'][0]
    lane['entries'] = lane['entries'][:1]
    lane['events'] = [e for e in lane['events'] if e['entry_index'] == 0]
    lane.update(entry_count=1, event_count=len(lane['events']))
    values[0]['counts'].update(observed_entries=1, admitted_entries=1)


class VectorStoreJoinTests(unittest.TestCase):
    def reject(self, values):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature join')) as evaluate:
            with self.assertRaises(ValueError): decode(values)
        evaluate.assert_not_called()

    def test_full_chain_detached_noio_falseproof(self):
        values = fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')):
            result = decode(values)
        self.assertEqual(values, before); self.assertEqual(len(result['candidate_records']), 91)
        store, target = analyses(values)
        self.assertEqual(result['vector_store_analysis'], store)
        self.assertEqual(result['frame_target_analysis'], target)
        row = result['candidate_records'][0]
        self.assertEqual(row['vector_store_group_count'], 2)
        self.assertEqual(row['vector_target_group_count'], 2)
        self.assertGreaterEqual(len(row['unknown_target_calls']), 8)
        for key in decoder.FALSE_FLAGS: self.assertIs(result[key], False)
        self.assertNotIn('PASS', result['status']); self.assertNotIn('f1_sum', result)
        row['compact_store_event_groups'][0]['entry']['r3'] = 99
        self.assertNotEqual(result['candidate_records'][1]['compact_store_event_groups'][0]['entry']['r3'], 99)

    def test_lane_global_index_not_completed_row_local(self):
        values = fixture(second_work=True); result = decode(values)
        self.assertEqual(result['candidate_records'][0]['vector_store_group_count'], 4)
        entries = values[0]['threads'][0]['entries']
        self.assertGreater(entries[1]['parent_event_index'], entries[0]['parent_event_index'])
        store, target = analyses(values)
        store['completed_invocations'][1]['entry']['parent_event_index'] = entries[0]['parent_event_index']
        issues = set(); self.assertEqual(decoder._evaluate(store, target, issues, set())[0], [])
        self.assertIn('ORPHAN_STORE_WITHOUT_EXACT_APPLICABLE_SLOT28_PRE', issues)

    def test_missing_body_excludes_whole_source_group(self):
        values = fixture(); keep_first_store(values); result = decode(values)
        self.assertEqual(result['candidate_records'], []); self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertEqual(len(result['excluded_candidate_records']), 91)
        self.assertEqual(result['excluded_candidate_records'][0]['expected_store_count'], 2)
        self.assertEqual(result['source_store_inventory'][0]['exclusion_reason'],
                         'MISSING_STORE_FOR_APPLICABLE_TARGET_SLOT28_PRE')

    def test_other_inner_calls_and_other_slot28_retained_unknown(self):
        result = decode(fixture(unknown=True)); self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['candidate_records'][0]['vector_store_group_count'], 1)
        self.assertIn('OTHER_TARGET_INNER_CALLS_RETAINED_UNKNOWN', result['retained_context_issues'])
        unknown = result['candidate_records'][0]['unknown_target_calls']
        self.assertTrue(any(x['call_pre']['site'] == 2 for x in unknown))
        self.assertTrue(any(x['call_pre']['site'] != 2 for x in unknown))

    def test_unselected_counts_cannot_hide_missing_selected_body(self):
        values = fixture(); keep_first_store(values)
        values[0]['counts'].update(observed_entries=2, unselected_entries=1, unselected_absent_entries=1)
        result = decode(values)
        self.assertEqual(result['candidate_records'], []); self.assertEqual(len(result['excluded_candidate_records']), 91)
        self.assertEqual(result['excluded_candidate_records'][0]['reason'], 'MISSING_STORE_FOR_APPLICABLE_TARGET_SLOT28_PRE')

    def test_raw_saved_upper_ctr_and_f1_drift_global(self):
        values = fixture(); values[0]['threads'][0]['entries'][0]['slot28_pre']['fields'][2] ^= 1 << 40
        self.assertIn('STORE_TARGET_IDENTITY_OR_SAVED_OPERAND_MISMATCH', decode(values)['candidate_blocking_issues'])
        values = fixture(); values[0]['threads'][0]['entries'][0]['target_entry']['vector_pre']['fields'][3] ^= 1 << 40
        self.assertIn('STORE_TARGET_IDENTITY_OR_SAVED_OPERAND_MISMATCH', decode(values)['candidate_blocking_issues'])

    def test_saved_operands_global_even_when_no_source_selected(self):
        for change in (lambda e: e['target_entry']['work_entry']['virtual_entry']['parent_entry'].__setitem__('r3', 999),
                       lambda e: e['slot28_pre']['fields'].__setitem__(2, e['slot28_pre']['fields'][2] ^ (1 << 40)),
                       lambda e: e['slot28_pre']['vector'].__setitem__(0, 999)):
            store, target = analyses(fixture()); target['candidate_records'] = []
            change(store['completed_invocations'][0]['entry']); issues = set()
            self.assertEqual(decoder._evaluate(store, target, issues, set())[0], [])
            self.assertIn('STORE_TARGET_IDENTITY_OR_SAVED_OPERAND_MISMATCH', issues)

    def test_wrong_raw_global_index_and_upper_r3(self):
        values = fixture(); values[0]['threads'][0]['entries'][0]['parent_event_index'] += 1
        self.assertIn('ORPHAN_STORE_WITHOUT_EXACT_APPLICABLE_SLOT28_PRE', decode(values)['candidate_blocking_issues'])
        values = fixture(); lane = values[0]['threads'][0]; entry = lane['entries'][0]
        entry['r3'] ^= 1 << 40; entry['slot28_pre']['fields'][1] = entry['r3']
        entry['target_entry']['r3'] = entry['r3']; entry['target_entry']['vector_pre']['fields'][2] = entry['r3']
        for event in lane['events']:
            if event['entry_index'] == 0 and event['event'] == 7: event['fields'][1] = entry['r3']
        self.assertIn('STORE_TARGET_IDENTITY_OR_SAVED_OPERAND_MISMATCH', decode(values)['candidate_blocking_issues'])

    def test_orphan_duplicate_and_duplicate_own_identity(self):
        store, target = analyses(fixture()); extra = copy.deepcopy(store['completed_invocations'][0])
        extra['entry']['invocation']['serial'] = 99; store['completed_invocations'].append(extra); issues = set()
        self.assertEqual(decoder._evaluate(store, target, issues, set())[0], [])
        self.assertIn('DUPLICATE_STORE_FOR_TARGET_SLOT28_PRE', issues)
        extra['entry']['invocation']['serial'] = 1
        with self.assertRaises(ValueError): decoder._evaluate(store, target, set(), set())
        store, target = analyses(fixture()); target['candidate_records'] = []
        store['completed_invocations'][0]['entry']['target_entry']['invocation']['serial'] = 999
        issues = set(); result = decoder._evaluate(store, target, issues, set())
        self.assertEqual(result[0], []); self.assertEqual(len(result[2]), 1)
        self.assertIn('ORPHAN_STORE_WITHOUT_EXACT_APPLICABLE_SLOT28_PRE', issues)

    def test_exact_site_not_only_same_requested_address(self):
        store, target = analyses(fixture())
        parent = target['vector_target_analysis']; lane = parent['vector_target_inventory']['threads'][0]
        row = parent['completed_invocations'][0]
        alternate = next(i for i, e in enumerate(lane['events']) if e['event'] == 2 and e['site'] == 1)
        lane['events'][alternate]['fields'][2] = decoder.store_decoder.TARGET
        next(e for e in row['events'] if e['event'] == 2 and e['site'] == 1)['fields'][2] = decoder.store_decoder.TARGET
        store['completed_invocations'][0]['entry']['parent_event_index'] = alternate
        issues = set(); decoder._evaluate(store, target, issues, set())
        self.assertIn('ORPHAN_STORE_WITHOUT_EXACT_APPLICABLE_SLOT28_PRE', issues)

    def test_events_inside_specific_pair_and_native_thread(self):
        for delta in (-1, 1):
            values = fixture()
            for event in values[0]['threads'][0]['events']: event['meta']['qpc'] += delta
            self.assertIn('STORE_EVENTS_OUTSIDE_EXACT_SLOT28_PAIR_OR_BOTH_CUTS', decode(values)['candidate_blocking_issues'])
        store, target = analyses(fixture()); store['completed_invocations'][0]['events'][1]['meta']['native_thread_id'] += 1
        issues = set(); decoder._evaluate(store, target, issues, set())
        self.assertIn('STORE_EVENTS_OUTSIDE_EXACT_SLOT28_PAIR_OR_BOTH_CUTS', issues)

    def test_no_later_or_equal_parent_completion_backfill(self):
        for delta in (0, 1):
            store, target = analyses(fixture()); parent = target['vector_target_analysis']
            tick = parent['completed_invocations'][0]['events'][-1]['meta']['qpc']
            store['vector_store_inventory']['freeze_qpc'] = tick
            if delta:
                store['vector_store_inventory']['freeze_qpc'] = tick+1
                parent['completed_invocations'][0]['events'][-1]['meta']['qpc'] = tick+2
                last = next(e for e in parent['vector_target_inventory']['threads'][0]['events'] if e['entry_index'] == 0 and e['event'] == 9)
                last['meta']['qpc'] = tick+2
            accepted, excluded, _, _ = decoder._evaluate(store, target, set(), set())
            self.assertEqual(accepted, []); self.assertEqual(len(excluded), 91)
            self.assertEqual(excluded[0]['reason'], 'PARENT_TARGET_COMPLETION_AT_OR_AFTER_STORE_CUT')

    def test_strict_original_source_frontier(self):
        for delta in (0, -1):
            store, target = analyses(fixture()); tick = store['completed_invocations'][0]['events'][0]['meta']['qpc']
            target['frame_work_analysis']['frame_virtual_analysis']['frame_consumer_analysis']['frame_router_analysis'][
                'timing_frontier_analysis']['closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive'] = tick+delta
            accepted, excluded, _, _ = decoder._evaluate(store, target, set(), set())
            self.assertEqual(accepted, []); self.assertEqual(len(excluded), 91)
            self.assertEqual(excluded[0]['reason'], 'STORE_EVENTS_TOUCH_OR_CROSS_SOURCE_CLOSED_FRONTIER')

    def test_headers_and_lifetime_before_join(self):
        for field in ('token', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc'):
            values = fixture(); values[0][field] = '1'*32 if field == 'token' else values[0][field]+1
            self.reject(values)
        values = fixture(); lane = values[0]['threads'][0]; lane['parent_target_thread_epoch'] = 2
        for entry in lane['entries']: entry['target_entry']['invocation']['thread_epoch'] = 2
        self.reject(values)

    def test_pending_failure_exception_equal_cut_never_healed(self):
        values = fixture(); keep_first_store(values); store_prefix(values[0], 9)
        self.assertEqual(decode(values)['candidate_records'], [])
        for flag in ('observer_failed', 'disarmed_at_cut'):
            values = fixture(); values[0][flag] = True; self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); values[0]['freeze_qpc'] = values[0]['threads'][0]['events'][-1]['meta']['qpc']
        self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); values[0].update(freeze_qpc=0, observer_failed=True, clock_failed=True)
        self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); lane = values[0]['threads'][0]
        lane['events'][8]['event'] = 9; lane['events'][9]['event'] = 11
        values[0]['counts']['original_exceptions'] = 1
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_all_raw_validated_before_relation(self):
        for change in (lambda v: v[0]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v: v[1]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v: v[2]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v: v[10]['records'][-1].update(slot=999)):
            values = fixture(); change(values); self.reject(values)
        values = fixture()
        with self.assertRaises(ValueError):
            decoder.validate_vector_store_join(values[0], *(raw(x) for x in values[1:11]), values[11], TOKEN)


if __name__ == '__main__': unittest.main()
