"""Pure in-memory regression cases for the diagnostic-only readback analyzer."""

from __future__ import annotations

import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "scripts" / "analyze-readback-cost.py"
SPEC = importlib.util.spec_from_file_location("readback_cost_analysis", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
analysis = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(analysis)


def sample_pair(mode="fast", duration_us=20_000_000):
    begin = {field: 100 for field in analysis.COUNTER_FIELDS}
    begin.update(
        schema=analysis.SAMPLE_SCHEMA,
        diagnostic_only=True,
        audio_counters_available=False,
        edge="begin",
        mode=mode,
        monotonic_us=1_000_000,
        title_id=analysis.TITLE_ID,
        backend=analysis.VULKAN_BACKEND,
    )
    end = copy.deepcopy(begin)
    end["edge"] = "end"
    end["monotonic_us"] += duration_us
    for field in analysis.COUNTER_FIELDS:
        end[field] += 200
    end["resolve_readback_bytes"] = begin["resolve_readback_bytes"] + 409_600
    end["resolve_readback_copy_us"] = begin["resolve_readback_copy_us"] + 500_000
    end["draw_packet_count"] = begin["draw_packet_count"] + 600_000
    end["resolve_count"] = begin["resolve_count"] + 4000
    end["watch_protect_failures"] = begin["watch_protect_failures"]
    return begin, end


def encode_pair(pair):
    return b"".join(
        json.dumps(sample, separators=(",", ":")).encode("utf-8") + b"\n"
        for sample in pair
    )


class ReadbackCostAnalysisTests(unittest.TestCase):
    def assert_rejected(self, raw):
        with self.assertRaises(analysis.AnalysisError):
            analysis.analyze_samples(raw)

    def test_representative_values_and_limits(self):
        result = analysis.analyze_samples(encode_pair(sample_pair()))
        self.assertEqual(result["duration_us"], 20_000_000)
        self.assertEqual(result["deltas"]["guest_output_sequence"], 200)
        self.assertEqual(result["rates"]["guest_outputs_per_second"], 10)
        self.assertEqual(result["rates"]["resolve_readback_bytes_per_output"], 2048)
        self.assertEqual(result["rates"]["resolve_readback_copy_ms_per_output"], 2.5)
        self.assertEqual(result["rates"]["draw_packets_per_output"], 3000)
        self.assertEqual(result["rates"]["resolves_per_output"], 20)
        self.assertEqual(result["rates"]["watch_arm_calls_per_output"], 1)
        self.assertEqual(result["rates"]["watch_disarm_calls_per_output"], 1)
        self.assertEqual(result["rates"]["non_mmio_write_faults_per_output"], 1)
        self.assertEqual(result["rates"]["watch_protect_failures_per_output"], 0)
        self.assertEqual(set(result["deltas"]), set(analysis.COUNTER_FIELDS))
        self.assertIs(result["diagnostic_only"], True)
        self.assertIs(result["visual_validation_required"], True)
        self.assertIs(result["scene_equality_verified"], False)
        self.assertIs(result["sample_process_identity_available"], False)
        self.assertIs(result["audio_counters_available"], False)
        self.assertNotIn("pass", result)
        self.assertNotIn("improvement", result)

    def test_mode_some(self):
        self.assertEqual(
            analysis.analyze_samples(encode_pair(sample_pair("some")))["mode"], "some"
        )

    def test_duration_boundaries(self):
        for duration in (19_500_000, 25_000_000):
            with self.subTest(duration=duration):
                self.assertEqual(
                    analysis.analyze_samples(encode_pair(sample_pair(duration_us=duration)))[
                        "duration_us"
                    ],
                    duration,
                )
        for duration in (-1, 0, 19_499_999, 25_000_001):
            with self.subTest(duration=duration):
                self.assert_rejected(encode_pair(sample_pair(duration_us=duration)))

    def test_truncated_and_wrong_line_counts(self):
        good = encode_pair(sample_pair())
        for raw in (b"", good[:-1], good[:-5], good + b"\n", good + good,
                    good.splitlines(keepends=True)[0], b"\n\n", good.replace(b"\n", b"\r\n")):
            with self.subTest(raw_length=len(raw)):
                self.assert_rejected(raw)

    def test_invalid_json_and_topology(self):
        good = encode_pair(sample_pair())
        for raw in (good.replace(b"{", b"[", 1), b"[]\n{}\n", b"true\n{}\n",
                    b"null\n{}\n", b"\xff\n{}\n", b"\xef\xbb\xbf" + good):
            with self.subTest(raw=raw[:12]):
                self.assert_rejected(raw)

    def test_duplicate_field_even_with_equal_value(self):
        raw = encode_pair(sample_pair())
        self.assert_rejected(raw.replace(b'"backend":2', b'"backend":2,"backend":2', 1))

    def test_duplicate_escaped_field(self):
        raw = encode_pair(sample_pair())
        self.assert_rejected(
            raw.replace(b'"backend":2', b'"backend":2,"\\u0062ackend":2', 1)
        )

    def test_edge_order_and_injection(self):
        for edge in ("end", "BEGIN", "begin ", "begin\nend", 'begin","extra":true', None):
            pair = sample_pair()
            pair[0]["edge"] = edge
            with self.subTest(edge=edge):
                self.assert_rejected(encode_pair(pair))
        self.assert_rejected(encode_pair(tuple(reversed(sample_pair()))))

    def test_mode_drift_and_invalid_modes(self):
        pair = sample_pair()
        pair[1]["mode"] = "some"
        self.assert_rejected(encode_pair(pair))
        for mode in ("none", "auto", "full", "FAST", "fast\n", None, True, []):
            with self.subTest(mode=mode):
                self.assert_rejected(encode_pair(sample_pair(mode)))

    def test_clock_invalid_types_and_range(self):
        for value in (0, -1, True, 1.0, "1000000", None, analysis.UINT64_MAX + 1):
            pair = sample_pair()
            pair[0]["monotonic_us"] = value
            with self.subTest(value=value):
                self.assert_rejected(encode_pair(pair))

    def test_each_counter_regression(self):
        for field in analysis.COUNTER_FIELDS:
            pair = sample_pair()
            pair[1][field] = pair[0][field] - 1
            with self.subTest(field=field):
                self.assert_rejected(encode_pair(pair))

    def test_each_counter_rejects_bool_float_and_out_of_range(self):
        for field in analysis.COUNTER_FIELDS:
            for value in (False, True, 100.0, -1, "100", None, analysis.UINT64_MAX + 1):
                pair = sample_pair()
                pair[0][field] = value
                with self.subTest(field=field, value=value):
                    self.assert_rejected(encode_pair(pair))

    def test_non_json_numbers(self):
        raw = encode_pair(sample_pair())
        for value in (b"NaN", b"Infinity", b"-Infinity", b"1e999"):
            with self.subTest(value=value):
                self.assert_rejected(raw.replace(b'"frames_closed":100', b'"frames_closed":' + value, 1))

    def test_huge_integer_is_rejected_without_unbounded_conversion(self):
        raw = encode_pair(sample_pair())
        self.assert_rejected(
            raw.replace(b'"frames_closed":100', b'"frames_closed":' + b"9" * 5000, 1)
        )

    def test_backend_and_title(self):
        for field, values in (
            ("backend", (0, 1, 3, True, 2.0, "2")),
            ("title_id", (0, analysis.TITLE_ID + 1, False, float(analysis.TITLE_ID), "545407F8")),
        ):
            for value in values:
                pair = sample_pair()
                pair[1][field] = value
                with self.subTest(field=field, value=value):
                    self.assert_rejected(encode_pair(pair))

    def test_schema_and_literal_flags(self):
        for field, values in (
            ("schema", ("wrong", "mcla-readback-cost-sample-v2", None)),
            ("diagnostic_only", (False, 1, "true")),
            ("audio_counters_available", (True, 0, "false")),
        ):
            for value in values:
                pair = sample_pair()
                pair[0][field] = value
                with self.subTest(field=field, value=value):
                    self.assert_rejected(encode_pair(pair))

    def test_every_missing_key_and_extra_key(self):
        for field in analysis.EXPECTED_KEYS:
            pair = sample_pair()
            del pair[1][field]
            with self.subTest(field=field):
                self.assert_rejected(encode_pair(pair))
        pair = sample_pair()
        pair[0]["unexpected"] = 0
        self.assert_rejected(encode_pair(pair))

    def test_no_output(self):
        pair = sample_pair()
        pair[1]["guest_output_sequence"] = pair[0]["guest_output_sequence"]
        self.assert_rejected(encode_pair(pair))

    def test_zero_counters_except_outputs_are_allowed(self):
        pair = sample_pair()
        for field in analysis.COUNTER_FIELDS:
            pair[0][field] = pair[1][field] = 0
        pair[1]["guest_output_sequence"] = 1
        result = analysis.analyze_samples(encode_pair(pair))
        self.assertEqual(result["rates"]["resolve_readback_copy_ms_per_output"], 0)

    def test_uint64_max_preserves_exact_integer_deltas(self):
        pair = sample_pair()
        for field in analysis.COUNTER_FIELDS:
            pair[0][field] = analysis.UINT64_MAX - 200
            pair[1][field] = analysis.UINT64_MAX
        pair[0]["monotonic_us"] = analysis.UINT64_MAX - 20_000_000
        pair[1]["monotonic_us"] = analysis.UINT64_MAX
        result = analysis.analyze_samples(encode_pair(pair))
        self.assertEqual(set(result["deltas"].values()), {200})
        self.assertEqual(result["duration_us"], 20_000_000)

    def test_size_bound_and_bounded_read(self):
        self.assert_rejected(b" " * (analysis.MAX_INPUT_BYTES + 1))
        source = mock.MagicMock()
        source.__enter__.return_value.read.return_value = b" " * (analysis.MAX_INPUT_BYTES + 1)
        with mock.patch.object(Path, "open", return_value=source):
            with self.assertRaises(analysis.AnalysisError):
                analysis.analyze_file(Path("not-read-from-disk.jsonl"))
        source.__enter__.return_value.read.assert_called_once_with(analysis.MAX_INPUT_BYTES + 1)

    def test_paired_results_require_only_distinct_modes_not_scene_equality(self):
        fast = analysis.analyze_samples(encode_pair(sample_pair("fast")))
        some = analysis.analyze_samples(encode_pair(sample_pair("some", 25_000_000)))
        with mock.patch.object(analysis, "analyze_file", side_effect=(some, fast)):
            result = analysis.analyze_paths([Path("some.jsonl"), Path("fast.jsonl")])
        self.assertEqual(list(result["arms"]), ["fast", "some"])
        self.assertIs(result["scene_equality_verified"], False)
        self.assertIs(result["same_artifacts_verified"], False)
        self.assertIs(result["same_profile_and_settings_verified"], False)
        self.assertNotIn("improvement", result)
        with mock.patch.object(analysis, "analyze_file", return_value=fast):
            with self.assertRaises(analysis.AnalysisError):
                analysis.analyze_paths([Path("a.jsonl"), Path("b.jsonl")])

    def test_cli_one_or_two_files_and_fail_closed_stdout(self):
        for modes in (("fast",), ("fast", "some")):
            stdout = io.StringIO()
            results = [analysis.analyze_samples(encode_pair(sample_pair(mode))) for mode in modes]
            with mock.patch.object(analysis, "analyze_file", side_effect=results):
                with contextlib.redirect_stdout(stdout):
                    self.assertEqual(analysis.main([f"{mode}.jsonl" for mode in modes]), 0)
            self.assertIs(json.loads(stdout.getvalue())["diagnostic_only"], True)

        for failure in (analysis.AnalysisError("malformed"), OSError("unavailable")):
            stdout, stderr = io.StringIO(), io.StringIO()
            good = analysis.analyze_samples(encode_pair(sample_pair()))
            with mock.patch.object(analysis, "analyze_file", side_effect=(good, failure)):
                with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                    with self.assertRaises(SystemExit) as caught:
                        analysis.main(["fast.jsonl", "some.jsonl"])
            self.assertEqual(caught.exception.code, 2)
            self.assertEqual(stdout.getvalue(), "")
            self.assertIn("rejected", stderr.getvalue())

    def test_invalid_argument_count(self):
        for paths in ([], [Path("a"), Path("b"), Path("c")]):
            with self.subTest(count=len(paths)):
                with self.assertRaises(analysis.AnalysisError):
                    analysis.analyze_paths(paths)


if __name__ == "__main__":
    unittest.main(verbosity=2)
