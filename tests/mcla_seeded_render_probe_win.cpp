// Test-only composition: reuse the pinned passive helper's production admission,
// owned process, target table, exact BMP validation and bounded filesystem IO.
#define MCLA_RENDER_PROBE_NO_MAIN
#include "mcla_render_target_probe_win.cpp"
#include "mcla_launcher_snapshot_win.h"
#include "mcla_build_lease_policy.h"
#include <limits>

namespace {
constexpr std::string_view kSeedReceipt =
    "D682CB13F81E3035F0EFBB0B894AECDFB7093410A473045D15AEA4FD6A4B1665";
constexpr std::u16string_view kSeedPath =
    u"c:/bdu/mcla-saves/mcla-f5d835a679843f1e95de84646d2a8646/"
    u"sessions/a77101732ba666eab05f5a123db5949b/before-user";
constexpr uint64_t kInspectionMilliseconds = 300000;
constexpr uint64_t kCloseGraceMilliseconds = 10000;
#if defined(MCLA_FAULT_RENDER_PROBE)
constexpr bool kFaultProbe = true;
#else
constexpr bool kFaultProbe = false;
#endif
#if defined(MCLA_PRODUCER_RENDER_PROBE)
constexpr bool kProducerProbe = true;
#else
constexpr bool kProducerProbe = false;
#endif
#if defined(MCLA_UPLOAD_RENDER_PROBE)
constexpr bool kUploadProbe = true;
#else
constexpr bool kUploadProbe = false;
#endif
#if defined(MCLA_ATLAS_RENDER_PROBE)
constexpr bool kAtlasProbe = true;
#else
constexpr bool kAtlasProbe = false;
#endif
#if defined(MCLA_RENDERDOC_RENDER_PROBE)
constexpr bool kRenderDocProbe = true;
#else
constexpr bool kRenderDocProbe = false;
#endif
constexpr uint64_t kCaptureAckMilliseconds = kRenderDocProbe ? 75000 : 15000;
#if defined(MCLA_CAMERA_RENDER_PROBE)
constexpr bool kCameraProbe = true;
#else
constexpr bool kCameraProbe = false;
#endif
static_assert(!kAtlasProbe || (kRenderDocProbe && !kCameraProbe));
#if defined(MCLA_CLEAN_VISUAL_RENDER_PROBE)
constexpr bool kCleanVisualProbe = true;
#else
constexpr bool kCleanVisualProbe = false;
#endif
static_assert(!kCleanVisualProbe || (!kRenderDocProbe && !kCameraProbe && !kAtlasProbe));
static_assert(!kUploadProbe || kAtlasProbe);
static_assert(!kProducerProbe || kUploadProbe);
static_assert(!kFaultProbe || kProducerProbe);
constexpr size_t kReceiptLimit = 1024 * 1024;
enum class RouteMode { Manual, VinylEntry, VinylCycle, VinylPair, StartupPrefix, VinylPairSlowStartup };
constexpr bool IsVinylPair(RouteMode mode) {
  return mode == RouteMode::VinylPair || mode == RouteMode::VinylPairSlowStartup;
}
constexpr std::string_view RouteName(RouteMode mode) {
  if (mode == RouteMode::StartupPrefix) return "startup-prefix-v1";
  if (mode == RouteMode::VinylPairSlowStartup) return "vinyl-pair-v3";
  return mode == RouteMode::VinylPair ? "vinyl-pair-v2" : mode == RouteMode::VinylCycle ? "vinyl-cycle-v1" :
      mode == RouteMode::VinylEntry ? "vinyl-entry-v1" : "manual";
}
constexpr size_t RouteCommands(RouteMode mode) {
  if (mode == RouteMode::StartupPrefix) return 2;
  return IsVinylPair(mode) ? 10 : mode == RouteMode::VinylCycle ? 15 : mode == RouteMode::VinylEntry ? 9 : 0;
}
constexpr size_t RouteCaptures(RouteMode mode) {
  if (mode == RouteMode::StartupPrefix) return 2;
  return IsVinylPair(mode) ? 2 : mode == RouteMode::VinylCycle ? 7 : mode == RouteMode::VinylEntry ? 1 : 0;
}
constexpr bool CaptureDue(size_t completed_commands, size_t completed_captures) {
  return completed_commands >= 9 && completed_captures < completed_commands - 8;
}
constexpr bool RouteClosesOnCompletion(RouteMode mode) {
  return mode == RouteMode::VinylCycle || IsVinylPair(mode) || mode == RouteMode::StartupPrefix;
}
constexpr bool RoutePurchases(RouteMode mode) {
  return mode == RouteMode::VinylCycle || IsVinylPair(mode);
}
constexpr bool PrefixProgressValid(size_t commands, size_t captures) {
  return (commands == 0 && captures == 0) || (commands == 1 && captures <= 1) ||
      (commands == 2 && (captures == 1 || captures == 2));
}
constexpr std::string_view PrefixMetadata(RouteMode mode) {
  return mode == RouteMode::StartupPrefix ?
      "\nprefix_first_capture_after_command 1\nprefix_first_settle_ms 30000"
      "\nprefix_second_capture_after_command 2\nprefix_second_settle_ms 0"
      "\nprefix_claim startup_transition_inspection_not_garage_or_editor" : "";
}
constexpr std::string_view SlowStartupMetadata(RouteMode mode) {
  return mode == RouteMode::VinylPairSlowStartup ?
      "\npair_startup_settle_after_command 1\npair_startup_settle_ms 80000"
      "\npair_startup_claim fixed_wait_not_scene_readiness" : "";
}
constexpr bool PairProgressValid(size_t completed_commands, size_t completed_captures) {
  return completed_commands < 8 ? completed_captures == 0 :
      completed_commands == 8 ? completed_captures <= 1 :
      completed_commands == 9 ? completed_captures == 1 :
      completed_commands == 10 && (completed_captures == 1 || completed_captures == 2);
}
constexpr bool CaptureDue(RouteMode mode, size_t completed_commands, size_t completed_captures) {
  if (mode == RouteMode::StartupPrefix)
    return (completed_commands == 1 && completed_captures == 0) ||
        (completed_commands == 2 && completed_captures == 1);
  if (IsVinylPair(mode))
    return (completed_commands == 8 && completed_captures == 0) ||
        (completed_commands == 10 && completed_captures == 1);
  return CaptureDue(completed_commands, completed_captures);
}
struct SeedOptions { Options passive; std::wstring seed, receipt; RouteMode route = RouteMode::Manual; bool center = false; bool atlas_on = false; bool upload_on = false; bool producer_on = false; bool fault_on = false; };
struct RouteStep { unsigned sequence; std::string_view action, buttons; uint64_t settle_ms; };
constexpr std::array<RouteStep, 15> kVinylRoute{{
    {1, "START", "0010", 30000}, {2, "START", "0010", 0},
    {3, "DPAD_DOWN", "0002", 0}, {4, "A", "1000", 15000},
    {5, "DPAD_DOWN", "0002", 0}, {6, "A", "1000", 0},
    {7, "DPAD_DOWN", "0002", 0}, {8, "DPAD_DOWN", "0002", 0},
    {9, "A", "1000", 0}, {10, "A", "1000", 0},
    {11, "B", "2000", 0}, {12, "A", "1000", 0},
    {13, "A", "1000", 0}, {14, "B", "2000", 0}, {15, "A", "1000", 0}}};
constexpr RouteStep StepFor(RouteMode mode, size_t index) {
  auto step = kVinylRoute[index];
  // The same first ten grounded commands; only the pair's capture boundaries
  // and post-ACK settling change. No ninth input precedes the outside capture.
  // v3 changes only the first START wait to 80s; this is NOT a readiness gate.
  if (IsVinylPair(mode)) {
    if (step.sequence == 8) step.settle_ms = 8000;
    if (step.sequence == 10) step.settle_ms = 20000;
  }
  if (mode == RouteMode::VinylPairSlowStartup && step.sequence == 1) step.settle_ms = 80000;
  return step;
}
// Existing host protocol slot names only: these are NOT semantic assertions
// about purchases/vehicle switching. Legacy images follow commands 9 through 15;
// vinyl-pair-v2/v3 reuse the first two slots after settled commands 8 and 10.
// startup-prefix-v1 reuses those slots after START commands 1 and 2 only.
constexpr std::array<std::string_view, 7> kCaptureSlots{
    "garage-main-before", "showroom-purchase-confirmed", "visual-purchase-confirmed",
    "performance-purchase-confirmed", "paint-purchase-confirmed", "vehicle-switch-confirmed",
    "free-roam-after-garage"};

bool SeedPaths(std::wstring_view seed, std::wstring_view receipt) {
  const auto s = NormalizeRoot(std::filesystem::path(seed).u16string());
  const auto r = NormalizeRoot(std::filesystem::path(receipt).u16string());
  if (!s || !r || *s != kSeedPath) return false;
  return *r == s->substr(0, s->size() - std::u16string_view(u"before-user").size()) +
      u"before-snapshot.txt";
}
std::optional<SeedOptions> ParseSeed(std::span<const std::wstring_view> arguments) {
  if (arguments.size() == 1 && arguments[0] == L"--self-test")
    return SeedOptions{Options{L"", L"", L"", L"", false, true}, L"", L""};
  SeedOptions out;
  bool sample_seen = false;
  bool atlas_seen = false;
  bool upload_seen = false;
  bool producer_seen = false;
  bool fault_seen = false;
  std::vector<std::wstring_view> passive;
  for (const auto arg : arguments) {
    if (arg.starts_with(L"--seed=")) {
      if (!out.seed.empty()) return {};
      out.seed = arg.substr(7);
      if (out.seed.empty()) return {};
    } else if (arg.starts_with(L"--receipt=")) {
      if (!out.receipt.empty()) return {};
      out.receipt = arg.substr(10);
      if (out.receipt.empty()) return {};
    } else if (arg.starts_with(L"--route=")) {
      if (out.route != RouteMode::Manual) return {};
      if (arg == L"--route=vinyl-entry-v1") out.route = RouteMode::VinylEntry;
      else if (arg == L"--route=vinyl-cycle-v1") out.route = RouteMode::VinylCycle;
      else if (arg == L"--route=vinyl-pair-v2") out.route = RouteMode::VinylPair;
      else if (arg == L"--route=vinyl-pair-v3" && kCleanVisualProbe) out.route = RouteMode::VinylPairSlowStartup;
      else if (arg == L"--route=startup-prefix-v1" && kCleanVisualProbe) out.route = RouteMode::StartupPrefix;
      else return {};
    } else if (arg.starts_with(L"--atlas-witness=")) {
      if (!kAtlasProbe || atlas_seen) return {};
      atlas_seen = true;
      if (arg == L"--atlas-witness=on") out.atlas_on = true;
      else if (arg != L"--atlas-witness=off") return {};
    } else if (arg.starts_with(L"--atlas-upload=")) {
      if (!kUploadProbe || upload_seen) return {};
      upload_seen = true;
      if (arg == L"--atlas-upload=on") out.upload_on = true;
      else if (arg != L"--atlas-upload=off") return {};
    } else if (arg.starts_with(L"--atlas-producer=")) {
      if (!kProducerProbe || producer_seen) return {};
      producer_seen = true;
      if (arg == L"--atlas-producer=on") out.producer_on = true;
      else if (arg != L"--atlas-producer=off") return {};
    } else if (arg.starts_with(L"--atlas-fault=")) {
      if (!kFaultProbe || fault_seen) return {};
      fault_seen = true;
      if (arg == L"--atlas-fault=on") out.fault_on = true;
      else if (arg != L"--atlas-fault=off") return {};
    } else if (arg.starts_with(L"--readback-sample=")) {
      if (!kRenderDocProbe || sample_seen) return {};
      sample_seen = true;
      if (arg == L"--readback-sample=center") out.center = true;
      else if (arg != L"--readback-sample=corner") return {};
    } else passive.push_back(arg);
  }
  const auto options = Parse(passive);
  if (!options || options->self_test || !SeedPaths(out.seed, out.receipt)) return {};
  if (out.center && options->target != L"1440") return {};
  // This diagnostic is admitted only on the normal-host clean visual helper,
  // at the observed failing target. It is not a new general input route.
  if ((out.route == RouteMode::StartupPrefix || out.route == RouteMode::VinylPairSlowStartup) &&
      (!kCleanVisualProbe || options->target != L"2160")) return {};
  if (kAtlasProbe && (!atlas_seen ||
      (options->target != L"720" && options->target != L"1440") ||
      (out.route != RouteMode::Manual && out.route != RouteMode::VinylPair))) return {};
  if (kUploadProbe && (!upload_seen || !out.atlas_on || out.center || options->target != L"1440" ||
      out.route != RouteMode::VinylPair)) return {};
  if (kProducerProbe && (!producer_seen || out.upload_on)) return {};
  if (kFaultProbe && (!fault_seen || out.producer_on)) return {};
  out.passive = *options;
  return out;
}
std::vector<std::wstring> SeedArguments(const Options &options, const Target &target,
    const std::filesystem::path &root, unsigned adapter, bool center = false, bool atlas_on = false,
    bool upload_on = false, bool producer_on = false, bool fault_on = false) {
  Require(kUploadProbe || !upload_on, "upload selection requires the dedicated upload helper");
  Require(kProducerProbe || !producer_on, "producer selection requires the dedicated producer helper");
  Require(!kProducerProbe || !upload_on, "producer witness requires upload OFF");
  Require(kFaultProbe || !fault_on, "fault selection requires the dedicated fault helper");
  Require(!kFaultProbe || !producer_on, "fault witness requires producer OFF");
  Require(!center || (kRenderDocProbe && options.target == L"1440"), "center readback requires RenderDoc 1440 target");
  Require(!kUploadProbe || (atlas_on && !center && options.target == L"1440"),
      "upload witness requires atlas ON, corner and target1440");
  auto args = Arguments(options, target, root, adapter);
  std::erase(args, L"--mcla_first_frame_probe=true");
  std::erase(args, L"--mcla_first_frame_settle_seconds=3");
  args.push_back(L"--mcla_garage_lifecycle_probe=true");
  args.push_back(L"--mcla_garage_lifecycle_cycle=1");
  if (kCameraProbe) args.push_back(L"--mcla_garage_camera_probe=true");
  if (kRenderDocProbe) {
    args.push_back(L"--mcla_garage_renderdoc_probe=true");
    args.push_back(center ? L"--readback_resolve_half_pixel_offset=true" : L"--readback_resolve_half_pixel_offset=false");
  }
  if (kAtlasProbe) args.push_back(atlas_on ? L"--mcla_atlas_witness=true" : L"--mcla_atlas_witness=false");
  if (kUploadProbe) args.push_back(upload_on ? L"--mcla_atlas_upload=true" : L"--mcla_atlas_upload=false");
  if (kProducerProbe) args.push_back(producer_on ? L"--mcla_atlas_producer=true" : L"--mcla_atlas_producer=false");
  if (kFaultProbe) args.push_back(fault_on ? L"--mcla_atlas_fault=true" : L"--mcla_atlas_fault=false");
  args.push_back(L"--mcla_first_frame_settle_seconds=35");
  return args;
}
bool Expired(uint64_t started, uint64_t now, uint64_t limit) {
  return now < started || now - started >= limit;
}

class HeldReceipt {
 public:
  explicit HeldReceipt(const std::filesystem::path &path) {
    const auto native = L"\\\\?\\" + path.wstring();
    handle_ = CreateFileW(native.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    Require(handle_ != INVALID_HANDLE_VALUE, "cannot read-pin immutable receipt");
  }
  ~HeldReceipt() { if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
  HeldReceipt(const HeldReceipt &) = delete;
  HeldReceipt &operator=(const HeldReceipt &) = delete;
 private:
  HANDLE handle_ = INVALID_HANDLE_VALUE;
};
std::string Receipt(const fs::DirectoryGuard &session) {
  const auto bytes = Take(fs::ReadFile(session, L"before-snapshot.txt", kReceiptLimit));
  Require(Take(mcla::lease::Sha256(bytes)) == kSeedReceipt, "immutable seed receipt digest mismatch");
  const std::string text(bytes.begin(), bytes.end());
  Require(text.starts_with("MCLA-SAVE-SNAPSHOT-V1\n"), "seed receipt format mismatch");
  return text;
}
bool CloseRequested(const fs::DirectoryGuard &root) {
  std::error_code error;
  const bool exists = std::filesystem::exists(std::filesystem::path(root.path()) / L"close.request", error);
  Require(!error, "cannot inspect owned close request");
  if (!exists) return false;
  const auto bytes = Take(fs::ReadFile(root, L"close.request", 16));
  Require(std::string(bytes.begin(), bytes.end()) == "close\n", "invalid owned close request");
  return true;
}

// Match complete protocol records, not partial/truncated lines or sequence prefixes.
bool LogRecord(std::string_view log, std::string_view prefix, std::string_view suffix = {}) {
  size_t at = 0;
  while ((at = log.find(prefix, at)) != log.npos) {
    const auto end = log.find('\n', at);
    if (end == log.npos) return false;
    auto record = log.substr(at, end - at);
    if (record.ends_with('\r')) record.remove_suffix(1);
    if (suffix.empty() ? record == prefix : record.ends_with(suffix)) return true;
    at = end + 1;
  }
  return false;
}
// Re-read the bounded transcript each tick. Exact complete runtime records only;
// neither requested argv nor a partial log line constitutes observed state.
struct RuntimeWitness {
  bool ready = false, pending_record = false;
  std::array<bool, 2> phase{}, requested{}, complete{};
  std::array<size_t, 2> phase_offset{};
  std::array<uint64_t, 2> request_sequence{}, completed_sequence{};
};
// Fixed host protocol grammar, not a general key/value parser. Reject signs,
// leading zeroes, overflow, missing fields, alternate order and trailing text.
bool ConsumeCaptureNumber(std::string_view &text, std::string_view field, uint64_t &value) {
  if (!text.starts_with(field)) return false;
  text.remove_prefix(field.size());
  const auto end = text.find(' ');
  const auto digits = text.substr(0, end);
  if (digits.empty() || (digits.size() > 1 && digits.front() == '0')) return false;
  value = 0;
  for (const char digit : digits) {
    if (digit < '0' || digit > '9' ||
        value > (std::numeric_limits<uint64_t>::max() - uint64_t(digit - '0')) / 10) return false;
    value = value * 10 + uint64_t(digit - '0');
  }
  text.remove_prefix(digits.size());
  return true;
}
bool CaptureRequestFields(std::string_view fields, uint64_t &sequence) {
  return ConsumeCaptureNumber(fields, " source_present_seq=", sequence) && fields.empty() && sequence > 0;
}
bool CaptureReadyFields(std::string_view fields, size_t slot, uint64_t requested_sequence,
    uint64_t &completed_sequence) {
  uint64_t bytes = 0, before = 0, after = 0, guest_before = 0, guest_after = 0;
  // Mirrors the pinned host's 8 GiB capture cap and two-capture policy.
  constexpr uint64_t max_bytes = 8ull * 1024 * 1024 * 1024;
  if (!ConsumeCaptureNumber(fields, " bytes=", bytes) ||
      !ConsumeCaptureNumber(fields, " count_before=", before) ||
      !ConsumeCaptureNumber(fields, " count_after=", after) ||
      !ConsumeCaptureNumber(fields, " guest_sequence_before=", guest_before) ||
      !ConsumeCaptureNumber(fields, " guest_sequence_after=", guest_after) ||
      fields != " replay_pending=1 multi_present_state_not_bmp=1") return false;
  if (slot >= 2 || !requested_sequence || !bytes || bytes > max_bytes || before != slot || after != slot + 1 ||
      guest_before < requested_sequence || guest_before > std::numeric_limits<uint64_t>::max() - 2 ||
      guest_after < guest_before + 2) return false;
  completed_sequence = guest_after;
  return true;
}
std::string RuntimeRecord(std::string_view phase, bool center, unsigned scale) {
  return "MCLA_RENDERDOC_RUNTIME v=1 phase=" + std::string(phase) + " half_pixel=" +
      (center ? "1" : "0") + " readback=fast scale_x=" + std::to_string(scale) +
      " scale_y=" + std::to_string(scale) + " status=PASS";
}
std::optional<RuntimeWitness> InspectRuntime(std::string_view log, bool center, unsigned scale) {
  RuntimeWitness out;
  size_t start = 0;
  while (start < log.size()) {
    const auto end = log.find('\n', start);
    if (end == log.npos) {
      const auto tail = log.substr(start);
      out.pending_record = tail.find("MCLA_RENDERDOC_RUNTIME") != tail.npos ||
          tail.find("MCLA_RENDERDOC_CAPTURE") != tail.npos;
      break; // Writer may still be completing this line; do not act on it.
    }
    auto line = log.substr(start, end - start);
    if (line.ends_with('\r')) line.remove_suffix(1);
    const auto runtime = line.find("MCLA_RENDERDOC_RUNTIME");
    const auto capture = line.find("MCLA_RENDERDOC_CAPTURE");
    if (runtime != line.npos) {
      if (capture != line.npos) return {};
      const auto record = line.substr(runtime);
      if (record == RuntimeRecord("ready", center, scale)) {
        if (out.ready || out.phase[0] || out.phase[1]) return {};
        out.ready = true;
      } else {
        const size_t slot = record == RuntimeRecord("outside", center, scale) ? 0 :
            record == RuntimeRecord("editor", center, scale) ? 1 : 2;
        if (slot == 2 || !out.ready || out.phase[slot] || (slot == 1 && !out.complete[0])) return {};
        out.phase[slot] = true;
        out.phase_offset[slot] = start + runtime;
      }
    } else if (capture != line.npos) {
      const auto record = line.substr(capture);
      const std::string prefix = "MCLA_RENDERDOC_CAPTURE v=1 state=";
      const size_t slot = record.starts_with(prefix + "outside ") ? 0 :
          record.starts_with(prefix + "editor ") ? 1 : 2;
      if (slot == 2 || !out.phase[slot]) return {};
      const auto state = prefix + (slot == 0 ? "outside" : "editor");
      const auto requested_prefix = state + " status=REQUESTED";
      const auto ready_prefix = state + " status=FILE_READY";
      if (record.starts_with(requested_prefix)) {
        if (out.requested[slot] || out.complete[slot] ||
            !CaptureRequestFields(record.substr(requested_prefix.size()), out.request_sequence[slot]) ||
            (slot == 1 && out.request_sequence[slot] < out.completed_sequence[0])) return {};
        out.requested[slot] = true;
      } else if (record.starts_with(ready_prefix)) {
        if (!out.requested[slot] || out.complete[slot] ||
            !CaptureReadyFields(record.substr(ready_prefix.size()), slot, out.request_sequence[slot],
                out.completed_sequence[slot])) return {};
        out.complete[slot] = true;
      } else return {}; // Includes FAIL; no retry or silent format fallback.
    }
    start = end + 1;
  }
  return out;
}
bool CaptureAttested(const RuntimeWitness &witness, size_t slot, size_t request_log_boundary) {
  return slot < 2 && witness.ready && !witness.pending_record && witness.complete[slot] &&
      witness.phase_offset[slot] >= request_log_boundary;
}
bool CommandAcknowledged(std::string_view log, const RouteStep &step) {
  const auto sequence = std::to_string(step.sequence);
  for (const auto side : {"source", "guest"}) {
    const auto prefix = "MCLA_FRONTEND_SMOKE_INPUT v=1 side=" + std::string(side) +
        " sequence=" + sequence + " buttons=";
    if (!LogRecord(log, prefix + std::string(step.buttons)) || !LogRecord(log, prefix + "0000")) return false;
  }
  return LogRecord(log, "MCLA_GARAGE_CONTROL v=1 sequence=" + sequence +
      " action=" + std::string(step.action) + " capture=0 width=0 height=0 present_seq=", " status=PASS");
}
bool RequestAbsent(const fs::DirectoryGuard &user, const wchar_t *name) {
  const auto path = std::filesystem::path(user.path()) / name;
  const auto attributes = GetFileAttributesW(path.c_str());
  if (attributes != INVALID_FILE_ATTRIBUTES) return false;
  const auto error = GetLastError();
  Require(error == ERROR_FILE_NOT_FOUND, "cannot inspect owned request absence");
  return true;
}
struct VinylRouteState {
  size_t next = 0, captures = 0;
  bool sent = false, settling = false, capture_sent = false, complete = false;
  uint64_t action_time = 0;
  size_t capture_log_boundary = 0;
  // One nonblocking tick per owned-process/close/deadline check in SeedProbe.
  void Tick(RouteMode mode, uint64_t started, uint64_t now, bool ready, std::string_view log,
      const fs::DirectoryGuard &user, const Target &target, const RuntimeWitness *runtime = nullptr) {
    if (complete) return;
    Require(mode != RouteMode::Manual, "manual route must never tick automatic input");
    Require(!IsVinylPair(mode) || PairProgressValid(next, captures), "vinyl pair progress bound/order");
    Require(mode != RouteMode::VinylPairSlowStartup || kCleanVisualProbe, "slow startup requires clean visual helper");
    Require(mode != RouteMode::StartupPrefix || (kCleanVisualProbe && PrefixProgressValid(next, captures)),
        "startup prefix progress bound/order");
    Require(!Expired(started, now, kInspectionMilliseconds), "route overall inspection deadline");
    if (!ready) {
      Require(!Expired(started, now, 90000), "route startup configuration deadline");
      return;
    }
    Require(!kRenderDocProbe || (runtime && runtime->ready), "route runtime readiness attestation missing");
    constexpr auto control = L".mcla-garage-control.request";
    // Gate subsequent commands on every due capture's exact ACK, file
    // consumption and BMP shape. Never retry or skip a protocol slot.
    if (CaptureDue(mode, next, captures)) {
      Require(captures < RouteCaptures(mode) && captures < kCaptureSlots.size(), "route capture bound");
      const auto id = std::string(kCaptureSlots[captures]);
      const auto basename = "mcla-garage-lifecycle-" + id;
      const auto capture = std::filesystem::path("." + basename + ".request").wstring();
      if (!capture_sent) {
        Require(RequestAbsent(user, control) && RequestAbsent(user, capture.c_str()), "route refuses existing capture/control request");
        capture_log_boundary = log.size();
        Write(user, capture.c_str(), "capture\n");
        capture_sent = true; action_time = now;
      } else if (LogRecord(log, "MCLA_GARAGE_LIFECYCLE_FRAME v=1 cycle=1 phase=" +
          std::to_string(captures + 1) + " id=" + id + " width=" +
          std::to_string(target.width) + " height=" + std::to_string(target.height) + " present_seq=", " status=PASS") &&
          RequestAbsent(user, capture.c_str())) {
        if (kRenderDocProbe && captures < 2)
          Require(runtime && CaptureAttested(*runtime, captures, capture_log_boundary), "capture runtime attestation missing or predates request");
        const auto frame = ReadBmp(std::filesystem::path(user.path()) / (basename + ".bmp"));
        Require(frame && *frame == std::pair{target.width, target.height}, "route capture BMP mismatch");
        ++captures; capture_sent = false;
        std::cout << "ROUTE_CAPTURE_COLLECTED slot=" << captures << " after_sequence=" << next <<
            " protocol_id=" << id << " semantic_claim=none\n" << std::flush;
        if (captures == RouteCaptures(mode)) {
          complete = true;
          std::cout << (mode == RouteMode::VinylEntry ?
              "ROUTE_STOPPED_FOR_PROMPT_REVIEW_NO_AUTOMATIC_PURCHASE next_manual_sequence=10\n" :
              mode == RouteMode::StartupPrefix ? "ROUTE_STARTUP_PREFIX_CAPTURED_CLOSING_NO_SCENE_VERDICT\n" :
              IsVinylPair(mode) ? "ROUTE_PAIR_CAPTURED_CLOSING_NO_GRAPHICS_VERDICT\n" :
              "ROUTE_CYCLE_CAPTURED_CLOSING_NO_GRAPHICS_VERDICT\n") << std::flush;
        }
      } else Require(!Expired(action_time, now, kCaptureAckMilliseconds), "route capture acknowledgment deadline");
      return;
    }
    if (next < RouteCommands(mode)) {
      const auto step = StepFor(mode, next);
      if (settling) {
        if (!Expired(action_time, now, step.settle_ms)) return;
        if (IsVinylPair(mode) && (step.sequence == 8 || step.sequence == 10 ||
            (mode == RouteMode::VinylPairSlowStartup && step.sequence == 1))) {
          Require(now >= action_time, "pair settle clock regressed");
          std::cout << "ROUTE_PAIR_SETTLED sequence=" << step.sequence << " elapsed_ms=" << now - action_time <<
              " required_ms=" << step.settle_ms << '\n' << std::flush;
        }
        if (mode == RouteMode::StartupPrefix) {
          Require(now >= action_time, "prefix settle clock regressed");
          std::cout << "ROUTE_PREFIX_SETTLED sequence=" << step.sequence << " elapsed_ms=" << now - action_time <<
              " required_ms=" << step.settle_ms << '\n' << std::flush;
        }
        settling = false; ++next;
        return;
      }
      if (!sent) {
        Require(RequestAbsent(user, control), "route refuses existing control request");
        Write(user, control, std::to_string(step.sequence) + " " + std::string(step.action) + "\n");
        sent = true; action_time = now;
        std::cout << "ROUTE_COMMAND_SENT sequence=" << step.sequence << " action=" << step.action << '\n' << std::flush;
      } else if (CommandAcknowledged(log, step) && RequestAbsent(user, control)) {
        sent = false; settling = true; action_time = now;
        std::cout << "ROUTE_COMMAND_ACKNOWLEDGED sequence=" << step.sequence << '\n' << std::flush;
      } else Require(!Expired(action_time, now, 45000), "route control acknowledgment deadline");
      return;
    }
    Require(false, "route exhausted without final capture");
  }
};

int SeedSelfTest() {
  unsigned checks = 0, failed = 0;
  const auto check = [&](bool ok) { ++checks; if (!ok) ++failed; };
  const auto seed = std::filesystem::path(kSeedPath).wstring();
  const auto receipt = (std::filesystem::path(seed).parent_path() / L"before-snapshot.txt").wstring();
  check(SeedPaths(seed, receipt));
  check(!SeedPaths(L"D" + seed.substr(1), L"D" + receipt.substr(1)));
  check(!SeedPaths(L"C:/fixtures/user", receipt));
  check(!SeedPaths(seed + L"/child", receipt));
  check(!SeedPaths(seed + L"/", receipt));
  check(!SeedPaths(seed, receipt + L":stream"));
  check(!SeedPaths(seed, L"C:/elsewhere/before-snapshot.txt"));
  check(!SeedPaths(seed, seed + L"/before-snapshot.txt"));
  check(!SeedPaths(L"//server/share/sessions/a77101732ba666eab05f5a123db5949b/before-user", receipt));
  check(!SeedPaths(L"C:/fixtures/sessions/a77101732ba666eab05f5a123db5949a/before-user", receipt));
  std::vector<std::wstring> owned{L"--build=C:/build", L"--game=D:/game", L"--gpu=GPU",
      kAtlasProbe ? L"--target=1440" : L"--target=2160", L"--seed=" + seed, L"--receipt=" + receipt, L"--run"};
  if (kAtlasProbe) owned.push_back(kUploadProbe ? L"--atlas-witness=on" : L"--atlas-witness=off");
  if (kUploadProbe) { owned.push_back(L"--atlas-upload=off"); owned.push_back(L"--route=vinyl-pair-v2"); }
  if (kProducerProbe) owned.push_back(L"--atlas-producer=off");
  if (kFaultProbe) owned.push_back(L"--atlas-fault=off");
  const auto parse = [&](const std::vector<std::wstring> &values) {
    std::vector<std::wstring_view> views(values.begin(), values.end()); return ParseSeed(views);
  };
  check(parse(owned).has_value());
  check(parse(owned)->route == (kUploadProbe ? RouteMode::VinylPair : RouteMode::Manual));
  check(!parse(owned)->center);
  check(parse(owned)->atlas_on == kUploadProbe);
  check(!parse(owned)->upload_on);
  check(!parse(owned)->producer_on);
  check(!parse(owned)->fault_on);
  auto prefix = owned;
  std::erase_if(prefix, [](const auto& value) { return value.starts_with(L"--route="); });
  prefix.push_back(L"--route=startup-prefix-v1");
  for (const auto target_name : {L"720", L"1080", L"1440", L"2160"}) {
    auto trial = prefix;
    std::erase_if(trial, [](const auto& value) { return value.starts_with(L"--target="); });
    trial.push_back(L"--target=" + std::wstring(target_name));
    check(parse(trial).has_value() == (kCleanVisualProbe && std::wstring_view(target_name) == L"2160"));
    if (const auto parsed = parse(trial)) {
      check(parsed->route == RouteMode::StartupPrefix && !parsed->center && !parsed->atlas_on &&
          !parsed->upload_on && !parsed->producer_on && !parsed->fault_on);
      const auto child = SeedArguments(parsed->passive, *TargetFor(target_name), L"C:/Temp/fixture", 1);
      const auto normal = SeedArguments(parse(owned)->passive, *TargetFor(L"2160"), L"C:/Temp/fixture", 1);
      check(child == normal);  // Route remains helper-side, not an added host flag.
    }
  }
  for (const auto conflict : {L"--route=startup-prefix-v1", L"--route=vinyl-pair-v2",
      L"--route=vinyl-entry-v1", L"--route=vinyl-cycle-v1", L"--readback-sample=corner",
      L"--atlas-witness=on", L"--atlas-upload=off", L"--atlas-producer=off", L"--atlas-fault=off"}) {
    auto trial = prefix; trial.push_back(conflict); check(!parse(trial));
  }
  for (const auto bad : {L"startup-prefix-v2", L"startup-prefix-v1\n", L"startup-prefix-v1 ",
      L"STARTUP-PREFIX-V1", L"startup-prefix-v1=extra"}) {
    auto trial = owned; trial.push_back(L"--route=" + std::wstring(bad)); check(!parse(trial));
  }
  auto slow = owned;
  std::erase_if(slow, [](const auto& value) { return value.starts_with(L"--route="); });
  slow.push_back(L"--route=vinyl-pair-v3");
  for (const auto target_name : {L"720", L"1080", L"1440", L"2160"}) {
    auto trial = slow;
    std::erase_if(trial, [](const auto& value) { return value.starts_with(L"--target="); });
    trial.push_back(L"--target=" + std::wstring(target_name));
    check(parse(trial).has_value() == (kCleanVisualProbe && std::wstring_view(target_name) == L"2160"));
    if (const auto parsed = parse(trial)) {
      check(parsed->route == RouteMode::VinylPairSlowStartup && !parsed->center && !parsed->atlas_on &&
          !parsed->upload_on && !parsed->producer_on && !parsed->fault_on);
      check(SeedArguments(parsed->passive, *TargetFor(target_name), L"C:/Temp/fixture", 1) ==
          SeedArguments(parse(owned)->passive, *TargetFor(L"2160"), L"C:/Temp/fixture", 1));
    }
  }
  for (const auto conflict : {L"--route=vinyl-pair-v3", L"--route=startup-prefix-v1", L"--route=vinyl-pair-v2",
      L"--route=vinyl-entry-v1", L"--route=vinyl-cycle-v1", L"--readback-sample=corner",
      L"--atlas-witness=on", L"--atlas-upload=off", L"--atlas-producer=off", L"--atlas-fault=off"}) {
    auto trial = slow; trial.push_back(conflict); check(!parse(trial));
  }
  for (const auto bad : {L"vinyl-pair-v4", L"vinyl-pair-v3\n", L"vinyl-pair-v3 ",
      L"VINYL-PAIR-V3", L"vinyl-pair-v3=extra"}) {
    auto trial = owned; trial.push_back(L"--route=" + std::wstring(bad)); check(!parse(trial));
  }
  auto no_fault = owned;
  std::erase_if(no_fault, [](const auto& value) { return value.starts_with(L"--atlas-fault="); });
  check(parse(no_fault).has_value() == !kFaultProbe);
  for (const auto value : {L"off", L"on", L"", L"OFF", L"ON", L"true", L" on", L"off ", L"on\n", L"on=extra"}) {
    auto trial = no_fault; trial.push_back(L"--atlas-fault=" + std::wstring(value));
    check(parse(trial).has_value() == (kFaultProbe &&
        (std::wstring_view(value) == L"off" || std::wstring_view(value) == L"on")));
    if (const auto p = parse(trial)) check(p->fault_on == (std::wstring_view(value) == L"on"));
    trial.push_back(L"--atlas-fault=off"); check(!parse(trial));
  }
  if (kFaultProbe) {
    for (const auto name : {L"720", L"1080", L"1440", L"2160"})
      for (const auto sample : {L"corner", L"center"})
        for (const auto route_name : {L"", L"vinyl-pair-v2", L"vinyl-entry-v1", L"vinyl-cycle-v1"})
          for (const auto atlas : {L"off", L"on"}) for (const auto upload : {L"off", L"on"})
            for (const auto producer : {L"off", L"on"}) for (const auto fault : {L"off", L"on"}) {
              auto trial = no_fault;
              std::erase_if(trial, [](const auto& value) { return value.starts_with(L"--route=") ||
                  value.starts_with(L"--atlas-witness=") || value.starts_with(L"--atlas-upload=") ||
                  value.starts_with(L"--atlas-producer="); });
              trial[3] = L"--target=" + std::wstring(name);
              trial.push_back(L"--readback-sample=" + std::wstring(sample));
              if (std::wstring_view(route_name) != L"") trial.push_back(L"--route=" + std::wstring(route_name));
              trial.push_back(L"--atlas-witness=" + std::wstring(atlas));
              trial.push_back(L"--atlas-upload=" + std::wstring(upload));
              trial.push_back(L"--atlas-producer=" + std::wstring(producer));
              trial.push_back(L"--atlas-fault=" + std::wstring(fault));
              check(parse(trial).has_value() == (std::wstring_view(name) == L"1440" &&
                  std::wstring_view(sample) == L"corner" && std::wstring_view(route_name) == L"vinyl-pair-v2" &&
                  std::wstring_view(atlas) == L"on" && std::wstring_view(upload) == L"off" &&
                  std::wstring_view(producer) == L"off"));
            }
  }
  auto no_producer = owned;
  std::erase_if(no_producer, [](const auto& value) { return value.starts_with(L"--atlas-producer="); });
  check(parse(no_producer).has_value() == !kProducerProbe);
  for (const auto value : {L"off", L"on", L"", L"OFF", L"ON", L"true", L" on", L"off ", L"on\n", L"on=extra"}) {
    auto trial = no_producer; trial.push_back(L"--atlas-producer=" + std::wstring(value));
    check(parse(trial).has_value() == (kProducerProbe &&
        (std::wstring_view(value) == L"off" || (!kFaultProbe && std::wstring_view(value) == L"on"))));
    if (const auto p = parse(trial)) check(p->producer_on == (std::wstring_view(value) == L"on"));
    trial.push_back(L"--atlas-producer=off"); check(!parse(trial));
  }
  if (kProducerProbe) {
    for (const auto name : {L"720", L"1080", L"1440", L"2160"})
      for (const auto sample : {L"corner", L"center"})
        for (const auto route_name : {L"", L"vinyl-pair-v2", L"vinyl-entry-v1", L"vinyl-cycle-v1"})
          for (const auto upload : {L"off", L"on"}) for (const auto producer : {L"off", L"on"}) {
            auto trial = owned;
            std::erase_if(trial, [](const auto& value) { return value.starts_with(L"--route=") ||
                value.starts_with(L"--atlas-upload=") || value.starts_with(L"--atlas-producer="); });
            trial[3] = L"--target=" + std::wstring(name);
            trial.push_back(L"--readback-sample=" + std::wstring(sample));
            if (std::wstring_view(route_name) != L"") trial.push_back(L"--route=" + std::wstring(route_name));
            trial.push_back(L"--atlas-upload=" + std::wstring(upload));
            trial.push_back(L"--atlas-producer=" + std::wstring(producer));
            check(parse(trial).has_value() == (std::wstring_view(name) == L"1440" &&
                std::wstring_view(sample) == L"corner" && std::wstring_view(route_name) == L"vinyl-pair-v2" &&
                std::wstring_view(upload) == L"off" && (!kFaultProbe || std::wstring_view(producer) == L"off")));
          }
  }
  auto no_upload = owned;
  std::erase_if(no_upload, [](const auto& value) { return value.starts_with(L"--atlas-upload="); });
  check(parse(no_upload).has_value() == !kUploadProbe);
  for (const auto value : {L"off", L"on", L"", L"OFF", L"ON", L"true", L" on", L"off ", L"on\n", L"on=extra"}) {
    auto trial = no_upload; trial.push_back(L"--atlas-upload=" + std::wstring(value));
    check(parse(trial).has_value() == (kUploadProbe &&
        (std::wstring_view(value) == L"off" || (!kProducerProbe && std::wstring_view(value) == L"on"))));
    if (const auto p = parse(trial)) check(p->upload_on == (std::wstring_view(value) == L"on"));
    trial.push_back(L"--atlas-upload=on"); check(!parse(trial));
  }
  auto no_atlas = owned;
  std::erase_if(no_atlas, [](const auto& value) { return value.starts_with(L"--atlas-witness="); });
  check(parse(no_atlas).has_value() == !kAtlasProbe);
  for (const auto value : {L"off", L"on", L"", L"OFF", L"true", L" on", L"off ", L"on\n", L"on=extra"}) {
    auto trial = no_atlas;
    if (kAtlasProbe) trial[3] = L"--target=1440";
    trial.push_back(L"--atlas-witness=" + std::wstring(value));
    const bool valid = kAtlasProbe && (std::wstring_view(value) == L"on" ||
        (!kUploadProbe && std::wstring_view(value) == L"off"));
    check(parse(trial).has_value() == valid);
    if (const auto p = parse(trial)) check(p->atlas_on == (std::wstring_view(value) == L"on"));
    trial.push_back(L"--atlas-witness=off"); check(!parse(trial));
  }
  for (const auto value : {L"--mcla_atlas_witness=true", L"--mcla_atlas_witness=false", L"--atlas-witness",
      L"--mcla_atlas_upload=true", L"--mcla_atlas_upload=false", L"--atlas-upload", L"--dump-dir=C:/dump",
      L"--atlas-upload-dir=C:/dump", L"--mcla_atlas_upload_dir=C:/dump",
      L"--atlas-producer", L"--mcla_atlas_producer=true", L"--mcla_atlas_producer=false",
      L"--atlas-producer-dir=C:/dump", L"--mcla_atlas_producer_dir=C:/dump",
      L"--atlas-fault", L"--mcla_atlas_fault=true", L"--mcla_atlas_fault=false",
      L"--atlas-fault-dir=C:/dump", L"--mcla_atlas_fault_dir=C:/dump"}) {
    auto trial = owned; trial.push_back(value); check(!parse(trial));
  }
  for (const auto name : {L"720", L"1080", L"1440", L"2160"}) {
    for (const auto mode : {L"off", L"on"}) {
      auto trial = no_atlas; trial[3] = L"--target=" + std::wstring(name);
      std::erase(trial, std::wstring(L"--route=vinyl-pair-v2"));
      trial.push_back(L"--atlas-witness=" + std::wstring(mode));
      const bool valid = kAtlasProbe && (std::wstring_view(name) == L"720" || std::wstring_view(name) == L"1440");
      check(parse(trial).has_value() == (valid && !kUploadProbe)); // Manual route (omitted option).
      trial.push_back(L"--route=vinyl-pair-v2");
      check(parse(trial).has_value() == (valid && (!kUploadProbe ||
          (std::wstring_view(name) == L"1440" && std::wstring_view(mode) == L"on"))));
    }
  }
  for (const auto name : {L"720", L"1080", L"1440", L"2160"}) {
    auto sample = owned; sample[3] = L"--target=" + std::wstring(name);
    sample.push_back(L"--readback-sample=corner");
    check(parse(sample).has_value() == (kRenderDocProbe &&
        (!kUploadProbe || std::wstring_view(name) == L"1440") &&
        (!kAtlasProbe || std::wstring_view(name) == L"720" || std::wstring_view(name) == L"1440")));
    sample.back() = L"--readback-sample=center";
    check(parse(sample).has_value() == (kRenderDocProbe && !kUploadProbe && std::wstring_view(name) == L"1440"));
    if (const auto parsed = parse(sample)) check(parsed->center);
    sample.push_back(L"--readback-sample=corner"); check(!parse(sample));
  }
  for (const auto value : {L"", L"true", L"CENTER", L"center\n", L" center", L"center=extra", L"corner "}) {
    auto sample = owned; sample[3] = L"--target=1440";
    sample.push_back(L"--readback-sample=" + std::wstring(value)); check(!parse(sample));
  }
  auto duplicate_sample = owned; duplicate_sample.push_back(L"--readback-sample=corner");
  duplicate_sample.push_back(L"--readback-sample=corner"); check(!parse(duplicate_sample));
  auto raw_sample = owned; raw_sample.push_back(L"--readback_resolve_half_pixel_offset=true"); check(!parse(raw_sample));
  auto route = owned; route.push_back(L"--route=vinyl-entry-v1");
  check(kAtlasProbe ? !parse(route) : (parse(route).has_value() && parse(route)->route == RouteMode::VinylEntry));
  route.push_back(L"--route=vinyl-entry-v1"); check(!parse(route));
  route = owned; route.push_back(L"--route=vinyl-cycle-v1");
  check(kAtlasProbe ? !parse(route) : (parse(route).has_value() && parse(route)->route == RouteMode::VinylCycle));
  route.push_back(L"--route=vinyl-entry-v1"); check(!parse(route));
  route = owned; route.push_back(L"--route=vinyl-cycle-v1");
  route.push_back(L"--route=vinyl-cycle-v1"); check(!parse(route));
  route = owned;
  std::erase(route, std::wstring(L"--route=vinyl-pair-v2"));
  route.push_back(L"--route=vinyl-pair-v2");
  check(parse(route).has_value() && parse(route)->route == RouteMode::VinylPair);
  if (kUploadProbe) {
    auto manual = owned; std::erase(manual, std::wstring(L"--route=vinyl-pair-v2"));
    check(!parse(manual));
    for (const auto value : {L"manual", L"vinyl-entry-v1", L"vinyl-cycle-v1", L"vinyl-pair-v2"}) {
      auto single_route = manual; single_route.push_back(L"--route=" + std::wstring(value));
      check(parse(single_route).has_value() == (std::wstring_view(value) == L"vinyl-pair-v2"));
    }
  }
  for (const auto duplicate : {L"--route=vinyl-pair-v2", L"--route=vinyl-entry-v1", L"--route=vinyl-cycle-v1"}) {
    auto repeated = route; repeated.push_back(duplicate); check(!parse(repeated));
  }
  for (const auto existing : {L"--route=vinyl-entry-v1", L"--route=vinyl-cycle-v1"}) {
    auto repeated = owned; repeated.push_back(existing); repeated.push_back(L"--route=vinyl-pair-v2"); check(!parse(repeated));
  }
  for (const auto value : {L"", L"manual", L"vinyl-entry-v2", L"vinyl-entry-v1\n", L"vinyl-entry-v1=extra",
      L"vinyl-cycle-v2", L"vinyl-cycle-v1\n", L"vinyl-cycle-v1=extra", L"VINYL-CYCLE-V1",
      L"vinyl-pair-v1", L"vinyl-pair-v2\n", L"vinyl-pair-v2=extra", L"VINYL-PAIR-V2", L" vinyl-pair-v2"}) {
    route = owned; route.push_back(L"--route=" + std::wstring(value)); check(!parse(route));
  }
  check(kVinylRoute.size() == 15 && kCaptureSlots.size() == 7);
  check(RouteCommands(RouteMode::Manual) == 0 && RouteCaptures(RouteMode::Manual) == 0);
  check(RouteCommands(RouteMode::VinylEntry) == 9 && RouteCaptures(RouteMode::VinylEntry) == 1);
  check(RouteCommands(RouteMode::VinylCycle) == 15 && RouteCaptures(RouteMode::VinylCycle) == 7);
  check(RouteCommands(RouteMode::VinylPair) == 10 && RouteCaptures(RouteMode::VinylPair) == 2);
  check(RouteCommands(RouteMode::VinylPairSlowStartup) == 10 && RouteCaptures(RouteMode::VinylPairSlowStartup) == 2);
  check(RouteName(RouteMode::VinylPairSlowStartup) == "vinyl-pair-v3");
  check(IsVinylPair(RouteMode::VinylPairSlowStartup) && IsVinylPair(RouteMode::VinylPair));
  check(RouteClosesOnCompletion(RouteMode::VinylPairSlowStartup) && RoutePurchases(RouteMode::VinylPairSlowStartup));
  check(SlowStartupMetadata(RouteMode::VinylPairSlowStartup) ==
      "\npair_startup_settle_after_command 1\npair_startup_settle_ms 80000"
      "\npair_startup_claim fixed_wait_not_scene_readiness");
  for (const auto mode : {RouteMode::Manual, RouteMode::VinylEntry, RouteMode::VinylCycle,
      RouteMode::VinylPair, RouteMode::StartupPrefix}) check(SlowStartupMetadata(mode).empty());
  for (const auto mode : {RouteMode::Manual, RouteMode::VinylEntry, RouteMode::VinylCycle,
      RouteMode::StartupPrefix}) check(!IsVinylPair(mode));
  check(RouteCommands(RouteMode::StartupPrefix) == 2 && RouteCaptures(RouteMode::StartupPrefix) == 2);
  check(RouteName(RouteMode::StartupPrefix) == "startup-prefix-v1");
  check(RouteClosesOnCompletion(RouteMode::StartupPrefix) && !RoutePurchases(RouteMode::StartupPrefix));
  check(RoutePurchases(RouteMode::VinylPair) && RoutePurchases(RouteMode::VinylCycle));
  check(!RoutePurchases(RouteMode::Manual) && !RoutePurchases(RouteMode::VinylEntry));
  check(PrefixMetadata(RouteMode::StartupPrefix) ==
      "\nprefix_first_capture_after_command 1\nprefix_first_settle_ms 30000"
      "\nprefix_second_capture_after_command 2\nprefix_second_settle_ms 0"
      "\nprefix_claim startup_transition_inspection_not_garage_or_editor");
  for (const auto mode : {RouteMode::Manual, RouteMode::VinylEntry, RouteMode::VinylCycle, RouteMode::VinylPair})
    check(PrefixMetadata(mode).empty());
  check(RouteName(RouteMode::VinylPair) == "vinyl-pair-v2");
  check(RouteClosesOnCompletion(RouteMode::VinylPair) && RouteClosesOnCompletion(RouteMode::VinylCycle));
  check(!RouteClosesOnCompletion(RouteMode::Manual) && !RouteClosesOnCompletion(RouteMode::VinylEntry));
  check(RouteName(RouteMode::Manual) == "manual" && RouteName(RouteMode::VinylEntry) == "vinyl-entry-v1" &&
      RouteName(RouteMode::VinylCycle) == "vinyl-cycle-v1");
  const std::array<std::string_view, 15> expected_actions{
      "START", "START", "DPAD_DOWN", "A", "DPAD_DOWN", "A", "DPAD_DOWN", "DPAD_DOWN", "A",
      "A", "B", "A", "A", "B", "A"};
  const std::array<std::string_view, 7> expected_slots{
      "garage-main-before", "showroom-purchase-confirmed", "visual-purchase-confirmed",
      "performance-purchase-confirmed", "paint-purchase-confirmed", "vehicle-switch-confirmed",
      "free-roam-after-garage"};
  for (size_t i = 0; i < expected_slots.size(); ++i) {
    check(kCaptureSlots[i] == expected_slots[i]);
    check(kVinylRoute[i + 8].sequence == i + 9);
    check(CaptureDue(i + 9, i) && !CaptureDue(i + 9, i + 1));
  }
  for (size_t i = 0; i < 9; ++i) check(!CaptureDue(i, 0));
  for (size_t command = 0; command <= 16; ++command) {
    for (size_t capture = 0; capture <= 8; ++capture) {
      check(PrefixProgressValid(command, capture) ==
          ((command == 0 && capture == 0) || (command == 1 && capture <= 1) ||
          (command == 2 && (capture == 1 || capture == 2))));
      check(CaptureDue(RouteMode::StartupPrefix, command, capture) ==
          ((command == 1 && capture == 0) || (command == 2 && capture == 1)));
      check(CaptureDue(RouteMode::VinylPair, command, capture) ==
          ((command == 8 && capture == 0) || (command == 10 && capture == 1)));
      check(CaptureDue(RouteMode::VinylPairSlowStartup, command, capture) ==
          CaptureDue(RouteMode::VinylPair, command, capture));
      const bool valid = (command < 8 && capture == 0) || (command == 8 && capture <= 1) ||
          (command == 9 && capture == 1) || (command == 10 && (capture == 1 || capture == 2));
      check(PairProgressValid(command, capture) == valid);
      check(CaptureDue(RouteMode::VinylCycle, command, capture) == CaptureDue(command, capture));
      check(CaptureDue(RouteMode::VinylEntry, command, capture) == CaptureDue(command, capture));
    }
  }
  for (size_t i = 0; i < RouteCommands(RouteMode::VinylPair); ++i) {
    const auto step = StepFor(RouteMode::VinylPair, i);
    check(step.sequence == kVinylRoute[i].sequence && step.action == kVinylRoute[i].action && step.buttons == kVinylRoute[i].buttons);
    check(step.settle_ms == (i == 7 ? 8000 : i == 9 ? 20000 : kVinylRoute[i].settle_ms));
    const auto slow_step = StepFor(RouteMode::VinylPairSlowStartup, i);
    check(slow_step.sequence == step.sequence && slow_step.action == step.action && slow_step.buttons == step.buttons);
    check(slow_step.settle_ms == (i == 0 ? 80000 : step.settle_ms));
  }
  check(!Expired(100, 80099, StepFor(RouteMode::VinylPairSlowStartup, 0).settle_ms));
  check(Expired(100, 80100, StepFor(RouteMode::VinylPairSlowStartup, 0).settle_ms));
  check(StepFor(RouteMode::VinylPair, 0).settle_ms == 30000 &&
      StepFor(RouteMode::StartupPrefix, 0).settle_ms == 30000);
  for (size_t i = 0; i < RouteCommands(RouteMode::StartupPrefix); ++i) {
    const auto step = StepFor(RouteMode::StartupPrefix, i);
    check(step.sequence == i + 1 && step.action == "START" && step.buttons == "0010");
    check(step.settle_ms == (i == 0 ? 30000 : 0));
  }
  check(!Expired(100, 30099, StepFor(RouteMode::StartupPrefix, 0).settle_ms));
  check(Expired(100, 30100, StepFor(RouteMode::StartupPrefix, 0).settle_ms));
  check(Expired(100, 100, StepFor(RouteMode::StartupPrefix, 1).settle_ms));
  // No command2 may precede slot1; no command3 may exist in this route.
  check(CaptureDue(RouteMode::StartupPrefix, 1, 0) && !PrefixProgressValid(2, 0));
  check(!PrefixProgressValid(3, 2) && !PrefixProgressValid(2, 3));
  check(!Expired(100, 8099, StepFor(RouteMode::VinylPair, 7).settle_ms));
  check(Expired(100, 8100, StepFor(RouteMode::VinylPair, 7).settle_ms));
  check(!Expired(100, 20099, StepFor(RouteMode::VinylPair, 9).settle_ms));
  check(Expired(100, 20100, StepFor(RouteMode::VinylPair, 9).settle_ms));
  check(!CaptureDue(RouteMode::VinylPair, 7, 0) && CaptureDue(RouteMode::VinylPair, 8, 0));
  check(!CaptureDue(RouteMode::VinylPair, 9, 1) && CaptureDue(RouteMode::VinylPair, 10, 1));
  for (size_t i = 0; i < kVinylRoute.size(); ++i) {
    const auto &step = kVinylRoute[i];
    check(step.sequence == i + 1 && step.action == expected_actions[i]);
    check(step.settle_ms == (i == 0 ? 30000 : i == 3 ? 15000 : 0));
    check(StepFor(RouteMode::VinylEntry, i).settle_ms == step.settle_ms &&
        StepFor(RouteMode::VinylCycle, i).settle_ms == step.settle_ms);
    check(step.buttons == (step.action == "START" ? "0010" : step.action == "A" ? "1000" :
        step.action == "B" ? "2000" : "0002"));
    const auto n = std::to_string(step.sequence);
    std::string acknowledgments;
    for (const auto side : {"source", "guest"}) {
      const auto prefix = "MCLA_FRONTEND_SMOKE_INPUT v=1 side=" + std::string(side) + " sequence=" + n + " buttons=";
      acknowledgments += prefix + std::string(step.buttons) + "\n" + prefix + "0000\n";
    }
    check(!CommandAcknowledged(acknowledgments, step));
    acknowledgments += "MCLA_GARAGE_CONTROL v=1 sequence=" + n + " action=" + std::string(step.action) +
        " capture=0 width=0 height=0 present_seq=22 status=PASS\n";
    check(CommandAcknowledged(acknowledgments, step));
    check(!CommandAcknowledged(acknowledgments.substr(acknowledgments.find('\n') + 1), step));
    check(!CommandAcknowledged(acknowledgments.substr(0, acknowledgments.size() - 1), step));
  }
  check(!LogRecord("recordXYZ\n", "record"));
  check(LogRecord("prefix record\r\n", "record"));
  for (const bool center : {false, true}) {
    for (const unsigned scale : {1u, 2u, 3u}) {
      const auto ready = RuntimeRecord("ready", center, scale) + "\n";
      const auto outside = RuntimeRecord("outside", center, scale) + "\n";
      const auto editor = RuntimeRecord("editor", center, scale) + "\n";
      const std::string outside_request = "MCLA_RENDERDOC_CAPTURE v=1 state=outside status=REQUESTED source_present_seq=7\n";
      const std::string outside_file = "MCLA_RENDERDOC_CAPTURE v=1 state=outside status=FILE_READY bytes=20 count_before=0 count_after=1 guest_sequence_before=7 guest_sequence_after=9 replay_pending=1 multi_present_state_not_bmp=1\n";
      const std::string editor_request = "MCLA_RENDERDOC_CAPTURE v=1 state=editor status=REQUESTED source_present_seq=9\n";
      const std::string editor_file = "MCLA_RENDERDOC_CAPTURE v=1 state=editor status=FILE_READY bytes=21 count_before=1 count_after=2 guest_sequence_before=9 guest_sequence_after=11 replay_pending=1 multi_present_state_not_bmp=1\n";
      const auto first = ready + outside + outside_request + outside_file;
      const auto both = first + editor + editor_request + editor_file;
      check(InspectRuntime("", center, scale).has_value() && !InspectRuntime("", center, scale)->ready);
      check(InspectRuntime(ready, center, scale)->ready);
      check(!InspectRuntime(ready, !center, scale));
      check(!InspectRuntime(ready, center, scale + 1));
      check(!InspectRuntime(ready + RuntimeRecord("outside", !center, scale) + "\n", center, scale));
      auto wrong_mode = ready; wrong_mode.replace(wrong_mode.find("readback=fast"), 13, "readback=full");
      check(!InspectRuntime(wrong_mode, center, scale));
      check(!InspectRuntime(ready + ready, center, scale));
      check(!InspectRuntime(outside, center, scale));
      check(!InspectRuntime(ready + editor, center, scale));
      check(!InspectRuntime(ready + outside + outside, center, scale));
      check(!InspectRuntime(ready + outside_request + outside, center, scale));
      check(!InspectRuntime(ready + outside + outside_file, center, scale));
      check(!InspectRuntime(first + outside_file, center, scale));
      check(!InspectRuntime(first + outside_request, center, scale));
      check(!InspectRuntime(both + editor, center, scale));
      check(!InspectRuntime(both + "MCLA_RENDERDOC_RUNTIME v=1 phase=ready status=FAIL\n", center, scale));
      check(!InspectRuntime(first + "MCLA_RENDERDOC_CAPTURE v=1 state=editor status=FAIL reason=x\n", center, scale));
      check(!InspectRuntime(ready + "MCLA_RENDERDOC_RUNTIME v=2 phase=outside\n", center, scale));
      check(!InspectRuntime(ready.substr(0, ready.size() - 1), center, scale)->ready);
      check(InspectRuntime(first.substr(0, first.size() - 1), center, scale)->complete[0] == false);
      check(InspectRuntime(first.substr(0, first.size() - 1), center, scale)->pending_record);
      check(!CaptureAttested(*InspectRuntime(ready, center, scale), 0, 0));
      const auto observed = InspectRuntime(both, center, scale);
      check(observed && observed->ready && observed->complete[0] && observed->complete[1]);
      check(observed && CaptureAttested(*observed, 0, ready.size()));
      check(observed && !CaptureAttested(*observed, 0, ready.size() + 1));
      check(observed && CaptureAttested(*observed, 1, first.size()));
      check(observed && !CaptureAttested(*observed, 2, 0));
      const auto partial_failure = InspectRuntime(both + "MCLA_RENDERDOC_RUNTIME v=1 status=FAIL", center, scale);
      check(partial_failure && partial_failure->pending_record && !CaptureAttested(*partial_failure, 1, 0));
      check(!InspectRuntime(RuntimeRecord("ready", center, scale) + " extra\n", center, scale));
      check(InspectRuntime("[info] " + RuntimeRecord("ready", center, scale) + "\r\n", center, scale)->ready);
      for (const auto value : {"", "0", "00", "07", "+7", "-7", "7.0", "7x", "7\t", "7 ",
          "18446744073709551616", "7 source_present_seq=8", "7 extra=1"}) {
        check(!InspectRuntime(ready + outside +
            "MCLA_RENDERDOC_CAPTURE v=1 state=outside status=REQUESTED source_present_seq=" + value + "\n", center, scale));
      }
      const auto corrupted_file = [&](std::string_view old_text, std::string_view new_text) {
        auto changed = outside_file;
        changed.replace(changed.find(old_text), old_text.size(), new_text);
        return ready + outside + outside_request + changed;
      };
      for (const auto value : {"", "0", "020", "+20", "-20", "20x", "20.0", "18446744073709551616",
          "8589934593", "20 bytes=20"})
        check(!InspectRuntime(corrupted_file("bytes=20", "bytes=" + std::string(value)), center, scale));
      for (const auto value : {"1", "00", "4294967296"})
        check(!InspectRuntime(corrupted_file("count_before=0", "count_before=" + std::string(value)), center, scale));
      for (const auto value : {"0", "2", "01", "4294967297"})
        check(!InspectRuntime(corrupted_file("count_after=1", "count_after=" + std::string(value)), center, scale));
      check(!InspectRuntime(corrupted_file("guest_sequence_before=7", "guest_sequence_before=6"), center, scale));
      check(!InspectRuntime(corrupted_file("guest_sequence_before=7", "guest_sequence_before=18446744073709551615"), center, scale));
      for (const auto value : {"0", "7", "8", "09", "18446744073709551616"})
        check(!InspectRuntime(corrupted_file("guest_sequence_after=9", "guest_sequence_after=" + std::string(value)), center, scale));
      check(!InspectRuntime(corrupted_file("replay_pending=1", "replay_pending=0"), center, scale));
      check(!InspectRuntime(corrupted_file("multi_present_state_not_bmp=1", "multi_present_state_not_bmp=0"), center, scale));
      check(!InspectRuntime(corrupted_file("\n", " extra=1\n"), center, scale));
      check(!InspectRuntime(corrupted_file(" guest_sequence_after=9", ""), center, scale));
      check(!InspectRuntime(corrupted_file("count_before=0 count_after=1", "count_after=1 count_before=0"), center, scale));
      check(!InspectRuntime(corrupted_file("status=FILE_READY", "status=FILE_READY_EXTRA"), center, scale));
      check(InspectRuntime(corrupted_file("bytes=20", "bytes=8589934592"), center, scale)->complete[0]);
      auto regressed_editor = editor_request;
      regressed_editor.replace(regressed_editor.find("seq=9"), 5, "seq=8");
      check(!InspectRuntime(first + editor + regressed_editor, center, scale));
    }
  }
  {
    uint64_t number = 0;
    check(CaptureRequestFields(" source_present_seq=18446744073709551615", number) && number == std::numeric_limits<uint64_t>::max());
    check(CaptureReadyFields(" bytes=1 count_before=1 count_after=2 guest_sequence_before=18446744073709551613 guest_sequence_after=18446744073709551615 replay_pending=1 multi_present_state_not_bmp=1",
        1, 7, number) && number == std::numeric_limits<uint64_t>::max());
    check(!CaptureReadyFields(" bytes=1 count_before=2 count_after=3 guest_sequence_before=7 guest_sequence_after=9 replay_pending=1 multi_present_state_not_bmp=1", 2, 7, number));
  }
  auto bad = owned; bad.pop_back(); check(!parse(bad));
  bad = owned; bad.push_back(L"--run"); check(!parse(bad));
  bad = owned; bad.push_back(owned[4]); check(!parse(bad));
  bad = owned; bad.push_back(owned[5]); check(!parse(bad));
  bad = owned; bad[3] = L"--target=999"; check(!parse(bad));
  bad = owned; bad.push_back(L"--automatic-start=true"); check(!parse(bad));
  bad = owned; bad[4] = L"--seed="; check(!parse(bad));
  bad = owned; bad[5] = L"--receipt="; check(!parse(bad));
  bad = owned; bad[4] = L"--seed=" + seed + L"\n"; check(!parse(bad));
  check(!Expired(100, 300099, kInspectionMilliseconds));
  check(Expired(100, 300100, kInspectionMilliseconds));
  check(Expired(100, 99, kInspectionMilliseconds));
  check(!Expired(100, 10099, kCloseGraceMilliseconds));
  check(Expired(100, 10100, kCloseGraceMilliseconds));
  check(!Expired(100, 90099, 90000) && Expired(100, 90100, 90000));
  check(!Expired(100, 45099, 45000) && Expired(100, 45100, 45000));
  check(!Expired(100, 15099, 15000) && Expired(100, 15100, 15000));
  for (const auto name : {L"720", L"1080", L"1440", L"2160"}) {
    if (kUploadProbe && std::wstring_view(name) != L"1440") continue;
    Options options{L"C:/build", L"D:/game", L"GPU", name, true, false};
    const auto target = *TargetFor(name);
    const auto args = SeedArguments(options, target, L"C:/Temp/fixture", 1, false, kUploadProbe);
    const auto count = [&](std::wstring_view value) { return std::count(args.begin(), args.end(), value); };
    check(count(L"--mcla_garage_lifecycle_probe=true") == 1);
    check(count(L"--mcla_garage_lifecycle_cycle=1") == 1);
    check(count(L"--mcla_garage_camera_probe=true") == (kCameraProbe ? 1 : 0));
    check(count(L"--mcla_garage_renderdoc_probe=true") == (kRenderDocProbe ? 1 : 0));
    check(count(L"--mcla_atlas_witness=false") == (kAtlasProbe && !kUploadProbe ? 1 : 0));
    check(count(L"--mcla_atlas_witness=true") == (kUploadProbe ? 1 : 0));
    check(count(L"--mcla_atlas_upload=false") == (kUploadProbe ? 1 : 0));
    check(count(L"--mcla_atlas_upload=true") == 0);
    check(count(L"--mcla_atlas_producer=false") == (kProducerProbe ? 1 : 0));
    check(count(L"--mcla_atlas_producer=true") == 0);
    check(count(L"--mcla_atlas_fault=false") == (kFaultProbe ? 1 : 0));
    check(count(L"--mcla_atlas_fault=true") == 0);
    check(std::count_if(args.begin(), args.end(), [](const auto& arg) {
      return arg.starts_with(L"--mcla_atlas_fault");
    }) == (kFaultProbe ? 1 : 0));
    check(std::count_if(args.begin(), args.end(), [](const auto& arg) {
      return arg.starts_with(L"--mcla_atlas_producer");
    }) == (kProducerProbe ? 1 : 0));
    check(std::count_if(args.begin(), args.end(), [](const auto& arg) {
      return arg.starts_with(L"--mcla_atlas_upload");
    }) == (kUploadProbe ? 1 : 0));
    check(std::count_if(args.begin(), args.end(), [](const auto& arg) {
      return arg.starts_with(L"--mcla_atlas_witness");
    }) == (kAtlasProbe ? 1 : 0));
    check(count(L"--readback_resolve_half_pixel_offset=false") == (kRenderDocProbe ? 1 : 0));
    check(count(L"--readback_resolve_half_pixel_offset=true") == 0);
    check(count(L"--mcla_first_frame_settle_seconds=35") == 1);
    check(count(L"--mcla_first_frame_probe=true") == 0);
    check(count(L"--xam_user_signin_state=1") == 1);
    check(count(L"--vsync=true") == 1 && count(L"--video_mode_refresh_rate=60") == 1);
    check(count(Flag(L"video_mode_width", std::to_wstring(target.guest_width))) == 1);
    check(count(Flag(L"video_mode_height", std::to_wstring(target.guest_height))) == 1);
    check(count(Flag(L"resolution_scale", std::to_wstring(target.scale))) == 1);
    check(count(Flag(L"user_data_root", (std::filesystem::path(L"C:/Temp/fixture") / L"user").wstring())) == 1);
    check(std::none_of(args.begin(), args.end(), [](const auto &a) {
      return a.find(L"race_") != a.npos ||
          (a.find(L"readback_") != a.npos && !(kRenderDocProbe && a == L"--readback_resolve_half_pixel_offset=false")) ||
          a.find(L"trace_gpu") != a.npos || a.find(L"timing_probe") != a.npos;
    }));
    // Exact extension allowlist, including absence of upload flags in every
    // other compiled variant. No arbitrary output path or hidden extra flag.
    auto expected = Arguments(options, target, L"C:/Temp/fixture", 1);
    std::erase(expected, L"--mcla_first_frame_probe=true");
    std::erase(expected, L"--mcla_first_frame_settle_seconds=3");
    expected.push_back(L"--mcla_garage_lifecycle_probe=true");
    expected.push_back(L"--mcla_garage_lifecycle_cycle=1");
    if (kCameraProbe) expected.push_back(L"--mcla_garage_camera_probe=true");
    if (kRenderDocProbe) {
      expected.push_back(L"--mcla_garage_renderdoc_probe=true");
      expected.push_back(L"--readback_resolve_half_pixel_offset=false");
    }
    if (kAtlasProbe) expected.push_back(kUploadProbe ? L"--mcla_atlas_witness=true" : L"--mcla_atlas_witness=false");
    if (kUploadProbe) expected.push_back(L"--mcla_atlas_upload=false");
    if (kProducerProbe) expected.push_back(L"--mcla_atlas_producer=false");
    if (kFaultProbe) expected.push_back(L"--mcla_atlas_fault=false");
    expected.push_back(L"--mcla_first_frame_settle_seconds=35");
    check(args == expected);
  }
  if (kRenderDocProbe && !kUploadProbe) {
    Options options{L"C:/build", L"D:/game", L"GPU", L"1440", true, false};
    const auto args = SeedArguments(options, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, true);
    check(std::count(args.begin(), args.end(), L"--readback_resolve_half_pixel_offset=true") == 1);
    check(std::count(args.begin(), args.end(), L"--readback_resolve_half_pixel_offset=false") == 0);
    check(std::count(args.begin(), args.end(), L"--mcla_atlas_witness=false") == (kAtlasProbe ? 1 : 0));
    auto baseline = SeedArguments(options, *TargetFor(L"1440"), L"C:/Temp/fixture", 1);
    auto expected = args;
    std::replace(expected.begin(), expected.end(), std::wstring(L"--readback_resolve_half_pixel_offset=true"),
        std::wstring(L"--readback_resolve_half_pixel_offset=false"));
    check(expected == baseline); // Exactly one child-argument difference.
  }
  for (const auto name : {L"720", L"1440"}) {
    if (kUploadProbe) continue;
    Options options{L"C:/build", L"D:/game", L"GPU", name, true, false};
    const auto off = SeedArguments(options, *TargetFor(name), L"C:/Temp/fixture", 1, false, false);
    const auto on = SeedArguments(options, *TargetFor(name), L"C:/Temp/fixture", 1, false, true);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_witness=true") == (kAtlasProbe ? 1 : 0));
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_witness=false") == 0);
    auto expected = on;
    std::replace(expected.begin(), expected.end(), std::wstring(L"--mcla_atlas_witness=true"),
        std::wstring(L"--mcla_atlas_witness=false"));
    check(expected == off); // Off/on differs only in the new helper's explicit flag.
  }
  if (kUploadProbe && !kProducerProbe) {
    Options fixed{L"C:/build", L"D:/game", L"GPU", L"1440", true, false};
    const auto off = SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, true, false);
    const auto on = SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, true, true);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_upload=true") == 1);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_upload=false") == 0);
    auto normalized = on;
    std::replace(normalized.begin(), normalized.end(), std::wstring(L"--mcla_atlas_upload=true"),
        std::wstring(L"--mcla_atlas_upload=false"));
    check(normalized == off); // Matched rd4 OFF/ON changes exactly one argument.
    for (const auto name : {L"720", L"1080", L"1440", L"2160"}) {
      for (const bool center : {false, true}) for (const bool atlas : {false, true}) for (const bool upload : {false, true}) {
        Options options{L"C:/build", L"D:/game", L"GPU", name, true, false};
        bool accepted = false;
        try { (void)SeedArguments(options, *TargetFor(name), L"C:/Temp/fixture", 1, center, atlas, upload); accepted = true; }
        catch (const std::exception&) {}
        check(accepted == (std::wstring_view(name) == L"1440" && !center && atlas));
      }
    }
  } else {
    Options fixed{L"C:/build", L"D:/game", L"GPU", L"1440", true, false};
    bool rejected = false;
    try { (void)SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, false, true); }
    catch (const std::exception&) { rejected = true; }
    check(rejected);
  }
  if (kProducerProbe && !kFaultProbe) {
    Options fixed{L"C:/build", L"D:/game", L"GPU", L"1440", true, false};
    const auto off = SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, true, false, false);
    const auto on = SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, true, false, true);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_producer=true") == 1);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_producer=false") == 0);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_upload=false") == 1);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_upload=true") == 0);
    auto expected = on;
    std::replace(expected.begin(), expected.end(), std::wstring(L"--mcla_atlas_producer=true"),
        std::wstring(L"--mcla_atlas_producer=false"));
    check(expected == off);
    for (const auto name : {L"720", L"1080", L"1440", L"2160"})
      for (const bool center : {false, true}) for (const bool atlas : {false, true})
        for (const bool upload : {false, true}) for (const bool producer : {false, true}) {
          Options options{L"C:/build", L"D:/game", L"GPU", name, true, false};
          bool accepted = false;
          try { (void)SeedArguments(options, *TargetFor(name), L"C:/Temp/fixture", 1, center, atlas, upload, producer); accepted = true; }
          catch (const std::exception&) {}
          check(accepted == (std::wstring_view(name) == L"1440" && !center && atlas && !upload));
        }
  } else {
    Options fixed{L"C:/build", L"D:/game", L"GPU", L"1440", true, false};
    bool rejected = false;
    try { (void)SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, true, false, true); }
    catch (const std::exception&) { rejected = true; }
    check(rejected);
  }
  if (kFaultProbe) {
    Options fixed{L"C:/build", L"D:/game", L"GPU", L"1440", true, false};
    const auto off = SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, true, false, false, false);
    const auto on = SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, true, false, false, true);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_fault=true") == 1);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_fault=false") == 0);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_producer=false") == 1);
    check(std::count(on.begin(), on.end(), L"--mcla_atlas_upload=false") == 1);
    auto expected = on;
    std::replace(expected.begin(), expected.end(), std::wstring(L"--mcla_atlas_fault=true"),
        std::wstring(L"--mcla_atlas_fault=false"));
    check(expected == off); // Exactly one child argument differs in matched rd6 arms.
    for (const auto name : {L"720", L"1080", L"1440", L"2160"})
      for (const bool center : {false, true}) for (const bool atlas : {false, true})
        for (const bool upload : {false, true}) for (const bool producer : {false, true})
          for (const bool fault : {false, true}) {
            Options options{L"C:/build", L"D:/game", L"GPU", name, true, false};
            bool accepted = false;
            try { (void)SeedArguments(options, *TargetFor(name), L"C:/Temp/fixture", 1,
                center, atlas, upload, producer, fault); accepted = true; }
            catch (const std::exception&) {}
            check(accepted == (std::wstring_view(name) == L"1440" && !center && atlas && !upload && !producer));
          }
  } else {
    Options fixed{L"C:/build", L"D:/game", L"GPU", L"1440", true, false};
    bool rejected = false;
    try { (void)SeedArguments(fixed, *TargetFor(L"1440"), L"C:/Temp/fixture", 1, false, true, false, false, true); }
    catch (const std::exception&) { rejected = true; }
    check(rejected);
  }
  std::cout << "Seeded inspection policy: " << checks << " checks, " << failed << " failures\n";
  return failed ? 1 : SelfTest(); // Existing BMP/CLI/target/argument policy tests, no runtime.
}

int SeedProbe(const SeedOptions &options) {
  Take(NoGameRunning());
  auto source = Take(fs::OpenDirectory(options.seed));
  auto session = Take(fs::OpenDirectory(std::filesystem::path(options.receipt).parent_path().wstring()));
  Require(SeedPaths(source.path(), (std::filesystem::path(session.path()) / L"before-snapshot.txt").wstring()),
      "canonical immutable seed/receipt relationship rejected");
  HeldReceipt receipt_pin(std::filesystem::path(session.path()) / L"before-snapshot.txt");
  const auto receipt = Receipt(session);
  auto build_root = Take(fs::OpenDirectory(options.passive.build));
  auto game_root = Take(fs::OpenDirectory(options.passive.game));
  Take(fs::CompareDisjoint(build_root, game_root));
  Take(fs::CompareDisjoint(source, build_root)); Take(fs::CompareDisjoint(source, game_root));
  auto build = Take(VerifyBuild(build_root));
  Require(build.fingerprint() == kBuild,
      "only exact reviewed runtime inventory is allowed");
  const auto host = std::find_if(build.files().begin(), build.files().end(), [](const auto &f) { return f.name == L"mcla.exe"; });
  Require(host != build.files().end() && host->sha256 == kHost, "only exact reviewed host is allowed");
  auto game = Take(VerifyGameRoot(game_root));
  const auto adapter = Take(Adapter(options.passive.gpu));
  const auto lease = Take(mcla::lease::CheckBuildLease(build_root, kHost, false, false, true));
  (void)lease;
  auto temp = Take(fs::OpenDirectory(TempParent(std::filesystem::temp_directory_path()).wstring()));
  Take(fs::CompareDisjoint(temp, source)); Take(fs::CompareDisjoint(temp, session));
  Take(fs::CompareDisjoint(temp, build_root)); Take(fs::CompareDisjoint(temp, game_root));
  std::array<uint8_t,16> random{};
  Require(BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0,
      "random identity failed");
  std::wstring name = L"mcla-seeded-inspection-";
  for (const auto b : random) { name += L"0123456789abcdef"[b >> 4]; name += L"0123456789abcdef"[b & 15]; }
  const auto path = std::filesystem::path(temp.path()) / name;
  Require(CreateDirectoryW(path.c_str(), nullptr) != FALSE, "exclusive TEMP root creation failed");
  std::wcout << L"PRESERVED EVIDENCE: " << path.wstring() << L'\n' << std::flush;
  auto root = Take(fs::OpenDirectory(path.wstring()));
  auto lock = Take(fs::AcquireWriterLock(root));
  std::vector<fs::DirectoryGuard> children;
  for (const auto child_name : {L"user", L"cache", L"update", L"metadata", L"diagnostics", L"source-after-user"}) {
    const auto child = path / child_name;
    Require(CreateDirectoryW(child.c_str(), nullptr) != FALSE, "exclusive TEMP child creation failed");
    children.push_back(Take(fs::OpenDirectory(child.wstring())));
  }
  // Immutable historical snapshot only: no source-side writer lock is created.
  // CopySaveTree pins source files and checks membership drift during each copy.
  const auto copied = Take(CopySaveTree(source, children[0]));
  Require(copied == receipt && Receipt(session) == receipt, "seed copy differs from pinned complete receipt");
  Write(root, L"seed-receipt.txt", copied);
  Write(root, L"managed.toml", mcla::managed_config::kSentinel);
  const auto target = *TargetFor(options.passive.target);
  const auto base = "MCLA-SEEDED-INSPECTION-V1\nhost " + std::string(kHost) + "\nbuild " + build.fingerprint() +
      "\ngame " + game.fingerprint() + "\nseed_receipt " + std::string(kSeedReceipt) +
      "\nrequested " + std::to_string(target.width) + "x" + std::to_string(target.height) +
      "\nroute " + std::string(RouteName(options.route)) +
      "\ncamera_probe " + (kCameraProbe ? "fixed_six_commands" : "disabled") +
      "\nrenderdoc_probe " + (kRenderDocProbe ? "exact_two_states_private_resources" : "disabled") +
      "\nreadback_sample_expected " + (kRenderDocProbe ? (options.center ? "center" : "corner") : "not_applicable") +
      (kAtlasProbe ? std::string("\natlas_witness_expected ") + (options.atlas_on ? "on" : "off") +
          "\natlas_witness_claim request_only_external_closed_log_and_draw_link_required" : "") +
      (kUploadProbe ? std::string("\natlas_upload_expected ") + (options.upload_on ? "on" : "off") +
          "\natlas_upload_claim request_only_external_closed_log_files_and_draw_link_required" : "") +
      (kProducerProbe ? std::string("\natlas_producer_expected ") + (options.producer_on ? "on" : "off") +
          "\natlas_producer_claim request_only_external_closed_log_and_boundary_link_required" : "") +
      (kFaultProbe ? std::string("\natlas_fault_expected ") + (options.fault_on ? "on" : "off") +
          "\natlas_fault_claim request_only_external_closed_log_and_breadcrumb_review_required" : "") +
      "\nautomatic_input " + (options.route != RouteMode::Manual ? "fixed_grounded_commands" : "false") +
      "\nautomatic_command_limit " + std::to_string(RouteCommands(options.route)) +
      "\nautomatic_capture_limit " + std::to_string(RouteCaptures(options.route)) +
      "\nautomatic_purchase " + (RoutePurchases(options.route) ? "true" : "false") +
      std::string(PrefixMetadata(options.route)) +
      std::string(SlowStartupMetadata(options.route)) +
      (IsVinylPair(options.route) ? "\npair_first_capture_after_command 8\npair_first_settle_ms 8000\npair_second_capture_after_command 10\npair_second_settle_ms 20000" : "") +
      "\nprotocol_phase_names_are_semantic_claims false\nmaximum_run_ms 300000\nclose_grace_ms 10000\nclaim inspection_only_no_gameplay_verdict\n";
  Write(root, L"result.txt", base + "state prepared\n");
  auto actual = options.passive; actual.build = build.path(); actual.game = game.path();
  Take(NoGameRunning());
  auto process = Take(StartOwnedProcess(std::filesystem::path(build.path()) / L"mcla.exe",
      SeedArguments(actual, target, path, adapter, options.center, options.atlas_on, options.upload_on, options.producer_on, options.fault_on), path));
  Write(root, L"result.txt", base + "state running\npid " + std::to_string(process.ProcessId()) + "\n");
  std::cout << (options.route == RouteMode::StartupPrefix ?
      "Startup diagnostic: START, existing 30s settle, capture; START, existing host ACK, capture then close. No A/DPAD/purchase; scene review required.\n" :
      options.route == RouteMode::VinylPairSlowStartup ?
      "Fixed ten-command v3 pair: 80s after first START is a fixed wait, NOT scene readiness; 8s/20s capture settles unchanged. Purchase ONLY in isolated TEMP clone; visual review required.\n" :
      IsVinylPair(options.route) ?
      "Fixed ten-command pair: settle 8s after command 8 before entering vinyl, 20s after command 10; two captures then close. Purchase ONLY in isolated TEMP clone.\n" :
      options.route == RouteMode::VinylCycle ?
      "Fixed fifteen-command route, including editor access purchase ONLY in isolated TEMP clone; seven captures then close.\n" :
      options.route == RouteMode::VinylEntry ?
      "Fixed nine-command route; automatic input stops before purchase confirmation.\n" :
      "No automatic input. Review guest image before explicit TEMP request files.\n") <<
      "Write exact close newline to root/close.request to close normally; 300-second deadline.\n" << std::flush;
  bool requested = false, first_marker = false, garage_ready = false;
  VinylRouteState route_state;
  RuntimeWitness runtime_witness;
  size_t previous_log_size = 0;
  std::string route_error;
  const auto started = GetTickCount64();
  while (!Expired(started, GetTickCount64(), kInspectionMilliseconds)) {
    const auto status = Take(process.Poll());
    if (status.exit_code) break;
    if (CloseRequested(root)) { requested = true; break; }
    if (const auto log = ReadLog(path / L"mcla.log")) {
      if (kRenderDocProbe) {
        const auto observed = InspectRuntime(*log, options.center, target.scale);
        if (!observed || log->size() < previous_log_size) {
          route_error = "RenderDoc runtime attestation invalid, duplicated, changed or log truncated";
          break;
        }
        previous_log_size = log->size();
        runtime_witness = *observed;
        if (!runtime_witness.ready && Expired(started, GetTickCount64(), 90000)) {
          route_error = "RenderDoc runtime readiness deadline";
          break;
        }
      }
      if (kRenderDocProbe &&
          (log->find("MCLA_RENDERDOC_CAPTURE v=1 state=outside status=FAIL") != log->npos ||
           log->find("MCLA_RENDERDOC_CAPTURE v=1 state=editor status=FAIL") != log->npos)) {
        route_error = "RenderDoc capture failed; no retry";
        break;
      }
      if (!first_marker && MarkerMatches(*log, target.width, target.height)) {
        const auto shape = ReadBmp(path / L"user" / L"mcla-first-frame.bmp");
        Require(shape && *shape == std::pair{target.width, target.height}, "initial guest BMP dimensions/layout mismatch");
        first_marker = true;
        std::cout << "FIRST_GUEST_IMAGE_AVAILABLE_NOT_GAMEPLAY\n" << std::flush;
      }
      if (!garage_ready && (!kRenderDocProbe || (runtime_witness.ready && !runtime_witness.pending_record)) &&
          log->find("MCLA_GARAGE_LIFECYCLE_CONFIG v=1 cycle=1") != log->npos) {
        garage_ready = true;
        std::cout << "OWNED_GUEST_REQUEST_PROTOCOL_READY_NO_INPUT_SENT\n" << std::flush;
      }
      if (options.route != RouteMode::Manual && (!kRenderDocProbe || !runtime_witness.pending_record)) {
        try {
          route_state.Tick(options.route, started, GetTickCount64(), garage_ready && first_marker, *log, children[0], target,
              kRenderDocProbe ? &runtime_witness : nullptr);
        } catch (const std::exception &error) { route_error = error.what(); break; }
        if (RouteClosesOnCompletion(options.route) && route_state.complete) break;
      }
    } else if ((kRenderDocProbe || options.route != RouteMode::Manual) && Expired(started, GetTickCount64(), 90000)) {
      route_error = "route startup log deadline";
      break;
    }
    Sleep(100);
  }
  const auto initial_exit = Take(process.Poll());
  const bool close_sent = !initial_exit.exit_code && CloseOwnedWindow(process.ProcessId());
  std::optional<ProcessStatus> stopped;
  const auto closing = GetTickCount64();
  while (!Expired(closing, GetTickCount64(), kCloseGraceMilliseconds)) {
    const auto status = Take(process.Poll());
    if (status.exit_code && status.active_job_processes == 0) { stopped = status; break; }
    Sleep(100);
  }
  const bool owned_cleanup = !stopped;
  if (owned_cleanup) Take(process.TerminateAndWait());
  // Include records flushed while closing; late drift/duplicate/FAIL cannot be
  // hidden by route completion or an owner's early close request.
  if (kRenderDocProbe) {
    const auto log = ReadLog(path / L"mcla.log");
    const auto observed = log ? InspectRuntime(*log, options.center, target.scale) : std::nullopt;
    if (!observed || !observed->ready || observed->pending_record || !log->ends_with('\n') || log->size() < previous_log_size ||
        observed->phase != observed->complete) {
      if (route_error.empty()) route_error = "final RenderDoc runtime attestation incomplete or invalid";
    } else runtime_witness = *observed;
  }
  // Re-copy only the immutable source to an untouched fresh verification tree,
  // never the game-written TEMP user; compare full membership and every byte.
  const auto after = Take(CopySaveTree(source, children[5]));
  const bool source_unchanged = after == receipt && Receipt(session) == receipt;
  const auto sentinel = Take(fs::ReadFile(root, L"managed.toml", 1024));
  const bool sentinel_ok = std::string(sentinel.begin(), sentinel.end()) == mcla::managed_config::kSentinel;
  const auto frame = ReadBmp(path / L"user" / L"mcla-garage-lifecycle-garage-main-before.bmp");
  const bool captured = frame && *frame == std::pair{target.width, target.height};
  const bool clean = stopped && stopped->exit_code == 0 && source_unchanged && sentinel_ok &&
      route_error.empty() && (options.route == RouteMode::Manual || route_state.complete);
  const auto result = base + "state " + (clean ? "INSPECTION_CLOSED_NO_GAMEPLAY_VERDICT" : "FAILED") +
      "\nclose_requested " + std::to_string(requested) + "\nclose_sent " + std::to_string(close_sent) +
      "\nnormal_exit " + std::to_string(stopped && stopped->exit_code == 0) +
      "\nowned_cleanup " + std::to_string(owned_cleanup) + "\nsource_unchanged " + std::to_string(source_unchanged) +
      "\nsentinel_unchanged " + std::to_string(sentinel_ok) + "\ninitial_guest_image " + std::to_string(first_marker) +
      "\ninspection_image_valid_dimensions " + std::to_string(captured) +
      "\nroute_complete " + std::to_string(route_state.complete) +
      "\nroute_completed_captures " + std::to_string(route_state.captures) +
      "\nroute_completed_commands " + std::to_string(route_state.next) +
      "\nreadback_runtime_ready_observed " + std::to_string(kRenderDocProbe && runtime_witness.ready) +
      "\nreadback_runtime_outside_observed " + std::to_string(kRenderDocProbe && runtime_witness.complete[0]) +
      "\nreadback_runtime_editor_observed " + std::to_string(kRenderDocProbe && runtime_witness.complete[1]) +
      "\nreadback_sample_observed " + (kRenderDocProbe && runtime_witness.ready ? (options.center ? "center" : "corner") : "not_observed") +
      "\nroute_completion_requested_close " + std::to_string(RouteClosesOnCompletion(options.route) && route_state.complete) +
      "\nroute_error " + (route_error.empty() ? "none" : route_error) + "\n";
  Write(root, L"result.txt", result);
  std::cout << result << "Preserved; no retry and no gameplay PASS.\n";
  return clean ? 0 : 1;
}
} // namespace

int wmain(int argc, wchar_t **argv) {
  try {
    std::vector<std::wstring_view> arguments;
    for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
    const auto options = ParseSeed(arguments);
    if (!options) {
      std::cerr << "Use --self-test OR --build=<absolute> --game=<absolute> --gpu=<exact> --target=720|1080|1440|2160 "
          "--seed=<pinned session before-user> --receipt=<same session before-snapshot.txt> "
          "[--route=vinyl-entry-v1|vinyl-cycle-v1|vinyl-pair-v2] --run\n";
      if (kRenderDocProbe) std::cerr << "RenderDoc only: [--readback-sample=corner|center]; center requires target1440.\n";
      if (kUploadProbe && !kProducerProbe) std::cerr << "Upload only: REQUIRED --atlas-upload=off|on --atlas-witness=on --target=1440 --route=vinyl-pair-v2; corner only; no output-path option.\n";
      if (kCleanVisualProbe) std::cerr << "Clean visual only: optional --route=startup-prefix-v1 requires target2160; two START commands and two captures, no purchase.\n";
      if (kCleanVisualProbe) std::cerr << "Clean visual only: --route=vinyl-pair-v3 requires target2160; ten commands, first START wait80s, two captures, isolated purchase; no scene-readiness assertion.\n";
      if (kFaultProbe) std::cerr << "Fault only: REQUIRED --atlas-fault=off|on --atlas-producer=off --atlas-upload=off --atlas-witness=on --target=1440 --route=vinyl-pair-v2; corner only; no output-path option.\n";
      else if (kProducerProbe) std::cerr << "Producer only: REQUIRED --atlas-producer=off|on --atlas-upload=off --atlas-witness=on --target=1440 --route=vinyl-pair-v2; corner only; no output-path option.\n";
      else if (kAtlasProbe) std::cerr << "Atlas only: REQUIRED --atlas-witness=off|on; target720|1440; omit route for manual or use vinyl-pair-v2.\n";
      return 2;
    }
    return options->passive.self_test ? SeedSelfTest() : SeedProbe(*options);
  } catch (const std::exception &error) {
    std::cerr << "Seeded inspection FAILED; preserve evidence; no retry. " << error.what() << '\n';
    return 1;
  }
}
