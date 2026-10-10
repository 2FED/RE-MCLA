// Synthetic portable-launcher child. Never initializes MCLA or touches a save.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

// Exported marker remains a contiguous binary identity even under optimization.
extern "C" __declspec(dllexport) const char kMclaPortableChildFixture[] =
    "mcla-portable-child-fixture-v1";

namespace {
std::string Utf8(std::wstring_view text) {
  if (text.empty()) return {};
  const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
      static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (count <= 0) return {};
  std::string result(count, '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
      static_cast<int>(text.size()), result.data(), count, nullptr, nullptr) != count) return {};
  return result;
}
std::string Json(std::string_view value) {
  std::string result = "\"";
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') result += '\\';
    if (c < 0x20) {
      constexpr char hex[] = "0123456789ABCDEF";
      result += "\\u00";
      result += hex[c >> 4]; result += hex[c & 15];
    } else result += static_cast<char>(c);
  }
  return result + "\"";
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc < 1 || argc > 32) return 10;
  wchar_t executable[32768];
  const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
  if (!length || length >= 32768) return 11;
  const auto root = std::filesystem::path(std::wstring_view(executable, length)).parent_path().parent_path();
  const auto results = root / L"results";
  // Explicit synthetic fixture marker prevents accidental use as a real child.
  std::ifstream marker(results / L"child-fixture.enabled", std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(marker)), std::istreambuf_iterator<char>());
  if (text != kMclaPortableChildFixture) return 12;
  std::string json = "{\"schema\":\"mcla-portable-child-argv-v1\",\"argv\":[";
  for (int index = 1; index < argc; ++index) {
    const std::wstring_view argument(argv[index]);
    if (argument.size() > 32768) return 13;
    if (index != 1) json += ',';
    json += Json(Utf8(argument));
  }
  json += "]}\n";
  const auto output = results / (L"child-argv-" + std::to_wstring(GetCurrentProcessId()) + L".json");
  const HANDLE file = CreateFileW(output.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return 14;
  DWORD written = 0;
  const bool okay = WriteFile(file, json.data(), static_cast<DWORD>(json.size()), &written, nullptr) &&
                    written == json.size();
  CloseHandle(file);
  return okay ? 0 : 15;
}
