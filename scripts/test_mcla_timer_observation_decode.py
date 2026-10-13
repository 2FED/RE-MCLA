"""Pure strict timer receipt tests; no files, runtime, real clocks or game reads."""
import copy
import hashlib
import json
import unittest

import mcla_timer_observation_decode as timer

TOKEN = '0123456789abcdef0123456789abcdef'
BEGIN, FREQUENCY = 1000, 100
END = BEGIN + 91 * FREQUENCY // 10


def state():
    return {'words': [0x7F800000, 0x7FC00042] + [0] * 16,
            'bytes': [0, 128, 255], 'qword64': timer.U64}


def row(ordinal=1, entry=1005, exit_=1006, *, address=0x1000, caller=0x82130004, thread=1):
    return {'entry_ordinal': ordinal, 'callback_issues': 0, 'timer_address': address,
            'caller_address': caller, 'native_thread_id': thread, 'entry_qpc': entry,
            'exit_qpc': exit_, 'incoming_r5_low_byte': 255, 'incoming_f1_bits': timer.U64,
            'before': state(), 'after': state()}


def fixture(records=None):
    if records is None: records = [row()]
    count = len(records)
    value = dict.fromkeys(timer.FALSE_FLAGS, False)
    value.update(schema='mcla-timer-observer-v1', token=TOKEN,
                 status='TIMER_OBSERVATIONS_REVIEW_PENDING', clock='win32-query-performance-counter',
                 thread_identity_domain='win32-native-thread-id-reuse-possible',
                 hook_address=timer.HOOK_ADDRESS, capacity=1024, begin_qpc=BEGIN,
                 end_qpc_exclusive=END, qpc_frequency=FREQUENCY, records=records,
                 counts={'observed_entries': count, 'admitted_entries': count,
                         'stored_entries': count, 'completed_entries': count,
                         'overflow_entries': 0, 'outside_before_entries': 0,
                         'outside_after_entries': 0, 'counter_saturated': False, 'observer_failed': False})
    return value


def encoded(value):
    return json.dumps(value, separators=(',', ':'), allow_nan=False).encode('utf-8')


def decode(value, **kwargs):
    arguments = dict(token=TOKEN, begin_qpc=BEGIN, end_qpc=END, qpc_frequency=FREQUENCY)
    arguments.update(kwargs)
    return timer.decode(encoded(value) if type(value) is dict else value, **arguments)


class TimerDecodeTests(unittest.TestCase):
    def test_healthy_opaque_bits_jsonserializable_no_admission(self):
        value = fixture(); original = copy.deepcopy(value); raw = encoded(value)
        result = decode(raw)
        self.assertEqual(value, original)
        self.assertEqual(result['status'], 'TIMER_OBSERVATIONS_DECODED_REVIEW_PENDING')
        self.assertEqual(result['records'][0]['before'], state())
        self.assertEqual(result['receipt_sha256'], hashlib.sha256(raw).hexdigest().upper())
        self.assertEqual(result['records'][0]['outer_qpc_witness'], [1005, 1006])
        self.assertEqual(result['records'][0]['raw_outer_elapsed_ticks'], 1)
        self.assertEqual(result['raw_callback_issues'], 0)
        json.dumps(result, allow_nan=False)
        for key in timer.FALSE_FLAGS: self.assertIs(result[key], False)
        for key in ('fps', 'rate', 'steps', 'delta_sum', 'simulated_time', 'cycle_row'):
            self.assertNotIn(key, result)

    def test_honest_empty_and_filtered_outside_counts(self):
        value = fixture([])
        value['counts'].update(observed_entries=900, admitted_entries=900, completed_entries=900,
                               outside_before_entries=850, outside_after_entries=50)
        result = decode(value)
        self.assertEqual(result['records'], [])
        self.assertEqual(result['identity_summaries'], [])
        self.assertIn('NO_RETAINED_TIMER_OBSERVATIONS', result['diagnostic_issues'])
        self.assertIn('INCONCLUSIVE', result['status'])

    def test_honest_overflow_and_failed_publication_counts(self):
        value = fixture(); value['counts'].update(observed_entries=3, overflow_entries=2)
        self.assertIn('STORAGE_OR_RESERVATION_OVERFLOW', decode(value)['diagnostic_issues'])
        value = fixture(); value['counts'].update(observed_entries=2, admitted_entries=2, observer_failed=True)
        self.assertIn('OBSERVER_FAILED_OR_INCOMPLETE_PUBLICATION', decode(value)['diagnostic_issues'])
        value['counts']['observer_failed'] = False
        with self.assertRaises(ValueError): decode(value)

    def test_saturated_counts_preserved_inconclusive(self):
        bad = fixture(); bad['counts']['counter_saturated'] = True
        with self.assertRaises(ValueError): decode(bad)
        value = fixture([])
        value['counts'].update(observed_entries=timer.U64, admitted_entries=timer.U64,
                               completed_entries=timer.U64, outside_before_entries=timer.U64,
                               overflow_entries=1, counter_saturated=True)
        self.assertIn('COUNTER_SATURATED', decode(value)['diagnostic_issues'])
        value['counts']['counter_saturated'] = False
        with self.assertRaises(ValueError): decode(value)

    def test_straddles_touch_and_equal_clock_ticks(self):
        value = fixture([row(1, BEGIN - 1, BEGIN), row(2, END - 1, END)])
        result = decode(value)
        self.assertIn('STRADDLES_OR_TOUCHES_WINDOW_BEGIN', result['records'][0]['diagnostic_issues'])
        self.assertIn('STRADDLES_OR_TOUCHES_WINDOW_END', result['records'][1]['diagnostic_issues'])
        self.assertFalse(result['records'][0]['wholly_inside_declared_window'])
        result = decode(fixture([row(entry=1005, exit_=1005)]))
        self.assertTrue(result['records'][0]['clock_witness_valid'])
        self.assertEqual(result['records'][0]['raw_outer_elapsed_ticks'], 0)

    def test_zero_backward_and_unsigned_out_of_qpc_domain_preserved(self):
        for entry, exit_ in ((0, 0), (1010, 1009), (timer.MAX_QPC, timer.MAX_QPC + 1)):
            with self.subTest(entry=entry, exit_=exit_):
                result = decode(fixture([row(entry=entry, exit_=exit_)]))
                record = result['records'][0]
                self.assertEqual(record['outer_qpc_witness'], [entry, exit_])
                self.assertIsNone(record['raw_outer_elapsed_ticks'])
                self.assertIn('INVALID_OUTER_CLOCK_WITNESS', result['diagnostic_issues'])

    def test_missing_state_and_identity_remain_diagnostics(self):
        for key in ('timer_address', 'caller_address', 'native_thread_id'):
            value = fixture(); value['records'][0][key] = 0
            self.assertIn('MISSING_OBSERVED_IDENTITY', decode(value)['diagnostic_issues'])
        value = fixture(); value['records'][0].update(before=None, after=None)
        result = decode(value)
        self.assertIsNone(result['records'][0]['before'])
        self.assertIn('MISSING_OPAQUE_STATE', result['diagnostic_issues'])

    def test_callback_masks_and_original_unwind_never_hidden(self):
        value = fixture(); value['records'][0]['callback_issues'] = 16
        result = decode(value)
        self.assertEqual(result['raw_callback_issues'], 16)
        self.assertIn('CALLBACK_FAILURE_OR_ORIGINAL_UNWIND', result['diagnostic_issues'])
        value['records'][0].update(callback_issues=31, entry_qpc=0, exit_qpc=0, before=None, after=None)
        self.assertEqual(decode(value)['raw_callback_issues'], 31)
        for mask, key, bad in ((1, 'entry_qpc', 1005), (2, 'exit_qpc', 1006),
                              (4, 'before', state()), (8, 'after', state())):
            value = fixture(); value['records'][0].update(callback_issues=mask)
            value['records'][0][key] = bad
            with self.assertRaises(ValueError): decode(value)

    def test_completion_order_preserved_ordinals_not_clocks_sorted(self):
        value = fixture([row(2, 1020, 1021), row(1, 1010, 1011)])
        result = decode(value)
        self.assertEqual([r['entry_ordinal'] for r in result['records']], [2, 1])
        self.assertEqual(result['admission_order_record_indices'], [1, 0])
        self.assertEqual(result['diagnostic_issues'], [])
        value = fixture([row(1, 1020, 1030), row(2, 1010, 1025)])
        result = decode(value)
        self.assertIn('ENTRY_CLOCK_ORDER_REGRESSED', result['diagnostic_issues'])
        self.assertIn('EXIT_CLOCK_ORDER_REGRESSED', result['diagnostic_issues'])
        self.assertEqual(result['records'][1]['outer_qpc_witness'], [1010, 1025])

    def test_overlap_negative_bounds_peridentity_not_global_sum(self):
        value = fixture([row(1, 1010, 1030), row(2, 1020, 1040),
                         row(3, 1040, 1050, address=0x2000)])
        result = decode(value)
        self.assertEqual(len(result['identity_summaries']), 2)
        first = result['identity_summaries'][0]
        self.assertEqual(first['record_indices'], [0, 1])
        self.assertEqual(first['adjacent_outer_witness_differences'][0]['raw_interval_difference_ticks_bounds'], [-10, 30])
        self.assertEqual(result['overlap_or_touch_pairs'], 2)
        self.assertIn('SHARED_ADDRESS_WITNESSES_OVERLAP_OR_TOUCH', result['diagnostic_issues'])
        value = fixture([row(1, 1010, 1011), row(2, 1020, 1021, caller=0x82130008),
                         row(3, 1030, 1031, thread=2)])
        self.assertEqual(len(decode(value)['identity_summaries']), 3)

    def test_max1024_bounded_dense_overlap_summary(self):
        value = fixture([row(i + 1, 1010, 1020) for i in range(1024)])
        result = decode(value)
        self.assertEqual(len(result['records']), 1024)
        self.assertEqual(result['overlap_or_touch_pairs'], 1024 * 1023 // 2)
        self.assertEqual(len(result['identity_summaries'][0]['adjacent_outer_witness_differences']), 1023)
        self.assertLess(len(encoded(result)), 2 * 1024 * 1024)
        with self.assertRaises(ValueError): decode(fixture([row(i + 1) for i in range(1025)]))

    def test_exact_schemas_all_levels(self):
        for location in ('root', 'counts', 'record', 'state'):
            for change in ('extra', 'missing'):
                value = fixture()
                target = {'root': value, 'counts': value['counts'], 'record': value['records'][0],
                          'state': value['records'][0]['before']}[location]
                if change == 'extra': target['extra'] = 0
                else: target.pop(next(iter(target)))
                with self.subTest(location=location, change=change), self.assertRaises(ValueError): decode(value)

    def test_bool_float_negative_and_overflow_integer_rejections(self):
        for key, limit in (('entry_ordinal', 1), ('callback_issues', 31), ('timer_address', timer.U32),
                           ('caller_address', timer.U32), ('native_thread_id', timer.U32),
                           ('entry_qpc', timer.U64), ('exit_qpc', timer.U64),
                           ('incoming_r5_low_byte', 255), ('incoming_f1_bits', timer.U64)):
            for bad in (True, 1.0, -1, str(1), limit + 1):
                value = fixture(); value['records'][0][key] = bad
                with self.subTest(key=key, bad=bad), self.assertRaises(ValueError): decode(value)
        for field in ('words', 'bytes'):
            limit = timer.U32 if field == 'words' else 255
            for bad in (True, -1, 1.0, limit + 1, timer.U64 + 1):
                value = fixture(); value['records'][0]['before'][field][0] = bad
                with self.assertRaises(ValueError): decode(value)
        for bad in (True, -1, 1.0, timer.U64 + 1):
            value = fixture(); value['records'][0]['before']['qword64'] = bad
            with self.assertRaises(ValueError): decode(value)
        for field in timer.COUNT_FIELDS:
            for bad in (True, -1, 1.0, timer.U64 + 1):
                value = fixture(); value['counts'][field] = bad
                with self.assertRaises(ValueError): decode(value)
        for field in ('words', 'bytes'):
            for bad in ([], [0], None, 'opaque'):
                value = fixture(); value['records'][0]['before'][field] = bad
                with self.assertRaises(ValueError): decode(value)

    def test_claim_flags_and_exact_count_booleans(self):
        for key in timer.FALSE_FLAGS:
            for bad in (True, 0, None):
                value = fixture(); value[key] = bad
                with self.assertRaises(ValueError): decode(value)
        for key in ('counter_saturated', 'observer_failed'):
            for bad in (0, 1, None, 'false'):
                value = fixture(); value['counts'][key] = bad
                with self.assertRaises(ValueError): decode(value)

    def test_counts_ordinals_and_impossible_outside_record(self):
        for key in timer.COUNT_FIELDS:
            value = fixture(); value['counts'][key] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): decode(value)
        for records in ([row(0)], [row(2)], [row(1), row(1)]):
            with self.assertRaises(ValueError): decode(fixture(records))
        with self.assertRaises(ValueError): decode(fixture([row(entry=900, exit_=901)]))
        value = fixture([row(entry=900, exit_=901)])
        value['records'][0]['callback_issues'] = 16
        self.assertIn('RETAINED_DIAGNOSTIC_OUTSIDE_WINDOW', decode(value)['diagnostic_issues'])

    def test_duplicatekeys_constants_utf8_size_and_nonbytes(self):
        raw = encoded(fixture())
        duplicates = [raw.replace(b'"schema":', b'"schema":"ignored","schema":', 1),
                      raw.replace(b'"stored_entries":', b'"stored_entries":1,"stored_entries":', 1),
                      raw.replace(b'"qword64":', b'"qword64":0,"qword64":', 1)]
        for bad in duplicates + [b'NaN', b'Infinity', b'-Infinity', b'\xff', b'', b'[' * 2000,
                                  raw + b'X', b' ' * (timer.MAX_BYTES + 1)]:
            with self.assertRaises(ValueError): decode(bad)
        for bad in (raw.decode(), bytearray(raw), None):
            with self.assertRaises(ValueError): decode(bad)

    def test_expected_window_token_and_native_header_bindings(self):
        for key, bad in (('token', 'F' * 32), ('token', 'f' * 31), ('token', 'f' * 32),
                         ('begin_qpc', True), ('end_qpc', END + 1), ('qpc_frequency', FREQUENCY + 1)):
            with self.subTest(key=key, bad=bad), self.assertRaises(ValueError): decode(fixture(), **{key: bad})
        for key in ('begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'capacity', 'hook_address'):
            value = fixture(); value[key] = True
            with self.assertRaises(ValueError): decode(value)
        for key in ('schema', 'clock', 'thread_identity_domain', 'status'):
            value = fixture(); value[key] = 'wrong'
            with self.assertRaises(ValueError): decode(value)


if __name__ == '__main__':
    unittest.main()
