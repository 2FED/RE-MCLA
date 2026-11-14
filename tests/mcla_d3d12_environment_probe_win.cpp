// Private initialization probe, not gameplay or replay acceptance.
// Run only through the owned replay supervisor with its 15-second job deadline.
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <dxgi1_2.h>
#include <d3d12.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;
void Require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Handle {
  HANDLE value = INVALID_HANDLE_VALUE;
  explicit Handle(HANDLE h = INVALID_HANDLE_VALUE) : value(h) {}
  Handle(const Handle&) = delete;
  Handle(Handle&& other) noexcept : value(other.value) { other.value = INVALID_HANDLE_VALUE; }
  ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};
template<class T> struct Com {
  T* value = nullptr;
  ~Com() { if (value) value->Release(); }
};
struct Module {
  HMODULE value = nullptr;
  ~Module() { if (value) FreeLibrary(value); }
};
bool Equal(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() && CompareStringOrdinal(a.data(), int(a.size()),
      b.data(), int(b.size()), TRUE) == CSTR_EQUAL;
}
bool Plain(const fs::path& p) {
  const auto s = p.wstring();
  if (s.size() < 4 || s.size() > 30000 || s[1] != L':' || s[2] != L'\\' ||
      !((s[0] >= L'A' && s[0] <= L'Z') || (s[0] >= L'a' && s[0] <= L'z')) ||
      s.find(L'\0') != s.npos) return false;
  for (const auto& part : p.relative_path()) {
    const auto c = part.wstring();
    if (c.empty() || c == L"." || c == L".." || c.back() == L'.' || c.back() == L' ' ||
        c.find_first_of(L":*?\"<>|/") != c.npos) return false;
  }
  return true;
}
std::wstring Final(HANDLE h) {
  std::wstring s(32768, L'\0');
  const auto n = GetFinalPathNameByHandleW(h, s.data(), DWORD(s.size()), FILE_NAME_NORMALIZED);
  Require(n && n < s.size(), "final directory path unavailable"); s.resize(n);
  if (s.compare(0, 4, L"\\\\?\\") == 0) s.erase(0, 4);
  return s;
}
void PinDirectories(const fs::path& p, std::vector<Handle>& pins) {
  Require(Plain(p) && GetDriveTypeW(p.root_path().c_str()) == DRIVE_FIXED, "local directory required");
  auto pin = [&](const fs::path& q) {
    Handle h(CreateFileW(q.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    Require(h.value != INVALID_HANDLE_VALUE && GetFileInformationByHandle(h.value, &info) &&
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && Equal(Final(h.value), q.wstring()),
        "directory pin rejected");
    pins.push_back(std::move(h));
  };
  auto q = p.root_path(); pin(q);
  for (const auto& part : p.relative_path()) { q /= part; pin(q); }
}
std::wstring Environment(const wchar_t* name) {
  wchar_t value[32768]{}; const auto n = GetEnvironmentVariableW(name, value, DWORD(std::size(value)));
  Require(n && n < std::size(value), "required isolated environment variable absent");
  return std::wstring(value, n);
}
std::string Utf8(std::wstring_view input) {
  if (input.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), int(input.size()), nullptr, 0, nullptr, nullptr);
  Require(n > 0, "UTF8 sizing failed"); std::string out(n, '\0');
  Require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), int(input.size()), out.data(), n, nullptr, nullptr) == n, "UTF8 conversion failed");
  return out;
}
std::string Escape(std::wstring_view input) {
  std::string out;
  for (unsigned char c : Utf8(input)) {
    if (c == '\\') out += "\\\\";
    else if (c < 32 || c == 127) {
      out += "\\x"; out += "0123456789ABCDEF"[c >> 4]; out += "0123456789ABCDEF"[c & 15];
    } else out += char(c);
  }
  return out;
}
struct Log {
  Handle file;
  ULONGLONG started = GetTickCount64();
  explicit Log(const fs::path& p) : file(CreateFileW(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
      nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)) {
    Require(file.value != INVALID_HANDLE_VALUE, "fresh probe output creation failed");
  }
  void Write(const std::string& marker, const std::string& detail = {}) {
    const auto row = std::to_string(GetTickCount64() - started) + "\t" + marker + "\t" + detail + "\n";
    WriteLine(row);
  }
  void WriteLine(const std::string& row) {
    Require(row.size() <= 131072, "marker size rejected"); DWORD written = 0;
    Require(WriteFile(file.value, row.data(), DWORD(row.size()), &written, nullptr) &&
        written == row.size() && FlushFileBuffers(file.value), "probe marker write failed");
  }
};
struct FolderResult {
  HRESULT hr;
  std::wstring path;
};
bool MatchesPrivateFolder(const FolderResult& result, const fs::path& expected) {
  return SUCCEEDED(result.hr) && Plain(result.path) && Equal(result.path, expected.wstring());
}
FolderResult KnownFolder(Log& log, REFKNOWNFOLDERID id, const char* name) {
  log.Write(std::string(name) + "_BEGIN"); PWSTR path = nullptr;
  const HRESULT hr = SHGetKnownFolderPath(id, KF_FLAG_SIMPLE_IDLIST | KF_FLAG_DONT_UNEXPAND, nullptr, &path);
  std::wstring copy;
  if (path) { copy = path; CoTaskMemFree(path); }
  log.Write(std::string(name) + "_END", std::to_string(uint32_t(hr)) + "\t" + Escape(copy));
  return {hr, std::move(copy)};
}
void LoadSystem(Log& log, Module& module, const wchar_t* name, const char* marker) {
  wchar_t directory[32768]{}; const auto n = GetSystemDirectoryW(directory, DWORD(std::size(directory)));
  Require(n && n < std::size(directory), "system directory unavailable");
  const auto expected = fs::path(directory) / name;
  Require(!GetModuleHandleW(name), "refuse preloaded graphics runtime");
  log.Write(std::string(marker) + "_BEGIN");
  module.value = LoadLibraryExW(expected.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  Require(module.value != nullptr, "system graphics DLL load failed");
  wchar_t actual[32768]{}; const auto length = GetModuleFileNameW(module.value, actual, DWORD(std::size(actual)));
  Require(length && length < std::size(actual) && Equal(std::wstring_view(actual, length), expected.wstring()), "graphics DLL path mismatch");
  log.Write(std::string(marker) + "_END", Escape(std::wstring_view(actual, length)));
}
void Probe(Log& log, const fs::path& root, std::vector<Handle>& pins) {
  const auto roaming = KnownFolder(log, FOLDERID_RoamingAppData, "KNOWN_ROAMING");
  const auto local = KnownFolder(log, FOLDERID_LocalAppData, "KNOWN_LOCAL");
  Require(MatchesPrivateFolder(roaming, root / L"profile" / L"AppData" / L"Roaming") &&
      MatchesPrivateFolder(local, root / L"profile" / L"AppData" / L"Local"),
      "Known Folders unavailable or outside the exact private profile layout; device not attempted");
  PinDirectories(roaming.path, pins); PinDirectories(local.path, pins);
  log.Write("KNOWN_PRIVATE_LAYOUT_ACCEPTED");
  Module dxgi, d3d12;
  LoadSystem(log, dxgi, L"dxgi.dll", "LOAD_DXGI");
  LoadSystem(log, d3d12, L"d3d12.dll", "LOAD_D3D12");
  using Factory = HRESULT(WINAPI*)(REFIID, void**);
  const auto make_factory = reinterpret_cast<Factory>(GetProcAddress(dxgi.value, "CreateDXGIFactory1"));
  const auto make_device = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(d3d12.value, "D3D12CreateDevice"));
  Require(make_factory && make_device, "graphics exports absent");
  Com<IDXGIFactory1> factory; Com<IDXGIAdapter1> selected; Com<ID3D12Device> device;
  log.Write("FACTORY_BEGIN");
  HRESULT hr = make_factory(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory.value));
  log.Write("FACTORY_END", std::to_string(uint32_t(hr))); Require(SUCCEEDED(hr) && factory.value, "factory creation failed");
  bool enumeration_complete = false;
  log.Write("ADAPTER_ENUM_BEGIN");
  for (UINT i = 0; i < 64; ++i) {
    Com<IDXGIAdapter1> adapter; hr = factory.value->EnumAdapters1(i, &adapter.value);
    if (hr == DXGI_ERROR_NOT_FOUND) { enumeration_complete = true; break; }
    Require(SUCCEEDED(hr) && adapter.value, "adapter enumeration failed");
    DXGI_ADAPTER_DESC1 desc{}; Require(SUCCEEDED(adapter.value->GetDesc1(&desc)), "adapter description failed");
    if (desc.VendorId == 0x10de && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
        std::wstring_view(desc.Description) == L"NVIDIA GeForce RTX 5060 Laptop GPU") {
      Require(!selected.value, "ambiguous exact adapter"); selected.value = adapter.value; adapter.value = nullptr;
      log.Write("ADAPTER_MATCH", std::to_string(i) + "\t" + std::to_string(desc.VendorId) + "\t" +
          std::to_string(desc.DeviceId) + "\t" + Escape(desc.Description));
    }
  }
  Require(enumeration_complete && selected.value, "exact RTX 5060 adapter absent or enumeration budget exceeded");
  log.Write("ADAPTER_ENUM_END");
  log.Write("DEVICE_BEGIN", "D3D_FEATURE_LEVEL_11_0");
  hr = make_device(selected.value, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&device.value));
  log.Write("DEVICE_END", std::to_string(uint32_t(hr)) + "\t" + (device.value ? "nonnull" : "null"));
  Require(SUCCEEDED(hr) && device.value, "D3D12 device creation failed");
  log.Write("DEVICE_RELEASE_BEGIN"); device.value->Release(); device.value = nullptr;
  log.Write("DEVICE_RELEASE_END");
  // COM owners are destroyed before module owners, before the completion marker.
}
int SelfTest() {
  const fs::path expected = L"C:\\fixture\\profile\\AppData\\Roaming";
  unsigned count = 0;
  auto check = [&](bool value) { ++count; Require(value, "private folder policy self-test failed"); };
  check(MatchesPrivateFolder({S_OK, expected.wstring()}, expected));
  check(MatchesPrivateFolder({S_OK, L"c:\\FIXTURE\\profile\\appdata\\roaming"}, expected));
  check(!MatchesPrivateFolder({HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND), expected.wstring()}, expected));
  check(!MatchesPrivateFolder({S_OK, L""}, expected));
  check(!MatchesPrivateFolder({S_OK, L"C:\\fixture\\profile\\AppData\\Local"}, expected));
  check(!MatchesPrivateFolder({S_OK, L"C:\\fixture\\profile\\AppData\\Roaming-extra"}, expected));
  check(!MatchesPrivateFolder({S_OK, L"C:\\fixture\\profile\\AppData\\Roaming\\child"}, expected));
  check(!MatchesPrivateFolder({S_OK, L"C:\\fixture\\profile\\AppData\\Local\\..\\Roaming"}, expected));
  check(!MatchesPrivateFolder({S_OK, L"\\\\server\\share\\Roaming"}, expected));
  std::cout << "SELF_TEST " << count << " PASS; no graphics initialization or filesystem writes\n";
  return 0;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
  try {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--self-test") return SelfTest();
    Require(argc == 2 && std::wstring_view(argv[1]) == L"--device-probe", "only explicit --device-probe accepted");
    Require(!GetModuleHandleW(L"renderdoc.dll"), "RenderDoc must not be loaded");
    BOOL in_job = FALSE; Require(IsProcessInJob(GetCurrentProcess(), nullptr, &in_job) && in_job, "owned supervisor job required");
    wchar_t buffer[32768]{}; const auto n = GetTempPathW(DWORD(std::size(buffer)), buffer);
    Require(n && n < std::size(buffer), "TEMP unavailable");
    std::wstring temp(buffer, n); while (!temp.empty() && temp.back() == L'\\') temp.pop_back();
    const fs::path temp_path(temp), root = temp_path.parent_path();
    const auto root_name = root.filename().wstring(); const std::wstring prefix = L"mcla-replay-supervisor-";
    Require(temp_path.filename() == L"temp" && root_name.size() == prefix.size() + 32 &&
        root_name.compare(0, prefix.size(), prefix) == 0 &&
        std::all_of(root_name.begin() + prefix.size(), root_name.end(), [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'); }), "TEMP is not an owned supervisor child");
    Require(Equal(Environment(L"TEMP"), temp) && Equal(Environment(L"TMP"), temp) &&
        Equal(Environment(L"USERPROFILE"), (root / L"profile").wstring()) &&
        Equal(Environment(L"APPDATA"), (root / L"profile" / L"AppData" / L"Roaming").wstring()) &&
        Equal(Environment(L"LOCALAPPDATA"), (root / L"profile" / L"AppData" / L"Local").wstring()), "isolated environment layout mismatch");
    std::vector<Handle> pins; PinDirectories(temp_path, pins);
    Log log(temp_path / L"device-probe.tsv");
    try {
      log.Write("MCLA_D3D12_ENVIRONMENT_PROBE_V1", "private; no RenderDoc; no graphics quality claim");
      Probe(log, root, pins);
      Require(!GetModuleHandleW(L"renderdoc.dll"), "unexpected RenderDoc load");
      log.WriteLine("COMPLETE\tDEVICE_CREATED_AND_RELEASED\n");
      return 0;
    } catch (const std::exception& e) {
      try { log.Write("DEVICE_PROBE_FAILED", e.what()); } catch (...) {}
      throw;
    }
  } catch (const std::exception& e) {
    std::cerr << "DEVICE_PROBE_FAILED: " << e.what() << "\nPreserve partial output; no automatic retry.\n";
    return 1;
  } catch (...) { std::cerr << "DEVICE_PROBE_FAILED: unknown exception\n"; return 1; }
}
