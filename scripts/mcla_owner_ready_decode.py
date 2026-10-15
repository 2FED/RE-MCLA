"""Strict owner-assertion/source/handoff pairing, never a gameplay or FPS proof."""
import json
from mcla_owner_ready import need, event_name

NUMBERS = {'frequency', 'journal_begin_qpc', 'journal_end_qpc', 'ready_deadline_qpc',
           'opened_qpc', 'waiting_before_qpc', 'waiting_after_qpc', 'ready_before_qpc',
           'ready_after_qpc', 'source_observed_qpc', 'freeze_qpc'}
KEYS = NUMBERS | {'schema', 'token', 'owner_assertion_only', 'scene_proven', 'clock'}


def strict(raw, maximum=4096):
    need(type(raw) is bytes and 0 < len(raw) <= maximum, 'bounded owner-ready sidecar')
    def pairs(items):
        result = {}
        for k, v in items:
            need(k not in result, 'duplicate owner-ready key')
            result[k] = v
        return result
    def invalid(_):
        raise ValueError('nonfinite owner-ready value')
    return json.loads(raw.decode('utf-8'), object_pairs_hook=pairs, parse_constant=invalid)


def validate(raw, signal, source, handoff, token):
    # Caller must also validate the full source/handoff journals before pixels.
    event_name(token)
    if type(handoff) is bytes:
        handoff = strict(handoff, 8 * 1024 * 1024)
    value = strict(raw)
    need(type(value) is dict and set(value) == KEYS, 'owner-ready exact schema')
    need(value['schema'] == 'mcla-owner-ready-v1' and value['token'] == token and
         value['clock'] == 'QueryPerformanceCounter' and value['owner_assertion_only'] is True and
         value['scene_proven'] is False, 'owner-ready identity or claim')
    need(all(type(value[k]) is int and 0 < value[k] <= 2**63-1 for k in NUMBERS), 'owner-ready integers')
    f = value['frequency']
    need(10 <= f <= 10**10 and value['journal_end_qpc'] == value['journal_begin_qpc'] + 180*f and
         value['ready_deadline_qpc'] == value['journal_end_qpc'] - 60*f, 'owner-ready unchanged horizon')
    keys = ('journal_begin_qpc', 'opened_qpc', 'waiting_before_qpc', 'waiting_after_qpc',
            'ready_before_qpc', 'ready_after_qpc', 'source_observed_qpc')
    need(all(value[a] <= value[b] for a, b in zip(keys, keys[1:])) and
         value['source_observed_qpc'] < value['ready_deadline_qpc'], 'owner-ready ordered clock brackets')
    signal_keys = {'schema', 'qpc_frequency', 'signal_before_qpc', 'signal_after_qpc',
                   'owner_assertion_only', 'scene_proven'}
    need(type(signal) is dict and set(signal) == signal_keys and
         signal['schema'] == 'mcla-owner-ready-signal-v1' and signal['owner_assertion_only'] is True and
         signal['scene_proven'] is False and type(signal['qpc_frequency']) is int and
         signal['qpc_frequency'] == f, 'owner-ready signal schema')
    before, after = signal['signal_before_qpc'], signal['signal_after_qpc']
    need(type(before) is int and type(after) is int and
         value['waiting_before_qpc'] <= before <= after < value['ready_deadline_qpc'] and
         before <= value['ready_after_qpc'], 'owner-ready signal clock bracket')
    # A native waiter can wake before the supervisor samples SetEvent-after.
    need(source['token'] == handoff['token'] == token and
         source['readiness_observed_qpc'] == value['source_observed_qpc'] and
         source['clock'] == 'win32-query-performance-counter' and
         source['sequence']['qpc_frequency'] == handoff['qpc_frequency'] == f and
         handoff['observer_begin_qpc'] == value['journal_begin_qpc'] and
         handoff['freeze_qpc'] == value['freeze_qpc'], 'owner-ready exact paired headers')
    phase_end = source['sequence']['begin_qpc'] + 91*f//10
    need(source['sequence']['begin_qpc'] == value['source_observed_qpc'] + 40*f and
         phase_end <= value['freeze_qpc'] < value['journal_end_qpc'], 'owner-ready actual cut inside horizon')
    return dict(owner_ready_assertion_validated=True, scene_proven=False, fps_proven=False,
                source_observed_qpc=value['source_observed_qpc'], freeze_qpc=value['freeze_qpc'])
