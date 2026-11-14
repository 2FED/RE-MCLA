// Synthetic context/ledger exercise only: no SDK kernel/game/window/save.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <rex/ui/swap_emission.h>
#include <rex/ppc/context.h>
#include <Windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>
namespace sw = rex::diagnostics::swap_emission;
namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const char* why) {
  ++checks; if (!value) { ++failures; std::cerr << why << '\n'; }
}
constexpr std::array<uint32_t, 4> stock{0x53574150, 0x1000, 1280, 720};
constexpr std::array<uint32_t, 6> fetch{10, 20, 30, 40, 50, 60};
thread_local int mode = 0;
thread_local sw::Payload nested;
thread_local const PPCContext* observed = nullptr;
thread_local const uint8_t* observed_base = nullptr;
std::atomic<uint64_t> next_callback_id{1};
// Test-only destinations capture the ID of THIS callback, never a latest-ID
// lookup or inferred correspondence with the SDK's issuance statistics.
thread_local uint64_t* expected_id_destination = nullptr;
thread_local uint64_t nested_expected_id = 0;
std::mutex gate_mutex;
std::condition_variable gate_changed;
bool gated_callback_entered = false, release_gated_callback = false;
std::atomic<bool> gate_timeout{false};
bool Collect(const PPCContext& context, const uint8_t* base, sw::Snapshot& snapshot) {
  observed = &context; observed_base = base;
  const uint64_t id = next_callback_id.fetch_add(1, std::memory_order_relaxed);
  snapshot.callback_id = id;
  if (expected_id_destination) *expected_id_destination = id;
  snapshot.timer_address = 0x827D7500;
  snapshot.timer_counter = context.r4.u32;
  snapshot.timer_delta_bits = 0x3D088889;
  snapshot.guest_caller_lr = 999;  // Native wrapper must override this.
  snapshot.native_thread_id = 999;
  SetLastError(765);
  if (mode == 1) return false;
  if (mode == 2) throw std::runtime_error("synthetic callback");
  if (mode == 3) {
    mode = 0;
    PPCContext child{}; child.lr = 0x2222; child.r4.u32 = 77;
    uint8_t child_memory = 0;
    auto* outer_destination = expected_id_destination;
    expected_id_destination = &nested_expected_id;
    { sw::ExportContextScope inner(child, &child_memory); nested = sw::Emit(stock, fetch); }
    expected_id_destination = outer_destination;
    mode = 3;
  }
  if (mode == 4) {
    std::unique_lock lock(gate_mutex);
    gated_callback_entered = true;
    gate_changed.notify_all();
    if (!gate_changed.wait_for(lock, std::chrono::seconds(1), [] { return release_gated_callback; }))
      gate_timeout.store(true, std::memory_order_relaxed);
  }
  if (mode == 5) sw::StopIssuing();  // ID assigned, but issuance must still disarm.
  return true;
}
uint64_t Qpc() { LARGE_INTEGER value{}; QueryPerformanceCounter(&value); return uint64_t(value.QuadPart); }
}
int main() {
  Check(sw::kSnapshotAbiVersion == 2, "explicit snapshot ABI2");
  const auto legacy = sw::Emit(stock, fetch);
  Check(legacy.length == 4, "unconfigured legacy");
  for (size_t i = 0; i < 4; ++i) Check(legacy.words[i] == stock[i], "legacy stock");
  LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
  sw::Configuration configuration{0x123456789abcdef0, Qpc(), Qpc() + uint64_t(frequency.QuadPart) * 12,
                                 uint64_t(frequency.QuadPart), Collect};
  // Re-read end from begin so the declared duration is exactly bounded.
  configuration.end_qpc = configuration.begin_qpc + configuration.qpc_frequency * 12;
  auto bad = configuration; bad.epoch = 0;
  Check(!sw::Configure(bad), "zero epoch rejected");
#if !defined(REXGLUE_SWAP_EMISSION_WITNESS)
  Check(!sw::Configure(configuration), "default off cannot configure");
  Check(sw::Emit(stock, fetch).length == 4, "default off legacy");
  Check(next_callback_id.load(std::memory_order_relaxed) == 1, "default off never calls ID observer");
#else
  Check(sw::Configure(configuration), "configured once");
  Check(!sw::Configure(configuration), "no reconfiguration");
  const auto no_context = sw::Emit(stock, fetch);
  Check(no_context.length == 11, "missing context still has emission identity");
  auto no_context_binding = sw::Consume(no_context.view(), fetch);
  Check(no_context_binding.valid() && !no_context_binding.match.snapshot.snapshot_available,
        "missing context not a timing observation");
  Check(no_context_binding.match.snapshot.callback_id == 0, "missing context has no callback ID");
  PPCContext context{}; context.lr = 0x1111; context.r4.u32 = 42;
  std::array<uint8_t, sizeof(PPCContext)> before{};
  std::memcpy(before.data(), &context, before.size());
  uint8_t memory = 99;
  sw::Payload ordinary;
  uint64_t ordinary_expected_id = 0;
  {
    sw::ExportContextScope scope(context, &memory);
    SetLastError(1234);
    expected_id_destination = &ordinary_expected_id;
    ordinary = sw::Emit(stock, fetch);
    expected_id_destination = nullptr;
    Check(GetLastError() == 1234, "LastError preserved across callback");
    Check(observed == &context && observed_base == &memory, "actual context/base");
  }
  Check(std::memcmp(before.data(), &context, before.size()) == 0 && memory == 99, "no context/memory mutation");
  SetLastError(5678);
  const auto binding = sw::Consume(ordinary.view(), fetch);
  Check(GetLastError() == 5678, "LastError preserved across consume");
  Check(binding.valid(), "ordinary match");
  Check(binding.match.snapshot.guest_caller_lr == 0x1111 &&
        binding.match.snapshot.native_thread_id == GetCurrentThreadId(), "actual native identities");
  Check(binding.match.snapshot.timer_counter == 42 && binding.match.snapshot.snapshot_available,
        "opaque observed state");
  Check(ordinary_expected_id != 0 && binding.match.snapshot.callback_id == ordinary_expected_id,
        "available snapshot carries its dedicated callback identity");
  Check(binding.consume_qpc >= binding.match.snapshot.qpc_after &&
        binding.match.snapshot.qpc_after >= binding.match.snapshot.qpc_before, "outer clock witnesses");
  Check(!sw::Consume(ordinary.view(), fetch).valid(), "replay rejected");
  for (int failure_mode : {1, 2}) {
    mode = failure_mode;
    uint64_t partial_expected_id = 0;
    sw::ExportContextScope scope(context, &memory);
    expected_id_destination = &partial_expected_id;
    const auto payload = sw::Emit(stock, fetch);
    expected_id_destination = nullptr;
    const auto unavailable = sw::Consume(payload.view(), fetch);
    Check(unavailable.valid() && unavailable.match.snapshot == sw::Snapshot{}, "partial/throw snapshot cleared");
    Check(partial_expected_id != 0 && unavailable.match.snapshot.callback_id == 0,
          "partial/throw clears the ID actually assigned by that callback");
  }
  mode = 3;
  sw::Payload outer;
  uint64_t outer_expected_id = 0;
  {
    sw::ExportContextScope scope(context, &memory);
    expected_id_destination = &outer_expected_id;
    outer = sw::Emit(stock, fetch);
    expected_id_destination = nullptr;
    mode = 0;
    const auto restored = sw::Emit(stock, fetch);
    Check(sw::Consume(restored.view(), fetch).match.snapshot.guest_caller_lr == 0x1111, "nested scope restored");
  }
  const auto inner = sw::Consume(nested.view(), fetch);
  const auto outer_binding = sw::Consume(outer.view(), fetch);
  Check(inner.valid() && outer_binding.valid(), "reentrant callback outside lock");
  Check(inner.match.snapshot.guest_caller_lr == 0x2222 &&
        outer_binding.match.snapshot.guest_caller_lr == 0x1111, "nested distinct identities");
  Check(outer_expected_id != 0 && nested_expected_id > outer_expected_id &&
        inner.match.snapshot.callback_id == nested_expected_id &&
        outer_binding.match.snapshot.callback_id == outer_expected_id &&
        inner.match.token.serial < outer_binding.match.token.serial,
        "nested callback ID order reverses token issuance without latest-ID binding");
  const auto no_scope = sw::Consume(sw::Emit(stock, fetch).view(), fetch);
  Check(no_scope.valid() && !no_scope.match.snapshot.snapshot_available, "no leaked TLS scope");
  std::array<sw::Payload, 64> concurrent{};
  std::array<sw::Binding, 64> matched{};
  std::array<uint64_t, 64> expected_ids{};
  std::vector<std::thread> threads;
  for (size_t t = 0; t < 8; ++t) threads.emplace_back([&, t] {
    PPCContext local{}; local.lr = uint32_t(0x3000 + t); local.r4.u32 = uint32_t(t);
    uint8_t local_memory = 0;
    sw::ExportContextScope scope(local, &local_memory);
    for (size_t i = 0; i < 8; ++i) {
      const size_t index = t * 8 + i;
      expected_id_destination = &expected_ids[index];
      concurrent[index] = sw::Emit(stock, fetch);
      expected_id_destination = nullptr;
      matched[index] = sw::Consume(concurrent[index].view(), fetch);
    }
  });
  for (auto& thread : threads) thread.join();
  std::set<uint64_t> callback_ids, token_serials;
  for (size_t i = 0; i < matched.size(); ++i) {
    Check(matched[i].valid() && matched[i].match.snapshot.guest_caller_lr == 0x3000 + i / 8,
          "concurrent same-thread context pairing");
    Check(expected_ids[i] != 0 && matched[i].match.snapshot.callback_id == expected_ids[i],
          "64 concurrent emissions retain their own callback ID");
    Check(callback_ids.insert(matched[i].match.snapshot.callback_id).second &&
          token_serials.insert(matched[i].match.token.serial).second,
          "concurrent callback IDs and issued tokens separately unique");
  }
  std::array<sw::Payload, 2> reversed{};
  std::array<uint64_t, 2> reversed_ids{};
  // Real threads: A assigns its ID and blocks IN the callback. B waits for
  // that event, assigns another ID and issues first, then releases A. Every
  // test rendezvous has a one-second predicate deadline; no timed sleeps.
  std::thread first([&] {
    PPCContext local{}; local.lr = 0x5001; local.r4.u32 = 1;
    uint8_t local_memory = 0;
    mode = 4; expected_id_destination = &reversed_ids[0];
    sw::ExportContextScope scope(local, &local_memory);
    reversed[0] = sw::Emit(stock, fetch);
    expected_id_destination = nullptr; mode = 0;
  });
  std::thread second([&] {
    {
      std::unique_lock lock(gate_mutex);
      if (!gate_changed.wait_for(lock, std::chrono::seconds(1), [] { return gated_callback_entered; })) {
        gate_timeout.store(true, std::memory_order_relaxed);
        release_gated_callback = true; gate_changed.notify_all(); return;
      }
    }
    PPCContext local{}; local.lr = 0x5002; local.r4.u32 = 2;
    uint8_t local_memory = 0;
    expected_id_destination = &reversed_ids[1];
    { sw::ExportContextScope scope(local, &local_memory); reversed[1] = sw::Emit(stock, fetch); }
    expected_id_destination = nullptr;
    { std::lock_guard lock(gate_mutex); release_gated_callback = true; }
    gate_changed.notify_all();
  });
  first.join(); second.join();
  const auto first_match = sw::Consume(reversed[0].view(), fetch);
  const auto second_match = sw::Consume(reversed[1].view(), fetch);
  Check(!gate_timeout.load(std::memory_order_relaxed) && first_match.valid() && second_match.valid(),
        "bounded real-thread reverse issuance rendezvous completes");
  Check(reversed_ids[0] != 0 && reversed_ids[0] < reversed_ids[1] &&
        first_match.match.token.serial > second_match.match.token.serial,
        "real-thread callback ID order is not token serial order");
  Check(first_match.match.snapshot.callback_id == reversed_ids[0] &&
        second_match.match.snapshot.callback_id == reversed_ids[1] &&
        first_match.match.snapshot.guest_caller_lr == 0x5001 &&
        second_match.match.snapshot.guest_caller_lr == 0x5002,
        "gated threads preserve exact callback/token/context association");
  auto wrong = fetch; wrong[3] ^= 1;
  const auto corrupt = sw::Emit(stock, fetch);
  Check(!sw::Consume(corrupt.view(), wrong).valid(), "wrong fetch rejected");
  Check(!sw::Consume(corrupt.view(), fetch).valid(), "wrong fetch burned token");
  const auto pending = sw::Emit(stock, fetch);
  const auto before_stop = sw::ReadStatistics();
  uint64_t stopped_expected_id = 0;
  sw::Payload stopped_payload;
  {
    mode = 5; expected_id_destination = &stopped_expected_id;
    sw::ExportContextScope scope(context, &memory);
    SetLastError(4321);
    stopped_payload = sw::Emit(stock, fetch);
    expected_id_destination = nullptr; mode = 0;
    Check(GetLastError() == 4321, "LastError preserved for disarm during callback");
  }
  Check(stopped_expected_id != 0 && stopped_payload.length == 4 &&
        sw::ReadStatistics().issued == before_stop.issued,
        "disarm after callback ID returns legacy without issuing a token");
  for (size_t i = 0; i < 4; ++i) Check(stopped_payload.words[i] == stock[i], "disarmed stock untouched");
  const auto stopped_binding = sw::Consume(stopped_payload.view(), fetch);
  Check(!stopped_binding.valid() && stopped_binding.match.snapshot.callback_id == 0 &&
        stopped_binding.match.token == sw::Token{}, "disarmed callback cannot bind via latest ID");
  Check(sw::Emit(stock, fetch).length == 4, "stop uses legacy");
  Check(sw::Consume(pending.view(), fetch).valid(), "in-flight packet survives stop");
  const auto stats = sw::ReadStatistics();
  Check(!stats.failed && stats.callback_failed == 1 && stats.rejected == 3 &&
        stats.issued == stats.matched + 1, "explicit bounded outcome counters");
#endif
  std::cout << "swap emission native synthetic: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
