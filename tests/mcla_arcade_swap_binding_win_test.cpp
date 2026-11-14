// SOURCE-only until a separately reviewed native recipe consumes fresh modes.
// Actual paired ABI2 SDK Emit/Consume, real phase/swap callbacks + new leaf.
// Include-source/synthetic originals/private allocated page, NOT Runtime,
// generated originals, presenter/capture/GPU/input/game/physical12s coverage.
#include "../src/mcla_phase_census_win.cpp"
#include "../src/mcla_swap_observer_win.cpp"
#include "../src/mcla_arcade_swap_binding_win.cpp"
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <cstring>
#include <iostream>

namespace mcla::guest_source_scout { bool Enabled() { return true; } }
namespace {
namespace leaf = mcla::arcade_swap_binding;
namespace phase = mcla::phase_census;
namespace sw = rex::diagnostics::swap_emission;
using Json = nlohmann::json;
constexpr char kToken[] = "0123456789abcdef0123456789abcdef";
constexpr uint64_t kEpoch = 0x0123456789abcdef;
constexpr uint32_t kTimer = 0x827D7500;
constexpr std::array<uint32_t, 4> kStock{0x53574150, 0x1000, 1280, 720};
constexpr std::array<uint32_t, 6> kFetch{10, 20, 30, 40, 50, 60};
unsigned checks{}, failures{}, originals{};
bool unwind_original{};
size_t destination = 1;
struct Native {
  DWORD error; unsigned mxcsr;
  static Native Get() { return {GetLastError(), _mm_getcsr()}; }
  void Put() const { _mm_setcsr(mxcsr); SetLastError(error); }
  bool operator==(const Native&) const = default;
};
constexpr Native kIncoming{77, 0x3F80}, kProduced{0x12345678, 0x3FA0};
struct Signal {};
struct Expected { bool present{}; rex::ui::GuestCaptureProvenance provenance; };
std::array<Expected, 25> expected;
sw::Binding observed_binding;
const PPCContext* seen_context{};
uint8_t* seen_base{};
void Check(bool ok, const char* why) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << why << '\n'; }
}
template<class F> void Reject(F&& f, const char* why) {
  bool rejected = false; try { f(); } catch (const std::exception&) { rejected = true; }
  Check(rejected, why);
}
void Record(size_t slot, const sw::Binding& binding) {
  rex::ui::GuestCaptureProvenance p;
  p.presenter_epoch = 7; p.acquired_sequence = slot + 1; p.swap_emission = binding;
  leaf::RecordCapture(slot, p); expected[slot] = {true, p};
}
sw::Binding Dirty() {
  sw::Binding b;
  b.result = static_cast<sw::Result>(310);
  b.match.token = {UINT64_MAX, UINT64_MAX};
  b.stock.fill(UINT32_MAX); b.fetch.fill(UINT32_MAX); b.consume_qpc = UINT64_MAX;
  b.match.snapshot_result = static_cast<sw::SnapshotResult>(311);
  b.match.snapshot = {UINT64_MAX, UINT64_MAX, UINT32_MAX, UINT32_MAX,
                     UINT32_MAX, UINT32_MAX, UINT32_MAX, false, UINT64_MAX};
  return b;
}
Json BindingExpected(const sw::Binding& b) {
  const auto& s = b.match.snapshot;
  return {{"result", uint32_t(b.result)}, {"stock", b.stock}, {"fetch", b.fetch},
      {"consume_qpc", b.consume_qpc}, {"match", {
      {"token", {{"epoch", b.match.token.epoch}, {"serial", b.match.token.serial}}},
      {"snapshot_result", uint32_t(b.match.snapshot_result)}, {"snapshot", {
      {"qpc_before", s.qpc_before}, {"qpc_after", s.qpc_after},
      {"native_thread_id", s.native_thread_id}, {"guest_caller_lr", s.guest_caller_lr},
      {"timer_address", s.timer_address}, {"timer_counter", s.timer_counter},
      {"timer_delta_bits", s.timer_delta_bits}, {"snapshot_available", s.snapshot_available},
      {"callback_id", s.callback_id}}}}}};
}
void Validate(const Json& value, const std::string& token) {
  Check(value["schema"] == "mcla-arcade-swap-binding-v1" && value["token"] == token,
        "exact schema and full token");
  Check(value["physical_capture_slots"] == 25 && value["records"].size() == 25,
        "all25 physical capture slots retained");
  unsigned count = 0;
  for (size_t i = 0; i < expected.size(); ++i) {
    const auto& raw = expected[i]; const auto& row = value["records"][i];
    Json want{{"slot", i}, {"has_capture", raw.present}, {"before_shared_window", i == 0},
              {"presenter_epoch", nullptr}, {"acquired_sequence", nullptr}, {"binding", nullptr}};
    if (raw.present) {
      ++count; want["presenter_epoch"] = raw.provenance.presenter_epoch;
      want["acquired_sequence"] = raw.provenance.acquired_sequence;
      want["binding"] = BindingExpected(raw.provenance.swap_emission);
    }
    Check(row == want, "entire raw binding/snapshot including unknown/unavailable and absent slots");
  }
  Check(value["attempt"]["source_captures"] == count, "count describes copies, not matched sources");
  for (const auto* key : {"statistics_quiescent", "exact_timer_window_configuration_coverage_proven",
       "runtime_admitted", "frame_to_step_mapping_proven", "source_frame_binding_proven",
       "snapshot_coherence_proven", "original_semantics_proven", "causality_proven",
       "simulation_proven", "physics_proven", "display_proven", "fps_proven"})
    Check(value[key] == false, "no runtime/causal/coverage/frame/FPS promotion");
  Check(value.dump().size() <= 128*1024, "bounded raw sidecar");
}
void Original(PPCContext& context, uint8_t* base) {
  ++originals; seen_context = &context; seen_base = base;
  Check(Native::Get() == kIncoming, "original receives exact incoming LastError/MXCSR");
  const auto before = context;
  sw::Payload packet;
  kIncoming.Put();
  { sw::ExportContextScope scope(context, base); packet = sw::Emit(kStock, kFetch); }
  const auto after_emit = Native::Get();
  Check(after_emit == kIncoming, "actual SDK Emit/Observe preserves incoming native state");
  kIncoming.Put(); observed_binding = sw::Consume(packet.view(), kFetch);
  const auto after_consume = Native::Get();
  Check(after_consume == kIncoming && observed_binding.valid(), "actual SDK Consume once/native state");
  Check(packet.length == 11 && std::memcmp(&before, &context, sizeof(context)) == 0,
        "diagnostics leave original context unchanged");
  Record(destination, observed_binding);
  ++context.r5.u64; kProduced.Put();
  if (unwind_original) throw Signal{};
}
void Invoke(PPCContext& context, uint8_t* base) {
  const auto before = context; const auto calls = originals;
  bool caught = false; kIncoming.Put();
  try { sub_822C1FA8(context, base); } catch (const Signal&) { caught = true; }
  const auto produced = Native::Get();
  auto wanted = before; ++wanted.r5.u64;
  Check(originals == calls + 1 && seen_context == &context && seen_base == base,
        "strong wrapper invokes same original/context/base exactly once");
  Check(caught == unwind_original && produced == kProduced &&
        std::memcmp(&context, &wanted, sizeof(context)) == 0,
        "normal/unwind preserves produced native state and all context effects");
}

std::mutex gate_mutex;
std::condition_variable gate_cv;
bool callback_entered{}, callback_release{};
uint64_t DelayedClock() {
  std::unique_lock lock(gate_mutex); callback_entered = true; gate_cv.notify_all();
  gate_cv.wait(lock, [] { return callback_release; });
  LARGE_INTEGER stamp{}; return QueryPerformanceCounter(&stamp) && stamp.QuadPart > 0 ? uint64_t(stamp.QuadPart) : 0;
}
struct ReleaseCallback {
  ~ReleaseCallback() {
    { std::lock_guard lock(gate_mutex); callback_release = true; }
    gate_cv.notify_all();
  }
};

void Exercise(const std::string& mode, uint8_t* memory) {
  Check(!leaf::Enabled(), "new leaf defaults OFF");
  const auto* flag = rex::cvar::GetFlagInfo("mcla_arcade_swap_binding_probe");
  Check(flag && flag->lifecycle == rex::cvar::Lifecycle::kInitOnly,
        "new flag InitOnly");
  if (mode != "disabled") Check(rex::cvar::SetFlagByName("mcla_arcade_swap_binding_probe", "true"), "enable only new leaf");
  Check(rex::cvar::SetFlagByName("mcla_phase_census_probe", "true"), "enable real phase only");
  Check(!mcla::swap_native::Enabled(), "legacy swap/scout remains OFF and unused");
  LARGE_INTEGER stamp{}, frequency{};
  const bool clock_ok = QueryPerformanceCounter(&stamp) && QueryPerformanceFrequency(&frequency) &&
        stamp.QuadPart > 0 && frequency.QuadPart > 0 && uint64_t(frequency.QuadPart) <= 10000000000ull;
  Check(clock_ok, "actual bounded QPC inputs");
  if (!clock_ok) return;
  const auto begin = uint64_t(stamp.QuadPart), hz = uint64_t(frequency.QuadPart);
  Check(begin <= sw::kMaxQpc - 12*hz, "fixed window overflow checked");
  if (!begin || !hz || hz > 10000000000ull || begin > sw::kMaxQpc - 12*hz) return;
  const auto end = begin + 12*hz;
  Check(reinterpret_cast<uintptr_t>(memory) > kTimer, "nonwrapping allocated guest translation");
  if (reinterpret_cast<uintptr_t>(memory) <= kTimer) return;
  auto* base = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(memory) - kTimer);
  memory[8] = 0x3D; memory[9] = 8; memory[10] = 0x88; memory[11] = 0x89;
  memory[44] = 0x12; memory[45] = 0x34; memory[46] = 0x56; memory[47] = 0x78;
  std::array<uint8_t,4096> memory_before{}; std::memcpy(memory_before.data(), memory, 4096);
  PPCContext context{}; context.lr = 0x8241A0E8; context.r3.u64 = 0x11223344;
  phase::Begin(begin, end, hz, kToken); // Sole phase Begin. No legacy swap Begin.
  if (mode != "disabled") Record(0, Dirty());
  else Reject([&] { Record(0, Dirty()); }, "disabled Record rejected without overwrite");
  Reject([&] { leaf::RecordCapture(25, {}); }, "out-of-range capture rejected");
  if (mode != "disabled") Reject([&] { Record(0, {}); }, "duplicate capture rejected without overwrite");
  std::string token = kToken;
  if (mode == "no-begin") {
    Check(!leaf::ReadAttempt().attempted, "no hidden configuration from capture0");
  } else {
    if (mode == "stop-before") leaf::StopIssuing();
    if (mode == "configure-failed")
      Check(sw::Configure({kEpoch, begin, end, hz, mcla::swap_native::detail::Observe}), "foreign real SDK configured once");
    const auto requested_token = mode == "bad-token" ? std::string("bad\"token") :
        mode == "zero-epoch" ? std::string("00000000000000000000000000000001") : token;
    if (mode == "zero-epoch") token = requested_token;
    const bool configured = leaf::BeginSharedWindow(begin, mode == "bad-window" ? begin+hz : end, hz, requested_token);
    const bool expected_success = mode == "normal" || mode == "partial" || mode == "unwind" ||
        mode == "delayed-callback" || mode == "post-stop-consume";
    Check(configured == expected_success, "one shared SDK Configure outcome");
    const auto saved = leaf::ReadAttempt();
    const auto reason = mode == "disabled" ? leaf::Reason::Disabled :
        mode == "bad-token" ? leaf::Reason::InvalidToken :
        mode == "bad-window" ? leaf::Reason::InvalidWindow :
        mode == "zero-epoch" ? leaf::Reason::ZeroTransportEpoch :
        mode == "stop-before" ? leaf::Reason::StopAlreadyRequested :
        mode == "configure-failed" ? leaf::Reason::SdkRejected : leaf::Reason::Configured;
    Check(saved.attempted && saved.configured == expected_success && saved.reason == reason &&
        saved.begin_qpc == begin && saved.end_qpc == (mode == "bad-window" ? begin+hz : end) &&
        saved.qpc_frequency == hz && saved.token_valid == (mode != "bad-token"),
        "raw attempt window and bounded precise failure reason retained");
    Check(!leaf::BeginSharedWindow(begin, end, hz, kToken) && leaf::ReadAttempt().reason == saved.reason,
          "failed/successful attempt never reconfigures or repairs reason");
    if (mode == "configure-failed") {
      leaf::StopIssuing(); sw::Payload packet;
      { sw::ExportContextScope scope(context, base); packet = sw::Emit(kStock, kFetch); }
      Check(packet.length == 11 && leaf::ReadAttempt().reason == leaf::Reason::SdkRejected,
            "failed leaf Configure/Stop never stops the foreign SDK owner");
      Record(1, sw::Consume(packet.view(), kFetch));
    } else if (mode == "delayed-callback") {
      auto* state = phase::detail::configured.load(); state->clock = DelayedClock;
      sw::Payload packet;
      std::jthread worker([&] { sw::ExportContextScope scope(context, base); packet = sw::Emit(kStock, kFetch); });
      bool entered;
      {
        // Release on every exception path BEFORE jthread's joining destructor.
        ReleaseCallback release;
        { std::unique_lock lock(gate_mutex); entered = gate_cv.wait_for(lock, std::chrono::seconds(3), [] { return callback_entered; }); }
        leaf::StopIssuing(); // SDK does not drain the callback parked before ledger.issue.
      }
      worker.join(); state->clock = phase::detail::Qpc;
      Check(entered && packet.length == 4 && state->callback_used == 1,
            "delayed actual callback completes after Stop without late token issuance");
      Record(1, sw::Consume(packet.view(), kFetch));
    } else if (mode == "post-stop-consume") {
      sw::Payload packet;
      { sw::ExportContextScope scope(context, base); packet = sw::Emit(kStock, kFetch); }
      leaf::StopIssuing();
      const auto text = leaf::Serialize(token); const auto value = Json::parse(text); Validate(value, token);
      Check(value["statistics"]["issued"] == 1 && value["statistics"]["matched"] == 0,
            "serialization samples instantaneous pre-consume statistics");
      const auto late = sw::Consume(packet.view(), kFetch);
      Check(late.valid() && sw::ReadStatistics().matched == 1 && value["statistics"]["matched"] == 0,
            "Consume remains mutable after Stop and serialized statistics are not quiescent");
      Reject([&] { Record(1, late); }, "post-serialization capture rejected");
      Reject([&] { leaf::Serialize(token); }, "no repeated statistics sampling/serialization");
      std::cout << "RAW_SWAP " << text << '\n';
      const auto phase_text = phase::Finish({}, kToken); std::cout << "PHASE_JSON " << phase_text << '\n';
      Check(std::memcmp(memory, memory_before.data(), 4096) == 0, "callbacks never change synthetic timer memory");
      return;
    } else if (configured) {
      unwind_original = mode == "unwind"; Invoke(context, base);
      Check(observed_binding.match.snapshot.snapshot_available && observed_binding.match.snapshot.callback_id == 1 &&
          observed_binding.match.snapshot.timer_counter == 0x12345678 &&
          observed_binding.match.snapshot.timer_delta_bits == 0x3D088889,
          "actual source callback ID/context/global timer payload retained");
      if (mode == "normal") {
        Record(2, sw::Consume(sw::Emit(kStock, kFetch).view(), kFetch)); // No ExportContextScope.
        Check(!expected[2].provenance.swap_emission.match.snapshot.snapshot_available,
              "actual unavailable snapshot remains an object with raw zero ID");
        phase::Disarm(); destination = 3; Invoke(context, base);
        Check(observed_binding.match.snapshot.snapshot_available && observed_binding.match.snapshot.callback_id == 0,
              "available actual snapshot ID0 never replaced by previous callback");
        for (size_t i = 4; i < 24; ++i) Record(i, Dirty());
        rex::ui::GuestCaptureProvenance zero_identity;
        zero_identity.swap_emission = Dirty();
        leaf::RecordCapture(24, zero_identity); expected[24] = {true, zero_identity};
      }
    }
  }
  const auto before = leaf::ReadAttempt();
  Reject([&] { leaf::Serialize("x"); }, "bad serialization token does not consume cold snapshot");
  if (before.token_valid) Reject([&] { leaf::Serialize("ffffffffffffffffffffffffffffffff"); }, "full128 token mismatch rejected");
  const auto text = leaf::Serialize(token); const auto value = Json::parse(text); Validate(value, token);
  const Json attempt_expected{{"attempted", before.attempted}, {"configured", before.configured},
      {"issuance_stop_requested", true}, {"token_valid", before.token_valid},
      {"reason", leaf::ReasonName(before.reason)}, {"source_captures", before.source_captures},
      {"epoch", before.epoch}, {"begin_qpc", before.begin_qpc},
      {"end_qpc_exclusive", before.end_qpc}, {"qpc_frequency", before.qpc_frequency}};
  Check(value["attempt"] == attempt_expected, "entire attempted/failed/no-start descriptor preserved");
  Check(value["statistics_available"] == before.configured &&
        value["statistics"].is_null() == !before.configured, "failed/no Begin never claims foreign statistics");
  Check(value["attempt"]["issuance_stop_requested"] == true, "only issuance stop, never drain claim");
  Reject([&] { leaf::Serialize(token); }, "one serialization attempt only");
  const auto phase_text = phase::Finish({}, kToken); const auto phase_value = Json::parse(phase_text);
  if (mode == "normal") Check(phase_value["swap_callbacks"].size() == 1 &&
      phase_value["swap_callbacks"][0]["has_scope"] == true && originals == 2,
      "real source token references one lexical phase callback, no universal scope claim");
  Check(std::memcmp(memory, memory_before.data(), 4096) == 0, "callbacks leave allocated timer memory unchanged");
  std::cout << "RAW_SWAP " << text << '\n' << "PHASE_JSON " << phase_text << '\n';
}
} // namespace
REX_EXTERN(__imp__sub_822C1FA8) { Original(ctx, base); }
REX_EXTERN(__imp__sub_82305D00) { Original(ctx, base); }
REX_EXTERN(__imp__sub_82131008) { Original(ctx, base); }

int main(int argc, char** argv) {
  const std::array<std::string_view, 11> modes{"normal", "partial", "unwind", "no-begin", "disabled",
      "bad-token", "bad-window", "zero-epoch", "stop-before", "configure-failed", "delayed-callback"};
  if (argc != 2 || (std::find(modes.begin(), modes.end(), argv[1]) == modes.end() &&
                   std::string_view(argv[1]) != "post-stop-consume")) return 2;
  const auto incoming = Native::Get();
  auto* memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  Check(memory != nullptr, "private synthetic page allocated");
  try { if (memory) Exercise(argv[1], memory); }
  catch (const std::exception& e) { Check(false, e.what()); }
  catch (...) { Check(false, "unexpected fixture exception"); }
  leaf::StopIssuing(); phase::Disarm();
  // Explicit fixture cleanup of foreign-owner case, not leaf behavior.
  if (std::string_view(argv[1]) == "configure-failed") sw::StopIssuing();
  if (memory) Check(VirtualFree(memory, 0, MEM_RELEASE) != FALSE, "own page released");
  incoming.Put();
  std::cout << "ARCADE_SWAP_BINDING mode=" << argv[1] << " checks=" << checks << " failures=" << failures
      << " SYNTHETIC_ORIGINALS_ACTUAL_SDK_ABI2_NO_RUNTIME_NO_GAME_NO_CAUSAL_FPS\n";
  // Explicit transport completion, not an SDK/result fallback. A consumed
  // predecessor exited0 with exactly4096 stdout bytes and a missing tail.
  std::cout.flush();
  if (!std::cout.good()) return 3;
  constexpr char marker[] = "ARCADE_SWAP_BINDING_STDOUT_FLUSH_EOF\n";
  DWORD marker_bytes = 0;
  if (!WriteFile(GetStdHandle(STD_ERROR_HANDLE), marker, sizeof(marker)-1, &marker_bytes, nullptr) ||
      marker_bytes != sizeof(marker)-1) return 4;
  incoming.Put();
  return failures ? 1 : 0;
}
