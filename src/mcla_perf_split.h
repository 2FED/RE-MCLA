#pragma once

// M10-001 host side of the command-processor cost split (owner-internal l53;
// docs/evidence/M10-001-harness-design-20261031.md section 2.7):
//
// - MCLA_PERF_SPLIT_DEFAULT builds set the SDK cvar d3d12_perf_split=1 at
//   graphics-plugin load unless the user set it (log once).
// - Each accepted F10 (and the other live-snapshot triggers) marks the split
//   so the SDK emits an MCLA_PERF_SPLIT reason=<source> line.
// - The guest render thread's GPU-serial wait (sub_82411E98, and
//   sub_82412F98 polls outside it) is timed into the SDK sink.
// - state.json gains a "perf_split" object (cumulative totals).
// - One MCLA_THREAD_CPU v=1 line per split window with the top threads'
//   CPU time:
//
//   MCLA_THREAD_CPU v=1 reason=<r> seq=<n> wall_ms=<ms> threads=<n> top=<k>
//     thread=<tid>,'<name>',<user_ms>,<kernel_ms> ... (k entries, one line)
//
// Everything in this header is pure and unit-tested
// (tests/mcla_perf_split_test.cpp); the SDK calls are in mcla_perf_split.cpp.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mcla::perf_split {

inline constexpr std::string_view kPerfSplitCvar = "d3d12_perf_split";
inline constexpr size_t kThreadCpuTop = 8;

enum class DefaultAction {
  kDisabledByOption,  // build without MCLA_PERF_SPLIT_DEFAULT
  kUnavailable,       // SDK without d3d12_perf_split
  kUserSet,           // the user set the cvar; left alone
  kEnable,            // write d3d12_perf_split=1
};

constexpr DefaultAction DecideDefault(bool option_on, bool sdk_has_cvar, bool user_set) {
  if (!option_on) {
    return DefaultAction::kDisabledByOption;
  }
  if (!sdk_has_cvar) {
    return DefaultAction::kUnavailable;
  }
  return user_set ? DefaultAction::kUserSet : DefaultAction::kEnable;
}

struct ThreadCpuSample {
  uint32_t id = 0;
  uint64_t user_100ns = 0;
  uint64_t kernel_100ns = 0;
  std::string name;
};

// Thread names come from the host and the guest; keep the line single and
// parseable (no quotes, commas or control characters, at most 48 chars).
inline std::string SanitizeThreadName(std::string_view name) {
  std::string text;
  for (const char c : name.substr(0, std::min<size_t>(name.size(), 48))) {
    text.push_back(c >= 32 && c < 127 && c != '\'' && c != ',' ? c : '?');
  }
  return text;
}

inline std::string FormatMs(double value) {
  char text[32];
  std::snprintf(text, sizeof(text), "%.1f", value);
  return text;
}

// CPU time of each thread in `current` since `previous` (a thread missing
// from `previous` is new in this window and counts from zero). The `top`
// threads by total CPU, ties by ascending id.
inline std::string FormatThreadCpuLine(std::string_view reason, uint64_t seq, double wall_ms,
                                       const std::vector<ThreadCpuSample> &previous,
                                       const std::vector<ThreadCpuSample> &current,
                                       size_t top = kThreadCpuTop) {
  std::unordered_map<uint32_t, const ThreadCpuSample *> before;
  for (const auto &sample : previous) {
    before[sample.id] = &sample;
  }
  struct Delta {
    uint32_t id;
    uint64_t user;
    uint64_t kernel;
    const std::string *name;
  };
  std::vector<Delta> deltas;
  deltas.reserve(current.size());
  for (const auto &sample : current) {
    uint64_t user = sample.user_100ns;
    uint64_t kernel = sample.kernel_100ns;
    if (const auto it = before.find(sample.id); it != before.end()) {
      user = user >= it->second->user_100ns ? user - it->second->user_100ns : 0;
      kernel = kernel >= it->second->kernel_100ns ? kernel - it->second->kernel_100ns : 0;
    }
    deltas.push_back({sample.id, user, kernel, &sample.name});
  }
  std::sort(deltas.begin(), deltas.end(), [](const Delta &a, const Delta &b) {
    const uint64_t ta = a.user + a.kernel;
    const uint64_t tb = b.user + b.kernel;
    return ta != tb ? ta > tb : a.id < b.id;
  });
  const size_t count = std::min(top, deltas.size());
  std::string line = "MCLA_THREAD_CPU v=1 reason=" + std::string(reason) +
                     " seq=" + std::to_string(seq) + " wall_ms=" + FormatMs(wall_ms) +
                     " threads=" + std::to_string(current.size()) +
                     " top=" + std::to_string(count);
  for (size_t i = 0; i < count; ++i) {
    const Delta &d = deltas[i];
    line += " thread=" + std::to_string(d.id) + ",'" + SanitizeThreadName(*d.name) + "'," +
            FormatMs(double(d.user) / 10000.0) + "," + FormatMs(double(d.kernel) / 10000.0);
  }
  return line;
}

// One JSON object from already-rendered values (numbers, or JSON strings
// with their quotes). Keys are ASCII identifiers chosen by the caller.
inline std::string FormatJsonObject(const std::vector<std::pair<std::string, std::string>> &fields) {
  std::string json = "{";
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i) {
      json += ", ";
    }
    json += "\"" + fields[i].first + "\": " + fields[i].second;
  }
  json += "}";
  return json;
}

// Milliseconds with three decimals; non-finite values become 0.
inline std::string JsonMs(double value) {
  if (!(value == value) || value > 1e15 || value < -1e15) {
    value = 0.0;
  }
  char text[48];
  std::snprintf(text, sizeof(text), "%.3f", value);
  return text;
}

// Starts the split at graphics-plugin load (see DecideDefault) and the
// MCLA_THREAD_CPU worker. Safe to call once; no effect without SDK support.
void OnGraphicsPluginLoaded();
// Requests a reason=<source> line at the next frame close (accepted F10).
void Mark(std::string_view source);
// The state.json "perf_split" object, or empty when the SDK has no split or
// no window was emitted yet.
std::string StateJson();
// Stops the MCLA_THREAD_CPU worker. Idempotent.
void Shutdown();

}  // namespace mcla::perf_split
