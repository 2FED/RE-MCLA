// KI-061 (M10-004): pure Vulkan device choice for the launcher-selected GPU.
#include "mcla_vulkan_device_select.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <vector>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

namespace vd = mcla::vulkan_device;

constexpr uint64_t kGb = 1024ull * 1024 * 1024;
constexpr uint32_t kAmd = 0x1002, kNvidia = 0x10DE;

vd::Luid MakeLuid(uint8_t seed) {
  vd::Luid luid{};
  for (size_t i = 0; i < luid.size(); ++i) luid[i] = static_cast<uint8_t>(seed + i);
  return luid;
}

// The owner's laptop (l63, 2026-11-02): device 0 the Radeon 860M iGPU,
// device 1 the RTX 5060 Laptop GPU.
std::vector<vd::Device> Laptop() {
  vd::Device igpu{"AMD Radeon 860M Graphics", kAmd, 0x1114, false, true, MakeLuid(0x10),
                  512ull * 1024 * 1024};
  vd::Device dgpu{"NVIDIA GeForce RTX 5060 Laptop GPU", kNvidia, 0x2D59, true, true,
                  MakeLuid(0x40), 8 * kGb};
  return {igpu, dgpu};
}

vd::Adapter AdapterOf(const vd::Device &device) {
  return {device.luid, device.vendor_id, device.device_id};
}

void TestLuidMatch() {
  const auto devices = Laptop();
  const auto choice = vd::Select(-1, AdapterOf(devices[1]), devices);
  Check(choice.index == 1 && choice.reason == vd::Reason::kLuid && choice.set_cvar,
        "launcher dGPU maps to Vulkan device 1 by LUID");
  // The launcher choosing the iGPU is respected (no discrete override).
  const auto igpu = vd::Select(-1, AdapterOf(devices[0]), devices);
  Check(igpu.index == 0 && igpu.reason == vd::Reason::kLuid && igpu.set_cvar,
        "launcher iGPU maps to Vulkan device 0 by LUID");
}

void TestLuidWinsOverIds() {
  // Two identical GPUs: only the LUID tells them apart.
  auto devices = Laptop();
  devices[0] = devices[1];
  devices[0].luid = MakeLuid(0x70);
  const auto choice = vd::Select(-1, AdapterOf(devices[1]), devices);
  Check(choice.index == 1 && choice.reason == vd::Reason::kLuid, "LUID picks the second twin");
}

void TestIdsMatch() {
  auto devices = Laptop();
  for (auto &device : devices) device.luid_valid = false;  // e.g. a driver without LUIDs
  vd::Adapter adapter = AdapterOf(devices[1]);
  adapter.luid = MakeLuid(0x99);
  const auto choice = vd::Select(-1, adapter, devices);
  Check(choice.index == 1 && choice.reason == vd::Reason::kIds && choice.set_cvar,
        "vendor+device ids match without a valid LUID");
  // An invalid LUID never matches even if the bytes happen to agree.
  devices = Laptop();
  devices[0].luid_valid = false;
  vd::Adapter same_bytes = AdapterOf(devices[0]);
  const auto ids = vd::Select(-1, same_bytes, devices);
  Check(ids.index == 0 && ids.reason == vd::Reason::kIds, "invalid LUID ignored, ids used");
}

void TestAmbiguousIdsFallThrough() {
  auto devices = Laptop();
  devices[0] = devices[1];
  devices[0].discrete = false;
  devices[0].device_local_bytes = 16 * kGb;
  for (auto &device : devices) device.luid_valid = false;
  const auto choice = vd::Select(-1, AdapterOf(devices[1]), devices);
  Check(choice.reason == vd::Reason::kDiscrete && choice.index == 1,
        "two devices with the same ids are not an ids match; discrete rule decides");
}

void TestNoMatch() {
  auto devices = Laptop();
  vd::Adapter unknown{MakeLuid(0xA0), 0x8086, 0x1234};
  const auto choice = vd::Select(-1, unknown, devices);
  Check(choice.index == 1 && choice.reason == vd::Reason::kDiscrete && choice.set_cvar,
        "unmatched launcher adapter falls back to the discrete GPU");
  // No match and no discrete GPU: the SDK's own selection stays.
  for (auto &device : devices) device.discrete = false;
  const auto none = vd::Select(-1, unknown, devices);
  Check(none.index == -1 && none.reason == vd::Reason::kFallback && !none.set_cvar,
        "no match and no discrete GPU leaves vulkan_device alone");
}

void TestDiscretePreference() {
  auto devices = Laptop();
  const auto choice = vd::Select(-1, std::nullopt, devices);
  Check(choice.index == 1 && choice.reason == vd::Reason::kDiscrete && choice.set_cvar,
        "no launcher adapter: the discrete GPU");
  // Several discrete GPUs: the most device-local memory, first on ties.
  vd::Device big{"Big", kAmd, 0x73BF, true, true, MakeLuid(0x50), 16 * kGb};
  vd::Device big2{"Big twin", kAmd, 0x73BF, true, true, MakeLuid(0x60), 16 * kGb};
  devices.push_back(big);
  devices.push_back(big2);
  const auto most = vd::Select(-1, std::nullopt, devices);
  Check(most.index == 2, "largest device-local memory wins, first on ties");
}

void TestSingleDevice() {
  // Steam Deck: one APU (integrated); nothing is overridden whatever the
  // launcher passed.
  std::vector<vd::Device> deck{
      {"AMD Custom GPU 0405 (RADV VANGOGH)", kAmd, 0x163F, false, false, {}, 1 * kGb}};
  const auto choice = vd::Select(-1, vd::Adapter{MakeLuid(1), kAmd, 0x163F}, deck);
  Check(choice.index == 0 && choice.reason == vd::Reason::kSingle && !choice.set_cvar,
        "single device: no override");
  const auto unmatched = vd::Select(-1, std::nullopt, deck);
  Check(unmatched.reason == vd::Reason::kSingle && !unmatched.set_cvar,
        "single device without launcher adapter: no override");
}

void TestExplicitKept() {
  const auto devices = Laptop();
  const auto choice = vd::Select(0, AdapterOf(devices[1]), devices);
  Check(choice.index == 0 && choice.reason == vd::Reason::kExplicit && !choice.set_cvar,
        "explicit vulkan_device is kept");
  const auto unprobed = vd::Select(3, std::nullopt, {});
  Check(unprobed.index == 3 && unprobed.reason == vd::Reason::kExplicit && !unprobed.set_cvar,
        "explicit index kept even without a probe");
}

void TestNoDevices() {
  const auto choice = vd::Select(-1, std::nullopt, {});
  Check(choice.index == -1 && choice.reason == vd::Reason::kNoDevices && !choice.set_cvar,
        "probe failure: no override");
}

static_assert(vd::ReasonName(vd::Reason::kLuid) == "LUID");
static_assert(vd::ReasonName(vd::Reason::kIds) == "ids");
static_assert(vd::ReasonName(vd::Reason::kDiscrete) == "discrete");
static_assert(vd::ReasonName(vd::Reason::kFallback) == "fallback");
}  // namespace

int main() {
  TestLuidMatch();
  TestLuidWinsOverIds();
  TestIdsMatch();
  TestAmbiguousIdsFallThrough();
  TestNoMatch();
  TestDiscretePreference();
  TestSingleDevice();
  TestExplicitKept();
  TestNoDevices();
  std::printf("mcla_vulkan_device_select_test: %d checks passed\n", checks);
  return 0;
}
