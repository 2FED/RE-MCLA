"""Synthetic complete raw-chain target association, without native or file IO."""
import copy
import unittest
from unittest import mock

import mcla_vector_target_join as decoder
from test_mcla_vector_target_decode import fixture as target_fixture, prefix as target_prefix
from test_mcla_work_emission_join import fixture as work_fixture, add_second_virtual, work_body
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN


def fixture(second_work=False, unknown=False):
    values = work_fixture()
    if second_work:
        add_second_virtual(values, True)
        lane = values[0]['threads'][0]
        entry, events = work_body(values[1]['threads'][0]['events'][8:], 2, 1, 'vector')
        lane['entries'][1] = entry
        lane['events'] = [e for e in lane['events'] if e['entry_index'] == 0]+events
        lane['event_count'] = len(lane['events'])
    work = values[0]; target = target_fixture(); lane = target['threads'][0]
    lane.update(entries=[], events=[], parent_work_thread_epoch=1,
                native_thread_id=work['threads'][0]['native_thread_id'])
    for field in ('token', 'clock', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc'):
        target[field] = work[field]
    for index, pre in enumerate(work['threads'][0]['events']):
        if pre['event'] != 19:
            continue
        if unknown and pre['fields'][1] == 0:
            continue
        pre['fields'][4] = 0xaabbccdd00000000 | decoder.target_decoder.TARGET
        part = target_fixture()['threads'][0]
        entry = part['entries'][0]; own_index = len(lane['entries'])
        entry.update(invocation=dict(thread_epoch=1, serial=own_index+1),
            work_entry=copy.deepcopy(work['threads'][0]['entries'][pre['entry_index']]),
            parent_event_index=index, vector_pre=dict(fields=copy.deepcopy(pre['fields']),
            vector=copy.deepcopy(pre['vector'])), r3=pre['fields'][2], v1=copy.deepcopy(pre['vector']))
        for event in part['events']:
            event.update(invocation=copy.deepcopy(entry['invocation']), entry_index=own_index)
            event['meta'].update(qpc=pre['meta']['qpc'], native_thread_id=pre['meta']['native_thread_id'])
            if event['event'] == 1:
                event['fields'][0] = ((entry['r3']+400) & 0xffffffff) & ~15
            if event['event'] == 2:
                event['fields'][1] = entry['r3']; event['vector'] = copy.deepcopy(entry['v1'])
            if event['event'] == 5:
                event['fields'][0] = (entry['r3']+657) & 0xffffffff
            if event['event'] in (7, 8):
                event['fields'][0] = (entry['r3']+664) & 0xffffffff
        lane['entries'].append(entry); lane['events'] += part['events']
    count = len(lane['entries'])
    lane.update(entry_count=count, event_count=len(lane['events']))
    target['counts'].update(observed_entries=count, admitted_entries=count)
    return (target,)+values


def decode(values):
    return decoder.validate_vector_target_join(*(raw(x) for x in values[:10]), values[10], TOKEN)


def analyses(values):
    return (decoder.target_decoder.validate_vector_target(raw(values[0]), TOKEN),
            decoder.work_join.validate_work_emission_join(*(raw(x) for x in values[1:10]), values[10], TOKEN))


def keep_first_target(values):
    lane = values[0]['threads'][0]
    lane['entries'] = lane['entries'][:1]
    lane['events'] = [e for e in lane['events'] if e['entry_index'] == 0]
    lane.update(entry_count=1, event_count=len(lane['events']))
    values[0]['counts'].update(observed_entries=1, admitted_entries=1)


class VectorTargetJoinTests(unittest.TestCase):
    def reject(self, values):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature join')) as evaluate:
            with self.assertRaises(ValueError):
                decode(values)
        evaluate.assert_not_called()

    def test_full_chain_detached_noio_falseproof(self):
        values = fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')):
            result = decode(values)
        self.assertEqual(values, before)
        self.assertEqual(len(result['candidate_records']), 91)
        target, work = analyses(values)
        self.assertEqual(result['vector_target_analysis'], target)
        self.assertEqual(result['frame_work_analysis'], work)
        row = result['candidate_records'][0]
        self.assertEqual(row['vector_target_group_count'], 2)
        self.assertEqual([g['vector_ordinal'] for g in row['compact_target_event_groups']], [1, 0])
        for key in decoder.FALSE_FLAGS:
            self.assertIs(result[key], False)
        self.assertNotIn('PASS', result['status']); self.assertNotIn('f1_sum', result)
        row['compact_target_event_groups'][0]['entry']['r3'] = 99
        self.assertNotEqual(result['candidate_records'][1]['compact_target_event_groups'][0]['entry']['r3'], 99)

    def test_lane_global_index_not_completed_row_local(self):
        values = fixture(second_work=True)
        result = decode(values)
        self.assertEqual(result['candidate_records'][0]['vector_target_group_count'], 4)
        entries = values[0]['threads'][0]['entries']
        self.assertGreater(entries[2]['parent_event_index'], entries[0]['parent_event_index'])
        # A local index is valid in isolation but binds to the wrong work row.
        target, work = analyses(values)
        target['completed_invocations'][2]['entry']['parent_event_index'] = entries[0]['parent_event_index']
        issues = set()
        self.assertEqual(decoder._evaluate(target, work, issues, set())[0], [])
        self.assertIn('ORPHAN_TARGET_WITHOUT_EXACT_APPLICABLE_VECTOR_PRE', issues)

    def test_missing_body_excludes_whole_group(self):
        values = fixture(); keep_first_target(values)
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertEqual(len(result['excluded_candidate_records']), 91)
        self.assertEqual(result['excluded_candidate_records'][0]['expected_target_count'], 2)
        self.assertEqual(result['source_target_inventory'][0]['exclusion_reason'],
                         'MISSING_TARGET_FOR_APPLICABLE_WORK_VECTOR_PRE')

    def test_other_requested_target_retained_unknown(self):
        result = decode(fixture(unknown=True))
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['candidate_records'][0]['vector_target_group_count'], 1)
        self.assertEqual(len(result['candidate_records'][0]['unknown_vector_calls']), 1)
        self.assertIn('OTHER_VECTOR_TARGETS_RETAINED_UNKNOWN', result['retained_context_issues'])

    def test_saved_raw_operands_global_even_unselected(self):
        for change in (lambda e: e['work_entry']['virtual_entry']['parent_entry'].__setitem__('r3', 999),
                       lambda e: e['vector_pre']['fields'].__setitem__(3, 123),
                       lambda e: e['vector_pre']['fields'].__setitem__(4, e['vector_pre']['fields'][4] ^ (1 << 40)),
                       lambda e: e['vector_pre']['fields'].__setitem__(1, 3)):
            target, work = analyses(fixture()); work['candidate_records'] = []
            change(target['completed_invocations'][0]['entry'])
            issues = set()
            self.assertEqual(decoder._evaluate(target, work, issues, set())[0], [])
            self.assertIn('TARGET_WORK_IDENTITY_OR_SAVED_OPERAND_MISMATCH', issues)

    def test_raw_saved_f1_drift_is_global_contradiction(self):
        values = fixture(); values[0]['threads'][0]['entries'][0]['vector_pre']['fields'][3] ^= 1 << 40
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertIn('TARGET_WORK_IDENTITY_OR_SAVED_OPERAND_MISMATCH', result['candidate_blocking_issues'])

    def test_raw_global_index_ordinal_and_upper_r3_contradictions(self):
        values = fixture(); values[0]['threads'][0]['entries'][0]['parent_event_index'] += 1
        self.assertIn('ORPHAN_TARGET_WITHOUT_EXACT_APPLICABLE_VECTOR_PRE',
                      decode(values)['candidate_blocking_issues'])
        values = fixture(); values[0]['threads'][0]['entries'][0]['vector_pre']['fields'][1] = 2
        self.assertIn('TARGET_WORK_IDENTITY_OR_SAVED_OPERAND_MISMATCH',
                      decode(values)['candidate_blocking_issues'])
        values = fixture(); lane = values[0]['threads'][0]; entry = lane['entries'][0]
        entry['r3'] ^= 1 << 40; entry['vector_pre']['fields'][2] = entry['r3']
        for event in lane['events']:
            if event['entry_index'] == 0 and event['event'] == 2:
                event['fields'][1] = entry['r3']
        self.assertIn('TARGET_WORK_IDENTITY_OR_SAVED_OPERAND_MISMATCH',
                      decode(values)['candidate_blocking_issues'])

    def test_distinct_body_timestamps_cannot_bind_adjacent_vector_pair(self):
        target, work = analyses(fixture())
        target['completed_invocations'][0]['entry']['parent_event_index'] += 2
        issues = set(); decoder._evaluate(target, work, issues, set())
        self.assertIn('DUPLICATE_TARGET_FOR_WORK_VECTOR_PRE', issues)
        # Same ordinal in a different work body is a distinct exact identity.
        result = decode(fixture(second_work=True))
        groups = result['candidate_records'][0]['compact_target_event_groups']
        self.assertEqual([g['vector_ordinal'] for g in groups], [1, 0, 1, 0])
        self.assertNotEqual(groups[0]['work_invocation'], groups[2]['work_invocation'])

    def test_orphan_duplicate_and_own_identity_failclosed(self):
        target, work = analyses(fixture())
        extra = copy.deepcopy(target['completed_invocations'][0])
        extra['entry']['invocation']['serial'] = 99
        target['completed_invocations'].append(extra); issues = set()
        self.assertEqual(decoder._evaluate(target, work, issues, set())[0], [])
        self.assertIn('DUPLICATE_TARGET_FOR_WORK_VECTOR_PRE', issues)
        extra['entry']['invocation']['serial'] = 1
        with self.assertRaises(ValueError):
            decoder._evaluate(target, work, set(), set())
        target, work = analyses(fixture()); work['candidate_records'] = []
        target['completed_invocations'][0]['entry']['work_entry']['invocation']['serial'] = 999
        issues = set(); result = decoder._evaluate(target, work, issues, set())
        self.assertEqual(result[0], []); self.assertEqual(len(result[2]), 1)
        self.assertIn('ORPHAN_TARGET_WITHOUT_EXACT_APPLICABLE_VECTOR_PRE', issues)

    def test_events_inside_specific_pair_and_native_thread(self):
        for delta in (-1, 1):
            values = fixture()
            for event in values[0]['threads'][0]['events']:
                event['meta']['qpc'] += delta
            result = decode(values)
            self.assertIn('TARGET_EVENTS_OUTSIDE_EXACT_VECTOR_PAIR_OR_BOTH_CUTS', result['candidate_blocking_issues'])
        target, work = analyses(fixture())
        target['completed_invocations'][0]['events'][1]['meta']['native_thread_id'] += 1
        issues = set(); decoder._evaluate(target, work, issues, set())
        self.assertIn('TARGET_EVENTS_OUTSIDE_EXACT_VECTOR_PAIR_OR_BOTH_CUTS', issues)

    def test_no_later_or_equal_parent_completion_backfill(self):
        for delta in (0, 1):
            target, work = analyses(fixture())
            tick = work['work_analysis']['completed_invocations'][0]['events'][-1]['meta']['qpc']
            target['vector_target_inventory']['freeze_qpc'] = tick-delta
            if delta:
                # Keep target events before its cut; only parent completion is later.
                target['vector_target_inventory']['freeze_qpc'] = tick+1
                parent = work['work_analysis']['completed_invocations'][0]
                parent['events'][-1]['meta']['qpc'] = tick+2
                work['work_analysis']['work_emission_inventory']['threads'][0]['events'][-1]['meta']['qpc'] = tick+2
            accepted, excluded, _, _ = decoder._evaluate(target, work, set(), set())
            self.assertEqual(accepted, []); self.assertEqual(len(excluded), 91)
            self.assertEqual(excluded[0]['reason'], 'PARENT_WORK_COMPLETION_AT_OR_AFTER_TARGET_CUT')

    def test_strict_source_barrier(self):
        for delta in (0, -1):
            target, work = analyses(fixture())
            tick = target['completed_invocations'][0]['events'][0]['meta']['qpc']
            work['frame_virtual_analysis']['frame_consumer_analysis']['frame_router_analysis'][
                'timing_frontier_analysis']['closed_handoff_analysis']['closed_prefix_barrier_qpc_exclusive'] = tick+delta
            accepted, excluded, _, _ = decoder._evaluate(target, work, set(), set())
            self.assertEqual(accepted, []); self.assertEqual(len(excluded), 91)
            self.assertEqual(excluded[0]['reason'], 'TARGET_EVENTS_TOUCH_OR_CROSS_SOURCE_CLOSED_FRONTIER')

    def test_headers_and_parent_lifetime_reject_before_join(self):
        for field in ('token', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'freeze_qpc'):
            values = fixture()
            values[0][field] = '1'*32 if field == 'token' else values[0][field]+1
            self.reject(values)
        values = fixture(); lane = values[0]['threads'][0]
        lane['parent_work_thread_epoch'] = 2
        for entry in lane['entries']:
            entry['work_entry']['invocation']['thread_epoch'] = 2
        self.reject(values)

    def test_pending_failed_and_equal_cut_are_never_healed(self):
        values = fixture(); keep_first_target(values)
        target_prefix(values[0], len(values[0]['threads'][0]['events'])-1)
        self.assertEqual(decode(values)['candidate_records'], [])
        for flag in ('observer_failed', 'disarmed_at_cut'):
            values = fixture(); values[0][flag] = True
            self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); values[0]['freeze_qpc'] = values[0]['threads'][0]['events'][-1]['meta']['qpc']
        self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); values[0].update(freeze_qpc=0, observer_failed=True, clock_failed=True)
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_malformed_final_inputs_all_validated_before_join(self):
        for change in (lambda v: v[0]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v: v[1]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v: v[2]['threads'][0]['events'][-1]['meta'].update(qpc=True),
                       lambda v: v[9]['records'][-1].update(slot=999)):
            values = fixture(); change(values); self.reject(values)
        values = fixture()
        with self.assertRaises(ValueError):
            decoder.validate_vector_target_join(values[0], *(raw(x) for x in values[1:10]), values[10], TOKEN)


if __name__ == '__main__':
    unittest.main()
