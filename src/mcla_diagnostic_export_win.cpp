#include "mcla_diagnostic_export_win.h"
#include "mcla_diagnostic_export_policy.h"
#include "mcla_launcher_writable_root_win.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>

namespace mcla::launcher::diagnostic_export {
namespace {
#ifdef MCLA_DIAGNOSTIC_EXPORT_TESTING
void (*test_hook)(ExportTestPhase, std::wstring_view) = nullptr;
#endif
struct Handle {
  HANDLE h = INVALID_HANDLE_VALUE;
  explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : h(value) {}
  ~Handle() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
  Handle(Handle &&other) noexcept : h(std::exchange(other.h, INVALID_HANDLE_VALUE)) {}
  Handle &operator=(Handle &&other) noexcept {
    if (this != &other) {
      if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
      h = std::exchange(other.h, INVALID_HANDLE_VALUE);
    }
    return *this;
  }
  Handle(const Handle &) = delete;
};
struct PinnedFile { Handle handle; std::string bytes; };
std::wstring Wide(std::string_view value) { return {value.begin(), value.end()}; }
std::wstring Wide(std::u16string_view value) { return {value.begin(), value.end()}; }
std::wstring Native(std::wstring_view path) { return L"\\\\?\\" + std::wstring(path); }
std::wstring Error(std::wstring_view what) {
  return std::wstring(what) + L" (Win32 " + std::to_wstring(GetLastError()) + L")";
}

std::expected<std::string, std::wstring> Hash(std::string_view bytes) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    return std::unexpected(L"Cannot initialize metadata SHA256");
  std::array<UCHAR, 32> digest{};
  const auto status = BCryptHash(algorithm, nullptr, 0,
      reinterpret_cast<PUCHAR>(const_cast<char *>(bytes.data())),
      static_cast<ULONG>(bytes.size()), digest.data(), static_cast<ULONG>(digest.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (status < 0) return std::unexpected(L"Cannot hash metadata");
  constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (const auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
  return result;
}

bool PlainStream(const std::wstring &path) {
  WIN32_FIND_STREAM_DATA data{};
  const auto search = FindFirstStreamW(Native(path).c_str(), FindStreamInfoStandard, &data, 0);
  if (search == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_HANDLE_EOF;
  bool plain = true;
  do { plain &= std::wstring_view(data.cStreamName) == L"::$DATA"; }
  while (FindNextStreamW(search, &data));
  const auto error = GetLastError();
  FindClose(search);
  return plain && error == ERROR_HANDLE_EOF;
}

// Caller passes fixed basenames only. Keep each handle through preview/confirm;
// a replacement or already-open writer cannot change the displayed source.
std::expected<PinnedFile, std::wstring> ReadPinned(
    const fs::DirectoryGuard &directory, std::wstring_view basename, size_t maximum) {
  const auto path = directory.path() + L"\\" + std::wstring(basename);
  Handle file(CreateFileW(Native(path).c_str(), GENERIC_READ, FILE_SHARE_READ,
      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
  if (file.h == INVALID_HANDLE_VALUE) return std::unexpected(Error(L"Cannot pin export metadata"));
  BY_HANDLE_FILE_INFORMATION info{};
  if (GetFileType(file.h) != FILE_TYPE_DISK || !GetFileInformationByHandle(file.h, &info) ||
      (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) ||
      info.nNumberOfLinks != 1 || info.nFileSizeHigh || info.nFileSizeLow > maximum || !PlainStream(path))
    return std::unexpected(L"Export input must be a bounded regular single-link file without alternate streams");
  std::string bytes(info.nFileSizeLow, '\0');
  DWORD count = 0;
  if (!::ReadFile(file.h, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) || count != bytes.size())
    return std::unexpected(L"Export input read was incomplete");
  char extra = 0;
  if (!::ReadFile(file.h, &extra, 1, &count, nullptr) || count != 0)
    return std::unexpected(L"Export input changed during read");
  return PinnedFile{std::move(file), std::move(bytes)};
}

bool LiveName(std::wstring_view name) {
  // Producer: live-YYYYMMDDTHHMMSSZ-<pid>-<sequence>. Never partial/crash/latest.
  if (name.size() < 25 || name.size() > 53 || !name.starts_with(L"live-") ||
      name[13] != L'T' || name[20] != L'Z' || name[21] != L'-') return false;
  for (size_t i = 5; i < 20; ++i)
    if (i != 13 && (name[i] < L'0' || name[i] > L'9')) return false;
  const auto separator = name.find(L'-', 22);
  if (separator == name.npos || separator == 22 || separator + 1 == name.size()) return false;
  for (const auto component : {name.substr(22, separator - 22), name.substr(separator + 1)}) {
    if (component.size() > 20 || component.front() == L'0') return false;
    for (const auto ch : component) if (ch < L'0' || ch > L'9') return false;
  }
  return true;
}

std::expected<std::wstring, std::wstring> RandomChild(const fs::DirectoryGuard &parent) {
  std::array<UCHAR, 16> bytes{};
  if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    return std::unexpected(L"Cannot allocate export identity");
  constexpr wchar_t hex[] = L"0123456789abcdef";
  auto path = parent.path() + L"\\MCLA-Report-";
  for (const auto byte : bytes) { path += hex[byte >> 4]; path += hex[byte & 15]; }
  if (!NormalizeRoot(std::u16string(path.begin(), path.end())))
    return std::unexpected(L"Export destination exceeds path policy");
  return path;
}

std::expected<void, std::wstring> WriteNew(
    const fs::DirectoryGuard &directory, std::wstring_view name, std::string_view bytes) {
  Handle file(CreateFileW(Native(directory.path() + L"\\" + std::wstring(name)).c_str(),
      GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, nullptr));
  if (file.h == INVALID_HANDLE_VALUE) return std::unexpected(Error(L"Cannot create exclusive export file"));
  DWORD count = 0;
  if (!::WriteFile(file.h, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr) ||
      count != bytes.size() || !FlushFileBuffers(file.h))
    return std::unexpected(L"Export write/flush failed");
  LARGE_INTEGER start{};
  std::string check(bytes.size(), '\0');
  if (!SetFilePointerEx(file.h, start, nullptr, FILE_BEGIN) ||
      !::ReadFile(file.h, check.data(), static_cast<DWORD>(check.size()), &count, nullptr) ||
      count != check.size() || check != bytes)
    return std::unexpected(L"Export readback differs from the preview");
  return {};
}

bool ExactOutput(const fs::DirectoryGuard &directory, size_t expected_count) {
  std::error_code error;
  std::filesystem::directory_iterator it(Native(directory.path()), error), end;
  size_t count = 0;
  for (; !error && it != end; it.increment(error)) {
    const auto name = it->path().filename().wstring();
    if (!expected_count || (name != L"report.json" && !(expected_count == 2 && name == L"EXPORT-COMPLETE.txt"))) return false;
    ++count;
  }
  return !error && count == expected_count;
}
} // namespace

#ifdef MCLA_DIAGNOSTIC_EXPORT_TESTING
void SetExportTestHook(void (*hook)(ExportTestPhase, std::wstring_view)) { test_hook = hook; }
#endif

struct ExportPlan::State {
  std::vector<fs::DirectoryGuard> protected_roots;
  std::vector<fs::DirectoryGuard> sources;
  std::optional<fs::DirectoryGuard> parent;
  std::optional<fs::WriterLock> lock;
  std::vector<PinnedFile> inputs;
  std::string report, inventory, confirmation;
  std::wstring destination;
  bool consumed = false;
};
ExportPlan::ExportPlan(std::unique_ptr<State> state) : state_(std::move(state)) {}
ExportPlan::ExportPlan(ExportPlan &&) noexcept = default;
ExportPlan &ExportPlan::operator=(ExportPlan &&) noexcept = default;
ExportPlan::~ExportPlan() = default;
const std::string &ExportPlan::report_json() const {
  static const std::string empty; return state_ ? state_->report : empty;
}
const std::string &ExportPlan::inventory_text() const {
  static const std::string empty; return state_ ? state_->inventory : empty;
}
const std::string &ExportPlan::confirmation_token() const {
  static const std::string empty; return state_ ? state_->confirmation : empty;
}
const std::wstring &ExportPlan::destination() const {
  static const std::wstring empty; return state_ ? state_->destination : empty;
}

std::expected<ExportPlan, std::wstring> PrepareLiveExport(
    const Preferences &preferences, const fs::DirectoryGuard &settings_root,
    std::string_view session_id, std::wstring_view live_package, std::wstring_view destination_parent) {
  if (!EncodePreferences(preferences) || !ValidOpaqueId(session_id) || !LiveName(live_package))
    return std::unexpected(L"Export requires valid preferences, an explicit session and a completed live package name");
  const ProfileEntry *selected = nullptr;
  for (const auto &entry : preferences.library.entries)
    if (entry.id == preferences.library.selected_id && entry.layout == Layout::ManagedExternalV1) selected = &entry;
  if (!selected) return std::unexpected(L"Select an existing managed profile for export");
  auto state = std::make_unique<ExportPlan::State>();
  auto parent = fs::OpenDirectory(destination_parent);
  if (!parent) return std::unexpected(parent.error());
  auto writable = ValidateWritableRoot(*parent);
  if (!writable) return std::unexpected(writable.error());
  state->parent.emplace(std::move(*parent));
  std::vector<std::wstring> protected_paths{settings_root.path(), Wide(preferences.build_root), Wide(preferences.game_root)};
  for (const auto &entry : preferences.library.entries) protected_paths.push_back(Wide(entry.root));
  for (const auto &path : protected_paths) {
    auto guard = fs::OpenDirectory(path);
    if (!guard) return std::unexpected(L"Cannot guard a configured protected root for export");
    auto separate = fs::CompareDisjoint(*guard, *state->parent);
    if (!separate) return std::unexpected(L"Export parent overlaps profile, build, game or settings data");
    state->protected_roots.push_back(std::move(*guard));
  }
  auto profile = fs::OpenDirectory(Wide(selected->root));
  if (!profile) return std::unexpected(profile.error());
  auto lock = fs::AcquireWriterLock(*profile, true);
  if (!lock) return std::unexpected(L"Profile is busy or has no existing managed writer lock; export writes nothing");
  state->lock.emplace(std::move(*lock));
  const auto pending = Native(profile->path() + L"\\pending-operation.dat");
  if (GetFileAttributesW(pending.c_str()) != INVALID_FILE_ATTRIBUTES || GetLastError() != ERROR_FILE_NOT_FOUND)
    return std::unexpected(L"An unfinished profile operation blocks export");
  auto marker = ReadPinned(*profile, L"managed-profile.dat", 256);
  if (!marker || marker->bytes != "MCLA managed profile v1\n" + selected->id + "\n")
    return std::unexpected(L"Selected profile identity differs from its completed managed marker");
  state->inputs.push_back(std::move(*marker));
  auto session = fs::OpenDirectory(profile->path() + L"\\sessions\\" + Wide(session_id));
  auto package = fs::OpenDirectory(profile->path() + L"\\diagnostics\\" + Wide(session_id) + L"\\live\\" + std::wstring(live_package));
  if (!session || !package) return std::unexpected(L"Selected completed session or live package is not accessible");
  auto receipt = ReadPinned(*session, L"launch-receipt.txt", 16384);
  auto status = ReadPinned(*session, L"state.txt", 256);
  auto manifest = ReadPinned(*package, L"manifest.json", 65536);
  auto diagnostic = ReadPinned(*package, L"state.json", 262144);
  if (!receipt || !status || !manifest || !diagnostic)
    return std::unexpected(L"Cannot pin required session and live-package metadata");
  std::optional<PinnedFile> capabilities;
  const auto attrs = GetFileAttributesW(Native(session->path() + L"\\capabilities.txt").c_str());
  if (attrs != INVALID_FILE_ATTRIBUTES) {
    auto read = ReadPinned(*session, L"capabilities.txt", 16384);
    if (!read) return std::unexpected(read.error());
    capabilities.emplace(std::move(*read));
  } else if (GetLastError() != ERROR_FILE_NOT_FOUND) {
    return std::unexpected(L"Cannot inspect optional capability record");
  }
  auto projection = ProjectLiveMetadata({selected->id, receipt->bytes, status->bytes,
      manifest->bytes, diagnostic->bytes, capabilities ? std::string_view(capabilities->bytes) : std::string_view{}});
  if (!projection) return std::unexpected(L"Unsupported or inconsistent export metadata: " + Wide(projection.error()));
  auto diagnostic_hash = Hash(diagnostic->bytes);
  if (!diagnostic_hash || *diagnostic_hash != projection->state_sha256 || diagnostic->bytes.size() != projection->state_bytes)
    return std::unexpected(L"Consumed diagnostic state differs from its manifest hash/length");
  state->report = std::move(projection->report_json);
  state->inputs.push_back(std::move(*receipt)); state->inputs.push_back(std::move(*status));
  state->inputs.push_back(std::move(*manifest)); state->inputs.push_back(std::move(*diagnostic));
  if (capabilities) state->inputs.push_back(std::move(*capabilities));
  state->sources.push_back(std::move(*profile)); state->sources.push_back(std::move(*session));
  state->sources.push_back(std::move(*package));
  auto report_hash = Hash(state->report);
  if (!report_hash) return std::unexpected(report_hash.error());
  state->inventory = "MCLA-METADATA-EXPORT-V1\n" + *report_hash + " " + std::to_string(state->report.size()) + " report.json\n";
  auto destination = RandomChild(*state->parent);
  if (!destination) return std::unexpected(destination.error());
  state->destination = std::move(*destination);
  // Token binds exact report/inventory, source bytes and UTF16 destination;
  // source digests and personal destination are never written to the report.
  std::string binding = state->report + state->inventory;
  for (const auto &input : state->inputs) {
    auto digest = Hash(input.bytes);
    if (!digest) return std::unexpected(digest.error());
    binding += *digest;
  }
  binding.append(reinterpret_cast<const char *>(state->destination.data()), state->destination.size() * sizeof(wchar_t));
  auto token = Hash(binding);
  if (!token) return std::unexpected(token.error());
  state->confirmation = std::move(*token);
  return ExportPlan(std::move(state));
}

std::expected<std::wstring, std::wstring> ConfirmExport(ExportPlan &plan, std::string_view token) {
  if (!plan.state_ || plan.state_->consumed || token != plan.state_->confirmation)
    return std::unexpected(L"Export preview confirmation is missing, stale or already consumed");
  auto &state = *plan.state_;
  state.consumed = true;
  auto writable = ValidateWritableRoot(*state.parent);
  if (!writable) return std::unexpected(writable.error());
  for (const auto &root : state.protected_roots)
    if (!fs::CompareDisjoint(root, *state.parent)) return std::unexpected(L"Export destination is no longer disjoint");
  if (!CreateDirectoryW(Native(state.destination).c_str(), nullptr))
    return std::unexpected(Error(L"Cannot create fresh export child; no existing data overwritten"));
  const auto incomplete = [&](std::wstring error) {
    return std::unexpected(std::move(error) + L"; incomplete export preserved at " + state.destination);
  };
  auto child = fs::OpenDirectory(state.destination);
  if (!child) return incomplete(child.error());
  // Win32 directory creation and pinning are two calls. This follows the
  // project's cooperative local-writer model, not an adversarial atomic-create
  // guarantee. Reject unexpected contents BEFORE the first write as well.
#ifdef MCLA_DIAGNOSTIC_EXPORT_TESTING
  if (test_hook) test_hook(ExportTestPhase::ChildCreated, state.destination);
#endif
  if (!ExactOutput(*child, 0)) return incomplete(L"New export directory is not empty");
  auto write = WriteNew(*child, L"report.json", state.report);
  if (!write) return incomplete(write.error());
#ifdef MCLA_DIAGNOSTIC_EXPORT_TESTING
  if (test_hook) test_hook(ExportTestPhase::ReportWritten, state.destination);
#endif
  auto report = ReadPinned(*child, L"report.json", 65536);
  if (!report || report->bytes != state.report || !ExactOutput(*child, 1))
    return incomplete(L"Export file inventory or report verification failed");
  write = WriteNew(*child, L"EXPORT-COMPLETE.txt", state.inventory);
  if (!write) return incomplete(write.error());
  auto receipt = ReadPinned(*child, L"EXPORT-COMPLETE.txt", 1024);
  if (!receipt || receipt->bytes != state.inventory || !ExactOutput(*child, 2))
    return incomplete(L"Export completion verification failed; completion marker is not sufficient without inventory verification");
  return state.destination;
}
} // namespace mcla::launcher::diagnostic_export
