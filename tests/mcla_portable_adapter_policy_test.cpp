#include "mcla_portable_adapter_policy.h"

#include <iostream>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
unsigned checks = 0;
unsigned failures = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) {
    ++failures;
    std::cerr << "FAIL " << label << '\n';
  }
}
} // namespace

int main(int argc, char **argv) {
  using namespace mcla::portable;
  std::string error;
  for (std::string_view name : {"User", "Orange", "Race Driver", "a", "123456789012345", "A_-9"}) {
    Check(ValidLocalProfileName(name), "valid local display name");
  }
  for (std::string_view name : {"", " User", "User ", "1234567890123456", "../User", "a/b", "a\\b", "a\nb", "a\tb", "a:b", "a\"b", "\xC3\xA9"}) {
    Check(!ValidLocalProfileName(name), "invalid display name rejected");
  }
  Check(!ValidLocalProfileName(std::string("a\0b", 3)), "name NUL rejected");
  Check(!ValidateExplicitGpuConfig("local_user_display_name='Other'", false, error, true, false), "profile config override rejected");
  Check(ValidateExplicitGpuConfig("gpu_backend='any'", false, error, true, false), "unmanaged backend remains compatible");
  if (argc == 2) {
    std::ifstream file(argv[1], std::ios::binary);
    Check(file.good(), "actual shipped config opened");
    const std::string config((std::istreambuf_iterator<char>(file)), {});
    Check(!config.empty() && ValidateExplicitGpuConfig(config, true, error, true), "actual shipped config accepted");
  } else {
    Check(false, "requires actual shipped config argument");
  }
  std::vector<AdapterCandidate> adapters = {
      {0, L"Integrated", true}, {1, L"Unsupported", false},
      {2, L"NVIDIA GeForce RTX 5060 Laptop GPU", true}};
  auto selected = SelectExactAdapter(adapters, L"NVIDIA GeForce RTX 5060 Laptop GPU", error);
  Check(selected && *selected == 2, "preserve unfiltered DXGI ordinal");
  selected = SelectExactAdapter(adapters, L"Integrated", error);
  Check(selected && *selected == 0, "index zero is a valid explicit selection");
  Check(!SelectExactAdapter(adapters, L"Unsupported", error), "incapable rejected");
  Check(!SelectExactAdapter(adapters, L"integrated", error), "exact case required");
  Check(!SelectExactAdapter(adapters, L"NVIDIA", error), "substring rejected");
  Check(!SelectExactAdapter(adapters, L" Integrated", error), "no trimming");
  Check(!SelectExactAdapter({}, L"Integrated", error), "empty inventory");
  Check(!SelectExactAdapter(adapters, L"", error), "empty name");
  adapters.push_back({3, L"Integrated", false});
  selected = SelectExactAdapter(adapters, L"Integrated", error);
  Check(selected && *selected == 0, "only capable matches counted");
  adapters.push_back({4, L"Integrated", true});
  Check(!SelectExactAdapter(adapters, L"Integrated", error), "ambiguous capable match");
  Check(ValidAdapterName(std::wstring(127, L'x')), "name length bound");
  Check(!ValidAdapterName(std::wstring(128, L'x')), "overlong name");
  Check(!ValidAdapterName(L"GPU\nwrong"), "control character");
  Check(!ValidAdapterName(std::wstring(L"GPU\0wrong", 9)), "embedded NUL");
  Check(CompatibleNamedBackend(L""), "implicit d3d12");
  Check(CompatibleNamedBackend(L"d3d12"), "explicit d3d12");
  Check(!CompatibleNamedBackend(L"any"), "any would permit fallback");
  Check(!CompatibleNamedBackend(L"vulkan"), "Vulkan conflicts");
  Check(!CompatibleNamedBackend(L"D3D12"), "backend spelling exact");

  for (std::string_view config : {
           "", "# gpu_backend = \"vulkan\"\n",
           "fullscreen = true\nwindow_width = 0\nscale = 2.0\n",
           "path = \"relative/#not-comment\" # trailing comment\n",
           "value = 'literal\\value'\n",
           "value = \"escaped\\\"quote\"\n",
           "\xEF\xBB\xBF" "fullscreen = true\r\n",
           "\tfullscreen\t= true\t# comment\r\n",
           "name = \"gpu_backend = vulkan\"\n"}) {
    Check(ValidateExplicitGpuConfig(config, true, error), "flat config allowed");
  }
  for (std::string_view config : {
           "gpu_backend = \"d3d12\"", "gpu_backend=\"vulkan\"",
           "d3d12_adapter = 0", "d3d12_adapter = -1",
           "d3d12_adapter = 2 # still forbidden even if matching",
           "\"gpu_backend\" = \"vulkan\"", "'gpu_backend' = 'vulkan'",
           "\"gpu_\\u0062ackend\" = \"vulkan\"",
           "[gpu]\nbackend = 'vulkan'", "gpu.backend = 'vulkan'",
           "value = '''multiline'''", "value = \"\"\"multiline\"\"\"",
           "value = \"unterminated", "value = [1, 2]",
           "value = {gpu_backend='vulkan'}", "value = \"x\" junk",
           "value = \"x\"; gpu_backend='vulkan'", "value = true false",
           "value = # absent", "=true", "value", "value =",
           "value = \"x\\\ncontinued\""}) {
    Check(!ValidateExplicitGpuConfig(config, true, error), "override or unsupported syntax rejected");
  }
  Check(ValidateExplicitGpuConfig("d3d12_adapter = 0", false, error),
        "backend-only choice preserves numeric adapter compatibility");
  Check(!ValidateExplicitGpuConfig("gpu_backend = 'any'", false, error),
        "explicit backend cannot be overridden");
  Check(!ValidateExplicitGpuConfig(std::string("a=true\0", 7), true, error),
        "config NUL rejected");
  Check(!ValidateExplicitGpuConfig(std::string(1024 * 1024 + 1, '#'), true, error),
        "config byte bound");
  std::cout << "Portable adapter policy: " << checks << " checks, " << failures
            << " failures\n";
  return failures ? 1 : 0;
}
