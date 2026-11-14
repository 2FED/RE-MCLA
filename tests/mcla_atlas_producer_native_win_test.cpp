// In-memory tests of the real diagnostic emitter. No game, GPU, files or RAM
// capture. This tests metadata mechanics, not production hook placement.
#ifndef MCLA_DIAGNOSTIC_ATLAS_WITNESS
#error This test requires MCLA_DIAGNOSTIC_ATLAS_WITNESS=1
#endif
#include <rex/graphics/d3d12/mcla_atlas_producer_witness.h>
#include <rex/logging/sink.h>
#include <functional>
#include <iostream>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

namespace {
namespace p = mcla::diagnostics::atlas_producer;
using Witness = rex::graphics::d3d12::AtlasProducerWitness;
unsigned checks = 0, failures = 0, cases = 0;
std::string active;
void Check(bool ok, const char* label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << active << ": " << label << '\n'; }
}
p::Event Event(p::EventKind kind = p::EventKind::ResolveQueued) {
  return {kind, p::kBase, p::kLength, 10, 20, false, 0};
}
void Finish(Witness& w) { w.Finish(1, 1, true, 11, 21); }
bool Failed(const Witness& w) { return w.policy.failure() != p::Failure::None; }
std::vector<std::string> Run(const char* name, const std::function<void()>& body,
                             size_t count, const char* required = nullptr) {
  active = name; ++cases;
  auto sink = std::make_shared<rex::LogCaptureSink>();
  sink->set_pattern("%v");
  rex::AddSink(rex::log::gpu(), sink);
  body();
  rex::FlushLogging();
  std::vector<rex::LogEntry> entries;
  sink->CopyEntries(entries);
  rex::RemoveSink(rex::log::gpu(), sink);
  std::vector<std::string> records;
  size_t bytes = 0;
  for (const auto& entry : entries) {
    if (!entry.text.starts_with("MCLA_ATLAS_PRODUCER ")) continue;
    records.push_back(entry.text); bytes += entry.text.size();
    Check(entry.text.starts_with("MCLA_ATLAS_PRODUCER v=1 seq=" +
        std::to_string(records.size()) + " type="), "contiguous protocol sequence");
    Check(entry.text.size() < p::kProtocolRecordBytes, "record bound");
  }
  Check(records.size() == count, "exact record count");
  Check(records.size() <= p::kProtocolRecordLimit && bytes <= 32768, "aggregate bound");
  if (required) {
    bool found = false;
    for (const auto& record : records) found |= record.find(required) != record.npos;
    Check(found, "required record");
  }
  return records;
}
void Tests() {
  auto zero = Run("explicit-zero", [] {
    Witness w; Check(w.Initialize(), "init");
    auto e = Event(); e.base = 0; e.length = 1; w.Observe(e);
    Finish(w); Check(w.policy.complete() && w.policy.events() == 0, "explicit zero finish");
    w.Observe({}); w.Checkpoint(); // Post-finish ordinary invalid data is ignored.
    Check(w.sequence == 2 && !Failed(w), "post-finish silent");
  }, 2, "type=finish id=1 gen=1 events=0 frame=11 submission=21 complete=1");
  Check(!zero.empty() && zero.front() == "MCLA_ATLAS_PRODUCER v=1 seq=1 type=init enabled=1 base=83492864 length=1048576 events_limit=64 claim=direct_destination_not_authorship", "exact init");

  const char* names[] = {"resolve_queued", "resolve_cpu_copy", "memexport_cpu_full",
                        "memexport_cpu_fast", "declared_gpu_range"};
  auto kinds = Run("five-kinds", [&] {
    Witness w; Check(w.Initialize(), "init");
    for (unsigned i = 0; i < 5; ++i) {
      auto e = Event(static_cast<p::EventKind>(i));
      if (i == 3) { e.source_submission_known = true; e.source_submission = 19; }
      if (i == 4) { e.base = 0; e.length = p::kPhysicalBytes; }
      w.Observe(e);
    }
    Finish(w); Check(w.policy.complete() && w.policy.events() == 5, "five accepted");
  }, 7);
  if (kinds.size() == 7) {
    for (unsigned i = 0; i < 5; ++i)
      Check(kinds[i + 1].find(std::string("kind=") + names[i] + " overlap_base=83492864 overlap_length=1048576") != std::string::npos, "kind and overlap");
    Check(kinds[4].find("source_known=1 source_submission=19 coarse_unknown_range=0") != std::string::npos, "prior slot distinct");
    Check(kinds[5].ends_with("source_known=0 source_submission=0 coarse_unknown_range=1"), "whole-range declaration");
  }
  Run("clipped-maximum-times", [] {
    Witness w; Check(w.Initialize(), "init"); auto e = Event();
    e.base = p::kBase - 1; e.length = 2;
    e.call_frame = e.call_submission = std::numeric_limits<uint64_t>::max();
    w.Observe(e); w.Finish(1, 1, true, e.call_frame, e.call_submission);
    Check(w.policy.complete(), "max decimal fits");
  }, 3, "overlap_base=83492864 overlap_length=1 call_frame=18446744073709551615 call_submission=18446744073709551615");
  Run("64-events", [] {
    Witness w; Check(w.Initialize(), "init");
    for (unsigned i = 0; i < 64; ++i) w.Observe(Event());
    Finish(w); Check(w.policy.complete() && w.sequence == 66, "64 complete");
  }, 66, "type=finish id=1 gen=1 events=64");
  Run("65-events", [] {
    Witness w; Check(w.Initialize(), "init");
    for (unsigned i = 0; i < 65; ++i) w.Observe(Event());
    Finish(w); w.Checkpoint(); w.Fail(2);
    Check(Failed(w) && w.policy.events() == 64, "sticky cap failure");
  }, 66, "type=fail reason=2 events=64 complete=0");
  Run("invalid-range", [] {
    Witness w; Check(w.Initialize(), "init"); auto e = Event(); e.length = 0;
    w.Observe(e); Check(Failed(w), "invalid range failed");
  }, 2, "type=fail reason=2");
  Run("false-source-attribution", [] {
    Witness w; Check(w.Initialize(), "init"); auto e = Event(p::EventKind::ResolveCpuCopy);
    e.source_submission_known = true; e.source_submission = 19;
    w.Observe(e); Check(Failed(w), "resolve origin rejected");
  }, 2, "type=fail reason=2");
  Run("wrong-checkpoint", [] {
    Witness w; Check(w.Initialize(), "init"); w.Finish(1, 2, true, 11, 21);
    Check(Failed(w), "wrong generation");
  }, 2, "type=fail reason=2");
  Run("missing-finish", [] { Witness w; Check(w.Initialize(), "init"); }, 2,
      "type=fail reason=6 events=0 complete=0");
  Run("duplicate-finish", [] {
    Witness w; Check(w.Initialize(), "init"); Finish(w); Finish(w);
    Check(Failed(w), "second finish invalidates");
  }, 3, "type=fail reason=2");
  Run("foreign-thread", [] {
    Witness w; Check(w.Initialize(), "init");
    std::thread other([&] { w.Observe(Event()); }); other.join();
    Check(w.foreign.load() && w.policy.events() == 0 && w.sequence == 1, "foreign only marks fault");
    w.Checkpoint(); Check(Failed(w), "owner emits fault");
  }, 2, "type=fail reason=5");
  Run("foreign-without-checkpoint", [] {
    Witness w; Check(w.Initialize(), "init");
    std::thread other([&] { w.Observe(Event()); }); other.join();
  }, 2, "type=fail reason=6");
  Run("post-finish-thread", [] {
    Witness w; Check(w.Initialize(), "init"); Finish(w);
    std::thread other([&] { w.Observe(Event()); }); other.join();
    w.Checkpoint(); Check(w.policy.complete() && !w.foreign.load(), "outside census ignored");
  }, 2);
  Run("reentrant", [] {
    Witness w; Check(w.Initialize(), "init");
    w.Visit([&] { w.Observe(Event()); });
    Check(Failed(w) && w.policy.events() == 0, "nested visit rejects");
  }, 2, "type=fail reason=5");
  Run("body-exception", [] {
    Witness w; Check(w.Initialize(), "init");
    w.Visit([] { throw 1; }); Check(Failed(w), "exception contained");
  }, 2, "type=fail reason=7");
  Run("format-overflow", [] {
    Witness w; Check(w.Initialize(), "init");
    std::string huge(384, 'x'); Check(!w.Emit("%s", huge.c_str()) && Failed(w), "oversize fails");
  }, 2, "type=fail reason=3");
  Run("record-budget", [] {
    Witness w; Check(w.Initialize(), "init");
    for (unsigned i = 0; i < 65; ++i) Check(w.Emit("type=test"), "within emitter budget");
    Check(!w.Emit("type=test") && Failed(w), "reserve final failure");
  }, 67, "type=fail reason=3");
  Run("info-disabled", [] {
    rex::SetCategoryLevel(rex::log::gpu(), spdlog::level::warn);
    { Witness w; Check(!w.Initialize() && Failed(w), "disabled info rejects"); }
    rex::SetCategoryLevel(rex::log::gpu(), spdlog::level::info);
  }, 1, "type=fail reason=4");
}
}  // namespace
int wmain(int argc, wchar_t** argv) {
  if (argc != 2 || std::wstring_view(argv[1]) != L"--run") return 2;
  rex::LogConfig logging; logging.log_to_console = false; logging.log_file = nullptr;
  rex::InitLogging(logging);
  Tests();
  rex::ShutdownLogging();
  std::cout << cases << " cases, " << checks << " checks, " << failures
            << " failures; in-memory synthetic emitter only\n";
  return failures ? 1 : 0;
}
