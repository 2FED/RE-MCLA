"""Pure acquired-mailbox publication witnesses, never animation timing.

The pinned SDK records refresh_cpu_end before its publication release-CAS and
acquire_after after acquiring that exact source. Thus the CAS is enclosed by
those two host-QPC witnesses. Neither is a simulation or display timestamp.
Caller-supplied execution scope is a claim, not independently verified identity.
No IO, collection, scene selection, interpolation or CycleEvidence export.
"""
import copy
import re

import mcla_game_source_scout_decode as scout

EXECUTION_KEYS = {'pid', 'birth_100ns', 'host_sha256'}
FALSE_FLAGS = scout.FALSE_FLAGS | {'execution_scope_independently_verified',
                                 'publication_windows_independently_reviewed'}
NATIVE_ADMITTED = False


def _execution(value):
    scout.exact(value, EXECUTION_KEYS, 'exact caller execution scope')
    scout.uint(value['pid'], 1, 2**32 - 1)
    scout.uint(value['birth_100ns'], 1, 2**64 - 1)
    digest = value['host_sha256']
    scout.need(type(digest) is str and re.fullmatch('[0-9A-F]{64}', digest),
               'uppercase host SHA256')
    return dict(value)


def analyze(receipt, *, token, image_names, read_image, execution):
    """Validate all metadata/inventory/pixels, then report exactly91 slots.

    Duplicate sources use the FIRST observed acquire_after upper witness. Their
    semantic publication elapsed is exactly zero, not a later checkout event.
    Distinct-source interval differences retain signed overlap uncertainty.
    Sequence jumps count unobserved source IDs, not missing collector slots or
    unobserved animation cycles. The injected reader is delegated to the
    streaming scout decoder; original caller metadata cannot affect later rows.
    """
    scope = _execution(execution)
    scout.validate_metadata(receipt, token=token)
    # Validation limits this to the fixed scalar/dict/list wire schema. Freeze
    # before invoking any caller reader; it may mutate every original input.
    frozen = copy.deepcopy(receipt)
    decoded = scout.decode(frozen, token=token, image_names=image_names,
                           read_image=read_image)
    census = decoded['census']
    images = {row['slot']: row for row in decoded['images']}
    publications, rows, gaps = {}, [], []
    previous = None
    unsampled_total = 0
    for slot, observation in zip(frozen['sequence']['slots'], census['observations']):
        index = slot['slot']
        row = {'slot': index, 'status': slot['status'], 'reason': slot['reason'],
               'due_qpc': slot['due_qpc'], 'observed_qpc': slot['observed_qpc'],
               'source_binding': None, 'publication_qpc_bounds': None,
               'first_observed_slot': None, 'duplicate_source': None,
               'bmp_sha256': None, 'rgb_sha256': None}
        if slot['status'] == 'COLLECTED':
            capture = slot['capture']
            identity = (capture['presenter_epoch'], capture['acquired_sequence'])
            duplicate = identity in publications
            if not duplicate:
                publications[identity] = (capture['refresh_cpu_end_qpc'],
                                          capture['acquire_after_qpc'], index)
            lower, upper, first = publications[identity]
            row.update(source_binding={'token': token, **scope,
                                       'presenter_epoch': identity[0],
                                       'acquired_sequence': identity[1]},
                       publication_qpc_bounds=[lower, upper],
                       first_observed_slot=first, duplicate_source=duplicate,
                       bmp_sha256=images[index]['sha256'],
                       rgb_sha256=observation['rgb_sha256'])
            if previous is not None:
                old = previous['publication_qpc_bounds']
                same = identity[1] == previous['source_binding']['acquired_sequence']
                difference = [lower - old[1], upper - old[0]]
                skipped_ids = 0 if same else identity[1] - previous['source_binding']['acquired_sequence'] - 1
                unsampled_total += skipped_ids
                gaps.append({'from_slot': previous['slot'], 'to_slot': index,
                             'uncollected_slots_between': index - previous['slot'] - 1,
                             'unsampled_source_ids_between': skipped_ids,
                             'same_publication': same,
                             'interval_difference_ticks_bounds': difference,
                             'publication_elapsed_ticks_bounds': [0, 0] if same else list(difference),
                             'witness_windows_overlap_or_touch': difference[0] <= 0})
            previous = row
        rows.append(row)
    result = {'schema': 'mcla-source-publication-windows-v1',
              'status': 'MAILBOX_PUBLICATION_WITNESSES_REVIEW_PENDING',
              'interpretation': 'MAILBOX_RELEASE_CAS_WITNESSES_NOT_SIMULATION_DISPLAY_OR_PIXEL_AGE',
              'execution_scope_claim': scope,
              'token': token, 'qpc_frequency': census['qpc_frequency'],
              'scheduled_slots': scout.SLOTS, 'counts': dict(census['counts']),
              'uncollected_slots': scout.SLOTS - census['counts']['COLLECTED'],
              'unique_sources': census['unique_sources'],
              'duplicate_sources': census['duplicate_sources'],
              'unsampled_source_ids_between_observed_endpoints': unsampled_total,
              'observations': tuple(rows), 'adjacent_selected_publications': tuple(gaps),
              'limitations': (
                  'Execution/process/session ownership requires independent caller review.',
                  'Bounds witness mailbox publication, not pixel origin, phase, simulation or scanout.',
                  'Unsampled source IDs do not identify their pixels or animation cycles.',
                  'No cadence regularity, FPS, scene or legacy cycle transfer is admitted.')}
    result.update(dict.fromkeys(FALSE_FLAGS, False))
    return result
