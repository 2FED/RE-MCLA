"""Separate offline timing join against a strictly closed handoff frontier.

The complete earlier timing verdict is retained, not upgraded or overwritten.
Only explicit IDs select groups; clocks reject edges, never create lineage.
No I/O, timestamp adjustment, native admission or simulation/FPS conclusion.
"""
import copy
import hashlib

import mcla_update_timing_decode as timing
import mcla_frame_handoff_closed_frontier as frontier


def validate_update_timing_frontier(timing_raw, handoff_raw, prefix_raw,
                                   phase_raw, swap_raw, source_receipt, token):
    baseline = timing.validate_update_timing(timing_raw, handoff_raw, prefix_raw,
                                           phase_raw, swap_raw, source_receipt, token)
    closed = frontier.validate_closed_frontier(handoff_raw, prefix_raw, phase_raw,
                                              swap_raw, source_receipt, token)
    data = timing.handoff._parse(timing_raw, timing.MAX_BYTES)
    issues, contexts = timing._metadata(data, token)
    phases = timing._cross_references(data, closed)
    issues.update('closed:' + value for value in closed['candidate_blocking_issues'])
    contexts.update('closed:' + value for value in closed['retained_context_issues'])
    grouped = timing._timing_join(data, closed, phases, issues, contexts)
    result = dict.fromkeys(timing.FALSE_FLAGS | {
        'update_causality_proven', 'causal_join_proven', 'display_proven',
        'source_timing_join_proven', 'closed_frontier_proven'}, False)
    result.update(schema='mcla-update-timing-frontier-analysis-v1', token=token,
        status='UPDATE_TIMING_FRONTIER_DIAGNOSTIC_INCONCLUSIVE' if issues or contexts else
               'UPDATE_TIMING_FRONTIER_DIAGNOSTIC_REVIEW_PENDING',
        timing_raw_sha256=hashlib.sha256(timing_raw).hexdigest(),
        timing_inventory=copy.deepcopy(data), baseline_analysis=copy.deepcopy(baseline),
        closed_handoff_analysis=copy.deepcopy(closed), candidate_blocking_issues=sorted(issues),
        retained_context_issues=sorted(contexts), physical_acceptance_pending=True,
        interpretation='EXPLICIT_UPDATE_OBSERVATIONS_BEFORE_STRICT_FRONTIER_NOT_ACTUAL_CONSUMER_OR_SIMULATION_PROOF')
    result.update(copy.deepcopy(grouped))
    return result
