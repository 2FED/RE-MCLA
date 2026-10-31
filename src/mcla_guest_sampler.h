#pragma once

// KI-035 F10 guest-thread sampler (diagnostic only). For 10 s after an
// accepted F10 it reads every running guest thread's innermost guest function
// (PPCContext::current_function) and last kernel import
// (PPCContext::last_import) at about 500 Hz and logs one line per thread:
//
//   MCLA_GUEST_SAMPLE v=1 thread='<name>' tid=<host id> handle=<guest handle>
//     samples=<n> fn=<top 12> last_import=<histogram>
//
// thread is the guest object name, or the host thread name ("XThreadNNNN
// (F80004E0)") when the guest gave none; tid joins the line with the
// native_threads table of state.json and MCLA_THREAD_CPU (M10-001).
//
// See docs/evidence/M9-006-ki-035-freeze-analysis-20261029.md. The histogram
// and line formatting below are pure and unit-tested
// (tests/mcla_guest_sampler_test.cpp); the thread walk is in
// mcla_guest_sampler.cpp.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rex::system {
class KernelState;
}

namespace mcla::guest_sampler {

inline constexpr size_t kTopFunctions = 12;
inline constexpr int kSampleSeconds = 10;
inline constexpr int kSampleIntervalMs = 2;

// Per-thread sample histogram. Imports are keyed by the static name pointer
// while sampling (cheap); FormatLine merges equal names.
struct Histogram {
  std::string name;
  uint32_t tid = 0;
  uint32_t handle = 0;
  uint32_t samples = 0;
  std::unordered_map<uint32_t, uint32_t> functions;
  std::unordered_map<const char *, uint32_t> imports;

  void Add(uint32_t function, const char *import) {
    ++samples;
    ++functions[function];
    ++imports[import];
  }
};

// " XXXXXXXX:count" for the `limit` most frequent functions, most frequent
// first, ties by ascending address (deterministic output).
inline std::string FormatTopFunctions(
    const std::unordered_map<uint32_t, uint32_t> &functions, size_t limit) {
  std::vector<std::pair<uint32_t, uint32_t>> top(functions.begin(), functions.end());
  std::sort(top.begin(), top.end(), [](const auto &a, const auto &b) {
    return a.second != b.second ? a.second > b.second : a.first < b.first;
  });
  std::string text;
  for (size_t k = 0; k < top.size() && k < limit; ++k) {
    char item[32];
    std::snprintf(item, sizeof(item), " %08X:%u", top[k].first, top[k].second);
    text += item;
  }
  return text;
}

// " name:count" per distinct import name ("none" for no import yet), most
// frequent first, ties by name.
inline std::string FormatImports(const std::unordered_map<const char *, uint32_t> &imports) {
  std::map<std::string, uint32_t> merged;
  for (const auto &[name, count] : imports) {
    merged[name ? std::string(name) : std::string("none")] += count;
  }
  std::vector<std::pair<std::string, uint32_t>> sorted(merged.begin(), merged.end());
  std::stable_sort(sorted.begin(), sorted.end(),
                   [](const auto &a, const auto &b) { return a.second > b.second; });
  std::string text;
  for (const auto &[name, count] : sorted) {
    text += ' ';
    text += name;
    text += ':';
    text += std::to_string(count);
  }
  return text;
}

// Thread names come from the guest; keep the line single and parseable.
inline std::string SanitizeName(std::string_view name) {
  std::string text;
  for (const char c : name.substr(0, std::min<size_t>(name.size(), 64))) {
    text.push_back(c >= 32 && c < 127 && c != '\'' ? c : '?');
  }
  return text;
}

// The complete log line, or an empty string for a thread without samples.
inline std::string FormatLine(const Histogram &histogram) {
  if (!histogram.samples) {
    return {};
  }
  char ids[48];
  std::snprintf(ids, sizeof(ids), "' tid=%u handle=%08X", histogram.tid, histogram.handle);
  return "MCLA_GUEST_SAMPLE v=1 thread='" + SanitizeName(histogram.name) + ids +
         " samples=" + std::to_string(histogram.samples) +
         " fn=" + FormatTopFunctions(histogram.functions, kTopFunctions) +
         " last_import=" + FormatImports(histogram.imports);
}

// Starts a sampling window (restarting a running one). Null is ignored.
void Start(rex::system::KernelState *kernel);
// Stops and joins the sampler; the partial window is still logged. Idempotent.
void Stop();

}  // namespace mcla::guest_sampler
