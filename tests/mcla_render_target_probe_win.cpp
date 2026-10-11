#include "mcla_launcher_build_win.h"
#include "mcla_launcher_game_win.h"
#include "mcla_launcher_process_win.h"
#include "mcla_managed_config_policy.h"
#include "mcla_build_lease_runtime_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(MCLA_LAUNCHER_GAME_TESTING) || defined(MCLA_LAUNCHER_CONTROLLER_TESTING)
#error The passive render probe must use production identity admission.
#endif

namespace {
using namespace mcla::launcher;
constexpr std::string_view kHost =
    "9195117A71A69F42B505838447C350EB8B0A6A2CC9FA14C535DCB0630240AAEE";
constexpr std::string_view kMarker = "MCLA graphics: nontrivial guest frame captured ";
struct Target { uint32_t width, height, guest_width, guest_height, scale; };
std::optional<Target> TargetFor(std::wstring_view value) {
  if (value == L"720") return Target{1280,720,1280,720,1};
  if (value == L"1080") return Target{1920,1080,960,540,2};
  if (value == L"1440") return Target{2560,1440,1280,720,2};
  if (value == L"2160") return Target{3840,2160,1280,720,3};
  return {};
}
struct Options { std::wstring build, game, gpu, target; bool run=false, self_test=false; };
std::optional<Options> Parse(std::span<const std::wstring_view> arguments) {
  Options out;
  if (arguments.size()==1 && arguments[0]==L"--self-test") { out.self_test=true; return out; }
  std::map<std::wstring_view,std::wstring_view> values;
  for (auto arg:arguments) {
    if (arg==L"--run" && !out.run) { out.run=true; continue; }
    const auto equal=arg.find(L'=');
    if (!arg.starts_with(L"--") || equal==arg.npos || equal+1==arg.size()) return {};
    const auto key=arg.substr(2,equal-2), value=arg.substr(equal+1);
    if ((key!=L"build" && key!=L"game" && key!=L"gpu" && key!=L"target") ||
        !values.emplace(key,value).second || value.find_first_of(L"\r\n")!=value.npos) return {};
  }
  if (!out.run || values.size()!=4 || !TargetFor(values[L"target"])) return {};
  out.build=values[L"build"]; out.game=values[L"game"]; out.gpu=values[L"gpu"]; out.target=values[L"target"];
  if (!NormalizeRoot(std::filesystem::path(out.build).u16string()) ||
      !NormalizeRoot(std::filesystem::path(out.game).u16string()) || out.gpu.size()>200) return {};
  return out;
}
uint32_t U32(std::span<const uint8_t> bytes,size_t at) {
  return uint32_t(bytes[at]) | uint32_t(bytes[at+1])<<8 |
      uint32_t(bytes[at+2])<<16 | uint32_t(bytes[at+3])<<24;
}
// Accept only the exact uncompressed, positive-height BGRA layout written by
// this pinned host's WriteFrameBmp. Header validation never allocates pixels.
std::optional<std::pair<uint32_t,uint32_t>> BmpShape(std::span<const uint8_t> h,uint64_t size) {
  if (h.size()!=54 || h[0]!='B' || h[1]!='M' || U32(h,6)!=0 || U32(h,10)!=54 ||
      U32(h,14)!=40 || h[26]!=1 || h[27]!=0 || h[28]!=32 || h[29]!=0 || U32(h,30)!=0) return {};
  const auto width=U32(h,18), height=U32(h,22);
  if (!width || width>8192 || !height || height>8192) return {};
  const uint64_t pixels=uint64_t(width)*height*4;
  if (size!=54+pixels || U32(h,2)!=size || U32(h,34)!=pixels) return {};
  return std::pair{width,height};
}
std::optional<std::pair<uint32_t,uint32_t>> ReadBmp(const std::filesystem::path &path) {
  std::error_code error;
  const auto size=std::filesystem::file_size(path,error);
  if (error) return {};
  std::array<uint8_t,54> header{};
  std::ifstream stream(path,std::ios::binary);
  if (!stream.read(reinterpret_cast<char *>(header.data()),header.size())) return {};
  return BmpShape(header,size);
}
std::optional<std::string> ReadLog(const std::filesystem::path &path) {
  std::error_code error;
  const auto size=std::filesystem::file_size(path,error);
  if (error || size>16*1024*1024) return {};
  std::string result(static_cast<size_t>(size),'\0');
  std::ifstream stream(path,std::ios::binary);
  if (!stream.read(result.data(),static_cast<std::streamsize>(size))) return {};
  return result;
}
bool MarkerMatches(std::string_view log,uint32_t width,uint32_t height) {
  const auto expected=std::string(kMarker)+std::to_string(width)+"x"+std::to_string(height)+", rgb555 bins ";
  return log.find(expected)!=log.npos;
}
std::expected<void,std::wstring> NoGameRunning() {
  const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
  if (snapshot==INVALID_HANDLE_VALUE) return std::unexpected(L"Cannot inspect running processes");
  PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry);
  if (!Process32FirstW(snapshot,&entry)) { CloseHandle(snapshot); return std::unexpected(L"Cannot enumerate processes"); }
  bool found=false;
  do { if (_wcsicmp(entry.szExeFile,L"mcla.exe")==0) found=true; } while (Process32NextW(snapshot,&entry));
  const auto error=GetLastError(); CloseHandle(snapshot);
  if (error!=ERROR_NO_MORE_FILES) return std::unexpected(L"Process inventory was incomplete");
  if (found) return std::unexpected(L"BLOCKED: mcla.exe is already running; no process was touched");
  return {};
}
std::expected<unsigned,std::wstring> Adapter(std::wstring_view name) {
  using Microsoft::WRL::ComPtr;
  ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return std::unexpected(L"Cannot enumerate GPU");
  std::optional<unsigned> found;
  for (unsigned i=0;;++i) {
    ComPtr<IDXGIAdapter1> adapter;
    const auto result=factory->EnumAdapters1(i,&adapter);
    if (result==DXGI_ERROR_NOT_FOUND) break;
    if (FAILED(result)) return std::unexpected(L"GPU enumeration failed");
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc))) return std::unexpected(L"GPU description failed");
    if (name!=desc.Description || (desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
    if (FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),nullptr)))
      return std::unexpected(L"Selected GPU does not support D3D12");
    if (found) return std::unexpected(L"GPU name is ambiguous");
    found=i;
  }
  if (!found) return std::unexpected(L"Exact requested GPU was not found");
  return *found;
}
struct Windows { DWORD pid; std::vector<HWND> windows; };
BOOL CALLBACK FindWindow(HWND window,LPARAM param) {
  auto &result=*reinterpret_cast<Windows *>(param);
  DWORD pid=0; GetWindowThreadProcessId(window,&pid);
  if (pid==result.pid && IsWindowVisible(window) && !GetWindow(window,GW_OWNER)) result.windows.push_back(window);
  return TRUE;
}
bool CloseOwnedWindow(DWORD pid) {
  Windows windows{pid,{}};
  if (!EnumWindows(FindWindow,reinterpret_cast<LPARAM>(&windows)) || windows.windows.size()!=1) return false;
  DWORD current=0; GetWindowThreadProcessId(windows.windows[0],&current);
  return current==pid && PostMessageW(windows.windows[0],WM_CLOSE,0,0);
}
void Require(bool ok,const char *reason) { if (!ok) throw std::runtime_error(reason); }
template<class T> T Take(std::expected<T,std::wstring> value) {
  if (!value) { std::wcerr<<value.error()<<L'\n'; throw std::runtime_error("admission or owned operation failed"); }
  return std::move(*value);
}
void Take(std::expected<void,std::wstring> value) {
  if (!value) { std::wcerr<<value.error()<<L'\n'; throw std::runtime_error("admission or owned operation failed"); }
}
void Write(const fs::DirectoryGuard &root,std::wstring_view name,std::string_view text) {
  Take(fs::AtomicWrite(root,name,{reinterpret_cast<const uint8_t *>(text.data()),text.size()}));
}
std::wstring Flag(std::wstring_view key,std::wstring_view value) { return L"--"+std::wstring(key)+L"="+std::wstring(value); }
std::filesystem::path TempParent(std::filesystem::path path) {
  // temp_directory_path includes a trailing separator on Windows. Remove only
  // that empty final component; keep all real components for strict admission.
  if (path.filename().empty() && path!=path.root_path()) path=path.parent_path();
  return path;
}
std::vector<std::wstring> Arguments(const Options &o,const Target &t,const std::filesystem::path &root,unsigned adapter) {
  std::vector<std::wstring> args{
    Flag(L"game_data_root",o.game),Flag(L"user_data_root",(root/L"user").wstring()),
    Flag(L"cache_root",(root/L"cache").wstring()),Flag(L"update_data_root",(root/L"update").wstring()),
    Flag(L"metadata_root",(root/L"metadata").wstring()),Flag(L"mcla_diagnostics_root",(root/L"diagnostics").wstring()),
    Flag(L"log_file",(root/L"mcla.log").wstring()),Flag(L"mcla_managed_launch_config",(root/L"managed.toml").wstring()),
    Flag(L"d3d12_adapter",std::to_wstring(adapter)),Flag(L"video_mode_width",std::to_wstring(t.guest_width)),
    Flag(L"video_mode_height",std::to_wstring(t.guest_height)),Flag(L"resolution_scale",std::to_wstring(t.scale)),
    Flag(L"draw_resolution_scale_x",std::to_wstring(t.scale)),Flag(L"draw_resolution_scale_y",std::to_wstring(t.scale)),
    L"--resolution=",L"--video_mode_refresh_rate=60",L"--vsync=true",L"--fullscreen=false",
    L"--window_width=1280",L"--window_height=720",L"--gpu_backend=d3d12",L"--xam_user_signin_state=1",
    L"--local_user_display_name=RenderProbe",L"--input_backend=sdl",L"--mnk_mode=false",
    L"--mcla_first_frame_probe=true",L"--mcla_first_frame_settle_seconds=3",L"--log_level=info",
    L"--log_max_file_size_mb=16",L"--log_max_files=1",L"--mcla_crash_reporter_dialog=false"};
  // No input/navigation, crash, lifecycle, race or automation probe is enabled.
  // Exact managed sentinel disables persistent config and filtered child env
  // excludes inherited REX_* overrides. The pinned host defaults all these off.
  return args;
}
int SelfTest() {
  unsigned checks=0,failed=0;
  const auto check=[&](bool ok) { ++checks; if(!ok) ++failed; };
  check(!NormalizeRoot(u"C:\\Users\\fixture\\Temp\\"));
  check(TempParent(L"C:\\Users\\fixture\\Temp\\")==L"C:\\Users\\fixture\\Temp");
  check(NormalizeRoot(TempParent(L"C:\\Users\\fixture\\Temp\\").u16string()).has_value());
  check(TempParent(L"C:\\")==L"C:\\");
  for (auto name:{L"720",L"1080",L"1440",L"2160"}) {
    const auto t=TargetFor(name); check(t.has_value());
    check(t && t->guest_width*t->scale==t->width && t->guest_height*t->scale==t->height);
    std::array<uint8_t,54> h{}; h[0]='B'; h[1]='M'; h[26]=1; h[28]=32;
    const auto put=[&](size_t at,uint32_t value) { for(unsigned i=0;i<4;++i) h[at+i]=static_cast<uint8_t>(value>>(8*i)); };
    const uint32_t pixels=t->width*t->height*4;
    put(2,pixels+54); put(10,54); put(14,40); put(18,t->width); put(22,t->height); put(34,pixels);
    check(BmpShape(h,pixels+54)==std::pair{t->width,t->height});
    check(!BmpShape(h,pixels+53)); check(!BmpShape(std::span(h).first(53),pixels+54));
    put(30,1); check(!BmpShape(h,pixels+54)); put(30,0);
    put(18,8193); check(!BmpShape(h,pixels+54)); put(18,t->width);
    put(10,55); check(!BmpShape(h,pixels+54)); put(10,54);
    put(34,pixels-1); check(!BmpShape(h,pixels+54)); put(34,pixels);
    put(22,0xffffffff); check(!BmpShape(h,pixels+54));
    const auto marker=std::string(kMarker)+std::to_string(t->width)+"x"+std::to_string(t->height)+", rgb555 bins 8";
    check(MarkerMatches(marker,t->width,t->height)); check(!MarkerMatches(marker,t->width+1,t->height));
    Options o{L"C:\\build",L"D:\\game",L"GPU",name,true,false};
    const auto args=Arguments(o,*t,L"C:\\temp\\fixture",1);
    check(std::count(args.begin(),args.end(),L"--mcla_first_frame_probe=true")==1);
    check(std::find(args.begin(),args.end(),L"--resolution=")!=args.end());
    check(std::count(args.begin(),args.end(),Flag(L"video_mode_width",std::to_wstring(t->guest_width)))==1);
    check(std::count(args.begin(),args.end(),Flag(L"video_mode_height",std::to_wstring(t->guest_height)))==1);
    check(std::count(args.begin(),args.end(),Flag(L"resolution_scale",std::to_wstring(t->scale)))==1);
    check(std::count(args.begin(),args.end(),Flag(L"draw_resolution_scale_x",std::to_wstring(t->scale)))==1);
    check(std::count(args.begin(),args.end(),Flag(L"draw_resolution_scale_y",std::to_wstring(t->scale)))==1);
  }
  check(!TargetFor(L"0720")); check(!TargetFor(L"0"));
  const std::array<std::wstring_view,5> valid{L"--build=C:\\build",L"--game=D:\\game",L"--gpu=GPU",L"--target=1080",L"--run"};
  check(Parse(valid).has_value()); check(!Parse(std::span(valid).first(4)));
  auto invalid=valid; invalid[4]=L"--target=720"; check(!Parse(invalid));
  invalid=valid; invalid[3]=L"--target=1081"; check(!Parse(invalid));
  invalid=valid; invalid[0]=L"--build=relative"; check(!Parse(invalid));
  invalid=valid; invalid[2]=L"--gpu=GPU\nname"; check(!Parse(invalid));
  std::cout<<"Render probe self-test: "<<checks<<" checks, "<<failed<<" failures\n";
  return failed?1:0;
}
int Probe(const Options &options) {
  Take(NoGameRunning());
  auto build_root=Take(fs::OpenDirectory(options.build));
  auto game_root=Take(fs::OpenDirectory(options.game));
  Take(fs::CompareDisjoint(build_root,game_root));
  auto build=Take(VerifyBuild(build_root));
  Require(build.fingerprint()=="9C6BEA718046F1B22D7FB9C4BEB8E03CC52DD3F466CCFB926CC585059F580BAA",
          "only the exact reviewed l6 runtime inventory is allowed");
  const auto host=std::find_if(build.files().begin(),build.files().end(),[](const auto &f){return f.name==L"mcla.exe";});
  Require(host!=build.files().end() && host->sha256==kHost,"only the exact reviewed l6 host is allowed");
  auto game=Take(VerifyGameRoot(game_root));
  const auto adapter=Take(Adapter(options.gpu));
  const auto lease=Take(mcla::lease::CheckBuildLease(build_root,kHost,false,false,true));
  (void)lease;
  auto temp=Take(fs::OpenDirectory(TempParent(std::filesystem::temp_directory_path()).wstring()));
  Take(fs::CompareDisjoint(temp,build_root)); Take(fs::CompareDisjoint(temp,game_root));
  Take(NoGameRunning());
  std::array<uint8_t,16> random{};
  Require(BCryptGenRandom(nullptr,random.data(),static_cast<ULONG>(random.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0,"random identity failed");
  std::wstring name=L"mcla-render-target-";
  for (const auto b:random) { name+=L"0123456789abcdef"[b>>4]; name+=L"0123456789abcdef"[b&15]; }
  const auto path=std::filesystem::path(temp.path())/name;
  Require(CreateDirectoryW(path.c_str(),nullptr)!=FALSE,"exclusive TEMP root creation failed");
  std::wcout<<L"PRESERVED EVIDENCE: "<<path.wstring()<<L'\n'<<std::flush;
  auto root=Take(fs::OpenDirectory(path.wstring()));
  auto lock=Take(fs::AcquireWriterLock(root));
  std::vector<fs::DirectoryGuard> children;
  for (auto name:{L"user",L"cache",L"update",L"metadata",L"diagnostics"}) {
    const auto child=path/name;
    Require(CreateDirectoryW(child.c_str(),nullptr)!=FALSE,"exclusive TEMP child creation failed");
    children.push_back(Take(fs::OpenDirectory(child.wstring())));
  }
  const auto target=*TargetFor(options.target);
  const auto base="MCLA-PASSIVE-RENDER-PROBE-V1\nhost "+std::string(kHost)+"\nbuild "+build.fingerprint()+
      "\ngame "+game.fingerprint()+"\nrequested "+std::to_string(target.width)+"x"+std::to_string(target.height)+
      "\nguest_mode "+std::to_string(target.guest_width)+"x"+std::to_string(target.guest_height)+
      "\nscale "+std::to_string(target.scale)+"\nadapter "+std::to_string(adapter)+
      "\nwindow 1280x720\ninput_injection false\npersonal_save_import false\nclaim startup_guest_output_dimensions_only\n";
  Write(root,L"managed.toml",mcla::managed_config::kSentinel);
  Write(root,L"result.txt",base+"state prepared\n");
  auto actual=options; actual.build=build.path(); actual.game=game.path();
  const auto args=Arguments(actual,target,path,adapter);
  Take(NoGameRunning());
  auto process=Take(StartOwnedProcess(std::filesystem::path(build.path())/L"mcla.exe",args,path));
  Write(root,L"result.txt",base+"state running\npid "+std::to_string(process.ProcessId())+"\n");
  bool marker=false;
  const auto start=GetTickCount64();
  while (GetTickCount64()-start<90000) {
    const auto status=Take(process.Poll());
    if (status.exit_code) break;
    const auto log=ReadLog(path/L"mcla.log");
    if (log && log->find(kMarker)!=log->npos) { marker=true; break; }
    Sleep(100);
  }
  const bool close_sent=CloseOwnedWindow(process.ProcessId());
  std::optional<ProcessStatus> stopped;
  const auto close_start=GetTickCount64();
  while (GetTickCount64()-close_start<10000) {
    auto status=Take(process.Poll());
    if (status.exit_code && status.active_job_processes==0) { stopped=status; break; }
    Sleep(100);
  }
  const auto observed=ReadBmp(path/L"user"/L"mcla-first-frame.bmp");
  const auto log=ReadLog(path/L"mcla.log");
  const bool dimensions=observed && observed->first==target.width && observed->second==target.height;
  const bool matching_log=observed && log && MarkerMatches(*log,observed->first,observed->second);
  const bool pass=marker && close_sent && stopped && stopped->exit_code==0 && dimensions && matching_log;
  const auto sentinel=Take(fs::ReadFile(root,L"managed.toml",1024));
  const bool sentinel_ok=std::string(sentinel.begin(),sentinel.end())==mcla::managed_config::kSentinel;
  const auto result=base+"state "+(pass && sentinel_ok?"STARTUP_DIMENSIONS_PASS_NOT_GAMEPLAY":"FAILED")+
      "\npid "+std::to_string(process.ProcessId())+"\nmarker "+std::to_string(marker)+
      "\nclose_sent "+std::to_string(close_sent)+"\nnormal_exit "+std::to_string(stopped && stopped->exit_code==0)+
      "\nowned_cleanup_required "+std::to_string(!stopped)+"\nsentinel_unchanged "+std::to_string(sentinel_ok)+
      "\nobserved "+(observed?std::to_string(observed->first)+"x"+std::to_string(observed->second):"invalid_or_missing")+"\n";
  Write(root,L"result.txt",result);
  std::cout<<result<<"No retries; preserve this evidence.\n";
  // The still-live OwnedProcess destructor closes only our non-breakaway job.
  return pass && sentinel_ok?0:1;
}
} // namespace
#ifndef MCLA_RENDER_PROBE_NO_MAIN
int wmain(int argc,wchar_t **argv) {
  try {
    std::vector<std::wstring_view> arguments;
    for(int i=1;i<argc;++i) arguments.emplace_back(argv[i]);
    const auto options=Parse(arguments);
    if (!options) {
      std::cerr<<"Use --self-test OR --build=<absolute> --game=<absolute> --gpu=<exact name> --target=720|1080|1440|2160 --run\n";
      return 2;
    }
    return options->self_test?SelfTest():Probe(*options);
  } catch (const std::exception &error) {
    std::cerr<<"Render target probe FAILED; all created evidence is preserved; no automatic retry. "<<error.what()<<'\n';
    return 1;
  }
}
#endif
