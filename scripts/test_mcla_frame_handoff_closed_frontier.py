"""Offline synthetic v2 frontier contract. No private data, files or native work."""
import copy
import unittest
from unittest import mock

import mcla_frame_handoff_closed_frontier as decoder
from test_mcla_frame_handoff_closed_prefix import pending_fixture, set_last_consume
from test_mcla_frame_handoff_decode import fixture, hand_scope
from test_mcla_phase_census_decode import scope, recount, raw
from test_mcla_semaphore_prefix_audit import release
from test_mcla_game_source_scout_decode import TOKEN


def pending_coordinator_fixture():
    """Mixed pending worker/coordinator suffix, all earlier 91 chains closed."""
    values = pending_fixture()
    h, pre, phase, swap, source = values
    end = phase['end_qpc_exclusive']
    phase['scopes'].extend((
        scope(5, 'update', end-90, end-75, epoch=7, tid=100, lr=0x821316A0),
        scope(6, 'coordinator', end-60, end+30, epoch=7, tid=100,
              lr=0x821316B8, r3=h['selected_owner'])))
    h['scopes'].extend((hand_scope(5, end-89, end-76, 100, 17, 3, 0),
                        hand_scope(6, end-59, 0, 100, 17, 4, 1, update=(17, 3))))
    h['scopes'][-1].update(completed=False, normal_return=False)
    recount(phase)
    return values


def with_owned_operation():
    values = pending_coordinator_fixture(); h, pre, phase, _, _ = values
    end = phase['end_qpc_exclusive']
    op = release(5, start=end-50, end=end-40, success=False, previous=None)
    op['native_thread_id'] = 100
    op['payload']['context']['generation'] = {'thread_epoch': 17, 'serial': 3}
    pre['operations'].append(op)
    pre['counts'].update(observed_operations=5, stored_operations=5, completed_operations=5)
    h['operation_annotations'].append(dict(operation_id=5, phase_call_id=6,
        caller_lr=0x82305DDC, native_thread_id=100, annotated=True))
    return values


def decode(values):
    return decoder.validate_closed_frontier(*(raw(v) for v in values[:4]), values[4], TOKEN)


class FrontierTests(unittest.TestCase):
    def blocked(self, values):
        try:
            result = decode(values)
        except ValueError:
            return None
        self.assertEqual(result['candidate_records'], [])
        self.assertNotIn('PASS', result['status'])
        return result

    def test_valid_mixed_suffix_preserves_old_zero_and_full_input(self):
        values = pending_coordinator_fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')):
            result = decode(values)
        self.assertEqual(values, before)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertEqual(result['v1_baseline_analysis']['candidate_records'], [])
        self.assertIn(decoder.baseline.PENDING, result['v1_baseline_analysis']['candidate_blocking_issues'])
        self.assertEqual(result['original_analysis']['candidate_records'], [])
        self.assertEqual(result['original_analysis']['handoff_inventory'], values[0])
        self.assertEqual(result['pending_handoff_scopes'], [values[0]['scopes'][3], values[0]['scopes'][-1]])
        self.assertEqual([r['route'] for r in result['pending_scope_classifications']],
                         ['KNOWN_WORKER_RENDER', 'KNOWN_MAIN_COORDINATOR'])
        self.assertEqual(result['closed_prefix_barrier_qpc_exclusive'], values[2]['scopes'][3]['entry_qpc'])
        for key in decoder.FALSE_FLAGS: self.assertIs(result[key], False)
        self.assertTrue(result['physical_acceptance_pending'])
        result['candidate_records'][0]['host_update_id']['serial'] = 77
        self.assertEqual(result['candidate_records'][1]['host_update_id']['serial'], 1)
        self.assertEqual(values, before)

    def test_no_pending_preserves_healthy_contract(self):
        result = decode(fixture())
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['pending_scope_classifications'], [])

    def test_render_only_preserves_v1_route(self):
        result = decode(pending_fixture())
        self.assertEqual([c['slot'] for c in result['candidate_records']], list(range(91)))
        self.assertEqual(result['closed_prefix_barrier_qpc_exclusive'],
                         result['v1_baseline_analysis']['closed_prefix_barrier_qpc_exclusive']-1)

    def test_coordinator_only_suffix(self):
        values = pending_coordinator_fixture()
        values[0]['scopes'][3].update(completed=True, normal_return=True,
                                     exit_qpc=values[2]['end_qpc_exclusive']+1)
        result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['closed_prefix_barrier_qpc_exclusive'], values[2]['scopes'][-1]['entry_qpc'])

    def test_owner_comparison_uses_native_u32_preserving_raw_bits(self):
        values = pending_coordinator_fixture()
        values[2]['scopes'][-1]['incoming_r3'] |= 0x1234567800000000
        result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['phase_inventory']['scopes'][-1]['incoming_r3'],
                         values[2]['scopes'][-1]['incoming_r3'])

    def test_coordinator_must_have_exact_next_tracker_serial(self):
        values = pending_coordinator_fixture(); values[0]['scopes'][-1]['serial'] += 1
        self.blocked(values)

    def test_update_cannot_have_another_coordinator_owner(self):
        values = pending_coordinator_fixture()
        h, _, phase, _, _ = values; end = phase['end_qpc_exclusive']
        phase['scopes'].append(scope(7, 'coordinator', end-70, end-65,
            epoch=7, tid=100, lr=0x821316B8, r3=h['selected_owner']))
        h['scopes'].append(hand_scope(7, end-69, end-66, 100, 17, 5, 1, update=(17, 3)))
        recount(phase)
        result = self.blocked(values)
        self.assertIsNotNone(result)  # Not merely rejected by an older validator.
        self.assertEqual(result['pending_scope_classifications'][-1]['route'], 'UNCLASSIFIED')

    def test_render_only_cannot_inherit_weaker_v1_pending_classification(self):
        values = pending_fixture()
        values[2]['scopes'][-1]['exit_qpc'] = values[0]['scopes'][-1]['entry_qpc']-1
        old = decoder.baseline.validate_closed_handoffs(*(raw(v) for v in values[:4]), values[4], TOKEN)
        self.assertEqual(len(old['candidate_records']), 91)
        result = self.blocked(values)
        self.assertIn(decoder.baseline.PENDING, result['candidate_blocking_issues'])

    def test_candidate_between_phase_and_handoff_entry_is_excluded(self):
        values = pending_coordinator_fixture()
        outer = values[2]['scopes'][3]['entry_qpc']
        values[0]['scopes'][3]['entry_qpc'] = outer+10
        values[0]['worker_events'][-1]['qpc'] = outer+10
        set_last_consume(values, outer+5)
        result = decode(values)
        self.assertEqual(len(result['v1_baseline_analysis']['candidate_records']), 0)
        self.assertEqual([c['slot'] for c in result['candidate_records']], list(range(90)))
        self.assertEqual(result['closed_prefix_barrier_qpc_exclusive'], outer)

    def test_candidate_phase_closure_between_outer_and_hand_entry_excluded(self):
        values = pending_fixture(different_worker=True)
        hand, pre, phase, _, _ = values; end = phase['end_qpc_exclusive']
        phase['scopes'][-1]['entry_qpc'] = end-205
        hand['scopes'][-1]['entry_qpc'] = end-190
        pre['operations'][2].update(entry_qpc=end-250, exit_qpc=end-240)
        pre['operations'][3].update(entry_qpc=end-230, exit_qpc=end-220)
        for row, qpc in zip(hand['worker_events'][6:],
                            (end-260, end-231, end-219, end-206, end-190)):
            row['qpc'] = qpc
        result = decode(values)
        self.assertEqual(len(result['v1_baseline_analysis']['candidate_records']), 91)
        self.assertEqual(result['candidate_records'], [])
        self.assertEqual(len(result['excluded_candidate_records']), 91)
        self.assertEqual(result['closed_prefix_barrier_qpc_exclusive'], end-205)
        self.assertTrue(all(c['reason'] == 'CHAIN_TOUCHES_OR_CROSSES_STRICT_BARRIER'
                            for c in result['excluded_candidate_records']))

    def test_pending_coordinator_owned_failed_release_retained(self):
        values = with_owned_operation(); result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['pending_coordinator_operation_ids'], [5])
        self.assertFalse(result['original_analysis']['handoff_inventory']['scopes'][-1]['completed'])

    def test_wrong_coordinator_route_or_owner_blocks(self):
        for key, bad in (('caller_lr', 0x821316BC), ('incoming_r3', 123),
                         ('context_nonnull', False), ('base_nonnull', False)):
            values = pending_coordinator_fixture(); values[2]['scopes'][-1][key] = bad
            if key == 'caller_lr': values[0]['scopes'][-1][key] = bad
            with self.subTest(key=key): self.blocked(values)

    def test_pending_entry_cannot_exceed_completed_phase_exit(self):
        for index in (3, 5):
            values = pending_coordinator_fixture()
            values[2]['scopes'][index]['exit_qpc'] = values[0]['scopes'][index]['entry_qpc']-1
            with self.subTest(index=index): self.blocked(values)

    def test_pending_phase_fault_flags_never_classified(self):
        for index in (3, 5):
            for key in ('invalid_bracket', 'original_exception'):
                values = pending_coordinator_fixture(); values[2]['scopes'][index][key] = True
                with self.subTest(index=index, key=key): self.blocked(values)

    def test_host_identity_missing_or_wrong_not_latest(self):
        for key, bad in (('update_serial', 99), ('update_thread_epoch', 99),
                         ('thread_epoch', 99), ('object_epoch', 99), ('tracked', False)):
            values = pending_coordinator_fixture(); values[0]['scopes'][-1][key] = bad
            with self.subTest(key=key): self.blocked(values)

    def test_prior_update_must_close_normally_in_both_journals(self):
        for target, key, bad in ((0, 'completed', False), (0, 'normal_return', False),
                                 (2, 'completed', False), (2, 'original_exception', True),
                                 (2, 'invalid_bracket', True), (2, 'context_nonnull', False),
                                 (2, 'base_nonnull', False)):
            values = pending_coordinator_fixture(); row = values[target]['scopes'][-2]
            row[key] = bad
            if key == 'completed':
                row['exit_qpc'] = 0
                if target == 0: row['normal_return'] = False
            recount(values[2])
            with self.subTest(target=target, key=key): self.blocked(values)

    def test_prior_update_exact_route_required(self):
        values = pending_coordinator_fixture()
        values[0]['scopes'][-2]['caller_lr'] = values[2]['scopes'][-2]['caller_lr'] = 0x821316A4
        self.blocked(values)

    def test_same_native_thread_and_phase_epoch_required(self):
        for key in ('native_thread_id', 'thread_epoch'):
            values = pending_coordinator_fixture(); values[2]['scopes'][-1][key] += 1
            if key == 'native_thread_id': values[0]['scopes'][-1][key] += 1
            with self.subTest(key=key): self.blocked(values)

    def test_update_touch_or_cross_coordinator_entry_rejected(self):
        for offset in (0, 1):
            values = pending_coordinator_fixture()
            values[2]['scopes'][-2]['exit_qpc'] = values[2]['scopes'][-1]['entry_qpc'] + offset
            with self.subTest(offset=offset): self.blocked(values)

    def test_unknown_pending_kind_and_pending_update_block(self):
        values = pending_coordinator_fixture()
        values[0]['scopes'][-2].update(completed=False, normal_return=False, exit_qpc=0)
        self.blocked(values)
        values = pending_coordinator_fixture(); values[0]['scopes'][-1]['kind'] = 2
        self.blocked(values)

    def test_loss_fault_and_coverage_never_downgraded(self):
        for key, value in (('observer_failed', True), ('lost_scopes', 1), ('lost_events', 1),
                           ('coverage_claim_requested', False)):
            values = pending_coordinator_fixture(); values[0][key] = value
            with self.subTest(key=key): self.blocked(values)

    def test_owned_operation_clock_before_handoff_entry_rejected(self):
        values = with_owned_operation(); op = values[1]['operations'][-1]
        op['entry_qpc'] = values[2]['scopes'][-1]['entry_qpc']
        self.blocked(values)

    def test_owned_operation_phase_exit_and_freeze_bounds(self):
        for end in ('phase', 'freeze'):
            values = with_owned_operation()
            values[1]['operations'][-1]['exit_qpc'] = (values[2]['scopes'][-1]['exit_qpc']
                                                      if end == 'phase' else values[0]['freeze_qpc']) + 1
            with self.subTest(end=end): self.blocked(values)

    def test_owned_annotation_route_and_generation_required(self):
        values = with_owned_operation(); values[0]['operation_annotations'][-1]['caller_lr'] += 4
        self.blocked(values)
        values = with_owned_operation()
        values[1]['operations'][-1]['payload']['context']['generation']['serial'] = 1
        self.blocked(values)

    def test_candidate_consumption_touch_or_cross_barrier_excluded(self):
        for offset in (0, 1):
            values = pending_coordinator_fixture()
            set_last_consume(values, values[0]['scopes'][3]['entry_qpc']+offset)
            result = decode(values)
            self.assertEqual([x['slot'] for x in result['candidate_records']], list(range(90)))
            self.assertEqual(result['excluded_candidate_records'][0]['reason'], 'CHAIN_TOUCHES_OR_CROSSES_STRICT_BARRIER')

    def test_later_phase_completion_never_backfills_pending(self):
        values = pending_coordinator_fixture()
        values[2]['scopes'][-1]['exit_qpc'] += 999
        result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['closed_prefix_barrier_qpc_exclusive'], values[2]['scopes'][3]['entry_qpc'])
        self.assertFalse(result['original_analysis']['handoff_inventory']['scopes'][-1]['completed'])
        self.assertEqual(result['v1_baseline_analysis']['candidate_records'], [])

    def test_captured_pending_render_never_donates_completed_phase(self):
        values = pending_coordinator_fixture(); hand, pre, phase, _, _ = values
        barrier = phase['end_qpc_exclusive']-1010
        phase['scopes'][2]['exit_qpc'] = barrier-40
        hand['scopes'][2]['exit_qpc'] = barrier-41
        hand['worker_events'][5]['qpc'] = barrier-39
        phase['scopes'][3].update(entry_qpc=barrier-1, exit_qpc=hand['freeze_qpc']+10)
        hand['scopes'][3]['entry_qpc'] = barrier
        pre['operations'][2].update(entry_qpc=barrier-30, exit_qpc=barrier-25)
        pre['operations'][3].update(entry_qpc=barrier-20, exit_qpc=barrier-15)
        for row, qpc in zip(hand['worker_events'][6:], (barrier-21, barrier-14, barrier-1, barrier)):
            row['qpc'] = qpc
        phase['swap_callbacks'][90]['active_scope_call_id'] = 4
        result = decode(values)
        self.assertEqual([c['slot'] for c in result['candidate_records']], list(range(90)))
        self.assertTrue(result['phase_inventory']['scopes'][3]['completed'])
        self.assertFalse(result['pending_handoff_scopes'][0]['completed'])
        self.assertEqual(result['v1_baseline_analysis']['candidate_records'], [])

    def test_owned_operation_cannot_be_donated_even_if_evaluator_proposes_it(self):
        values = with_owned_operation()
        # Defense in depth: full real baseline validation first. Only replace
        # the new evaluator call with a deliberately impossible proposed edge.
        old = decoder.baseline.validate_closed_handoffs(*(raw(v) for v in values[:4]), values[4], TOKEN)
        healthy = decode(pending_coordinator_fixture())['candidate_records'][0]
        healthy = copy.deepcopy(healthy); healthy['release_operation_id'] = 5
        with mock.patch.object(decoder.baseline, 'validate_closed_handoffs', return_value=old), \
             mock.patch.object(decoder.baseline.original, '_evaluate', return_value=([healthy], [])):
            result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertEqual(result['excluded_candidate_records'][0]['reason'],
                         'PENDING_COORDINATOR_OPERATION_CANNOT_BE_REUSED')

    def test_malformed_last_operation_count_precedes_classifier(self):
        values = with_owned_operation(); values[1]['operations'][-1]['payload']['count'] = True
        with mock.patch.object(decoder, '_pending_coordinator', side_effect=AssertionError('premature')) as classify:
            with self.assertRaises(ValueError): decode(values)
        classify.assert_not_called()

    def test_malformed_tail_validated_before_v2_classification(self):
        values = pending_coordinator_fixture(); values[0]['scopes'][-1]['serial'] = True
        with mock.patch.object(decoder, '_pending_coordinator', side_effect=AssertionError('premature')) as classify:
            with self.assertRaises(ValueError): decode(values)
        classify.assert_not_called()

    def test_pending_row_order_does_not_select_later_barrier(self):
        values = pending_coordinator_fixture(); values[0]['scopes'].reverse()
        result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['closed_prefix_barrier_qpc_exclusive'], values[2]['scopes'][3]['entry_qpc'])


if __name__ == '__main__':
    unittest.main()
