"""Strict pure decoder of observed dynamic scopes, never a causal frame join.

No IO, clocks, game memory, pixels, timestamp matching or native admission.
References below are the journal's explicit TLS relations, not relations
inferred from interval overlap. Parent0 means no admitted observed parent,
not a game root. Equal QPC ticks do not identify unique active scopes.
"""
import hashlib
import json
import re

from mcla_game_source_scout_decode import validate_metadata

MAX_BYTES = 2 * 1024 * 1024
U32, U64, MAX_QPC = 2**32 - 1, 2**64 - 1, 2**63 - 1
SCOPE_CAPACITY, CALLBACK_CAPACITY, TLS_CAPACITY = 2048, 1024, 8
FALSE_FLAGS = {'native_admitted', 'simulation_proven', 'render_proven', 'fps_proven',
               'frame_to_step_mapping_proven', 'causal_join_proven'}
HEALTH_FLAGS = {'observer_failed', 'drain_incomplete', 'invalid_qpc',
                'original_exception', 'identity_exhausted'}
COUNT_KEYS = {'stored_scopes', 'completed_scopes', 'active_scopes', 'stored_callbacks',
              'dropped_scopes', 'dropped_callbacks', 'stack_overflow',
              'scope_overflow', 'callback_overflow'}
TOP_KEYS = FALSE_FLAGS | HEALTH_FLAGS | {'schema', 'token', 'status', 'clock', 'begin_qpc',
    'end_qpc_exclusive', 'qpc_frequency', 'scope_capacity', 'callback_capacity',
    'tls_capacity', 'counts', 'scopes', 'swap_callbacks', 'unobserved_ancestry_possible'}
SCOPE_FLAGS = {'completed', 'original_exception', 'invalid_bracket', 'context_nonnull',
              'base_nonnull', 'has_parent', 'parent_context_same', 'parent_base_same'}
SCOPE_KEYS = SCOPE_FLAGS | {'call_id', 'parent_call_id', 'thread_epoch', 'native_thread_id',
                          'entry_qpc', 'exit_qpc', 'caller_lr', 'incoming_r3', 'kind'}
CALLBACK_FLAGS = {'has_scope', 'context_same', 'base_same', 'suppressed_scope'}
CALLBACK_KEYS = CALLBACK_FLAGS | {'qpc', 'active_scope_call_id', 'thread_epoch',
                                'native_thread_id', 'caller_lr', 'incoming_r3', 'swap_token_bound'}
EXPECTED_LRS = {'update': {0x821316A0}, 'coordinator': {0x821316B8},
                'render': {0x82305DFC, 0x82305F80}}


def need(ok, message):
    if not ok:
        raise ValueError(message)


def exact(value, keys, message):
    need(type(value) is dict and set(value) == keys, message)


def uint(value, high=U64, low=0):
    need(type(value) is int and low <= value <= high, 'bounded exact unsigned integer')
    return value


def flags(value, keys):
    need(all(type(value[key]) is bool for key in keys), 'exact boolean flags')


def _pairs(pairs):
    value = {}
    for key, item in pairs:
        need(key not in value, 'duplicate JSON key')
        value[key] = item
    return value


def _integer(text):
    need(len(text) <= 20 and text.isascii() and text.isdecimal(), 'unsigned bounded JSON integer')
    return int(text)


def _no_float(_):
    raise ValueError('floating/NaN/Infinity JSON value unsupported')


def _parse(raw):
    need(type(raw) is bytes and 0 < len(raw) <= MAX_BYTES, 'immutable bounded JSON bytes')
    try:
        return json.loads(raw.decode('utf-8', errors='strict'), object_pairs_hook=_pairs,
                          parse_int=_integer, parse_float=_no_float, parse_constant=_no_float)
    except (UnicodeError, RecursionError) as error:
        raise ValueError('invalid UTF8 or excessive JSON nesting') from error


def validate_phase_census(raw_bytes, source_receipt, token):
    """Decode a configured-window census; all claims remain false.

    The source receipt contributes only independently validated configuration
    metadata. This is not a token/source/phase association. The supervisor owns
    native source pins, immutable capture inventory, pixels and process scope.
    Clock-fault exits, scope straddles and native health failures are explicit.
    """
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token), 'expected32lowerhex token')
    validate_metadata(source_receipt, token=token)
    sequence = source_receipt['sequence']
    frequency = sequence['qpc_frequency']
    begin = uint(sequence['begin_qpc'] - frequency, MAX_QPC, 1)
    end = uint(sequence['begin_qpc'] + 91 * frequency // 10, MAX_QPC, 1)
    need(0 < end - begin <= 12 * frequency, 'bounded shared phase window')
    data = _parse(raw_bytes)
    exact(data, TOP_KEYS, 'exact phase census schema')
    for key, expected in (('schema', 'mcla-phase-census-v1'), ('token', token),
                          ('status', 'PHASE_CENSUS_REVIEW_PENDING'),
                          ('clock', 'win32-query-performance-counter')):
        need(type(data[key]) is str and data[key] == expected, 'schema/token/status/clock')
    need(all(data[key] is False for key in FALSE_FLAGS), 'unsupported proof/admission claim')
    need(data['unobserved_ancestry_possible'] is True, 'unobserved ancestry must remain possible')
    flags(data, HEALTH_FLAGS)
    for key, expected in (('begin_qpc', begin), ('end_qpc_exclusive', end),
                          ('qpc_frequency', frequency), ('scope_capacity', SCOPE_CAPACITY),
                          ('callback_capacity', CALLBACK_CAPACITY), ('tls_capacity', TLS_CAPACITY)):
        need(uint(data[key], MAX_QPC, 1) == expected, 'exact configured phase window/capacity')
    counts = data['counts']
    exact(counts, COUNT_KEYS, 'exact journal count schema')
    for value in counts.values(): uint(value)
    scopes, callbacks = data['scopes'], data['swap_callbacks']
    need(type(scopes) is list and len(scopes) <= SCOPE_CAPACITY, 'bounded scope journal')
    need(type(callbacks) is list and len(callbacks) <= CALLBACK_CAPACITY, 'bounded callback journal')
    need(counts['stored_scopes'] == len(scopes) and counts['stored_callbacks'] == len(callbacks),
         'stored counts versus arrays')
    need(counts['completed_scopes'] + counts['active_scopes'] == len(scopes),
         'completion/active count conservation')
    for key in ('stack_overflow', 'scope_overflow'):
        need(counts[key] <= counts['dropped_scopes'], 'scope overflow versus drops')
    need(counts['callback_overflow'] <= counts['dropped_callbacks'], 'callback overflow versus drops')
    need(not counts['scope_overflow'] or len(scopes) == SCOPE_CAPACITY, 'scope overflow without full ledger')
    need(not counts['callback_overflow'] or len(callbacks) == CALLBACK_CAPACITY,
         'callback overflow without full ledger')
    need(not counts['active_scopes'] or data['observer_failed'] or data['drain_incomplete'],
         'unexplained incomplete drain')
    need(not data['identity_exhausted'] or counts['dropped_scopes'] or counts['dropped_callbacks'],
         'identity exhaustion without drops')
    if counts['dropped_scopes'] or counts['dropped_callbacks']:
        need(any(counts[key] for key in ('stack_overflow', 'scope_overflow', 'callback_overflow')) or
             data['invalid_qpc'] or data['identity_exhausted'] or data['observer_failed'],
             'dropped observations without diagnostic cause')
    by_call, epochs, depths = {}, {}, {}
    inventories = {kind: {'count': 0, 'caller_lrs': {}, 'incoming_r3_values': {}}
                   for kind in EXPECTED_LRS}
    issues = {key for key in HEALTH_FLAGS if data[key]}
    if not scopes: issues.add('empty_scope_inventory')
    if not callbacks: issues.add('empty_callback_inventory')
    issues.update(key for key in ('dropped_scopes', 'dropped_callbacks', 'stack_overflow',
                                 'scope_overflow', 'callback_overflow') if counts[key])
    completed, invalid, exceptions, straddles = 0, 0, 0, 0

    def thread(row):
        epoch = uint(row['thread_epoch'], U64 - 1, 1)
        tid = uint(row['native_thread_id'], U32, 1)
        need(epoch not in epochs or epochs[epoch]['native_thread_id'] == tid, 'TLS epoch changed native thread')
        epochs.setdefault(epoch, {'thread_epoch': epoch, 'native_thread_id': tid,
                                  'scopes': 0, 'swap_callbacks': 0})
        return epoch, tid

    for row in scopes:
        exact(row, SCOPE_KEYS, 'exact scope record')
        flags(row, SCOPE_FLAGS)
        call = uint(row['call_id'], U64 - 1, 1)
        need(call not in by_call, 'duplicate scope call ID')
        parent_id = uint(row['parent_call_id'], U64 - 1)
        epoch, tid = thread(row)
        entry = uint(row['entry_qpc'], MAX_QPC, 1)
        exit_tick = uint(row['exit_qpc'])  # Failed clock values must not be clamped.
        uint(row['caller_lr']); uint(row['incoming_r3'])
        need(type(row['kind']) is str and row['kind'] in EXPECTED_LRS, 'known inventory kind')
        need(begin <= entry < end, 'entry admitted inside exclusive window')
        need(row['context_nonnull'], 'scope reference context cannot be null')
        if row['completed']:
            completed += 1
            bad = not (entry <= exit_tick <= MAX_QPC)
            need(row['invalid_bracket'] is bad, 'clock fault versus bracket flag')
            invalid += int(bad)
            straddles += int(not bad and exit_tick >= end)
        else:
            need(exit_tick == 0 and not row['invalid_bracket'] and not row['original_exception'],
                 'incomplete scope contains completed exit data')
            issues.add('incomplete_scopes')
        exceptions += int(row['original_exception'])
        if row['has_parent']:
            need(parent_id in by_call and parent_id < call, 'parent must be earlier inventoried call')
            parent = by_call[parent_id]
            need((parent['thread_epoch'], parent['native_thread_id']) == (epoch, tid),
                 'parent changed TLS/thread identity')
            need(parent['entry_qpc'] <= entry, 'observed parent starts after child')
            if parent['completed'] and not parent['invalid_bracket']:
                need(entry <= parent['exit_qpc'], 'child entry after observed parent exit')
                if row['completed'] and not row['invalid_bracket']:
                    need(exit_tick <= parent['exit_qpc'], 'child exit after observed parent exit')
            need(not row['parent_base_same'] or row['base_nonnull'] == parent['base_nonnull'],
                 'same parent base with conflicting nullness')
            need(parent['base_nonnull'] or row['base_nonnull'] or row['parent_base_same'],
                 'two null bases cannot differ')
            depths[call] = depths[parent_id] + 1
        else:
            need(parent_id == 0 and not row['parent_context_same'] and not row['parent_base_same'],
                 'no parent must clear relation fields')
            depths[call] = 1
        need(depths[call] <= TLS_CAPACITY, 'observed TLS nesting exceeds fixed capacity')
        by_call[call] = row
        epochs[epoch]['scopes'] += 1
        inventory = inventories[row['kind']]
        inventory['count'] += 1
        for field, key in (('caller_lr', 'caller_lrs'), ('incoming_r3', 'incoming_r3_values')):
            value = row[field]
            inventory[key][value] = inventory[key].get(value, 0) + 1
    need(completed == counts['completed_scopes'], 'completed count versus records')
    need(not invalid or data['invalid_qpc'], 'invalid bracket without global clock diagnostic')
    need(data['original_exception'] is bool(exceptions), 'exception aggregate versus records')
    callback_scoped, callback_unscoped, callback_suppressed = 0, 0, 0
    for row in callbacks:
        exact(row, CALLBACK_KEYS, 'exact callback record')
        flags(row, CALLBACK_FLAGS)
        need(row['swap_token_bound'] is False, 'unsupported callback token association')
        epoch, tid = thread(row)
        qpc = uint(row['qpc'], MAX_QPC, 1)
        need(begin <= qpc < end, 'callback admitted inside exclusive window')
        call = uint(row['active_scope_call_id'], U64 - 1)
        uint(row['caller_lr']); uint(row['incoming_r3'])
        if row['has_scope']:
            need(not row['suppressed_scope'] and call in by_call, 'unknown or suppressed active scope')
            scope = by_call[call]
            need((scope['thread_epoch'], scope['native_thread_id']) == (epoch, tid),
                 'callback changed explicit scope TLS/thread')
            need(scope['entry_qpc'] <= qpc, 'callback precedes explicit scope entry')
            if scope['completed'] and not scope['invalid_bracket']:
                need(qpc <= scope['exit_qpc'], 'callback exceeds explicit scope exit')
            callback_scoped += 1
        else:
            need(call == 0 and not row['context_same'] and not row['base_same'],
                 'unscoped callback must clear relation fields')
            callback_unscoped += 1
        callback_suppressed += int(row['suppressed_scope'])
        epochs[epoch]['swap_callbacks'] += 1
    if callback_suppressed:
        need(counts['dropped_scopes'] > 0, 'suppressed callback without a dropped scope')
        issues.add('suppressed_callbacks')
    if callback_unscoped: issues.add('callbacks_without_observed_scope')
    # No token linkage, nor time-derived choice among scopes, appears here.
    for kind, inventory in inventories.items():
        inventory['unknown_caller_lr_count'] = sum(n for lr, n in inventory['caller_lrs'].items()
                                                    if lr not in EXPECTED_LRS[kind])
        for key in ('caller_lrs', 'incoming_r3_values'):
            inventory[key] = [{'value': value, 'count': count}
                              for value, count in sorted(inventory[key].items())]
    result = {'schema': 'mcla-phase-census-decode-v1', 'token': token,
              'status': 'PHASE_CENSUS_DIAGNOSTIC_INCONCLUSIVE' if issues else 'PHASE_CENSUS_REVIEW_PENDING',
              'raw_sha256': hashlib.sha256(raw_bytes).hexdigest(), 'begin_qpc': begin,
              'end_qpc_exclusive': end, 'qpc_frequency': frequency, 'counts': dict(counts),
              'health': {key: data[key] for key in sorted(HEALTH_FLAGS)}, 'scopes': scopes,
              'swap_callbacks': callbacks, 'scope_inventory': inventories,
              'thread_inventory': [epochs[key] for key in sorted(epochs)],
              'completed_clock_faults': invalid, 'scope_exits_at_or_after_window_end': straddles,
              'scoped_callbacks': callback_scoped, 'unscoped_callbacks': callback_unscoped,
              'suppressed_callbacks': callback_suppressed, 'diagnostic_inconclusive': bool(issues),
              'diagnostic_issues': sorted(issues), 'source_phase_join_proven': False,
              'route_conclusions_proven': False, 'unobserved_ancestry_possible': True,
              'state_coherence_proven': False, 'display_proven': False,
              'interpretation': 'OBSERVED_TLS_SCOPE_CENSUS_NO_SOURCE_TOKEN_OR_CAUSAL_JOIN'}
    result.update(dict.fromkeys(FALSE_FLAGS, False))
    return result
