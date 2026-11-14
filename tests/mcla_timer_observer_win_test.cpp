// No game, dispatcher install, window, GPU, file or external process operations.
// Actual pinned SDK context, self-process reader and native LastError seam.
#include "../src/mcla_timer_observer_win.cpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstring>

namespace mcla::guest_source_scout { bool Enabled() { return false; } }
namespace {
namespace native = mcla::timer_native;
using Json = nlohmann::json;
unsigned checks = 0, failures = 0, original_calls = 0;
DWORD seen_error = 0;
bool throw_original = false;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
void Check(bool value, const char* name) {
  ++checks; if (!value) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
void Original(PPCContext& context, uint8_t* memory) {
  ++original_calls; seen_error = GetLastError();
  memory[context.r3.u32 + 8] ^= 0x5A;
  context.r3.u64 = 0xFEEDFACE12345678ull;
  context.r5.u64 ^= 0xDEADBEEF;
  context.f1.u64 ^= 0xAA55AA55AA55AA55ull;
  context.lr = 0x82221100;
  SetLastError(0xABCD1234);
  if (throw_original) throw std::runtime_error("original exception");
}
auto NewObserver() {
  LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
  const auto now = native::Qpc();
  return std::make_unique<native::Observer>(mcla::timer_journal::Window{
      now, now + uint64_t(frequency.QuadPart) * 10, uint64_t(frequency.QuadPart)}, true);
}
void ContextTests(bool unwind) {
  std::array<uint8_t, 512> baseline_memory{}, observed_memory{};
  for (size_t i = 0; i < baseline_memory.size(); ++i) baseline_memory[i] = uint8_t(i);
  observed_memory = baseline_memory;
  PPCContext baseline{}, observed{};
  baseline.r3.u32 = 128; baseline.r5.u64 = 0xAABBCCDD;
  baseline.f1.u64 = 0x0123456789ABCDEF; baseline.lr = 0x821BCA94;
  std::memcpy(&observed, &baseline, sizeof(PPCContext));
  auto core = NewObserver();
  const auto before_calls = original_calls; throw_original = unwind;
  auto call = [&](PPCContext& context, uint8_t* base, native::Observer* observer) {
    bool caught = false; SetLastError(0x24681357);
    try { native::InvokeObserved(Original, observer, context, base); }
    catch (const std::runtime_error& error) { caught = std::string_view(error.what()) == "original exception"; }
    const auto after_error = GetLastError();
    Check(caught == unwind, "original exception outcome preserved");
    Check(seen_error == 0x24681357 && after_error == 0xABCD1234, "original native LastError input/output preserved");
  };
  call(baseline, baseline_memory.data(), nullptr);
  call(observed, observed_memory.data(), core.get());
  Check(original_calls == before_calls + 2, "baseline and observed original exactly once each");
  Check(std::memcmp(&baseline, &observed, sizeof(PPCContext)) == 0, "whole actual PPC context equals baseline");
  Check(baseline_memory == observed_memory, "all original memory effects equal baseline");
  auto snapshot = std::make_unique<native::Observer::Snapshot>();
  Check(core->try_freeze_into(*snapshot) && snapshot->records().size() == 1, "native wrapper drains one call");
  const auto& row = snapshot->records()[0];
  Check(row.observation.identity.timer_address == 128 && row.observation.identity.caller_address == 0x821BCA94 &&
        row.observation.identity.thread_identity == GetCurrentThreadId(), "incoming identity recorded before context changes");
  Check(row.observation.before_available && row.observation.after_available &&
        (row.observation.before.words[0] ^ row.observation.after.words[0]) == 0x5A000000,
        "after read uses initial timer despite original r3 change");
  Check(row.observation.incoming_f1_bits == 0x0123456789ABCDEF && row.observation.incoming_r5_low_byte == 0xDD,
        "incoming argument bits unchanged");
  Check(bool(row.callback_issues & mcla::timer_observer::kOriginalUnwound) == unwind, "native unwind explicitly recorded");
  const auto json = Json::parse(native::Serialize(*snapshot, token));
  Check(json["records"].size() == 1 && json["records"][0]["before"]["words"].size() == 18 &&
        json["records"][0]["before"]["bytes"].size() == 3, "serialized allowlisted states only");
  Check(!json["simulation_proven"].get<bool>() && !json["fps_proven"].get<bool>() &&
        !json["native_admitted"].get<bool>(), "no runtime or timing admission");
}
void DrainTests() {
  using Clock = std::chrono::steady_clock;
  for (unsigned scenario = 0; scenario < 4; ++scenario) {
    auto core = NewObserver(); std::stop_source stop;
    if (scenario == 1) stop.request_stop();
    unsigned calls = 0, waits = 0; bool rejected = false;
    auto now = [&] {
      ++calls;
      // Initial, pre-freeze, post-freeze, post-serialize checks.
      if (scenario >= 2 && calls >= scenario + 1) return Clock::time_point{} + std::chrono::seconds(3);
      return Clock::time_point{};
    };
    try { native::Drain(*core, stop.get_token(), token, now, [&] { ++waits; }); }
    catch (const std::runtime_error&) { rejected = true; }
    Check(rejected == (scenario != 0) && waits == 0, "cancel/late successful freeze or serialization rejected");
  }
  auto core = NewObserver(); auto snapshot = std::make_unique<native::Observer::Snapshot>();
  core->try_freeze_into(*snapshot);
  for (const auto* invalid : {"", "ABCDEF01234567890123456789012345", "../012345678901234567890123456789"}) {
    bool rejected = false;
    try { native::Serialize(*snapshot, invalid); } catch (...) { rejected = true; }
    Check(rejected, "invalid output token rejected");
  }
  Check(native::StateJson({}, false) == "null", "unavailable state never emits stale numeric fields");
}
}
int main() {
  ContextTests(false); ContextTests(true); DrainTests();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
