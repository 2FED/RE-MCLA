// Real fixed collector tests, entirely in memory: no GPU/game, page protection,
// exception injection, files or save access. Fixture synchronization is NOT in
// the production exception path. A breadcrumb never proves a completed store.
#include <rex/system/mcla_atlas_fault_collector.h>
#include <rex/graphics/d3d12/mcla_atlas_fault_witness.h>
#include <rex/logging/sink.h>
#include <rex/system/thread_state.h>
#include <array>
#include <atomic>
#include <barrier>
#include <iostream>
#include <limits>
#include <thread>
#include <type_traits>
#include <charconv>
#include <string_view>
#include <vector>

namespace {
namespace f = rex::diagnostics::atlas_fault;
unsigned checks = 0, failures = 0;
int owner_a = 0, owner_b = 0;
void Check(bool ok, const char* label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
f::Record Record(uint32_t index = 0) {
  f::Record r{};
  r.physical = f::kBase + index; r.thread = 1 + index; r.context = 1;
  r.function = 0x82001000; r.pc = 0x82001040 + 4 * index;
  r.lr = 0x82002004; r.handled = index & 1;
  return r;
}
void CollectorTests() {
  static_assert(std::is_trivially_copyable_v<f::Record>);
  static_assert(std::is_trivially_copyable_v<f::Snapshot>);
  Check(f::kLimit == 16 && f::kLength == 1048576 && f::kBase == 83492864, "frozen range and cap");
  {
    f::Collector c; f::Snapshot s;
    Check(c.Reserve(&owner_a, f::kBase) == f::kNoSlot, "unarmed ignores faults");
    Check(c.Arm(&owner_a), "one arm");
    for (auto address : {0u, f::kBase - 1, f::kBase + f::kLength, UINT32_MAX})
      Check(c.Reserve(&owner_a, address) == f::kNoSlot, "outside exact byte range ignored");
    Check(c.Close(&owner_a, s) && s.count == 0 && s.errors == 0, "explicit zero snapshot");
    Check(c.Reserve(&owner_a, f::kBase) == f::kNoSlot, "closed ignores later fault");
  }
  {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "boundaries arm");
    for (uint32_t i = 0; i < 2; ++i) {
      auto r = Record(i); r.physical = i ? f::kBase + f::kLength - 1 : f::kBase;
      auto slot = c.Reserve(&owner_a, r.physical);
      Check(slot == i, "boundary reservation ordinal"); c.Publish(slot, r);
    }
    Check(c.Close(&owner_a, s) && s.count == 2, "both endpoint bytes accepted");
    Check(s.records[0].physical == f::kBase && s.records[1].physical == f::kBase + f::kLength - 1, "unrounded addresses preserved");
    Check(s.records[0].handled == 0 && s.records[1].handled == 1, "handled distinction preserved");
  }
  {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "full arm");
    std::array<uint32_t, f::kLimit> slots{};
    for (uint32_t i = 0; i < f::kLimit; ++i) {
      slots[i] = c.Reserve(&owner_a, f::kBase + i); Check(slots[i] == i, "16 bounded reservations");
    }
    for (uint32_t i = f::kLimit; i-- > 0;) c.Publish(slots[i], Record(i));
    Check(c.Close(&owner_a, s) && s.count == f::kLimit, "reverse publication accepted");
    for (uint32_t i = 0; i < s.count; ++i)
      Check(s.records[i].thread == i + 1 && s.records[i].pc == 0x82001040 + 4 * i, "reservation order not publication order");
  }
  {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "overflow arm");
    for (uint32_t i = 0; i < f::kLimit; ++i) c.Publish(c.Reserve(&owner_a, f::kBase), Record(i));
    Check(c.Reserve(&owner_a, f::kBase) == f::kNoSlot, "17th cumulative record rejected");
    Check(!c.Close(&owner_a, s) && s.count == f::kLimit && (s.errors & f::Overflow), "overflow never successful truncation");
  }
  {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "inflight arm");
    auto slot = c.Reserve(&owner_a, f::kBase);
    Check(slot == 0, "inflight reserved");
    Check(!c.Close(&owner_a, s) && s.count == 1 && (s.errors & f::Inflight), "inflight close fails without waiting");
    Check(s.records[0].physical == 0, "unfinished payload not read");
    c.Publish(slot, Record()); // Real storage must remain alive after failed close.
    Check(c.Reserve(&owner_a, f::kBase) == f::kNoSlot, "late publication does not reopen");
    Check(!c.Close(&owner_a, s) && (s.errors & f::Inflight) && (s.errors & f::Duplicate), "late publication cannot rescue failed gate");
  }
  {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "identity arm");
    Check(c.Reserve(&owner_b, f::kBase) == f::kNoSlot, "wrong memory rejected");
    Check(!c.Close(&owner_a, s) && s.count == 0 && (s.errors & f::Identity), "identity sticky failure");
  }
  {
    f::Collector c; f::Snapshot s; Check(!c.Arm(nullptr), "null arm rejected");
    Check(c.Arm(&owner_a), "valid arm after null remains visibly poisoned");
    Check(!c.Close(&owner_a, s) && (s.errors & f::Identity), "null arm failure cannot be hidden");
  }
  {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "duplicate arm initial");
    Check(!c.Arm(&owner_a) && !c.Arm(&owner_b), "no reset or rearm");
    Check(!c.Close(&owner_a, s) && (s.errors & f::Identity), "rearm fail sticky");
  }
  {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "disable arm");
    c.Disable(&owner_b);
    auto slot = c.Reserve(&owner_a, f::kBase); Check(slot == 0, "foreign disable cannot disable owner");
    c.Disable(&owner_a); c.Publish(slot, Record());
    Check(c.Reserve(&owner_a, f::kBase) == f::kNoSlot, "disable blocks future reservations");
    Check(!c.Close(&owner_a, s) && (s.errors & f::Duplicate), "disabled is not completed");
  }
  for (uint32_t reason : {uint32_t(f::Contention), uint32_t(f::Reentry), uint32_t(f::Invalid)}) {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "explicit failure arm");
    c.Fail(reason); Check(!c.Close(&owner_a, s) && s.errors == reason, "explicit failure retained");
  }
  // Concurrent reservations can legitimately succeed or fail contention; neither
  // timing outcome is required. No dropped entry may produce an accepted census.
  for (unsigned iteration = 0; iteration < 64; ++iteration) {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "concurrency arm");
    std::barrier start(9); std::array<std::thread, 8> workers;
    std::array<uint32_t, 8> slots{};
    for (uint32_t i = 0; i < 8; ++i) workers[i] = std::thread([&, i] {
      start.arrive_and_wait(); slots[i] = c.Reserve(&owner_a, f::kBase + i);
      if (slots[i] != f::kNoSlot) c.Publish(slots[i], Record(i));
    });
    start.arrive_and_wait(); for (auto& worker : workers) worker.join();
    uint32_t admitted = 0; for (auto slot : slots) admitted += slot != f::kNoSlot;
    const bool complete = c.Close(&owner_a, s);
    Check(s.count == admitted && s.count <= 8, "concurrent exact reservation count");
    Check(complete ? admitted == 8 : (s.errors & f::Contention) != 0, "no silent contention loss");
    for (uint32_t i = 0; i < 8; ++i) if (slots[i] != f::kNoSlot)
      Check(s.records[slots[i]].thread == i + 1 && s.records[slots[i]].physical == f::kBase + i, "acquire published payload");
  }
  for (unsigned iteration = 0; iteration < 128; ++iteration) {
    f::Collector c; f::Snapshot s; Check(c.Arm(&owner_a), "close race arm");
    std::barrier start(2);
    std::thread worker([&] {
      start.arrive_and_wait(); auto slot = c.Reserve(&owner_a, f::kBase);
      if (slot != f::kNoSlot) c.Publish(slot, Record());
    });
    start.arrive_and_wait(); const bool complete = c.Close(&owner_a, s); worker.join();
    Check(s.count <= 1, "close race bound");
    Check(complete ? s.errors == 0 : (s.errors & f::Inflight) != 0, "close race complete or explicit inflight");
    if (complete && s.count == 1) Check(s.records[0].function == 0x82001000, "complete close never reads unfinished payload");
  }
}
// Each emitter case runs in a fresh process because production storage cannot
// reset/rearm. The opaque identity is never dereferenced (no Memory allocation).
void EmitterTest(unsigned which) {
  using Witness = rex::graphics::d3d12::AtlasFaultWitness;
  auto* memory = reinterpret_cast<rex::memory::Memory*>(&owner_a);
  auto* foreign = reinterpret_cast<rex::memory::Memory*>(&owner_b);
  auto sink = std::make_shared<rex::LogCaptureSink>(); sink->set_pattern("%v");
  rex::AddSink(rex::log::gpu(), sink);
  size_t expected_records = 2;
  std::string expected_final = "type=finish id=1 gen=2 records=0 frame=11 submission=21 complete=1";
  {
    Witness w(memory);
    if (which == 12) rex::SetCategoryLevel(rex::log::gpu(), spdlog::level::warn);
    const bool initialized = w.Initialize();
    Check(initialized == (which != 12), "real runtime arm and emitter init");
    if (which == 12) {
      expected_records = 1; expected_final = "type=fail reason=7";
      rex::SetCategoryLevel(rex::log::gpu(), spdlog::level::info);
    } else if (which == 13) {
      Check(!w.Initialize(), "real runtime rearm refused"); expected_final = "type=fail reason=2";
    } else if (which == 10) {
      expected_final = "type=fail reason=8"; // Destructor catches incomplete run.
    } else if (which == 11) {
      std::string huge(384, 'x'); Check(!w.Emit("%s", huge.c_str()), "bounded formatting rejects");
      expected_final = "type=fail reason=6";
    } else if (which == 9) {
      w.Begin(1, 2, true, 11, 21); expected_final = "type=fail reason=4";
    } else if (which == 14) {
      std::thread worker([&] { w.Begin(1, 1, true, 10, 20); }); worker.join();
      expected_final = "type=fail reason=5";
    } else {
      w.Begin(2, 1, false, 9, 19); // Unselected resource is not interpreted.
      w.Begin(1, 1, which != 8, 10, 20);
      if (which == 8) expected_final = "type=fail reason=4";
      else if (which == 7) { w.Begin(1, 1, true, 10, 20); expected_final = "type=fail reason=4"; }
      else if (which >= 19 && which <= 21) {
        rex::runtime::ThreadState thread(37, 0, 0, which == 20 ? foreign : memory);
        thread.context()->current_function = 0x82001000;
        thread.context()->guest_pc = 0x82001040;
        thread.context()->lr = 0x82002004;
        rex::runtime::ThreadState::Bind(&thread);
        if (which == 21) {
          std::thread worker([&] { f::NaturalFaultScope scope(memory, f::kBase, true); scope.Handled(); });
          worker.join();
        } else { f::NaturalFaultScope scope(memory, f::kBase, true); scope.Handled(); }
        rex::runtime::ThreadState::Bind(nullptr);
        expected_records = 3;
        expected_final = "type=finish id=1 gen=2 records=1 frame=11 submission=21 complete=1";
        w.Begin(1, 2, true, 11, 21);
      } else if (which == 1 || which == 18) {
        { f::NaturalFaultScope read(memory, f::kBase, false); read.Handled(); }
        { f::NaturalFaultScope outside(memory, f::kBase - 1, true); outside.Handled(); }
        if (which == 1) {
          { f::NaturalFaultScope unhandled(memory, f::kBase, true); }
          { f::NaturalFaultScope handled(memory, f::kBase + 1, true); handled.Handled(); }
          expected_records = 4;
          expected_final = "type=finish id=1 gen=2 records=2 frame=11 submission=21 complete=1";
        }
        w.Begin(1, 2, true, 11, 21);
      } else if (which == 2 || which == 3 || which == 15 || which == 16 || which == 17) {
        const uint32_t count = which == 2 || which == 3 ? 16 : 1;
        for (uint32_t i = 0; i < count; ++i) {
          f::Record r; auto slot = f::BeginNaturalFault(memory, f::kBase + i, r);
          Check(slot == i && r.context == 0 && r.thread == 0, "real null ThreadState context");
          r = Record(i); // Synthetic context tests serializer, not ThreadState acquisition.
          if (which == 15) { r.thread = r.function = r.pc = UINT32_MAX; r.lr = UINT64_MAX; }
          if (which == 16) r.context = 0;
          if (which == 17) r.handled = 2;
          f::PublishNaturalFault(slot, r);
        }
        if (which == 3) {
          f::Record r; Check(f::BeginNaturalFault(memory, f::kBase, r) == f::kNoSlot, "real cumulative overflow");
          expected_final = "type=fail reason=264";
        } else if (which == 16 || which == 17) expected_final = "type=fail reason=3";
        else {
          expected_records = 2 + count;
          expected_final = "type=finish id=1 gen=2 records=" + std::to_string(count) + " frame=11 submission=21 complete=1";
        }
        w.Begin(1, 2, true, 11, 21);
      } else if (which == 4) {
        f::Record r; const auto slot = f::BeginNaturalFault(memory, f::kBase, r);
        Check(slot == 0, "real inflight reservation");
        w.Begin(1, 2, true, 11, 21); f::PublishNaturalFault(slot, r);
        expected_final = "type=fail reason=272";
      } else if (which == 5) {
        { f::NaturalFaultScope outer(memory, f::kBase, true);
          { f::NaturalFaultScope inner(memory, f::kBase, true); inner.Handled(); }
          outer.Handled(); }
        w.Begin(1, 2, true, 11, 21); expected_final = "type=fail reason=260";
      } else if (which == 6) {
        { f::NaturalFaultScope wrong(foreign, f::kBase, true); wrong.Handled(); }
        w.Begin(1, 2, true, 11, 21); expected_final = "type=fail reason=257";
      } else w.Begin(1, 2, true, 11, 21);
      if (w.closed) {
        w.Begin(1, 3, true, 12, 22);
        { f::NaturalFaultScope late(memory, f::kBase, true); late.Handled(); }
      }
    }
  }
  rex::FlushLogging(); std::vector<rex::LogEntry> entries; sink->CopyEntries(entries);
  rex::RemoveSink(rex::log::gpu(), sink);
  std::vector<std::string> records; size_t bytes = 0;
  for (const auto& entry : entries) if (entry.text.starts_with("MCLA_ATLAS_FAULT ")) {
    records.push_back(entry.text); bytes += entry.text.size();
    Check(entry.text.size() < 384, "emitter bounded record");
    Check(entry.text.starts_with("MCLA_ATLAS_FAULT v=1 seq=" + std::to_string(records.size()) + " type="), "emitter contiguous sequence");
  }
  Check(records.size() == expected_records, "emitter exact record count");
  Check(records.size() <= 19 && bytes < 8192, "emitter bounded transcript");
  const auto final_tail = expected_final.starts_with("type=fail") ?
      " records=0 complete=0" : " claim=natural_fault_breadcrumb_not_completed_write";
  Check(!records.empty() && records.back() == "MCLA_ATLAS_FAULT v=1 seq=" +
      std::to_string(expected_records) + " " + expected_final + final_tail, "emitter exact final outcome");
  if (which != 12) Check(!records.empty() && records.front() ==
      "MCLA_ATLAS_FAULT v=1 seq=1 type=init enabled=1 base=83492864 length=1048576 records_limit=16 close_gen=2 claim=natural_fault_breadcrumb_not_completed_write", "emitter exact init");
  if (which == 1 && records.size() == 4) {
    Check(records[1].ends_with("physical=83492864 thread=0 context=0 function=0 pc=0 lr=0 handled=0"), "real RAII unhandled unknown context");
    Check(records[2].ends_with("physical=83492865 thread=0 context=0 function=0 pc=0 lr=0 handled=1"), "real RAII handled and TLS cleanup");
  }
  if (which == 15 && records.size() == 3) Check(records[1].find("lr=18446744073709551615") != std::string::npos, "full LR width preserved");
  if (which == 19 && records.size() == 3) Check(records[1].ends_with(
      "physical=83492864 thread=37 context=1 function=2181042176 pc=2181042240 lr=2181046276 handled=1"), "real bound ThreadState copied");
  if ((which == 20 || which == 21) && records.size() == 3) Check(records[1].ends_with(
      "physical=83492864 thread=0 context=0 function=0 pc=0 lr=0 handled=1"), "foreign memory or other thread context excluded");
}
}  // namespace
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::string_view argument(argv[1]);
  if (argument == "--collector") CollectorTests();
  else if (argument.starts_with("--emitter=")) {
    unsigned which = 99; auto value = argument.substr(10);
    auto parsed = std::from_chars(value.data(), value.data() + value.size(), which);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || which > 21) return 2;
    rex::LogConfig logging; logging.log_to_console = false; logging.log_file = nullptr;
    rex::InitLogging(logging); EmitterTest(which); rex::ShutdownLogging();
  } else return 2;
  std::cout << "atlas fault fixture: " << checks << " checks, " << failures << " failures" << std::endl;
  return failures ? 1 : 0;
}
