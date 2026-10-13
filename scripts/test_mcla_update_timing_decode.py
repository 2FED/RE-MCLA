"""Pure synthetic exact-phase timing tests, no pixels, files or native launch."""
import copy
import json
import unittest
from unittest import mock

import mcla_update_timing_decode as decoder
from test_mcla_frame_handoff_decode import fixture as handoff_fixture
from test_mcla_phase_census_decode import scope, recount, raw
from test_mcla_game_source_scout_decode import TOKEN


def state():
    return {'words': [0x7F800001]*18, 'bytes': [0, 255, 23], 'qword64': decoder.U64,
            'reciprocal_bits': 0xFFFFFFFF, 'count': decoder.U32,
            'saved_unsplit_bits': 0x80000000, 'split_state': 255}


def row(oid, kind, lr, entry, *, selector=0):
    return {'observation_id': oid, 'kind': kind, 'caller_lr': lr, 'native_thread_id': 100,
        'entry_qpc': entry, 'exit_qpc': entry+1,
        'binding': {'phase_call_id': 2, 'thread_epoch': 7, 'native_thread_id': 100},
        'expected_route': True,
        'incoming': {'r3': 0x82900000 if kind == 'router' else decoder.TIMER_ADDRESS,
            'r4': selector, 'r5': decoder.U64, 'f1_bits': 0x7FF8000000000001,
            'f28_bits': decoder.U64, 'f30_bits': 0x8000000000000000, 'f31_bits': 0,
            'loop_r23': 2 if kind == 'subdivision' and lr == 0x822C246C else 0,
            'loop_r24': 7 if kind == 'subdivision' and lr == 0x822C246C else 0},
        'before': state(), 'after': state(), 'completed': True, 'normal_return': True,
        'exit_binding_matches': True}


def recount_timing(timing):
    rows = timing['records']; counts = timing['counts']
    counts.update(observed_entries=len(rows), admitted_entries=len(rows), stored_entries=len(rows),
        completed_entries=sum(r['completed'] for r in rows), pending_entries=sum(not r['completed'] for r in rows),
        unbound_entries=sum(r['binding'] is None or not r['expected_route'] for r in rows),
        binding_mismatches=sum(r['completed'] and r['binding'] is not None and not r['exit_binding_matches'] for r in rows),
        original_exceptions=sum(r['completed'] and not r['normal_return'] for r in rows),
        read_failures=sum(r['completed'] and r['binding'] is not None and r['expected_route'] and r['before'] is None for r in rows) +
            sum(r['completed'] and r['binding'] is not None and r['expected_route'] and r['exit_binding_matches'] and r['after'] is None for r in rows))


def fixture():
    base = handoff_fixture(); hand, pre, phase, swap, source = base
    begin = phase['begin_qpc']; routes = [('timer', lr, 0) for lr in sorted(decoder.TIMER_LRS)]
    routes += [('subdivision', lr, 0) for lr in sorted(decoder.SUBDIVISION_LRS)]
    routes += [('router', lr, selector) for lr, selector in decoder.ROUTER_SELECTORS.items()]
    timing = dict.fromkeys(decoder.FALSE_FLAGS, False)
    timing.update(schema='mcla-update-timing-v1', token=TOKEN, clock='win32-query-performance-counter',
        capacity=4096, begin_qpc=begin, end_qpc_exclusive=phase['end_qpc_exclusive'],
        qpc_frequency=phase['qpc_frequency'], freeze_qpc=hand['freeze_qpc']+10,
        observer_failed=False, clock_failed=False,
        counts=dict.fromkeys(decoder.COUNT_KEYS-{'counter_saturated'}, 0),
        records=[row(i+1, kind, lr, begin+20+i*3, selector=selector)
                 for i, (kind, lr, selector) in enumerate(routes)])
    timing['counts']['counter_saturated'] = False; recount_timing(timing)
    return (timing,)+base


def decode(values):
    timing, h, p, phase, swap, source = values
    return decoder.validate_update_timing(raw(timing), raw(h), raw(p), raw(phase), raw(swap), source, TOKEN)


def make_unknown(timing, index=0):
    r = timing['records'][index]
    r.update(expected_route=False, before=None, after=None)
    recount_timing(timing)


class TimingTests(unittest.TestCase):
    def reject_before_join(self, values):
        with mock.patch.object(decoder, '_timing_join', side_effect=AssertionError('premature timing join')) as join:
            with self.assertRaises(ValueError): decode(values)
        join.assert_not_called()

    def assert_blocked(self, values, issue):
        result = decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertEqual(result['grouped_update_records'], [])
        self.assertIn(issue, result['candidate_blocking_issues'])
        self.assertEqual(result['status'], 'UPDATE_TIMING_DIAGNOSTIC_INCONCLUSIVE')
        return result

    def test_complete_all_routes_exact_phase_only_detached_no_io_no_pass(self):
        values = fixture(); before = copy.deepcopy(values)
        with mock.patch('builtins.open', side_effect=AssertionError('IO forbidden')):
            result = decode(values)
        self.assertEqual(values, before)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertEqual(result['retained_context_issues'], [])
        self.assertEqual(result['status'], 'UPDATE_TIMING_DIAGNOSTIC_REVIEW_PENDING')
        self.assertEqual(result['grouped_update_records'][0]['observation_ids'], list(range(1, 11)))
        self.assertEqual(result['candidate_records'][0]['timing_phase_thread_epoch'], 7)
        self.assertEqual(result['candidate_records'][0]['host_update_id']['thread_epoch'], 17)
        self.assertEqual(result['timing_inventory'], before[0])
        self.assertEqual(result['closed_handoff_analysis']['original_analysis']['handoff_inventory'], before[1])
        for key in decoder.FALSE_FLAGS | {'update_causality_proven', 'causal_join_proven', 'source_timing_join_proven'}:
            self.assertIs(result[key], False)
        self.assertNotIn('PASS', result['status'])
        for key in ('fps', 'step_count', 'delta_sum', 'game_units', 'period'): self.assertNotIn(key, result)
        json.dumps(result, allow_nan=False)
        values[0]['records'][0]['incoming']['f1_bits'] = 0
        result['candidate_records'][0]['timing_observation_ids'].clear()
        self.assertEqual(result['candidate_records'][1]['timing_observation_ids'], list(range(1, 11)))
        self.assertEqual(result['timing_inventory']['records'][0]['incoming']['f1_bits'], 0x7FF8000000000001)

    def test_full_register_bits_sign_extended_guest_pointer_and_selector(self):
        values = fixture()
        for r in values[0]['records']:
            r['incoming']['r3'] |= 0xFFFFFFFF00000000
            r['incoming']['r4'] |= 0xFFFFFFFF00000000
        result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertTrue(all(r['incoming']['r3'] > decoder.U32 for r in result['timing_inventory']['records']))

    def test_row_inventory_order_is_not_replaced_by_clock_sorting(self):
        values = fixture(); values[0]['records'].reverse()
        result = decode(values)
        self.assertEqual(result['grouped_update_records'][0]['observation_ids'], list(range(10, 0, -1)))
        self.assertEqual([r['observation_id'] for r in result['timing_inventory']['records']], list(range(10, 0, -1)))

    def test_outside_window_counters_preserved_without_inventing_rows(self):
        values = fixture(); values[0]['counts'].update(outside_before_entries=3,
            outside_after_entries=5, observed_entries=18)
        result = decode(values)
        self.assertEqual(len(result['timing_inventory']['records']), 10)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['timing_inventory']['counts']['outside_after_entries'], 5)

    def test_unknown_caller_selector_pointer_retained_not_filtered_from_bound_update(self):
        for mutate, index in ((lambda r: r.update(caller_lr=0x1234), 0),
                              (lambda r: r['incoming'].update(r3=0), 0),
                              (lambda r: r['incoming'].update(r4=99), 5),
                              (lambda r: r['incoming'].update(r3=0xFFFFFFFF00000000), 5)):
            values = fixture(); mutate(values[0]['records'][index]); make_unknown(values[0], index)
            result = decode(values)
            self.assertEqual(result['candidate_blocking_issues'], [])
            self.assertEqual(result['candidate_records'], [])
            self.assertEqual(result['unknown_route_observation_ids'], [index+1])
            self.assertIn('BOUND_UNKNOWN_ROUTE_ROWS', result['update_inventory'][0]['diagnostic_issues'])

    def test_unbound_rows_are_context_never_assigned_by_timestamp(self):
        values = fixture(); r = values[0]['records'][0]
        r.update(binding=None, before=None, after=None, exit_binding_matches=False); recount_timing(values[0])
        result = decode(values)
        self.assertEqual(result['unbound_observation_ids'], [1])
        self.assertEqual(result['grouped_update_records'][0]['observation_ids'], list(range(2, 11)))
        self.assertEqual(result['status'], 'UPDATE_TIMING_DIAGNOSTIC_INCONCLUSIVE')

    def test_unrelated_exact_update_id_not_transplanted_by_same_thread_or_time(self):
        values = fixture(); phase = values[3]
        phase['scopes'].append(scope(5, 'update', phase['begin_qpc']+15, phase['begin_qpc']+80, epoch=7, tid=100))
        recount(phase); values[0]['records'][0]['binding']['phase_call_id'] = 5
        result = decode(values)
        self.assertEqual(result['grouped_update_records'][0]['phase_call_id'], 2)
        self.assertEqual(result['grouped_update_records'][0]['observation_ids'], list(range(2, 11)))
        self.assertEqual(result['update_inventory'][1]['phase_call_id'], 5)
        self.assertEqual(result['update_inventory'][1]['observation_ids'], [1])

    def test_unknown_update_caller_remains_inventory_not_group_claim(self):
        values = fixture(); phase = values[3]
        phase['scopes'].append(scope(5, 'update', phase['begin_qpc']+15, phase['begin_qpc']+80,
                                    epoch=7, tid=100, lr=0x1234)); recount(phase)
        values[0]['records'][0]['binding']['phase_call_id'] = 5
        result = decode(values)
        self.assertIn('UNKNOWN_UPDATE_CALLER_ROUTE', result['update_inventory'][1]['diagnostic_issues'])
        self.assertEqual(result['grouped_update_records'][0]['observation_ids'], list(range(2, 11)))

    def test_wrong_phase_kind_epoch_thread_and_unknown_positive_id_reject(self):
        for key, value in (('phase_call_id', 3), ('phase_call_id', 999), ('thread_epoch', 17), ('native_thread_id', 28500)):
            values = fixture(); values[0]['records'][-1]['binding'][key] = value
            self.reject_before_join(values)

    def test_valid_clocks_only_reject_exact_phase_containment(self):
        values = fixture(); r = values[0]['records'][0]
        r['entry_qpc'] = values[3]['begin_qpc']+9; r['exit_qpc'] = r['entry_qpc']+1
        self.reject_before_join(values)
        values = fixture(); values[0]['records'][-1]['exit_qpc'] = values[3]['begin_qpc']+101
        self.reject_before_join(values)

    def test_last_pending_bound_entry_after_completed_phase_exit_rejects_before_join(self):
        values = fixture(); phase = values[3]; begin = phase['begin_qpc']
        phase['scopes'].append(scope(5, 'update', begin+15, begin+80, epoch=7, tid=100))
        recount(phase)
        # Exact ID/epoch/TID and honest pending counters cannot make an entry
        # AFTER this completed Update's exit belong to that invocation. Keeping
        # it on an unselected update must not let the other update group pass.
        last = values[0]['records'][-1]
        last['binding']['phase_call_id'] = 5
        last.update(entry_qpc=begin+90, exit_qpc=0, completed=False,
                    normal_return=False, exit_binding_matches=False, after=None)
        recount_timing(values[0])
        self.reject_before_join(values)

    def test_flagged_pending_entry_after_phase_keeps_diagnostic_without_groups(self):
        values = fixture(); phase = values[3]
        phase['scopes'].append(scope(5, 'update', phase['begin_qpc']+15,
                                    phase['begin_qpc']+80, epoch=7, tid=100))
        recount(phase)
        r = values[0]['records'][-1]
        r['binding']['phase_call_id'] = 5
        r.update(entry_qpc=phase['begin_qpc']+90, completed=False,
                 normal_return=False, exit_binding_matches=False, exit_qpc=0, after=None)
        recount_timing(values[0])
        values[0].update(observer_failed=True, clock_failed=True)
        result = self.assert_blocked(values, 'CLOCK_FAILED')
        self.assertEqual(result['pending_observation_ids'], [10])
        self.assertEqual(result['timing_inventory']['records'][-1]['entry_qpc'], r['entry_qpc'])

    def test_window_frequency_and_separate_cut_binding(self):
        for key in ('begin_qpc', 'end_qpc_exclusive', 'qpc_frequency'):
            values = fixture(); values[0][key] += 1; self.reject_before_join(values)
        values = fixture(); values[0]['freeze_qpc'] = values[1]['freeze_qpc']-1; self.reject_before_join(values)
        values = fixture(); values[0]['freeze_qpc'] = values[1]['freeze_qpc']+1000
        self.assertEqual(len(decode(values)['candidate_records']), 91)

    def test_pending_row_not_completed_from_later_phase_state(self):
        values = fixture(); r = values[0]['records'][-1]
        r.update(completed=False, normal_return=False, exit_binding_matches=False, exit_qpc=0, after=None)
        recount_timing(values[0]); result = decode(values)
        self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertEqual(result['pending_observation_ids'], [10])
        self.assertEqual(result['candidate_records'], [])
        self.assertTrue(result['closed_handoff_analysis']['phase_inventory']['scopes'][0]['completed'])
        self.assertFalse(result['timing_inventory']['records'][-1]['completed'])

    def test_pending_before_null_may_be_unfinished_read_not_failure(self):
        values = fixture(); r = values[0]['records'][-1]
        r.update(completed=False, normal_return=False, exit_binding_matches=False, exit_qpc=0,
                 before=None, after=None); recount_timing(values[0])
        result = decode(values)
        self.assertEqual(result['timing_inventory']['counts']['read_failures'], 0)
        self.assertEqual(result['candidate_blocking_issues'], [])
        values[0]['counts']['read_failures'] = 1
        self.assert_blocked(values, 'READ_FAILURES')

    def test_read_binding_exception_and_observation_faults_are_global(self):
        values = fixture(); values[0]['records'][0]['before'] = None; recount_timing(values[0])
        self.assert_blocked(values, 'READ_FAILURES')
        values = fixture(); values[0]['records'][0].update(exit_binding_matches=False, after=None)
        values[0]['observer_failed'] = True; recount_timing(values[0]); self.assert_blocked(values, 'BINDING_MISMATCHES')
        values = fixture(); values[0]['records'][0]['normal_return'] = False
        recount_timing(values[0]); self.assert_blocked(values, 'ORIGINAL_EXCEPTIONS')
        values = fixture(); values[0]['counts']['observation_errors'] = 1
        values[0]['observer_failed'] = True; self.assert_blocked(values, 'OBSERVATION_ERRORS')

    def test_rejected_entry_clock_counter_and_counter_saturation_global(self):
        values = fixture(); values[0]['counts']['invalid_clock_entries'] = 1
        values[0]['counts']['observed_entries'] += 1
        values[0].update(observer_failed=True, clock_failed=True)
        self.assert_blocked(values, 'INVALID_CLOCK_ENTRIES')
        values = fixture(); values[0]['counts'].update(counter_saturated=True,
            observed_entries=decoder.U64, admitted_entries=decoder.U64)
        values[0]['observer_failed'] = True; self.assert_blocked(values, 'COUNTER_SATURATED')

    def test_honest_capacity_overflow_inventory_preserved_no_groups(self):
        values = fixture(); timing = values[0]; template = timing['records'][0]
        timing['records'] = [copy.deepcopy(template) for _ in range(decoder.CAPACITY)]
        for i, r in enumerate(timing['records']): r['observation_id'] = i+1
        recount_timing(timing); timing['counts'].update(overflow_entries=1, admitted_entries=4097, observed_entries=4097)
        timing['observer_failed'] = True
        result = self.assert_blocked(values, 'OVERFLOW_ENTRIES')
        self.assertEqual(len(result['timing_inventory']['records']), 4096)

    def test_flagged_failed_clocks_cuts_raw_u64_preserved(self):
        for exit_tick in (0, 1, decoder.U64):
            values = fixture(); values[0]['records'][0]['exit_qpc'] = exit_tick
            values[0].update(observer_failed=True, clock_failed=True)
            result = self.assert_blocked(values, 'CLOCK_FAILED')
            self.assertEqual(result['timing_inventory']['records'][0]['exit_qpc'], exit_tick)
            self.assertTrue(result['timing_inventory']['records'][0]['completed'])
        for cut in (0, decoder.U64, 1):
            values = fixture(); values[0]['freeze_qpc'] = cut
            values[0].update(observer_failed=True, clock_failed=True)
            result = self.assert_blocked(values, 'CLOCK_FAILED')
            self.assertEqual(result['timing_inventory']['freeze_qpc'], cut)

    def test_unflagged_invalid_clock_cut_or_entry_regression_rejects(self):
        values = fixture(); values[0]['records'][0]['exit_qpc'] = 0; self.reject_before_join(values)
        values = fixture(); values[0]['freeze_qpc'] = 0; self.reject_before_join(values)
        values = fixture(); values[0]['records'][1]['entry_qpc'] -= 4; self.reject_before_join(values)
        values = fixture(); values[0]['clock_failed'] = True; self.reject_before_join(values)

    def test_fault_flags_never_excuse_false_phase_selection(self):
        values = fixture(); values[0].update(observer_failed=True, clock_failed=True)
        values[0]['records'][0]['binding']['phase_call_id'] = 3; self.reject_before_join(values)

    def test_raw_nan_fpr_and_state_bits_allowed_but_json_nan_rejected(self):
        values = fixture(); result = decode(values)
        self.assertEqual(result['timing_inventory']['records'][0]['before']['words'][0], 0x7F800001)
        data = raw(values[0]).replace(b'"f1_bits":9221120237041090561', b'"f1_bits":NaN')
        with mock.patch.object(decoder, '_timing_join') as join:
            with self.assertRaises(ValueError):
                decoder.validate_update_timing(data, *[raw(v) for v in values[1:5]], values[-1], TOKEN)
        join.assert_not_called()

    def test_all_exact_schema_and_bool_as_int_ranges(self):
        for mutate in (
            lambda t: t.update(extra=0), lambda t: t['counts'].update(extra=0),
            lambda t: t['records'][-1].update(extra=0),
            lambda t: t['records'][-1]['incoming'].update(extra=0),
            lambda t: t['records'][-1]['binding'].update(extra=0),
            lambda t: t['records'][-1]['after'].update(extra=0),
            lambda t: t['records'][-1]['after']['words'].__setitem__(0, True),
            lambda t: t['records'][-1]['incoming'].update(loop_r23=True),
            lambda t: t['records'][-1].update(observation_id=True),
            lambda t: t['records'][-1]['incoming'].update(f31_bits=2**64),
            lambda t: t['records'][-1]['after'].update(split_state=256)):
            values = fixture(); mutate(values[0]); self.reject_before_join(values)

    def test_state_lengths_route_predicate_and_loop_allowlist(self):
        for mutate in (lambda t: t['records'][-1]['after']['words'].pop(),
                       lambda t: t['records'][-1]['after']['bytes'].append(0),
                       lambda t: t['records'][-1].update(expected_route=False),
                       lambda t: t['records'][0]['incoming'].update(loop_r24=1)):
            values = fixture(); mutate(values[0]); self.reject_before_join(values)

    def test_count_conservation_ordinals_and_forged_outcomes(self):
        for mutate in (
            lambda t: t['counts'].update(stored_entries=9),
            lambda t: t['counts'].update(unbound_entries=1),
            lambda t: t['counts'].update(observed_entries=11),
            lambda t: t['counts'].update(read_failures=1),
            lambda t: t['counts'].update(binding_mismatches=1),
            lambda t: t['records'][-1].update(observation_id=1),
            lambda t: t['records'][-1].update(completed=False),
            lambda t: t['records'][-1].update(binding=None)):
            values = fixture(); mutate(values[0]); self.reject_before_join(values)

    def test_malformed_last_timing_and_other_metadata_before_extra_evaluator(self):
        for mutate in (lambda v: v[0]['records'][-1]['after'].update(count=True),
                       lambda v: v[1]['worker_events'][-1].update(event=99),
                       lambda v: v[2]['operations'][-1]['payload'].update(result='forged'),
                       lambda v: v[3]['swap_callbacks'][-1].update(callback_id=999),
                       lambda v: v[4]['records'][-1]['binding']['snapshot'].update(callback_id=999),
                       lambda v: v[5]['sequence']['slots'][-1]['capture'].update(acquired_sequence=True)):
            values = fixture(); mutate(values); self.reject_before_join(values)

    def test_timing_tail_validates_before_even_closed_evaluator(self):
        values = fixture(); values[0]['records'][-1]['after']['words'][-1] = True
        with mock.patch.object(decoder.closed_prefix, 'validate_closed_handoffs', side_effect=AssertionError('premature closed')) as closed:
            with self.assertRaises(ValueError): decode(values)
        closed.assert_not_called()

    def test_duplicate_keys_bad_bytes_sizes_negative_float_and_flags(self):
        values = fixture(); encoded = raw(values[0])
        for bad in (b'', bytearray(encoded), b'\xff', b' '*(decoder.MAX_BYTES+1),
                    encoded.replace(b'"capacity":4096', b'"capacity":4096,"capacity":4096'),
                    encoded.replace(b'"capacity":4096', b'"capacity":-1'),
                    encoded.replace(b'"capacity":4096', b'"capacity":4096.0')):
            with self.assertRaises(ValueError):
                decoder.validate_update_timing(bad, *[raw(v) for v in values[1:5]], values[-1], TOKEN)
        for flag in decoder.FALSE_FLAGS:
            values = fixture(); values[0][flag] = True; self.reject_before_join(values)

    def test_source_mutation_during_existing_validator_does_not_rebind(self):
        values = fixture(); source = values[-1]; original = decoder.closed_prefix.validate_closed_handoffs
        def mutate(*args):
            source['sequence']['begin_qpc'] = 1
            return original(*args)
        with mock.patch.object(decoder.closed_prefix, 'validate_closed_handoffs', side_effect=mutate):
            result = decode(values)
        self.assertEqual(len(result['candidate_records']), 91)
        self.assertEqual(result['closed_handoff_analysis']['source_inventory']['sequence']['begin_qpc'], 500000)


if __name__ == '__main__': unittest.main()
