"""Offline integration: the new frontier cannot heal timing or lineage faults."""
import copy
import unittest
from unittest import mock

import mcla_update_timing_frontier as decoder
from test_mcla_update_timing_decode import fixture, recount_timing, make_unknown
from test_mcla_phase_census_decode import raw
from test_mcla_game_source_scout_decode import TOKEN
from test_mcla_frame_handoff_closed_frontier import pending_coordinator_fixture


def decode(values):
    t, h, pre, phase, swap, source = values
    return decoder.validate_update_timing_frontier(raw(t), raw(h), raw(pre),
                                                  raw(phase), raw(swap), source, TOKEN)


class TimingFrontierTests(unittest.TestCase):
    def test_mixed_suffix_allows_only_completed_old_groups_preserves_old_blocker(self):
        values = (fixture()[0],) + pending_coordinator_fixture()
        before = copy.deepcopy(values); result = decode(values)
        self.assertEqual(values, before)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertEqual(result['baseline_analysis']['candidate_records'], [])
        self.assertIn('closed:PENDING_HANDOFF_SCOPE', result['baseline_analysis']['candidate_blocking_issues'])
        self.assertEqual(result['grouped_update_records'][0]['phase_call_id'], 2)
        self.assertEqual(result['grouped_update_records'][0]['observation_ids'], list(range(1, 11)))
        self.assertNotIn('PASS', result['status'])

    def test_mixed_suffix_cannot_hide_timing_read_failure(self):
        values = (fixture()[0],) + pending_coordinator_fixture()
        values[0]['records'][0]['before'] = None; recount_timing(values[0])
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertIn('READ_FAILURES', result['candidate_blocking_issues'])

    def test_unknown_pending_coordinator_is_still_global(self):
        values = (fixture()[0],) + pending_coordinator_fixture()
        values[1]['scopes'][-1]['caller_lr'] = 123
        values[3]['scopes'][-1]['caller_lr'] = 123
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertIn('closed:PENDING_HANDOFF_SCOPE', result['candidate_blocking_issues'])

    def test_scope_after_closed_frontier_cannot_donate_timing_rows(self):
        values = (fixture()[0],) + pending_coordinator_fixture()
        row = values[0]['records'][0]
        row['binding']['phase_call_id'] = 5  # A real suffix Update, not source Update2.
        with self.assertRaises(ValueError): decode(values)  # Timestamp does not belong to exact ID.

    def test_exact_healthy_groups_detached_and_no_io_or_proof(self):
        values = fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')):
            result = decode(values)
        self.assertEqual(values, before)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['baseline_analysis'], decoder.timing.validate_update_timing(
            raw(values[0]), raw(values[1]), raw(values[2]), raw(values[3]), raw(values[4]), values[5], TOKEN))
        self.assertEqual(result['grouped_update_records'][0]['observation_ids'], list(range(1, 11)))
        for key in decoder.timing.FALSE_FLAGS | {'closed_frontier_proven', 'causal_join_proven', 'source_timing_join_proven'}:
            self.assertIs(result[key], False)
        result['candidate_records'][0]['timing_observation_ids'].clear()
        self.assertEqual(result['candidate_records'][1]['timing_observation_ids'], list(range(1, 11)))
        self.assertNotIn('PASS', result['status'])

    def test_bad_timing_metadata_fails_before_new_frontier(self):
        values = fixture(); values[0]['counts']['stored_entries'] += 1
        with mock.patch.object(decoder.frontier, 'validate_closed_frontier', side_effect=AssertionError('premature')) as join:
            with self.assertRaises(ValueError): decode(values)
        join.assert_not_called()

    def test_foreign_phase_epoch_thread_or_id_cannot_be_repaired(self):
        for key, value in (('phase_call_id', 3), ('phase_call_id', 999), ('thread_epoch', 17), ('native_thread_id', 28500)):
            values = fixture(); values[0]['records'][0]['binding'][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): decode(values)

    def test_corrupt_suffix_rejected_even_with_complete_timing(self):
        values = fixture(); values[1]['scopes'][-1]['phase_call_id'] = 999
        with self.assertRaises(ValueError): decode(values)

    def test_unknown_timing_route_never_hidden(self):
        values = fixture(); values[0]['records'][0]['caller_lr'] = 123; make_unknown(values[0])
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertEqual(result['unknown_route_observation_ids'], [1])
        self.assertEqual(result['baseline_analysis']['candidate_records'], [])

    def test_pending_timing_not_completed_by_later_phase(self):
        values = fixture(); values[0]['records'][-1].update(completed=False, normal_return=False,
            exit_binding_matches=False, exit_qpc=0, after=None)
        recount_timing(values[0]); result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertEqual(result['pending_observation_ids'], [10])
        self.assertFalse(result['timing_inventory']['records'][-1]['completed'])

    def test_all_observer_failure_classes_still_global(self):
        for name in ('observation_errors', 'invalid_clock_entries'):
            values = fixture(); values[0]['counts'][name] = 1; values[0]['observer_failed'] = True
            if name == 'invalid_clock_entries':
                values[0]['counts']['observed_entries'] += 1; values[0]['clock_failed'] = True
            result = decode(values)
            self.assertEqual(result['candidate_records'], [])
            self.assertIn(name.upper(), result['candidate_blocking_issues'])

    def test_read_failure_stays_global(self):
        values = fixture(); values[0]['records'][0]['before'] = None; recount_timing(values[0])
        result = decode(values)
        self.assertIn('READ_FAILURES', result['candidate_blocking_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_complete_timing_outside_exact_update_rejected(self):
        values = fixture(); values[0]['records'][0]['exit_qpc'] = values[3]['begin_qpc'] + 101
        with self.assertRaises(ValueError): decode(values)

    def test_wrong_token_rejected(self):
        values = fixture(); values[0]['token'] = '1' * 32
        with self.assertRaises(ValueError): decode(values)


if __name__ == '__main__':
    unittest.main()
