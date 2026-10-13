"""Offline synthetic receipts only: no files, native objects or actual clocks."""
import copy
import hashlib
import json
import unittest

import mcla_semaphore_credit_audit as credit

TOKEN = '0123456789abcdef0123456789abcdef'
BEGIN, END, FREQUENCY = 100, 10000, 1000


def epoch(eid=1, cookie=1000, created=101, destroyed=None):
    return {'epoch_id': eid, 'object_cookie': cookie, 'creation_qpc': created,
            'destroy_qpc': destroyed, 'initial_count': 0, 'maximum_count': 8,
            'origin': 'fresh', 'history_complete': True}


def operation(oid, start, end, kind, payload, *, eid=1, handle=100, tid=1):
    return {'op_id': oid, 'epoch_id': eid, 'native_thread_id': tid,
            'guest_handle': handle, 'entry_qpc': start, 'exit_qpc': end,
            'kind': kind, 'payload': payload}


def release(oid=1, start=110, end=120, *, count=1, previous=0, success=True,
            context='update', generation=7, thread_epoch=1, **kwargs):
    return operation(oid, start, end, 'release',
                     {'count': count, 'success': success, 'previous_count': previous,
                      'context': {'kind': context,
                                  'generation': {'thread_epoch': thread_epoch, 'serial': generation}
                                  if context == 'update' else None}},
                     **kwargs)


def wait(oid=2, start=130, end=140, *, result='success', timeout='infinite', **kwargs):
    return operation(oid, start, end, 'wait',
                     {'result': result, 'timeout_kind': timeout}, **kwargs)


def fixture(operations=None, epochs=None):
    operations = [release(), wait()] if operations is None else operations
    value = dict.fromkeys(credit.FALSE_FLAGS, False)
    value.update(schema='mcla-semaphore-credit-journal-v1', token=TOKEN,
                 clock='win32-query-performance-counter', qpc_frequency=FREQUENCY,
                 begin_qpc=BEGIN, end_qpc_exclusive=END, capacity=credit.CAPACITY,
                 history_complete=True, observer_failed=False,
                 counts={'observed_operations': len(operations),
                         'stored_operations': len(operations),
                         'completed_operations': len(operations), 'overflow_operations': 0},
                 epochs=[epoch()] if epochs is None else epochs, operations=operations)
    return value


def encoded(value):
    return json.dumps(value, separators=(',', ':'), allow_nan=False).encode('utf-8')


def audit(value, **kwargs):
    params = dict(token=TOKEN, begin_qpc=BEGIN, end_qpc=END, qpc_frequency=FREQUENCY)
    params.update(kwargs)
    return credit.audit(encoded(value) if type(value) is dict else value, **params)


class CreditAuditTests(unittest.TestCase):
    def assert_inconclusive(self, value, code):
        result = audit(value)
        self.assertEqual(result['status'], 'SEMAPHORE_CREDIT_AUDIT_INCONCLUSIVE')
        self.assertEqual(result['associations'], [])
        self.assertTrue(any(code in issue for issue in result['diagnostic_issues']), result)
        return result

    def test_unique_pair_immutable_bounded_evidence_never_admission(self):
        value = fixture(); original = copy.deepcopy(value); raw = encoded(value)
        result = audit(raw)
        self.assertEqual(value, original)
        self.assertEqual(result['status'], 'SEMAPHORE_CREDIT_CANDIDATES_REVIEW_PENDING')
        self.assertEqual(len(result['associations']), 1)
        self.assertEqual(result['associations'][0]['release_op_id'], 1)
        self.assertEqual(result['associations'][0]['wait_op_id'], 2)
        self.assertEqual(result['associations'][0]['context']['generation'],
                         {'thread_epoch': 1, 'serial': 7})
        self.assertEqual(result['receipt_sha256'], hashlib.sha256(raw).hexdigest().upper())
        self.assertEqual(result['epoch_summaries'][0]['ending_count'], 0)
        for key in credit.FALSE_FLAGS:
            self.assertIs(result[key], False)
        for key in ('fps', 'rate', 'steps', 'simulated_time', 'render_frame_id'):
            self.assertNotIn(key, result)
        json.dumps(result, allow_nan=False)

    def test_update_ids_preserve_same_serial_in_distinct_thread_epochs(self):
        result = audit(fixture([release(thread_epoch=11), wait(),
            release(3, 150, 160, thread_epoch=12), wait(4, 170, 180)]))
        self.assertEqual([r['context']['generation'] for r in result['associations']],
                         [{'thread_epoch': 11, 'serial': 7}, {'thread_epoch': 12, 'serial': 7}])
        self.assertTrue(all(result[key] is False for key in credit.FALSE_FLAGS))

    def test_update_id_exact_shape_and_positive_u64_members(self):
        for generation in (7, None, {}, {'thread_epoch': 1}, {'serial': 7},
                           {'thread_epoch': 1, 'serial': 7, 'extra': 1},
                           {'thread': 1, 'serial': 7},
                           {'thread_epoch': 0, 'serial': 7},
                           {'thread_epoch': True, 'serial': 7},
                           {'thread_epoch': credit.U64 + 1, 'serial': 7},
                           {'thread_epoch': 1, 'serial': 0},
                           {'thread_epoch': 1, 'serial': False},
                           {'thread_epoch': 1, 'serial': credit.U64 + 1}):
            value = fixture()
            value['operations'][0]['payload']['context']['generation'] = generation
            with self.subTest(generation=generation), self.assertRaises(ValueError):
                audit(value)
        result = audit(fixture([release(thread_epoch=credit.U64, generation=credit.U64), wait()]))
        self.assertEqual(result['associations'][0]['context']['generation'],
                         {'thread_epoch': credit.U64, 'serial': credit.U64})

    def test_output_update_id_containers_detached_from_other_outputs_and_input(self):
        value = fixture(); original = copy.deepcopy(value)
        result = audit(value)
        candidate = result['associations'][0]
        consumption = result['unique_credit_consumptions'][0]
        self.assertIsNot(candidate['context'], consumption['context'])
        self.assertIsNot(candidate['context']['generation'], consumption['context']['generation'])
        candidate['context']['generation']['serial'] = 99
        candidate['release_qpc_bracket'][0] = 999
        self.assertEqual(consumption['context']['generation'], {'thread_epoch': 1, 'serial': 7})
        self.assertEqual(consumption['release_qpc_bracket'], [110, 120])
        self.assertEqual(value, original)

    def test_nonupdate_context_requires_null_id_not_pair(self):
        for kind in ('control', 'unknown'):
            value = fixture([release(context=kind), wait()])
            value['operations'][0]['payload']['context']['generation'] = {'thread_epoch': 1, 'serial': 7}
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                audit(value)

    def test_two_competing_releases_do_not_assume_fifo(self):
        self.assert_inconclusive(fixture([
            release(), release(2, 130, 140, previous=1, generation=8),
            wait(3, 150, 160), wait(4, 170, 180)]), 'COMPETING_OR_BATCH')

    def test_batch_release_is_outside_sufficient_rule(self):
        self.assert_inconclusive(fixture([release(count=2), wait()]), 'COMPETING_OR_BATCH')

    def test_initial_credits_cannot_be_attributed(self):
        value = fixture([wait()]); value['epochs'][0]['initial_count'] = 1
        self.assert_inconclusive(value, 'INITIAL_CREDITS')

    def test_timeout_before_release_does_not_consume(self):
        result = audit(fixture([wait(1, 105, 109, result='timeout', timeout='zero'),
                                release(2, 110, 120), wait(3, 130, 140)]))
        self.assertEqual(result['associations'][0]['wait_op_id'], 3)
        self.assertEqual(result['epoch_summaries'][0]['successful_waits'], 1)

    def test_timeout_with_available_credit_is_contradiction(self):
        self.assert_inconclusive(fixture([release(), wait(result='timeout')]),
                                 'TIMEOUT_WITH_AVAILABLE')

    def test_apc_and_alerted_do_not_consume(self):
        for outcome in ('apc', 'alerted'):
            with self.subTest(outcome=outcome):
                result = audit(fixture([release(), wait(result=outcome), wait(3, 150, 160)]))
                self.assertEqual(result['associations'][0]['wait_op_id'], 3)

    def test_aliases_use_epoch_not_handle_or_thread(self):
        result = audit(fixture([release(handle=42, tid=3), wait(handle=900, tid=8)]))
        self.assertEqual(len(result['associations']), 1)
        self.assertFalse(result['object_identity_proven'])

    def test_native_object_access_does_not_require_handle(self):
        result = audit(fixture([release(handle=None), wait(handle=900)]))
        self.assertEqual(len(result['associations']), 1)

    def test_control_credit_consumed_but_not_update(self):
        result = audit(fixture([release(context='control'), wait(),
                                release(3, 150, 160), wait(4, 170, 180)]))
        self.assertEqual([row['release_op_id'] for row in result['associations']], [3])
        self.assertEqual(len(result['unique_credit_consumptions']), 2)

    def test_control_only_is_not_update_candidate(self):
        result = self.assert_inconclusive(fixture([release(context='control'), wait()]),
                                          'NO_UNAMBIGUOUS_UPDATE')
        self.assertEqual(result['unique_credit_consumptions'][0]['context']['kind'], 'control')

    def test_unknown_context_never_becomes_update(self):
        self.assert_inconclusive(fixture([release(context='unknown'), wait()]),
                                 'NO_UNAMBIGUOUS_UPDATE')

    def test_failed_release_does_not_create_credit(self):
        self.assert_inconclusive(fixture([release(success=False, previous=None), wait()]),
                                 'SUCCESSFUL_WAIT_WITHOUT_CREDIT')
        result = audit(fixture([release(success=False, previous=None),
                                release(3, 150, 160), wait(4, 170, 180)]))
        self.assertEqual(result['associations'][0]['release_op_id'], 3)

    def test_failed_release_undefined_outparam_must_be_null(self):
        for previous in (0, 1):
            with self.subTest(previous=previous), self.assertRaises(ValueError):
                audit(fixture([release(success=False, previous=previous)]))

    def test_zero_time_successful_poll_consumes(self):
        result = audit(fixture([release(), wait(timeout='zero')]))
        self.assertEqual(len(result['associations']), 1)
        self.assertEqual(result['epoch_summaries'][0]['ending_count'], 0)

    def test_isolated_overlap_touch_and_nested_unique_credit(self):
        # Includes wait-before-release, wait-return-before-release-poststamp,
        # both directions of touching, and all observations in the same tick.
        for start, end in ((105, 130), (105, 115), (115, 130), (120, 130),
                           (105, 110), (109, 125), (111, 119), (110, 110)):
            with self.subTest(start=start, end=end):
                result = audit(fixture([wait(start=start, end=end, tid=2), release()]))
                self.assertEqual(result['status'], 'SEMAPHORE_CREDIT_CANDIDATES_REVIEW_PENDING')
                match = result['associations'][0]
                self.assertEqual((match['release_op_id'], match['wait_op_id']), (1, 2))
                self.assertEqual(match['association_rule'], 'isolated-overlap-single-credit')
                self.assertEqual(result['epoch_summaries'][0]['ending_count'], 0)
                self.assertTrue(all(result[key] is False for key in credit.FALSE_FLAGS))
        result = audit(fixture([release(start=110, end=110), wait(start=110, end=110)]))
        self.assertEqual(len(result['associations']), 1)

    def test_overlap_competing_release_or_wait_suppresses_clean_prefix(self):
        clean = [release(10, 105, 106), wait(11, 107, 108)]
        for competitor in (release(3, 115, 135, previous=1), wait(3, 115, 135)):
            with self.subTest(kind=competitor['kind']):
                result = self.assert_inconclusive(fixture(clean + [
                    release(), wait(start=109, end=140), competitor]), 'OVERLAP_OR_TOUCH')
                self.assertEqual(result['unique_credit_consumptions'], [])

    def test_overlap_failed_timeout_apc_unknown_and_batch_not_covered(self):
        variants = [
            [release(success=False, previous=None), wait(start=109, end=130)],
            [release(), wait(start=109, end=130, result='timeout')],
            [release(), wait(start=109, end=130, result='apc')],
            [release(), wait(start=109, end=130, result='alerted')],
            [release(), wait(start=109, end=130, result='unknown')],
            [release(), wait(start=109, end=130, result='failed')],
            [release(), wait(start=109, end=130, result='abandoned')],
            [release(count=2), wait(start=109, end=130)],
        ]
        for rows in variants:
            with self.subTest(rows=rows):
                self.assert_inconclusive(fixture(rows), 'OVERLAP_OR_TOUCH')
        self.assert_inconclusive(fixture([release(), wait(start=109, end=130),
            operation(3, 119, 121, 'save', {})]), 'UNSUPPORTED_OPERATION_SAVE')

    def test_overlap_existing_credit_or_impossible_previous_count_rejected(self):
        for previous in (0, 1):
            self.assert_inconclusive(fixture([release(3, 105, 108),
                release(previous=previous), wait(start=109, end=130)]), 'OVERLAP_OR_TOUCH')
        self.assert_inconclusive(fixture([release(previous=1),
            wait(start=109, end=130)]), 'OVERLAP_OR_TOUCH')
        value = fixture([release(count=2), wait(start=109, end=130)])
        value['epochs'][0]['maximum_count'] = 1
        self.assert_inconclusive(value, 'OVERLAP_OR_TOUCH')

    def test_overlap_control_credit_never_update_and_zero_poll_allowed(self):
        result = audit(fixture([release(context='control'), wait(start=109, end=130),
            release(3, 150, 160), wait(4, 145, 165, timeout='zero')]))
        self.assertEqual([r['release_op_id'] for r in result['associations']], [3])
        self.assertEqual(len(result['unique_credit_consumptions']), 2)

    def test_overlap_neighbor_components_require_strict_gap(self):
        rows = [release(), wait(start=109, end=130), release(3, 140, 150),
                wait(4, 131, 155)]
        result = audit(fixture(rows))
        self.assertEqual(len(result['associations']), 2)
        rows[-1]['entry_qpc'] = 130  # touching joins all four operations
        self.assert_inconclusive(fixture(rows), 'OVERLAP_OR_TOUCH')
        # A third operation touching the opposite edge also joins the component.
        self.assert_inconclusive(fixture([wait(3, 105, 109, result='timeout'),
            release(), wait(start=109, end=130)]), 'OVERLAP_OR_TOUCH')

    def test_overlap_requires_fresh_complete_history_globally(self):
        for scope, field, replacement, code in (
            ('top', 'history_complete', False, 'JOURNAL_HISTORY'),
            ('epoch', 'history_complete', False, 'EPOCH_HISTORY'),
            ('epoch', 'origin', 'restored', 'NONFRESH'),
            ('epoch', 'initial_count', 1, 'INITIAL_CREDITS'),
        ):
            with self.subTest(scope=scope, field=field):
                value = fixture([release(), wait(start=109, end=130)])
                target = value if scope == 'top' else value['epochs'][0]
                target[field] = replacement
                result = self.assert_inconclusive(value, code)
                self.assertEqual(result['unique_credit_consumptions'], [])

    def test_disjoint_wait_before_release_not_salvaged_as_overlap(self):
        self.assert_inconclusive(fixture([wait(start=105, end=109), release()]),
                                 'SUCCESSFUL_WAIT_WITHOUT_CREDIT')

    def test_completion_order_and_op_id_order_do_not_define_execution(self):
        result = audit(fixture([wait(1), release(99)]))
        self.assertEqual(result['associations'][0]['release_op_id'], 99)
        self.assertEqual(result['associations'][0]['wait_op_id'], 1)

    def test_empty_and_unconsumed_credit_no_candidate(self):
        self.assert_inconclusive(fixture([]), 'NO_UNAMBIGUOUS_UPDATE')
        result = self.assert_inconclusive(fixture([release()]), 'NO_UNAMBIGUOUS_UPDATE')
        self.assertEqual(result['epoch_summaries'][0]['unconsumed_release_op_id'], 1)

    def test_previous_count_and_maximum_count_contradictions(self):
        self.assert_inconclusive(fixture([release(previous=1), wait()]), 'PREVIOUS_COUNT')
        value = fixture([release(count=2), wait()]); value['epochs'][0]['maximum_count'] = 1
        self.assert_inconclusive(value, 'EXCEEDS_MAXIMUM')

    def test_all_unsupported_operations_visible_no_prefix_salvage(self):
        for kind in ('unknown', 'save', 'signal_and_wait', 'multiple_wait',
                     'restore', 'lifecycle_gap', 'future_operation'):
            with self.subTest(kind=kind):
                value = fixture([release(), wait(), operation(3, 150, 160, kind, {})])
                self.assert_inconclusive(value, 'UNSUPPORTED_OPERATION_' + kind.upper())

    def test_unsupported_other_epoch_still_invalidates_whole_journal(self):
        value = fixture([release(), wait(), operation(3, 150, 160, 'save', {}, eid=2)],
                        [epoch(), epoch(2, cookie=2000)])
        self.assert_inconclusive(value, 'UNSUPPORTED_OPERATION_SAVE')

    def test_unsupported_wait_results_visible(self):
        for outcome in ('failed', 'abandoned', 'unknown'):
            with self.subTest(outcome=outcome):
                self.assert_inconclusive(fixture([release(), wait(result=outcome)]),
                                         'UNSUPPORTED_WAIT_RESULT')

    def test_overflow_and_undrained_operations_suppress_candidates(self):
        value = fixture(); value['counts'].update(observed_operations=3,
                                                  completed_operations=3, overflow_operations=1)
        self.assert_inconclusive(value, 'OVERFLOW')
        value['counts']['completed_operations'] = 2
        self.assert_inconclusive(value, 'NOT_DRAINED')

    def test_history_observer_and_nonfresh_origins(self):
        value = fixture(); value['history_complete'] = False
        self.assert_inconclusive(value, 'JOURNAL_HISTORY')
        value = fixture(); value['observer_failed'] = True
        self.assert_inconclusive(value, 'OBSERVER_FAILED')
        value = fixture(); value['epochs'][0]['history_complete'] = False
        self.assert_inconclusive(value, 'EPOCH_HISTORY')
        for origin in ('native', 'restored', 'unknown'):
            value = fixture(); value['epochs'][0]['origin'] = origin
            self.assert_inconclusive(value, 'NONFRESH')

    def test_pointer_and_handle_reuse_separate_nonoverlapping_epochs(self):
        value = fixture([release(), wait(), release(3, 210, 220, eid=2),
                         wait(4, 230, 240, eid=2)],
                        [epoch(destroyed=150), epoch(2, created=200)])
        result = audit(value)
        self.assertEqual([row['epoch_id'] for row in result['associations']], [1, 2])
        value['epochs'][0]['destroy_qpc'] = None
        self.assert_inconclusive(value, 'COOKIE_LIFETIMES')

    def test_independent_epochs_may_overlap_without_global_order(self):
        result = audit(fixture([release(), wait(), release(3, eid=2), wait(4, eid=2)],
                               [epoch(), epoch(2, cookie=2000)]))
        self.assertEqual(len(result['associations']), 2)

    def test_qpc_boundaries_zero_backwards_and_lifetime_touch(self):
        for start, end in ((0, 120), (120, 110), (END - 1, END), (END, END)):
            with self.subTest(start=start, end=end):
                self.assert_inconclusive(fixture([release(start=start, end=end), wait()]),
                                         'INVALID_OR_OUTSIDE')
        value = fixture(); value['epochs'][0]['creation_qpc'] = 110
        self.assert_inconclusive(value, 'TOUCHES_OR_EXCEEDS_LIFETIME')
        value = fixture(); value['epochs'][0]['destroy_qpc'] = 140
        self.assert_inconclusive(value, 'TOUCHES_OR_EXCEEDS_LIFETIME')
        value = fixture(); value['epochs'][0]['creation_qpc'] = 0
        self.assert_inconclusive(value, 'CREATION_OUTSIDE')
        value = fixture(); value['epochs'][0]['destroy_qpc'] = END
        self.assert_inconclusive(value, 'DESTRUCTION_BRACKET')

    def test_zero_duration_operation_allowed_if_other_brackets_disjoint(self):
        self.assertEqual(len(audit(fixture([release(start=110, end=110), wait()]))['associations']), 1)

    def test_duplicate_ids_and_missing_epoch_rejected(self):
        value = fixture(); value['operations'][1]['op_id'] = 1
        with self.assertRaises(ValueError): audit(value)
        value = fixture(); value['operations'][1]['epoch_id'] = 2
        with self.assertRaises(ValueError): audit(value)
        value = fixture(); value['epochs'].append(epoch())
        with self.assertRaises(ValueError): audit(value)

    def test_exact_types_fields_and_counter_consistency(self):
        mutations = [lambda v: v.update(extra=0),
                     lambda v: v.update(native_admitted=True),
                     lambda v: v['counts'].update(stored_operations=1),
                     lambda v: v['counts'].update(completed_operations=1),
                     lambda v: v['counts'].update(observed_operations=3),
                     lambda v: v['operations'][0].update(op_id=True),
                     lambda v: v['operations'][0].update(entry_qpc=credit.MAX_QPC + 1),
                     lambda v: v['operations'][0].update(native_thread_id=0),
                     lambda v: v['operations'][0]['payload'].update(success=1),
                     lambda v: v['operations'][0]['payload'].update(previous_count=None),
                     lambda v: v['operations'][0]['payload']['context'].update(generation=0),
                     lambda v: v['epochs'][0].update(maximum_count=0),
                     lambda v: v['epochs'][0].update(initial_count=9)]
        for mutate in mutations:
            value = fixture(); mutate(value)
            with self.subTest(mutation=mutate), self.assertRaises(ValueError): audit(value)

    def test_byte_bound_unknown_schema_duplicate_keys_floats_and_constants(self):
        for raw in (b'', b' ' * (credit.MAX_BYTES + 1), b'[]',
                    b'{"schema":1,"schema":2}', b'{"a":NaN}', b'{"a":1.5}',
                    b'{"a":-1}', b'{"a":123456789012345678901}', b'\xff'):
            with self.subTest(raw=raw[:40]), self.assertRaises(ValueError): audit(raw)
        value = fixture(); value['schema'] = 'newer'
        with self.assertRaises(ValueError): audit(value)
        with self.assertRaises(ValueError): audit(bytearray(encoded(fixture())))

    def test_expected_identity_binding(self):
        for kwargs in ({'token': 'f' * 32}, {'begin_qpc': BEGIN + 1},
                       {'end_qpc': END + 1}, {'qpc_frequency': FREQUENCY + 1},
                       {'qpc_frequency': True}, {'begin_qpc': END}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                audit(fixture(), **kwargs)

    def test_operation_and_epoch_caps(self):
        value = fixture([]); value['epochs'] = [epoch(i + 1, cookie=i + 1)
                                               for i in range(credit.MAX_EPOCHS + 1)]
        with self.assertRaises(ValueError): audit(value)
        value = fixture([wait(i + 1, result='timeout') for i in range(credit.CAPACITY + 1)])
        with self.assertRaises(ValueError): audit(value)


if __name__ == '__main__':
    unittest.main()
