"""Pure91-slot wire/BMP regressions: no files, processes, capture or admission."""
import copy
import struct
import tracemalloc
import unittest

import mcla_game_source_scout_decode as decoder

TOKEN = '0123456789abcdef0123456789abcdef'
ANCHOR, FREQUENCY = 100000, 10000
BEGIN = ANCHOR + 40 * FREQUENCY


def fixture(ids=None, *, frequency=FREQUENCY, anchor=ANCHOR):
    if ids is None:
        ids = tuple(range(61, 152))
    begin = anchor + 40 * frequency
    slots, publications = [], {}
    for index, sequence in enumerate(ids):
        due = begin + index * frequency // 10
        publications.setdefault(sequence, (due - 5, due - 4))
        pub_begin, pub_end = publications[sequence]
        slots.append({'slot': index, 'due_qpc': due, 'observed_qpc': due + 7,
                      'status': 'COLLECTED', 'reason': None,
                      'call_before_qpc': due + 1, 'call_after_qpc': due + 7,
                      'capture': {'presenter_epoch': 7, 'acquired_sequence': sequence,
                                  'qpc_frequency': frequency, 'width': 1280, 'height': 720, 'stride': 5120,
                                  'refresh_cpu_begin_qpc': pub_begin, 'refresh_cpu_end_qpc': pub_end,
                                  'acquire_before_qpc': due + 2, 'acquire_after_qpc': due + 3,
                                  'copy_submit_before_qpc': due + 4,
                                  'copy_completion_observed_qpc': due + 5, 'pixels_ready_qpc': due + 6},
                      'image_file': f'slot-{index:02d}.bmp', 'image_state': 'complete',
                      'image_bytes_written': decoder.BMP_BYTES})
    result = dict.fromkeys(decoder.FALSE_FLAGS, False)
    result.update(schema='mcla-guest-source-scout-v1', token=TOKEN, status=decoder.COMPLETE,
                  reason=decoder.REASON, readiness_sequence_not_pixel_identity=60, readiness_wait_ms=123,
                  readiness_observed_qpc=anchor, scout_delay_seconds=40,
                  diagnostic_buffer_limit_bytes=536870912, maximum_raw_capture_bytes=335462400,
                  clock='win32-query-performance-counter', pixel_format='R8G8B8X8',
                  file_format='24bit-BGR-bottom-up-BMP', interpretation=decoder.INTERPRETATION,
                  sequence={'begin_qpc': begin, 'qpc_frequency': frequency, 'scheduled_slots': 91,
                            'schedule_hz': 10, 'width': 1280, 'height': 720, 'slots': slots})
    return result


def bmp():
    data = bytearray(decoder.BMP_BYTES)
    data[:2] = b'BM'
    struct.pack_into('<IHHI', data, 2, len(data), 0, 0, 54)
    struct.pack_into('<IiiHHIIiiII', data, 14, 40, 1280, 720, 1, 24, 0, 1280 * 720 * 3, 0, 0, 0, 0)
    data[54:54 + decoder.BMP_STRIDE] = bytes((43, 41, 37)) * 1280
    data[-decoder.BMP_STRIDE:] = bytes((31, 23, 17)) * 1280
    return bytes(data)


def names(receipt):
    return [row['image_file'] for row in receipt['sequence']['slots'] if row['status'] == 'COLLECTED']


def unattempted(row, status, reason, observed):
    row.update(status=status, reason=reason, observed_qpc=observed, call_before_qpc=None,
               call_after_qpc=None, capture=None, image_state='absent', image_bytes_written=0)


def fail_suffix(receipt, index):
    rows = receipt['sequence']['slots']
    observed = rows[index]['observed_qpc']
    rows[index].update(status='FAILED', reason='API_FAILED', capture=None, image_state='absent', image_bytes_written=0)
    for row in rows[index + 1:]:
        unattempted(row, 'NOT_ATTEMPTED', 'PREVIOUS_TERMINAL_FAILURE', observed)
    receipt['status'] = decoder.INCOMPLETE


class ScoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = bmp()

    def assert_no_read(self, receipt, *, image_names=None, token=TOKEN):
        calls = []
        if image_names is None:
            image_names = [f'slot-{i:02d}.bmp' for i in range(91)]
        with self.assertRaises(ValueError):
            decoder.decode(receipt, token=token, image_names=image_names,
                           read_image=lambda name: calls.append(name) or self.image)
        self.assertEqual(calls, [])

    def test_full91_same_pixels_different_ids_keeps_all(self):
        receipt = fixture(); original = copy.deepcopy(receipt); calls = []
        value = decoder.decode(receipt, token=TOKEN, image_names=names(receipt),
                               read_image=lambda name: calls.append(name) or self.image)
        self.assertEqual(receipt, original)
        self.assertEqual(calls, names(receipt))
        self.assertEqual(value['census']['unique_sources'], 91)
        self.assertEqual(value['census']['attempted_calls'], 91)
        self.assertEqual(value['census']['duplicate_sources'], 0)
        self.assertEqual(len({r['rgb_sha256'] for r in value['census']['observations']}), 1)
        self.assertEqual(len(value['census']['external_call_costs']), 91)
        self.assertEqual(len(value['census']['source_witness_spans']), 91)
        for target in (value, value['census']):
            for key in decoder.FALSE_FLAGS:
                self.assertIs(target[key], False)
            self.assertNotIn('cycle_row', target)
            self.assertNotIn('period', target)

    def test_metadata_only_and_default_not_admitted(self):
        value = fixture()
        self.assertIs(decoder.validate_metadata(value, token=TOKEN), value)
        self.assertFalse(decoder.NATIVE_ADMITTED)

    def test_canonical_rgb_bottom_up(self):
        rgb = decoder._bmp_rgb(self.image)
        self.assertIs(type(rgb), bytes)
        self.assertEqual(len(rgb), 1280 * 720 * 3)
        self.assertEqual(rgb[:3], bytes((17, 23, 31)))
        self.assertEqual(rgb[-3:], bytes((37, 41, 43)))
        self.assertEqual(rgb[3840:3843], bytes(3))

    def test_duplicates_retained_streaming_memory_bounded(self):
        receipt = fixture((61,) * 91)
        # Preallocated image is excluded; decoder peak must be independent of91
        # source-image retention. A full-image list would exceed250MiB here.
        tracemalloc.start()
        try:
            value = decoder.decode(receipt, token=TOKEN, image_names=names(receipt), read_image=lambda _: self.image)
            _, peak = tracemalloc.get_traced_memory()
        finally:
            tracemalloc.stop()
        self.assertLess(peak, 24 * 1024 * 1024)
        self.assertEqual(value['census']['duplicate_sources'], 90)
        self.assertEqual(value['census']['unique_sources'], 1)
        self.assertEqual(len(value['images']), 91)
        def no_pixels(obj):
            self.assertNotIsInstance(obj, (bytes, bytearray, memoryview))
            if isinstance(obj, dict):
                for val in obj.values(): no_pixels(val)
            elif isinstance(obj, (tuple, list)):
                for val in obj: no_pixels(val)
        no_pixels(value)

    def test_duplicate_rgb_failure_no_retry(self):
        receipt = fixture((61,) * 91)
        changed = bytearray(self.image); changed[54] ^= 1
        calls = []
        def read(name):
            calls.append(name)
            return bytes(changed) if name == 'slot-01.bmp' else self.image
        with self.assertRaisesRegex(ValueError, 'changed RGB'):
            decoder.decode(receipt, token=TOKEN, image_names=names(receipt), read_image=read)
        self.assertEqual(calls, ['slot-00.bmp', 'slot-01.bmp'])

    def test_duplicate_binding_survives_missed_slot(self):
        receipt = fixture((61,) * 91); fail_suffix(receipt, 3)
        row = receipt['sequence']['slots'][1]
        unattempted(row, 'MISSED_DEADLINE', 'SLOT_WINDOW_ELAPSED', row['due_qpc'] + 1000)
        value = decoder.decode(receipt, token=TOKEN, image_names=names(receipt), read_image=lambda _: self.image)
        self.assertEqual(value['census']['duplicate_sources'], 1)
        self.assertEqual(value['census']['counts']['MISSED_DEADLINE'], 1)
        self.assertEqual([v['slot'] for v in value['images']], [0, 2])
        changed = bytearray(self.image); changed[54] ^= 1
        with self.assertRaisesRegex(ValueError, 'changed RGB'):
            decoder.decode(receipt, token=TOKEN, image_names=names(receipt),
                           read_image=lambda name: self.image if name == 'slot-00.bmp' else bytes(changed))

    def test_failure_suffix_and_stop_during_delay(self):
        receipt = fixture(); fail_suffix(receipt, 2)
        value = decoder.decode(receipt, token=TOKEN, image_names=names(receipt), read_image=lambda _: self.image)
        self.assertEqual(value['census']['counts'], {'COLLECTED': 2, 'FAILED': 1, 'MISSED_DEADLINE': 0, 'NOT_ATTEMPTED': 88})
        self.assertFalse(value['census']['all_slots_collected'])
        receipt = fixture(); receipt['status'] = decoder.INCOMPLETE
        for i, row in enumerate(receipt['sequence']['slots']):
            unattempted(row, 'NOT_ATTEMPTED', 'STOPPED' if i == 0 else 'PREVIOUS_STOP', ANCHOR + 1)
        value = decoder.decode(receipt, token=TOKEN, image_names=[], read_image=lambda _: self.fail('unexpected read'))
        self.assertEqual(value['census']['attempted_calls'], 0)
        receipt['sequence']['slots'][0]['observed_qpc'] = ANCHOR - 1
        self.assert_no_read(receipt, image_names=[])

    def test_all_missed_no_image_reads(self):
        receipt = fixture(); receipt['status'] = decoder.INCOMPLETE
        for row in receipt['sequence']['slots']:
            unattempted(row, 'MISSED_DEADLINE', 'SLOT_WINDOW_ELAPSED', row['due_qpc'] + 1000)
        value = decoder.decode(receipt, token=TOKEN, image_names=[], read_image=lambda _: self.fail('unexpected read'))
        self.assertEqual(value['census']['counts']['MISSED_DEADLINE'], 91)

    def test_budget_guard_is_uncalled_terminal_in_open_slot(self):
        receipt = fixture(); receipt['status'] = decoder.INCOMPLETE
        rows = receipt['sequence']['slots']
        observed = rows[0]['due_qpc'] + 1
        for i, row in enumerate(rows):
            unattempted(row, 'NOT_ATTEMPTED',
                        'BUFFER_BUDGET_EXHAUSTED' if i == 0 else 'PREVIOUS_TERMINAL_FAILURE', observed)
        value = decoder.decode(receipt, token=TOKEN, image_names=[], read_image=lambda _: self.fail('unexpected read'))
        self.assertEqual(value['census']['attempted_calls'], 0)
        self.assertEqual(value['census']['counts']['NOT_ATTEMPTED'], 91)
        self.assertTrue(all(item is None for item in value['census']['external_call_costs']))
        for tick in (BEGIN - 1, BEGIN + 1000):
            changed = copy.deepcopy(receipt)
            for row in changed['sequence']['slots']: row['observed_qpc'] = tick
            self.assert_no_read(changed, image_names=[])
        for key in ('call_before_qpc', 'call_after_qpc'):
            changed = copy.deepcopy(receipt); changed['sequence']['slots'][0][key] = observed
            self.assert_no_read(changed, image_names=[])
        changed = copy.deepcopy(receipt)
        changed['sequence']['slots'][0]['capture'] = fixture()['sequence']['slots'][0]['capture']
        self.assert_no_read(changed, image_names=[])
        changed = copy.deepcopy(receipt)
        changed['sequence']['slots'][1] = fixture()['sequence']['slots'][1]
        self.assert_no_read(changed, image_names=['slot-01.bmp'])
        changed = copy.deepcopy(receipt); changed['sequence']['slots'][1]['reason'] = 'PREVIOUS_STOP'
        self.assert_no_read(changed, image_names=[])
        changed = copy.deepcopy(receipt); changed['sequence']['slots'][0]['image_state'] = 'complete'
        self.assert_no_read(changed, image_names=[])

    def test_final_slot_all_metadata_before_any_read(self):
        for key in decoder.CAPTURE_KEYS:
            receipt = fixture(); receipt['sequence']['slots'][90]['capture'][key] = True
            self.assert_no_read(receipt)
        for key in ('slot', 'due_qpc', 'observed_qpc', 'call_before_qpc', 'call_after_qpc', 'image_bytes_written'):
            receipt = fixture(); receipt['sequence']['slots'][90][key] = True
            self.assert_no_read(receipt)
        for key, value in (('image_state', 'partial'), ('image_state', 'absent'), ('image_bytes_written', 54),
                           ('status', 'PASS'), ('reason', 'OK')):
            receipt = fixture(); receipt['sequence']['slots'][90][key] = value
            self.assert_no_read(receipt)

    def test_top_schema_claims_and_integers(self):
        for key in decoder.FALSE_FLAGS:
            for bad in (True, 0, 'false'):
                receipt = fixture(); receipt[key] = bad; self.assert_no_read(receipt)
        for key in ('schema', 'reason', 'clock', 'pixel_format', 'file_format', 'interpretation', 'token', 'status'):
            receipt = fixture(); receipt[key] = 'wrong'; self.assert_no_read(receipt)
        for key in ('readiness_sequence_not_pixel_identity', 'readiness_wait_ms', 'readiness_observed_qpc',
                    'scout_delay_seconds', 'diagnostic_buffer_limit_bytes', 'maximum_raw_capture_bytes'):
            receipt = fixture(); receipt[key] = True; self.assert_no_read(receipt)
        for key in ('begin_qpc', 'qpc_frequency', 'scheduled_slots', 'schedule_hz', 'width', 'height'):
            receipt = fixture(); receipt['sequence'][key] = True; self.assert_no_read(receipt)
        for token in ('', TOKEN.upper(), 'f' * 32, True):
            self.assert_no_read(fixture(), token=token)

    def test_exact_schemas(self):
        for path in ((), ('sequence',), ('sequence', 'slots', 90), ('sequence', 'slots', 90, 'capture')):
            for extra in (False, True):
                receipt = fixture(); target = receipt
                for key in path: target = target[key]
                if extra: target['extra'] = 1
                else: target.pop(next(iter(target)))
                self.assert_no_read(receipt)

    def test_fixed91_delay_and_resource_limits(self):
        for key, bad in (('readiness_sequence_not_pixel_identity', 59), ('readiness_wait_ms', 90000),
                         ('readiness_observed_qpc', decoder.MAX_QPC), ('scout_delay_seconds', 39),
                         ('diagnostic_buffer_limit_bytes', decoder.BUFFER_LIMIT + 1),
                         ('maximum_raw_capture_bytes', decoder.MAX_RAW_BYTES - 1)):
            receipt = fixture(); receipt[key] = bad; self.assert_no_read(receipt)
        for key, bad in (('qpc_frequency', 9), ('qpc_frequency', 10**10+1), ('begin_qpc', BEGIN+1),
                         ('width', 3840), ('scheduled_slots', 13), ('schedule_hz', 60)):
            receipt = fixture(); receipt['sequence'][key] = bad; self.assert_no_read(receipt)
        for count in (90, 92):
            self.assert_no_read(fixture(tuple(range(61, 61+count))))
        self.assert_no_read(fixture(anchor=decoder.MAX_QPC - 40*FREQUENCY))
        self.assertEqual(decoder.MAX_RAW_BYTES, 91 * 1280 * 720 * 4)
        # Non-divisible frequencies use integer floor schedules, not float drift.
        decoder.validate_metadata(fixture(frequency=97), token=TOKEN)

    def test_epoch_sequence_publication_and_call_chronology(self):
        for key, bad in (('presenter_epoch', 8), ('acquired_sequence', 59), ('qpc_frequency', 9999),
                         ('refresh_cpu_begin_qpc', BEGIN-5), ('refresh_cpu_end_qpc', BEGIN+999999),
                         ('acquire_before_qpc', BEGIN), ('acquire_after_qpc', BEGIN),
                         ('copy_submit_before_qpc', BEGIN), ('copy_completion_observed_qpc', BEGIN),
                         ('pixels_ready_qpc', BEGIN+999999), ('width', 1920), ('stride', 5121)):
            receipt = fixture(); receipt['sequence']['slots'][90]['capture'][key] = bad
            self.assert_no_read(receipt)
        for key in ('refresh_cpu_begin_qpc', 'refresh_cpu_end_qpc'):
            receipt = fixture((61,) * 91); receipt['sequence']['slots'][90]['capture'][key] += 1
            self.assert_no_read(receipt)
        receipt = fixture(); fail_suffix(receipt, 1)
        receipt['sequence']['slots'][2] = fixture()['sequence']['slots'][2]
        self.assert_no_read(receipt)
        receipt = fixture(); receipt['status'] = decoder.INCOMPLETE; self.assert_no_read(receipt)

    def test_names_and_inventory_exact_before_reads(self):
        for bad in ('../slot-90.bmp', 'slot-89.bmp', 'C:\\frame.bmp', 'slot-90.bmp:ads', 'SLOT-90.BMP', 'slot-90.bmp\0'):
            receipt = fixture(); receipt['sequence']['slots'][90]['image_file'] = bad
            self.assert_no_read(receipt)
        wanted = names(fixture())
        for actual in (wanted[:-1], wanted+['other.bmp'], wanted+['result.json'], wanted[:-1]+[wanted[0]],
                       [1]+wanted[1:], set(wanted), (), None):
            self.assert_no_read(fixture(), image_names=actual if actual is not None else ())
        receipt = fixture(); fail_suffix(receipt, 0)
        self.assert_no_read(receipt, image_names=['slot-00.bmp'])

    def test_bad_bmp_stops_once_without_replacement(self):
        cases = [b'', self.image[:-1], self.image+b'\0', bytearray(self.image), None]
        for offset, fmt, value in ((0, '<H', 0), (2, '<I', 1), (6, '<H', 1), (8, '<H', 1),
                                   (10, '<I', 55), (14, '<I', 108), (18, '<i', 1279), (22, '<i', -720),
                                   (26, '<H', 2), (28, '<H', 32), (30, '<I', 3), (34, '<I', 0),
                                   (38, '<i', 1), (42, '<i', 1), (46, '<I', 1), (50, '<I', 1)):
            changed = bytearray(self.image); struct.pack_into(fmt, changed, offset, value); cases.append(bytes(changed))
        receipt = fixture(); fail_suffix(receipt, 1)
        for data in cases:
            calls = []
            with self.assertRaises(ValueError):
                decoder.decode(receipt, token=TOKEN, image_names=names(receipt), read_image=lambda name: calls.append(name) or data)
            self.assertEqual(calls, ['slot-00.bmp'])

    def test_callback_failure_has_no_retry(self):
        calls = []
        def fail(name):
            calls.append(name); raise OSError('synthetic read failure')
        with self.assertRaises(OSError):
            decoder.decode(fixture(), token=TOKEN, image_names=names(fixture()), read_image=fail)
        self.assertEqual(calls, ['slot-00.bmp'])

    def test_callback_cannot_mutate_prevalidated_future_metadata(self):
        receipt = fixture(); fail_suffix(receipt, 2); image_names = names(receipt); calls = []
        def read(name):
            calls.append(name)
            receipt['sequence']['slots'][1]['capture']['acquired_sequence'] = 999
            receipt['sequence']['slots'][1]['image_file'] = '../evil.bmp'
            receipt['sequence']['begin_qpc'] = 1
            receipt['readiness_observed_qpc'] = 1
            receipt['status'] = 'FAILED'
            image_names.clear()
            return self.image
        value = decoder.decode(receipt, token=TOKEN, image_names=image_names, read_image=read)
        self.assertEqual(calls, ['slot-00.bmp', 'slot-01.bmp'])
        self.assertEqual(value['census']['begin_qpc'], BEGIN)
        self.assertEqual(value['census']['readiness_observed_qpc'], ANCHOR)
        self.assertEqual(value['census']['observations'][1]['source_identity'], (7, 62))


if __name__ == '__main__':
    unittest.main()
