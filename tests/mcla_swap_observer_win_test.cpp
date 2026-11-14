#include "../src/mcla_swap_observer_win.cpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstring>

namespace mcla::guest_source_scout {
bool test_enabled = true;
bool Enabled() { return test_enabled; }
}
int main(int argc, char** argv) {
  namespace observer = mcla::swap_native;
  namespace sw = rex::diagnostics::swap_emission;
  using Profile = mcla::guest_source_scout::detail::Profile;
  if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--short")) return 2;
  const auto profile = argc == 2 ? Profile::kShort31 : Profile::kLegacy91;
  const auto other_profile = argc == 2 ? Profile::kLegacy91 : Profile::kShort31;
  const auto count = mcla::guest_source_scout::detail::SlotCount(profile);
  unsigned checks = 0, failures = 0;
  auto check = [&](bool value) { ++checks; if (!value) ++failures; };
  auto rejects = [&](auto operation) {
    bool rejected = false;
    try { operation(); } catch (...) { rejected = true; }
    check(rejected);
  };
  check(!observer::Enabled());
  LARGE_INTEGER now{}, frequency{};
  check(QueryPerformanceCounter(&now) && QueryPerformanceFrequency(&frequency));
  const uint64_t begin = now.QuadPart + 2 * frequency.QuadPart;
  const std::string token = "123456789abcdef0123456789abcdef0";
  rejects([&] { observer::Begin(begin, frequency.QuadPart, token); });
  check(rex::cvar::SetFlagByName("mcla_swap_observer_probe", "true"));
  for (const auto& bad : {std::string("x"), std::string(32, '0'), std::string(32, 'A')})
    rejects([&] { observer::Begin(begin, frequency.QuadPart, bad); });
  rejects([&] { observer::Begin(0, frequency.QuadPart, token); });
  rejects([&] { observer::Begin(UINT64_MAX, frequency.QuadPart, token); });
  rejects([&] { observer::Begin(begin, 0, token); });
  mcla::guest_source_scout::test_enabled = false;
  rejects([&] { observer::Begin(begin, frequency.QuadPart, token); });
  mcla::guest_source_scout::test_enabled = true;
  rejects([&] { observer::Begin(begin, frequency.QuadPart, token, static_cast<Profile>(2)); });
  observer::Begin(begin, frequency.QuadPart, token, profile);
  rejects([&] { observer::Begin(begin, frequency.QuadPart, token); });
  PPCContext context{};
  context.r3.u64 = 0xFFFF0000; context.lr = 0x1234;
  const auto before = context;
  sw::Snapshot snapshot{1, 2, 3, 4, 5, 6, 7, true};
  SetLastError(77);
  check(!observer::detail::Observe(context, nullptr, snapshot));
  check(snapshot == sw::Snapshot{} && GetLastError() == 77);
  auto* memory = static_cast<uint8_t*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  check(memory != nullptr);
  if (memory) {
    memory[8] = 0x3D; memory[9] = 8; memory[10] = 0x88; memory[11] = 0x89;
    memory[44] = 0x12; memory[45] = 0x34; memory[46] = 0x56; memory[47] = 0x78;
    const auto base = reinterpret_cast<uintptr_t>(memory) - 0x827D7500;
    check(observer::detail::Observe(context, reinterpret_cast<const uint8_t*>(base), snapshot));
    check(snapshot.timer_address == 0x827D7500 && snapshot.timer_counter == 0x12345678 &&
          snapshot.timer_delta_bits == 0x3D088889);
    check(!snapshot.snapshot_available && !snapshot.qpc_before && !snapshot.native_thread_id);
    check(GetLastError() == 77 && std::memcmp(&before, &context, sizeof(context)) == 0);
    DWORD old{};
    check(VirtualProtect(memory, 4096, PAGE_NOACCESS, &old));
    check(!observer::detail::Observe(context, reinterpret_cast<const uint8_t*>(base), snapshot));
    check(snapshot == sw::Snapshot{});
    check(VirtualFree(memory, 0, MEM_RELEASE));
  }
  mcla::guest_source_scout::detail::Sequence sequence;
  sequence.frequency = frequency.QuadPart; sequence.begin = begin;
  rejects([&] { observer::Serialize(sequence, token, other_profile); });
  rejects([&] { observer::Serialize(sequence, token, static_cast<Profile>(2)); });
  rejects([&] { observer::Serialize(sequence, "123456789abcdef0ffffffffffffffff", profile); });
  sequence.begin++;
  rejects([&] { observer::Serialize(sequence, token, profile); });
  sequence.begin = begin;
  const auto correct_end = observer::window.end_qpc;
  observer::window.end_qpc++;
  rejects([&] { observer::Serialize(sequence, token, profile); });
  observer::window.end_qpc = correct_end;
  auto data = nlohmann::json::parse(observer::Serialize(sequence, token, profile));
  check(data.at("records").size() == count && data.at("epoch") == 0x123456789abcdef0ull);
  check(data.at("begin_qpc") == begin - frequency.QuadPart);
  check(data.at("end_qpc_exclusive") == begin + count * frequency.QuadPart / 10);
  for (size_t i = 0; i < count; ++i) {
    const auto& row = data["records"][i];
    check(row["slot"] == i && row["binding"].is_null() &&
          row["presenter_epoch"].is_null() && row["acquired_sequence"].is_null());
  }
  auto& slot = sequence.slots[0]; slot.has_capture = true;
  slot.paired.provenance.presenter_epoch = 7; slot.paired.provenance.acquired_sequence = 42;
  data = nlohmann::json::parse(observer::Serialize(sequence, token, profile));
  check(data["records"][0]["binding"].is_null() && data["records"][0]["acquired_sequence"] == 42);
  auto& binding = slot.paired.provenance.swap_emission;
  binding.result = sw::Result::kTokenMatched; binding.match.token = {0x123456789abcdef0ull, 9};
  binding.stock = {0x53574150, 0x1000, 1280, 720}; binding.fetch = {1,2,3,4,5,6};
  binding.consume_qpc = begin;
  data = nlohmann::json::parse(observer::Serialize(sequence, token, profile));
  check(data["records"][0]["binding"]["snapshot"].is_null());
  binding.match.snapshot_result = sw::SnapshotResult::kAvailable;
  binding.match.snapshot = {begin-2, begin-1, 123, 456, 0x827D7500, 88, 0x3D088889, true};
  data = nlohmann::json::parse(observer::Serialize(sequence, token, profile));
  const auto& b = data["records"][0]["binding"];
  check(b["serial"] == 9 && b["stock"].size() == 4 && b["fetch"].size() == 6);
  check(b["snapshot"]["timer_counter"] == 88 && b["snapshot"]["guest_caller_lr"] == 456);
  for (const auto* key : {"simulation_proven", "frame_to_step_mapping_proven",
                         "display_proven", "fps_proven", "statistics_quiescent"})
    check(data[key] == false);
  sequence.frequency++;
  rejects([&] { observer::Serialize(sequence, token, profile); });
  observer::Disarm();
  std::cout << "swap observer: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
