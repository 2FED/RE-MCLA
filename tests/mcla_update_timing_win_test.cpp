// Synthetic contexts + mock originals/readers only. No game, GPU, files or windows.
#include "../src/mcla_update_timing_win.cpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
namespace timing = mcla::update_timing;
namespace d = timing::detail;
using Json = nlohmann::json;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
thread_local mcla::phase_census::ActiveUpdate active{};
thread_local int mode{};
std::atomic<uint64_t> ticks{100}, calls{}, reads{};
std::atomic<bool> entered{false}, release_original{false};
int read_mode{};
unsigned checks{}, failures{};
bool phase_enabled = true, scout_enabled = true;
uint64_t Clock() { SetLastError(800); return ticks.fetch_add(1); }
uint64_t ThrowClock() { throw 5; }
uint64_t ZeroClock() { return 0; }
void Check(bool condition, const char* label) {
  ++checks; if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
template<class F> void Reject(F&& operation, const char* label) {
  bool rejected = false; try { operation(); } catch (...) { rejected = true; }
  Check(rejected, label);
}
bool Reader(uintptr_t, uint32_t address, mcla::step_timer_memory::State& state) {
  const auto ordinal = reads.fetch_add(1) + 1; SetLastError(801);
  if (address != d::kTimer) throw 19;
  state.timer.words[0] = 0x3d088889; state.timer.words[8] = 77;
  state.reciprocal_bits = 0x3f800000; state.count = 2;
  state.saved_unsplit_bits = 0x3d088889; state.split_state = 255;
  if (read_mode == 2) throw 20;
  if (read_mode == 3 || (read_mode == 4 && ordinal == 2)) {
    entered.store(true, std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!release_original.load(std::memory_order_acquire)) {
      if (std::chrono::steady_clock::now() > deadline) std::abort();
      std::this_thread::yield();
    }
  }
  return read_mode == 0 || read_mode == 4;
}
std::unique_ptr<d::State> State() {
  auto state = std::make_unique<d::State>();
  state->data.begin = 100; state->data.end = 100000; state->data.frequency = 10000;
  state->data.token = token; state->clock = Clock; state->reader = Reader;
  ticks.store(100); reads.store(0); calls.store(0); read_mode = 0; mode = 0;
  active = {17, 3, GetCurrentThreadId()};
  return state;
}
PPCContext Context() {
  PPCContext ctx{}; ctx.lr = 0x822C21B4; ctx.r3.u64 = 0xffffffff827d7500ull;
  ctx.r4.u64 = 0; ctx.r5.u64 = UINT64_MAX-7;
  ctx.f1.u64 = 0x7ff8000000000001ull; ctx.f28.u64 = 0x8000000000000000ull;
  ctx.f30.u64 = UINT64_MAX; ctx.f31.u64 = 0x3ff0000000000000ull;
  ctx.r23.u32 = 7; ctx.r24.u32 = 8;
  return ctx;
}
void Original(PPCContext& ctx, uint8_t*) {
  ++calls;
  if (GetLastError() != 77) throw 999;
  ++ctx.r5.u64; SetLastError(1234);
  if (mode == 1) throw 42;
  if (mode == 2) active.phase_call_id++;
  if (mode == 5) ticks.store(99);
  if (mode == 3) {
    entered.store(true, std::memory_order_release);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!release_original.load(std::memory_order_acquire)) {
      if (std::chrono::steady_clock::now() > deadline) std::abort();
      std::this_thread::yield();
    }
  }
  if (mode == 4) {
    const auto parent = active; ++active.phase_call_id; mode = 0;
    SetLastError(77);
    sub_821BDA90(ctx, reinterpret_cast<uint8_t*>(1));
    active = parent; mode = 4; SetLastError(1234);
  }
}
void Run(d::State* state, d::Kind kind, PPCContext& ctx) {
  auto expected = ctx; ++expected.r5.u64; const auto previous = calls.load();
  SetLastError(77);
  d::Invoke(state, kind, Original, ctx, reinterpret_cast<uint8_t*>(1));
  const auto error = GetLastError();
  Check(calls == previous+1, "exactly one original");
  Check(error == 1234 && std::memcmp(&ctx, &expected, sizeof(ctx)) == 0, "complete original context and LastError effects retained");
}
Json Cut(d::State& state) { return Json::parse(d::Cut(state, token)); }
template<class Predicate> void WaitUntil(Predicate&& predicate) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!predicate()) {
    if (std::chrono::steady_clock::now() > deadline) std::abort();
    std::this_thread::yield();
  }
}
void WaitEntered() { WaitUntil([] { return entered.load(std::memory_order_acquire); }); }
}
namespace mcla::phase_census {
bool Enabled() { return phase_enabled; }
ActiveUpdate ReadActiveUpdate(const PPCContext&, const uint8_t* base) noexcept { return base ? active : ActiveUpdate{}; }
}
namespace mcla::guest_source_scout { bool Enabled() { return scout_enabled; } }
REX_EXTERN(__imp__sub_821BDA90) { Original(ctx, base); }
REX_EXTERN(__imp__sub_821BD910) { Original(ctx, base); }
REX_EXTERN(__imp__sub_822C52C8) { Original(ctx, base); }

int main() {
  Check(!timing::Enabled(), "default disabled");
  Reject([] { timing::Freeze(token); }, "freeze before Begin rejected");
  Check(rex::cvar::SetFlagByName("mcla_update_timing_probe", "true"), "explicit option exists");
  LARGE_INTEGER qpf{}; Check(QueryPerformanceFrequency(&qpf) != FALSE, "real QPF");
  const auto frequency = uint64_t(qpf.QuadPart);
  Reject([&] { timing::Begin(100, 200, frequency, token); }, "Begin before Install rejected");
  d::installed.store(true); // Test-only bypass: production requires real dispatcher identity.
  // Exercise the pre-Begin terminal guard through internal test state only;
  // the actual public Disarm at the end is never reset or reconfigured.
  d::disarmed.store(true);
  Reject([&] { timing::Begin(100, 200, frequency, token); }, "pre-Begin disarmed configuration rejected");
  d::disarmed.store(false);
  for (const auto& bad : {std::string(32, '0'), std::string(32, 'A'), std::string("bad")})
    Reject([&] { timing::Begin(100, 200, frequency, bad); }, "invalid token rejected");
  Reject([&] { timing::Begin(0, 200, frequency, token); }, "zero begin");
  Reject([&] { timing::Begin(200, 100, frequency, token); }, "reversed window");
  Reject([&] { timing::Begin(100, 101+12*frequency, frequency, token); }, "window over twelve seconds");
  Reject([&] { timing::Begin(100, 200, frequency+1, token); }, "QPF mismatch");
  phase_enabled = false;
  Reject([&] { timing::Begin(100, 200, frequency, token); }, "phase prerequisite");
  phase_enabled = true; scout_enabled = false;
  Reject([&] { timing::Begin(100, 200, frequency, token); }, "scout prerequisite");
  scout_enabled = true;
  {
    auto ctx = Context(); Run(nullptr, d::Kind::Timer, ctx);
    auto state = State(); ctx = Context(); Run(state.get(), d::Kind::Timer, ctx);
    const auto data = Cut(*state); const auto& row = data["records"][0];
    Check(reads == 2 && row["before"]["count"] == 2 && row["after"]["split_state"] == 255, "single fixed read each boundary with raw fields");
    Check(row["incoming"]["r3"] == 0xffffffff827d7500ull && row["expected_route"] == true, "sign extended PPC timer accepted, raw retained");
    Check(row["incoming"]["f1_bits"] == 0x7ff8000000000001ull && row["incoming"]["f30_bits"] == UINT64_MAX, "opaque FPR bits preserved");
    Check(row["incoming"]["loop_r23"] == 0 && row["incoming"]["loop_r24"] == 0, "loop fields not sampled outside helper route");
    Check(row["binding"]["phase_call_id"] == 17 && row["exit_binding_matches"] == true, "exact phase ID retained");
    for (const auto* key : {"native_admitted", "coverage_proven", "actual_consumer_proven", "state_coherence_proven", "object_lifetime_proven", "simulation_proven", "frame_to_step_mapping_proven", "fps_proven"})
      Check(data[key] == false, "all proof flags false");
    Check(data["counts"]["completed_entries"] == 1 && data["counts"]["pending_entries"] == 0, "closed accounting");
  }
  for (auto lr : {0x822C21B4ull, 0x822C22E4ull, 0x822C230Cull, 0x822C246Cull, 0x822C267Cull}) {
    auto state = State(); auto ctx = Context(); ctx.lr = lr;
    const auto kind = lr < 0x822C230C ? d::Kind::Timer : d::Kind::Subdivision;
    Run(state.get(), kind, ctx); const auto data = Cut(*state);
    Check(reads == 2 && data["records"][0]["expected_route"] == true, "all approved timer/helper routes");
    Check(data["records"][0]["incoming"]["loop_r23"] == (lr == 0x822C246C ? 7 : 0), "loop LR allowlist");
  }
  constexpr std::array<uint64_t, 5> router_lrs{0x822C24C4,0x822C273C,0x822C2768,0x822C27C0,0x822C2848};
  constexpr std::array<uint32_t, 5> selectors{1,4,0,8,2};
  for (size_t i = 0; i < router_lrs.size(); ++i) {
    auto state = State(); auto ctx = Context(); ctx.lr = router_lrs[i]; ctx.r3.u64 = 0xffffffff12345678ull; ctx.r4.u64 = 0xffffffff00000000ull | selectors[i];
    Run(state.get(), d::Kind::Router, ctx); auto data = Cut(*state);
    Check(reads == 2 && data["records"][0]["expected_route"] == true, "all router selectors use low PPC word");
    state = State(); ctx.r4.u32 = 99; Run(state.get(), d::Kind::Router, ctx); data = Cut(*state);
    Check(reads == 0 && data["records"][0]["before"].is_null() && data["records"][0]["expected_route"] == false, "wrong selector never reads arbitrary r3");
  }
  for (int variant = 0; variant < 4; ++variant) {
    auto state = State(); auto ctx = Context();
    if (variant == 0) active = {};
    if (variant == 1) active.native_thread_id++;
    if (variant == 2) ctx.lr = 0x1234;
    if (variant == 3) ctx.r3.u32 = 0xdeadbeef;
    Run(state.get(), d::Kind::Timer, ctx); const auto data = Cut(*state);
    Check(reads == 0 && data["counts"]["unbound_entries"] == 1 && data["records"].size() == 1, "unknown/unbound retained without RPM");
  }
  {
    auto state = State(); auto ctx = Context(); ctx.lr = 0x822C24C4; ctx.r4.u32 = 1; ctx.r3.u64 = 0xffffffff00000000ull;
    Run(state.get(), d::Kind::Router, ctx); const auto data = Cut(*state);
    Check(reads == 0 && data["records"][0]["expected_route"] == false, "router requires nonzero guest low-word pointer");
  }
  for (int variant : {1,2}) {
    auto state = State(); read_mode = variant; auto ctx = Context(); Run(state.get(), d::Kind::Timer, ctx);
    const auto data = Cut(*state);
    Check(data["counts"]["read_failures"] == 2 && data["records"][0]["before"].is_null() && data["records"][0]["after"].is_null(), "failed/throwing reader cleared and counted");
  }
  {
    auto state = State(); auto ctx = Context(); mode = 1; SetLastError(77); bool same = false;
    try { d::Invoke(state.get(), d::Kind::Timer, Original, ctx, reinterpret_cast<uint8_t*>(1)); }
    catch (int code) { same = code == 42; }
    const auto error = GetLastError(); const auto data = Cut(*state);
    Check(same && calls == 1 && error == 1234, "same original exception and LastError propagated once");
    Check(data["counts"]["original_exceptions"] == 1 && data["records"][0]["completed"] == true && data["records"][0]["normal_return"] == false, "exception completion explicit");
  }
  {
    auto state = State(); auto ctx = Context(); mode = 2; Run(state.get(), d::Kind::Timer, ctx);
    const auto data = Cut(*state);
    Check(reads == 1 && data["counts"]["binding_mismatches"] == 1 && data["observer_failed"] == true, "changed nested/current update cannot donate after state");
    Check(data["records"][0]["after"].is_null() && data["records"][0]["exit_binding_matches"] == false, "mismatch after unavailable");
  }
  {
    auto state = State(); d::configured.store(state.get()); auto ctx = Context(); auto expected = ctx; expected.r5.u64 += 2;
    mode = 4; SetLastError(77); sub_821BDA90(ctx, reinterpret_cast<uint8_t*>(1));
    const auto error = GetLastError(); d::configured.store(nullptr); const auto data = Cut(*state);
    Check(calls == 2 && reads == 4 && error == 1234 && std::memcmp(&ctx, &expected, sizeof(ctx)) == 0, "nested original work runs unlocked with exact effects");
    Check(data["records"][0]["binding"]["phase_call_id"] == 17 && data["records"][1]["binding"]["phase_call_id"] == 18, "nested update identities remain distinct");
    Check(data["records"][0]["exit_binding_matches"] == true && data["records"][1]["exit_binding_matches"] == true, "restored lexical binding matches each invocation");
  }
  {
    auto state = State(); auto ctx = Context(); ticks.store(99); Run(state.get(), d::Kind::Timer, ctx);
    ticks.store(state->data.end); Run(state.get(), d::Kind::Timer, ctx);
    const auto data = Cut(*state);
    Check(reads == 0 && data["counts"]["outside_before_entries"] == 1 && data["counts"]["outside_after_entries"] == 1, "exclusive window boundaries");
  }
  for (auto clock : {ZeroClock, ThrowClock}) {
    auto state = State(); auto ctx = Context(); state->clock = clock; Run(state.get(), d::Kind::Timer, ctx);
    const auto data = Cut(*state);
    Check(data["clock_failed"] == true && data["counts"]["invalid_clock_entries"] == 1 && reads == 0, "failed clock never admits RPM");
  }
  {
    auto state = State(); auto ctx = Context(); mode = 5; Run(state.get(), d::Kind::Timer, ctx);
    const auto data = Cut(*state);
    Check(data["clock_failed"] == true && data["records"][0]["exit_qpc"] == 99 && data["records"][0]["completed"] == true, "bad actual exit retained, no timestamp fabrication");
    Check(data["counts"]["invalid_clock_entries"] == 0, "entry-clock rejection count excludes exit faults");
  }
  {
    auto state = State(); auto ctx = Context(); Run(state.get(), d::Kind::Timer, ctx);
    ticks.store(100); Run(state.get(), d::Kind::Timer, ctx); const auto data = Cut(*state);
    Check(data["clock_failed"] == true && data["counts"]["invalid_clock_entries"] == 1, "entry clock regression sticky");
  }
  {
    auto state = State(); state->data.counts.observed = UINT64_MAX; auto ctx = Context(); Run(state.get(), d::Kind::Timer, ctx);
    const auto data = Cut(*state);
    Check(data["counts"]["observed_entries"] == UINT64_MAX && data["counts"]["counter_saturated"] == true && data["observer_failed"] == true, "counters saturate without wrapping");
  }
  {
    auto state = State(); auto ctx = Context();
    for (size_t i = 0; i < d::kCapacity + 1; ++i) { ctx = Context(); Run(state.get(), d::Kind::Timer, ctx); }
    const auto data = Cut(*state);
    Check(data["records"].size() == d::kCapacity && data["counts"]["overflow_entries"] == 1 && data["observer_failed"] == true, "bounded overflow no overwrite");
    Check(data["records"].back()["observation_id"] == d::kCapacity && reads == 2*d::kCapacity, "ordinal and read bound");
  }
  {
    auto state = State(); entered.store(false); release_original.store(false);
    std::thread worker([&] { active = {900,22,GetCurrentThreadId()}; mode = 3; auto ctx = Context(); SetLastError(77); d::Invoke(state.get(), d::Kind::Timer, Original, ctx, reinterpret_cast<uint8_t*>(1)); });
    WaitEntered(); const auto before = d::Cut(*state, token); const auto data = Json::parse(before);
    Check(data["counts"]["pending_entries"] == 1 && data["records"][0]["exit_qpc"] == 0 && data["records"][0]["after"].is_null(), "held original retained pending, no drain");
    release_original.store(true, std::memory_order_release); worker.join();
    Check(d::Cut(*state, token) == before && reads == 1, "late original return cannot backfill frozen rows");
    auto ctx = Context(); Run(state.get(), d::Kind::Timer, ctx);
    Check(d::Cut(*state, token) == before, "postfreeze calls preserve immutable journal");
  }
  {
    auto state = State(); entered.store(false); release_original.store(false); read_mode = 3;
    std::thread worker([&] { active = {901,23,GetCurrentThreadId()}; mode = 0; auto ctx = Context(); SetLastError(77); d::Invoke(state.get(), d::Kind::Timer, Original, ctx, reinterpret_cast<uint8_t*>(1)); });
    WaitEntered(); const auto before = d::Cut(*state, token); const auto data = Json::parse(before);
    Check(data["counts"]["pending_entries"] == 1 && data["records"][0]["before"].is_null() && data["counts"]["read_failures"] == 0, "RPM outside observer lock and unfinished copy stays pending not failed");
    release_original.store(true, std::memory_order_release); worker.join();
    Check(d::Cut(*state, token) == before && calls == 1 && reads == 1, "late RPM and original cannot mutate cut");
  }
  {
    auto state = State(); entered.store(false); release_original.store(false); read_mode = 4;
    std::atomic<bool> done{false};
    std::thread worker([&] {
      active = {902,24,GetCurrentThreadId()}; mode = 0; auto ctx = Context();
      SetLastError(77); d::Invoke(state.get(), d::Kind::Timer, Original, ctx, reinterpret_cast<uint8_t*>(1));
      done.store(true, std::memory_order_release);
    });
    WaitEntered();
    const auto before = d::Cut(*state, token); const auto data = Json::parse(before);
    Check(calls == 1 && reads == 2 && data["counts"]["pending_entries"] == 1,
          "original returned but held exit RPM remains pending at live cut");
    Check(!data["records"][0]["before"].is_null() && data["records"][0]["after"].is_null() &&
          data["records"][0]["completed"] == false && data["records"][0]["exit_qpc"] == 0,
          "exit RPM does not publish partial completion before copy finishes");
    release_original.store(true, std::memory_order_release);
    WaitUntil([&] { return done.load(std::memory_order_acquire); }); worker.join();
    Check(d::Cut(*state, token) == before && calls == 1 && reads == 2,
          "late exit RPM cannot backfill immutable pending row or retry original");
  }
  {
    auto state = State();
    constexpr size_t kThreads = 4, kEach = 64;
    std::array<std::thread, kThreads> workers;
    std::array<DWORD, kThreads> tids{};
    std::atomic<unsigned> ready{0}, done{0}, errors{0};
    std::atomic<bool> go{false};
    for (size_t i = 0; i < kThreads; ++i) workers[i] = std::thread([&, i] {
      tids[i] = GetCurrentThreadId(); active = {1000+i,2000+i,tids[i]}; mode = 0;
      ready.fetch_add(1, std::memory_order_release);
      WaitUntil([&] { return go.load(std::memory_order_acquire); });
      for (size_t n = 0; n < kEach; ++n) {
        auto ctx = Context(), expected = ctx; ++expected.r5.u64;
        SetLastError(77);
        d::Invoke(state.get(), d::Kind::Timer, Original, ctx, reinterpret_cast<uint8_t*>(1));
        if (GetLastError() != 1234 || std::memcmp(&ctx, &expected, sizeof(ctx)) != 0) ++errors;
      }
      done.fetch_add(1, std::memory_order_release);
    });
    WaitUntil([&] { return ready.load(std::memory_order_acquire) == kThreads; });
    go.store(true, std::memory_order_release);
    WaitUntil([&] { return done.load(std::memory_order_acquire) == kThreads; });
    for (auto& worker : workers) worker.join();
    const auto data = Cut(*state);
    Check(errors == 0 && calls == kThreads*kEach && reads == 2*kThreads*kEach,
          "concurrent originals preserve exact effects without lost admissions");
    Check(data["counts"]["observed_entries"] == kThreads*kEach &&
          data["counts"]["stored_entries"] == kThreads*kEach &&
          data["counts"]["completed_entries"] == kThreads*kEach &&
          data["counts"]["pending_entries"] == 0 && data["counts"]["overflow_entries"] == 0 &&
          data["observer_failed"] == false && data["clock_failed"] == false,
          "concurrent admission/completion accounting stays exact and healthy");
    for (size_t i = 0; i < kThreads; ++i) {
      Check(tids[i] != 0 && std::count(tids.begin(), tids.end(), tids[i]) == 1,
            "simultaneously live threads have distinct native IDs");
      size_t matched = 0;
      for (const auto& row : data["records"]) if (row["native_thread_id"] == tids[i]) {
        ++matched;
        Check(row["binding"]["native_thread_id"] == tids[i] &&
              row["binding"]["phase_call_id"] == 1000+i && row["binding"]["thread_epoch"] == 2000+i &&
              row["exit_binding_matches"] == true,
              "each concurrent row retains its own exact thread/update namespace");
      }
      Check(matched == kEach, "each native thread contributes exactly its own calls");
    }
    for (size_t i = 0; i < kThreads*kEach; ++i)
      Check(data["records"][i]["observation_id"] == i+1, "concurrent admission IDs unique and consecutive");
  }
  {
    auto state = State(); const auto old = state->data.counts.observed; Reject([&] { d::Cut(*state, "bad"); }, "wrong freeze token");
    Check(!state->frozen && state->data.counts.observed == old, "wrong token does not freeze");
    auto ctx = Context(); d::configured.store(state.get());
    for (auto wrapper : {sub_821BDA90,sub_821BD910,sub_822C52C8}) {
      SetLastError(77); const auto n = calls.load(); wrapper(ctx, reinterpret_cast<uint8_t*>(1));
      Check(calls == n+1 && GetLastError() == 1234, "actual strong public wrappers exactly once");
    }
    d::configured.store(nullptr);
  }
  {
    LARGE_INTEGER now{}; Check(QueryPerformanceCounter(&now) != FALSE && now.QuadPart > 0, "public Begin real QPC anchor");
    const auto begin = uint64_t(now.QuadPart), end = begin + frequency;
    timing::Begin(begin, end, frequency, token);
    auto* configured = d::configured.load(std::memory_order_acquire);
    Check(configured != nullptr && configured->data.begin == begin && configured->data.end == end &&
          configured->data.frequency == frequency && configured->data.token == token,
          "successful public Begin retains real QPF and exact one-shot configuration");
    Reject([&] { timing::Begin(begin, end, frequency, token); }, "second successful-shape Begin rejected");
    SetLastError(888); timing::Disarm(); Check(GetLastError() == 888, "Disarm LastError");
    Check(d::disarmed.load() && configured->stopped.load(), "public Disarm is terminal for active configuration");
    Reject([&] { timing::Begin(begin, end, frequency, token); }, "Begin after terminal Disarm rejected");
    SetLastError(889); const auto frozen = timing::Freeze(token);
    Check(GetLastError() == 889 && Json::parse(frozen)["qpc_frequency"] == frequency,
          "public Freeze after Disarm retains actual clock and LastError");
    Check(timing::Freeze(token) == frozen && d::configured.load() == configured,
          "one-shot storage retained and repeated public Freeze immutable");
  }
  std::cout << "update timing: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
