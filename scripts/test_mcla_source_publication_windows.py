"""Pure publication-witness tests; fixture readers never touch files/native IO."""
import copy
import hashlib
import unittest

import mcla_source_publication_windows as audit
from test_mcla_game_source_scout_decode import (
    ANCHOR, BEGIN, TOKEN, bmp, fail_suffix, fixture, names, unattempted)

EXECUTION = {'pid': 42, 'birth_100ns': 134363501998429344, 'host_sha256': 'A' * 64}


class PublicationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = bmp()

    def analyze(self, receipt, **kwargs):
        return audit.analyze(receipt, token=TOKEN, image_names=names(receipt),
                             read_image=lambda _: self.image, execution=EXECUTION, **kwargs)

    def prefix(self, ids=(61, 62, 63)):
        receipt = fixture(tuple(ids) + tuple(range(200, 200 + 91 - len(ids))))
        fail_suffix(receipt, len(ids))
        return receipt

    def assert_no_read(self, receipt, **kwargs):
        calls = []
        arguments = dict(token=TOKEN, image_names=names(receipt),
                         read_image=lambda name: calls.append(name) or self.image,
                         execution=EXECUTION)
        arguments.update(kwargs)
        with self.assertRaises(ValueError):
            audit.analyze(receipt, **arguments)
        self.assertEqual(calls, [])

    def test_full91_binding_bounds_and_no_admission(self):
        receipt = fixture(); original = copy.deepcopy(receipt)
        value = self.analyze(receipt)
        self.assertEqual(receipt, original)
        self.assertEqual(len(value['observations']), 91)
        self.assertEqual(len(value['adjacent_selected_publications']), 90)
        self.assertEqual(value['uncollected_slots'], 0)
        self.assertEqual(value['unique_sources'], 91)
        self.assertEqual(value['duplicate_sources'], 0)
        self.assertEqual(value['unsampled_source_ids_between_observed_endpoints'], 0)
        row = value['observations'][0]
        self.assertEqual(row['publication_qpc_bounds'], [BEGIN - 4, BEGIN + 3])
        self.assertEqual(row['source_binding'], {**EXECUTION, 'token': TOKEN,
                                               'presenter_epoch': 7, 'acquired_sequence': 61})
        self.assertEqual(row['bmp_sha256'], hashlib.sha256(self.image).hexdigest().upper())
        self.assertRegex(row['rgb_sha256'], '^[0-9A-F]{64}$')
        self.assertEqual(row['rgb_sha256'], hashlib.sha256(audit.scout._bmp_rgb(self.image)).hexdigest().upper())
        for key in audit.FALSE_FLAGS:
            self.assertIs(value[key], False)
        for key in ('cycle_row', 'landmarks', 'fps', 'rate', 'period'):
            self.assertNotIn(key, value)
        self.assertFalse(audit.NATIVE_ADMITTED)

    def test_duplicate_original_window_and_exact_zero_elapsed(self):
        value = self.analyze(self.prefix((61, 61, 61)))
        rows = value['observations'][:3]
        self.assertEqual([r['first_observed_slot'] for r in rows], [0, 0, 0])
        self.assertEqual([r['duplicate_source'] for r in rows], [False, True, True])
        self.assertTrue(all(r['publication_qpc_bounds'] == [BEGIN - 4, BEGIN + 3] for r in rows))
        for gap in value['adjacent_selected_publications']:
            self.assertIs(gap['same_publication'], True)
            self.assertEqual(gap['interval_difference_ticks_bounds'], [-7, 7])
            self.assertEqual(gap['publication_elapsed_ticks_bounds'], [0, 0])

    def test_null_slot_and_source_jump_are_separate(self):
        receipt = self.prefix((61, 62, 68))
        slot = receipt['sequence']['slots'][1]
        unattempted(slot, 'MISSED_DEADLINE', 'SLOT_WINDOW_ELAPSED', slot['due_qpc'] + 1000)
        value = self.analyze(receipt)
        self.assertIsNone(value['observations'][1]['publication_qpc_bounds'])
        self.assertIsNone(value['observations'][1]['source_binding'])
        self.assertEqual(value['uncollected_slots'], 89)
        gap = value['adjacent_selected_publications'][0]
        self.assertEqual(gap['uncollected_slots_between'], 1)
        self.assertEqual(gap['unsampled_source_ids_between'], 6)
        self.assertEqual(value['unsampled_source_ids_between_observed_endpoints'], 6)

    def test_duplicate_survives_null_slot(self):
        receipt = self.prefix((61, 61, 61))
        slot = receipt['sequence']['slots'][1]
        unattempted(slot, 'MISSED_DEADLINE', 'SLOT_WINDOW_ELAPSED', slot['due_qpc'] + 1000)
        value = self.analyze(receipt)
        self.assertEqual(value['observations'][2]['publication_qpc_bounds'], [BEGIN - 4, BEGIN + 3])
        self.assertTrue(value['adjacent_selected_publications'][0]['same_publication'])

    def test_distinct_overlap_and_equal_ticks_not_clamped(self):
        receipt = self.prefix((61, 62))
        capture = receipt['sequence']['slots'][1]['capture']
        capture['refresh_cpu_begin_qpc'] = BEGIN - 4
        capture['refresh_cpu_end_qpc'] = BEGIN - 4
        gap = self.analyze(receipt)['adjacent_selected_publications'][0]
        self.assertFalse(gap['same_publication'])
        self.assertEqual(gap['publication_elapsed_ticks_bounds'][0], -7)
        self.assertTrue(gap['witness_windows_overlap_or_touch'])
        receipt['sequence']['slots'][0]['capture']['refresh_cpu_end_qpc'] = BEGIN + 3
        capture['refresh_cpu_begin_qpc'] = BEGIN + 3
        capture['refresh_cpu_end_qpc'] = BEGIN + 3
        value = self.analyze(receipt)
        self.assertEqual(value['observations'][0]['publication_qpc_bounds'], [BEGIN + 3, BEGIN + 3])
        self.assertEqual(value['adjacent_selected_publications'][0]['publication_elapsed_ticks_bounds'][0], 0)

    def test_all_missed_no_reads_and_91_nulls(self):
        receipt = fixture(); receipt['status'] = audit.scout.INCOMPLETE
        for row in receipt['sequence']['slots']:
            unattempted(row, 'MISSED_DEADLINE', 'SLOT_WINDOW_ELAPSED', row['due_qpc'] + 1000)
        value = audit.analyze(receipt, token=TOKEN, image_names=[],
                             read_image=lambda _: self.fail('read'), execution=EXECUTION)
        self.assertEqual(len(value['observations']), 91)
        self.assertEqual(value['adjacent_selected_publications'], ())
        self.assertEqual(value['uncollected_slots'], 91)

    def test_callback_mutations_cannot_rebind_frozen_metadata(self):
        receipt = self.prefix(); execution = dict(EXECUTION); image_names = names(receipt)
        def read(_):
            receipt['sequence']['slots'][2]['capture']['acquired_sequence'] = 999
            receipt['sequence']['slots'][2]['capture']['refresh_cpu_end_qpc'] = 1
            receipt['token'] = 'f' * 32
            execution['pid'] = 999
            image_names.clear()
            return self.image
        value = audit.analyze(receipt, token=TOKEN, image_names=image_names,
                             read_image=read, execution=execution)
        row = value['observations'][2]
        self.assertEqual(row['source_binding']['acquired_sequence'], 63)
        self.assertEqual(row['source_binding']['token'], TOKEN)
        self.assertEqual(row['source_binding']['pid'], 42)
        self.assertEqual(row['publication_qpc_bounds'][0], BEGIN + 2000 - 4)

    def test_execution_schema_integer_hash_fail_before_reader(self):
        cases = [None, {}, {**EXECUTION, 'extra': False}]
        for key in ('pid', 'birth_100ns'):
            for bad in (True, 0, -1, 1.0, '1', 2**65):
                cases.append({**EXECUTION, key: bad})
        cases.extend(({**EXECUTION, 'pid': 2**32}, {**EXECUTION, 'birth_100ns': 2**64}))
        for bad in ('a' * 64, 'A' * 63, 'G' * 64, 'A' * 64 + '\n', None, True):
            cases.append({**EXECUTION, 'host_sha256': bad})
        for value in cases:
            with self.subTest(value=value):
                self.assert_no_read(self.prefix(), execution=value)

    def test_final_metadata_inventory_and_true_claims_before_reads(self):
        receipt = fixture(); receipt['sequence']['slots'][90]['capture']['acquire_after_qpc'] = True
        self.assert_no_read(receipt)
        self.assert_no_read(self.prefix(), token='f' * 32)
        self.assert_no_read(self.prefix(), image_names=['slot-00.bmp'])
        for key in audit.scout.FALSE_FLAGS:
            receipt = self.prefix(); receipt[key] = True
            self.assert_no_read(receipt)

    def test_duplicate_changed_metadata_and_rgb_reject(self):
        receipt = self.prefix((61, 61))
        receipt['sequence']['slots'][1]['capture']['refresh_cpu_end_qpc'] += 1
        self.assert_no_read(receipt)
        receipt = self.prefix((61, 61)); changed = bytearray(self.image); changed[54] ^= 1; calls = []
        def read(name):
            calls.append(name)
            return self.image if name == 'slot-00.bmp' else bytes(changed)
        with self.assertRaisesRegex(ValueError, 'changed RGB'):
            audit.analyze(receipt, token=TOKEN, image_names=names(receipt), read_image=read, execution=EXECUTION)
        self.assertEqual(calls, ['slot-00.bmp', 'slot-01.bmp'])

    def test_bad_bmp_and_callback_error_fail_without_retry(self):
        receipt = self.prefix(); calls = []
        def read(name):
            calls.append(name)
            return b'not BMP'
        with self.assertRaises(ValueError):
            audit.analyze(receipt, token=TOKEN, image_names=names(receipt), read_image=read, execution=EXECUTION)
        self.assertEqual(calls, ['slot-00.bmp'])
        def failed(_):
            raise RuntimeError('reader failed')
        with self.assertRaisesRegex(RuntimeError, 'reader failed'):
            audit.analyze(receipt, token=TOKEN, image_names=names(receipt), read_image=failed, execution=EXECUTION)

    def test_result_does_not_alias_inputs_or_other_rows(self):
        receipt = self.prefix(); execution = dict(EXECUTION)
        value = audit.analyze(receipt, token=TOKEN, image_names=names(receipt),
                             read_image=lambda _: self.image, execution=execution)
        value['execution_scope_claim']['pid'] = 100
        value['observations'][0]['source_binding']['pid'] = 101
        value['observations'][0]['publication_qpc_bounds'][0] = 1
        self.assertEqual(execution, EXECUTION)
        self.assertEqual(value['observations'][1]['source_binding']['pid'], 42)
        self.assertEqual(receipt['sequence']['slots'][0]['capture']['refresh_cpu_end_qpc'], BEGIN - 4)


if __name__ == '__main__':
    unittest.main()
