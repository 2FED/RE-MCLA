#pragma once
#include "mcla_launcher_fs_win.h"
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace mcla::lease {
inline constexpr std::wstring_view kTicketFile=L"mcla-test-build.ticket";
struct Admission {
  uint64_t expires_utc=0;
  bool extended=false;
  std::wstring note;
};
// Admission only. Never starts/stops a process or reads/writes a save. The caller
// verifies/pins the host; an empty hash makes this function hash mcla.exe itself.
std::expected<Admission,std::wstring> CheckBuildLease(
    const launcher::fs::DirectoryGuard &build,std::string_view pinned_host_hash={},bool prefer_cached=false,
    bool ticket_only=false,bool read_only=false);
// read_only verifies existing cached/online signed grants without creating a
// cache directory/lock or publishing bytes. No change to normal host admission.
// ticket_only is stricter: no online/offline extensions and no cache discovery.
std::expected<Admission,std::wstring> CheckCurrentHostLease(bool prefer_cached=false,bool ticket_only=false);
#ifdef MCLA_BUILD_LEASE_RUNTIME_TESTING
// Explicit test seam: no network and no user-profile directory discovery.
std::expected<Admission,std::wstring> CheckLeaseForTest(
    std::string_view ticket,std::string_view hash,std::span<const uint8_t> key,
    uint64_t now,const launcher::fs::DirectoryGuard *cache,
    const std::function<std::expected<std::string,std::wstring>()> &fetch,
    bool prefer_cached=false,const std::function<uint64_t()> &after_clock={},bool read_only=false);
#endif
}
