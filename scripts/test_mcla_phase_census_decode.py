"""Synthetic file/native-independent phase journal regressions."""
import copy
import json
import unittest

import mcla_phase_census_decode as decoder
from test_mcla_game_source_scout_decode import fixture as source_fixture, TOKEN


def scope(call, kind, entry, exit_tick, *, parent=0, epoch=1, tid=100, lr=None, r3=0):
    return {'call_id': call, 'parent_call_id': parent, 'thread_epoch': epoch,
            'native_thread_id': tid, 'entry_qpc': entry, 'exit_qpc': exit_tick,
            'caller_lr': next(iter(decoder.EXPECTED_LRS[kind])) if lr is None else lr,
            'incoming_r3': r3, 'completed': True, 'original_exception': False,
            'invalid_bracket': False, 'context_nonnull': True, 'base_nonnull': True,
            'has_parent': bool(parent), 'parent_context_same': bool(parent),
            'parent_base_same': bool(parent), 'kind': kind}


def callback(qpc, call=0, *, epoch=1, tid=100):
    return {'qpc': qpc, 'active_scope_call_id': call, 'thread_epoch': epoch,
            'native_thread_id': tid, 'caller_lr': 0x8241A0E8, 'incoming_r3': 0,
            'has_scope': bool(call), 'context_same': bool(call), 'base_same': bool(call),
            'suppressed_scope': False, 'swap_token_bound': False}


def recount(value):
    value['counts'].update(stored_scopes=len(value['scopes']),
                          completed_scopes=sum(row['completed'] for row in value['scopes']),
                          active_scopes=sum(not row['completed'] for row in value['scopes']),
                          stored_callbacks=len(value['swap_callbacks']))


def fixture():
    source = source_fixture()
    freq = source['sequence']['qpc_frequency']
    begin = source['sequence']['begin_qpc'] - freq
    value = dict.fromkeys(decoder.FALSE_FLAGS | decoder.HEALTH_FLAGS, False)
    value.update(schema='mcla-phase-census-v1', token=TOKEN, status='PHASE_CENSUS_REVIEW_PENDING',
                 clock='win32-query-performance-counter', begin_qpc=begin,
                 end_qpc_exclusive=source['sequence']['begin_qpc'] + 91 * freq // 10,
                 qpc_frequency=freq, scope_capacity=2048, callback_capacity=1024,
                 tls_capacity=8, unobserved_ancestry_possible=True,
                 counts=dict.fromkeys(decoder.COUNT_KEYS, 0),
                 scopes=[scope(1, 'update', begin+10, begin+100),
                         scope(2, 'coordinator', begin+200, begin+600, r3=0x82900000),
                         scope(3, 'render', begin+300, begin+500, parent=2)],
                 swap_callbacks=[callback(begin+400, 3)])
    recount(value)
    return value, source


def raw(value):
    return json.dumps(value, separators=(',', ':'), allow_nan=False).encode('utf-8')


class PhaseDecodeTests(unittest.TestCase):
    def decode(self, value, source, token=TOKEN):
        return decoder.validate_phase_census(raw(value), source, token)

    def reject(self, value, source, token=TOKEN):
        with self.assertRaises(ValueError): self.decode(value, source, token)

    def test_normal_inventory_explicit_refs_no_claims(self):
        value, source = fixture(); original = copy.deepcopy((value, source))
        result = self.decode(value, source)
        self.assertEqual((value, source), original)
        self.assertEqual(result['scope_inventory']['render']['incoming_r3_values'], [{'value': 0, 'count': 1}])
        self.assertEqual(result['scoped_callbacks'], 1)
        self.assertEqual(result['thread_inventory'][0]['scopes'], 3)
        self.assertEqual(result['scopes'][2]['parent_call_id'], 2)
        for key in decoder.FALSE_FLAGS | {'source_phase_join_proven', 'state_coherence_proven',
                                         'display_proven', 'route_conclusions_proven'}:
            self.assertIs(result[key], False)
        self.assertIs(result['unobserved_ancestry_possible'], True)
        self.assertNotIn('PASS', result['status'])
        for key in ('fps', 'simulation_steps', 'delta_sum', 'source_bindings', 'cycle_row'):
            self.assertNotIn(key, result)
        value['scopes'][0]['incoming_r3'] = 99
        source['sequence']['begin_qpc'] = 1
        self.assertEqual(result['scopes'][0]['incoming_r3'], 0)
        json.dumps(result, allow_nan=False)

    def test_raw_bytes_bounds_utf8_and_json(self):
        value, source = fixture()
        for bad in (b'', bytearray(raw(value)), raw(value).decode(), None,
                    b' ' * (decoder.MAX_BYTES + 1), b'\xff', b'\xef\xbb\xbf{}',
                    b'{} trailing', b'[' * 2000 + b']' * 2000):
            with self.assertRaises(ValueError): decoder.validate_phase_census(bad, source, TOKEN)

    def test_duplicate_keys_float_nan_and_integer_lexemes(self):
        value, source = fixture(); data = raw(value)
        variants = [data.replace(b'"schema":', b'"schema":"extra","schema":', 1),
                    data.replace(b'"call_id":1', b'"call_id":1,"call_id":1', 1),
                    data.replace(b'"call_id":1', b'"call_id":-0', 1),
                    data.replace(b'"call_id":1', b'"call_id":1.0', 1),
                    data.replace(b'"call_id":1', b'"call_id":1e0', 1),
                    data.replace(b'"call_id":1', b'"call_id":NaN', 1),
                    data.replace(b'"call_id":1', b'"call_id":Infinity', 1),
                    data.replace(b'"call_id":1', b'"call_id":100000000000000000000', 1)]
        for bad in variants:
            with self.assertRaises(ValueError): decoder.validate_phase_census(bad, source, TOKEN)

    def test_exact_keys_every_level(self):
        for path in ((), ('counts',), ('scopes', 0), ('swap_callbacks', 0)):
            for extra in (False, True):
                value, source = fixture(); target = value
                for key in path: target = target[key]
                if extra: target['extra'] = 1
                else: target.pop(next(iter(target)))
                self.reject(value, source)

    def test_header_token_source_window_and_false_flags(self):
        for key in ('schema', 'token', 'status', 'clock'):
            for bad in ('wrong', None, True):
                value, source = fixture(); value[key] = bad; self.reject(value, source)
        for key in ('begin_qpc', 'end_qpc_exclusive', 'qpc_frequency', 'scope_capacity', 'callback_capacity', 'tls_capacity'):
            for bad in (True, 0, decoder.U64+1):
                value, source = fixture(); value[key] = bad; self.reject(value, source)
            value, source = fixture(); value[key] += 1; self.reject(value, source)
        for key in decoder.FALSE_FLAGS:
            for bad in (True, 0, 'false'):
                value, source = fixture(); value[key] = bad; self.reject(value, source)
        for token in (TOKEN.upper(), '', True, 'f'*32): self.reject(*fixture(), token=token)
        for bad in (False, 1, None):
            value, source = fixture(); value['unobserved_ancestry_possible'] = bad; self.reject(value, source)
        value, source = fixture(); source['sequence']['slots'][90]['capture']['acquired_sequence'] = True
        self.reject(value, source)

    def test_all_exact_booleans_and_unsigned_bounds(self):
        for target_path, keys in (((), decoder.HEALTH_FLAGS), (('scopes', 0), decoder.SCOPE_FLAGS),
                                  (('swap_callbacks', 0), decoder.CALLBACK_FLAGS)):
            for key in keys:
                value, source = fixture(); target = value
                for part in target_path: target = target[part]
                target[key] = 1; self.reject(value, source)
        for target_path, keys in ((('scopes', 0), decoder.SCOPE_KEYS-decoder.SCOPE_FLAGS-{'kind'}),
                                  (('swap_callbacks', 0), decoder.CALLBACK_KEYS-decoder.CALLBACK_FLAGS-{'swap_token_bound'}),
                                  (('counts',), decoder.COUNT_KEYS)):
            for key in keys:
                for bad in (True, -1, decoder.U64+1):
                    value, source = fixture(); target = value
                    for part in target_path: target = target[part]
                    target[key] = bad; self.reject(value, source)

    def test_array_caps_count_conservation(self):
        for key in decoder.COUNT_KEYS:
            value, source = fixture(); value['counts'][key] += 1; self.reject(value, source)
        for key, capacity in (('scopes', 2048), ('swap_callbacks', 1024)):
            value, source = fixture(); value[key] = [copy.deepcopy(value[key][0]) for _ in range(capacity+1)]
            recount(value); self.reject(value, source)
            value, source = fixture(); value[key] = None; self.reject(value, source)

    def test_unique_scope_ids_identity_and_kind(self):
        for field, bad in (('call_id', 1), ('call_id', 0), ('call_id', decoder.U64),
                           ('thread_epoch', 0), ('thread_epoch', decoder.U64),
                           ('native_thread_id', 0), ('native_thread_id', decoder.U32+1),
                           ('kind', 'physics'), ('context_nonnull', False)):
            value, source = fixture(); value['scopes'][2][field] = bad; self.reject(value, source)
        value, source = fixture(); value['scopes'][2]['native_thread_id'] = 101; self.reject(value, source)
        value, source = fixture(); value['swap_callbacks'][0]['native_thread_id'] = 101; self.reject(value, source)

    def test_unknown_lr_full_u64_r3_and_tid_reuse_remain_inventory(self):
        value, source = fixture(); row = value['scopes'][2]
        row.update(caller_lr=decoder.U64, incoming_r3=decoder.U64)
        result = self.decode(value, source)
        self.assertEqual(result['scope_inventory']['render']['unknown_caller_lr_count'], 1)
        self.assertEqual(result['scopes'][2]['incoming_r3'], decoder.U64)
        value, source = fixture(); begin = value['begin_qpc']
        value['scopes'].append(scope(4, 'render', begin+700, begin+800, epoch=2, tid=100))
        value['swap_callbacks'].append(callback(begin+750, 4, epoch=2, tid=100)); recount(value)
        self.assertEqual(len(self.decode(value, source)['thread_inventory']), 2)

    def test_known_parent_graph_flags_and_brackets(self):
        cases = [('parent_call_id', 999), ('parent_call_id', 3), ('has_parent', False),
                 ('thread_epoch', 2), ('entry_qpc', 0)]
        for key, bad in cases:
            value, source = fixture(); value['scopes'][2][key] = bad; self.reject(value, source)
        for key in ('parent_context_same', 'parent_base_same'):
            value, source = fixture(); value['scopes'][0][key] = True; self.reject(value, source)
        value, source = fixture(); row = value['scopes'][2]
        row['entry_qpc'] = value['scopes'][1]['entry_qpc']-1; self.reject(value, source)
        value, source = fixture(); value['scopes'][2]['exit_qpc'] = value['scopes'][1]['exit_qpc']+1
        self.reject(value, source)
        value, source = fixture(); value['scopes'][2]['base_nonnull'] = False; self.reject(value, source)

    def test_fixed_tls_depth_and_equal_tick_siblings_not_inferred(self):
        value, source = fixture(); begin = value['begin_qpc']
        value['scopes'] = [scope(i, 'render', begin+10, begin+10, parent=i-1) for i in range(1, 9)]
        value['swap_callbacks'] = [callback(begin+10, 8)]; recount(value)
        self.decode(value, source)
        value['scopes'].append(scope(9, 'render', begin+10, begin+10, parent=8)); recount(value)
        self.reject(value, source)
        value, source = fixture(); begin = value['begin_qpc']
        value['scopes'] = [scope(1, 'render', begin+10, begin+10), scope(2, 'render', begin+10, begin+10)]
        value['swap_callbacks'] = [callback(begin+10, 1), callback(begin+10, 2)]; recount(value)
        result = self.decode(value, source)
        self.assertEqual([r['parent_call_id'] for r in result['scopes']], [0, 0])
        self.assertEqual([r['active_scope_call_id'] for r in result['swap_callbacks']], [1, 2])

    def test_window_entry_callback_exclusive_exit_straddle(self):
        for path, key in ((('scopes', 0), 'entry_qpc'), (('swap_callbacks', 0), 'qpc')):
            for offset in (-1, None):
                value, source = fixture(); target = value
                for part in path: target = target[part]
                target[key] = value['begin_qpc']-1 if offset == -1 else value['end_qpc_exclusive']
                self.reject(value, source)
        value, source = fixture(); value['scopes'][0]['entry_qpc'] = value['begin_qpc']
        value['scopes'][0]['exit_qpc'] = value['end_qpc_exclusive']+500
        result = self.decode(value, source)
        self.assertEqual(result['scope_exits_at_or_after_window_end'], 1)

    def test_invalid_exit_raw_zero_high_regression_preserved(self):
        for bad in (0, decoder.MAX_QPC+1, decoder.U64):
            value, source = fixture(); value['scopes'][0]['exit_qpc'] = bad
            self.reject(value, source)
            value['scopes'][0]['invalid_bracket'] = True; self.reject(value, source)
            value['invalid_qpc'] = True
            result = self.decode(value, source)
            self.assertEqual(result['scopes'][0]['exit_qpc'], bad)
            self.assertTrue(result['diagnostic_inconclusive'])
        value, source = fixture(); row = value['scopes'][0]
        row.update(exit_qpc=row['entry_qpc']-1, invalid_bracket=True); value['invalid_qpc'] = True
        self.decode(value, source)
        value, source = fixture(); value['scopes'][0]['invalid_bracket'] = True
        self.reject(value, source)

    def test_original_exception_aggregate_and_incomplete(self):
        value, source = fixture(); value['scopes'][0]['original_exception'] = True
        self.reject(value, source); value['original_exception'] = True
        self.assertTrue(self.decode(value, source)['diagnostic_inconclusive'])
        value, source = fixture(); row = value['scopes'][0]
        row.update(completed=False, exit_qpc=0); recount(value)
        self.reject(value, source); value['drain_incomplete'] = True
        result = self.decode(value, source)
        self.assertEqual(result['counts']['active_scopes'], 1)
        self.assertIn('incomplete_scopes', result['diagnostic_issues'])
        row['original_exception'] = True; self.reject(value, source)

    def test_explicit_callback_reference_and_no_nearest_repair(self):
        for key, bad in (('active_scope_call_id', 999), ('thread_epoch', 2),
                         ('has_scope', False), ('suppressed_scope', True), ('swap_token_bound', True)):
            value, source = fixture(); value['swap_callbacks'][0][key] = bad; self.reject(value, source)
        for bad in ('before', 'after'):
            value, source = fixture(); scope_row = value['scopes'][2]
            value['swap_callbacks'][0]['qpc'] = scope_row['entry_qpc']-1 if bad=='before' else scope_row['exit_qpc']+1
            self.reject(value, source)
        value, source = fixture(); row = value['swap_callbacks'][0]
        row.update(has_scope=False, active_scope_call_id=0, context_same=False, base_same=False)
        result = self.decode(value, source)
        self.assertEqual(result['unscoped_callbacks'], 1)
        self.assertEqual(result['swap_callbacks'][0]['active_scope_call_id'], 0)
        row['suppressed_scope'] = True
        self.reject(value, source)
        value['counts'].update(dropped_scopes=1, stack_overflow=1)
        self.assertEqual(self.decode(value, source)['suppressed_callbacks'], 1)

    def test_mismatched_context_or_base_not_repaired(self):
        value, source = fixture(); value['scopes'][2].update(parent_context_same=False, parent_base_same=False)
        value['swap_callbacks'][0].update(context_same=False, base_same=False)
        result = self.decode(value, source)
        self.assertFalse(result['swap_callbacks'][0]['context_same'])
        self.assertFalse(result['scopes'][2]['parent_context_same'])

    def test_capacity_overflow_drop_diagnostics_and_empty(self):
        value, source = fixture(); begin = value['begin_qpc']
        value['scopes'] = [scope(i, 'render', begin+10, begin+10) for i in range(1, 2049)]
        value['swap_callbacks'] = [callback(begin+10, 1) for _ in range(1024)]
        recount(value)
        value['counts'].update(dropped_scopes=1, scope_overflow=1, stack_overflow=1,
                               dropped_callbacks=1, callback_overflow=1)
        encoded = raw(value); self.assertLess(len(encoded), decoder.MAX_BYTES)
        result = self.decode(value, source)
        self.assertTrue(result['diagnostic_inconclusive'])
        self.assertEqual(len(result['scopes']), 2048)
        self.assertEqual(len(result['swap_callbacks']), 1024)
        value, source = fixture(); value['scopes'] = []; value['swap_callbacks'] = []; recount(value)
        result = self.decode(value, source)
        self.assertEqual(result['counts']['stored_scopes'], 0)
        for key in decoder.FALSE_FLAGS: self.assertIs(result[key], False)

    def test_health_faults_and_unexplained_drops(self):
        for key in ('observer_failed', 'drain_incomplete', 'invalid_qpc'):
            value, source = fixture(); value[key] = True
            self.assertTrue(self.decode(value, source)['diagnostic_inconclusive'])
        value, source = fixture(); value['identity_exhausted'] = True; self.reject(value, source)
        value['counts']['dropped_scopes'] = 1
        self.assertTrue(self.decode(value, source)['diagnostic_inconclusive'])
        value, source = fixture(); value['counts']['dropped_callbacks'] = 1
        self.reject(value, source); value['invalid_qpc'] = True
        self.assertTrue(self.decode(value, source)['diagnostic_inconclusive'])

    def test_completed_clock_throw_is_failure_not_incomplete(self):
        value, source = fixture()
        value['scopes'][0].update(exit_qpc=0, invalid_bracket=True)
        value.update(observer_failed=True, invalid_qpc=True)
        result = self.decode(value, source)
        self.assertEqual(result['counts']['active_scopes'], 0)
        self.assertEqual(result['completed_clock_faults'], 1)
        self.assertTrue(result['diagnostic_inconclusive'])

    def test_concurrent_worker_descriptive_root_not_game_root(self):
        value, source = fixture(); begin = value['begin_qpc']
        value['scopes'].append(scope(4, 'render', begin+250, begin+550,
                                     epoch=2, tid=200, lr=0x82305F80))
        value['swap_callbacks'].append(callback(begin+450, 4, epoch=2, tid=200)); recount(value)
        result = self.decode(value, source)
        self.assertEqual(result['scopes'][3]['parent_call_id'], 0)
        self.assertTrue(result['unobserved_ancestry_possible'])
        self.assertFalse(result['route_conclusions_proven'])
        self.assertEqual(result['scope_inventory']['render']['unknown_caller_lr_count'], 0)

    def test_integer_floor_nondivisible_frequency(self):
        value, _ = fixture(); source = source_fixture(frequency=97)
        old_begin = value['begin_qpc']
        begin = source['sequence']['begin_qpc']-97
        value.update(begin_qpc=begin, end_qpc_exclusive=source['sequence']['begin_qpc']+91*97//10,
                     qpc_frequency=97)
        for row in value['scopes']:
            for key in ('entry_qpc', 'exit_qpc'): row[key] += begin-old_begin
        for row in value['swap_callbacks']: row['qpc'] += begin-old_begin
        result = self.decode(value, source)
        self.assertEqual(result['qpc_frequency'], 97)


if __name__ == '__main__':
    unittest.main()
