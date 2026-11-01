// KI-047 fix candidate host side. See mcla_readback_sync.h.

#include "mcla_readback_sync.h"

#include "mcla_logging.h"

#include <rex/cvar.h>

#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#endif

#ifndef MCLA_READBACK_SYNC_COMPOSITE_DEFAULT
#define MCLA_READBACK_SYNC_COMPOSITE_DEFAULT 0
#endif

REXCVAR_DEFINE_BOOL(mcla_readback_sync_composite, MCLA_READBACK_SYNC_COMPOSITE_DEFAULT != 0,
    "MCLA",
    "KI-047: copy livery-composite read-back resolves synchronously "
    "(d3d12_readback_sync_composite). Only builds with "
    "MCLA_READBACK_SYNC_COMPOSITE_DEFAULT act on it; such builds also read a "
    "per-user override file for the owner A/B")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace mcla::readback_sync {
namespace {

#if defined(_WIN32) && MCLA_READBACK_SYNC_COMPOSITE_DEFAULT
// Reads the per-user override file. Only a regular file (no directory, no
// reparse point) of at most kOverrideMaxBytes is read.
OverrideStatus ReadOverrideFile() {
  PWSTR raw = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw))) {
    if (raw) {
      CoTaskMemFree(raw);
    }
    return OverrideStatus::kAbsent;
  }
  std::wstring path(raw);
  CoTaskMemFree(raw);
  path += L"\\";
  path.append(kOverrideDirectory.begin(), kOverrideDirectory.end());
  path += L"\\";
  path.append(kOverrideFile.begin(), kOverrideFile.end());
  HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return OverrideStatus::kAbsent;
  }
  OverrideStatus status = OverrideStatus::kInvalid;
  BY_HANDLE_FILE_INFORMATION info{};
  if (GetFileInformationByHandle(file, &info) &&
      !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
      info.nFileSizeHigh == 0 && info.nFileSizeLow <= kOverrideMaxBytes) {
    char buffer[kOverrideMaxBytes + 1] = {};
    DWORD read = 0;
    if (ReadFile(file, buffer, DWORD(kOverrideMaxBytes + 1), &read, nullptr) &&
        read <= kOverrideMaxBytes) {
      status = ParseOverride(std::string_view(buffer, read));
    }
  }
  CloseHandle(file);
  return status;
}
#else
OverrideStatus ReadOverrideFile() {
  return OverrideStatus::kAbsent;
}
#endif

}  // namespace

void OnGraphicsPluginLoaded() {
  constexpr bool option_on = MCLA_READBACK_SYNC_COMPOSITE_DEFAULT != 0;
  if (!option_on) {
    return;
  }
  const std::string sdk_cvar(kSdkCvar);
  const std::string host_cvar(kHostCvar);
  const bool sdk_has_cvar = rex::cvar::GetFlagInfo(sdk_cvar) != nullptr;
  const bool sdk_user_set = sdk_has_cvar && rex::cvar::HasNonDefaultValue(sdk_cvar);
  const bool host_value = REXCVAR_GET(mcla_readback_sync_composite);
  const bool host_user_set = rex::cvar::HasNonDefaultValue(host_cvar);
  const OverrideStatus file = ReadOverrideFile();
  if (file != OverrideStatus::kAbsent) {
    MCLA_GPU_INFO("MCLA readback sync: override file {} found: {}{}", kOverrideFile,
                  OverrideStatusName(file),
                  file == OverrideStatus::kInvalid
                      ? " (ignored; expected one line mcla_readback_sync_composite=false or =true)"
                      : (host_user_set ? " (ignored; the command line sets the switch)"
                                       : " (applied)"));
  }
  const bool effective = EffectiveHostValue(host_value, host_user_set, file);
  switch (DecideStart(option_on, sdk_has_cvar, sdk_user_set, effective)) {
    case StartAction::kDisabledByOption:
      break;
    case StartAction::kUnavailable:
      MCLA_GPU_INFO("MCLA readback sync: {} unavailable in this SDK", sdk_cvar);
      break;
    case StartAction::kSdkUserSet:
      MCLA_GPU_INFO("MCLA readback sync: {} set by the user ({}), left alone", sdk_cvar,
                    rex::cvar::Query<bool>(sdk_cvar));
      break;
    case StartAction::kOffByHost:
      MCLA_GPU_INFO("MCLA readback sync: fix OFF ({}=false{})", host_cvar,
                    host_user_set ? ", command line" : ", override file");
      break;
    case StartAction::kEnable:
      if (rex::cvar::SetFlagByName(sdk_cvar, "true")) {
        MCLA_GPU_INFO("MCLA readback sync: fix ON, build default {}=true", sdk_cvar);
      } else {
        MCLA_GPU_ERROR("MCLA readback sync: could not set {}=true", sdk_cvar);
      }
      break;
  }
}

}  // namespace mcla::readback_sync
