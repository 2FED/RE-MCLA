"""Pure, deliberately incomplete semaphore-credit accounting; never native proof.

The caller supplies a complete object-epoch journal starting at creation. Epoch
identity, OS coverage, operation brackets and contexts are assertions in that
journal, not independently established by this reader. Sufficient rules cover
disjoint single-credit operations and isolated overlap/touch components with
exactly one successful one-credit release and one successful wait, entering
with zero credits. They refuse FIFO and nearest-time/latest-generation matches.
Update IDs are {thread_epoch, serial}; their causal/coordinator binding remains
outside this contract. No IO, clocks, handles, game memory, native imports or
input injection occur.
"""
import copy
import hashlib
import json
import re
from mcla_history_limits import (LEGACY_SEMAPHORE_CAPACITY, SEMAPHORE_MAX_BYTES,
                                 semaphore_capacity)

MAX_BYTES = SEMAPHORE_MAX_BYTES
CAPACITY = LEGACY_SEMAPHORE_CAPACITY
MAX_EPOCHS = 64
U32, U64, MAX_QPC = 2**32 - 1, 2**64 - 1, 2**63 - 1
MAX_COUNT = 2**31 - 1
FALSE_FLAGS = {'native_admitted', 'simulation_proven', 'render_proven',
               'fps_proven', 'frame_to_step_mapping_proven',
               'object_identity_proven', 'object_lifetime_proven'}
TOP_KEYS = FALSE_FLAGS | {'schema', 'token', 'clock', 'qpc_frequency',
                         'begin_qpc', 'end_qpc_exclusive', 'capacity',
                         'history_complete', 'observer_failed', 'counts',
                         'epochs', 'operations'}
EPOCH_KEYS = {'epoch_id', 'object_cookie', 'creation_qpc', 'destroy_qpc',
              'initial_count', 'maximum_count', 'origin', 'history_complete'}
OP_KEYS = {'op_id', 'epoch_id', 'native_thread_id', 'guest_handle',
           'entry_qpc', 'exit_qpc', 'kind', 'payload'}
COUNT_KEYS = {'observed_operations', 'stored_operations',
              'completed_operations', 'overflow_operations'}


def need(ok, reason):
    if not ok:
        raise ValueError(reason)


def exact(value, keys, label):
    need(type(value) is dict and set(value) == keys, label)


def uint(value, high=U64, low=0):
    need(type(value) is int and low <= value <= high, 'bounded exact integer')
    return value


def boolean(value):
    need(type(value) is bool, 'exact boolean')


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        need(key not in result, 'duplicate JSON key')
        result[key] = value
    return result


def _integer(text):
    need(len(text) <= 20 and text.isascii() and text.isdecimal(),
         'bounded unsigned JSON integer')
    return int(text)


def _no_float(_):
    raise ValueError('floating/NaN/Infinity JSON value unsupported')


def _epoch(value):
    exact(value, EPOCH_KEYS, 'exact object epoch schema')
    uint(value['epoch_id'], low=1)
    uint(value['object_cookie'], low=1)
    uint(value['creation_qpc'], MAX_QPC)
    if value['destroy_qpc'] is not None:
        uint(value['destroy_qpc'], MAX_QPC)
    maximum = uint(value['maximum_count'], MAX_COUNT, 1)
    uint(value['initial_count'], maximum)
    need(type(value['origin']) is str and value['origin'] in
         {'fresh', 'native', 'restored', 'unknown'}, 'epoch origin')
    boolean(value['history_complete'])


def _operation(value, epochs):
    exact(value, OP_KEYS, 'exact operation schema')
    uint(value['op_id'], low=1)
    uint(value['epoch_id'], low=1)
    need(value['epoch_id'] in epochs, 'operation references unknown epoch')
    uint(value['native_thread_id'], U32, 1)
    # Ke/native-object access need not carry a guest handle. Object epoch, not
    # this optional access-path annotation, is the accounting identity.
    if value['guest_handle'] is not None:
        uint(value['guest_handle'], U32, 1)
    uint(value['entry_qpc'], MAX_QPC)
    uint(value['exit_qpc'], MAX_QPC)
    kind = value['kind']
    need(type(kind) is str and re.fullmatch('[a-z][a-z_]{0,31}', kind),
         'bounded operation kind')
    payload = value['payload']
    if kind == 'release':
        exact(payload, {'count', 'success', 'previous_count', 'context'},
              'exact release schema')
        uint(payload['count'], MAX_COUNT, 1)
        boolean(payload['success'])
        if payload['previous_count'] is not None:
            uint(payload['previous_count'], epochs[value['epoch_id']]['maximum_count'])
        need(not payload['success'] or payload['previous_count'] is not None,
             'successful release requires previous count')
        need(payload['success'] or payload['previous_count'] is None,
             'failed release previous count must be null (undefined output)')
        context = payload['context']
        exact(context, {'kind', 'generation'}, 'exact release context')
        need(type(context['kind']) is str and context['kind'] in
             {'update', 'control', 'unknown'}, 'release context kind')
        if context['kind'] == 'update':
            exact(context['generation'], {'thread_epoch', 'serial'}, 'exact update ID pair')
            uint(context['generation']['thread_epoch'], low=1)
            uint(context['generation']['serial'], low=1)
        else:
            need(context['generation'] is None, 'non-update has no generation')
    elif kind == 'wait':
        exact(payload, {'result', 'timeout_kind'}, 'exact single-wait schema')
        need(type(payload['result']) is str and payload['result'] in
             {'success', 'timeout', 'apc', 'alerted', 'abandoned', 'failed', 'unknown'},
             'single-wait result')
        need(type(payload['timeout_kind']) is str and payload['timeout_kind'] in
             {'zero', 'finite', 'infinite'}, 'wait timeout kind')
    else:
        # Unknown kinds are visible unsupported evidence, never silently ignored.
        exact(payload, set(), 'unsupported operation carries no interpreted payload')


def audit(data, *, token, begin_qpc, end_qpc, qpc_frequency):
    """Validate immutable JSON bytes and return conservative candidate evidence.

    Malformed schemas/types/bounds/counters raise ValueError. Well-shaped but
    insufficient/contradictory evidence returns INCONCLUSIVE with no candidates.
    QPC sorting establishes strict disjoint component order, not order within an
    overlap/touch component. The only admitted nonsingleton component is a zero-
    balance, single successful release(count=1, previous=0) and successful wait.
    Complete history leaves that wait only one possible credit; success forces
    release-before-consumption without asserting either linearization timestamp.
    A diagnostic issue anywhere suppresses all associations, including earlier
    apparently clean pairs. This reader never establishes native completeness.
    """
    need(type(data) is bytes and 0 < len(data) <= MAX_BYTES, 'bounded immutable JSON bytes')
    need(type(token) is str and re.fullmatch('[0-9a-f]{32}', token), 'expected token')
    uint(begin_qpc, MAX_QPC, 1)
    uint(end_qpc, MAX_QPC, 1)
    uint(qpc_frequency, MAX_QPC, 1)
    need(begin_qpc < end_qpc, 'positive expected window')
    try:
        source = json.loads(data.decode('utf-8'), object_pairs_hook=_pairs,
                            parse_int=_integer, parse_float=_no_float,
                            parse_constant=_no_float)
    except (UnicodeError, RecursionError) as error:
        raise ValueError('invalid or excessively nested JSON') from error
    exact(source, TOP_KEYS, 'exact journal schema')
    need(source['schema'] == 'mcla-semaphore-credit-journal-v1', 'supported schema')
    need(source['token'] == token, 'journal token binding')
    need(source['clock'] == 'win32-query-performance-counter', 'QPC clock domain')
    for key, expected in (('begin_qpc', begin_qpc), ('end_qpc_exclusive', end_qpc),
                          ('qpc_frequency', qpc_frequency)):
        uint(source[key])
        need(source[key] == expected, 'exact journal clock/capacity binding')
    capacity = semaphore_capacity(source['capacity'])
    for key in FALSE_FLAGS:
        need(source[key] is False, 'proof/admission flag must be false')
    boolean(source['history_complete'])
    boolean(source['observer_failed'])
    epoch_rows, operations = source['epochs'], source['operations']
    need(type(epoch_rows) is list and 0 < len(epoch_rows) <= MAX_EPOCHS,
         'bounded nonempty epochs')
    need(type(operations) is list and len(operations) <= capacity, 'bounded operations')
    epochs = {}
    for epoch in epoch_rows:
        _epoch(epoch)
        need(epoch['epoch_id'] not in epochs, 'duplicate object epoch')
        epochs[epoch['epoch_id']] = epoch
    op_ids = set()
    for operation in operations:
        _operation(operation, epochs)
        need(operation['op_id'] not in op_ids, 'duplicate operation ID')
        op_ids.add(operation['op_id'])
    counts = source['counts']
    exact(counts, COUNT_KEYS, 'exact operation counts')
    for number in counts.values():
        uint(number)
    stored, observed, completed, overflow = (counts[key] for key in
        ('stored_operations', 'observed_operations', 'completed_operations', 'overflow_operations'))
    need(stored == len(operations) and observed == stored + overflow and
         stored <= completed <= observed, 'operation counter consistency')

    issues = set()
    def issue(code, epoch_id=None):
        issues.add(code if epoch_id is None else f'EPOCH_{epoch_id}:{code}')
    if not source['history_complete']:
        issue('JOURNAL_HISTORY_INCOMPLETE')
    if source['observer_failed']:
        issue('OBSERVER_FAILED')
    if overflow:
        issue('OPERATION_OVERFLOW')
    if completed != observed:
        issue('OPERATIONS_NOT_DRAINED')
    for epoch in epoch_rows:
        eid = epoch['epoch_id']
        creation, destruction = epoch['creation_qpc'], epoch['destroy_qpc']
        if not begin_qpc <= creation < end_qpc:
            issue('CREATION_OUTSIDE_WINDOW', eid)
        if destruction is not None and not creation < destruction < end_qpc:
            issue('INVALID_DESTRUCTION_BRACKET', eid)
        if epoch['initial_count']:
            issue('INITIAL_CREDITS_NOT_ATTRIBUTABLE', eid)
        if epoch['origin'] != 'fresh':
            issue('NONFRESH_CREATION_OR_RESTORE', eid)
        if not epoch['history_complete']:
            issue('EPOCH_HISTORY_INCOMPLETE', eid)
    # A pointer may be reused only after a witnessed destruction, never by alias.
    for index, left in enumerate(epoch_rows):
        for right in epoch_rows[index + 1:]:
            if left['object_cookie'] != right['object_cookie']:
                continue
            older, newer = sorted((left, right), key=lambda row: row['creation_qpc'])
            if older['destroy_qpc'] is None or older['destroy_qpc'] >= newer['creation_qpc']:
                issue('OBJECT_COOKIE_LIFETIMES_OVERLAP_OR_GAP')

    candidates, consumptions, summaries = [], [], []
    def record_match(eid, release, wait, rule):
        context = release['payload']['context']
        match = {'epoch_id': eid, 'release_op_id': release['op_id'],
                 'wait_op_id': wait['op_id'], 'context': copy.deepcopy(context),
                 'association_rule': rule,
                 'release_qpc_bracket': [release['entry_qpc'], release['exit_qpc']],
                 'wait_qpc_bracket': [wait['entry_qpc'], wait['exit_qpc']]}
        consumptions.append(match)
        if context['kind'] == 'update':
            candidates.append(copy.deepcopy(match))

    for eid, epoch in epochs.items():
        rows = sorted((row for row in operations if row['epoch_id'] == eid),
                      key=lambda row: (row['entry_qpc'], row['op_id']))
        components = []
        component_exit = None
        valid_order = True
        for row in rows:
            entry, exit_ = row['entry_qpc'], row['exit_qpc']
            if not begin_qpc <= entry <= exit_ < end_qpc:
                issue('INVALID_OR_OUTSIDE_OPERATION_BRACKET', eid)
                valid_order = False
            if entry <= epoch['creation_qpc'] or (epoch['destroy_qpc'] is not None and
                                                  exit_ >= epoch['destroy_qpc']):
                issue('OPERATION_TOUCHES_OR_EXCEEDS_LIFETIME', eid)
                valid_order = False
            # Inclusive connected components: a third touching operation must
            # not be peeled off to manufacture a convenient two-operation pair.
            if component_exit is None or entry > component_exit:
                components.append([row])
                component_exit = exit_
            else:
                components[-1].append(row)
                component_exit = max(component_exit, exit_)
            if row['kind'] not in {'release', 'wait'}:
                issue('UNSUPPORTED_OPERATION_' + row['kind'].upper(), eid)
                valid_order = False
        balance, pending = epoch['initial_count'], None
        successful_releases = successful_waits = 0
        if valid_order:
            for component in components:
                if len(component) != 1:
                    releases = [r for r in component if r['kind'] == 'release']
                    waits = [r for r in component if r['kind'] == 'wait']
                    pair = (len(component) == 2 and len(releases) == 1 and len(waits) == 1)
                    if pair:
                        released, waited = releases[0], waits[0]
                        payload = released['payload']
                        pair = (balance == 0 and payload['success'] and
                                payload['count'] == 1 and payload['previous_count'] == 0 and
                                waited['payload']['result'] == 'success')
                    if not pair:
                        issue('OVERLAP_OR_TOUCH_OUTSIDE_MODEL_COVERAGE', eid)
                        valid_order = False
                        break
                    # Exactly one produced credit and one consumption, with no
                    # entering credit or other operation possibly competing.
                    # Brackets can overlap, nest, or touch; no FIFO is assumed.
                    successful_releases += 1
                    successful_waits += 1
                    balance, pending = 0, None
                    record_match(eid, released, waited, 'isolated-overlap-single-credit')
                    continue
                row = component[0]
                payload = row['payload']
                if row['kind'] == 'release':
                    if not payload['success']:
                        continue
                    successful_releases += payload['count']
                    if payload['previous_count'] != balance:
                        issue('PREVIOUS_COUNT_CONTRADICTS_LEDGER', eid)
                    balance += payload['count']
                    if balance > epoch['maximum_count']:
                        issue('SUCCESSFUL_RELEASE_EXCEEDS_MAXIMUM', eid)
                    if balance > 1:
                        issue('COMPETING_OR_BATCH_CREDITS_OUTSIDE_MODEL_COVERAGE', eid)
                        pending = None
                    elif balance == 1:
                        pending = row
                elif payload['result'] == 'success':
                    successful_waits += 1
                    if balance == 0:
                        issue('SUCCESSFUL_WAIT_WITHOUT_CREDIT', eid)
                    else:
                        balance -= 1
                    if pending is not None:
                        record_match(eid, pending, row, 'strict-disjoint-single-credit')
                        pending = None
                elif payload['result'] in {'timeout', 'apc', 'alerted'}:
                    # These outcomes do not consume a semaphore credit. A timeout
                    # with a known available credit contradicts disjoint coverage.
                    if payload['result'] == 'timeout' and balance:
                        issue('TIMEOUT_WITH_AVAILABLE_CREDIT', eid)
                else:
                    issue('UNSUPPORTED_WAIT_RESULT_' + payload['result'].upper(), eid)
        summaries.append({'epoch_id': eid, 'operation_count': len(rows),
                          'ledger_evaluated': valid_order,
                          'successful_release_credits': successful_releases if valid_order else None,
                          'successful_waits': successful_waits if valid_order else None,
                          'ending_count': balance if valid_order else None,
                          'unconsumed_release_op_id': pending['op_id'] if pending else None})
    # Completeness is global in v1: never keep a convenient prefix or unaffected
    # pair when the supplied journal itself is incomplete or contradictory.
    if issues:
        candidates, consumptions = [], []
    if not candidates:
        issue('NO_UNAMBIGUOUS_UPDATE_WAIT_CANDIDATE')
    result = dict.fromkeys(FALSE_FLAGS, False)
    result.update(schema='mcla-semaphore-credit-audit-v1',
                  status=('SEMAPHORE_CREDIT_AUDIT_INCONCLUSIVE' if issues else
                          'SEMAPHORE_CREDIT_CANDIDATES_REVIEW_PENDING'),
                  receipt_sha256=hashlib.sha256(data).hexdigest().upper(),
                  scope_restriction='disjoint-or-isolated-overlap-single-credit-sufficient-rules',
                  diagnostic_issues=sorted(issues), associations=candidates,
                  unique_credit_consumptions=consumptions, epoch_summaries=summaries,
                  validated_operation_count=len(operations))
    return result
