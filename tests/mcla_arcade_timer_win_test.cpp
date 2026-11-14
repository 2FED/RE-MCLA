// Synthetic-only include-source seams. Primary must compile/run separately.
// No game, GPU, window, files, hook installation or external process operations.
#include "../src/mcla_arcade_timer_win.cpp"
#include <nlohmann/json.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <string_view>

// Deliberately different bodies: even an ICF-enabled synthetic link must retain
// distinguishable public/implementation identities for admission checks.
REX_EXTERN(sub_821BDA90) { ctx.r0.u64 = 1; }
REX_EXTERN(__imp__sub_821BDA90) { ctx.r0.u64 = 2; }

namespace {
namespace api = mcla::arcade_timer;
namespace d = api::detail;
using Json = nlohmann::json;
unsigned checks{}, failures{}, calls{};
DWORD seen_error{};
unsigned seen_mxcsr{};
bool unwind{};
uint64_t tick = 100;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
void Check(bool ok, const char* name) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
template<class F> void Reject(F&& fn, const char* name) {
  bool caught = false; try { fn(); } catch (const std::runtime_error&) { caught = true; }
  Check(caught, name);
}
void Disturb() { SetLastError(0x99887766); _mm_setcsr(0x5F80); }
const d::Operations dirty_operations{
    [] { Disturb(); return tick++; },
    [](const PPCContext&, const d::MemoryArg& m, mcla::timer_journal::State& out) {
      Disturb(); return mcla::timer_memory::Read(reinterpret_cast<uintptr_t>(m.base), m.initial_timer, out);
    }};
void Original(PPCContext& ctx, uint8_t* base) {
  ++calls; seen_error = GetLastError(); seen_mxcsr = _mm_getcsr();
  base[ctx.r3.u32+8] ^= 0x5A;
  ctx.r3.u64 = 0xFEEDFACE12345678ull; ctx.r31.u64 ^= 0x8877665544332211ull;
  ctx.r5.u64 ^= 0xDEADBEEFull; ctx.f1.u64 ^= 0x1122334455667788ull;
  ctx.lr = 0x82221100;
  _mm_setcsr(0x3F80); SetLastError(0xABCD1234);
  if (unwind) throw std::runtime_error("original exception");
}
void ContextCase(bool throws, bool observed) {
  std::array<uint8_t, 512> direct_memory{}, wrapped_memory{};
  for (size_t i=0; i<direct_memory.size(); ++i) direct_memory[i] = uint8_t(i);
  wrapped_memory = direct_memory;
  PPCContext direct{}, wrapped{};
  direct.r3.u32=128; direct.r5.u64=0xAABBCCDD; direct.f1.u64=0x0123456789ABCDEFull;
  direct.lr=0x821BCA94; std::memcpy(&wrapped, &direct, sizeof direct);
  auto observer = std::make_unique<d::Observer>(mcla::timer_journal::Window{100,100000,10000}, true);
  unwind=throws; tick=100; const auto before_calls=calls;
  auto run = [&](PPCContext& ctx, uint8_t* base, bool instrumentation) {
    _mm_setcsr(0x1F80); SetLastError(0x24681357); bool caught=false;
    try {
      if (instrumentation && observed) d::InvokeObserved(Original, observer.get(),
                                                        ctx, base, dirty_operations, d::HostState::Capture());
      else if (instrumentation) api::Invoke(Original, ctx, base); // Public pre-Begin route.
      else Original(ctx, base);
    } catch (const std::runtime_error& e) { caught=std::string_view(e.what())=="original exception"; }
    const auto error=GetLastError(); const auto mxcsr=_mm_getcsr();
    Check(caught==throws, "original exception propagated");
    Check(seen_error==0x24681357 && seen_mxcsr==0x1F80, "original sees incoming LastError and MXCSR");
    Check(error==0xABCD1234 && mxcsr==0x3F80, "original output LastError and MXCSR survive afterwork");
  };
  run(direct,direct_memory.data(),false); run(wrapped,wrapped_memory.data(),true);
  Check(calls==before_calls+2,"one original call per invocation");
  Check(std::memcmp(&direct,&wrapped,sizeof direct)==0,"full context differential");
  Check(direct_memory==wrapped_memory,"full memory differential");
  auto snapshot=std::make_unique<d::Observer::Snapshot>();
  Check(observer->try_freeze_into(*snapshot),"drained differential snapshot");
  Check(snapshot->records().size()==(observed?1:0),"observer optional row count");
  if(observed) {
    const auto& row=snapshot->records()[0];
    Check(row.observation.identity.timer_address==128 && row.observation.identity.caller_address==0x821BCA94,
          "incoming identity retained");
    Check(row.observation.before_available && row.observation.after_available,
          "allowlisted before/after reads available");
    Check((row.observation.before.words[0]^row.observation.after.words[0])==0x5A000000,
          "after-state uses initial address despite mutated r3");
    Check(bool(row.callback_issues & mcla::timer_observer::kOriginalUnwound)==throws,"unwind truthful");
    const auto json=Json::parse(d::Serialize(*snapshot,token));
    Check(json["schema"]=="mcla-arcade-timer-observer-v1","distinct schema");
    Check(json["word_offsets"]==mcla::timer_journal::kWordOffsets &&
          json["byte_offsets"]==mcla::timer_journal::kByteOffsets &&
          json["qword_offsets"]==mcla::timer_journal::kQwordOffsets,"exact offsets");
    for(const char* flag:{"simulation_proven","fps_proven","frame_to_step_mapping_proven",
                          "hud_binding_proven","update_count_proven","native_admitted"})
      Check(json[flag]==false,"no implicit runtime or binding proof");
  }
}
void InstallTests() {
  d::State s;
  Reject([&]{d::InstallState(s,false,sub_821BDA90);},"disabled install rejected");
  Reject([&]{d::InstallState(s,true,nullptr);},"missing dispatcher target rejected");
  Reject([&]{d::InstallState(s,true,__imp__sub_821BDA90);},"imp instead of public rejected");
  Reject([&]{d::InstallState(s,true,Original);},"unrelated registered target rejected");
  d::InstallState(s,true,sub_821BDA90);
  Reject([&]{d::InstallState(s,true,sub_821BDA90);},"repeat install rejected");
  d::DisarmState(s);
  Reject([&]{d::BeginState(s,true,100,1200,100,token);},"disarm before begin terminal");
  d::State cancelled;d::DisarmState(cancelled);
  Reject([&]{d::InstallState(cancelled,true,sub_821BDA90);},"disarm before install terminal");
}
void LifecycleTests() {
  using Clock=std::chrono::steady_clock;
  auto now=[] {return Clock::time_point{};}; auto wait=[] {};
  d::State absent;
  Reject([&]{d::BeginState(absent,true,100,1200,100,token);},"begin requires install");
  Reject([&]{d::WindowState(absent,token);},"window absent before begin");
  Reject([&]{d::FinishState(absent,true,{},token,1200,now,wait);},"no empty success without begin");
  d::State disabled;d::InstallState(disabled,true,sub_821BDA90);
  Reject([&]{d::BeginState(disabled,false,100,1200,100,token);},"disabled begin rejected");
  for(const auto* bad:{"", "00000000000000000000000000000000", "ABCDEF01234567890123456789012345"}) {
    d::State s; d::InstallState(s,true,sub_821BDA90);
    Reject([&]{d::BeginState(s,true,100,1200,100,bad);},"invalid begin token");
  }
  for(auto window:{mcla::timer_journal::Window{0,100,100}, {100,100,100}, {100,1301,100}, {100,1200,0}}) {
    d::State s; d::InstallState(s,true,sub_821BDA90);
    Reject([&]{d::BeginState(s,true,window.begin_qpc,window.end_qpc,window.qpc_frequency,token);},"invalid window");
  }
  for(unsigned mode=0;mode<7;++mode) {
    d::State s; d::InstallState(s,true,sub_821BDA90); d::BeginState(s,true,100,1300,100,token);
    const auto window=d::WindowState(s,token);
    Check(window.begin_qpc==100 && window.end_qpc==1300 && window.qpc_frequency==100,"immutable exact max12s window");
    Reject([&]{d::WindowState(s,"11111111111111111111111111111111");},"window requires exact token");
    Reject([&]{d::BeginState(s,true,100,1300,100,token);},"one begin only");
    std::stop_source stop; if(mode==2)stop.request_stop();
    unsigned clock_calls=0;
    auto clock=[&]{++clock_calls;return Clock::time_point{}+
        (mode>=3 && clock_calls>=(mode==6?5:mode) ? std::chrono::seconds(3) : std::chrono::seconds(0));};
    bool failed=false;
    try {d::FinishState(s,true,stop.get_token(),mode==5?"11111111111111111111111111111111":token,
                       mode==1?1299:1300,clock,wait);}
    catch(const std::runtime_error&){failed=true;}
    Check(failed==(mode!=0),"end/cancel/drain-late/token checked");
    Reject([&]{d::FinishState(s,true,{},token,1301,now,wait);},"finish retry always rejected");
    Reject([&]{d::BeginState(s,true,1301,1400,100,token);},"finish terminal begin forbidden");
    delete s.observer.load(); // Local synthetic storage only, no in-flight work.
  }
  d::State early; d::InstallState(early,true,sub_821BDA90); d::BeginState(early,true,100,1300,100,token);
  d::DisarmState(early);
  Reject([&]{d::FinishState(early,true,{},token,1300,now,wait);},"early disarm never emits complete receipt");
  delete early.observer.load();
}
void OverflowAndDisarmed() {
  auto observer=std::make_unique<d::Observer>(mcla::timer_journal::Window{100,100000,10000},true);
  std::array<uint8_t,512> memory{}; unwind=false; tick=100;
  const auto before=calls;
  for(unsigned i=0;i<1026;++i){PPCContext ctx{};ctx.r3.u32=128;ctx.lr=0x821BCA94;
    _mm_setcsr(0x1F80);SetLastError(0x24681357);
    d::InvokeObserved(Original,observer.get(),ctx,memory.data(),dirty_operations,d::HostState::Capture());
    if(i==1025)Check(seen_error==0x24681357 && seen_mxcsr==0x1F80 &&
                     GetLastError()==0xABCD1234 && _mm_getcsr()==0x3F80,"overflow preserves host state");}
  observer->stop_admissions();
  PPCContext ctx{};ctx.r3.u32=128;ctx.lr=0x821BCA94;
  _mm_setcsr(0x1F80);SetLastError(0x24681357);
  d::InvokeObserved(Original,observer.get(),ctx,memory.data(),dirty_operations,d::HostState::Capture());
  Check(seen_error==0x24681357 && seen_mxcsr==0x1F80 &&
        GetLastError()==0xABCD1234 && _mm_getcsr()==0x3F80,"disarmed preserves host state");
  auto snapshot=std::make_unique<d::Observer::Snapshot>();observer->try_freeze_into(*snapshot);
  Check(calls==before+1027,"overflow and disarmed original still once");
  Check(snapshot->records().size()==1024 && snapshot->counts().overflow_entries==2,"sticky bounded overflow");
  Check(snapshot->counts().observed_entries==1026,"disarmed not falsely observed");
}
void RetentionTests() {
  std::array<uint8_t,512> memory{};
  for(unsigned scenario=0;scenario<7;++scenario) {
    auto observer=std::make_unique<d::Observer>(mcla::timer_journal::Window{100,200,100},true);
    constexpr uint64_t brackets[][2]={{50,99},{200,201},{50,100},{199,200},{0,150},{110,150},{110,150}};
    unsigned index=0;
    const d::Operations ops{
        [&]{return brackets[scenario][index++];},
        [&](const PPCContext&,const d::MemoryArg& m,mcla::timer_journal::State& out){
          if(scenario==5){out.words.fill(UINT32_MAX);return false;}
          return mcla::timer_memory::Read(reinterpret_cast<uintptr_t>(m.base),m.initial_timer,out);
        }};
    PPCContext ctx{};ctx.r3.u32=128;ctx.lr=scenario==6?0:0x821BCA94;unwind=false;
    d::InvokeObserved(Original,observer.get(),ctx,memory.data(),ops,d::HostState::Capture());
    auto snapshot=std::make_unique<d::Observer::Snapshot>();observer->try_freeze_into(*snapshot);
    Check(snapshot->records().size()==(scenario<2?0:1),"only wholly outside valid calls discarded");
    if(scenario==0)Check(snapshot->counts().outside_before_entries==1,"outside-before count truthful");
    if(scenario==1)Check(snapshot->counts().outside_after_entries==1,"outside-after count truthful");
    if(scenario>=2) {
      auto report=snapshot->classify_offline();
      const uint32_t issue=scenario==2?mcla::timer_journal::kStraddlesBegin:
          scenario==3?mcla::timer_journal::kStraddlesEnd:
          scenario==4?mcla::timer_journal::kInvalidClock:
          scenario==5?mcla::timer_journal::kMissingState:mcla::timer_journal::kMissingIdentity;
      Check((report->issues()&issue)!=0,"straddle/malformed/missing states not accepted away");
      if(scenario==5) {
        const auto json=Json::parse(d::Serialize(*snapshot,token));
        Check(json["records"][0]["before"].is_null() && json["records"][0]["after"].is_null(),
              "failed reads never serialize partial numeric state");
      }
    }
  }
}
std::atomic<bool> pending_entered{},pending_release{};
void PendingOriginal(PPCContext&,uint8_t*) {
  pending_entered.store(true);
  while(!pending_release.load())std::this_thread::yield();
}
void PendingDrainTest() {
  auto observer=std::make_unique<d::Observer>(mcla::timer_journal::Window{100,100000,10000},true);
  std::array<uint8_t,512> memory{};PPCContext ctx{};ctx.r3.u32=128;ctx.lr=0x821BCA94;
  pending_entered=false;pending_release=false;tick=100;
  std::thread worker([&]{d::InvokeObserved(PendingOriginal,observer.get(),ctx,memory.data(),dirty_operations,d::HostState::Capture());});
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(!pending_entered.load() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
  const bool entered=pending_entered.load();Check(entered,"synthetic pending original entered");
  unsigned waits=0;using Clock=std::chrono::steady_clock;
  if(entered) {
    Reject([&]{d::Drain(*observer,{},token,[&]{return Clock::time_point{}+std::chrono::seconds(waits?3:0);},
                       [&]{++waits;});},"pending original causes bounded drain failure");
    Check(waits==1,"pending drain timeout uses cooperative wait");
  }
  pending_release.store(true);worker.join();
  auto snapshot=std::make_unique<d::Observer::Snapshot>();
  Check(observer->try_freeze_into(*snapshot) && snapshot->records().size()==1,"pending storage alive until completion");
}
void PublicFrequencyTest() {
  Check(rex::cvar::SetFlagByName("mcla_arcade_timer_probe","true"),"public frequency test enables init-only option");
  d::InstallState(*d::configured,true,sub_821BDA90); // Synthetic exact-target seam only.
  LARGE_INTEGER actual{};Check(QueryPerformanceFrequency(&actual)!=FALSE && actual.QuadPart>0,"public actual QPF");
  const auto frequency=uint64_t(actual.QuadPart),begin=d::Qpc();
  Reject([&]{api::Begin(begin,begin+frequency,frequency+1,token);},"public Begin wrong-QPF rejected");
  Check(!d::configured->begun && d::configured->observer.load()==nullptr,"wrong QPF never publishes or consumes Begin");
  api::Begin(begin,begin+frequency,frequency,token);
  Check(api::ObservationWindow(token).qpc_frequency==frequency,"correct public QPF still begins once");
  api::Disarm();
  Reject([&]{api::Begin(begin,begin+frequency,frequency,token);},"public disarm terminal after correct Begin");
}
}
int main(){
  const auto initial=d::HostState::Capture();
  ContextCase(false,true);ContextCase(true,true);ContextCase(false,false);ContextCase(true,false);
  InstallTests();LifecycleTests();OverflowAndDisarmed();RetentionTests();PendingDrainTest();PublicFrequencyTest();initial.Restore();
  std::cout<<"checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
