"""Synthetic pure metadata tests; no images, IO or native admission."""
import copy
import json
import unittest

import mcla_game_swap_decode as decoder
from test_mcla_game_source_scout_decode import fixture as scout_fixture
from test_mcla_game_source_scout_decode import fail_suffix, unattempted, TOKEN


def fixture(ids=None, *, frequency=10000):
    scout = scout_fixture(ids, frequency=frequency)
    epoch = int(TOKEN[:16], 16)
    begin = scout['sequence']['begin_qpc'] - frequency
    end = scout['sequence']['begin_qpc'] + 91 * frequency // 10
    records, seen = [], {}
    for row in scout['sequence']['slots']:
        capture = row['capture']
        identity = (capture['presenter_epoch'], capture['acquired_sequence'])
        if identity not in seen:
            consume = capture['refresh_cpu_begin_qpc'] - 1
            seen[identity] = {
                'epoch': epoch, 'serial': len(seen) + 1,
                'stock': [decoder.STOCK_MAGIC, decoder.U32, 1280, 720],
                'fetch': [0, 1, 2, 3, 4, decoder.U32], 'consume_qpc': consume,
                'snapshot_result': 'available',
                'snapshot': {'qpc_before': consume - 2, 'qpc_after': consume - 1,
                             'native_thread_id': 28500, 'guest_caller_lr': 0x822C21B4,
                             'timer_address': decoder.TIMER_ADDRESS,
                             'timer_counter': len(seen), 'timer_delta_bits': 0x3D088889}}
        records.append({'slot': row['slot'], 'presenter_epoch': identity[0],
                        'acquired_sequence': identity[1], 'binding': copy.deepcopy(seen[identity])})
    value = dict.fromkeys(decoder.FALSE_FLAGS, False)
    value.update(schema='mcla-game-swap-observer-v1', token=TOKEN,
                 clock='win32-query-performance-counter', epoch=epoch,
                 begin_qpc=begin, end_qpc_exclusive=end, qpc_frequency=frequency,
                 timer_address=decoder.TIMER_ADDRESS,
                 statistics={'issued': len(seen), 'matched': len(seen), 'rejected': 0,
                             'callback_failed': 0, 'failed': False}, records=records)
    return value, scout


def clear_missing(data, scout):
    for output, row in zip(data['records'], scout['sequence']['slots']):
        if row['capture'] is None:
            output.update(presenter_epoch=None, acquired_sequence=None, binding=None)


class SwapDecodeTests(unittest.TestCase):
    def validate(self, data, scout, token=TOKEN):
        return decoder.validate_swap_receipt(data, scout, token)

    def reject(self, data, scout, token=TOKEN):
        with self.assertRaises(ValueError):
            self.validate(data, scout, token)

    def test_full91_descriptive_detached_no_claim(self):
        data, scout = fixture(); original = copy.deepcopy((data, scout))
        result = self.validate(data, scout)
        self.assertEqual((data, scout), original)
        self.assertEqual(result['counts']['bound'], 91)
        self.assertEqual(result['counts']['available_snapshots'], 91)
        self.assertEqual(result['unique_bound_tokens'], 91)
        self.assertEqual(len(result['records']), 91)
        self.assertFalse(result['diagnostic_inconclusive'])
        for key in decoder.FALSE_FLAGS | {'native_admitted', 'scene_admitted', 'cycle_timing_proven'}:
            self.assertIs(result[key], False)
        self.assertNotIn('PASS', result['status'])
        for key in ('fps', 'period', 'simulation_steps', 'cycle_row'):
            self.assertNotIn(key, result)
        json.dumps(result, allow_nan=False)
        data['records'][0]['binding']['stock'][1] = 0
        data['records'][0]['binding']['snapshot']['timer_counter'] = 123
        data['statistics']['failed'] = True
        scout['sequence']['slots'][0]['capture']['acquired_sequence'] = 999
        self.assertEqual(result['records'][0]['binding']['stock'][1], decoder.U32)
        self.assertEqual(result['records'][0]['binding']['snapshot']['timer_counter'], 0)
        self.assertFalse(result['statistics']['failed'])

    def test_exact_keys_at_every_level(self):
        paths = [(), ('statistics',), ('records', 90), ('records', 90, 'binding'),
                 ('records', 90, 'binding', 'snapshot')]
        for path in paths:
            for extra in (False, True):
                with self.subTest(path=path, extra=extra):
                    data, scout = fixture(); target = data
                    for key in path: target = target[key]
                    if extra: target['extra'] = 1
                    else: target.pop(next(iter(target)))
                    self.reject(data, scout)

    def test_header_and_token_binding(self):
        for key in ('schema', 'clock', 'token'):
            for bad in ('wrong', None, True, 1):
                data, scout = fixture(); data[key] = bad; self.reject(data, scout)
        for token in ('', TOKEN.upper(), 'f' * 32, '0' * 32, True, None, 1):
            self.reject(*fixture(), token=token)
        for key in ('epoch', 'begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'timer_address'):
            for bad in (True, 1.0, -1, 0, decoder.U64 + 1):
                data, scout = fixture(); data[key] = bad; self.reject(data, scout)
            data, scout = fixture(); data[key] += 1; self.reject(data, scout)

    def test_no_proof_or_quiescent_claim(self):
        for key in decoder.FALSE_FLAGS:
            for bad in (True, 0, 'false', None):
                data, scout = fixture(); data[key] = bad; self.reject(data, scout)

    def test_statistics_bounds_and_nonquiescent_health(self):
        for key in ('issued', 'matched', 'rejected', 'callback_failed'):
            high = decoder.CAPACITY if key in ('issued', 'matched') else decoder.U64
            for bad in (True, 1.0, -1, high + 1):
                data, scout = fixture(); data['statistics'][key] = bad; self.reject(data, scout)
        for bad in (0, 1, 'false', None):
            data, scout = fixture(); data['statistics']['failed'] = bad; self.reject(data, scout)
        data, scout = fixture(); data['statistics'].update(issued=1, matched=2); self.reject(data, scout)
        data, scout = fixture()
        data['statistics'].update(issued=91, matched=91, rejected=decoder.U64,
                                  callback_failed=decoder.U64, failed=True)
        # Counts can include unsampled work; equality is not required.
        result = self.validate(data, scout)
        self.assertTrue(result['diagnostic_inconclusive'])
        self.assertEqual(result['counts']['bound'], 91)
        self.assertIn('statistics_failed', result['diagnostic_issues'])

    def test_post_collection_statistics_lower_bounds(self):
        for issued, matched, callback_failed in ((0, 0, 0), (90, 90, 0), (91, 90, 0), (91, 91, 92)):
            data, scout = fixture()
            data['statistics'].update(issued=issued, matched=matched, callback_failed=callback_failed)
            self.reject(data, scout)
        data, scout = fixture()
        data['statistics'].update(issued=1024, matched=1000)
        self.validate(data, scout)

    def test_unavailable_consume_still_follows_window_start(self):
        data, scout = fixture()
        data['records'][0]['binding'].update(snapshot_result='unavailable', snapshot=None,
                                            consume_qpc=data['begin_qpc'] - 1)
        self.reject(data, scout)

    def test_record_count_type_and_order(self):
        for change in ('short', 'long', 'tuple', 'null', 'duplicate', 'reorder', 'bool'):
            data, scout = fixture(); records = data['records']
            if change == 'short': records.pop()
            elif change == 'long': records.append(copy.deepcopy(records[-1]))
            elif change == 'tuple': data['records'] = tuple(records)
            elif change == 'null': data['records'] = None
            elif change == 'duplicate': records[-1]['slot'] = 89
            elif change == 'reorder': records[0], records[1] = records[1], records[0]
            else: records[0]['slot'] = False
            self.reject(data, scout)

    def test_source_exact_identity(self):
        for key in ('presenter_epoch', 'acquired_sequence'):
            for bad in (True, 1.0, 0, None, decoder.U64 + 1):
                data, scout = fixture(); data['records'][90][key] = bad; self.reject(data, scout)
            data, scout = fixture(); data['records'][90][key] += 1; self.reject(data, scout)

    def test_missing_slots_and_terminal_suffix_are_preserved(self):
        data, scout = fixture(); fail_suffix(scout, 2); clear_missing(data, scout)
        row = scout['sequence']['slots'][1]
        unattempted(row, 'MISSED_DEADLINE', 'SLOT_WINDOW_ELAPSED', row['due_qpc'] + 1000)
        scout['status'] = 'SOURCE_SCOUT_CENSUS_INCOMPLETE_REVIEW_PENDING'
        clear_missing(data, scout)
        result = self.validate(data, scout)
        self.assertEqual(result['counts']['missing'], 90)
        self.assertEqual(result['counts']['bound'], 1)
        self.assertIsNone(result['records'][1]['binding'])
        self.assertIsNone(result['records'][90]['acquired_sequence'])
        for key in ('presenter_epoch', 'acquired_sequence', 'binding'):
            changed = copy.deepcopy(data); changed['records'][90][key] = 1
            self.reject(changed, scout)

    def test_honest_empty_and_unbound(self):
        data, scout = fixture(); fail_suffix(scout, 0); clear_missing(data, scout)
        result = self.validate(data, scout)
        self.assertEqual(result['counts']['missing'], 91)
        self.assertEqual(result['unique_bound_tokens'], 0)
        data, scout = fixture()
        for row in data['records']: row['binding'] = None
        result = self.validate(data, scout)
        self.assertEqual(result['counts']['unbound'], 91)
        self.assertTrue(result['diagnostic_inconclusive'])

    def test_scout_metadata_strictly_rechecked_without_pixels(self):
        for path in (('token',), ('sequence', 'qpc_frequency'),
                     ('sequence', 'slots', 90, 'capture', 'refresh_cpu_begin_qpc')):
            data, scout = fixture(); target = scout
            for key in path[:-1]: target = target[key]
            target[path[-1]] = True
            self.reject(data, scout)

    def test_binding_epoch_serial_and_words(self):
        for key in ('epoch', 'serial'):
            high = decoder.U64 if key == 'epoch' else decoder.CAPACITY
            for bad in (True, 1.0, 0, -1, high + 1):
                data, scout = fixture(); data['records'][90]['binding'][key] = bad; self.reject(data, scout)
        for key, count in (('stock', 4), ('fetch', 6)):
            for bad in (None, (), [0] * (count - 1), [0] * (count + 1), [True] * count,
                        [decoder.U32 + 1] * count, [-1] * count, [0.0] * count):
                data, scout = fixture(); data['records'][90]['binding'][key] = bad; self.reject(data, scout)
        for index in (0, 2, 3):
            data, scout = fixture(); data['records'][90]['binding']['stock'][index] += 1
            self.reject(data, scout)

    def test_available_snapshot_exact_types_bounds_identity(self):
        for key in decoder.SNAPSHOT_KEYS:
            high = decoder.MAX_QPC if key.startswith('qpc_') else decoder.U32
            for bad in (True, 1.0, -1, high + 1, None):
                data, scout = fixture(); data['records'][90]['binding']['snapshot'][key] = bad
                self.reject(data, scout)
        for key in ('qpc_before', 'qpc_after', 'native_thread_id', 'guest_caller_lr', 'timer_address'):
            data, scout = fixture(); data['records'][90]['binding']['snapshot'][key] = 0
            self.reject(data, scout)
        data, scout = fixture(); data['records'][90]['binding']['snapshot']['timer_address'] += 4
        self.reject(data, scout)
        data, scout = fixture(); data['records'][90]['binding']['snapshot'] = None
        self.reject(data, scout)

    def test_available_chronology_and_exclusive_end(self):
        cases = [('qpc_before', lambda d, b: d['begin_qpc'] - 1),
                 ('qpc_after', lambda d, b: b['snapshot']['qpc_before'] - 1),
                 ('qpc_after', lambda d, b: d['end_qpc_exclusive']),
                 ('qpc_after', lambda d, b: b['consume_qpc'] + 1)]
        for key, bad in cases:
            data, scout = fixture(); binding = data['records'][90]['binding']
            binding['snapshot'][key] = bad(data, binding); self.reject(data, scout)
        for bad in (True, 0, decoder.MAX_QPC + 1):
            data, scout = fixture(); data['records'][90]['binding']['consume_qpc'] = bad
            self.reject(data, scout)
        data, scout = fixture(); binding = data['records'][90]['binding']
        binding['consume_qpc'] = scout['sequence']['slots'][90]['capture']['refresh_cpu_begin_qpc'] + 1
        self.reject(data, scout)
        data, scout = fixture(); binding = data['records'][0]['binding']
        binding['snapshot']['qpc_before'] = data['begin_qpc']
        binding['snapshot']['qpc_after'] = binding['consume_qpc']
        self.validate(data, scout)  # Equal ticks are preserved, not interpolated.

    def test_unavailable_status_clears_snapshot_and_still_binds(self):
        for status in decoder.SNAPSHOT_RESULTS - {'available'}:
            data, scout = fixture(); binding = data['records'][0]['binding']
            binding['snapshot_result'] = status
            self.reject(data, scout)
            binding['snapshot'] = None
            result = self.validate(data, scout)
            self.assertEqual(result['counts']['bound'], 91)
            self.assertEqual(result['counts']['unavailable_snapshots'], 1)
            self.assertTrue(result['diagnostic_inconclusive'])
            binding['consume_qpc'] = scout['sequence']['slots'][0]['capture']['refresh_cpu_begin_qpc'] + 1
            self.reject(data, scout)
        for bad in ('unknown', None, True, 1):
            data, scout = fixture(); data['records'][0]['binding']['snapshot_result'] = bad
            self.reject(data, scout)

    def test_duplicate_exact_across_missing_and_null(self):
        ids = (61,) * 3 + tuple(range(62, 150))
        data, scout = fixture(ids)
        row = scout['sequence']['slots'][1]
        unattempted(row, 'MISSED_DEADLINE', 'SLOT_WINDOW_ELAPSED', row['due_qpc'] + 1000)
        scout['status'] = 'SOURCE_SCOUT_CENSUS_INCOMPLETE_REVIEW_PENDING'
        clear_missing(data, scout)
        result = self.validate(data, scout)
        self.assertEqual(result['counts']['duplicate_sources'], 1)
        self.assertEqual(result['counts']['missing'], 1)
        data['records'][0]['binding'] = None; data['records'][2]['binding'] = None
        self.validate(data, scout)
        data['records'][2]['binding'] = fixture(ids)[0]['records'][2]['binding']
        self.reject(data, scout)

    def test_duplicate_changes_every_binding_field_reject(self):
        ids = (61,) * 91
        for key in decoder.BINDING_KEYS:
            data, scout = fixture(ids); binding = data['records'][90]['binding']
            if key in ('epoch', 'serial', 'consume_qpc'): binding[key] += 1
            elif key == 'stock': binding[key][1] ^= 1
            elif key == 'fetch': binding[key][0] ^= 1
            elif key == 'snapshot': binding[key]['timer_counter'] += 1
            else: binding.update(snapshot_result='unavailable', snapshot=None)
            self.reject(data, scout)

    def test_distinct_tokens_no_nearest_time_or_serial_adjacency(self):
        data, scout = fixture(); data['records'][90]['binding']['serial'] = 1
        self.reject(data, scout)
        data, scout = fixture(); data['records'][89]['binding'] = None
        data['records'][90]['binding']['serial'] = 88
        self.reject(data, scout)
        data, scout = fixture()
        for row in data['records']: row['binding']['serial'] *= 10
        data['records'][90]['binding']['serial'] = 1024
        data['statistics']['issued'] = 1024
        result = self.validate(data, scout)
        self.assertEqual(result['records'][90]['binding']['serial'], 1024)
        data, scout = fixture((61,) + tuple(range(1000, 1090)))
        result = self.validate(data, scout)
        self.assertEqual(result['records'][1]['acquired_sequence'], 1000)

    def test_nondivisible_frequency_uses_exact_integer_window(self):
        # Small frequencies still permit positive snapshot witnesses when the
        # fixture's first-before remains within the prior-one-second window.
        data, scout = fixture(frequency=97)
        result = self.validate(data, scout)
        self.assertEqual(result['end_qpc_exclusive'], scout['sequence']['begin_qpc'] + 91 * 97 // 10)


if __name__ == '__main__':
    unittest.main()
