// mcla - toolchain-free private campaign-test bundle launcher for
// Windows/Proton

#include <Windows.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "mcla_portable_path_policy.h"
#include "mcla_portable_adapter_policy.h"

namespace {

constexpr size_t kHashCharacters = 64;
constexpr size_t kMaximumManifestEntries = 4096;
constexpr size_t kMaximumSaveEntries = 4096;
constexpr uint64_t kMaximumSaveBytes = 512ull * 1024 * 1024;
constexpr size_t kMaximumSnapshotsPerSession = 32;
constexpr size_t kMaximumCompletedSessions = 32;
constexpr uint64_t kMaximumSessionManifestBytes = 64ull * 1024;

struct Handle {
  HANDLE value = INVALID_HANDLE_VALUE;
  Handle() = default;
  explicit Handle(HANDLE handle) : value(handle) {}
  ~Handle() {
    if (value != INVALID_HANDLE_VALUE && value != nullptr) {
      CloseHandle(value);
    }
  }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
  Handle(Handle &&other) noexcept : value(other.value) {
    other.value = INVALID_HANDLE_VALUE;
  }
  Handle &operator=(Handle &&other) noexcept {
    if (this != &other) {
      if (value != INVALID_HANDLE_VALUE && value != nullptr) {
        CloseHandle(value);
      }
      value = other.value;
      other.value = INVALID_HANDLE_VALUE;
    }
    return *this;
  }
  explicit operator bool() const {
    return value != INVALID_HANDLE_VALUE && value != nullptr;
  }
};

struct LockFile {
  Handle handle;
  std::filesystem::path path;
  LockFile() = default;
  LockFile(const LockFile &) = delete;
  LockFile &operator=(const LockFile &) = delete;
  LockFile(LockFile &&other) noexcept
      : handle(std::move(other.handle)), path(std::move(other.path)) {
    other.path.clear();
  }
  ~LockFile() {
    handle = Handle{};
    if (!path.empty()) {
      std::error_code ec;
      std::filesystem::remove(path, ec);
    }
  }
};

struct ManifestEntry {
  std::string hash;
  std::string relative_text;
  std::filesystem::path relative_path;
};

std::string ToUtf8(std::wstring_view value) {
  if (value.empty()) {
    return {};
  }
  const int bytes = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
      static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (bytes <= 0) {
    return {};
  }
  std::string result(static_cast<size_t>(bytes), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), bytes,
                      nullptr, nullptr);
  return result;
}

std::wstring FromUtf8(std::string_view value) {
  if (value.empty()) {
    return {};
  }
  const int characters =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), nullptr, 0);
  if (characters <= 0) {
    return {};
  }
  std::wstring result(static_cast<size_t>(characters), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(),
                      characters);
  return result;
}

std::string JsonEscape(std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 16);
  for (const unsigned char c : value) {
    switch (c) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (c < 0x20) {
        char buffer[7]{};
        std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
        escaped += buffer;
      } else {
        escaped.push_back(static_cast<char>(c));
      }
    }
  }
  return escaped;
}

std::string UtcStamp() {
  SYSTEMTIME time{};
  GetSystemTime(&time);
  char buffer[32]{};
  std::snprintf(buffer, sizeof(buffer), "%04u%02u%02uT%02u%02u%02uZ",
                time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute,
                time.wSecond);
  return buffer;
}

std::filesystem::path ExecutableRoot() {
  std::wstring buffer(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                          static_cast<DWORD>(buffer.size()));
  if (!length || length >= buffer.size()) {
    return {};
  }
  buffer.resize(length);
  return std::filesystem::path(buffer).parent_path();
}

bool WriteText(const std::filesystem::path &path, std::string_view text) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  stream.flush();
  return static_cast<bool>(stream);
}

bool WriteTextAtomic(const std::filesystem::path &path, std::string_view text) {
  auto temporary = path;
  temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId());
  if (!WriteText(temporary, text)) {
    return false;
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::error_code ec;
    std::filesystem::remove(temporary, ec);
    return false;
  }
  return true;
}

bool RenameDirectoryAtomic(const std::filesystem::path &source,
                           const std::filesystem::path &destination,
                           std::string &error) {
  DWORD last_error = ERROR_SUCCESS;
  for (size_t attempt = 0; attempt != 20; ++attempt) {
    if (MoveFileExW(source.c_str(), destination.c_str(),
                    MOVEFILE_WRITE_THROUGH)) {
      return true;
    }
    last_error = GetLastError();
    if (last_error != ERROR_ACCESS_DENIED &&
        last_error != ERROR_SHARING_VIOLATION &&
        last_error != ERROR_LOCK_VIOLATION) {
      break;
    }
    Sleep(50);
  }
  error =
      "atomic directory rename failed (win32=" + std::to_string(last_error) +
      ")";
  return false;
}

std::optional<std::string> ReadText(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return std::nullopt;
  }
  std::ostringstream output;
  output << stream.rdbuf();
  if (!stream.good() && !stream.eof()) {
    return std::nullopt;
  }
  return output.str();
}

std::string Trim(std::string value) {
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  return value.substr(start);
}

bool IsHexHash(std::string_view value) {
  return value.size() == kHashCharacters &&
         std::all_of(value.begin(), value.end(),
                     [](unsigned char c) { return std::isxdigit(c) != 0; });
}

std::optional<std::string> Sha256(const std::filesystem::path &path) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD object_bytes = 0;
  DWORD hash_bytes = 0;
  DWORD copied = 0;
  std::vector<UCHAR> object;
  std::vector<UCHAR> digest;
  auto cleanup = [&]() {
    if (hash) {
      BCryptDestroyHash(hash);
    }
    if (algorithm) {
      BCryptCloseAlgorithmProvider(algorithm, 0);
    }
  };
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0 ||
      BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&object_bytes),
                        sizeof(object_bytes), &copied, 0) < 0 ||
      BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                        reinterpret_cast<PUCHAR>(&hash_bytes),
                        sizeof(hash_bytes), &copied, 0) < 0) {
    cleanup();
    return std::nullopt;
  }
  object.resize(object_bytes);
  digest.resize(hash_bytes);
  if (BCryptCreateHash(algorithm, &hash, object.data(), object_bytes, nullptr,
                       0, 0) < 0) {
    cleanup();
    return std::nullopt;
  }
  std::ifstream stream(path, std::ios::binary);
  std::vector<char> buffer(1024 * 1024);
  while (stream) {
    stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto read = stream.gcount();
    if (read > 0 &&
        BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                       static_cast<ULONG>(read), 0) < 0) {
      cleanup();
      return std::nullopt;
    }
  }
  if (!stream.eof() ||
      BCryptFinishHash(hash, digest.data(), hash_bytes, 0) < 0) {
    cleanup();
    return std::nullopt;
  }
  std::ostringstream hex;
  hex << std::uppercase << std::hex << std::setfill('0');
  for (const UCHAR byte : digest) {
    hex << std::setw(2) << static_cast<unsigned>(byte);
  }
  cleanup();
  return hex.str();
}

bool HasReparseAttribute(const std::filesystem::path &path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes == INVALID_FILE_ATTRIBUTES ||
         (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool SafeRelative(const std::filesystem::path &path) {
  if (path.empty() || path.is_absolute() || path.has_root_name() ||
      path.has_root_directory()) {
    return false;
  }
  for (const auto &component : path) {
    if (component == L".." || component == L"." || component.empty()) {
      return false;
    }
  }
  return true;
}

bool IsRecoverableSessionPartialPath(const std::filesystem::path &root,
                                     const std::filesystem::path &path) {
  const auto relative = path.lexically_relative(root);
  if (!SafeRelative(relative)) {
    return false;
  }
  auto component = relative.begin();
  if (component == relative.end() || *component != L"results") {
    return false;
  }
  ++component;
  if (component == relative.end()) {
    return false;
  }
  std::string session = ToUtf8(component->wstring());
  std::transform(
      session.begin(), session.end(), session.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return session.starts_with("session-") && session.ends_with(".partial");
}

bool ValidateTree(const std::filesystem::path &root, std::string &error,
                  bool allow_recoverable_session_partials = false) {
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator
           it(root, std::filesystem::directory_options::skip_permission_denied,
              ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      error = "cannot enumerate the bundle tree: " + ec.message();
      return false;
    }
    const auto name = ToUtf8(it->path().filename().wstring());
    std::string lower = name;
    std::transform(
        lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (HasReparseAttribute(it->path())) {
      error = "reparse points are not allowed in the bundle: " + name;
      return false;
    }
    if (lower.find(".sync-conflict-") != std::string::npos ||
        lower.starts_with(".syncthing.")) {
      error = "Syncthing conflict or incomplete transfer found: " + name;
      return false;
    }
    if (lower.ends_with(".partial")) {
      const auto relative = it->path().lexically_relative(root);
      bool unfinished_live_diagnostic = false;
      if (SafeRelative(relative)) {
        const bool is_directory = it->is_directory(ec);
        if (ec) {
          error = "cannot inspect partial bundle entry: " + ec.message();
          return false;
        }
        unfinished_live_diagnostic =
            mcla::portable::IsUnfinishedLiveDiagnosticPartial(
                ToUtf8(relative.generic_wstring()), is_directory);
      }
      if (!unfinished_live_diagnostic &&
          (!allow_recoverable_session_partials ||
           !IsRecoverableSessionPartialPath(root, it->path()))) {
        error = "incomplete bundle/session publication found: " + name;
        return false;
      }
      // Keep traversing unfinished diagnostics for unsafe descendants. Do not
      // publish, recover, delete or use their private save copies as a seed.
    }
  }
  if (ec) {
    error = "cannot enumerate the bundle tree: " + ec.message();
    return false;
  }
  return true;
}

bool ParseAndVerifyManifest(const std::filesystem::path &root,
                            size_t &verified_files, std::string &error) {
  const auto manifest_path = root / L"bundle-files.sha256";
  const auto text = ReadText(manifest_path);
  if (!text) {
    error = "bundle-files.sha256 is missing or unreadable";
    return false;
  }
  std::istringstream lines(*text);
  std::string line;
  std::vector<ManifestEntry> entries;
  std::set<std::string> unique;
  while (std::getline(lines, line)) {
    line = Trim(std::move(line));
    if (line.empty()) {
      continue;
    }
    if (line.size() < 67 || line[64] != ' ' || line[65] != '*' ||
        !IsHexHash(std::string_view(line).substr(0, 64))) {
      error = "bundle-files.sha256 contains a malformed row";
      return false;
    }
    auto relative_text = line.substr(66);
    std::replace(relative_text.begin(), relative_text.end(), '\\', '/');
    const auto relative = std::filesystem::path(FromUtf8(relative_text));
    if (!SafeRelative(relative) || !unique.insert(relative_text).second) {
      error = "bundle-files.sha256 contains an unsafe or duplicate path";
      return false;
    }
    std::string expected = line.substr(0, 64);
    std::transform(
        expected.begin(), expected.end(), expected.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    entries.push_back(
        {std::move(expected), std::move(relative_text), relative});
    if (entries.size() > kMaximumManifestEntries) {
      error = "bundle-files.sha256 exceeds the entry bound";
      return false;
    }
  }
  const std::array required = {
      "Launch-MCLA.exe",    "bundle-id.txt",       "bundle-manifest.json",
      "game-manifest.json", "bin/mcla.exe",        "bin/mcla_crash_handler.exe",
      "bin/rexruntime.dll", "bin/TracyClient.dll", "bin/rexgpu-xenos.dll",
      "bin/mcla.toml",      "game/default.xex",
      // Each executable directory needs its own app-local CRT closure.
      // These hashes are an integrity check, not a pre-execution trust boundary:
      // the loader has already loaded this launcher's imported root DLLs.
      "msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll",
      "bin/msvcp140.dll", "bin/vcruntime140.dll",
      "bin/vcruntime140_1.dll", "bin/msvcp140_atomic_wait.dll"};
  for (const auto *name : required) {
    if (!unique.contains(name)) {
      error = std::string("required immutable file is absent from manifest: ") +
              name;
      return false;
    }
  }
  for (const auto &entry : entries) {
    const auto candidate = root / entry.relative_path;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(candidate, ec) || ec ||
        HasReparseAttribute(candidate)) {
      error = "manifest file is missing or unsafe: " + entry.relative_text;
      return false;
    }
    const auto actual = Sha256(candidate);
    if (!actual || *actual != entry.hash) {
      error = "SHA-256 mismatch: " + entry.relative_text;
      return false;
    }
  }
  verified_files = entries.size();
  return true;
}

std::optional<std::string> ReadBundleId(const std::filesystem::path &root) {
  auto value = ReadText(root / L"bundle-id.txt");
  if (!value) {
    return std::nullopt;
  }
  *value = Trim(std::move(*value));
  if (value->empty() || value->size() > 128 ||
      !std::all_of(value->begin(), value->end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-';
      })) {
    return std::nullopt;
  }
  return value;
}

std::optional<LockFile> AcquireLock(const std::filesystem::path &root,
                                    std::string &error) {
  LockFile lock;
  lock.path = root / L"bundle.lock";
  auto create = [&]() {
    return Handle(
        CreateFileW(lock.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
  };
  lock.handle = create();
  if (!lock.handle && (GetLastError() == ERROR_FILE_EXISTS ||
                       GetLastError() == ERROR_ALREADY_EXISTS)) {
    // The live launcher owns this file without sharing. Deletion therefore
    // succeeds only after an abrupt Steam/OS termination has closed the real
    // exclusive handle, and never steals an active writer's lock.
    if (DeleteFileW(lock.path.c_str())) {
      lock.handle = create();
      if (lock.handle) {
        std::cout << "Recovered stale bundle.lock from an interrupted prior "
                     "launcher.\n";
      }
    }
  }
  if (!lock.handle) {
    error =
        "bundle.lock already exists or cannot be created; only one host may "
        "run this writable bundle at a time";
    lock.path.clear();
    return std::nullopt;
  }
  const std::string body =
      "{\n  \"schema\": \"mcla-portable-lock-v1\",\n  \"started_utc\": \"" +
      UtcStamp() +
      "\",\n  \"process_id\": " + std::to_string(GetCurrentProcessId()) +
      "\n}\n";
  DWORD written = 0;
  if (!WriteFile(lock.handle.value, body.data(),
                 static_cast<DWORD>(body.size()), &written, nullptr) ||
      written != body.size() || !FlushFileBuffers(lock.handle.value)) {
    error = "cannot publish bundle.lock";
    return std::nullopt;
  }
  return lock;
}

std::optional<std::string> JsonStringField(std::string_view json,
                                           std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  size_t position = json.find(needle);
  if (position == std::string_view::npos) {
    return std::nullopt;
  }
  position = json.find(':', position + needle.size());
  if (position == std::string_view::npos) {
    return std::nullopt;
  }
  ++position;
  while (position < json.size() &&
         std::isspace(static_cast<unsigned char>(json[position]))) {
    ++position;
  }
  if (position >= json.size() || json[position] != '"') {
    return std::nullopt;
  }
  ++position;
  std::string value;
  while (position < json.size()) {
    const char character = json[position++];
    if (character == '"') {
      return value;
    }
    // Launcher-generated session fields are deliberately restricted to plain
    // ASCII tokens. Reject escapes instead of ambiguously decoding input that
    // may have been altered during synchronization.
    if (character == '\\' || static_cast<unsigned char>(character) < 0x20) {
      return std::nullopt;
    }
    value.push_back(character);
  }
  return std::nullopt;
}

std::wstring Quote(std::wstring_view value) {
  std::wstring quoted = L"\"";
  size_t slashes = 0;
  for (const wchar_t c : value) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    if (c == L'"') {
      quoted.append(slashes * 2 + 1, L'\\');
      quoted.push_back(L'"');
      slashes = 0;
      continue;
    }
    quoted.append(slashes, L'\\');
    slashes = 0;
    quoted.push_back(c);
  }
  quoted.append(slashes * 2, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

std::wstring Flag(std::wstring_view name, const std::filesystem::path &path) {
  return std::wstring(name) + L"=" + Quote(path.wstring());
}

bool CopyProfileTree(const std::filesystem::path &source,
                     const std::filesystem::path &destination,
                     std::string &error) {
  std::error_code ec;
  std::filesystem::create_directories(destination, ec);
  if (ec) {
    error = "cannot create save snapshot root: " + ec.message();
    return false;
  }
  size_t entries = 0;
  uint64_t bytes = 0;
  for (std::filesystem::recursive_directory_iterator
           it(source,
              std::filesystem::directory_options::skip_permission_denied, ec),
       end;
       it != end; it.increment(ec)) {
    if (ec || ++entries > kMaximumSaveEntries ||
        HasReparseAttribute(it->path())) {
      error = "save snapshot source is unreadable, too large, or contains a "
              "reparse point";
      return false;
    }
    const auto relative = std::filesystem::relative(it->path(), source, ec);
    if (ec || !SafeRelative(relative)) {
      error = "save snapshot relative path is unsafe";
      return false;
    }
    const auto target = destination / relative;
    if (it->is_directory(ec)) {
      std::filesystem::create_directories(target, ec);
    } else if (it->is_regular_file(ec)) {
      const auto size = it->file_size(ec);
      if (ec || size > kMaximumSaveBytes - bytes) {
        error = "save snapshot exceeds the byte bound";
        return false;
      }
      bytes += size;
      std::filesystem::create_directories(target.parent_path(), ec);
      if (!ec) {
        std::filesystem::copy_file(
            it->path(), target,
            std::filesystem::copy_options::overwrite_existing, ec);
      }
    }
    if (ec) {
      error = "cannot copy save snapshot: " + ec.message();
      return false;
    }
  }
  return true;
}

struct SaveState {
  std::string save_hash;
  std::string header_hash;
};

std::optional<SaveState> ReadSaveState(const std::filesystem::path &user_root) {
  const auto save =
      user_root / L"B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav";
  const auto header =
      user_root / L"B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header";
  std::error_code ec;
  if (!std::filesystem::is_regular_file(save, ec) || ec ||
      !std::filesystem::is_regular_file(header, ec) || ec) {
    return std::nullopt;
  }
  const auto save_hash = Sha256(save);
  const auto header_hash = Sha256(header);
  if (!save_hash || !header_hash) {
    return std::nullopt;
  }
  return SaveState{*save_hash, *header_hash};
}

void PruneDirectories(const std::filesystem::path &root, size_t keep,
                      std::string_view required_prefix = {}) {
  std::error_code ec;
  std::vector<std::filesystem::directory_entry> entries;
  for (const auto &entry : std::filesystem::directory_iterator(root, ec)) {
    if (ec) {
      return;
    }
    const auto name = ToUtf8(entry.path().filename().wstring());
    if (!entry.is_directory(ec) || ec || HasReparseAttribute(entry.path()) ||
        name.ends_with(".partial") ||
        (!required_prefix.empty() && !name.starts_with(required_prefix))) {
      ec.clear();
      continue;
    }
    entries.push_back(entry);
  }
  std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
    std::error_code left_error;
    std::error_code right_error;
    return a.last_write_time(left_error) > b.last_write_time(right_error);
  });
  for (size_t index = keep; index < entries.size(); ++index) {
    std::filesystem::remove_all(entries[index].path(), ec);
    ec.clear();
  }
}

bool CaptureSave(const std::filesystem::path &user_root,
                 const std::filesystem::path &session_root,
                 std::string_view reason, std::optional<SaveState> &last,
                 size_t &snapshot_count, std::string &error) {
  const auto before = ReadSaveState(user_root);
  if (!before) {
    return true;
  }
  if (last && before->save_hash == last->save_hash &&
      before->header_hash == last->header_hash) {
    return true;
  }
  // Keep this deliberately short.  The portable bundle may itself live under
  // a deep Syncthing root, and the complete Xbox profile tree has several
  // nested components.  A content-addressed directory also makes repeated
  // captures of the same save state idempotent.
  const auto snapshots = session_root / L"saves";
  std::error_code ec;
  std::filesystem::create_directories(snapshots, ec);
  if (ec) {
    error = "cannot create saves directory: " + ec.message();
    return false;
  }
  const std::string name = before->save_hash.substr(0, 16);
  const auto partial = snapshots / FromUtf8(name + ".partial");
  const auto completed = snapshots / FromUtf8(name);
  if (std::filesystem::is_directory(completed, ec) && !ec) {
    last = before;
    return true;
  }
  ec.clear();
  std::filesystem::remove_all(partial, ec);
  const auto source_profile = user_root / L"B13EBABEBABEBABE";
  const auto copied_profile = partial / L"B13EBABEBABEBABE";
  if (!CopyProfileTree(source_profile, copied_profile, error)) {
    std::filesystem::remove_all(partial, ec);
    return false;
  }
  const auto after = ReadSaveState(user_root);
  const auto copied = ReadSaveState(partial);
  if (!after || !copied || before->save_hash != after->save_hash ||
      before->header_hash != after->header_hash ||
      before->save_hash != copied->save_hash ||
      before->header_hash != copied->header_hash) {
    std::filesystem::remove_all(partial, ec);
    return true;
  }
  const std::string manifest =
      "{\n  \"schema\": \"mcla-portable-save-snapshot-v1\",\n  "
      "\"captured_utc\": \"" +
      UtcStamp() + "\",\n  \"reason\": \"" + JsonEscape(reason) +
      "\",\n  \"save_sha256\": \"" + copied->save_hash +
      "\",\n  \"header_sha256\": \"" + copied->header_hash +
      "\",\n  \"complete_profile_tree\": true\n}\n";
  if (!WriteText(partial / L"snapshot.json", manifest)) {
    error = "cannot write save snapshot manifest";
    std::filesystem::remove_all(partial, ec);
    return false;
  }
  if (!RenameDirectoryAtomic(partial, completed, error)) {
    error = "cannot atomically publish save snapshot: " + error;
    return false;
  }
  last = copied;
  ++snapshot_count;
  PruneDirectories(snapshots, kMaximumSnapshotsPerSession);
  return true;
}

size_t CountCompletedSaveSnapshots(const std::filesystem::path &session_root) {
  const auto snapshots = session_root / L"saves";
  std::error_code ec;
  size_t count = 0;
  for (const auto &entry : std::filesystem::directory_iterator(snapshots, ec)) {
    if (ec) {
      return count;
    }
    const auto name = ToUtf8(entry.path().filename().wstring());
    if (entry.is_directory(ec) && !ec && !HasReparseAttribute(entry.path()) &&
        name.size() == 16 &&
        std::all_of(name.begin(), name.end(),
                    [](unsigned char c) { return std::isxdigit(c) != 0; })) {
      ++count;
    }
    ec.clear();
  }
  return count;
}

bool RemoveInterruptedTemporaryState(const std::filesystem::path &session_root,
                                     std::string &error) {
  std::error_code ec;
  std::vector<std::filesystem::path> temporary;
  for (std::filesystem::recursive_directory_iterator
           it(session_root,
              std::filesystem::directory_options::skip_permission_denied, ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      error = "cannot enumerate interrupted session state: " + ec.message();
      return false;
    }
    std::string name = ToUtf8(it->path().filename().wstring());
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (name.ends_with(".partial") || name.find(".tmp-") != std::string::npos) {
      if (it->is_directory(ec) && !ec) {
        it.disable_recursion_pending();
      }
      ec.clear();
      temporary.push_back(it->path());
    }
  }
  if (ec) {
    error = "cannot enumerate interrupted session state: " + ec.message();
    return false;
  }
  std::sort(temporary.begin(), temporary.end(),
            [](const auto &left, const auto &right) {
              return left.native().size() > right.native().size();
            });
  for (const auto &path : temporary) {
    std::filesystem::remove_all(path, ec);
    if (ec) {
      error = "cannot remove interrupted temporary state: " + ec.message();
      return false;
    }
  }
  return true;
}

bool RecoverInterruptedSessions(const std::filesystem::path &root,
                                std::string_view bundle_id,
                                size_t verified_files, std::string &error) {
  const auto results = root / L"results";
  std::error_code ec;
  std::vector<std::filesystem::path> interrupted;
  for (const auto &entry : std::filesystem::directory_iterator(results, ec)) {
    if (ec) {
      error = "cannot enumerate session results: " + ec.message();
      return false;
    }
    std::string name = ToUtf8(entry.path().filename().wstring());
    std::string lower = name;
    std::transform(
        lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!lower.starts_with("session-") || !lower.ends_with(".partial")) {
      continue;
    }
    if (!entry.is_directory(ec) || ec || HasReparseAttribute(entry.path())) {
      error =
          "interrupted session staging path is not a safe directory: " + name;
      return false;
    }
    interrupted.push_back(entry.path());
  }
  if (ec) {
    error = "cannot enumerate session results: " + ec.message();
    return false;
  }

  std::sort(interrupted.begin(), interrupted.end());
  for (const auto &partial : interrupted) {
    const std::string partial_name = ToUtf8(partial.filename().wstring());
    const std::string session_id = partial_name.substr(
        0, partial_name.size() - std::string(".partial").size());
    const auto completed = partial.parent_path() / FromUtf8(session_id);
    if (std::filesystem::exists(completed, ec) || ec) {
      error = "interrupted session destination already exists: " + session_id;
      return false;
    }

    const auto session_path = partial / L"session.json";
    const auto manifest_bytes = std::filesystem::file_size(session_path, ec);
    if (ec || manifest_bytes > kMaximumSessionManifestBytes) {
      error = "interrupted session manifest is missing or exceeds its bound: " +
              session_id;
      return false;
    }
    const auto session = ReadText(session_path);
    const auto schema =
        session ? JsonStringField(*session, "schema") : std::nullopt;
    const auto recorded_bundle =
        session ? JsonStringField(*session, "bundle_id") : std::nullopt;
    const auto recorded_session =
        session ? JsonStringField(*session, "session_id") : std::nullopt;
    const auto started_utc =
        session ? JsonStringField(*session, "started_utc") : std::nullopt;
    const auto mode =
        session ? JsonStringField(*session, "mode") : std::nullopt;
    const auto state =
        session ? JsonStringField(*session, "state") : std::nullopt;
    if (!schema || *schema != "mcla-portable-session-v1" || !recorded_bundle ||
        *recorded_bundle != bundle_id || !recorded_session ||
        *recorded_session != session_id || !started_utc || !mode ||
        (*mode != "gameplay" && *mode != "diagnostics-probe") || !state ||
        *state != "running") {
      error = "interrupted session manifest identity is invalid: " + session_id;
      return false;
    }
    if (!RemoveInterruptedTemporaryState(partial, error)) {
      return false;
    }

    std::optional<SaveState> last_save;
    size_t snapshots = CountCompletedSaveSnapshots(partial);
    if (!CaptureSave(root / L"user", partial, "launcher-recovery", last_save,
                     snapshots, error)) {
      return false;
    }
    const auto final_save = ReadSaveState(root / L"user");
    const std::string recovered_utc = UtcStamp();
    const std::string result =
        "{\n  \"schema\": \"mcla-portable-interrupted-session-result-v1\",\n  "
        "\"bundle_id\": \"" +
        JsonEscape(bundle_id) + "\",\n  \"session_id\": \"" +
        JsonEscape(session_id) + "\",\n  \"started_utc\": \"" +
        JsonEscape(*started_utc) + "\",\n  \"recovered_utc\": \"" +
        recovered_utc + "\",\n  \"mode\": \"" + JsonEscape(*mode) +
        "\",\n  \"exit_code\": null,\n  \"verified_file_count\": " +
        std::to_string(verified_files) +
        ",\n  \"save_snapshot_count\": " + std::to_string(snapshots) +
        ",\n  \"final_save_sha256\": " +
        (final_save ? "\"" + final_save->save_hash + "\"" : "null") +
        ",\n  \"final_header_sha256\": " +
        (final_save ? "\"" + final_save->header_hash + "\"" : "null") +
        ",\n  \"interruption_reason\": "
        "\"launcher-terminated-before-session-finalization\",\n  "
        "\"state\": \"interrupted\"\n}\n";
    if (!WriteTextAtomic(partial / L"result.json", result)) {
      error = "cannot publish interrupted session result: " + session_id;
      return false;
    }
    if (!RenameDirectoryAtomic(partial, completed, error)) {
      error = "cannot atomically recover interrupted session: " + error;
      return false;
    }
    std::cout << "Recovered interrupted MCLA session: " << session_id
              << " (save snapshots " << snapshots << ")\n";
  }
  return true;
}

bool HasHistoryCapacity(const std::filesystem::path &root, std::string &error) {
  // Keep the complete seed-to-tip provenance, including administrative rename
  // edges. Pruning only sessions can both exceed the reader's combined bound
  // and remove the seed edge. Require an archived successor before record33.
  std::error_code ec;
  size_t records = 0;
  for (std::filesystem::directory_iterator it(root / L"results", ec), end;
       !ec && it != end; it.increment(ec)) {
    const auto name = it->path().filename().wstring();
    if (name.starts_with(L"session-") || name.starts_with(L"profile-rename-")) {
      if (HasReparseAttribute(it->path()) || !it->is_directory(ec) || ec) {
        error = "invalid portable history entry; preserve results for recovery";
        return false;
      }
      if (++records >= kMaximumCompletedSessions) {
        error = "portable session/profile-rename history is full (32 records); "
                "preserve this bundle and publish an archived successor before "
                "playing or renaming again; do not delete history";
        return false;
      }
    }
  }
  if (ec) {
    error = "cannot inspect portable history; preserve results for recovery";
    return false;
  }
  return true;
}

int Fail(std::string_view message) {
  const auto wide = FromUtf8(std::string(message));
  std::wcerr << L"MCLA portable launcher: " << wide << L"\n";
  MessageBoxW(nullptr, wide.c_str(), L"MCLA-R portable launcher",
              MB_OK | MB_ICONERROR | MB_TASKMODAL);
  return 1;
}

bool EnvironmentVariablePresent(const wchar_t *name) {
  SetLastError(ERROR_SUCCESS);
  const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
  // Windows lookup is case insensitive. Empty variables and unexpected API
  // errors also fail closed; never disclose the inherited value.
  return size != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
}

bool WriteProfileName(const std::filesystem::path &path, std::string_view name) {
  // Exclusive temporary creation prevents following a preexisting file or
  // hardlink. Rename replaces only the directory entry, never another link's
  // contents. Keep failed temporary state for explicit recovery.
  const auto temporary = path.parent_path() / L"local-display-name.partial";
  Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                          CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file) {
    return false;
  }
  DWORD written = 0;
  if (!WriteFile(file.value, name.data(), static_cast<DWORD>(name.size()),
                 &written, nullptr) || written != name.size() ||
      !FlushFileBuffers(file.value)) {
    return false;
  }
  file = Handle{};
  return MoveFileExW(temporary.c_str(), path.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

using ProfileInventory = std::map<std::filesystem::path, std::string>;
std::optional<ProfileInventory> ProfileInventoryOf(const std::filesystem::path &root) {
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec) || ec || HasReparseAttribute(root)) {
    return std::nullopt;
  }
  ProfileInventory inventory;
  uint64_t total = 0;
  for (std::filesystem::recursive_directory_iterator it(root, ec), end;
       it != end; it.increment(ec)) {
    if (ec || inventory.size() >= kMaximumSaveEntries || HasReparseAttribute(it->path())) {
      return std::nullopt;
    }
    const auto relative = it->path().lexically_relative(root);
    if (!SafeRelative(relative)) {
      return std::nullopt;
    }
    if (it->is_directory(ec) && !ec) {
      inventory.emplace(relative, "directory");
    } else if (it->is_regular_file(ec) && !ec) {
      const auto bytes = it->file_size(ec);
      if (ec || bytes > kMaximumSaveBytes - total) {
        return std::nullopt;
      }
      total += bytes;
      const auto hash = Sha256(it->path());
      if (!hash) {
        return std::nullopt;
      }
      inventory.emplace(relative, std::to_string(bytes) + ":" + *hash);
    } else {
      return std::nullopt;
    }
  }
  return ec ? std::nullopt : std::optional(std::move(inventory));
}

bool RenameProfileWithReceipt(const std::filesystem::path &root,
                              const std::string &bundle_id, size_t verified_files,
                              std::string_view name, std::string &error) {
  if (!HasHistoryCapacity(root, error)) {
    return false;
  }
  const auto profile = root / L"user" / L"B13EBABEBABEBABE";
  const auto before_state = ReadSaveState(root / L"user");
  const auto before = ProfileInventoryOf(profile);
  const auto nonempty = [&before](const std::filesystem::path &path) {
    if (!before) { return false; }
    const auto found = before->find(path);
    return found != before->end() && found->second != "directory" &&
           !found->second.starts_with("0:");
  };
  if (!before_state || !before ||
      !nonempty(L"545407F8/00000001/mc4.sav/mc4.sav") ||
      !nonempty(L"545407F8/Headers/00000001/mc4.sav.header")) {
    error = "rename requires a complete existing save/profile; no profile will be seeded";
    return false;
  }
  const std::string id = "profile-rename-" + UtcStamp() + "-" + std::to_string(GetCurrentProcessId());
  const auto partial = root / L"results" / FromUtf8(id + ".partial");
  const auto completed = root / L"results" / FromUtf8(id);
  std::error_code ec;
  if (!std::filesystem::create_directory(partial, ec) || ec) {
    error = "cannot exclusively create profile rename journal";
    return false;
  }
  const auto before_copy = partial / L"before" / L"B13EBABEBABEBABE";
  const auto after_copy = partial / L"after" / L"B13EBABEBABEBABE";
  if (!CopyProfileTree(profile, before_copy, error) ||
      ProfileInventoryOf(before_copy) != before || ProfileInventoryOf(profile) != before) {
    error = "profile changed while preserving pre-rename snapshot; journal retained";
    return false;
  }
  if (!WriteProfileName(profile / L"local-display-name.txt", name)) {
    error = "profile preference publication failed; journal retained";
    return false;
  }
  const auto after = ProfileInventoryOf(profile);
  if (!after) {
    error = "cannot verify renamed profile; journal retained";
    return false;
  }
  auto before_game = *before;
  auto after_game = *after;
  before_game.erase(L"local-display-name.txt");
  after_game.erase(L"local-display-name.txt");
  if (before_game != after_game ||
      !CopyProfileTree(profile, after_copy, error) ||
      ProfileInventoryOf(after_copy) != after || ProfileInventoryOf(profile) != after) {
    error = "unexpected profile change during rename; journal retained for recovery";
    return false;
  }
  const std::string receipt =
      "{\"schema\":\"mcla-portable-profile-rename-v1\",\"bundle_id\":\"" +
      JsonEscape(bundle_id) + "\",\"transition_id\":\"" + id +
      "\",\"completed_utc\":\"" + UtcStamp() +
      "\",\"verified_file_count\":" + std::to_string(verified_files) + "}\n";
  if (!WriteTextAtomic(partial / L"rename.json", receipt) ||
      !RenameDirectoryAtomic(partial, completed, error)) {
    error = "cannot publish completed profile rename journal; preserve partial state";
    return false;
  }
  return true;
}

std::optional<uint32_t> ResolveNamedAdapter(std::wstring_view name,
                                          std::string &error) {
  Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
  if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.GetAddressOf())))) {
    error = "cannot create DXGI factory for named adapter selection";
    return std::nullopt;
  }
  std::vector<mcla::portable::AdapterCandidate> candidates;
  // Use the same raw EnumAdapters1 ordinal and FL11_0 capability check as the
  // SDK provider. Do not compact indices after excluding incapable adapters.
  for (UINT index = 0; index < 128; ++index) {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    const HRESULT result = factory->EnumAdapters1(index, adapter.GetAddressOf());
    if (result == DXGI_ERROR_NOT_FOUND) {
      return mcla::portable::SelectExactAdapter(candidates, name, error);
    }
    if (FAILED(result)) {
      error = "DXGI adapter enumeration failed; no partial-list selection";
      return std::nullopt;
    }
    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(adapter->GetDesc1(&description))) {
      error = "DXGI adapter description unavailable; no partial-list selection";
      return std::nullopt;
    }
    const bool capable = SUCCEEDED(D3D12CreateDevice(
        adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
    candidates.push_back({index, description.Description, capable});
  }
  error = "DXGI adapter enumeration exceeds its bound";
  return std::nullopt;
}

} // namespace

int wmain(int argc, wchar_t **argv) {
  bool adapter_check_requested = false;
  bool rename_requested = false;
  for (int index = 1; index < argc; ++index) {
    adapter_check_requested |=
        std::wstring_view(argv[index]) == L"--verify-adapter-only";
    rename_requested |= std::wstring_view(argv[index]).starts_with(L"--set-profile-name=");
  }
  const auto argument_fail = [adapter_check_requested, rename_requested](std::string_view message) {
    if (adapter_check_requested || rename_requested) {
      std::cerr << "MCLA adapter verification: " << message << '\n';
      return 1;
    }
    return Fail(message);
  };
  bool verify_only = false;
  bool diagnostics_probe = false;
  bool verify_adapter_only = false;
  std::optional<std::wstring> gpu_backend;
  std::optional<std::wstring> adapter_name;
  std::optional<std::string> requested_profile_name;
  for (int index = 1; index < argc; ++index) {
    const std::wstring_view argument(argv[index]);
    if (argument.starts_with(L"--set-profile-name=")) {
      const auto value = ToUtf8(argument.substr(std::wstring_view(L"--set-profile-name=").size()));
      if (requested_profile_name || !mcla::portable::ValidLocalProfileName(value)) {
        return argument_fail("profile name requires 1-15 ASCII letters, digits, spaces, _ or -; no edge spaces");
      }
      requested_profile_name = value;
    } else if (argument == L"--verify-only") {
      verify_only = true;
    } else if (argument == L"--diagnostics-probe") {
      diagnostics_probe = true;
    } else if (argument == L"--verify-adapter-only") {
      if (verify_adapter_only) {
        return argument_fail("--verify-adapter-only must be specified once");
      }
      verify_adapter_only = true;
    } else if (argument.starts_with(L"--d3d12-adapter-name=")) {
      const auto value = argument.substr(std::wstring_view(L"--d3d12-adapter-name=").size());
      if (adapter_name || !mcla::portable::ValidAdapterName(value)) {
        return argument_fail("--d3d12-adapter-name requires one bounded exact name");
      }
      adapter_name = value;
    } else if (argument.starts_with(L"--gpu-backend=")) {
      const std::wstring value(argument.substr(14));
      if ((value != L"any" && value != L"d3d12" && value != L"vulkan") ||
          gpu_backend) {
        return argument_fail(
            "--gpu-backend must be specified once as any, d3d12, or vulkan");
      }
      gpu_backend = value;
    } else {
      return argument_fail("unknown launcher argument");
    }
  }
  if (verify_only && diagnostics_probe) {
    return argument_fail("--verify-only and --diagnostics-probe are mutually exclusive");
  }
  if (verify_adapter_only && (verify_only || diagnostics_probe || !adapter_name)) {
    return argument_fail("--verify-adapter-only requires a name and excludes bundle modes");
  }
  if (requested_profile_name && (verify_only || diagnostics_probe || verify_adapter_only || gpu_backend || adapter_name)) {
    return argument_fail("profile rename is a standalone operation, not a game/verification mode");
  }
  if (((gpu_backend || adapter_name) && EnvironmentVariablePresent(L"REX_GPU_BACKEND")) ||
      (adapter_name && EnvironmentVariablePresent(L"REX_D3D12_ADAPTER"))) {
    return argument_fail("remove inherited REX_GPU_BACKEND/REX_D3D12_ADAPTER before explicit GPU selection");
  }
  if (!verify_adapter_only && EnvironmentVariablePresent(L"REX_LOCAL_USER_DISPLAY_NAME")) {
    return argument_fail("remove inherited REX_LOCAL_USER_DISPLAY_NAME; use the persistent launcher profile setting");
  }
  std::optional<uint32_t> adapter_index;
  if (adapter_name) {
    if (!mcla::portable::CompatibleNamedBackend(gpu_backend.value_or(L""))) {
      return argument_fail("named D3D12 adapter conflicts with requested GPU backend");
    }
    gpu_backend = L"d3d12";
    std::string adapter_error;
    adapter_index = ResolveNamedAdapter(*adapter_name, adapter_error);
    if (!adapter_index) {
      return argument_fail(adapter_error);
    }
    std::cout << "Named D3D12 adapter selected: " << ToUtf8(*adapter_name)
              << " (EnumAdapters1 index " << *adapter_index << ")\n";
  }
  if (verify_adapter_only) {
    // Hardware discovery only: no bundle lock, directory/result/save writes,
    // host launch or retained D3D device (capability calls use nullptr).
    std::cout << "{\"schema\":\"mcla-portable-adapter-verification-v1\","
                 "\"name\":\"" << JsonEscape(ToUtf8(*adapter_name))
              << "\",\"d3d12_adapter\":" << *adapter_index
              << ",\"game_launched\":false}\n";
    return 0;
  }

  const auto root = ExecutableRoot();
  if (root.empty()) {
    return Fail("cannot resolve the bundle root");
  }
  if (HasReparseAttribute(root)) {
    return Fail("bundle root must not be a reparse point");
  }
  std::error_code ec;
  for (const auto *name : {L"user", L"cache", L"logs", L"diagnostics",
                           L"results", L"update", L"metadata"}) {
    std::filesystem::create_directories(root / name, ec);
    if (ec) {
      return Fail("cannot create a writable bundle directory: " + ec.message());
    }
  }

  std::string error;
  auto lock = AcquireLock(root, error);
  if (!lock) {
    return Fail(error);
  }
  if (!ValidateTree(root, error, !verify_only && !requested_profile_name)) {
    return Fail(error);
  }
  std::cout << "Verifying the portable MCLA-R bundle..." << std::endl;
  size_t verified_files = 0;
  if (!ParseAndVerifyManifest(root, verified_files, error)) {
    return Fail(error);
  }
  {
    const auto config_path = root / L"bin" / L"mcla.toml";
    const auto config_bytes = std::filesystem::file_size(config_path, ec);
    if (ec || config_bytes > 1024 * 1024) {
      return Fail("immutable GPU configuration missing or exceeds bound");
    }
    const auto config = ReadText(config_path);
    if (!config || !mcla::portable::ValidateExplicitGpuConfig(
                       *config, adapter_name.has_value(), error, true, gpu_backend.has_value())) {
      return Fail(config ? error : "cannot read immutable GPU configuration");
    }
  }
  const auto bundle_id = ReadBundleId(root);
  if (!bundle_id) {
    return Fail("bundle-id.txt is malformed");
  }

  const auto profile_root = root / L"user" / L"B13EBABEBABEBABE";
  const auto profile_name_path = profile_root / L"local-display-name.txt";
  std::string profile_name = "User";
  const bool has_profile_name = std::filesystem::exists(profile_name_path, ec);
  if (ec) {
    return argument_fail("cannot inspect stored profile name");
  }
  if (has_profile_name) {
    const auto bytes = std::filesystem::file_size(profile_name_path, ec);
    if (ec || bytes < 1 || bytes > 15) {
      return argument_fail("stored profile name is invalid; preserve it for recovery");
    }
    const auto stored = ReadText(profile_name_path);
    if (!stored || !mcla::portable::ValidLocalProfileName(*stored)) {
      return argument_fail("stored profile name is invalid; preserve it for recovery");
    }
    profile_name = *stored;
  }
  if (requested_profile_name) {
    // No guest, snapshots, recovery or main save mutation. The held bundle lock
    // excludes another launcher session. Preference belongs to archived XUID tree.
    if (*requested_profile_name != profile_name &&
        !RenameProfileWithReceipt(root, *bundle_id, verified_files, *requested_profile_name, error)) {
      return argument_fail(error);
    }
    std::cout << "Local profile name saved; XUID and save paths unchanged. No game launched.\n";
    return 0;
  }

  if (!verify_only) {
    if (!RecoverInterruptedSessions(root, *bundle_id, verified_files, error)) {
      return Fail(error);
    }
    if (!ValidateTree(root, error)) {
      return Fail(error);
    }
  }

  if (verify_only) {
    const auto path = root / L"results" /
                      FromUtf8("portable-verification-" + UtcStamp() + ".json");
    const std::string result =
        "{\n  \"schema\": \"mcla-portable-verification-v1\",\n  \"bundle_id\": "
        "\"" +
        JsonEscape(*bundle_id) + "\",\n  \"verified_utc\": \"" + UtcStamp() +
        "\",\n  \"verified_file_count\": " + std::to_string(verified_files) +
        ",\n  \"relocatable_root_verified\": true,\n  \"decision\": "
        "\"portable-bundle-integrity-pass\"\n}\n";
    if (!WriteTextAtomic(path, result)) {
      return Fail("cannot atomically publish the verification result");
    }
    std::cout << "MCLA portable bundle verified: " << *bundle_id << " ("
              << verified_files << " immutable files)\n";
    return 0;
  }

  if (!HasHistoryCapacity(root, error)) {
    // A normal, recoverable admission stop, also usable by unattended callers.
    std::cerr << "MCLA portable launcher: " << error << '\n';
    return 1;
  }
  const std::string session_id =
      "session-" + UtcStamp() + "-" + std::to_string(GetCurrentProcessId());
  const auto session_partial =
      root / L"results" / FromUtf8(session_id + ".partial");
  const auto session_completed = root / L"results" / FromUtf8(session_id);
  std::filesystem::create_directory(session_partial, ec);
  if (ec) {
    return Fail("cannot create the atomic session staging directory: " +
                ec.message());
  }
  const std::string started_utc = UtcStamp();
  const std::string started =
      "{\n  \"schema\": \"mcla-portable-session-v1\",\n  \"bundle_id\": \"" +
      JsonEscape(*bundle_id) + "\",\n  \"session_id\": \"" +
      JsonEscape(session_id) + "\",\n  \"started_utc\": \"" + started_utc +
      "\",\n  \"mode\": \"" +
      std::string(diagnostics_probe ? "diagnostics-probe" : "gameplay") +
      "\",\n  \"state\": \"running\"\n}\n";
  if (!WriteTextAtomic(session_partial / L"session.json", started)) {
    return Fail("cannot publish the running session manifest");
  }

  const auto executable = root / L"bin" / L"mcla.exe";
  std::vector<std::wstring> arguments = {
      Quote(executable.wstring()),
      Flag(L"--game_data_root", root / L"game"),
      Flag(L"--user_data_root", root / L"user"),
      Flag(L"--update_data_root", root / L"update"),
      Flag(L"--cache_root", root / L"cache"),
      Flag(L"--metadata_root", root / L"metadata"),
      L"--xam_user_signin_state=1",
      L"--input_backend=sdl",
      L"--mnk_mode=false"};
  if (gpu_backend) {
    arguments.push_back(L"--gpu_backend=" + *gpu_backend);
  }
  arguments.push_back(L"--local_user_display_name=" + Quote(FromUtf8(profile_name)));
  if (adapter_index) {
    arguments.push_back(L"--d3d12_adapter=" + std::to_wstring(*adapter_index));
  }
  if (diagnostics_probe) {
    arguments.push_back(L"--mcla_diagnostics_snapshot_probe=true");
    arguments.push_back(L"--mcla_crash_reporter_dialog=false");
  }
  std::wstring command;
  for (const auto &argument : arguments) {
    if (!command.empty()) {
      command.push_back(L' ');
    }
    command += argument;
  }
  std::optional<SaveState> last_save;
  size_t snapshots = 0;
  if (!CaptureSave(root / L"user", session_partial, "launcher-start", last_save,
                   snapshots, error)) {
    std::filesystem::remove_all(session_partial, ec);
    return Fail("cannot create the initial recoverable save snapshot: " +
                error);
  }
  std::cout << "Bundle verified; starting MCLA-R..." << std::endl;

  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
                      FALSE, 0, nullptr, root.c_str(), &startup, &process)) {
    const DWORD launch_error = GetLastError();
    std::filesystem::remove_all(session_partial, ec);
    return Fail("cannot start bin/mcla.exe (win32=" +
                std::to_string(launch_error) + ")");
  }
  Handle process_handle(process.hProcess);
  Handle thread_handle(process.hThread);

  Handle job(CreateJobObjectW(nullptr, nullptr));
  if (job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job.value, JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job.value, process_handle.value)) {
      job = Handle{};
    }
  }

  size_t save_watcher_errors = 0;
  for (;;) {
    const DWORD wait = WaitForSingleObject(process_handle.value, 15000);
    if (wait == WAIT_OBJECT_0) {
      break;
    }
    if (wait != WAIT_TIMEOUT) {
      return Fail("cannot monitor the game process");
    }
    if (!CaptureSave(root / L"user", session_partial, "changed-during-session",
                     last_save, snapshots, error)) {
      std::cerr << "Save watcher warning: " << error << "\n";
      ++save_watcher_errors;
      error.clear();
    }
  }
  if (!CaptureSave(root / L"user", session_partial, "process-exit", last_save,
                   snapshots, error)) {
    std::cerr << "Save watcher warning: " << error << "\n";
    ++save_watcher_errors;
    error.clear();
  }
  DWORD exit_code = 0;
  if (!GetExitCodeProcess(process_handle.value, &exit_code)) {
    return Fail("cannot read the game exit code");
  }
  job = Handle{};

  const auto final_save = ReadSaveState(root / L"user");
  const std::string result =
      "{\n  \"schema\": \"mcla-portable-session-result-v1\",\n  \"bundle_id\": "
      "\"" +
      JsonEscape(*bundle_id) + "\",\n  \"session_id\": \"" +
      JsonEscape(session_id) + "\",\n  \"started_utc\": \"" + started_utc +
      "\",\n  \"completed_utc\": \"" + UtcStamp() + "\",\n  \"mode\": \"" +
      std::string(diagnostics_probe ? "diagnostics-probe" : "gameplay") +
      "\",\n  \"exit_code\": " + std::to_string(exit_code) +
      ",\n  \"verified_file_count\": " + std::to_string(verified_files) +
      ",\n  \"save_snapshot_count\": " + std::to_string(snapshots) +
      ",\n  \"save_watcher_error_count\": " +
      std::to_string(save_watcher_errors) + ",\n  \"final_save_sha256\": " +
      (final_save ? "\"" + final_save->save_hash + "\"" : "null") +
      ",\n  \"final_header_sha256\": " +
      (final_save ? "\"" + final_save->header_hash + "\"" : "null") +
      ",\n  \"state\": \"complete\"\n}\n";
  if (!WriteTextAtomic(session_partial / L"result.json", result)) {
    return Fail("cannot atomically publish the session result");
  }
  if (!RenameDirectoryAtomic(session_partial, session_completed, error)) {
    return Fail("cannot atomically complete the session directory: " + error);
  }
  std::cout << "MCLA session complete: " << session_id << " (exit " << exit_code
            << ", save snapshots " << snapshots << ")\n";
  return exit_code == 0 ? 0 : static_cast<int>(exit_code);
}
