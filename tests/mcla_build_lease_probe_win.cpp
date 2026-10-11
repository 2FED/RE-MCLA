#include "mcla_build_lease_runtime_win.h"
#include "mcla_build_lease_network_win.h"
#include "mcla_build_lease_policy.h"
#include "mcla_build_lease_public_key.h"
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string_view>
// Explicit headless engineering probe. Never launches the game or touches saves.
int wmain(int argc,wchar_t **argv) {
  if(argc==3 && std::wstring_view(argv[1])==L"--verify-leases") {
    const std::filesystem::path path(argv[2]);
    auto root=mcla::launcher::fs::OpenDirectory(path.parent_path().wstring());
    if(!root) { std::wcerr<<root.error()<<L"\n"; return 2; }
    auto bytes=mcla::launcher::fs::ReadFile(*root,path.filename().wstring(),65536);
    if(!bytes) { std::wcerr<<bytes.error()<<L"\n"; return 2; }
    auto leases=mcla::lease::ParseLeases(std::string(bytes->begin(),bytes->end()),mcla::lease::kPublicKey);
    if(!leases) { std::wcerr<<leases.error()<<L"\n"; return 1; }
    std::cout<<"SIGNED_LEASES_VERIFIED entries="<<leases->grants.size()
             <<" issued_utc="<<leases->issued_utc<<" (local verification, not deployment)\n";
    return 0;
  }
  if(argc==2 && std::wstring_view(argv[1])==L"--network-smoke") {
    const auto start=GetTickCount64();
    auto result=mcla::lease::FetchPublicLeases();
    std::cout<<"elapsed_ms="<<(GetTickCount64()-start)<<" received="<<bool(result)<<"\n";
    if(!result) std::wcerr<<result.error()<<L"\n";
    else std::cout<<"bytes="<<result->size()<<" (transport only, not authenticated)\n";
    return result?0:1;
  }
  if(argc!=2) return 2;
  auto root=mcla::launcher::fs::OpenDirectory(argv[1]);
  if(!root) { std::wcerr<<root.error()<<L"\n"; return 2; }
  auto result=mcla::lease::CheckBuildLease(*root);
  if(!result) { std::wcerr<<result.error()<<L"\n"; return 1; }
  std::cout<<"SIGNED_BUILD_ADMITTED expires_utc="<<result->expires_utc<<" extended="<<result->extended<<"\n";
  return 0;
}
