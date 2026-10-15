"""Pure capacity regressions; no native launch, game data or file writes."""
import json
import unittest

import mcla_frame_handoff_decode as handoff
import mcla_owner_ready_decode as ready
import mcla_semaphore_credit_audit as credit
import mcla_semaphore_prefix_audit as prefix
import test_mcla_frame_handoff_decode as h
import test_mcla_semaphore_credit_audit as c
import test_mcla_semaphore_prefix_audit as p
from test_mcla_owner_ready import data as ready_data


class HistoryLimitsTests(unittest.TestCase):
    def test_supported_capacities_preserve_candidates_and_projection(self):
        for capacity in (4096, 32768):
            with self.subTest(capacity=capacity):
                value = c.fixture(); value['capacity'] = capacity
                self.assertEqual(len(c.audit(value)['associations']), 1)
                value = p.fixture(); value['capacity'] = capacity
                projected = json.loads(prefix._projection(value, value['epochs'][0],
                    value['operations'], p.FREEZE))
                self.assertEqual(projected['capacity'], capacity)
                self.assertEqual(len(p.audit(value)['associations']), 1)
                values = h.fixture(); values[1]['capacity'] = capacity
                self.assertEqual(len(h.decode(values)['candidate_records']), 91)
        self.assertEqual((credit.CAPACITY, prefix.CAPACITY), (4096, 4096))

    def test_unknown_or_noninteger_capacities_rejected(self):
        for capacity in (True, False, 4096.0, '32768', None, 0, 8192, 32767, 32769):
            with self.subTest(capacity=capacity):
                value = c.fixture(); value['capacity'] = capacity
                with self.assertRaises(ValueError): c.audit(value)
                value = p.fixture(); value['capacity'] = capacity
                with self.assertRaises(ValueError): p.audit(value)
                values = h.fixture(); values[1]['capacity'] = capacity
                with self.assertRaises(ValueError): h.decode(values)

    def test_prefix_above_old_capacity_projects_without_rewriting(self):
        value = p.fixture([p.wait(i+1, 110+i*2, 111+i*2, outcome='timeout')
                           for i in range(4098)])
        value.update(capacity=32768, freeze_qpc=10000)
        result = prefix.audit(p.raw(value), token=p.TOKEN, observer_begin_qpc=p.BEGIN,
                              freeze_qpc=10000, qpc_frequency=p.FREQUENCY)
        self.assertEqual(result['validated_operation_count'], 4098)
        self.assertEqual(result['diagnostic_issues'], [])
        self.assertEqual(len(result['prefixes'][0]['operation_ids']), 4098)
        value['capacity'] = 4096
        with self.assertRaisesRegex(ValueError, 'bounded operations'):
            prefix.audit(p.raw(value), token=p.TOKEN, observer_begin_qpc=p.BEGIN,
                         freeze_qpc=10000, qpc_frequency=p.FREQUENCY)

    def test_declared_capacity_bounds_both_metadata_passes(self):
        for capacity in (4096, 32768):
            value = c.fixture(); value['capacity'] = capacity
            value['operations'] *= capacity//2+1
            with self.assertRaisesRegex(ValueError, 'bounded operations'): c.audit(value)
            value = p.fixture(); value['capacity'] = capacity
            value['operations'] *= capacity//2+1
            with self.assertRaisesRegex(ValueError, 'bounded operations'): p.audit(value)
            values = h.fixture(); values[1]['capacity'] = capacity
            values[1]['operations'] *= capacity//2+1
            with self.assertRaisesRegex(ValueError, 'bounded prefix operations'): h.decode(values)

    def test_annotations_bound_by_paired_declaration_not_maximum(self):
        for capacity in (4096, 32768):
            values = h.fixture(); values[1]['capacity'] = capacity
            values[0]['operation_annotations'] *= capacity//2+1
            with self.assertRaisesRegex(ValueError, 'bounded operation_annotations'):
                h.decode(values)

    def test_new_capacity_handoff_above_legacy_annotation_count(self):
        values = h.fixture(); hand, pre = values[:2]
        pre['capacity'] = 32768
        first = pre['operations'][-1]['exit_qpc']+1
        for i in range(4096):
            row = p.wait(i+3, first+i*2, first+i*2+1, outcome='timeout')
            row['native_thread_id'] = 28500
            pre['operations'].append(row)
            hand['operation_annotations'].append(dict(operation_id=i+3,
                phase_call_id=0, caller_lr=0, native_thread_id=28500, annotated=False))
        pre['counts'].update(observed_operations=4098, stored_operations=4098,
                             completed_operations=4098)
        result = h.decode(values)
        self.assertEqual(len(result['handoff_inventory']['operation_annotations']), 4098)
        self.assertEqual(result['prefix_audit']['validated_operation_count'], 4098)
        self.assertIn('UNANNOTATED_OPERATION', result['diagnostic_issues'])
        self.assertEqual(result['candidate_records'], [])

    def test_new_worker_bound_and_unchanged_scopes(self):
        values = h.fixture(); values[1]['capacity'] = 32768
        event = h.worker_event(values[0]['freeze_qpc'], 10)
        values[0]['worker_events'] = [event] * 65536
        result = h.decode(values)
        self.assertEqual(result['candidate_records'], [])
        self.assertIn('WORKER_OBSERVER_FAULT', result['diagnostic_issues'])
        values[0]['worker_events'].append(event)
        with self.assertRaisesRegex(ValueError, 'bounded worker_events'): h.decode(values)
        values = h.fixture(); values[0]['scopes'] *= 684
        with self.assertRaisesRegex(ValueError, 'bounded scopes'): h.decode(values)

    def test_legacy_owner5_loss_shape_stays_inconclusive(self):
        value = p.fixture([p.wait(i+1, 110+i*2, 111+i*2, outcome='timeout')
                           for i in range(4096)], losses=[p.loss(4097, 4890, None)])
        value['freeze_qpc'] = 10000
        result = prefix.audit(p.raw(value), token=p.TOKEN, observer_begin_qpc=p.BEGIN,
                              freeze_qpc=10000, qpc_frequency=p.FREQUENCY)
        self.assertEqual(result['status'], 'SEMAPHORE_PREFIX_AUDIT_INCONCLUSIVE')
        self.assertIn('LOSS_WITHOUT_RELIABLE_ENTRY_LOWER_BOUND', result['diagnostic_issues'])
        self.assertEqual(result['original_counts']['lost_operations'], 794)
        self.assertEqual(result['associations'], [])

    def test_serialized_limits_and_owner_ready_independent_reader(self):
        self.assertEqual(credit.MAX_BYTES, 32*1024*1024)
        self.assertEqual(prefix.MAX_BYTES, 32*1024*1024)
        self.assertEqual(handoff.MAX_BYTES, 48*1024*1024)
        value, signal, source, hand = ready_data()
        padded = json.dumps(hand).encode() + b' ' * (8*1024*1024)
        self.assertTrue(ready.validate(json.dumps(value).encode(), signal, source,
                                      padded, value['token'])[
                                      'owner_ready_assertion_validated'])
        with self.assertRaisesRegex(ValueError, 'bounded owner-ready'):
            ready.validate(json.dumps(value).encode(), signal, source,
                           b' ' * (48*1024*1024+1), value['token'])

    def test_reader_byte_boundaries_are_enforced_not_just_constants(self):
        raw = c.encoded(c.fixture())
        self.assertEqual(len(c.audit(raw.ljust(credit.MAX_BYTES, b' '))['associations']), 1)
        with self.assertRaisesRegex(ValueError, 'bounded immutable'):
            c.audit(raw.ljust(credit.MAX_BYTES+1, b' '))
        raw = p.raw(p.fixture())
        args = dict(token=p.TOKEN, observer_begin_qpc=p.BEGIN, freeze_qpc=p.FREEZE,
                    qpc_frequency=p.FREQUENCY)
        self.assertEqual(len(prefix.audit(raw.ljust(prefix.MAX_BYTES, b' '), **args)[
                             'associations']), 1)
        with self.assertRaisesRegex(ValueError, 'bounded immutable'):
            prefix.audit(raw.ljust(prefix.MAX_BYTES+1, b' '), **args)
        values = h.fixture()
        args = (p.raw(values[1]), p.raw(values[2]), p.raw(values[3]), values[4], h.TOKEN)
        raw = p.raw(values[0])
        self.assertEqual(len(handoff.validate_frame_handoff(
            raw.ljust(handoff.MAX_BYTES, b' '), *args)['candidate_records']), 91)
        with self.assertRaisesRegex(ValueError, 'bounded immutable'):
            handoff.validate_frame_handoff(raw.ljust(handoff.MAX_BYTES+1, b' '), *args)


if __name__ == '__main__':
    unittest.main()
