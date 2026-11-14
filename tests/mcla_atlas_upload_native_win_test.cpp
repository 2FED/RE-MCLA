// Native filesystem/lifecycle tests of the SAME production diagnostic object.
// No game, GPU, RenderDoc, guest RAM, or accepted SDK is touched. Run explicitly
// with --run. All synthetic evidence is preserved in a fresh owned TEMP tree.
#ifndef MCLA_DIAGNOSTIC_ATLAS_WITNESS
#error This test requires MCLA_DIAGNOSTIC_ATLAS_WITNESS=1
#endif
#include <rex/graphics/d3d12/mcla_atlas_upload_witness.h>
#include <rex/logging/sink.h>
#include <bcrypt.h>
#include <algorithm>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using Witness = rex::graphics::d3d12::AtlasUploadWitness;
using Failure = mcla::diagnostics::atlas_upload::Failure;
constexpr uint32_t kBase = 83492864;
unsigned checks = 0, failures = 0, skips = 0;
std::string active_case;
void Check(bool ok, const char* label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << active_case << ": " << label << '\n'; }
}
void Require(bool ok, const char* label) {
  if (!ok) throw std::runtime_error(label);
}
std::wstring CurrentDirectory() {
  wchar_t buffer[32768];
  const DWORD n = GetCurrentDirectoryW(DWORD(std::size(buffer)), buffer);
  Require(n && n < std::size(buffer), "cannot read fixture CWD");
  return {buffer, n};
}
struct Environment {
  std::wstring name, value;
  bool existed = false;
  explicit Environment(const wchar_t* key) : name(key) {
    SetLastError(ERROR_SUCCESS);
    const DWORD size = GetEnvironmentVariableW(key, nullptr, 0);
    existed = size != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
    if (size) {
      std::vector<wchar_t> buffer(size);
      const DWORD n = GetEnvironmentVariableW(key, buffer.data(), size);
      Require(n < size, "environment changed while reading");
      value.assign(buffer.data(), n);
    }
  }
  ~Environment() { SetEnvironmentVariableW(name.c_str(), existed ? value.c_str() : nullptr); }
};
struct ProcessScope {
  std::wstring cwd = CurrentDirectory();
  Environment temp{L"TEMP"}, tmp{L"TMP"};
  ~ProcessScope() { SetCurrentDirectoryW(cwd.c_str()); }
};
void SetTemp(const std::wstring& path) {
  Require(SetEnvironmentVariableW(L"TEMP", path.c_str()) &&
          SetEnvironmentVariableW(L"TMP", path.c_str()), "cannot set process-local TEMP");
}
void Enter(const std::wstring& path) {
  Require(SetCurrentDirectoryW(path.c_str()) != 0, "cannot enter owned fixture directory");
}
void CreateDirectoryNew(const std::wstring& path) {
  Require(CreateDirectoryW(path.c_str(), nullptr) != 0, "fixture directory must be fresh");
}
bool Exists(const std::wstring& path) {
  return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}
void WriteFresh(const std::wstring& path, const void* data, size_t size) {
  Require(size <= 2 * Witness::kBytes, "fixture write budget");
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  Require(file != INVALID_HANDLE_VALUE, "fixture output collision");
  DWORD written = 0;
  const bool ok = WriteFile(file, data, DWORD(size), &written, nullptr) && written == size &&
                  FlushFileBuffers(file);
  CloseHandle(file);
  Require(ok, "fixture output write failed");
}
void WriteFresh(const std::wstring& path, const std::string& text) {
  WriteFresh(path, text.data(), text.size());
}
std::vector<uint8_t> ReadBounded(const std::wstring& path) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  Require(file != INVALID_HANDLE_VALUE, "cannot read fixture output");
  LARGE_INTEGER size{};
  BY_HANDLE_FILE_INFORMATION info{};
  const bool admitted = GetFileSizeEx(file, &size) && size.QuadPart >= 0 &&
      size.QuadPart <= 2 * Witness::kBytes && GetFileInformationByHandle(file, &info) &&
      !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
  if (!admitted) { CloseHandle(file); throw std::runtime_error("invalid fixture output"); }
  std::vector<uint8_t> bytes(size_t(size.QuadPart));
  DWORD read = 0;
  const bool ok = ReadFile(file, bytes.data(), DWORD(bytes.size()), &read, nullptr) && read == bytes.size();
  CloseHandle(file);
  Require(ok, "short fixture read");
  return bytes;
}
std::wstring FreshRoot() {
  wchar_t buffer[32768];
  const DWORD n = GetTempPathW(DWORD(std::size(buffer)), buffer);
  Require(n && n < std::size(buffer), "no TEMP path");
  std::wstring parent(buffer, n);
  while (parent.size() > 3 && parent.back() == L'\\') parent.pop_back();
  Require(parent.size() > 3 && parent[1] == L':' && parent[2] == L'\\' &&
      parent.find(L"..") == parent.npos, "TEMP must be an ordinary absolute local directory");
  for (size_t end = 3;;) {
    const DWORD attributes = GetFileAttributesW(parent.substr(0, end).c_str());
    Require(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
            !(attributes & FILE_ATTRIBUTE_REPARSE_POINT), "TEMP ancestry must not be reparse-backed");
    if (end == parent.size()) break;
    end = parent.find(L'\\', end == 3 ? 3 : end + 1);
    if (end == parent.npos) end = parent.size();
  }
  std::array<unsigned char, 16> random{};
  Require(BCryptGenRandom(nullptr, random.data(), ULONG(random.size()),
                         BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0, "random fixture identity failed");
  std::wstring suffix;
  constexpr wchar_t hex[] = L"0123456789abcdef";
  for (auto b : random) { suffix += hex[b >> 4]; suffix += hex[b & 15]; }
  auto root = parent + L"\\mcla-upload-native-" + suffix;
  CreateDirectoryNew(root);
  WriteFresh(root + L"\\SYNTHETIC-ONLY.txt", std::string(
      "Native unit fixture of the production AtlasUploadWitness class.\n"
      "No guest, GPU or SDK hook-placement evidence. Logs are upload-only;\n"
      "parse_records is applicable, not a complete atlas runtime certificate.\n"));
  return root;
}
std::wstring Snapshot(const std::wstring& root, unsigned generation = 1) {
  return root + L"\\mcla-atlas-upload-g" + std::to_wstring(generation) + L".bin";
}
std::vector<uint8_t> Pattern(unsigned seed) {
  std::vector<uint8_t> bytes(Witness::kBytes);
  for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t((i * 37 + i / 4096 + seed) & 255);
  return bytes;
}
void Upload(Witness& witness, const std::vector<uint8_t>& bytes, uint64_t submission = 1) {
  witness.Prepare(1, true, kBase);
  constexpr uint32_t part = Witness::kBytes / 4;
  for (uint32_t i = 0; i < 4; ++i)
    witness.Observe(kBase + i * part, bytes.data() + i * part, part, submission + i);
}
bool Failed(const Witness& witness) { return witness.policy.failure() != Failure::None; }
std::string Transcript(const std::shared_ptr<rex::LogCaptureSink>& sink) {
  rex::FlushLogging();
  std::vector<rex::LogEntry> entries;
  sink->CopyEntries(entries);
  Require(entries.size() <= 64, "fixture log record budget exceeded");
  std::string text;
  for (const auto& entry : entries) {
    Require(entry.text.starts_with("MCLA_ATLAS_UPLOAD v=1 seq=") && entry.text.size() < 900,
            "unexpected fixture log record");
    text += entry.text + '\n';
  }
  return text;
}
struct Fixture {
  const std::wstring root;
  unsigned cases = 0;
  explicit Fixture(std::wstring path) : root(std::move(path)) {}
  template<class F> void Run(const char* name, F&& fn, std::string_view expected = {}) {
    active_case = name;
    ProcessScope restore;
    SetTemp(root);
    wchar_t hex[33];
    std::swprintf(hex, std::size(hex), L"%032x", ++cases);
    const auto path = root + L"\\mcla-seeded-inspection-" + hex;
    CreateDirectoryNew(path);
    Enter(path);
    auto sink = std::make_shared<rex::LogCaptureSink>();
    sink->set_pattern("%v");
    rex::AddSink(rex::log::gpu(), sink);
    try {
      fn(path);
      const auto text = Transcript(sink);
      Check(!text.empty(), "real logging sink received records");
      if (!expected.empty()) Check(text.find(expected) != text.npos, "expected protocol record");
      if (expected.starts_with("type=fail"))
        Check(text.find("complete=1") == text.npos, "failure never certifies complete");
      WriteFresh(path + L"\\mcla.log", text);
      WriteFresh(path + L"\\case.txt", std::string(name) + "\nsynthetic production-class fixture only\n");
    } catch (...) {
      // Preserve the actual transcript even when the fixture itself fails.
      try { if (!Exists(path + L"\\mcla.log")) WriteFresh(path + L"\\mcla.log", Transcript(sink)); }
      catch (...) {}
      rex::RemoveSink(rex::log::gpu(), sink);
      throw;
    }
    rex::RemoveSink(rex::log::gpu(), sink);
  }
};
bool MakeSymlink(const std::wstring& path, const std::wstring& target) {
  if (CreateSymbolicLinkW(path.c_str(), target.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | 2)) return true;
  const auto error = GetLastError();
  if (error == ERROR_PRIVILEGE_NOT_HELD) {
    ++skips; std::cout << "SKIP " << active_case << ": symlink privilege unavailable (1314)\n";
    return false;
  }
  throw std::runtime_error("unexpected symlink fixture creation failure");
}
void RunTests(Fixture& fixture) {
  const auto first = Pattern(3), second = Pattern(91);
  fixture.Run("two-generations-retained-input", [&](const auto& root) {
    Witness w; Require(w.Initialize(), "valid witness root rejected");
    w.EndFrame();
    auto source = first;
    Upload(w, source);
    Check(!Exists(Snapshot(root)), "observe does not publish");
    std::fill(source.begin(), source.end(), 0);
    w.Begin(1, 1, true);
    Check(!Exists(Snapshot(root)), "reconcile does not publish");
    w.Queued(1, 1, 4);
    Check(ReadBounded(Snapshot(root)) == first, "exact observed staging retained independently");
    w.EndFrame();
    Upload(w, second, 5); w.Begin(1, 2, true); w.Queued(1, 2, 8); w.EndFrame();
    const auto sequence = w.sequence; w.EndFrame();
    Check(w.sequence == sequence, "unchanged end-frame does not duplicate status");
    Check(!Failed(w) && w.written == 2 && w.policy.retained_bytes() == 2097152,
          "both complete generations bounded to 2MiB");
    Check(ReadBounded(Snapshot(root, 2)) == second, "second generation exact bytes");
  }, "type=status arms=2 written=2 retained=2097152 active=0 inconclusive=0 failed=0 complete=1");
  for (unsigned mode = 0; mode < 4; ++mode) fixture.Run(
      mode == 0 ? "resident-zero" : mode == 1 ? "partial" : mode == 2 ? "gap" : "overlap",
      [&](const auto& root) {
        Witness w; Require(w.Initialize(), "init"); w.Prepare(1, true, kBase);
        if (mode) w.Observe(kBase, first.data(), 4096, 1);
        if (mode == 2) w.Observe(kBase + 8192, first.data(), 4096, 2);
        if (mode == 3) w.Observe(kBase, first.data(), 4096, 2);
        w.Begin(1, 1, true); w.Queued(1, 1, 3); w.EndFrame();
        Check(Failed(w) && !Exists(Snapshot(root)), "incomplete or noncontiguous input rejected");
      }, "type=fail");
  for (unsigned mode = 0; mode < 7; ++mode) fixture.Run(
      mode == 0 ? "repeated-prepare" : mode == 1 ? "request-failure" : mode == 2 ? "failure-after-begin" :
      mode == 3 ? "wrong-generation" : mode == 4 ? "wrong-queue-submission" :
      mode == 5 ? "gpu-write-armed" : "gpu-write-reconciled", [&](const auto& root) {
        Witness w; Require(w.Initialize(), "init"); Upload(w, first);
        if (mode == 0) w.Prepare(1, true, kBase);
        if (mode == 1) w.Cancel(10);
        if (mode == 5) w.Cancel(11);
        w.Begin(1, mode == 3 ? 2 : 1, true);
        if (mode == 2) w.Cancel(10);
        if (mode == 6) w.Cancel(11);
        w.Queued(1, 1, mode == 4 ? 3 : 4);
        w.Prepare(1, true, kBase); w.EndFrame();
        Check(Failed(w) && !Exists(Snapshot(root)), "failure is sticky and cannot publish");
      }, "type=fail");
  fixture.Run("third-generation", [&](const auto& root) {
    Witness w; Require(w.Initialize(), "init");
    Upload(w, first); w.Begin(1, 1, true); w.Queued(1, 1, 4);
    Upload(w, second, 5); w.Begin(1, 2, true); w.Queued(1, 2, 8);
    w.Prepare(1, true, kBase); w.EndFrame();
    Check(Failed(w) && w.written == 2 && !Exists(Snapshot(root, 3)), "third arm rejected");
    Check(ReadBounded(Snapshot(root)) == first && ReadBounded(Snapshot(root, 2)) == second,
          "existing two generations unchanged");
  }, "type=fail reason=4");
  for (bool directory : {false, true}) fixture.Run(directory ? "directory-collision" : "file-collision",
      [&](const auto& root) {
        const std::string sentinel = "preexisting synthetic sentinel";
        if (directory) CreateDirectoryNew(Snapshot(root)); else WriteFresh(Snapshot(root), sentinel);
        Witness w; Require(w.Initialize(), "init"); Upload(w, first);
        w.Begin(1, 1, true); w.Queued(1, 1, 4); w.EndFrame();
        Check(Failed(w) && w.written == 0, "CREATE_NEW collision rejects ready");
        if (directory) Check((GetFileAttributesW(Snapshot(root).c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0,
                             "existing directory preserved");
        else { const auto bytes = ReadBounded(Snapshot(root));
          Check(std::string(bytes.begin(), bytes.end()) == sentinel, "existing file preserved"); }
      }, "type=fail reason=9");
  fixture.Run("wrong-root-name", [&](const auto& root) {
    const auto wrong = root + L"\\wrong"; CreateDirectoryNew(wrong); Enter(wrong);
    Witness w; Check(!w.Initialize() && Failed(w), "wrong root name rejected");
  }, "type=fail reason=1");
  fixture.Run("wrong-temp-parent", [&](const auto& root) {
    SetTemp(root); Witness w; Check(!w.Initialize() && Failed(w), "wrong TEMP parent rejected");
  }, "type=fail reason=1");
  fixture.Run("reparse-root", [&](const auto& root) {
    const auto parent = root + L"\\links", target = root + L"\\target";
    CreateDirectoryNew(parent); CreateDirectoryNew(target);
    const auto link = parent + L"\\mcla-seeded-inspection-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    if (MakeSymlink(link, target)) { SetTemp(parent); Enter(link);
      Witness w; Check(!w.Initialize() && Failed(w), "reparse root rejected"); }
    else { Witness w; Require(w.Initialize(), "skip fixture init"); w.EndFrame(); }
  });
  fixture.Run("reparse-ancestor", [&](const auto& root) {
    const auto target = root + L"\\target", link = root + L"\\linked-parent";
    CreateDirectoryNew(target);
    const auto name = L"\\mcla-seeded-inspection-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    CreateDirectoryNew(target + name);
    if (MakeSymlink(link, target)) { SetTemp(link); Enter(link + name);
      Witness w; Check(!w.Initialize() && Failed(w), "reparse ancestor rejected"); }
    else { Witness w; Require(w.Initialize(), "skip fixture init"); w.EndFrame(); }
  });
  fixture.Run("pinned-root-and-output", [&](const auto& root) {
    Witness w; Require(w.Initialize(), "init");
    const auto other = root + L"\\other"; CreateDirectoryNew(other); Enter(other);
    Upload(w, first); w.Begin(1, 1, true); w.Queued(1, 1, 4);
    Check(Exists(Snapshot(root)) && !Exists(Snapshot(other)), "mutable CWD cannot redirect output");
    Enter(fixture.root);  // Do not let the process CWD itself explain the rename denial.
    Check(!MoveFileExW(root.c_str(), (root + L"-moved").c_str(), 0), "pinned root cannot rename");
    HANDLE replacement = CreateFileW(Snapshot(root).c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Check(replacement == INVALID_HANDLE_VALUE, "retained output denies modification");
    if (replacement != INVALID_HANDLE_VALUE) CloseHandle(replacement);
    HANDLE deletion = CreateFileW(Snapshot(root).c_str(), DELETE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    Check(deletion == INVALID_HANDLE_VALUE, "retained output denies delete access");
    if (deletion != INVALID_HANDLE_VALUE) CloseHandle(deletion);  // Never actually delete evidence.
    Check(ReadBounded(Snapshot(root)) == first, "pinned output content unchanged");
    w.EndFrame();
  });
  fixture.Run("foreign-thread", [&](const auto& root) {
    Witness w; Require(w.Initialize(), "init");
    std::thread worker([&] { w.Prepare(1, true, kBase); }); worker.join();
    w.EndFrame();
    Check(Failed(w) && w.policy.arms() == 0 && !Exists(Snapshot(root)), "owner checkpoint rejects foreign call");
  }, "type=fail reason=16");
  fixture.Run("reentry", [&](const auto& root) {
    Witness w; Require(w.Initialize(), "init");
    w.Visit([&] { w.Prepare(1, true, kBase); }); w.EndFrame();
    Check(Failed(w) && w.policy.arms() == 0 && !Exists(Snapshot(root)), "nested visit rejected");
  }, "type=fail reason=16");
  for (bool destructor : {false, true}) fixture.Run(destructor ? "abandoned-destructor" : "abandoned-end-frame",
      [&](const auto& root) {
        { Witness w; Require(w.Initialize(), "init"); Upload(w, first);
          if (!destructor) { w.EndFrame(); Check(Failed(w), "abandoned frame rejected"); } }
        Check(!Exists(Snapshot(root)), "abandoned preparation cannot publish");
      }, destructor ? "type=fail reason=13" : "type=fail reason=12");
  fixture.Run("info-disabled", [&](const auto&) {
    rex::SetCategoryLevel(rex::log::gpu(), spdlog::level::warn);
    { Witness w; Check(!w.Initialize() && Failed(w), "INFO disabled cannot create evidence"); }
    rex::SetCategoryLevel(rex::log::gpu(), spdlog::level::info);
  }, "type=fail reason=15");
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
  if (argc != 2 || std::wstring_view(argv[1]) != L"--run") {
    std::cerr << "Usage: mcla_atlas_upload_native_win_test --run\n"
                 "Synthetic native tests only; preserves fresh TEMP evidence.\n";
    return 2;
  }
  ProcessScope restore;
  try {
    Fixture fixture(FreshRoot());
    std::wcout << L"Synthetic evidence: " << fixture.root << L'\n';
    rex::LogConfig logging;
    logging.log_to_console = false;
    logging.default_level = spdlog::level::info;
    rex::InitLogging(logging);
    RunTests(fixture);
    rex::ShutdownLogging();
    std::cout << checks << " checks, " << failures << " failures, " << skips << " privilege skips\n";
    return failures ? 1 : 0;
  } catch (const std::exception& error) {
    rex::ShutdownLogging();
    std::cerr << "FIXTURE ERROR " << active_case << ": " << error.what() << '\n';
    return 1;
  }
}
