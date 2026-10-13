"""Synthetic, standard-library-only tests; no native handles or file writes."""
import copy
import hashlib
import itertools
import json
import unittest
from unittest import mock

import mcla_semaphore_prefix_audit as prefix
import mcla_semaphore_credit_audit as v1

TOKEN = '4' * 32
BEGIN, FREEZE, FREQUENCY = 100, 1000, 10000


def epoch(eid=1, cookie=200, created=101, destroyed=None):
    return dict(epoch_id=eid, object_cookie=cookie, creation_qpc=created,
                destroy_qpc=destroyed, initial_count=0, maximum_count=8,
                origin='fresh', history_complete=True)


def op(oid, start, end, kind, payload, *, eid=1, completed=True, handle=12):
    return dict(op_id=oid, epoch_id=eid, native_thread_id=25, guest_handle=handle,
                entry_qpc=start, exit_qpc=end, kind=kind, payload=payload,
                completed=completed)


def release(oid=1, start=110, end=120, *, count=1, previous=0, success=True,
            context='update', generation=7, eid=1, completed=True, handle=12):
    identity = {'thread_epoch': 1, 'serial': generation} if context == 'update' else None
    return op(oid, start, end, 'release',
              dict(count=count, previous_count=previous, success=success,
                   context={'kind': context, 'generation': identity}),
              eid=eid, completed=completed, handle=handle)


def wait(oid=2, start=130, end=140, *, outcome='success', eid=1,
         completed=True, timeout='infinite', handle=12):
    return op(oid, start, end, 'wait', dict(result=outcome, timeout_kind=timeout),
              eid=eid, completed=completed, handle=handle)


def loss(first=3, last=3, bound=150, kind='overflow'):
    return dict(first_op_id=first, last_op_id=last, entry_lower_bound_qpc=bound, kind=kind)


def fixture(operations=None, epochs=None, losses=None):
    operations = [release(), wait()] if operations is None else operations
    losses = [] if losses is None else losses
    lost = sum(x['last_op_id'] - x['first_op_id'] + 1 for x in losses)
    done = sum(x['completed'] for x in operations)
    result = dict.fromkeys(prefix.FALSE_FLAGS, False)
    result.update(schema='mcla-semaphore-prefix-journal-v2', token=TOKEN,
                  clock='win32-query-performance-counter', qpc_frequency=FREQUENCY,
                  observer_begin_qpc=BEGIN, freeze_qpc=FREEZE, capacity=prefix.CAPACITY,
                  admission_inventory_complete=True, observer_failed=False,
                  unknown_coverage_gap=False, epochs=[epoch()] if epochs is None else epochs,
                  operations=operations, loss_ranges=losses,
                  counts=dict(observed_operations=len(operations) + lost,
                              stored_operations=len(operations), completed_operations=done,
                              pending_operations=len(operations) - done, lost_operations=lost))
    return result


def raw(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode('ascii')


def audit(value):
    return prefix.audit(raw(value), token=TOKEN, observer_begin_qpc=BEGIN,
                        freeze_qpc=FREEZE, qpc_frequency=FREQUENCY)


class PrefixTests(unittest.TestCase):
    def assert_candidate(self, value, count=1):
        result = audit(value)
        self.assertEqual(len(result['associations']), count)
        self.assertEqual(result['status'], 'SEMAPHORE_PREFIX_CANDIDATES_REVIEW_PENDING')
        self.assertEqual(result['diagnostic_issues'], [])
        for flag in prefix.FALSE_FLAGS:
            self.assertIs(result[flag], False)
        return result

    def assert_inconclusive(self, value, issue=None):
        result = audit(value)
        self.assertEqual(result['associations'], [])
        self.assertEqual(result['status'], 'SEMAPHORE_PREFIX_AUDIT_INCONCLUSIVE')
        if issue:
            self.assertTrue(any(issue in x for x in result['diagnostic_issues']))
        return result

    def test_disjoint_complete_pair_digest_and_real_boundary(self):
        value = fixture(); before = copy.deepcopy(value)
        result = self.assert_candidate(value)
        self.assertEqual(value, before)
        self.assertEqual(result['original_receipt_sha256'], hashlib.sha256(raw(value)).hexdigest().upper())
        self.assertEqual(result['prefixes'][0]['end_qpc_exclusive'], FREEZE)
        self.assertEqual(result['associations'][0]['release_op_id'], 1)
        self.assertEqual(result['associations'][0]['wait_op_id'], 2)
        self.assertEqual(result['excluded_operation_ids'], [])

    def test_wait_before_release_unique_overlap(self):
        result = self.assert_candidate(fixture([
            wait(1, 110, 145), release(2, 120, 140)]))
        self.assertEqual(result['associations'][0]['association_rule'], 'isolated-overlap-single-credit')

    def test_later_shutdown_batch_keeps_earlier_pair_not_v1(self):
        value = fixture([release(), wait(), release(3, 150, 160, count=8, context='control')])
        result = self.assert_candidate(value)
        self.assertEqual(result['prefixes'][0]['end_qpc_exclusive'], 150)
        self.assertEqual(result['excluded_operation_ids'], [3])
        native_like = prefix._projection(value, value['epochs'][0], value['operations'], FREEZE)
        old = v1.audit(native_like, token=TOKEN, begin_qpc=BEGIN, end_qpc=FREEZE, qpc_frequency=FREQUENCY)
        self.assertEqual(old['associations'], [])
        self.assertTrue(any('COMPETING_OR_BATCH' in x for x in old['diagnostic_issues']))

    def test_later_control_pair_never_becomes_update(self):
        result = self.assert_candidate(fixture([release(), wait(),
            release(3, 150, 160, context='control'), wait(4, 170, 180)]))
        self.assertEqual(len(result['unique_credit_consumptions']), 2)

    def test_pending_wait_after_pair_retained_without_fabrication(self):
        row = wait(3, 150, None, completed=False, outcome=None)
        value = fixture([release(), wait(), row]); original = raw(value)
        result = self.assert_candidate(value)
        self.assertEqual(result['global_barrier_qpc'], 150)
        self.assertEqual(result['excluded_operation_ids'], [3])
        self.assertEqual(result['original_counts']['pending_operations'], 1)
        self.assertEqual(raw(value), original)

    def test_pending_release_has_no_fabricated_outcome(self):
        row = release(3, 150, None, completed=False, success=None, previous=None)
        self.assert_candidate(fixture([release(), wait(), row]))
        for key, value in [('success', False), ('previous_count', 0)]:
            bad = copy.deepcopy(row); bad['payload'][key] = value
            with self.assertRaises(ValueError): audit(fixture([release(), wait(), bad]))

    def test_early_pending_blocks_all_later_pairs(self):
        result = self.assert_inconclusive(fixture([
            wait(1, 105, None, completed=False, outcome=None),
            release(2, 110, 120), wait(3, 130, 140)]))
        self.assertEqual(result['global_barrier_qpc'], 105)
        self.assertEqual(result['excluded_operation_ids'], [1, 2, 3])

    def test_pending_in_other_epoch_is_global_barrier(self):
        self.assert_inconclusive(fixture([
            wait(1, 105, None, completed=False, outcome=None, eid=2),
            release(2), wait(3)], epochs=[epoch(), epoch(2, 201)]))

    def test_boundary_touch_excludes_connected_component(self):
        result = self.assert_inconclusive(fixture([release(), wait(),
            wait(3, 140, None, completed=False, outcome=None)]))
        self.assertEqual(result['prefixes'][0]['operation_ids'], [])
        self.assertLessEqual(result['prefixes'][0]['end_qpc_exclusive'], 130)

    def test_transitive_component_not_peeled_into_convenient_pair(self):
        self.assert_inconclusive(fixture([
            release(1, 110, 130), wait(2, 120, 145),
            wait(3, 145, None, completed=False, outcome=None)]))

    def test_completed_exit_at_freeze_not_admitted(self):
        self.assert_inconclusive(fixture([release(), wait(2, 130, FREEZE)]))

    def test_zero_balance_rollback_has_observed_not_synthetic_end(self):
        result = self.assert_candidate(fixture([release(), wait(), release(3, 150, 160)]))
        self.assertEqual(result['prefixes'][0]['end_qpc_exclusive'], 150)
        self.assertEqual(result['prefixes'][0]['ending_count'], 0)
        self.assertEqual(result['excluded_operation_ids'], [3])

    def test_no_zero_component_means_no_prefix(self):
        self.assert_inconclusive(fixture([release()]))

    def test_unsupported_suffix_all_explicit_kinds(self):
        for kind in ('save', 'multiple_wait', 'signal_and_wait', 'restore', 'future_unknown'):
            with self.subTest(kind=kind):
                result = self.assert_candidate(fixture([release(), wait(), op(3, 150, 160, kind, {})]))
                self.assertEqual(result['global_barrier_qpc'], 150)
                self.assertEqual(result['excluded_operation_ids'], [3])

    def test_unsupported_first_has_no_candidates(self):
        self.assert_inconclusive(fixture([op(1, 105, 106, 'save', {}), release(2), wait(3)]))

    def test_unsupported_wait_result_is_early_barrier(self):
        for outcome in ('failed', 'abandoned', 'unknown'):
            result = self.assert_candidate(fixture([release(), wait(), wait(3, 150, 160, outcome=outcome)]))
            self.assertEqual(result['global_barrier_qpc'], 150)

    def test_failed_release_and_timeout_apc_alerted_no_consumption(self):
        for outcome in ('timeout', 'apc', 'alerted'):
            value = fixture([wait(1, 105, 107, outcome=outcome, timeout='zero'),
                             release(2, 108, 109, success=False, previous=None),
                             release(3, 110, 120), wait(4, 130, 140, timeout='zero')])
            self.assert_candidate(value)

    def test_overlapping_failed_release_not_ignored(self):
        self.assert_inconclusive(fixture([release(1, 110, 130),
            release(2, 115, 120, success=False, previous=None), wait(3, 125, 140)]))

    def test_later_bounded_loss_preserves_prefix_and_inventory(self):
        for kind in ('overflow', 'coverage_gap', 'unsupported_access'):
            item = loss(3, 5, 150, kind)
            value = fixture(losses=[item]); result = self.assert_candidate(value)
            self.assertEqual(result['loss_ranges'], [item])
            self.assertEqual(result['original_counts']['lost_operations'], 3)
            self.assertEqual(result['global_barrier_qpc'], 150)

    def test_null_loss_bound_or_unknown_gap_rejects_all(self):
        self.assert_inconclusive(fixture(losses=[loss(bound=None)]), 'LOSS_WITHOUT')
        value = fixture(); value['unknown_coverage_gap'] = True
        self.assert_inconclusive(value, 'UNBOUNDED_COVERAGE')

    def test_loss_id_partition_omissions_overlaps_and_tail(self):
        values = [fixture([release(), wait(3)]), fixture(losses=[loss(4, 4)]),
                  fixture(losses=[loss(2, 3)]), fixture(losses=[loss(3, 4), loss(4, 5)])]
        for value in values:
            with self.subTest(value=value), self.assertRaises(ValueError): audit(value)

    def test_large_loss_ranges_use_intervals_not_enumeration(self):
        value = fixture(losses=[loss(3, 2**63, 150)])
        result = self.assert_candidate(value)
        self.assertEqual(result['original_counts']['observed_operations'], 2**63)

    def test_nonmonotonic_entry_ids_and_loss_bounds_rejected(self):
        for value in (fixture([release(2), wait(1)]), fixture(losses=[loss(bound=120)])):
            with self.assertRaises(ValueError): audit(value)

    def test_unknown_operation_clock_rejects_even_late_suffix(self):
        for key in ('entry_qpc', 'exit_qpc'):
            tail = release(3, 150, 160, context='control'); tail[key] = None
            self.assert_inconclusive(fixture([release(), wait(), tail]), 'UNKNOWN_OPERATION_CLOCK')

    def test_coverage_flags_not_flipped_for_projection(self):
        for scope, key, value in [('top', 'observer_failed', True),
                                  ('top', 'admission_inventory_complete', False),
                                  ('epoch', 'history_complete', False),
                                  ('epoch', 'origin', 'restored'),
                                  ('epoch', 'initial_count', 1),
                                  ('epoch', 'creation_qpc', BEGIN)]:
            source = fixture()
            (source if scope == 'top' else source['epochs'][0])[key] = value
            self.assert_inconclusive(source)

    def test_late_evaluated_counter_contradiction_rejects_earlier_pair(self):
        for tail in (release(3, 150, 160, previous=1), wait(3, 150, 160)):
            self.assert_inconclusive(fixture([release(), wait(), tail]), 'PREFIX')
        self.assert_inconclusive(fixture([release(), wait(), release(3, 150, 160),
                                          wait(4, 170, 180, outcome='timeout')]), 'PREFIX')

    def test_other_epoch_contradiction_clears_already_evaluated_candidates(self):
        value = fixture([release(), wait(), wait(3, 150, 160, eid=2)],
                        [epoch(), epoch(2, 201)])
        result = self.assert_inconclusive(value, 'PREFIX')
        self.assertTrue(all(not p['prefix_eligible'] and p['candidate_count'] == 0
                            for p in result['prefixes']))
        self.assertEqual(result['excluded_operation_ids'], [1, 2, 3])

    def test_unmodeled_suffix_not_claimed_ledger_consistent(self):
        value = fixture([release(), wait(), release(3, 150, 160, count=2),
                         release(4, 170, 180, previous=0)])
        result = self.assert_candidate(value)
        self.assertEqual(result['excluded_operation_ids'], [3, 4])
        self.assertEqual(result['suffix_semantics'], 'RETAINED_UNMODELED_NOT_ASSERTED_COUNTER_CONSISTENT')

    def test_full_suffix_schema_checked_before_candidates(self):
        value = fixture([release(), wait(), release(3, 150, 160, count=2),
                         release(4, 170, 180, count=9)])
        with self.assertRaises(ValueError): audit(value)
        value['operations'][-1]['payload']['count'] = 1
        value['operations'][-1]['payload']['unexpected'] = 0
        with self.assertRaises(ValueError): audit(value)

    def test_malformed_final_metadata_never_calls_candidate_evaluator(self):
        values = [fixture([release(), wait(), release(3, 150, 160, count=2),
                           release(4, 170, 180, count=9)]),
                  fixture(losses=[loss(4, 4)])]
        for value in values:
            with mock.patch.object(v1, 'audit', side_effect=AssertionError('premature evaluator')) as evaluator:
                with self.assertRaises(ValueError): audit(value)
                evaluator.assert_not_called()

    def test_handle_aliases_same_epoch_and_object_reuse_separate(self):
        self.assert_candidate(fixture([release(handle=12), wait(handle=99)]))
        value = fixture([release(), wait(), release(3, 210, 220, eid=2), wait(4, 230, 240, eid=2)],
                        [epoch(destroyed=150), epoch(2, created=200)])
        self.assert_candidate(value, 2)
        value['epochs'][0]['destroy_qpc'] = None
        self.assert_inconclusive(value, 'COOKIE_REUSE')

    def test_pending_with_destroyed_epoch_rejected(self):
        value = fixture([wait(1, 110, None, outcome=None, completed=False)], [epoch(destroyed=150)])
        with self.assertRaises(ValueError): audit(value)

    def test_all_counter_mismatches_rejected(self):
        for key in prefix.COUNT_KEYS:
            value = fixture(); value['counts'][key] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): audit(value)

    def test_bool_unknownkeys_duplicateids_and_malformed_outcomes(self):
        mutations = [lambda v: v.update(capacity=True),
                     lambda v: v.update(claimed_cutoff=150),
                     lambda v: v['operations'][1].update(op_id=1),
                     lambda v: v['operations'][0]['payload'].update(success=False),
                     lambda v: v['operations'][0].update(completed=1),
                     lambda v: v['operations'][1].update(exit_qpc=FREEZE+1)]
        for change in mutations:
            value = fixture(); change(value)
            with self.subTest(value=value), self.assertRaises(ValueError): audit(value)

    def test_input_bounds_and_duplicate_json_keys(self):
        value = raw(fixture())
        cases = [value.decode('ascii'), b'[]', b'{"schema":1,"schema":1}',
                 value.replace(b'10000', b'1e4'), value.replace(b'10000', b'-1'),
                 b' ' * (prefix.MAX_BYTES+1)]
        for data in cases:
            with self.subTest(type=type(data)), self.assertRaises(ValueError):
                prefix.audit(data, token=TOKEN, observer_begin_qpc=BEGIN,
                             freeze_qpc=FREEZE, qpc_frequency=FREQUENCY)

    def test_returned_nested_ids_are_detached(self):
        value = fixture(); result = self.assert_candidate(value)
        result['associations'][0]['context']['generation']['serial'] = 99
        self.assertEqual(result['unique_credit_consumptions'][0]['context']['generation']['serial'], 7)
        self.assertEqual(value['operations'][0]['payload']['context']['generation']['serial'], 7)

    def test_collection_caps_proof_flags_and_empty_coverage(self):
        self.assert_inconclusive(fixture([]))
        for flag in prefix.FALSE_FLAGS:
            value = fixture(); value[flag] = True
            with self.subTest(flag=flag), self.assertRaises(ValueError): audit(value)
        excessive = fixture([wait(i+1, 110, 111, outcome='timeout')
                             for i in range(prefix.CAPACITY+1)])
        with self.assertRaises(ValueError): audit(excessive)
        excessive = fixture([], losses=[loss(i+1, i+1, 120)
                                        for i in range(prefix.MAX_LOSS_RANGES+1)])
        with self.assertRaises(ValueError): audit(excessive)
        excessive = fixture([], epochs=[epoch(i+1, 200+i) for i in range(v1.MAX_EPOCHS+1)])
        with self.assertRaises(ValueError): audit(excessive)

    def test_update_identity_pair_distinguishes_thread_lifetimes(self):
        value = fixture([release(), wait(), release(3, 150, 160), wait(4, 170, 180)])
        value['operations'][2]['payload']['context']['generation']['thread_epoch'] = 2
        result = self.assert_candidate(value, 2)
        self.assertEqual([m['context']['generation'] for m in result['associations']],
                         [{'thread_epoch': 1, 'serial': 7}, {'thread_epoch': 2, 'serial': 7}])

    def test_every_candidate_agrees_with_all_small_legal_linearizations(self):
        # Independent finite oracle: two one-credit releases (each actually
        # observed previous_count=0) and two successful waits. Enumerate every
        # compatible native-effect order, not a FIFO interpretation of begin IDs.
        brackets = [(110, 120), (120, 140), (130, 150), (160, 170)]
        admitted = 0
        for selected in itertools.product(brackets, repeat=4):
            original = [release(1, *selected[0], generation=1),
                        release(2, *selected[1], generation=2),
                        wait(3, *selected[2]), wait(4, *selected[3])]
            rows = sorted(original, key=lambda r: (r['entry_qpc'], r['op_id']))
            for oid, row in enumerate(rows, 1): row['op_id'] = oid
            possible = []
            for order in itertools.permutations(rows):
                positions = {r['op_id']: i for i, r in enumerate(order)}
                if any(a['exit_qpc'] < b['entry_qpc'] and
                       positions[a['op_id']] >= positions[b['op_id']]
                       for a in rows for b in rows if a is not b):
                    continue
                outstanding, consumed = None, {}
                for row in order:
                    if row['kind'] == 'release':
                        if outstanding is not None: break  # actual previous=0
                        outstanding = row['op_id']
                    else:
                        if outstanding is None: break
                        consumed[row['op_id']] = outstanding
                        outstanding = None
                else:
                    possible.append(consumed)
            result = audit(fixture(rows))
            for candidate in result['associations']:
                admitted += 1
                self.assertTrue(possible, 'cannot accept physically inconsistent history')
                self.assertTrue(all(mapping.get(candidate['wait_op_id']) == candidate['release_op_id']
                                    for mapping in possible), 'association must hold for EVERY legal order')
        self.assertGreater(admitted, 0, 'oracle matrix must exercise positive candidates')


if __name__ == '__main__':
    unittest.main()
