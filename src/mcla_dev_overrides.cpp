// Owner-internal developer overrides (M10 l57). See mcla_dev_overrides.h.

#include "mcla_dev_overrides.h"

#include "mcla_logging.h"

#include <rex/cvar.h>

#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>

#ifndef MCLA_DEV_OVERRIDES
#define MCLA_DEV_OVERRIDES 0
#endif

#if defined(_WIN32) && MCLA_DEV_OVERRIDES
#include <windows.h>
#include <shlobj.h>
#endif

namespace mcla::dev_overrides {
namespace {

#if MCLA_DEV_OVERRIDES

// Written on the main thread by OnStartup before any reader runs.
Parsed g_parsed;
std::array<std::optional<bool>, kKeyCount> g_applied{};
// The diagnostics manager's roots, for xma_dump_contexts_on (OnStartup).
std::filesystem::path g_user_data_root;
std::filesystem::path g_diagnostics_root;

// UTF-8 text of a path, for log lines.
std::string LogPath(const std::filesystem::path &path) {
  const std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

// KI-071 step 3: composes and creates the per-session XMA dump directory and
// returns the text for the SDK string cvar xma_dump_contexts (empty on
// failure, logged). The directory is made like the diagnostics manager's
// folders (create_directories under the same root, inheriting its ACL); an
// existing reparse point there is refused. The cvar text is path::string(),
// the inverse of the SDK's std::filesystem::path(std::string).
std::string PrepareXmaDumpDirectory(rex::LogCategoryId category) {
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
#if defined(_WIN32)
  const bool have_time = gmtime_s(&utc, &now) == 0;
#else
  const bool have_time = gmtime_r(&now, &utc) != nullptr;
#endif
  const std::string stamp =
      have_time ? FormatDumpStamp(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                                  utc.tm_min, utc.tm_sec)
                : std::string();
  std::error_code ec;
  std::filesystem::path dir = XmaDumpDirectory(g_user_data_root, g_diagnostics_root, stamp);
  if (dir.empty()) {
    REXLOG_CAT_ERROR(category, "MCLA dev overrides: no XMA dump directory (no root or clock); "
                               "xma_dump_contexts_on=true ignored");
    return {};
  }
  dir = std::filesystem::absolute(dir, ec).lexically_normal();
  if (!ec) {
    std::filesystem::create_directories(dir, ec);
  }
  const std::filesystem::file_status status =
      ec ? std::filesystem::file_status{} : std::filesystem::symlink_status(dir, ec);
  if (ec || status.type() != std::filesystem::file_type::directory) {
    REXLOG_CAT_ERROR(category,
                     "MCLA dev overrides: could not create the XMA dump directory {} ({}); "
                     "xma_dump_contexts_on=true ignored",
                     LogPath(dir), ec ? ec.message() : std::string("not a plain directory"));
    return {};
  }
  try {
    return dir.string();
  } catch (const std::exception &) {
    REXLOG_CAT_ERROR(category,
                     "MCLA dev overrides: XMA dump directory {} is not representable in the "
                     "process code page; xma_dump_contexts_on=true ignored",
                     LogPath(dir));
    return {};
  }
}

#if defined(_WIN32)
// Reads the per-user override file. Only a regular file (no directory, no
// reparse point) of at most kMaxBytes is read.
Parsed ReadOverrideFile() {
  Parsed absent;
  PWSTR raw = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw))) {
    if (raw) {
      CoTaskMemFree(raw);
    }
    return absent;
  }
  std::wstring path(raw);
  CoTaskMemFree(raw);
  path += L"\\";
  path.append(kDirectory.begin(), kDirectory.end());
  path += L"\\";
  path.append(kFile.begin(), kFile.end());
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return absent;
  }
  Parsed parsed;
  parsed.status = FileStatus::kRejected;
  BY_HANDLE_FILE_INFORMATION info{};
  if (GetFileInformationByHandle(file, &info) &&
      !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
      info.nFileSizeHigh == 0 && info.nFileSizeLow <= kMaxBytes) {
    char buffer[kMaxBytes + 1] = {};
    DWORD read = 0;
    if (::ReadFile(file, buffer, DWORD(kMaxBytes + 1), &read, nullptr) && read <= kMaxBytes) {
      parsed = Parse(std::string_view(buffer, read));
    }
  }
  CloseHandle(file);
  return parsed;
}
#else
Parsed ReadOverrideFile() {
  return Parsed{};
}
#endif

void ApplyPhase(Phase phase) {
  const rex::LogCategoryId category =
      phase == Phase::kStartup ? ::mcla::logging::App() : ::mcla::logging::Gpu();
  for (size_t i = 0; i < kKeyCount; ++i) {
    const KeyInfo &info = kKeys[i];
    const std::optional<bool> value = g_parsed.values[i];
    if (info.phase != phase || !value) {
      continue;
    }
    const std::string key_name(info.name);
    const std::string name(CvarName(info));
    std::string text(CvarText(info, *value));
    if (text.empty()) {
      REXLOG_CAT_INFO(category, "MCLA dev overrides: {}={} leaves {} unchanged", key_name, *value,
                      name);
      continue;
    }
    const bool registered = rex::cvar::GetFlagInfo(name) != nullptr;
    const bool non_default = registered && rex::cvar::HasNonDefaultValue(name);
    switch (DecideApply(registered, non_default)) {
      case ApplyAction::kUnavailable:
        REXLOG_CAT_INFO(category, "MCLA dev overrides: {} unavailable in this build; {}={} ignored",
                        name, key_name, *value);
        break;
      case ApplyAction::kUserSet:
        REXLOG_CAT_INFO(category,
                        "MCLA dev overrides: {} set by the command line or config ({}); file "
                        "value {}={} ignored",
                        name, rex::cvar::GetFlagByName(name), key_name, *value);
        break;
      case ApplyAction::kApply:
        if (*value && info.true_source == TextSource::kXmaDumpDirectory) {
          text = PrepareXmaDumpDirectory(category);
          if (text.empty()) {
            break;
          }
        }
        if (rex::cvar::SetFlagByName(name, text)) {
          g_applied[i] = *value;
          if (key_name == name) {
            REXLOG_CAT_INFO(category, "MCLA dev override: {}={}", name, *value);
          } else {
            REXLOG_CAT_INFO(category, "MCLA dev override: {}={} ({}={})", key_name, *value, name,
                            text);
          }
        } else {
          REXLOG_CAT_ERROR(category, "MCLA dev overrides: could not set {}={}", name, text);
        }
        break;
    }
  }
}

#endif  // MCLA_DEV_OVERRIDES

}  // namespace

bool Enabled() {
  return MCLA_DEV_OVERRIDES != 0;
}

void OnStartup(const std::filesystem::path &user_data_root,
               const std::filesystem::path &diagnostics_root) {
#if MCLA_DEV_OVERRIDES
  g_user_data_root = user_data_root;
  g_diagnostics_root = diagnostics_root;
  g_parsed = ReadOverrideFile();
  switch (g_parsed.status) {
    case FileStatus::kAbsent:
      MCLA_APP_INFO("MCLA dev overrides: {} not present; build defaults", kFile);
      return;
    case FileStatus::kRejected:
      MCLA_APP_INFO("MCLA dev overrides: {} rejected (not a regular ASCII file of at most {} "
                    "bytes); nothing applied",
                    kFile, kMaxBytes);
      return;
    case FileStatus::kParsed:
      break;
  }
  MCLA_APP_INFO("MCLA dev overrides: {} read: {} value(s), {} line(s) ignored", kFile,
                g_parsed.ValueCount(), g_parsed.ignored_lines);
  for (size_t i = 0; i < g_parsed.issue_count; ++i) {
    MCLA_APP_INFO("MCLA dev overrides: line {} ignored: {}", g_parsed.issues[i].line,
                  LineIssueName(g_parsed.issues[i].issue));
  }
  ApplyPhase(Phase::kStartup);
#else
  (void)user_data_root;
  (void)diagnostics_root;
#endif
}

void OnGraphicsPluginLoaded() {
#if MCLA_DEV_OVERRIDES
  if (g_parsed.status != FileStatus::kParsed) {
    return;
  }
  ApplyPhase(Phase::kGraphicsPlugin);
#endif
}

std::optional<bool> AppliedValue(Key key) {
#if MCLA_DEV_OVERRIDES
  return g_applied[static_cast<size_t>(key)];
#else
  (void)key;
  return std::nullopt;
#endif
}

std::optional<bool> FileValue(Key key) {
#if MCLA_DEV_OVERRIDES
  return g_parsed.Value(key);
#else
  (void)key;
  return std::nullopt;
#endif
}

}  // namespace mcla::dev_overrides
