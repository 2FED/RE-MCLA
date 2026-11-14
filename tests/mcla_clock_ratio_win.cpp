// Guest-free, bounded clock-only DLL diagnostic. No Runtime::Setup or game.
// Usage: test.exe ABSOLUTE_DLL_PATH [--mutate-ratio]
// Exit0=integer bounds satisfied;1=violations/detected mutation;2=input/runtime
// failure;3=mutation not detected. Primary pins the DLL and dependency closure.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace clock_ratio {
using Json=nlohmann::json;
using U64=uint64_t;
using Ratio=std::pair<U64,U64>;
constexpr U64 kGuestFrequency=50000000;
constexpr unsigned kSparse=65,kDense=4097,kSavedViolations=64;
void Need(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
U64 Add(U64 a,U64 b){Need(a<=UINT64_MAX-b,"integer addition overflow");return a+b;}
U64 Mul(U64 a,U64 b){Need(!b||a<=UINT64_MAX/b,"integer multiplication overflow");return a*b;}
U64 Scale(U64 ticks,Ratio ratio){Need(ratio.first&&ratio.second,"positive ratio");return Mul(ticks,ratio.first)/ratio.second;}
U64 Qpc(){LARGE_INTEGER v{};Need(QueryPerformanceCounter(&v)&&v.QuadPart>0,"positive QPC sample");return U64(v.QuadPart);}
std::string Utf8(const std::wstring& text){
  Need(text.size()<=32767,"bounded path");
  const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
  Need(size>0,"path UTF8 conversion");std::string result(size,'\0');
  Need(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),int(text.size()),result.data(),size,nullptr,nullptr)==size,"complete path conversion");return result;
}
bool SamePath(const std::wstring& a,const std::wstring& b){
  return a.size()==b.size()&&CompareStringOrdinal(a.data(),int(a.size()),b.data(),int(b.size()),TRUE)==CSTR_EQUAL;
}
std::wstring AbsolutePath(const wchar_t* arg){
  std::wstring path(arg);Need(path.size()>=4&&path.size()<32767,"bounded explicit DLL path");
  std::replace(path.begin(),path.end(),L'/',L'\\');
  Need(((path[0]>=L'A'&&path[0]<=L'Z')||(path[0]>=L'a'&&path[0]<=L'z'))&&path[1]==L':'&&path[2]==L'\\',"plain absolute DOS path required");
  Need(path.find(L':',2)==std::wstring::npos,"no alternate stream path");
  std::vector<wchar_t> canonical(32768);const DWORD size=GetFullPathNameW(path.c_str(),DWORD(canonical.size()),canonical.data(),nullptr);
  Need(size>0&&size<canonical.size(),"absolute path resolution");
  std::wstring result(canonical.data(),size);Need(SamePath(path,result),"canonical absolute path required");
  const auto attrs=GetFileAttributesW(result.c_str());Need(attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_DIRECTORY),"existing DLL file required");
  return result;
}
struct Module {
  HMODULE handle{};
  ~Module(){if(handle)FreeLibrary(handle);}
  Module(const Module&)=delete;Module& operator=(const Module&)=delete;
  explicit Module(const std::wstring& path){handle=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);Need(handle!=nullptr,"explicit DLL load failed");}
  std::wstring Path() const {std::vector<wchar_t> path(32768);const auto size=GetModuleFileNameW(handle,path.data(),DWORD(path.size()));Need(size>0&&size<path.size(),"loaded module path");return std::wstring(path.data(),size);}
  template<class T>T Symbol(const char* name) const {const auto value=GetProcAddress(handle,name);Need(value!=nullptr,"required exact clock export missing");return reinterpret_cast<T>(value);}
};
struct Api {
  using Get=U64(__cdecl*)();using GetDouble=double(__cdecl*)();
  using Set=void(__cdecl*)(U64);using SetDouble=void(__cdecl*)(double);
  using GetRatio=Ratio(__cdecl*)();using Flag=bool&(__cdecl*)();
  Get guest,host_frequency,guest_frequency;GetDouble scalar;Set frequency_set;SetDouble scalar_set;GetRatio ratio;Flag no_scaling,source_raw;
  explicit Api(const Module& m):
    guest(m.Symbol<Get>("?QueryGuestTickCount@Clock@chrono@rex@@SA_KXZ")),
    host_frequency(m.Symbol<Get>("?QueryHostTickFrequency@Clock@chrono@rex@@SA_KXZ")),
    guest_frequency(m.Symbol<Get>("?guest_tick_frequency@Clock@chrono@rex@@SA_KXZ")),
    scalar(m.Symbol<GetDouble>("?guest_time_scalar@Clock@chrono@rex@@SANXZ")),
    frequency_set(m.Symbol<Set>("?set_guest_tick_frequency@Clock@chrono@rex@@SAX_K@Z")),
    scalar_set(m.Symbol<SetDouble>("?set_guest_time_scalar@Clock@chrono@rex@@SAXN@Z")),
    ratio(m.Symbol<GetRatio>("?guest_tick_ratio@Clock@chrono@rex@@SA?AU?$pair@_K_K@std@@XZ")),
    no_scaling(m.Symbol<Flag>("?FLAGS_clock_no_scaling_storage_@@YAAEA_NXZ")),
    source_raw(m.Symbol<Flag>("?FLAGS_clock_source_raw_storage_@@YAAEA_NXZ")){}
};
struct Sample {U64 before,guest,after;};
Sample Capture(const Api& api){const auto before=Qpc();const auto guest=api.guest();const auto after=Qpc();Need(after>=before,"QPC bracket reversed");return {before,guest,after};}
Json Wire(Sample s){return Json{{"qpc_before",s.before},{"guest_ticks",s.guest},{"qpc_after",s.after}};}
struct Bounds {U64 lower,upper;};
Bounds Expected(Sample first,Sample last,U64 query_intervals,Ratio ratio){
  Need(query_intervals>0&&last.before>=first.before&&last.after>=first.after,"monotonic bracket endpoints");
  const U64 lower_ticks=last.before>first.after?last.before-first.after:0;
  const U64 upper_ticks=last.after-first.before;
  // For n successful single-thread updates, sum(floor(x_i)) lies between
  // floor(sum(x_i))-(n-1) and floor(sum(x_i)). QPC brackets bound sum(x_i).
  const auto scaled_lower=Scale(lower_ticks,ratio);const auto truncation=query_intervals-1;
  return {scaled_lower>truncation?scaled_lower-truncation:0,Scale(upper_ticks,ratio)};
}
} // namespace clock_ratio

int wmain(int argc,wchar_t** argv){using namespace clock_ratio;
  try{
    Need(argc==2||(argc==3&&std::wstring(argv[2])==L"--mutate-ratio"),"usage ABSOLUTE_DLL_PATH [--mutate-ratio]");
    const bool mutation=argc==3;const auto requested=AbsolutePath(argv[1]);Module module(requested);
    const auto loaded=module.Path();Need(SamePath(requested,loaded),"actual loaded DLL path differs");Api api(module);
    Need(!api.no_scaling()&&!api.source_raw(),"both clock cvars must have default false values");
    const auto initial_frequency=api.guest_frequency();const auto initial_scalar=api.scalar();const auto initial_ratio=api.ratio();
    // Process-local clock configuration only, matching Runtime::Setup's two
    // relevant assignments. Runtime::Setup itself is never called.
    api.frequency_set(kGuestFrequency);api.scalar_set(1.0);
    LARGE_INTEGER qpf{};Need(QueryPerformanceFrequency(&qpf)&&qpf.QuadPart>0,"positive QPF");
    const auto host_frequency=api.host_frequency(),guest_frequency=api.guest_frequency();const auto ratio=api.ratio();
    Need(host_frequency==U64(qpf.QuadPart)&&guest_frequency==kGuestFrequency&&api.scalar()==1.0,"selected clock configuration");
    Need(!api.no_scaling()&&!api.source_raw()&&ratio.first&&ratio.second,"unchanged default clock cvars");
    Need(Mul(ratio.first,host_frequency)==Mul(ratio.second,kGuestFrequency),"configured exact rational frequency");
    // Establish the configured clock before the measured first baseline. Initial
    // load/startup accumulation must not be misinterpreted as an elapsed sample.
    const auto warmup=Capture(api);std::vector<Sample> samples;samples.reserve(kSparse+kDense);
    for(unsigned i=0;i<kSparse;++i){if(i)Sleep(16);samples.push_back(Capture(api));}
    for(unsigned i=0;i<kDense;++i)samples.push_back(Capture(api));
    Need(api.guest_frequency()==guest_frequency&&api.scalar()==1.0&&api.ratio()==ratio&&
         !api.no_scaling()&&!api.source_raw()&&api.host_frequency()==host_frequency,"clock settings unchanged after collection");
    Ratio expected_ratio=ratio;if(mutation)expected_ratio.first=Mul(expected_ratio.first,2);
    U64 adjacent_bad=0,cumulative_bad=0,monotonic_bad=0;Json violations=Json::array(),wire=Json::array();
    auto check=[&](const char* kind,size_t first,size_t last,U64 intervals,U64& count){
      const auto bounds=Expected(samples[first],samples[last],intervals,expected_ratio);
      const bool monotonic=samples[last].guest>=samples[first].guest;
      const U64 observed=monotonic?samples[last].guest-samples[first].guest:0;
      const bool good=monotonic&&observed>=bounds.lower&&observed<=bounds.upper;
      if(!good){++count;if(violations.size()<kSavedViolations)violations.push_back(Json{{"kind",kind},{"first",first},{"last",last},
          {"query_intervals",intervals},{"guest_monotonic",monotonic},{"observed_difference",monotonic?Json(observed):Json(nullptr)},
          {"lower",bounds.lower},{"upper",bounds.upper}});}
    };
    for(size_t i=0;i<samples.size();++i){auto value=Wire(samples[i]);value["ordinal"]=i;value["phase"]=i<kSparse?"sparse":"dense";wire.push_back(std::move(value));
      if(i){monotonic_bad+=samples[i].guest<samples[i-1].guest;check("adjacent",i-1,i,1,adjacent_bad);check("cumulative",0,i,U64(i),cumulative_bad);}}
    const bool failed=adjacent_bad||cumulative_bad||monotonic_bad;int code=failed?1:0;
    const char* status=failed?(mutation?"CLOCK_RATIO_MUTATION_DETECTED":"CLOCK_RATIO_BOUND_VIOLATION"):"CLOCK_RATIO_INTEGER_BOUNDS_SATISFIED";
    if(mutation&&!failed){code=3;status="CLOCK_RATIO_MUTATION_NOT_DETECTED";}
    Json report{{"schema","mcla-clock-ratio-diagnostic-v1"},{"status",status},{"requested_dll",Utf8(requested)},{"loaded_dll",Utf8(loaded)},
      {"pid",GetCurrentProcessId()},{"tid",GetCurrentThreadId()},{"qpc_frequency",U64(qpf.QuadPart)},
      {"initial_guest_frequency",initial_frequency},{"initial_guest_scalar",initial_scalar},{"initial_ratio",{initial_ratio.first,initial_ratio.second}},
      {"guest_frequency",guest_frequency},{"guest_scalar",api.scalar()},{"observed_ratio",{ratio.first,ratio.second}},
      {"comparison_ratio",{expected_ratio.first,expected_ratio.second}},{"mutation",mutation},{"clock_no_scaling",api.no_scaling()},
      {"clock_source_raw",api.source_raw()},{"warmup",Wire(warmup)},{"sparse_samples",kSparse},{"dense_samples",kDense},
      {"sparse_sleep_request_ms",16},{"samples",wire},{"adjacent_violations",adjacent_bad},{"cumulative_violations",cumulative_bad},
      {"monotonic_violations",monotonic_bad},{"violation_examples",violations},{"maximum_violation_examples",kSavedViolations},
      {"cumulative_truncation_budget","query_intervals_minus_one_guest_tick_units"},
      {"scope","ONE_CALLING_THREAD_DEFAULT_QPC_SOURCE_PROCESS_LOCAL_50MHZ_CONFIGURATION"},
      {"sample_brackets_are_not_atomic",true},{"single_updater_assumption_independently_proven",false},
      {"dll_source_reproduction_proven",false},{"dll_integrity_independently_verified",false},
      {"runtime_setup_called",false},{"game_launched",false},{"system_timer_resolution_changed",false},{"priority_changed",false},
      {"fps_proven",false},{"simulation_proven",false},{"correct_game_speed_proven",false},{"native_game_admitted",false}};
    const auto output=report.dump();Need(output.size()<=2*1024*1024,"bounded clock report");std::cout<<output<<'\n';return std::cout?code:2;
  }catch(const std::exception& error){std::cerr<<"Clock diagnostic rejected: "<<error.what()<<'\n';return 2;}
  catch(...){std::cerr<<"Clock diagnostic rejected: unknown exception\n";return 2;}
}
