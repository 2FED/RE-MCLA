"""Synthetic immutable metadata only; no native, private, pixels or file IO."""
import copy
import json
import unittest
from unittest import mock

import mcla_frame_handoff_decode as decoder
from test_mcla_phase_source_join import fixture as joined_fixture
from test_mcla_phase_census_decode import scope, recount, raw
from test_mcla_game_source_scout_decode import TOKEN
from test_mcla_semaphore_prefix_audit import fixture as prefix_fixture, epoch, release, wait


def hand_scope(pid, entry, exit_, tid, host_epoch, serial, kind, *, update=(0, 0), wait_id=0):
    lr = {0: 0x821316A0, 1: 0x821316B8, 3: 0x82305F80}[kind]
    return dict(phase_call_id=pid, entry_qpc=entry, exit_qpc=exit_, caller_lr=lr,
        native_thread_id=tid, thread_epoch=host_epoch, serial=serial,
        update_thread_epoch=update[0], update_serial=update[1], wait_operation=wait_id,
        object_epoch=0 if kind == 0 else 1, kind=kind, tracked=True, completed=True,
        normal_return=True)


def worker_event(qpc, kind, *, helper=0, operation=0, dispatch=0, phase=0, eligible=False):
    return dict(qpc=qpc, thread_epoch=23, worker_serial=1, native_thread_id=28500,
        context=0x1000, base=0x2000, owner=0x82900000, object_epoch=1,
        event=kind, reason=0, helper=helper, operation=operation, dispatch=dispatch,
        phase_call_id=phase, retired_operation=0, retirement_reason=0, eligible=eligible,
        phase_bound=bool(phase and eligible and kind == 6))


def fixture():
    phase, swap, source = joined_fixture()
    begin, end = phase['begin_qpc'], phase['end_qpc_exclusive']
    phase['scopes'] = [scope(2, 'update', begin+10, begin+100, epoch=7, tid=100),
        scope(3, 'coordinator', begin+200, begin+600, epoch=7, tid=100, r3=0x82900000),
        scope(1, 'render', begin+700, end-1, epoch=9, tid=28500, lr=0x82305F80)]
    for row in phase['swap_callbacks']: row['thread_epoch'] = 9
    recount(phase)
    observer_begin, cut = begin-1000, end+100
    r = release(start=begin+300, end=begin+310)
    r['payload']['context']['generation'] = {'thread_epoch': 17, 'serial': 1}
    r['native_thread_id'] = 100
    w = wait(start=begin+320, end=begin+330); w['native_thread_id'] = 28500
    pre = prefix_fixture([r, w], [epoch(created=observer_begin+1)])
    pre.update(token=TOKEN, observer_begin_qpc=observer_begin, freeze_qpc=cut,
               qpc_frequency=phase['qpc_frequency'])
    pre['epochs'][0]['maximum_count'] = 32767
    hand = dict.fromkeys(decoder.FALSE_FLAGS, False)
    hand.update(schema='mcla-frame-handoff-v1', token=TOKEN,
        qpc_frequency=phase['qpc_frequency'], observer_begin_qpc=observer_begin, freeze_qpc=cut,
        selected_cookie=200, selected_epoch=1, selected_owner=0x82900000, selected_handle=12,
        observer_failed=False, coverage_claim_requested=True, lost_scopes=0, lost_events=0,
        scopes=[hand_scope(2, begin+11, begin+99, 100, 17, 1, 0),
                hand_scope(3, begin+201, begin+599, 100, 17, 2, 1, update=(17, 1)),
                hand_scope(1, begin+701, end-2, 28500, 23, 1, 3, wait_id=2)],
        worker_events=[worker_event(begin+250, 1), worker_event(begin+319, 2, helper=1),
            worker_event(begin+331, 3, helper=1, operation=2, eligible=True),
            worker_event(begin+700, 5, helper=1, operation=2, dispatch=1, eligible=True),
            worker_event(begin+701, 6, helper=1, operation=2, dispatch=1, phase=1, eligible=True),
            worker_event(end-1, 7, helper=1, operation=2, dispatch=1)],
        operation_annotations=[dict(operation_id=1, phase_call_id=3, caller_lr=0x82305DDC,
                                    native_thread_id=100, annotated=True),
                               dict(operation_id=2, phase_call_id=0, caller_lr=0x82305EB8,
                                    native_thread_id=28500, annotated=True)],
        collector_counters=dict.fromkeys(decoder.COUNTER_KEYS, 0))
    return hand, pre, phase, swap, source


def decode(values, token=TOKEN):
    hand, pre, phase, swap, source = values
    return decoder.validate_frame_handoff(raw(hand), raw(pre), raw(phase), raw(swap), source, token)


class HandoffTests(unittest.TestCase):
    def reject_before_evaluate(self, values):
        with mock.patch.object(decoder, '_evaluate', side_effect=AssertionError('premature evaluation')) as evaluator:
            with self.assertRaises(ValueError): decode(values)
        evaluator.assert_not_called()

    def test_complete_explicit_chain_detached_false_claims_no_io(self):
        values = fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('IO forbidden')):
            result = decode(values)
        self.assertEqual(values, before)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['diagnostic_issues'], [])
        self.assertEqual(result['candidate_records'][0]['host_update_id'], {'thread_epoch': 17, 'serial': 1})
        self.assertEqual(result['candidate_records'][0]['render_phase_call_id'], 1)
        for key in decoder.FALSE_FLAGS | {'render_proven', 'display_proven', 'coverage_proven', 'causal_join_proven'}:
            self.assertIs(result[key], False)
        self.assertNotIn('PASS', result['status'])
        self.assertTrue(result['physical_acceptance_pending'])
        self.assertTrue(result['unobserved_ancestry_possible'])
        values[0]['scopes'][0]['serial'] = 900
        values[4]['sequence']['begin_qpc'] = 1
        self.assertEqual(result['handoff_inventory']['scopes'][0]['serial'], 1)
        result['candidate_records'][0]['host_update_id']['serial'] = 900
        self.assertEqual(result['candidate_records'][1]['host_update_id']['serial'], 1)
        json.dumps(result, allow_nan=False)

    def test_host_epoch_not_phase_epoch(self):
        result = decode(fixture())
        self.assertEqual(result['source_phase_analysis']['records'][0]['scope']['thread_epoch'], 9)
        self.assertEqual(result['handoff_inventory']['scopes'][-1]['thread_epoch'], 23)
        self.assertEqual(len(result['candidate_records']), 91)

    def test_actual_cut_can_precede_configured_phase_end(self):
        values = fixture(); cut = values[2]['end_qpc_exclusive']-1
        values[0]['freeze_qpc'] = values[1]['freeze_qpc'] = cut
        self.assertEqual(len(decode(values)['candidate_records']), 91)

    def test_control_release_never_becomes_update(self):
        values = fixture(); values[1]['operations'][0]['payload']['context'] = {'kind': 'control', 'generation': None}
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_ambiguous_batch_not_fitted_or_fifo(self):
        values = fixture(); values[1]['operations'][0]['payload']['count'] = 2
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_update_outside_phase_window_no_synthetic_ancestor(self):
        values = fixture(); values[0]['scopes'].pop(0)
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertTrue(all(row['disposition'] == 'INCOMPLETE_EXPLICIT_UPDATE_COORDINATOR_WORKER_CHAIN'
                            for row in result['source_dispositions']))

    def test_unknown_coverage_and_request_is_not_proof(self):
        values = fixture(); values[0]['coverage_claim_requested'] = False
        self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); values[1]['admission_inventory_complete'] = False
        values[1]['epochs'][0]['history_complete'] = False
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertIn('prefix:ADMISSION_INVENTORY_INCOMPLETE', result['diagnostic_issues'])

    def test_pending_scope_retained(self):
        values = fixture(); row = values[0]['scopes'][-1]
        row.update(exit_qpc=0, completed=False, normal_return=False)
        result = decode(values)
        self.assertFalse(result['handoff_inventory']['scopes'][-1]['completed'])
        self.assertIn('PENDING_HANDOFF_SCOPE', result['diagnostic_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_untracked_scope_can_retain_opaque_previous_context(self):
        values = fixture(); values[0]['scopes'][-1]['tracked'] = False
        result = decode(values)
        self.assertIn('UNTRACKED_HANDOFF_SCOPE', result['diagnostic_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_startup_phase_zero_consumption_is_retained_unbound(self):
        values = fixture(); event = values[0]['worker_events'][4]
        event.update(phase_call_id=0, phase_bound=False)
        result = decode(values)
        self.assertEqual(result['handoff_inventory']['worker_events'][4]['phase_call_id'], 0)
        self.assertEqual(result['candidate_records'], [])

    def test_extra_wait_retirement_cannot_donate_old_credit(self):
        values = fixture(); events = values[0]['worker_events']
        extra = worker_event(events[3]['qpc']-1, 2, helper=2)
        extra.update(retired_operation=2, retirement_reason=16)
        events.insert(3, extra)
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_duplicate_render_is_ambiguous_not_latest(self):
        values = fixture(); events = values[0]['worker_events']
        events.insert(5, copy.deepcopy(events[4]))
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_worker_clock_regression_is_retained_inconclusive(self):
        values = fixture(); events = values[0]['worker_events']
        events[3]['qpc'] = events[2]['qpc']-1
        result = decode(values)
        self.assertIn('WORKER_CLOCK_REGRESSION', result['diagnostic_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_dispatch_must_be_exact_wait_stage_not_just_dispatch_id(self):
        for key, value in (('helper', 2), ('operation', 0), ('eligible', False)):
            values = fixture(); values[0]['worker_events'][3][key] = value
            self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); values[0]['worker_events'][2]['qpc'] = values[1]['operations'][1]['exit_qpc']-1
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_missing_wait_entry_or_eligibility_never_filled_by_time(self):
        for index in (1, 2):
            values = fixture(); values[0]['worker_events'].pop(index)
            self.assertEqual(decode(values)['candidate_records'], [])

    def test_release_must_be_inside_narrower_handoff_coordinator(self):
        values = fixture(); coord = values[0]['scopes'][1]
        coord['entry_qpc'] = values[1]['operations'][0]['entry_qpc']+1
        self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); values[0]['scopes'][1]['exit_qpc'] = values[1]['operations'][0]['exit_qpc']-1
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_phase_zero_consumption_blocks_donation_to_later_render(self):
        values = fixture(); events = values[0]['worker_events']
        unbound = copy.deepcopy(events[4]); unbound.update(phase_call_id=0, phase_bound=False)
        events.insert(4, unbound)
        result = decode(values)
        self.assertIn('DUPLICATE_WORKER_WAIT_CONSUMPTION', result['diagnostic_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_distinct_worker_lifetime_cannot_reuse_successful_wait(self):
        values = fixture(); hand, pre, phase, swap, source = values
        begin = phase['begin_qpc']
        second = scope(4, 'render', begin+900, begin+1000, epoch=10, tid=28501, lr=0x82305F80)
        phase['scopes'].append(second); recount(phase)
        hand['scopes'].append(hand_scope(4, begin+901, begin+999, 28501, 24, 1, 3, wait_id=2))
        duplicate = copy.deepcopy(hand['worker_events'][4])
        duplicate.update(qpc=begin+901, phase_call_id=4, thread_epoch=24,
                         worker_serial=2, native_thread_id=28501)
        hand['worker_events'].append(duplicate)
        result = decode(values)
        self.assertIn('DUPLICATE_WORKER_WAIT_CONSUMPTION', result['diagnostic_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_worker_fault_zero_clock_retained_no_candidate(self):
        values = fixture(); values[0]['observer_failed'] = True
        values[0]['worker_events'].append(worker_event(0, 10))
        result = decode(values)
        self.assertIn('INVALID_WORKER_CLOCK', result['diagnostic_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_loss_and_annotation_mismatch_inconclusive(self):
        for name in ('lost_scopes', 'lost_events'):
            values = fixture(); values[0][name] = 1; values[0]['observer_failed'] = True
            self.assertEqual(decode(values)['candidate_records'], [])
        values = fixture(); values[0]['collector_counters']['annotation_mismatches'] = 1
        self.assertEqual(decode(values)['candidate_records'], [])

    def test_unannotated_retained_operation(self):
        values = fixture(); values[0]['operation_annotations'][-1].update(
            annotated=False, phase_call_id=0, caller_lr=0)
        result = decode(values)
        self.assertIn('UNANNOTATED_OPERATION', result['diagnostic_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_last_handoff_record_malformed_before_evaluator(self):
        for field, rowkey in (('scopes', 'serial'), ('worker_events', 'operation'),
                              ('operation_annotations', 'operation_id')):
            values = fixture(); values[0][field][-1][rowkey] = True
            self.reject_before_evaluate(values)

    def test_all_nested_exact_schemas(self):
        for mutate in (lambda h: h.update(extra=0), lambda h: h['scopes'][-1].update(extra=0),
                       lambda h: h['worker_events'][-1].update(extra=0),
                       lambda h: h['operation_annotations'][-1].update(extra=0),
                       lambda h: h['collector_counters'].update(extra=0)):
            values = fixture(); mutate(values[0]); self.reject_before_evaluate(values)

    def test_header_cross_input_binding(self):
        for key in ('token', 'qpc_frequency', 'observer_begin_qpc', 'freeze_qpc', 'selected_epoch',
                    'selected_cookie'):
            values = fixture(); values[0][key] = 'f'*32 if key == 'token' else values[0][key]+1
            self.reject_before_evaluate(values)

    def test_handle_alias_does_not_change_object_epoch_identity(self):
        values = fixture(); values[1]['operations'][1]['guest_handle'] = 99
        self.assertEqual(len(decode(values)['candidate_records']), 91)

    def test_bool_float_negative_overflow(self):
        for value in (True, 1.0, -1, 2**64):
            values = fixture(); values[0]['scopes'][-1]['serial'] = value
            self.reject_before_evaluate(values)

    def test_no_positive_unknown_phase_wait_or_annotation_ids(self):
        for field, key in (('scopes', 'phase_call_id'), ('scopes', 'wait_operation'),
                           ('worker_events', 'phase_call_id'), ('operation_annotations', 'operation_id')):
            values = fixture(); index = 4 if field == 'worker_events' else -1
            values[0][field][index][key] = 999
            self.reject_before_evaluate(values)

    def test_brackets_and_threads_only_reject_explicit_edges(self):
        for mutation in (lambda h: h['scopes'][-1].update(entry_qpc=h['scopes'][-1]['entry_qpc']-2),
                         lambda h: h['scopes'][-1].update(native_thread_id=100),
                         lambda h: h['operation_annotations'][0].update(native_thread_id=28500)):
            values = fixture(); mutation(values[0]); self.reject_before_evaluate(values)

    def test_pending_prefix_tail_preserved_global_inconclusive(self):
        values = fixture(); pre = values[1]
        pending = wait(3, start=pre['freeze_qpc']-1, end=None, outcome=None, completed=False)
        pending['native_thread_id'] = 28500; pre['operations'].append(pending)
        pre['counts'].update(observed_operations=3, stored_operations=3, pending_operations=1)
        values[0]['operation_annotations'].append(dict(operation_id=3, phase_call_id=0,
            caller_lr=0x82305EB8, native_thread_id=28500, annotated=True))
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertEqual(result['prefix_audit']['original_counts']['pending_operations'], 1)

    def test_malformed_prefix_phase_swap_source_tail_before_combined_evaluator(self):
        values = fixture(); values[1]['operations'][-1]['payload']['result'] = 'forged'
        self.reject_before_evaluate(values)
        values = fixture(); values[2]['swap_callbacks'][-1]['callback_id'] = 999
        self.reject_before_evaluate(values)
        values = fixture(); values[3]['records'][-1]['binding']['snapshot']['callback_id'] = 999
        self.reject_before_evaluate(values)
        values = fixture(); values[4]['sequence']['slots'][-1]['capture']['acquired_sequence'] = True
        self.reject_before_evaluate(values)

    def test_handoff_tail_also_precedes_prefix_evaluator(self):
        values = fixture(); values[0]['worker_events'][-1]['event'] = 11
        with mock.patch.object(decoder.prefix, 'audit', side_effect=AssertionError('premature prefix')) as evaluator:
            with self.assertRaises(ValueError): decode(values)
        evaluator.assert_not_called()

    def test_invalid_raw_bounds_and_lexemes(self):
        h, p, phase, swap, source = fixture()
        for bad in (b'', bytearray(raw(h)), b'\xff', b'{} trailing', b'['*2000+b']'*2000,
                    raw(h).replace(b'"lost_events":0', b'"lost_events":NaN'),
                    raw(h).replace(b'"lost_events":0', b'"lost_events":0,"lost_events":0'),
                    b' '*(decoder.MAX_BYTES+1)):
            with self.assertRaises(ValueError):
                decoder.validate_frame_handoff(bad, raw(p), raw(phase), raw(swap), source, TOKEN)

    def test_mutation_during_existing_validator_cannot_rebind_source(self):
        values = fixture(); source = values[-1]
        original = decoder.phase_join.validate_phase_source_join
        def mutate(*args):
            source['sequence']['slots'][-1]['capture']['acquired_sequence'] = True
            return original(*args)
        with mock.patch.object(decoder.phase_join, 'validate_phase_source_join', side_effect=mutate):
            result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)

    def test_claim_flags_cannot_be_true(self):
        for key in decoder.FALSE_FLAGS:
            values = fixture(); values[0][key] = True; self.reject_before_evaluate(values)


if __name__ == '__main__': unittest.main()
