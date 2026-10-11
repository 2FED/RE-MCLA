#include "mcla_build_lease_runtime_win.h"
#include "mcla_build_lease_policy.h"
#include "mcla_build_lease_network_win.h"
#include "mcla_build_lease_public_key.h"
#include <windows.h>
#include <shlobj.h>
#include <array>
#include <filesystem>
#include <optional>

namespace mcla::lease {
namespace {
namespace fs=launcher::fs;
uint64_t Now() {
  FILETIME ft{}; GetSystemTimeAsFileTime(&ft);
  const uint64_t ticks=(uint64_t{ft.dwHighDateTime}<<32)|ft.dwLowDateTime;
  return ticks/10000000-11644473600ULL;
}
std::expected<fs::DirectoryGuard,std::wstring> CacheDirectory() {
  PWSTR raw=nullptr;
  if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_DEFAULT,nullptr,&raw)))
    return std::unexpected(L"Cannot locate local lease cache");
  const std::wstring base(raw); CoTaskMemFree(raw);
  auto parent=fs::OpenDirectory(base); if(!parent) return std::unexpected(parent.error());
  const auto child=parent->path()+L"\\MCLA-R-TestLeaseCache-v1";
  if(!CreateDirectoryW((L"\\\\?\\"+child).c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
    return std::unexpected(L"Cannot create local lease cache");
  return fs::OpenDirectory(child);
}
using Fetch=std::function<std::expected<std::string,std::wstring>()>;
std::expected<Admission,std::wstring> Check(std::string_view envelope,std::string_view hash,
    std::span<const uint8_t> key,uint64_t now,const fs::DirectoryGuard *cache,const Fetch &fetch,
    bool prefer_cached=false,const std::function<uint64_t()> &after_clock={}) {
  auto ticket=ParseTicket(envelope,key); if(!ticket) return std::unexpected(ticket.error());
  auto base=Evaluate(*ticket,hash,now,std::nullopt);
  // Only expiration can be remedied by a lease. Do not contact the server on
  // a different executable, an invalid clock or a future-dated signed ticket.
  if(ticket->host_sha256!=hash || now<ticket->created_utc || now<1577836800 || now>4102444800ULL)
    return std::unexpected(base ? L"Invalid test-build identity or clock" : base.error());
  const uint64_t built_in=ticket->created_utc+1209600;
  uint64_t best=built_in;
  const std::wstring filename=std::wstring(hash.begin(),hash.end())+L".lease";
  std::wstring note;
  std::optional<fs::WriterLock> lock;
  if(cache) {
    auto held=fs::AcquireWriterLock(*cache);
    if(held) lock.emplace(std::move(*held));
    else note=L"Lease cache busy/unavailable; online proof can still authorize this launch. ";
  }
  if(lock) {
    auto bytes=fs::ReadFile(*cache,filename,65536);
    if(bytes) {
      const std::string cached(bytes->begin(),bytes->end());
      auto leases=ParseLeases(cached,key);
      if(leases) {
        auto valid=Evaluate(*ticket,hash,now,*leases);
        if(valid && *valid>best) best=*valid;
      }
    }
  }
  // Managed host re-verifies its ticket/cache after the launcher checked online.
  // This never admits an expired unsigned build; if the launcher could not cache
  // its extension, the host still fetches and verifies independently.
  auto downloaded=(prefer_cached && now<best) ?
      std::expected<std::string,std::wstring>(std::unexpected(L"Cached admission sufficient")) : fetch();
  if(downloaded) {
    auto leases=ParseLeases(*downloaded,key);
    auto valid=leases ? Evaluate(*ticket,hash,now,*leases) : std::expected<uint64_t,std::wstring>(std::unexpected(leases.error()));
    if(valid && *valid>best) {
      best=*valid;
      if(lock) {
        const std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(downloaded->data()),downloaded->size());
        auto saved=fs::AtomicWrite(*cache,filename,bytes);
        if(!saved) note+=L"Signed extension valid for this launch; offline cache write failed. ";
      } else note+=L"Signed extension valid for this launch; offline cache unavailable. ";
    } else if(!leases) note+=L"Untrusted online response ignored. ";
  } else note+=L"Online extension unavailable; checking local allowance. ";
  const auto completed=after_clock?after_clock():now;
  if(completed<now || completed>4102444800ULL)
    return std::unexpected(L"System UTC clock changed backwards or is invalid; launch stopped before save initialization.");
  if(completed>=best) return std::unexpected(L"BUILD EXPIRED - GET THE NEW ONE. Your saves are unchanged. Connect to check for a signed extension or obtain a new test build.");
  return Admission{best,best>built_in,std::move(note)};
}
}
std::expected<Admission,std::wstring> CheckBuildLease(const launcher::fs::DirectoryGuard &build,std::string_view pinned_host_hash,bool prefer_cached,bool ticket_only) {
  auto ticket=launcher::fs::ReadFile(build,kTicketFile,65536);
  if(!ticket) return std::unexpected(L"Test-build ticket missing or unsafe. Obtain a complete current test build; saves are unchanged.");
  const std::string envelope(ticket->begin(),ticket->end());
  auto parsed=ParseTicket(envelope,kPublicKey);
  if(!parsed) return std::unexpected(parsed.error());
  std::string hash(pinned_host_hash);
  if(hash.empty()) {
    auto host=launcher::fs::ReadFile(build,L"mcla.exe",64*1024*1024);
    if(!host) return std::unexpected(host.error());
    auto hashed=Sha256(*host); if(!hashed) return std::unexpected(hashed.error());
    hash=std::move(*hashed);
  }
  const auto now=Now();
  if(parsed->host_sha256!=hash || now<parsed->created_utc || now<1577836800 || now>4102444800ULL)
    return std::unexpected(L"Test-build identity or system UTC clock is invalid; launch stopped before save initialization.");
  if(ticket_only) return Check(envelope,hash,kPublicKey,now,nullptr,[] {
    return std::expected<std::string,std::wstring>(std::unexpected(L"Lifecycle probe: extensions disabled"));
  },true,Now);
  auto cache=CacheDirectory();
  return Check(envelope,hash,kPublicKey,now,cache?&*cache:nullptr,FetchPublicLeases,prefer_cached,Now);
}
std::expected<Admission,std::wstring> CheckCurrentHostLease(bool prefer_cached,bool ticket_only) {
  std::array<wchar_t,32768> name{};
  const DWORD size=GetModuleFileNameW(nullptr,name.data(),static_cast<DWORD>(name.size()));
  if(!size || size>=name.size()) return std::unexpected(L"Cannot locate test-build executable");
  const std::filesystem::path module(std::wstring(name.data(),size));
  if(_wcsicmp(module.filename().c_str(),L"mcla.exe")!=0)
    return std::unexpected(L"Test-build host must keep its original mcla.exe filename");
  auto root=launcher::fs::OpenDirectory(module.parent_path().wstring());
  if(!root) return std::unexpected(root.error());
  return CheckBuildLease(*root,{},prefer_cached,ticket_only);
}
#ifdef MCLA_BUILD_LEASE_RUNTIME_TESTING
std::expected<Admission,std::wstring> CheckLeaseForTest(std::string_view ticket,std::string_view hash,
    std::span<const uint8_t> key,uint64_t now,const launcher::fs::DirectoryGuard *cache,const Fetch &fetch,
    bool prefer_cached,const std::function<uint64_t()> &after_clock) {
  return Check(ticket,hash,key,now,cache,fetch,prefer_cached,after_clock);
}
#endif
}
