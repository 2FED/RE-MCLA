"""Finite31/91 profile regressions; synthetic bytes only, no native or file IO."""
import copy
import unittest
from unittest.mock import patch

import mcla_game_source_scout_decode as scout
import mcla_game_swap_decode as swap
import mcla_phase_census_decode as phase
import mcla_phase_source_join as join
import mcla_owner_ready_decode as ready
import mcla_source_publication_windows as windows
from test_mcla_game_source_scout_decode import fixture, bmp, names, fail_suffix, TOKEN
from test_mcla_game_swap_decode import fixture as swap_fixture
from test_mcla_phase_census_decode import fixture as phase_fixture, raw, recount
from test_mcla_phase_source_join import fixture as join_fixture
from test_mcla_owner_ready import data as ready_fixture
from test_mcla_source_publication_windows import EXECUTION


def shorten(source):
    source['schema'] = 'mcla-guest-source-scout-short31-v1'
    source['maximum_raw_capture_bytes'] = 114278400
    source['sequence']['scheduled_slots'] = 31
    source['sequence']['slots'] = source['sequence']['slots'][:31]
    return source


def short_join():
    p, s, source = join_fixture()
    shorten(source)
    end = source['sequence']['begin_qpc'] + 31 * source['sequence']['qpc_frequency'] // 10
    p['end_qpc_exclusive'] = s['end_qpc_exclusive'] = end
    p['scopes'][0]['exit_qpc'] = end - 1
    p['swap_callbacks'] = p['swap_callbacks'][:31]
    recount(p)
    s['records'] = s['records'][:31]
    s['statistics'].update(issued=31, matched=31)
    return p, s, source


class ShortProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = bmp()

    def decode(self, source, **kwargs):
        args = dict(token=TOKEN, image_names=names(source), read_image=lambda _: self.image,
                    expected_profile=scout.SHORT_PROFILE)
        args.update(kwargs)
        return scout.decode(source, **args)

    def reject_no_pixels(self, source, **kwargs):
        calls = []
        with self.assertRaises(ValueError):
            self.decode(source, read_image=lambda n: calls.append(n) or self.image, **kwargs)
        self.assertEqual(calls, [])

    def test_full_short_profile_pixels_and_same_output_keys(self):
        source = shorten(fixture())
        result = self.decode(source)
        old = fixture(); fail_suffix(old, 0)
        legacy = scout.decode(old, token=TOKEN, image_names=[], read_image=lambda _: self.image)
        self.assertEqual(set(result), set(legacy))
        self.assertEqual(set(result['census']), set(legacy['census']))
        self.assertEqual(len(result['images']), 31)
        self.assertEqual(result['census']['scheduled_slots'], 31)
        self.assertEqual(result['census']['unique_sources'], 31)
        self.assertEqual(result['census']['schedule_end_qpc'], 531000)
        for key in scout.FALSE_FLAGS:
            self.assertIs(result[key], False)
            self.assertIs(result['census'][key], False)

    def test_default_legacy_admission_and_explicit_cross_profile_rejection(self):
        source = shorten(fixture())
        self.reject_no_pixels(source, expected_profile=scout.LEGACY_PROFILE)
        with self.assertRaises(ValueError): scout.validate_metadata(source, token=TOKEN)
        with self.assertRaises(ValueError):
            scout.decode(source, token=TOKEN, image_names=names(source), read_image=lambda _: self.fail('pixel read'))
        self.reject_no_pixels(fixture())
        self.assertIs(scout.validate_metadata(source, token=TOKEN, expected_profile=scout.SHORT_PROFILE), source)

    def test_resolver_is_detached_and_finite(self):
        source = shorten(fixture(frequency=97))
        result = scout.resolve_source_schedule(source, token=TOKEN)
        self.assertEqual(result['schedule_end_qpc'], source['sequence']['begin_qpc'] + 31 * 97 // 10)
        self.assertEqual(result['journal_begin_qpc'], source['sequence']['begin_qpc'] - 97)
        source['sequence']['slots'][0]['capture']['acquired_sequence'] = 999
        source['sequence']['scheduled_slots'] = 1
        self.assertEqual(result['slots'][0]['capture']['acquired_sequence'], 61)
        self.assertEqual(result['scheduled_slots'], 31)
        for profile in ('', 'short31', 'legacy91_10hz ', 31, True, {}, []):
            self.reject_no_pixels(shorten(fixture()), expected_profile=profile)

    def test_invalid_schema_budget_rate_count_and_extra_field_before_pixels(self):
        for key, value in (('schema', 'mcla-guest-source-scout-v1'),
                           ('schema', 'mcla-guest-source-scout-short32-v1'),
                           ('maximum_raw_capture_bytes', 335462400),
                           ('maximum_raw_capture_bytes', 114278401),
                           ('diagnostic_buffer_limit_bytes', 114278400),
                           ('scout_delay_seconds', 4), ('profile', scout.SHORT_PROFILE)):
            source = shorten(fixture()); source[key] = value
            with self.subTest(key=key, value=value): self.reject_no_pixels(source)
        for key, value in (('scheduled_slots', 91), ('scheduled_slots', 31.0),
                           ('scheduled_slots', True), ('schedule_hz', 30), ('schedule_hz', 10.0)):
            source = shorten(fixture()); source['sequence'][key] = value
            with self.subTest(key=key, value=value): self.reject_no_pixels(source)

    def test_missing_extra_reordered_last_and_inventory_before_pixels(self):
        for change in ('missing', 'extra', 'wrong_index', 'wrong_due', 'malformed_last'):
            source = shorten(fixture()); rows = source['sequence']['slots']
            if change == 'missing': rows.pop()
            elif change == 'extra': rows.append(copy.deepcopy(rows[-1]))
            elif change == 'wrong_index': rows[-1]['slot'] = 31
            elif change == 'wrong_due': rows[-1]['due_qpc'] += 1
            else: rows[-1]['capture']['pixels_ready_qpc'] = 1
            with self.subTest(change=change): self.reject_no_pixels(source)
        source = shorten(fixture())
        for inventory in (names(source)[:-1], names(source) + ['slot-31.bmp'], names(source) + names(source)[-1:]):
            self.reject_no_pixels(source, image_names=inventory)

    def test_incomplete_short_and_callback_cannot_change_schedule(self):
        source = shorten(fixture()); fail_suffix(source, 2)
        def read(_):
            source['sequence']['slots'].clear()
            source['sequence']['scheduled_slots'] = 91
            source['schema'] = 'bad'
            return self.image
        result = self.decode(source, read_image=read)
        self.assertEqual(len(result['images']), 2)
        self.assertEqual(len(result['census']['observations']), 31)
        self.assertEqual(result['census']['status'], scout.INCOMPLETE)

    def test_short_swap_and_phase_windows(self):
        s, source = swap_fixture(); shorten(source)
        end = source['sequence']['begin_qpc'] + 31000
        s['records'] = s['records'][:31]; s['end_qpc_exclusive'] = end
        self.assertEqual(len(swap.validate_swap_receipt(s, source, TOKEN)['records']), 31)
        p, _ = phase_fixture(); p['end_qpc_exclusive'] = end
        self.assertEqual(phase.validate_phase_census(raw(p), source, TOKEN)['end_qpc_exclusive'], end)
        for delta in (-1, 1, 60000):
            s['end_qpc_exclusive'] = p['end_qpc_exclusive'] = end + delta
            with self.assertRaises(ValueError): swap.validate_swap_receipt(s, source, TOKEN)
            with self.assertRaises(ValueError): phase.validate_phase_census(raw(p), source, TOKEN)

    def test_short_v2_join_and_crosspairs(self):
        p, s, source = short_join()
        result = join.validate_phase_source_join(raw(p), raw(s), source, TOKEN)
        self.assertEqual(result['counts']['joined'], 31)
        self.assertEqual(len(result['records']), 31)
        self.assertFalse(result['diagnostic_inconclusive'])
        for flag in join.FALSE_FLAGS: self.assertIs(result[flag], False)
        lp, ls, legacy = join_fixture()
        for pp, ss, src in ((lp, ls, source), (p, s, legacy), (p, ls, source), (lp, s, source)):
            with self.assertRaises(ValueError): join.validate_phase_source_join(raw(pp), raw(ss), src, TOKEN)

    def test_truncated_and_extra_swap_rows_and_json_rejected(self):
        for delta in (-1, 1):
            p, s, source = short_join()
            if delta == -1: s['records'].pop()
            else: s['records'].append(copy.deepcopy(s['records'][-1]))
            with self.assertRaises(ValueError): join.validate_phase_source_join(raw(p), raw(s), source, TOKEN)
        p, s, source = short_join()
        for pr, sr in ((raw(p)[:-1], raw(s)), (raw(p), raw(s)[:-1])):
            with self.assertRaises(ValueError): join.validate_phase_source_join(pr, sr, source, TOKEN)

    def test_owner_ready_exact_short_cut(self):
        r, signal, source, handoff = ready_fixture(); shorten(source)
        end = source['sequence']['begin_qpc'] + 31000
        r['freeze_qpc'] = handoff['freeze_qpc'] = end
        self.assertTrue(ready.validate(raw(r), signal, source, handoff, TOKEN)['owner_ready_assertion_validated'])
        r['freeze_qpc'] = handoff['freeze_qpc'] = end - 1
        with self.assertRaises(ValueError): ready.validate(raw(r), signal, source, handoff, TOKEN)

    def test_publication_short_explicit_profile_default_rejects_before_pixels(self):
        source = shorten(fixture()); calls = []
        args = dict(token=TOKEN, image_names=names(source), read_image=lambda n: calls.append(n) or self.image,
                    execution=EXECUTION)
        with self.assertRaises(ValueError): windows.analyze(source, **args)
        self.assertEqual(calls, [])
        result = windows.analyze(source, **args, expected_profile=scout.SHORT_PROFILE)
        self.assertEqual(result['scheduled_slots'], 31)
        self.assertEqual(result['uncollected_slots'], 0)
        self.assertEqual(len(result['observations']), 31)
        for flag in windows.FALSE_FLAGS: self.assertIs(result[flag], False)

    def test_full_short_source_to_store_chain_unchanged_deeper_joins(self):
        from test_mcla_vector_store_join import fixture as store_fixture, decode as store_decode
        import mcla_vector_store_join as store_join
        values = store_fixture()
        source = shorten(values[-1])
        end = source['sequence']['begin_qpc'] + 31000
        # All explicit metadata windows agree. Keep later completed scope exits
        # and freezes: valid straddles are not silently shortened or omitted.
        for sidecar in values[:-1]:
            if 'end_qpc_exclusive' in sidecar: sidecar['end_qpc_exclusive'] = end
        p = values[-3]; p['swap_callbacks'] = p['swap_callbacks'][:31]; recount(p)
        values[-2]['records'] = values[-2]['records'][:31]
        result = store_decode(values)
        self.assertEqual(len(result['candidate_records']), 31)
        self.assertEqual(result['candidate_blocking_issues'], [])
        self.assertEqual(result['excluded_candidate_records'], [])
        for flag in store_join.FALSE_FLAGS: self.assertIs(result[flag], False)
        values[0]['end_qpc_exclusive'] += 1
        with self.assertRaises(ValueError): store_decode(values)

    def test_consumed_owner8_read_pair_remains_legacy_before_inventory_or_pixels(self):
        import mcla_game_work_owner_v8_probe as owner8
        source = shorten(fixture())
        # Every IO boundary is replaced; the old supervisor itself is unchanged.
        with patch.object(owner8, 'safe_path', side_effect=lambda p: p), \
             patch.object(owner8, 'pin', return_value={'bytes': 1}), \
             patch.object(owner8, 'bounded_read', return_value=raw(source)) as reader, \
             patch.object(owner8, 'read_sidecar', side_effect=AssertionError('sidecar IO')) as sidecar, \
             patch.object(owner8, 'decode', side_effect=AssertionError('pixel decode')) as pixels:
            with self.assertRaisesRegex(ValueError, 'expected source profile'):
                owner8.read_pair(TOKEN)
        self.assertEqual(reader.call_count, 1)
        sidecar.assert_not_called(); pixels.assert_not_called()


if __name__ == '__main__':
    unittest.main()
