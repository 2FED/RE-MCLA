// Synthetic allocated memory/context only. No game, GPU, window or file IO.
// Compile both real host modules with MCLA_PHASE_CENSUS_PROBE,
// MCLA_SWAP_OBSERVER_PROBE and REXGLUE_SWAP_EMISSION_WITNESS, link fresh ABI2 SDK.
#include "../src/mcla_phase_census_win.cpp"
#include "../src/mcla_swap_observer_win.cpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>

namespace mcla::guest_source_scout {
bool Enabled() { return true; }
}

namespace {
namespace phase = mcla::phase_census;
namespace observer = mcla::swap_native;
namespace sw = rex::diagnostics::swap_emission;
using Profile = mcla::guest_source_scout::detail::Profile;
using Json = nlohmann::json;
unsigned checks{}, failures{}, originals{};
std::array<unsigned, 3> original_kinds{};
std::array<sw::Binding, 8> bindings{};
size_t binding_count{};
constexpr uint32_t kTimerAddress = 0x827D7500;
constexpr uint64_t kEpoch = 0x0123456789abcdef;
constexpr char kToken[] = "0123456789abcdef0123456789abcdef";
constexpr std::array<uint32_t, 4> kStock{0x53574150, 0x1000, 1280, 720};
constexpr std::array<uint32_t, 6> kFetch{10, 20, 30, 40, 50, 60};

void Check(bool ok, const char* label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
template<class F> void Reject(F&& action, const char* label) {
  bool rejected = false;
  try { action(); } catch (const std::exception&) { rejected = true; }
  Check(rejected, label);
}
void Original(unsigned kind, PPCContext& context, uint8_t* base) {
  ++originals; ++original_kinds[kind];
  Check(GetLastError() == 77, "original receives unchanged incoming LastError");
  const auto before = context;
  sw::Payload packet;
  { sw::ExportContextScope actual(context, base); packet = sw::Emit(kStock, kFetch); }
  Check(GetLastError() == 77, "actual SDK callback restores LastError");
  Check(std::memcmp(&context, &before, sizeof(context)) == 0,
        "actual callback cannot modify guest context");
  Check(packet.length == 11, "configured SDK issues bounded diagnostic packet");
  const auto binding = sw::Consume(packet.view(), kFetch);
  Check(GetLastError() == 77 && binding.valid(), "actual SDK consume matches once and preserves error");
  Check(binding_count < bindings.size(), "synthetic binding array bound");
  if (binding_count < bindings.size()) bindings[binding_count++] = binding;
  // Only this mock original writes the context, never the observers.
  ++context.r5.u64;
  SetLastError(0x12345678);
}
void Invoke(PPCFunc* wrapper, PPCContext& context, uint8_t* base) {
  const auto before = context;
  const auto calls = originals;
  SetLastError(77);
  wrapper(context, base);
  const auto error = GetLastError();
  auto expected = before; ++expected.r5.u64;
  Check(originals == calls + 1, "strong wrapper calls mock original exactly once");
  Check(error == 0x12345678 && std::memcmp(&context, &expected, sizeof(context)) == 0,
        "strong wrapper preserves all original context and error effects");
}

void Exercise(uint8_t* memory, Profile profile) {
  const auto count = mcla::guest_source_scout::detail::SlotCount(profile);
  const auto other_profile = profile == Profile::kShort31 ? Profile::kLegacy91 : Profile::kShort31;
  static_assert(sw::kSnapshotAbiVersion == 2);
  Check(!phase::Enabled() && !observer::Enabled(), "both host probes default off");
  Check(rex::cvar::SetFlagByName("mcla_phase_census_probe", "true"), "explicit phase enable");
  Check(rex::cvar::SetFlagByName("mcla_swap_observer_probe", "true"), "explicit swap enable");
  LARGE_INTEGER stamp{}, frequency{};
  const bool clock_ok = QueryPerformanceCounter(&stamp) && QueryPerformanceFrequency(&frequency) &&
                        stamp.QuadPart > 0 && frequency.QuadPart > 0;
  Check(clock_ok, "actual QPC frequency available");
  if (!clock_ok) return;
  const auto hz = uint64_t(frequency.QuadPart);
  const auto begin = uint64_t(stamp.QuadPart), scout_begin = begin + hz;
  const auto end = scout_begin + count * hz / 10;
  phase::Begin(begin, end, hz, kToken);
  observer::Begin(scout_begin, hz, kToken, profile);
  auto* state = phase::detail::configured.load(std::memory_order_acquire);
  Check(state != nullptr, "real phase configuration retained");
  if (!state) return;

  // Translate the allowlisted guest timer to our own one-page allocation.
  const auto address = reinterpret_cast<uintptr_t>(memory);
  Check(address > kTimerAddress, "allocated host address permits nonwrapping guest translation");
  if (address <= kTimerAddress) return;
  auto* base = reinterpret_cast<uint8_t*>(address - kTimerAddress);
  memory[8] = 0x3D; memory[9] = 0x08; memory[10] = 0x88; memory[11] = 0x89;
  memory[44] = 0x12; memory[45] = 0x34; memory[46] = 0x56; memory[47] = 0x78;
  std::array<uint8_t, 4096> memory_before{};
  std::memcpy(memory_before.data(), memory, memory_before.size());
  PPCContext context{}; context.lr = 0x8241A0E8; context.r3.u64 = 0x12345678;
  Invoke(sub_822C1FA8, context, base);
  Invoke(sub_82305D00, context, base);
  Invoke(sub_82131008, context, base);
  for (size_t i = 0; i < 3; ++i) {
    const auto& snapshot = bindings[i].match.snapshot;
    const auto& callback = state->callbacks[i];
    Check(snapshot.snapshot_available && snapshot.callback_id == i + 1 &&
          callback.callback_id == snapshot.callback_id,
          "actual phase callback returned ID reaches SDK ledger match");
    Check(snapshot.timer_address == kTimerAddress && snapshot.timer_counter == 0x12345678 &&
          snapshot.timer_delta_bits == 0x3D088889, "ID propagation preserves opaque timer values");
    Check(snapshot.qpc_before <= callback.qpc && callback.qpc <= snapshot.qpc_after &&
          snapshot.qpc_after <= bindings[i].consume_qpc,
          "phase callback QPC inside actual SDK before-after witness");
    Check(callback.has_scope && callback.call == state->scopes[i].call &&
          callback.context_same && callback.base_same && !callback.suppressed_scope,
          "callback references active exact original wrapper scope");
    Check(callback.tid == snapshot.native_thread_id && callback.lr == snapshot.guest_caller_lr,
          "phase and SDK actual thread/caller identities agree");
  }

  sw::Snapshot dirty{1, 2, 3, 4, 5, 6, 7, true}; dirty.callback_id = UINT64_MAX;
  SetLastError(77);
  Check(!observer::detail::Observe(context, nullptr, dirty), "null base timer observation fails");
  Check(dirty == sw::Snapshot{} && GetLastError() == 77 && state->callback_used == 4,
        "null timer failure clears whole snapshot but retains unmatched phase callback");
  Invoke(sub_82131008, context, nullptr);
  Check(bindings[3].match.snapshot == sw::Snapshot{} && state->callback_used == 4,
        "SDK null-base guard skips host callback and clears entire unavailable snapshot");

  DWORD previous{};
  const bool protected_ok = VirtualProtect(memory, 4096, PAGE_NOACCESS, &previous) != FALSE;
  Check(protected_ok, "synthetic page protected unreadable");
  if (!protected_ok) return;
  Invoke(sub_82131008, context, base);
  Check(bindings[4].match.snapshot == sw::Snapshot{} &&
        bindings[4].match.snapshot_result == sw::SnapshotResult::kUnavailable &&
        state->callback_used == 5 && state->callbacks[4].callback_id == 5,
        "actual failed RPM leaves ID5 unmatched and SDK-normalized snapshot fully clear");
  dirty = {1, 2, 3, 4, 5, 6, 7, true}; dirty.callback_id = UINT64_MAX;
  SetLastError(77);
  Check(!observer::detail::Observe(context, base, dirty), "direct inaccessible timer fails without exception");
  Check(dirty == sw::Snapshot{} && GetLastError() == 77 && state->callback_used == 6,
        "inaccessible direct read clears stale ID and retains unmatched phase callback6");
  DWORD ignored{};
  const bool restored = VirtualProtect(memory, 4096, previous, &ignored) != FALSE;
  Check(restored, "restore own page access");
  if (!restored) return;
  Check(std::memcmp(memory, memory_before.data(), memory_before.size()) == 0,
        "callbacks never write even the synthetic timer allocation");

  phase::Disarm();
  Invoke(sub_82131008, context, base);
  Check(bindings[5].match.snapshot.snapshot_available && bindings[5].match.snapshot.callback_id == 0 &&
        bindings[5].match.snapshot.timer_counter == 0x12345678 && state->callback_used == 6,
        "phase disarm retains timer availability with ID0, never latest callback fallback");
  Check(originals == 6 && original_kinds == std::array<unsigned, 3>{1, 1, 4},
        "all six mock-original executions accounted exactly once");

  mcla::guest_source_scout::detail::Sequence sequence;
  sequence.begin = scout_begin; sequence.frequency = hz;
  for (size_t i = 0; i < binding_count; ++i) {
    auto& slot = sequence.slots[i]; slot.has_capture = true;
    slot.paired.provenance.presenter_epoch = 7;
    slot.paired.provenance.acquired_sequence = i + 1;
    slot.paired.provenance.swap_emission = bindings[i];
  }
  Reject([&] { observer::Serialize(sequence, kToken, other_profile); },
         "phase/swap cannot silently change selected profile");
  Reject([&] { observer::Serialize(sequence, kToken, static_cast<Profile>(2)); },
         "phase/swap rejects unknown profile");
  const auto correct_end = observer::window.end_qpc;
  observer::window.end_qpc++;
  Reject([&] { observer::Serialize(sequence, kToken, profile); },
         "phase/swap rejects changed exclusive end");
  observer::window.end_qpc = correct_end;
  sequence.begin++;
  Reject([&] { observer::Serialize(sequence, kToken, profile); },
         "phase/swap rejects changed source begin");
  sequence.begin = scout_begin;
  const auto swap_json = Json::parse(observer::Serialize(sequence, kToken, profile));
  const auto phase_json = Json::parse(phase::Finish({}, kToken));
  Check(swap_json["schema"] == "mcla-game-swap-observer-v2" &&
        phase_json["schema"] == "mcla-phase-census-v3", "actual serializers emit swap v2 and phase v3 schemas");
  Check(swap_json["token"] == kToken && phase_json["token"] == kToken &&
        swap_json["epoch"] == kEpoch, "shared full session token and transport epoch");
  Check(swap_json["records"].size() == count &&
        swap_json["begin_qpc"] == begin && phase_json["begin_qpc"] == begin &&
        swap_json["end_qpc_exclusive"] == end && phase_json["end_qpc_exclusive"] == end,
        "selected profile has exact record count and shared prewindow/exclusive end");
  Check(end - begin == hz + count * hz / 10,
        "selected profile preserves one-second prewindow");
  for (size_t i = binding_count; i < count; ++i)
    Check(swap_json["records"][i]["slot"] == i && swap_json["records"][i]["binding"].is_null() &&
          swap_json["records"][i]["acquired_sequence"].is_null(),
          "all requested uncollected rows retained without fabricated binding");
  Check(phase_json["counts"]["stored_callbacks"] == 6 && phase_json["counts"]["active_scopes"] == 0 &&
        phase_json["counts"]["stored_scopes"] == 5 && phase_json["counts"]["completed_scopes"] == 5,
        "complete journal includes failed-read callbacks but no post-disarm admissions");
  for (size_t i = 0; i < 6; ++i) {
    const auto& row = phase_json["swap_callbacks"][i];
    Check(row["callback_id"] == i + 1 && row["swap_token_bound"] == false,
          "serialized phase identities consecutive and no premature binding claim");
    const auto& snap = swap_json["records"][i]["binding"]["snapshot"];
    if (i < 3) Check(snap["callback_id"] == i + 1, "serialized available snapshot preserves its ID");
    else if (i < 5) Check(snap.is_null(), "serialized unavailable snapshot has no leaked callback ID");
    else Check(snap["callback_id"] == 0, "serialized available but unjoined snapshot explicit zero ID");
  }
  Check(phase_json["swap_callbacks"][3]["has_scope"] == false &&
        phase_json["swap_callbacks"][4]["has_scope"] == true &&
        phase_json["swap_callbacks"][5]["has_scope"] == false,
        "failed reads retain actual scoped/unscoped callback inventory");
  for (const auto* key : {"simulation_proven", "fps_proven", "frame_to_step_mapping_proven"})
    Check(swap_json[key] == false && phase_json[key] == false, "neither serializer promotes synthetic proof");
  Check(!phase_json["observer_failed"].get<bool>() && !phase_json["drain_incomplete"].get<bool>() &&
        !phase_json["invalid_qpc"].get<bool>(), "synthetic host integration completes healthy");
  const auto frozen = phase::Finish({}, kToken);
  SetLastError(77);
  Check(phase::ObserveSwap(context, base) == 0 && GetLastError() == 77 &&
        phase::Finish({}, kToken) == frozen, "post-freeze zero ID cannot mutate serialized journal");
}
}  // namespace

REX_EXTERN(__imp__sub_822C1FA8) { Original(0, ctx, base); }
REX_EXTERN(__imp__sub_82305D00) { Original(1, ctx, base); }
REX_EXTERN(__imp__sub_82131008) { Original(2, ctx, base); }

int main(int argc, char** argv) {
  if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--short")) return 2;
  const auto profile = argc == 2 ? Profile::kShort31 : Profile::kLegacy91;
  auto* memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  Check(memory != nullptr, "allocate bounded private synthetic timer page");
  try { if (memory) Exercise(memory, profile); }
  catch (const std::exception& error) { Check(false, error.what()); }
  catch (...) { Check(false, "unexpected synthetic integration exception"); }
  observer::Disarm(); phase::Disarm();
  if (memory) Check(VirtualFree(memory, 0, MEM_RELEASE) != FALSE, "release own synthetic allocation");
  std::cout << "phase/swap observer glue: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
