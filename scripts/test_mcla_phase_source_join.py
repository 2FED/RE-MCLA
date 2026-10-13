"""Pure synthetic v2 ID joins: no files, pixels, processes or game admission."""
import copy
import hashlib
import json
import unittest
from unittest import mock

import mcla_phase_source_join as decoder
from test_mcla_game_swap_decode import fixture as swap_fixture, clear_missing
from test_mcla_phase_census_decode import fixture as phase_fixture, scope, callback, recount, raw
from test_mcla_game_source_scout_decode import TOKEN, fail_suffix


def fixture(ids=None):
    swap, source = swap_fixture(ids)
    phase, _ = phase_fixture()
    phase['schema'] = 'mcla-phase-census-v2'
    swap['schema'] = 'mcla-game-swap-observer-v2'
    phase['scopes'] = [scope(1, 'render', phase['begin_qpc'],
                             phase['end_qpc_exclusive'] - 1, tid=28500)]
    phase['swap_callbacks'] = []
    seen = set()
    for row in swap['records']:
        binding = row['binding']; snapshot = binding['snapshot']
        serial = binding['serial']; snapshot['callback_id'] = serial
        if serial not in seen:
            item = callback(snapshot['qpc_before'], 1, tid=snapshot['native_thread_id'])
            item.update(callback_id=serial, caller_lr=snapshot['guest_caller_lr'])
            phase['swap_callbacks'].append(item)
            seen.add(serial)
    recount(phase)
    return phase, swap, source


class JoinTests(unittest.TestCase):
    def decode(self, phase, swap, source, token=TOKEN):
        return decoder.validate_phase_source_join(raw(phase), raw(swap), source, token)

    def reject(self, phase, swap, source):
        with self.assertRaises(ValueError): self.decode(phase, swap, source)

    def test_full91_no_io_detached_and_all_claims_false(self):
        phase, swap, source = fixture(); original = copy.deepcopy((phase, swap, source))
        with mock.patch('builtins.open', side_effect=AssertionError('IO forbidden')):
            result = self.decode(phase, swap, source)
        self.assertEqual((phase, swap, source), original)
        self.assertEqual(result['counts']['joined'], 91)
        self.assertEqual(result['counts']['scoped'], 91)
        self.assertEqual(result['unique_joined_callbacks'], 91)
        self.assertEqual(result['unique_sources'], 91)
        self.assertEqual(result['unmatched_callback_ids'], [])
        self.assertFalse(result['diagnostic_inconclusive'])
        self.assertEqual(result['phase_raw_sha256'], hashlib.sha256(raw(phase)).hexdigest())
        self.assertEqual(result['swap_raw_sha256'], hashlib.sha256(raw(swap)).hexdigest())
        for flag in decoder.FALSE_FLAGS: self.assertIs(result[flag], False)
        self.assertTrue(result['physical_acceptance_pending'])
        self.assertTrue(result['unobserved_ancestry_possible'])
        self.assertNotIn('PASS', result['status'])
        phase['scopes'][0]['kind'] = 'update'
        phase['swap_callbacks'][0]['incoming_r3'] = 123
        swap['statistics']['failed'] = True
        source['sequence']['slots'][0]['capture']['acquired_sequence'] = 999
        self.assertEqual(result['records'][0]['scope_role'], 'render')
        self.assertEqual(result['records'][0]['callback']['incoming_r3'], 0)
        self.assertFalse(result['swap_statistics']['failed'])
        result['records'][0]['scope']['kind'] = 'changed'
        self.assertEqual(result['records'][1]['scope']['kind'], 'render')
        json.dumps(result, allow_nan=False)

    def test_reversed_ids_and_journal_order_are_legal(self):
        phase, swap, source = fixture()
        for row in phase['swap_callbacks']: row['callback_id'] = 92 - row['callback_id']
        phase['swap_callbacks'].reverse()
        for row in swap['records']:
            snap = row['binding']['snapshot']; snap['callback_id'] = 92 - snap['callback_id']
        result = self.decode(phase, swap, source)
        self.assertEqual(result['records'][0]['callback_id'], 91)
        self.assertEqual(result['counts']['joined'], 91)

    def test_missing_or_shifted_callback_id_set_rejected(self):
        for shift in (1, 100):
            phase, swap, source = fixture()
            for row in phase['swap_callbacks']: row['callback_id'] += shift
            for row in swap['records']: row['binding']['snapshot']['callback_id'] += shift
            with self.assertRaisesRegex(ValueError, 'consecutive'):
                self.decode(phase, swap, source)

    def test_duplicate_source_same_id_allowed_changed_id_rejected(self):
        phase, swap, source = fixture([61] * 91)
        result = self.decode(phase, swap, source)
        self.assertEqual(result['counts']['joined'], 91)
        self.assertEqual(result['unique_joined_callbacks'], 1)
        self.assertEqual(result['unique_sources'], 1)
        swap['records'][-1]['binding']['snapshot']['callback_id'] = 0
        self.reject(phase, swap, source)

    def test_same_id_distinct_tokens_rejected(self):
        phase, swap, source = fixture()
        swap['records'][1]['binding']['snapshot']['callback_id'] = 1
        with self.assertRaisesRegex(ValueError, 'distinct swap token'):
            self.decode(phase, swap, source)

    def test_positive_unknown_id_not_hole(self):
        phase, swap, source = fixture()
        swap['records'][-1]['binding']['snapshot']['callback_id'] = 1024
        with self.assertRaisesRegex(ValueError, 'unknown positive'):
            self.decode(phase, swap, source)

    def test_all_phase_ids_unique_even_unmatched(self):
        phase, swap, source = fixture()
        phase['swap_callbacks'].append(copy.deepcopy(phase['swap_callbacks'][0])); recount(phase)
        self.reject(phase, swap, source)

    def test_id_bounds_and_types(self):
        for target, values in (('phase', (0, -1, 1025, True, False, None, '1', 1.0)),
                               ('swap', (-1, 1025, True, False, None, '1', 1.0))):
            for bad in values:
                with self.subTest(target=target, value=bad):
                    phase, swap, source = fixture()
                    row = phase['swap_callbacks'][-1] if target == 'phase' else swap['records'][-1]['binding']['snapshot']
                    row['callback_id'] = bad; self.reject(phase, swap, source)

    def test_zero_id_unavailable_unbound_and_missing_are_distinct(self):
        phase, swap, source = fixture()
        swap['records'][0]['binding']['snapshot']['callback_id'] = 0
        swap['records'][1]['binding'].update(snapshot_result='unavailable', snapshot=None)
        swap['records'][2]['binding'] = None
        fail_suffix(source, 90); clear_missing(swap, source)
        result = self.decode(phase, swap, source)
        for key in ('zero_callback_id', 'unavailable_snapshot', 'unbound', 'missing_source'):
            self.assertEqual(result['counts'][key], 1)
        self.assertEqual(result['unmatched_callback_ids'], [1, 2, 3, 91])
        self.assertTrue(result['diagnostic_inconclusive'])

    def test_unavailable_cannot_carry_id_or_snapshot(self):
        phase, swap, source = fixture()
        swap['records'][0]['binding']['snapshot_result'] = 'unavailable'
        self.reject(phase, swap, source)

    def test_bracket_endpoints_inclusive_no_nearest_fallback(self):
        for endpoint in ('qpc_before', 'qpc_after'):
            phase, swap, source = fixture()
            phase['swap_callbacks'][0]['qpc'] = swap['records'][0]['binding']['snapshot'][endpoint]
            self.assertEqual(self.decode(phase, swap, source)['counts']['joined'], 91)
        for offset in (-1, 2):
            phase, swap, source = fixture()
            phase['swap_callbacks'][0]['qpc'] += offset
            with self.assertRaisesRegex(ValueError, 'snapshot bracket'): self.decode(phase, swap, source)

    def test_thread_and_lr_mismatch(self):
        for key in ('native_thread_id', 'guest_caller_lr'):
            phase, swap, source = fixture()
            swap['records'][0]['binding']['snapshot'][key] += 1
            self.reject(phase, swap, source)

    def test_context_and_base_mismatch_descriptive_not_causal(self):
        phase, swap, source = fixture()
        phase['swap_callbacks'][0].update(context_same=False, base_same=False)
        result = self.decode(phase, swap, source)
        self.assertEqual(result['counts']['joined'], 91)
        self.assertEqual(result['counts']['scope_context_mismatch'], 1)
        self.assertEqual(result['counts']['scope_base_mismatch'], 1)
        self.assertTrue(result['diagnostic_inconclusive'])

    def test_unscoped_suppressed_and_unhealthy_preserved(self):
        phase, swap, source = fixture()
        phase['swap_callbacks'][0].update(has_scope=False, active_scope_call_id=0,
                                         context_same=False, base_same=False, suppressed_scope=True)
        phase['counts'].update(dropped_scopes=1, stack_overflow=1)
        phase['observer_failed'] = True
        swap['statistics'].update(failed=True, callback_failed=1)
        result = self.decode(phase, swap, source)
        self.assertEqual(result['counts']['joined'], 91)
        self.assertEqual(result['counts']['suppressed'], 1)
        self.assertEqual(result['counts']['unscoped'], 1)
        self.assertIsNone(result['records'][0]['scope'])
        self.assertTrue(result['phase_health']['observer_failed'])
        self.assertTrue(result['swap_statistics']['failed'])
        self.assertTrue(result['diagnostic_inconclusive'])

    def test_unmatched_extra_callback_legal(self):
        phase, swap, source = fixture()
        extra = copy.deepcopy(phase['swap_callbacks'][0]); extra['callback_id'] = 92
        phase['swap_callbacks'].append(extra); recount(phase)
        result = self.decode(phase, swap, source)
        self.assertEqual(result['unmatched_callback_ids'], [92])
        self.assertFalse(result['diagnostic_inconclusive'])

    def test_full_validation_precedes_join_failure(self):
        # Early ID could not join; corrupt final metadata must win instead.
        for which in ('phase', 'swap', 'source'):
            phase, swap, source = fixture()
            swap['records'][0]['binding']['snapshot']['callback_id'] = 1024
            if which == 'phase': phase['swap_callbacks'][-1]['qpc'] = 0
            elif which == 'swap': swap['records'][-1]['binding']['stock'][0] = 0
            else: source['sequence']['slots'][-1]['capture']['width'] = 1
            with self.assertRaises(ValueError) as caught: self.decode(phase, swap, source)
            self.assertNotIn('unknown positive', str(caught.exception))

    def test_old_schemas_and_missing_extra_keys_rejected(self):
        for which in ('phase_schema', 'swap_schema', 'phase_id', 'swap_id', 'extra'):
            phase, swap, source = fixture()
            if which == 'phase_schema': phase['schema'] = 'mcla-phase-census-v1'
            elif which == 'swap_schema': swap['schema'] = 'mcla-game-swap-observer-v1'
            elif which == 'phase_id': del phase['swap_callbacks'][-1]['callback_id']
            elif which == 'swap_id': del swap['records'][-1]['binding']['snapshot']['callback_id']
            else: phase['swap_callbacks'][-1]['extra'] = 0
            self.reject(phase, swap, source)

    def test_raw_immutable_size_utf8_and_nesting(self):
        phase, swap, source = fixture()
        for target, limit in (('phase', decoder.PHASE_MAX_BYTES), ('swap', decoder.SWAP_MAX_BYTES)):
            for bad in (b'', None, bytearray(b'{}'), '{}', b' ' * (limit + 1), b'\xff',
                        b'\xef\xbb\xbf{}', b'{} trailing', b'[' * 2000 + b']' * 2000):
                with self.subTest(target=target, kind=type(bad)):
                    args = [raw(phase), raw(swap), source, TOKEN]; args[target == 'swap'] = bad
                    with self.assertRaises(ValueError): decoder.validate_phase_source_join(*args)

    def test_duplicate_keys_floats_and_unsigned_lexemes(self):
        phase, swap, source = fixture()
        for target in (0, 1):
            original = raw(phase if target == 0 else swap)
            cases = [original.replace(b'"callback_id":1', b'"callback_id":1,"callback_id":1', 1)]
            cases += [original.replace(b'"callback_id":1', b'"callback_id":' + value, 1)
                      for value in (b'1.0', b'1e0', b'NaN', b'Infinity', b'-0', b'-1', b'1' * 21)]
            for bad in cases:
                args = [raw(phase), raw(swap), source, TOKEN]; args[target] = bad
                with self.assertRaises(ValueError): decoder.validate_phase_source_join(*args)

    def test_tokens_window_frequency_and_false_claims(self):
        for which in ('token', 'window', 'frequency', 'proof', 'source_epoch'):
            phase, swap, source = fixture()
            if which == 'token': phase['token'] = 'f' * 32
            elif which == 'window': swap['begin_qpc'] += 1
            elif which == 'frequency': phase['qpc_frequency'] += 1
            elif which == 'proof': phase['causal_join_proven'] = True
            else: source['sequence']['slots'][-1]['capture']['presenter_epoch'] += 1
            self.reject(phase, swap, source)

    def test_all_zero_ids_remain_unjoined(self):
        phase, swap, source = fixture()
        for row in swap['records']: row['binding']['snapshot']['callback_id'] = 0
        result = self.decode(phase, swap, source)
        self.assertEqual(result['counts']['joined'], 0)
        self.assertEqual(result['counts']['zero_callback_id'], 91)
        self.assertEqual(len(result['unmatched_callback_ids']), 91)
        self.assertTrue(all(row['callback'] is None for row in result['records']))

    def test_full_callback_capacity_and_overflow(self):
        phase, swap, source = fixture()
        for identity in range(92, 1025):
            extra = copy.deepcopy(phase['swap_callbacks'][0]); extra['callback_id'] = identity
            phase['swap_callbacks'].append(extra)
        recount(phase)
        result = self.decode(phase, swap, source)
        self.assertEqual(len(result['unmatched_callback_ids']), 933)
        self.assertEqual(result['counts']['joined'], 91)
        phase['swap_callbacks'].append(copy.deepcopy(phase['swap_callbacks'][0])); recount(phase)
        self.reject(phase, swap, source)

    def test_unknown_scope_caller_remains_observed_inventory_not_route_proof(self):
        phase, swap, source = fixture()
        phase['scopes'][0]['caller_lr'] = 123
        result = self.decode(phase, swap, source)
        self.assertEqual(result['phase_scope_inventory']['render']['unknown_caller_lr_count'], 1)
        self.assertEqual(result['records'][0]['scope']['caller_lr'], 123)
        self.assertFalse(result['route_conclusions_proven'])
        self.assertFalse(result['update_causality_proven'])

    def test_no_callbacks_all_unbound_valid_coverage_holes(self):
        phase, swap, source = fixture()
        phase['swap_callbacks'] = []; recount(phase)
        for row in swap['records']: row['binding'] = None
        result = self.decode(phase, swap, source)
        self.assertEqual(result['counts']['unbound'], 91)
        self.assertEqual(result['counts']['joined'], 0)
        self.assertTrue(result['diagnostic_inconclusive'])


if __name__ == '__main__':
    unittest.main()
