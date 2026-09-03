// mcla - Windows out-of-process minidump and crash-package writer

#include "mcla_crash_ipc.h"

#include <DbgHelp.h>
#include <Windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <winternl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace mcla::diagnostics::native {
namespace {

constexpr uint64_t kMaxSaveBytes = 64ull * 1024 * 1024;
constexpr uint64_t kMaxSaveFileBytes = 16ull * 1024 * 1024;
constexpr uint32_t kMaxSaveEntries = 4096;
constexpr uint64_t kMaxCrashDumpBytes = 128ull * 1024 * 1024;
constexpr size_t kCrashRetentionCount = 5;
constexpr uint64_t kCrashRetentionBytes = 512ull * 1024 * 1024;
constexpr auto kPartialRetentionAge = std::chrono::hours(24);

HANDLE ParseHandle(std::wstring_view argument, std::wstring_view name) {
  const std::wstring prefix = std::wstring(name) + L"=";
  if (!argument.starts_with(prefix)) {
    return nullptr;
  }
  wchar_t *end = nullptr;
  const auto value = _wcstoui64(argument.data() + prefix.size(), &end, 10);
  if (!end || *end != L'\0') {
    return nullptr;
  }
  return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(value));
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
  temporary += ".tmp-" + std::to_string(GetCurrentProcessId());
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

bool IsReparsePoint(const std::filesystem::path &path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes == INVALID_FILE_ATTRIBUTES ||
         (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

std::string UtcStamp() {
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
  gmtime_s(&utc, &now);
  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y%m%dT%H%M%SZ");
  return stream.str();
}

std::string Hex32(uint32_t value) {
  std::ostringstream stream;
  stream << "0x" << std::hex << std::uppercase << std::setfill('0')
         << std::setw(8) << value;
  return stream.str();
}

std::string Hex64(uint64_t value) {
  std::ostringstream stream;
  stream << "0x" << std::hex << std::uppercase << std::setfill('0')
         << std::setw(16) << value;
  return stream.str();
}

std::string NarrowAscii(std::wstring_view value) {
  std::string result;
  result.reserve(value.size());
  for (const wchar_t character : value) {
    result.push_back(character <= 0x7F ? static_cast<char>(character) : '?');
  }
  return result;
}

std::string PathUtf8(const std::filesystem::path &path) {
  const auto value = path.generic_u8string();
  return std::string(reinterpret_cast<const char *>(value.data()),
                     value.size());
}

std::string JsonEscape(std::string_view value) {
  std::string result;
  result.reserve(value.size() + 16);
  for (const unsigned char character : value) {
    if (character == '\\' || character == '"') {
      result.push_back('\\');
      result.push_back(static_cast<char>(character));
    } else if (character == '\n') {
      result += "\\n";
    } else if (character == '\r') {
      result += "\\r";
    } else if (character == '\t') {
      result += "\\t";
    } else if (character >= 0x20) {
      result.push_back(static_cast<char>(character));
    }
  }
  return result;
}

bool WriteDump(HANDLE process, DWORD process_id,
               const std::filesystem::path &path, DWORD thread_id,
               uint64_t exception_pointers) {
  HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  MINIDUMP_EXCEPTION_INFORMATION exception_info{};
  MINIDUMP_EXCEPTION_INFORMATION *exception_info_ptr = nullptr;
  if (exception_pointers) {
    exception_info.ThreadId = thread_id;
    exception_info.ExceptionPointers =
        reinterpret_cast<EXCEPTION_POINTERS *>(exception_pointers);
    exception_info.ClientPointers = TRUE;
    exception_info_ptr = &exception_info;
  }
  const auto dump_type = static_cast<MINIDUMP_TYPE>(
      MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
  const BOOL written = MiniDumpWriteDump(process, process_id, file, dump_type,
                                         exception_info_ptr, nullptr, nullptr);
  FlushFileBuffers(file);
  CloseHandle(file);
  if (!written) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
  return written == TRUE;
}

bool CopySharedFile(const std::filesystem::path &source,
                    const std::filesystem::path &destination) {
  HANDLE input =
      CreateFileW(source.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (input == INVALID_HANDLE_VALUE) {
    return false;
  }
  HANDLE output = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (output == INVALID_HANDLE_VALUE) {
    CloseHandle(input);
    return false;
  }
  std::array<char, 64 * 1024> buffer{};
  bool ok = true;
  for (;;) {
    DWORD read = 0;
    if (!ReadFile(input, buffer.data(), static_cast<DWORD>(buffer.size()),
                  &read, nullptr)) {
      ok = false;
      break;
    }
    if (!read) {
      break;
    }
    DWORD offset = 0;
    while (offset < read) {
      DWORD written = 0;
      if (!WriteFile(output, buffer.data() + offset, read - offset, &written,
                     nullptr) ||
          !written) {
        ok = false;
        break;
      }
      offset += written;
    }
    if (!ok) {
      break;
    }
  }
  FlushFileBuffers(output);
  CloseHandle(output);
  CloseHandle(input);
  if (!ok) {
    std::error_code ec;
    std::filesystem::remove(destination, ec);
  }
  return ok;
}

struct SaveStats {
  struct File {
    std::string relative_path;
    uint64_t bytes = 0;
    std::string sha256;
  };

  uint64_t bytes = 0;
  uint32_t files = 0;
  uint32_t skipped = 0;
  bool found = false;
  std::vector<File> inventory;
};

std::string Sha256(const std::filesystem::path &path);

SaveStats CopySaveTrees(const std::filesystem::path &user_root,
                        const std::filesystem::path &destination_root) {
  SaveStats stats;
  uint32_t visited_entries = 0;
  std::error_code ec;
  for (const auto &profile :
       std::filesystem::directory_iterator(user_root, ec)) {
    if (++visited_entries > kMaxSaveEntries) {
      ++stats.skipped;
      break;
    }
    if (ec) {
      break;
    }
    if (!profile.is_directory(ec) || IsReparsePoint(profile.path())) {
      continue;
    }
    const auto title_root = profile.path() / "545407F8";
    if (!std::filesystem::is_directory(title_root, ec) ||
        IsReparsePoint(title_root)) {
      ec.clear();
      continue;
    }
    stats.found = true;
    for (std::filesystem::recursive_directory_iterator
             it(title_root,
                std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         it != end; it.increment(ec)) {
      if (++visited_entries > kMaxSaveEntries) {
        ++stats.skipped;
        break;
      }
      if (ec) {
        ec.clear();
        ++stats.skipped;
        continue;
      }
      if (IsReparsePoint(it->path())) {
        it.disable_recursion_pending();
        ++stats.skipped;
        continue;
      }
      const auto relative =
          std::filesystem::relative(it->path(), user_root, ec);
      if (ec || relative.empty()) {
        ec.clear();
        ++stats.skipped;
        continue;
      }
      const auto destination = destination_root / relative;
      if (it->is_directory(ec)) {
        std::filesystem::create_directories(destination, ec);
        continue;
      }
      if (!it->is_regular_file(ec)) {
        continue;
      }
      const auto bytes = it->file_size(ec);
      const auto write_time = it->last_write_time(ec);
      if (ec || bytes > kMaxSaveFileBytes ||
          stats.bytes + bytes > kMaxSaveBytes) {
        ec.clear();
        ++stats.skipped;
        continue;
      }
      std::filesystem::create_directories(destination.parent_path(), ec);
      bool stable = false;
      for (int attempt = 0; !ec && attempt < 3; ++attempt) {
        if (!CopySharedFile(it->path(), destination)) {
          continue;
        }
        const auto bytes_after = it->file_size(ec);
        const auto write_time_after = it->last_write_time(ec);
        if (!ec && bytes == bytes_after && write_time == write_time_after) {
          stable = true;
          break;
        }
        ec.clear();
      }
      if (stable) {
        const std::string hash = Sha256(destination);
        if (hash.empty()) {
          std::filesystem::remove(destination, ec);
          ec.clear();
          ++stats.skipped;
          continue;
        }
        stats.bytes += bytes;
        ++stats.files;
        stats.inventory.push_back({PathUtf8(relative), bytes, hash});
      } else {
        std::filesystem::remove(destination, ec);
        ec.clear();
        ++stats.skipped;
      }
    }
  }
  return stats;
}

std::string BuildSaveInventory(const SaveStats &save) {
  std::ostringstream out;
  out << "{\n"
      << "  \"schema\": \"mcla-private-save-inventory-v1\",\n"
      << "  \"safe_to_share\": false,\n"
      << "  \"files\": [\n";
  for (size_t i = 0; i < save.inventory.size(); ++i) {
    const auto &file = save.inventory[i];
    out << "    {\"path\": \"" << JsonEscape(file.relative_path)
        << "\", \"bytes\": " << file.bytes << ", \"sha256\": \"" << file.sha256
        << "\"}" << (i + 1 == save.inventory.size() ? "\n" : ",\n");
  }
  out << "  ]\n}\n";
  return out.str();
}

std::string Sha256(const std::filesystem::path &path) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD object_size = 0;
  DWORD result_size = 0;
  std::vector<UCHAR> object;
  std::array<UCHAR, 32> digest{};
  std::string result;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0 ||
      BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&object_size),
                        sizeof(object_size), &result_size, 0) < 0) {
    goto done;
  }
  object.resize(object_size);
  if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0,
                       0) < 0) {
    goto done;
  }
  {
    std::ifstream stream(path, std::ios::binary);
    std::array<char, 64 * 1024> buffer{};
    while (stream) {
      stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const auto count = stream.gcount();
      if (count > 0 &&
          BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                         static_cast<ULONG>(count), 0) < 0) {
        goto done;
      }
    }
    if (!stream.eof()) {
      goto done;
    }
  }
  if (BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()),
                       0) < 0) {
    goto done;
  }
  {
    std::ostringstream out;
    out << std::hex << std::uppercase << std::setfill('0');
    for (const UCHAR byte : digest) {
      out << std::setw(2) << static_cast<unsigned>(byte);
    }
    result = out.str();
  }
done:
  if (hash) {
    BCryptDestroyHash(hash);
  }
  if (algorithm) {
    BCryptCloseAlgorithmProvider(algorithm, 0);
  }
  return result;
}

uint64_t DirectoryBytes(const std::filesystem::path &root) {
  uint64_t bytes = 0;
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator
           it(root, std::filesystem::directory_options::skip_permission_denied,
              ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (IsReparsePoint(it->path())) {
      it.disable_recursion_pending();
    } else if (it->is_regular_file(ec)) {
      bytes += it->file_size(ec);
    }
  }
  return bytes;
}

void PruneCrashPackages(const std::filesystem::path &root) {
  struct Entry {
    std::filesystem::path path;
    std::filesystem::file_time_type time;
    uint64_t bytes;
  };
  std::vector<Entry> entries;
  std::error_code ec;
  for (const auto &item : std::filesystem::directory_iterator(root, ec)) {
    if (ec) {
      return;
    }
    if (!item.is_directory(ec) || IsReparsePoint(item.path()) ||
        item.path().filename().string().rfind("crash-", 0) != 0) {
      continue;
    }
    if (item.path().extension() == ".partial") {
      const auto write_time = item.last_write_time(ec);
      if (!ec && write_time < std::filesystem::file_time_type::clock::now() -
                                  kPartialRetentionAge) {
        std::filesystem::remove_all(item.path(), ec);
      }
      ec.clear();
      continue;
    }
    entries.push_back(
        {item.path(), item.last_write_time(ec), DirectoryBytes(item.path())});
  }
  std::sort(entries.begin(), entries.end(),
            [](const Entry &a, const Entry &b) { return a.time > b.time; });
  uint64_t retained = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    retained += entries[i].bytes;
    if (i == 0 ||
        (i < kCrashRetentionCount && retained <= kCrashRetentionBytes)) {
      continue;
    }
    std::filesystem::remove_all(entries[i].path, ec);
    ec.clear();
  }
}

std::string OsVersion() {
  using RtlGetVersionFn = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);
  auto *function = reinterpret_cast<RtlGetVersionFn>(
      GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
  RTL_OSVERSIONINFOW version{};
  version.dwOSVersionInfoSize = sizeof(version);
  if (!function || function(&version) != 0) {
    return "unknown";
  }
  return std::to_string(version.dwMajorVersion) + "." +
         std::to_string(version.dwMinorVersion) + "." +
         std::to_string(version.dwBuildNumber);
}

void CaptureCrashPackage(HANDLE parent, HANDLE dump_done,
                         CrashIpcState &state) {
  const auto crash_root =
      std::filesystem::path(state.diagnostics_root) / "crash";
  const std::string name =
      "crash-" + UtcStamp() + "-" + std::to_string(state.parent_process_id);
  const auto partial = crash_root / (name + ".partial");
  const auto complete = crash_root / name;
  std::error_code ec;
  std::filesystem::create_directories(crash_root, ec);
  if (!ec) {
    PruneCrashPackages(crash_root);
  }
  std::filesystem::remove_all(partial, ec);
  ec.clear();
  std::filesystem::create_directories(partial, ec);
  if (ec) {
    InterlockedExchange(&state.crash_result, -1);
    SetEvent(dump_done);
    return;
  }
  const auto dump_path = partial / "crash-private.dmp";
  bool dump_written =
      WriteDump(parent, state.parent_process_id, dump_path,
                state.crash_thread_id, state.exception_pointers);
  if (dump_written) {
    const auto dump_bytes = std::filesystem::file_size(dump_path, ec);
    if (ec || dump_bytes > kMaxCrashDumpBytes) {
      dump_written = false;
      ec.clear();
      std::filesystem::remove(dump_path, ec);
      ec.clear();
    }
  }
  MemoryBarrier();
  SetEvent(dump_done);
  const bool journal_written = CopySharedFile(
      state.journal_path, partial / "runtime-journal-private.log");
  const SaveStats save =
      CopySaveTrees(state.user_data_root, partial / "save-private");
  const bool save_inventory_written =
      WriteText(partial / "save-files-private.json", BuildSaveInventory(save));
  const bool readme_written = WriteText(
      partial / "README.txt",
      "MCLA-R native crash package. Send only this local folder path to a "
      "developer unless they explicitly request individual files.\r\n"
      "Do not upload the complete folder publicly without reviewing it.\r\n"
      "The minidump, runtime journal, and save snapshot are private and may "
      "contain local paths or gameplay/profile data. Nothing is uploaded "
      "automatically.\r\n");
  if (!save_inventory_written || !readme_written) {
    InterlockedExchange(&state.crash_result, -1);
    return;
  }

  uint64_t dump_bytes = 0;
  uint64_t journal_bytes = 0;
  uint64_t readme_bytes = 0;
  uint64_t save_inventory_bytes = 0;
  if (dump_written) {
    dump_bytes = std::filesystem::file_size(dump_path, ec);
    ec.clear();
  }
  if (journal_written) {
    journal_bytes =
        std::filesystem::file_size(partial / "runtime-journal-private.log", ec);
    ec.clear();
  }
  readme_bytes = std::filesystem::file_size(partial / "README.txt", ec);
  if (ec) {
    InterlockedExchange(&state.crash_result, -1);
    return;
  }
  save_inventory_bytes =
      std::filesystem::file_size(partial / "save-files-private.json", ec);
  if (ec) {
    InterlockedExchange(&state.crash_result, -1);
    return;
  }
  const std::string dump_hash = dump_written ? Sha256(dump_path) : "";
  const std::string journal_hash =
      journal_written ? Sha256(partial / "runtime-journal-private.log") : "";
  const std::string readme_hash = Sha256(partial / "README.txt");
  const std::string save_inventory_hash =
      Sha256(partial / "save-files-private.json");
  if ((dump_written && dump_hash.empty()) ||
      (journal_written && journal_hash.empty()) || readme_hash.empty() ||
      save_inventory_hash.empty()) {
    InterlockedExchange(&state.crash_result, -1);
    return;
  }
  std::ostringstream manifest;
  manifest
      << "{\n"
      << "  \"schema\": \"mcla-native-crash-package-v1\",\n"
      << "  \"kind\": \"native-crash\",\n"
      << "  \"created_utc\": \"" << UtcStamp() << "\",\n"
      << "  \"mcla_version\": \"" << NarrowAscii(state.app_version) << "\",\n"
      << "  \"platform\": \"windows\",\n"
#if defined(_M_ARM64)
      << "  \"architecture\": \"arm64\",\n"
#else
      << "  \"architecture\": \"x64\",\n"
#endif
      << "  \"os_version\": \"" << OsVersion() << "\",\n"
      << "  \"process_id\": " << state.parent_process_id << ",\n"
      << "  \"thread_id\": " << state.crash_thread_id << ",\n"
      << "  \"exception_code\": \"" << Hex32(state.exception_code) << "\",\n"
      << "  \"exception_address\": \"" << Hex64(state.exception_address)
      << "\",\n"
      << "  \"privacy\": {\"automatic_upload\": false, "
         "\"package_safe_to_share\": false, \"private_artifacts\": "
         "[\"crash-private.dmp\", \"runtime-journal-private.log\", "
         "\"README.txt\", \"save-files-private.json\", "
         "\"save-private\"]},\n"
      << "  \"capture\": {\"minidump\": " << (dump_written ? "true" : "false")
      << ", \"journal\": " << (journal_written ? "true" : "false")
      << ", \"save_found\": " << (save.found ? "true" : "false")
      << ", \"save_files\": " << save.files
      << ", \"save_bytes\": " << save.bytes
      << ", \"save_skipped_files\": " << save.skipped
      << ", \"readme\": " << (readme_written ? "true" : "false") << "},\n"
      << "  \"artifacts\": {\n"
      << "    \"crash-private.dmp\": {\"bytes\": " << dump_bytes
      << ", \"sha256\": \"" << dump_hash << "\", \"safe_to_share\": false},\n"
      << "    \"runtime-journal-private.log\": {\"bytes\": " << journal_bytes
      << ", \"sha256\": \"" << journal_hash
      << "\", \"safe_to_share\": false},\n"
      << "    \"README.txt\": {\"bytes\": " << readme_bytes
      << ", \"sha256\": \"" << readme_hash << "\", \"safe_to_share\": false},\n"
      << "    \"save-files-private.json\": {\"bytes\": " << save_inventory_bytes
      << ", \"sha256\": \"" << save_inventory_hash
      << "\", \"safe_to_share\": false}\n"
      << "  }\n"
      << "}\n";
  if (!WriteText(partial / "manifest.json", manifest.str())) {
    std::filesystem::remove_all(partial, ec);
    InterlockedExchange(&state.crash_result, -1);
    return;
  }
  std::filesystem::rename(partial, complete, ec);
  bool pointer_written = false;
  if (!ec) {
    pointer_written = WriteTextAtomic(
        std::filesystem::path(state.diagnostics_root) / "latest-crash.txt",
        name + "\n");
    PruneCrashPackages(crash_root);
  }
  const bool package_complete = !ec && dump_written;
  const bool published = package_complete && pointer_written;
  InterlockedExchange(&state.crash_result, published ? 1 : -1);
  if (package_complete &&
      InterlockedCompareExchange(&state.show_reporter_dialog, 0, 0) != 0) {
    const std::wstring message =
        L"MCLA-R crashed, but a local diagnostic package was saved.\r\n\r\n" +
        complete.wstring() +
        L"\r\n\r\nNothing was uploaded automatically. The package is private. "
        L"Press Ctrl+C to copy this message and send the folder path first.";
    MessageBoxW(nullptr, message.c_str(), L"MCLA-R Crash Reporter",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
  }
}

} // namespace
} // namespace mcla::diagnostics::native

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  using namespace mcla::diagnostics::native;
  int argument_count = 0;
  wchar_t **arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
  if (!arguments) {
    return 2;
  }
  HANDLE parent = nullptr;
  HANDLE mapping = nullptr;
  HANDLE live_request = nullptr;
  HANDLE live_done = nullptr;
  HANDLE crash_request = nullptr;
  HANDLE crash_done = nullptr;
  HANDLE stop = nullptr;
  for (int i = 1; i < argument_count; ++i) {
    const std::wstring_view argument(arguments[i]);
    if (!parent)
      parent = ParseHandle(argument, L"--parent");
    if (!mapping)
      mapping = ParseHandle(argument, L"--mapping");
    if (!live_request)
      live_request = ParseHandle(argument, L"--live-request");
    if (!live_done)
      live_done = ParseHandle(argument, L"--live-done");
    if (!crash_request)
      crash_request = ParseHandle(argument, L"--crash-request");
    if (!crash_done)
      crash_done = ParseHandle(argument, L"--crash-done");
    if (!stop)
      stop = ParseHandle(argument, L"--stop");
  }
  LocalFree(arguments);
  if (!parent || !mapping || !live_request || !live_done || !crash_request ||
      !crash_done || !stop) {
    return 3;
  }
  auto *shared = static_cast<CrashIpcState *>(
      MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CrashIpcState)));
  if (!shared || shared->magic != kCrashIpcMagic ||
      shared->version != kCrashIpcVersion) {
    return 4;
  }

  HANDLE waits[] = {crash_request, live_request, stop, parent};
  int result = 0;
  for (;;) {
    const DWORD wait =
        WaitForMultipleObjects(std::size(waits), waits, FALSE, INFINITE);
    if (wait == WAIT_OBJECT_0) {
      CaptureCrashPackage(parent, crash_done, *shared);
      break;
    }
    if (wait == WAIT_OBJECT_0 + 1) {
      const LONG sequence =
          InterlockedCompareExchange(&shared->live_sequence, 0, 0);
      const std::filesystem::path dump_path(shared->live_dump_path);
      const bool written =
          WriteDump(parent, shared->parent_process_id, dump_path, 0, 0);
      InterlockedExchange(&shared->live_result, written ? 1 : -1);
      InterlockedExchange(&shared->live_completed_sequence, sequence);
      MemoryBarrier();
      SetEvent(live_done);
      continue;
    }
    if (wait == WAIT_OBJECT_0 + 2 || wait == WAIT_OBJECT_0 + 3) {
      break;
    }
    result = 5;
    break;
  }
  UnmapViewOfFile(shared);
  return result;
}
