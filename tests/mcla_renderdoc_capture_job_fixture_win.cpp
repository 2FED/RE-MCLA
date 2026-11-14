// No game, DLL, GPU, arbitrary command, profile, or existing-file mutation.
// Parent must be launched by the reviewed outer job supervisor. This fixture
// reports membership in SOME job; the supervisor proves exact outer ownership.
#include "mcla_launcher_fs_win.h"
#include "mcla_launcher_process_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using namespace mcla::launcher;
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
template<class T> T Take(std::expected<T, std::wstring> value) {
  if (!value) throw std::runtime_error("Production guard/process operation failed");
  return std::move(*value);
}
bool FixtureName(std::wstring_view name) {
  constexpr std::wstring_view prefix=L"mcla-capture-job-fixture-";
  if (!name.starts_with(prefix) || name.size()!=prefix.size()+32) return false;
  for (wchar_t c:name.substr(prefix.size()))
    if (!((c>=L'0' && c<=L'9') || (c>=L'a' && c<=L'f'))) return false;
  return true;
}
std::filesystem::path Root(std::wstring_view text) {
  std::filesystem::path path(text);
  Require(path.is_absolute() && FixtureName(path.filename().wstring()), "Exact absolute fixture root required");
  return path;
}
bool InJob() {
  BOOL in_job=FALSE;
  Require(IsProcessInJob(GetCurrentProcess(),nullptr,&in_job)!=FALSE,"Job membership query failed");
  return in_job!=FALSE;
}
std::filesystem::path Self() {
  std::array<wchar_t,32768> buffer{};
  const auto count=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
  Require(count && count<buffer.size(),"Exact self path unavailable");
  return std::filesystem::path(std::wstring(buffer.data(),count));
}
void WriteNew(const fs::DirectoryGuard& root, const wchar_t* name, const std::string& bytes) {
  Require(bytes.size()<=4096,"Fixed record budget exceeded");
  const auto path=std::filesystem::path(root.path())/name;
  HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,
      CREATE_NEW,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_WRITE_THROUGH,nullptr);
  Require(file!=INVALID_HANDLE_VALUE,"Fresh record creation failed");
  DWORD written=0;
  const bool okay=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)!=FALSE &&
      written==bytes.size() && FlushFileBuffers(file)!=FALSE;
  CloseHandle(file);
  Require(okay,"Fresh record write failed; preserved");
}
std::string Read(const fs::DirectoryGuard& root,const wchar_t* name) {
  auto bytes=Take(fs::ReadFile(root,name,4096));
  return {bytes.begin(),bytes.end()};
}
DWORD Pid(std::wstring_view text) {
  Require(!text.empty() && text.size()<=10 && text.front()!=L'0',"Canonical parent PID required");
  uint64_t n=0;
  for (wchar_t c:text) { Require(c>=L'0' && c<=L'9',"Invalid parent PID"); n=n*10+(c-L'0'); }
  Require(n && n<=MAXDWORD,"Parent PID out of range");
  return static_cast<DWORD>(n);
}
std::string ParentRecord(DWORD pid,bool hold) {
  return "MCLA-CAPTURE-JOB-FIXTURE-V1\nparent_pid "+std::to_string(pid)+
      "\nmode "+(hold?"hold":"normal")+"\nparent_in_any_job 1\n";
}
int Child(const std::filesystem::path& path,bool hold,DWORD parent_pid) {
  auto root=Take(fs::OpenDirectory(path.wstring()));
  Require(Read(root,L"parent-ready.txt")==ParentRecord(parent_pid,hold),"Owned parent record mismatch");
  Require(InJob(),"Nested child has no job membership");
  WriteNew(root,L"child-ready.txt","MCLA-CAPTURE-JOB-CHILD-V1\nchild_pid "+
      std::to_string(GetCurrentProcessId())+"\nchild_in_any_job 1\nmode "+(hold?"hold":"normal")+"\n");
  if (hold) {
    // Intentional bounded wait only; no polling of arbitrary commands/files.
    Sleep(10000);
    WriteNew(root,L"hold-expired.txt","HOLD_EXPIRED_WITHOUT_OUTER_TERMINATION\n");
    return 2;
  }
  // One fixed no-game handshake, never an arbitrary command: the outer control
  // opens held PID handles and verifies its exact job before releasing normal
  // completion. Missing/malformed release fails within five seconds.
  bool released=false;
  const auto started=GetTickCount64();
  while (GetTickCount64()-started<5000) {
    auto release=fs::ReadFile(root,L"normal-release.request",64);
    if (release) {
      Require(std::string(release->begin(),release->end())=="RELEASE_NORMAL_CHILD\n","Invalid fixed release token");
      released=true;
      break;
    }
    Sleep(10);
  }
  Require(released,"Normal child release deadline exceeded");
  WriteNew(root,L"child-complete.txt","NORMAL_CHILD_EXIT_REQUESTED\n");
  return 0;
}
int Parent(const std::filesystem::path& path,bool hold) {
  Require(InJob(),"Parent requires an outer job; no standalone launch");
  auto parent=Take(fs::OpenDirectory(path.parent_path().wstring()));
  const auto exact=std::filesystem::path(parent.path())/path.filename();
  Require(CreateDirectoryW(exact.c_str(),nullptr)!=FALSE,"Fixture root must be new; never adopt");
  auto root=Take(fs::OpenDirectory(exact.wstring()));
  auto executable=Self();
  auto executable_parent=Take(fs::OpenDirectory(executable.parent_path().wstring()));
  const auto exact_executable=std::filesystem::path(executable_parent.path())/executable.filename();
  WriteNew(root,L"parent-ready.txt",ParentRecord(GetCurrentProcessId(),hold));
  auto process=Take(StartOwnedProcess(exact_executable,
      {hold?L"--child-hold":L"--child-normal",root.path(),std::to_wstring(GetCurrentProcessId())},root.path()));
  const auto expected="MCLA-CAPTURE-JOB-CHILD-V1\nchild_pid "+std::to_string(process.ProcessId())+
      "\nchild_in_any_job 1\nmode "+(hold?"hold":"normal")+"\n";
  bool ready=false;
  const auto started=GetTickCount64();
  while (GetTickCount64()-started<12000) {
    if (!ready) {
      auto record=fs::ReadFile(root,L"child-ready.txt",4096);
      if (record && std::string(record->begin(),record->end())==expected) {
        ready=true;
        WriteNew(root,L"nested-ready.txt","MCLA-CAPTURE-NESTED-READY-V1\nparent_pid "+
            std::to_string(GetCurrentProcessId())+"\nchild_pid "+std::to_string(process.ProcessId())+
            "\nparent_in_any_job 1\nchild_in_any_job 1\n");
      }
    }
    const auto status=Take(process.Poll());
    if (status.exit_code && status.active_job_processes==0) {
      Require(ready,"Child stopped before validated readiness");
      Require(!hold && *status.exit_code==0,"Hold expired or child failed; no forced-cleanup proof");
      Require(Read(root,L"child-complete.txt")=="NORMAL_CHILD_EXIT_REQUESTED\n","Child completion record missing");
      WriteNew(root,L"normal-complete.txt","MCLA-CAPTURE-NESTED-COMPLETE-V1\nchild_exit 0\ninner_active_processes 0\n");
      return 0;
    }
    Sleep(10);
  }
  throw std::runtime_error("Bounded parent fixture deadline exceeded; own inner job cleanup only");
}
} // namespace

int wmain(int argc,wchar_t** argv) {
  try {
    Require(argc==3 || argc==4,"Use --normal|--hold NEWROOT; child modes are internal");
    const std::wstring_view mode=argv[1];
    const auto root=Root(argv[2]);
    if (argc==3 && (mode==L"--normal" || mode==L"--hold")) return Parent(root,mode==L"--hold");
    if (argc==4 && (mode==L"--child-normal" || mode==L"--child-hold"))
      return Child(root,mode==L"--child-hold",Pid(argv[3]));
    throw std::runtime_error("Unknown fixed fixture mode");
  } catch (const std::exception& error) {
    std::cerr<<"CAPTURE_JOB_FIXTURE_FAILED: "<<error.what()<<'\n';
    return 1;
  }
}
