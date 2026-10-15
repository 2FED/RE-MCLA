"""Pure v2 callback-ID/source linkage audit, not simulation or native proof.

Both complete journals and source metadata are validated before any join.
The v1 validators are reused only through detached, explicit downgrades.
IDs select records; clocks/threads merely reject inconsistent selections.
No IO, pixels, nearest-time matching, native admission or causal inference.
"""
import copy
import hashlib
import json

import mcla_phase_census_decode as phase_v1
import mcla_game_swap_decode as swap_v1
from mcla_game_source_scout_decode import resolve_source_schedule

PHASE_MAX_BYTES = 2 * 1024 * 1024
SWAP_MAX_BYTES = 128 * 1024
FALSE_FLAGS = phase_v1.FALSE_FLAGS | {
    'source_phase_join_proven', 'update_causality_proven', 'state_coherence_proven',
    'display_proven', 'scene_admitted', 'cycle_timing_proven',
    'route_conclusions_proven', 'physical_accepted'}


def need(ok, message):
    if not ok:
        raise ValueError(message)


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def _integer(text):
    need(len(text) <= 20 and text.isascii() and text.isdecimal(),
         'unsigned bounded JSON integer')
    return int(text)


def _no_float(_):
    raise ValueError('floating/NaN/Infinity JSON value unsupported')


def _parse(raw, limit):
    need(type(raw) is bytes and 0 < len(raw) <= limit, 'immutable bounded JSON bytes')
    try:
        return json.loads(raw.decode('utf-8', errors='strict'), object_pairs_hook=_pairs,
                          parse_int=_integer, parse_float=_no_float, parse_constant=_no_float)
    except (UnicodeError, RecursionError) as error:
        raise ValueError('invalid UTF8 or excessive JSON nesting') from error


def _callback_id(value, low):
    need(type(value) is int and low <= value <= 1024, 'bounded exact callback ID')
    return value


def validate_phase_source_join(phase_raw_bytes, swap_raw_bytes, source_receipt, token):
    """Return detached finite-profile observations; never native acceptance.

    Unmatched phase callbacks and ID0 are honest coverage holes. Positive
    unknown IDs, reused IDs for distinct tokens, or inconsistent duplicate
    sources are integrity failures. Unhealthy scopes remain descriptive.
    The caller still owns input provenance, native cleanup and pixel review.
    """
    schedule = resolve_source_schedule(source_receipt, token=token)
    phase = _parse(phase_raw_bytes, PHASE_MAX_BYTES)
    swap = _parse(swap_raw_bytes, SWAP_MAX_BYTES)
    phase_v1.exact(phase, phase_v1.TOP_KEYS, 'exact v2 phase schema')
    swap_v1.exact(swap, swap_v1.TOP_KEYS, 'exact v2 swap schema')
    need(phase['schema'] == 'mcla-phase-census-v2', 'phase v2 required')
    need(swap['schema'] == 'mcla-game-swap-observer-v2', 'swap v2 required')
    callbacks = phase['swap_callbacks']
    need(type(callbacks) is list and len(callbacks) <= 1024, 'bounded callback array')
    callback_ids = set()
    for row in callbacks:
        phase_v1.exact(row, phase_v1.CALLBACK_KEYS | {'callback_id'}, 'exact v2 callback')
        callback_id = _callback_id(row['callback_id'], 1)
        need(callback_id not in callback_ids, 'duplicate callback ID')
        callback_ids.add(callback_id)
    need(callback_ids == set(range(1, len(callbacks) + 1)),
         'complete consecutive phase callback IDs required')
    records = swap['records']
    need(type(records) is list and len(records) == schedule['scheduled_slots'], 'exact profile v2 swap records')
    for row in records:
        swap_v1.exact(row, swap_v1.ROW_KEYS, 'exact v2 swap row')
        binding = row['binding']
        if binding is not None:
            swap_v1.exact(binding, swap_v1.BINDING_KEYS, 'exact v2 binding')
            if binding['snapshot_result'] == 'available':
                snapshot = binding['snapshot']
                swap_v1.exact(snapshot, swap_v1.SNAPSHOT_KEYS | {'callback_id'}, 'exact v2 snapshot')
                _callback_id(snapshot['callback_id'], 0)

    # Copy before validation, and never expose any caller-owned container.
    source = copy.deepcopy(source_receipt)
    old_phase, old_swap = copy.deepcopy(phase), copy.deepcopy(swap)
    old_phase['schema'] = 'mcla-phase-census-v1'
    old_swap['schema'] = 'mcla-game-swap-observer-v1'
    for row in old_phase['swap_callbacks']:
        del row['callback_id']
    for row in old_swap['records']:
        binding = row['binding']
        if binding is not None and binding['snapshot_result'] == 'available':
            del binding['snapshot']['callback_id']
    phase_audit = phase_v1.validate_phase_census(
        json.dumps(old_phase, separators=(',', ':'), allow_nan=False).encode(), source, token)
    swap_audit = swap_v1.validate_swap_receipt(old_swap, source, token)

    # All metadata (including the final records) has passed before this point.
    by_id = {row['callback_id']: row for row in callbacks}
    by_scope = {row['call_id']: row for row in phase['scopes']}
    id_tokens, source_ids, used = {}, {}, set()
    rows = []
    counts = dict.fromkeys(('joined', 'missing_source', 'unbound', 'unavailable_snapshot',
                           'zero_callback_id', 'scoped', 'unscoped', 'suppressed',
                           'scope_context_mismatch', 'scope_base_mismatch'), 0)
    for row in records:
        identity = None if row['presenter_epoch'] is None else (
            row['presenter_epoch'], row['acquired_sequence'])
        binding = row['binding']
        snapshot = None if binding is None else binding['snapshot']
        callback_id = None if snapshot is None else snapshot['callback_id']
        if identity is not None:
            need(identity not in source_ids or source_ids[identity] == callback_id,
                 'duplicate source changed callback ID')
            source_ids[identity] = callback_id
        callback = scope = None
        if identity is None:
            disposition = 'missing_source'
        elif binding is None:
            disposition = 'unbound'
        elif snapshot is None:
            disposition = 'unavailable_snapshot'
        elif callback_id == 0:
            disposition = 'zero_callback_id'
        else:
            need(callback_id in by_id, 'unknown positive callback ID')
            callback = by_id[callback_id]
            swap_token = (binding['epoch'], binding['serial'])
            need(callback_id not in id_tokens or id_tokens[callback_id] == swap_token,
                 'callback ID reused by distinct swap token')
            id_tokens[callback_id] = swap_token
            need(snapshot['qpc_before'] <= callback['qpc'] <= snapshot['qpc_after'],
                 'callback outside explicit snapshot bracket')
            need(callback['native_thread_id'] == snapshot['native_thread_id'],
                 'callback native thread mismatch')
            need(callback['caller_lr'] == snapshot['guest_caller_lr'], 'callback caller LR mismatch')
            used.add(callback_id)
            disposition = 'joined'
            if callback['has_scope']:
                scope = by_scope[callback['active_scope_call_id']]
                counts['scoped'] += 1
                counts['scope_context_mismatch'] += int(not callback['context_same'])
                counts['scope_base_mismatch'] += int(not callback['base_same'])
            else:
                counts['unscoped'] += 1
            counts['suppressed'] += int(callback['suppressed_scope'])
        counts[disposition] += 1
        rows.append({'slot': row['slot'], 'presenter_epoch': row['presenter_epoch'],
                     'acquired_sequence': row['acquired_sequence'],
                     'swap_token': None if binding is None else {
                         'epoch': binding['epoch'], 'serial': binding['serial']},
                     'callback_id': callback_id, 'disposition': disposition,
                     'callback': copy.deepcopy(callback), 'scope': copy.deepcopy(scope),
                     'scope_role': None if scope is None else scope['kind']})
    issues = ['phase:' + item for item in phase_audit['diagnostic_issues']]
    issues += ['swap:' + item for item in swap_audit['diagnostic_issues']]
    issues += [key for key, count in counts.items() if count and key not in ('joined', 'scoped')]
    result = {'schema': 'mcla-phase-source-join-v2', 'token': token,
              'status': 'PHASE_SOURCE_JOIN_DIAGNOSTIC_INCONCLUSIVE' if issues else
                        'PHASE_SOURCE_JOIN_REVIEW_PENDING',
              'phase_raw_sha256': hashlib.sha256(phase_raw_bytes).hexdigest(),
              'swap_raw_sha256': hashlib.sha256(swap_raw_bytes).hexdigest(),
              'begin_qpc': phase['begin_qpc'], 'end_qpc_exclusive': phase['end_qpc_exclusive'],
              'qpc_frequency': phase['qpc_frequency'], 'counts': counts,
              'unique_joined_callbacks': len(used), 'unique_sources': len(source_ids),
              'unmatched_callback_ids': sorted(callback_ids - used), 'records': rows,
              'phase_health': dict(phase_audit['health']),
              'phase_scope_inventory': copy.deepcopy(phase_audit['scope_inventory']),
              'phase_thread_inventory': copy.deepcopy(phase_audit['thread_inventory']),
              'swap_statistics': dict(swap_audit['statistics']),
              'diagnostic_inconclusive': bool(issues), 'diagnostic_issues': sorted(set(issues)),
              'physical_acceptance_pending': True, 'unobserved_ancestry_possible': True,
              'interpretation': 'EXPLICIT_CALLBACK_ID_SOURCE_METADATA_LINK_NOT_UPDATE_OR_SIMULATION_PROOF'}
    result.update(dict.fromkeys(FALSE_FLAGS, False))
    return result
