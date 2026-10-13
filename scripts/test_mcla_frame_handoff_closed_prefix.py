"""Synthetic refinement tests only; immutable recorded reader stays unchanged."""
import copy
import hashlib
import unittest
from unittest import mock

import mcla_frame_handoff_closed_prefix as decoder
from test_mcla_frame_handoff_decode import fixture, hand_scope, worker_event
from test_mcla_phase_census_decode import scope, callback, recount, raw
from test_mcla_semaphore_prefix_audit import release, wait
from test_mcla_game_source_scout_decode import TOKEN


def decode(values):
    h, p, phase, swap, source = values
    return decoder.validate_closed_handoffs(raw(h), raw(p), raw(phase), raw(swap), source, TOKEN)


def pending_fixture(*, unscoped=False, different_worker=False):
    values = fixture(); hand, pre, phase, swap, source = values
    end = phase['end_qpc_exclusive']; barrier = end-99
    phase['scopes'][-1]['exit_qpc'] = end-200
    hand['scopes'][-1]['exit_qpc'] = end-201
    hand['worker_events'][-1]['qpc'] = end-199
    tid, host_epoch, worker_serial, phase_epoch = (28501, 24, 1, 10) if different_worker else (28500, 23, 1, 9)
    phase['scopes'].append(scope(4, 'render', barrier-1, end+10,
                                epoch=phase_epoch, tid=tid, lr=0x82305F80))
    pending = hand_scope(4, barrier, 0, tid, host_epoch, 1 if different_worker else 2, 3, wait_id=4)
    pending.update(completed=False, normal_return=False); hand['scopes'].append(pending)
    r = release(3, start=end-180, end=end-170, context='control'); r['native_thread_id'] = 100
    w = wait(4, start=end-160, end=end-150); w['native_thread_id'] = tid
    pre['operations'].extend((r, w))
    pre['counts'].update(observed_operations=4, stored_operations=4, completed_operations=4)
    hand['operation_annotations'].extend((
        dict(operation_id=3, phase_call_id=0, caller_lr=0x823056FC, native_thread_id=100, annotated=True),
        dict(operation_id=4, phase_call_id=0, caller_lr=0x82305EB8, native_thread_id=tid, annotated=True)))
    events = ([worker_event(end-191, 1)] if different_worker else []) + [
        worker_event(end-161, 2, helper=2),
        worker_event(end-149, 3, helper=2, operation=4, eligible=True),
        worker_event(barrier-1, 5, helper=2, operation=4, dispatch=2, eligible=True),
        worker_event(barrier, 6, helper=2, operation=4, dispatch=2, phase=4, eligible=True)]
    for row in events:
        row.update(native_thread_id=tid, thread_epoch=host_epoch, worker_serial=worker_serial)
    hand['worker_events'].extend(events)
    if unscoped:
        row = callback(end-50, epoch=7, tid=100); row['callback_id'] = 92
        phase['swap_callbacks'].append(row)
    recount(phase)
    return values


def narrow_barrier_fixture():
    values = pending_fixture(different_worker=True); hand, pre, phase, swap, source = values
    barrier = source['sequence']['slots'][-1]['due_qpc']+1
    phase['scopes'][2]['exit_qpc'] = barrier-3; hand['scopes'][2]['exit_qpc'] = barrier-4
    hand['worker_events'][5]['qpc'] = barrier-2
    phase['scopes'][-1]['entry_qpc'] = barrier-1; hand['scopes'][-1]['entry_qpc'] = barrier
    pre['operations'][2].update(entry_qpc=barrier-30, exit_qpc=barrier-25)
    pre['operations'][3].update(entry_qpc=barrier-20, exit_qpc=barrier-15)
    for row, qpc in zip(hand['worker_events'][6:], (barrier-50, barrier-21, barrier-14, barrier-1, barrier)):
        row['qpc'] = qpc
    hand['worker_events'].sort(key=lambda row: row['qpc'])
    return values


def set_last_consume(values, consume):
    binding = values[3]['records'][-1]['binding']; binding['consume_qpc'] = consume
    slot = values[-1]['sequence']['slots'][-1]; start = consume+1
    slot['capture'].update(refresh_cpu_begin_qpc=start, refresh_cpu_end_qpc=start+1,
        acquire_before_qpc=start+2, acquire_after_qpc=start+3, copy_submit_before_qpc=start+4,
        copy_completion_observed_qpc=start+5, pixels_ready_qpc=start+6)
    slot.update(call_after_qpc=start+7, observed_qpc=start+7)


class ClosedPrefixTests(unittest.TestCase):
    def assert_no_candidates(self, values, blocker=None):
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertNotIn('PASS', result['status'])
        if blocker: self.assertIn(blocker, result['candidate_blocking_issues'])
        return result

    def test_wholly_healthy91_no_pass_false_proofs_and_no_io(self):
        values = fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('no IO')):
            result = decode(values)
        self.assertEqual(values, before)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertEqual(result['retained_context_issues'], [])
        self.assertEqual(result['excluded_candidate_records'], [])
        self.assertEqual(result['status'], 'FRAME_HANDOFF_CLOSED_PREFIX_CANDIDATES_REVIEW_PENDING')
        for key in decoder.FALSE_FLAGS: self.assertIs(result[key], False)
        self.assertTrue(result['physical_acceptance_pending'])
        self.assertTrue(result['unobserved_ancestry_possible'])
        self.assertEqual(result['raw_receipt_sha256']['handoff'], hashlib.sha256(raw(values[0])).hexdigest())

    def test_valid_pending_worker_is_context_not_erased_or_completed(self):
        values = pending_fixture(); result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertIn(decoder.PENDING, result['retained_context_issues'])
        self.assertIn(decoder.DERIVED, result['retained_context_issues'])
        self.assertEqual(result['status'], 'FRAME_HANDOFF_CLOSED_PREFIX_PARTIAL_INCONCLUSIVE')
        original = result['original_analysis']
        self.assertEqual(original['candidate_records'], [])
        self.assertIn(decoder.PENDING, original['diagnostic_issues'])
        self.assertFalse(original['handoff_inventory']['scopes'][-1]['completed'])
        self.assertEqual(original['handoff_inventory']['scopes'][-1]['exit_qpc'], 0)
        self.assertEqual(result['pending_handoff_scopes'], [values[0]['scopes'][-1]])
        self.assertTrue(all(c['closed_chain_witness']['chain_last_observed_qpc'] <
                            result['closed_prefix_barrier_qpc_exclusive'] for c in result['candidate_records']))

    def test_only_uncaptured_nonsuppressed_unscoped_callback_relaxed(self):
        values = pending_fixture(unscoped=True); result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertIn(decoder.UNSCOPED, result['retained_context_issues'])
        self.assertEqual(result['uncaptured_unscoped_callback_ids'], [92])
        self.assertFalse(result['phase_inventory']['swap_callbacks'][-1]['has_scope'])
        self.assertEqual(len(result['phase_inventory']['swap_callbacks']), 92)
        self.assertEqual(result['original_analysis']['source_phase_analysis']['unmatched_callback_ids'], [92])

    def test_multiple_pending_workers_use_minimum_entry_not_array_order(self):
        values = pending_fixture(); hand, pre, phase, swap, source = values
        end = phase['end_qpc_exclusive']; entry = end-50
        phase['scopes'].append(scope(5, 'render', entry-1, hand['freeze_qpc']+20,
                                    epoch=11, tid=28502, lr=0x82305F80)); recount(phase)
        h = hand_scope(5, entry, 0, 28502, 25, 1, 3, wait_id=6)
        h.update(completed=False, normal_return=False); hand['scopes'].insert(3, h)
        r = release(5, start=entry-40, end=entry-30, context='control'); r['native_thread_id'] = 100
        w = wait(6, start=entry-20, end=entry-10); w['native_thread_id'] = 28502
        pre['operations'].extend((r, w)); pre['counts'].update(
            observed_operations=6, stored_operations=6, completed_operations=6)
        hand['operation_annotations'].extend((
            dict(operation_id=5, phase_call_id=0, caller_lr=0x823056FC, native_thread_id=100, annotated=True),
            dict(operation_id=6, phase_call_id=0, caller_lr=0x82305EB8, native_thread_id=28502, annotated=True)))
        events = [worker_event(entry-50, 1), worker_event(entry-21, 2, helper=1),
                  worker_event(entry-9, 3, helper=1, operation=6, eligible=True),
                  worker_event(entry-1, 5, helper=1, operation=6, dispatch=1, eligible=True),
                  worker_event(entry, 6, helper=1, operation=6, dispatch=1, phase=5, eligible=True)]
        for row in events: row.update(thread_epoch=25, worker_serial=1, native_thread_id=28502)
        hand['worker_events'].extend(events); hand['worker_events'].sort(key=lambda row: row['qpc'])
        result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(len(result['pending_handoff_scopes']), 2)
        self.assertEqual(result['closed_prefix_barrier_qpc_exclusive'], hand['scopes'][-1]['entry_qpc'])

    def test_unscoped_context_without_pending_scope_is_still_partial(self):
        values = fixture(); phase = values[2]
        row = callback(phase['end_qpc_exclusive']-50, epoch=7, tid=100); row['callback_id'] = 92
        phase['swap_callbacks'].append(row); recount(phase)
        result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['retained_context_issues'], [decoder.DERIVED, decoder.UNSCOPED])
        self.assertEqual(result['status'], 'FRAME_HANDOFF_CLOSED_PREFIX_PARTIAL_INCONCLUSIVE')

    def test_any_captured_unscoped_callback_globally_blocks_whole_issue(self):
        values = pending_fixture(unscoped=True)
        row = values[2]['swap_callbacks'][90]
        row.update(active_scope_call_id=0, has_scope=False, context_same=False, base_same=False)
        result = self.assert_no_candidates(values, decoder.UNSCOPED)
        self.assertIn('phase_source:unscoped', result['candidate_blocking_issues'])
        self.assertNotIn(decoder.UNSCOPED, result['retained_context_issues'])

    def test_suppressed_uncaptured_callback_is_not_safe_context(self):
        values = pending_fixture(unscoped=True); phase = values[2]
        phase['swap_callbacks'][-1]['suppressed_scope'] = True
        phase['counts']['dropped_scopes'] = 1; phase['observer_failed'] = True
        result = self.assert_no_candidates(values, decoder.UNSCOPED)
        self.assertIn('phase_source:phase:suppressed_callbacks', result['candidate_blocking_issues'])

    def test_pending_update_or_coordinator_never_relaxed(self):
        for index in (0, 1):
            values = fixture(); values[0]['scopes'][index].update(
                completed=False, normal_return=False, exit_qpc=0)
            result = self.assert_no_candidates(values, decoder.PENDING)
            self.assertNotIn(decoder.PENDING, result['retained_context_issues'])

    def test_untracked_pending_render_never_relaxed(self):
        values = pending_fixture(); values[0]['scopes'][-1]['tracked'] = False
        result = self.assert_no_candidates(values, decoder.PENDING)
        self.assertIn('UNTRACKED_HANDOFF_SCOPE', result['candidate_blocking_issues'])

    def test_invalid_pending_clock_never_relaxed(self):
        values = pending_fixture(); values[0]['scopes'][-1]['entry_qpc'] = 0
        result = self.assert_no_candidates(values, decoder.PENDING)
        self.assertIn('INVALID_HANDOFF_SCOPE_CLOCK', result['candidate_blocking_issues'])

    def test_strict_barrier_rejects_touch_and_cross_even_other_worker(self):
        for offset in (0, 1):
            values = pending_fixture(different_worker=True)
            barrier = values[0]['scopes'][-1]['entry_qpc']
            values[2]['scopes'][2]['exit_qpc'] = barrier+offset
            values[0]['scopes'][2]['exit_qpc'] = barrier-1
            values[0]['worker_events'][5]['qpc'] = barrier+offset+1
            values[0]['worker_events'].sort(key=lambda row: row['qpc'])
            result = self.assert_no_candidates(values)
            self.assertEqual(result['candidate_blocking_issues'], [])
            self.assertTrue(all(row['reason'] == 'CHAIN_TOUCHES_OR_CROSSES_STRICT_BARRIER'
                                for row in result['excluded_candidate_records']))

    def test_pending_wait_id_not_reused_by_an_earlier_complete_scope(self):
        values = pending_fixture(); values[0]['scopes'][-1]['wait_operation'] = 2
        self.assert_no_candidates(values, decoder.PENDING)

    def test_pending_renderer_requires_exact_positive_wait_edge(self):
        for mutate in (lambda h: h['worker_events'].pop(),
                       lambda h: h['worker_events'][-1].update(operation=2),
                       lambda h: h['worker_events'][-1].update(phase_call_id=0, phase_bound=False)):
            values = pending_fixture(); mutate(values[0])
            self.assert_no_candidates(values, decoder.PENDING)

    def test_later_phase_completion_cannot_backfill_captured_pending_render(self):
        values = pending_fixture(); hand, pre, phase, swap, source = values
        barrier = phase['end_qpc_exclusive']-1010
        phase['scopes'][2]['exit_qpc'] = barrier-40
        hand['scopes'][2]['exit_qpc'] = barrier-41
        hand['worker_events'][5]['qpc'] = barrier-39
        phase['scopes'][-1].update(entry_qpc=barrier-1, exit_qpc=hand['freeze_qpc']+10)
        hand['scopes'][-1]['entry_qpc'] = barrier
        pre['operations'][2].update(entry_qpc=barrier-30, exit_qpc=barrier-25)
        pre['operations'][3].update(entry_qpc=barrier-20, exit_qpc=barrier-15)
        for row, qpc in zip(hand['worker_events'][6:], (barrier-21, barrier-14, barrier-1, barrier)):
            row['qpc'] = qpc
        phase['swap_callbacks'][90]['active_scope_call_id'] = 4
        result = decode(values)
        self.assertEqual([row['slot'] for row in result['candidate_records']], list(range(90)))
        self.assertTrue(result['phase_inventory']['scopes'][-1]['completed'])
        self.assertGreater(result['phase_inventory']['scopes'][-1]['exit_qpc'], hand['freeze_qpc'])
        self.assertFalse(result['original_analysis']['handoff_inventory']['scopes'][-1]['completed'])
        self.assertNotIn(4, {row['render_phase_call_id'] for row in result['candidate_records']})

    def test_pending_semaphore_suffix_is_still_global_blocker(self):
        values = pending_fixture(); hand, pre, phase, swap, source = values
        pending = wait(5, start=pre['freeze_qpc']-1, end=None, outcome=None, completed=False)
        pending['native_thread_id'] = 28500; pre['operations'].append(pending)
        pre['counts'].update(observed_operations=5, stored_operations=5, pending_operations=1)
        hand['operation_annotations'].append(dict(operation_id=5, phase_call_id=0,
            caller_lr=0x82305EB8, native_thread_id=28500, annotated=True))
        self.assert_no_candidates(values, 'PENDING_SEMAPHORE_OPERATIONS')

    def test_pending_phase_scope_is_not_completed_from_handoff(self):
        values = pending_fixture(); phase = values[2]
        phase['scopes'][0].update(completed=False, exit_qpc=0)
        phase['drain_incomplete'] = True; recount(phase)
        self.assert_no_candidates(values, 'phase_source:phase:incomplete_scopes')

    def test_other_global_faults_combined_with_both_allowed_contexts(self):
        for mutate, blocker in (
            (lambda h, p: h.update(observer_failed=True), 'HANDOFF_OBSERVER_FAILED'),
            (lambda h, p: h['collector_counters'].update(failures=1), 'COLLECTOR_FAILURES'),
            (lambda h, p: h.update(lost_events=1), 'HANDOFF_INVENTORY_LOSS'),
            (lambda h, p: h.update(coverage_claim_requested=False), 'COVERAGE_NOT_REQUESTED_OR_ESTABLISHED'),
            (lambda h, p: p.update(unknown_coverage_gap=True), 'prefix:UNBOUNDED_COVERAGE_GAP')):
            values = pending_fixture(unscoped=True); mutate(values[0], values[1])
            self.assert_no_candidates(values, blocker)

    def test_invalid_worker_clock_duplicate_consumption_are_global(self):
        values = pending_fixture(); values[0]['worker_events'][-1]['qpc'] = 0
        self.assert_no_candidates(values, 'INVALID_WORKER_CLOCK')
        values = pending_fixture(); event = copy.deepcopy(values[0]['worker_events'][4])
        values[0]['worker_events'].insert(5, event)
        self.assert_no_candidates(values, 'DUPLICATE_WORKER_WAIT_CONSUMPTION')

    def test_exact_dispatch_completion_required_not_nearest_or_missing(self):
        values = pending_fixture(); values[0]['worker_events'].pop(5)
        result = self.assert_no_candidates(values)
        self.assertTrue(all(row['reason'] == 'EXACT_HELPER_OR_DISPATCH_COMPLETION_NOT_UNIQUE'
                            for row in result['excluded_candidate_records']))
        values = pending_fixture(); values[0]['worker_events'][5]['dispatch'] = 99
        self.assert_no_candidates(values)

    def test_duplicate_or_mismatched_dispatch_completion_rejected(self):
        values = pending_fixture(); values[0]['worker_events'].insert(6, copy.deepcopy(values[0]['worker_events'][5]))
        result = self.assert_no_candidates(values)
        self.assertTrue(all(row['reason'] == 'EXACT_HELPER_OR_DISPATCH_COMPLETION_NOT_UNIQUE'
                            for row in result['excluded_candidate_records']))
        for key, value in (('helper', 99), ('operation', 4), ('eligible', True), ('reason', 14)):
            values = pending_fixture(); values[0]['worker_events'][5][key] = value
            result = self.assert_no_candidates(values)
            self.assertTrue(all(row['reason'] == 'DISPATCH_COMPLETION_NOT_CLOSED_NORMAL_MATCHING_CHAIN'
                                for row in result['excluded_candidate_records']))

    def test_exact_packet_consume_touch_or_cross_barrier_excludes_only_that_source(self):
        for offset in (0, 1):
            values = narrow_barrier_fixture(); barrier = values[0]['scopes'][-1]['entry_qpc']
            set_last_consume(values, barrier+offset)
            result = decode(values)
            self.assertEqual([row['slot'] for row in result['candidate_records']], list(range(90)))
            self.assertEqual(result['excluded_candidate_records'][0]['candidate']['slot'], 90)
            self.assertEqual(result['excluded_candidate_records'][0]['reason'], 'CHAIN_TOUCHES_OR_CROSSES_STRICT_BARRIER')

    def test_emission_outer_bracket_touch_or_cross_barrier_never_clamped(self):
        for offset in (0, 1):
            values = narrow_barrier_fixture(); barrier = values[0]['scopes'][-1]['entry_qpc']
            values[3]['records'][-1]['binding']['snapshot']['qpc_after'] = barrier+offset
            set_last_consume(values, barrier+offset+1)
            result = decode(values)
            self.assertEqual([row['slot'] for row in result['candidate_records']], list(range(90)))
            self.assertEqual(result['excluded_candidate_records'][0]['reason'], 'CHAIN_TOUCHES_OR_CROSSES_STRICT_BARRIER')

    def test_dispatch_completion_before_render_return_cannot_widen(self):
        values = pending_fixture(); values[0]['worker_events'][5]['qpc'] = values[0]['scopes'][2]['exit_qpc']-1
        result = self.assert_no_candidates(values)
        self.assertTrue(all(row['reason'] == 'DISPATCH_COMPLETION_NOT_CLOSED_NORMAL_MATCHING_CHAIN'
                            for row in result['excluded_candidate_records']))

    def test_later_phase_completion_for_old_render_rejects_not_backfill(self):
        values = pending_fixture(different_worker=True)
        values[2]['scopes'][2]['exit_qpc'] = values[0]['freeze_qpc']+1
        values[0]['worker_events'][5]['qpc'] = values[0]['freeze_qpc']
        result = self.assert_no_candidates(values)
        self.assertTrue(all(row['reason'] == 'DISPATCH_COMPLETION_NOT_CLOSED_NORMAL_MATCHING_CHAIN'
                            for row in result['excluded_candidate_records']))

    def test_original_metadata_before_refinement_malformed_tails(self):
        for mutate in (
            lambda v: v[0]['worker_events'][-1].update(operation=True),
            lambda v: v[1]['operations'][-1]['payload'].update(result='forged'),
            lambda v: v[2]['swap_callbacks'][-1].update(callback_id=999),
            lambda v: v[3]['records'][-1]['binding']['snapshot'].update(callback_id=999),
            lambda v: v[4]['sequence']['slots'][-1]['capture'].update(acquired_sequence=True)):
            values = pending_fixture(unscoped=True); mutate(values)
            with mock.patch.object(decoder, '_refine', side_effect=AssertionError('early refinement')) as refine:
                with self.assertRaises(ValueError): decode(values)
            refine.assert_not_called()

    def test_original_first_and_all_containers_detached(self):
        values = pending_fixture(unscoped=True); before = copy.deepcopy(values)
        original = decoder.original.validate_frame_handoff
        calls = []
        def observe(*args):
            calls.append('original'); result = original(*args)
            values[-1]['sequence']['begin_qpc'] = 1
            args[-2]['sequence']['begin_qpc'] = 2
            return result
        with mock.patch.object(decoder.original, 'validate_frame_handoff', side_effect=observe):
            result = decode(values)
        self.assertEqual(calls, ['original'])
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['source_inventory'], before[-1])
        self.assertEqual(result['original_analysis']['handoff_inventory'], before[0])
        self.assertEqual(result['original_analysis']['prefix_inventory'], before[1])
        result['candidate_records'][0]['host_update_id']['serial'] = 99
        self.assertEqual(result['candidate_records'][1]['host_update_id']['serial'], 1)


if __name__ == '__main__': unittest.main()
