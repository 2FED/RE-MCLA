#!/usr/bin/env python3
"""Analyze private readback-cost samples without claiming scene equivalence.

Only standard-library code is used. Input files are read, never modified; JSON
is printed to stdout only after every requested input has been validated.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


MAX_INPUT_BYTES = 64 * 1024
UINT64_MAX = (1 << 64) - 1
TITLE_ID = 0x545407F8
VULKAN_BACKEND = 2
MIN_DURATION_US = 19_500_000
MAX_DURATION_US = 25_000_000
LEGACY_SAMPLE_SCHEMA = "mcla-readback-cost-sample-v1"
SAMPLE_SCHEMA = "mcla-readback-cost-sample-v2"

LEGACY_COUNTER_FIELDS = (
    "guest_output_sequence",
    "guest_vblank_sequence",
    "frames_closed",
    "draw_packet_count",
    "draw_packet_us",
    "primary_buffer_count",
    "primary_buffer_us",
    "resolve_count",
    "resolve_us",
    "resolve_readback_copy_count",
    "resolve_readback_bytes",
    "resolve_readback_copy_us",
    "resolve_readback_wait_count",
    "resolve_readback_wait_us",
    "watch_arm_calls",
    "watch_arm_us",
    "watch_disarm_calls",
    "watch_disarm_us",
    "non_mmio_write_fault_count",
    "non_mmio_write_fault_us",
    "watch_protect_failures",
    "binding_update_count",
    "binding_update_us",
)
GAP_FIELDS = ("watch_arm_gap_splits", "watch_disarm_safe_gap_splits",
              "watch_disarm_gap_lock_misses")
COUNTER_FIELDS = LEGACY_COUNTER_FIELDS + GAP_FIELDS
METADATA_KEYS = {
    "schema",
    "diagnostic_only",
    "audio_counters_available",
    "edge",
    "mode",
    "monotonic_us",
    "title_id",
    "backend",
}
EXPECTED_KEYS = frozenset(COUNTER_FIELDS) | METADATA_KEYS
LEGACY_EXPECTED_KEYS = frozenset(LEGACY_COUNTER_FIELDS) | METADATA_KEYS


class AnalysisError(ValueError):
    """The input cannot support the requested diagnostic calculation."""


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise AnalysisError(f"Duplicate JSON field: {key!r}")
        result[key] = value
    return result


def _reject_constant(value: str) -> None:
    raise AnalysisError(f"Non-JSON numeric constant: {value}")


def _integer_token(value: str) -> int:
    # Bound conversion independently of Python's configurable large-int limit.
    # Twenty digits suffice for every unsigned 64-bit serializer value.
    if len(value) > 20:
        raise AnalysisError("Integer token exceeds the serializer's range")
    return int(value)


def _unsigned(value: Any, name: str, maximum: int = UINT64_MAX) -> int:
    # bool is an int subclass in Python, but it is not a diagnostic counter.
    if type(value) is not int or value < 0 or value > maximum:
        raise AnalysisError(f"{name} must be an unsigned integer in range")
    return value


def _validate_sample(sample: Any, edge: str) -> dict[str, Any]:
    if type(sample) is not dict:
        raise AnalysisError(f"{edge} sample has missing, extra, or invalid fields")
    schema = sample.get("schema")
    if schema not in (SAMPLE_SCHEMA, LEGACY_SAMPLE_SCHEMA):
        raise AnalysisError(f"{edge} sample has an unsupported schema")
    expected = EXPECTED_KEYS if schema == SAMPLE_SCHEMA else LEGACY_EXPECTED_KEYS
    if set(sample) != expected:
        raise AnalysisError(f"{edge} sample has missing, extra, or invalid fields")
    if sample["diagnostic_only"] is not True:
        raise AnalysisError(f"{edge} sample must be diagnostic-only")
    if sample["audio_counters_available"] is not False:
        raise AnalysisError(f"{edge} sample must explicitly lack audio counters")
    if sample["edge"] != edge:
        raise AnalysisError("Samples must occur exactly in begin, end order")
    if sample["mode"] not in ("fast", "some"):
        raise AnalysisError(f"{edge} sample has an unsupported readback mode")
    if _unsigned(sample["monotonic_us"], "monotonic_us") == 0:
        raise AnalysisError("monotonic_us must be positive")
    if _unsigned(sample["title_id"], "title_id", (1 << 32) - 1) != TITLE_ID:
        raise AnalysisError("The sample title is not MCLA")
    if _unsigned(sample["backend"], "backend", (1 << 32) - 1) != VULKAN_BACKEND:
        raise AnalysisError("The sample backend is not Vulkan")
    for field in COUNTER_FIELDS if schema == SAMPLE_SCHEMA else LEGACY_COUNTER_FIELDS:
        _unsigned(sample[field], field)
    return sample


def analyze_samples(raw: bytes) -> dict[str, Any]:
    """Validate exactly two complete LF-terminated samples and return deltas."""
    if type(raw) is not bytes or len(raw) > MAX_INPUT_BYTES:
        raise AnalysisError("Input must be at most 64 KiB of bytes")
    # The binary-mode serializer emits one LF per record, not CRLF or blank lines.
    if not raw.endswith(b"\n") or raw.count(b"\n") != 2 or b"\r" in raw:
        raise AnalysisError("Expected exactly two complete LF-terminated JSON lines")
    lines = raw[:-1].split(b"\n")
    if len(lines) != 2 or any(not line.strip() for line in lines):
        raise AnalysisError("Expected exactly two nonempty JSON samples")
    samples = []
    for edge, line in zip(("begin", "end"), lines):
        try:
            decoded = json.loads(
                line.decode("utf-8"),
                object_pairs_hook=_unique_object,
                parse_constant=_reject_constant,
                parse_int=_integer_token,
            )
        except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
            raise AnalysisError(f"Invalid {edge} JSON sample") from error
        samples.append(_validate_sample(decoded, edge))

    begin, end = samples
    if begin["schema"] != end["schema"]:
        raise AnalysisError("Sample schema changed inside the interval")
    if begin["mode"] != end["mode"]:
        raise AnalysisError("Readback mode changed between the samples")
    duration_us = end["monotonic_us"] - begin["monotonic_us"]
    if not MIN_DURATION_US <= duration_us <= MAX_DURATION_US:
        raise AnalysisError("Sample duration must be between 19.5 and 25 seconds")
    deltas = {}
    has_gaps = begin["schema"] == SAMPLE_SCHEMA
    for field in COUNTER_FIELDS if has_gaps else LEGACY_COUNTER_FIELDS:
        if end[field] < begin[field]:
            raise AnalysisError(f"Cumulative counter regressed: {field}")
        deltas[field] = end[field] - begin[field]
    outputs = deltas["guest_output_sequence"]
    if outputs == 0:
        raise AnalysisError("No new guest output was published in the interval")

    return {
        "schema": "mcla-readback-cost-analysis-v2",
        "sample_schema": begin["schema"],
        "diagnostic_only": True,
        "visual_validation_required": True,
        "scene_equality_verified": False,
        "sample_process_identity_available": False,
        "audio_counters_available": False,
        "counter_timings_overlap": True,
        "disarm_fragmentation": {
            "available": has_gaps,
            "safe_gap_splits": deltas["watch_disarm_safe_gap_splits"] if has_gaps else None,
            "skipped_metadata_ranges": deltas["watch_disarm_gap_lock_misses"] if has_gaps else None,
            "interval_consistent_failure_free_and_complete": (
                deltas["watch_protect_failures"] == 0
                and deltas["watch_disarm_gap_lock_misses"] == 0
                and deltas["watch_disarm_safe_gap_splits"] <= deltas["watch_disarm_calls"]
            ) if has_gaps else None,
            "estimated_saved_time_available": False,
            "scope": "structural-opportunities-only; independent-cumulative-snapshots; no-protection-policy-change",
        },
        "mode": begin["mode"],
        "title_id": TITLE_ID,
        "backend": VULKAN_BACKEND,
        "begin_monotonic_us": begin["monotonic_us"],
        "end_monotonic_us": end["monotonic_us"],
        "duration_us": duration_us,
        "duration_seconds": duration_us / 1_000_000,
        "deltas": deltas,
        "rates": {
            "guest_outputs_per_second": outputs * 1_000_000 / duration_us,
            "resolve_readback_bytes_per_output": deltas["resolve_readback_bytes"] / outputs,
            "resolve_readback_copy_ms_per_output": (
                deltas["resolve_readback_copy_us"] / 1000 / outputs
            ),
            "draw_packets_per_output": deltas["draw_packet_count"] / outputs,
            "primary_buffers_per_output": deltas["primary_buffer_count"] / outputs,
            "resolves_per_output": deltas["resolve_count"] / outputs,
            "watch_arm_calls_per_output": deltas["watch_arm_calls"] / outputs,
            "watch_disarm_calls_per_output": deltas["watch_disarm_calls"] / outputs,
            "non_mmio_write_faults_per_output": deltas["non_mmio_write_fault_count"] / outputs,
            "watch_protect_failures_per_output": deltas["watch_protect_failures"] / outputs,
        },
    }


def analyze_file(path: Path) -> dict[str, Any]:
    with path.open("rb") as source:
        # Never load an unbounded file before checking its size.
        raw = source.read(MAX_INPUT_BYTES + 1)
    return analyze_samples(raw)


def analyze_paths(paths: list[Path]) -> dict[str, Any]:
    if len(paths) not in (1, 2):
        raise AnalysisError("Provide one sample file or two distinct-mode sample files")
    analyses = [analyze_file(path) for path in paths]
    if len(analyses) == 1:
        return analyses[0]
    if {analysis["mode"] for analysis in analyses} != {"fast", "some"}:
        raise AnalysisError("Paired analysis requires one fast arm and one some arm")
    by_mode = {analysis["mode"]: analysis for analysis in analyses}
    return {
        "schema": "mcla-readback-cost-pair-analysis-v1",
        "diagnostic_only": True,
        "visual_validation_required": True,
        "scene_equality_verified": False,
        "same_artifacts_verified": False,
        "same_profile_and_settings_verified": False,
        "audio_counters_available": False,
        "arms": {mode: by_mode[mode] for mode in ("fast", "some")},
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", nargs="+", type=Path, help="completed samples.jsonl file(s)")
    arguments = parser.parse_args(argv)
    try:
        result = analyze_paths(arguments.samples)
    except (AnalysisError, OSError) as error:
        parser.exit(2, f"Readback-cost analysis rejected: {error}\n")
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
