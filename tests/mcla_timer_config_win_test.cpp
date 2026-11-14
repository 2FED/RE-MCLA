// Synthetic contexts/readers/originals only; no game, GPU, windows or files.
// Build with MCLA_TIMER_CONFIG_PROBE and the same SDK link closure as the
// existing mcla_update_timing_win_test. Production Install is not mocked here.
#include "../src/mcla_timer_config_win.cpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
namespace api=mcla::timer_config;
namespace d=api::detail;
using Json=nlohmann::json;
constexpr char token[]="0123456789abcdef0123456789abcdef";
std::atomic<uint64_t> ticks{100},calls{},config_reads{},timer_reads{};
std::atomic<bool> entered{},release_work{};
thread_local d::State* active{};
thread_local PPCContext* corrupt_context{};
thread_local int mode{},read_mode{},nest_depth{};
unsigned checks{},failures{};
void Check(bool ok,const char* label) {++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
template<class F> void Reject(F&& fn,const char* label) {bool rejected=false;try{fn();}catch(...){rejected=true;}Check(rejected,label);}
template<class F> void Wait(F&& predicate) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!predicate()){if(std::chrono::steady_clock::now()>deadline)std::abort();std::this_thread::yield();}
}
void Block() {entered.store(true);Wait([]{return release_work.load();});}
uint64_t Clock(){SetLastError(800);return ticks.fetch_add(1);}
uint64_t EqualClock(){return 100;}
uint64_t ZeroClock(){return 0;}
uint64_t ThrowClock(){throw 90;}
void Disturb() {
  SetLastError(801);_mm_setcsr(_mm_getcsr()^0x2000);
  if(corrupt_context)corrupt_context->r31.u64=0xbad;
}
bool ConfigReader(uintptr_t,uint32_t,d::Config& out) {
  ++config_reads;out.words.fill(0x12345678);out.byte100=255;Disturb();
  if(read_mode==2)throw 91;
  if(read_mode==3)Block();
  return read_mode!=1;
}
bool TimerReader(uintptr_t,uint32_t address,mcla::step_timer_memory::State& out) {
  ++timer_reads;out.timer.words.fill(0x3d088889);out.count=2;out.split_state=255;
  Disturb();if(address!=d::kTimer)throw 92;if(read_mode==2)throw 93;return read_mode!=1;
}
std::unique_ptr<d::State> State() {
  auto s=std::make_unique<d::State>();s->data.begin=100;s->data.end=100000;
  s->data.frequency=1000;s->data.token=token;s->clock=Clock;
  s->config_reader=ConfigReader;s->timer_reader=TimerReader;
  ticks=100;calls=config_reads=timer_reads=0;mode=read_mode=nest_depth=0;
  corrupt_context=nullptr;entered=false;release_work=false;return s;
}
PPCContext Context(uint32_t address=0x1000) {
  PPCContext c{};c.lr=0x822D29C0;c.r3.u64=0xffffffff00000000ull|address;
  c.r5.u64=UINT64_MAX-4;c.f1.u64=0x7ff8000000000001ull;c.f2.u64=0x8000000000000000ull;
  c.r31.u64=77;return c;
}
uint8_t* Base(){return reinterpret_cast<uint8_t*>(1);}
void Original(PPCContext& ctx,uint8_t* base);
void Call(d::State& s,d::Kind kind,PPCContext& ctx,uint8_t* base=Base()) {
  auto* previous=active;active=&s;d::Invoke(&s,kind,Original,ctx,base);active=previous;
}
void Original(PPCContext& ctx,uint8_t* base) {
  ++calls;
  if(mode==1){ctx.r7.u64=99;SetLastError(1234);_mm_setcsr(_mm_getcsr()^0x4000);throw 42;}
  if(mode==2){Block();return;}
  if(mode==3){ctx.r3.u32+=4;return;}
  if(mode==4){ticks=99;return;}
  if(mode==5&&nest_depth<9) {++nest_depth;Call(*active,d::Kind::Constructor,ctx,base);--nest_depth;return;}
  if(mode==6) {
    const auto saved=mode;mode=7;ctx.lr=0x822D26C4;
    Call(*active,d::Kind::Apply,ctx,base);mode=saved;return;
  }
  if(mode==7) {
    const auto saved=mode;mode=0;const auto r3=ctx.r3.u64;
    ctx.r3.u32=d::kTimer;ctx.lr=0x822D24CC;Call(*active,d::Kind::RawSetter,ctx,base);
    ctx.lr=0x822D24DC;Call(*active,d::Kind::FixedSetter,ctx,base);
    ctx.r3.u64=r3;mode=saved;return;
  }
  if(mode==8){ctx.r7.u64=99;SetLastError(1234);_mm_setcsr(_mm_getcsr()^0x4000);}
}
void Register(d::State& s,PPCContext& ctx) {ctx.lr=0x822D29C0;Call(s,d::Kind::Constructor,ctx);}
Json Cut(d::State& s){return Json::parse(d::Cut(s,token));}
void Chain(d::State& s,PPCContext& ctx) {Register(s,ctx);ctx.lr=0x11111111;mode=6;Call(s,d::Kind::Initializer,ctx);mode=0;}
void RealConfigReaderTests() {
  constexpr uint32_t guest=0x1000;
  constexpr std::array<uint32_t,7> expected{
    0x01234567,0x89abcdef,0xff008080,0x80000000,0x7fc00001,0x00000001,0xfedcba98};
  auto Populate=[&](uint8_t* object) {
    for(size_t i=0;i<expected.size();++i) {
      const auto at=72+4*i;const auto word=expected[i];
      object[at]=uint8_t(word>>24);object[at+1]=uint8_t(word>>16);
      object[at+2]=uint8_t(word>>8);object[at+3]=uint8_t(word);
    }
    object[100]=0xa5;
  };
  auto Dirty=[] {d::Config out;out.words.fill(UINT32_MAX);out.byte100=255;return out;};
  auto Cleared=[](const d::Config& out) {
    return out.byte100==0&&std::all_of(out.words.begin(),out.words.end(),[](uint32_t v){return v==0;});
  };
  auto Exact=[&](const d::Config& out){return out.words==expected&&out.byte100==0xa5;};
  std::array<uint8_t,108> bytes{};bytes.fill(0xcc);Populate(bytes.data());
  const auto untouched=bytes;const auto base=reinterpret_cast<uintptr_t>(bytes.data())-guest;
  auto out=Dirty();
  Check(d::ReadConfig(base,guest,out)&&Exact(out),"actual self RPM decodes exact seven BE words and byte100");
  Check(bytes==untouched,"actual reader never writes its source");
  for(size_t i=0;i<72;++i)bytes[i]^=0xff;
  for(size_t i=101;i<bytes.size();++i)bytes[i]^=0xff;
  out=Dirty();Check(d::ReadConfig(base,guest,out)&&Exact(out),"unselected bytes do not enter decoded Config");
  struct Invalid {uintptr_t base;uint32_t guest;};
  for(const auto bad:std::array<Invalid,5>{{
      {0,guest},{base,0},{base,0xdfffffc0u},{base,UINT32_MAX-106u},
      {UINTPTR_MAX-guest,guest}}}) {
    out=Dirty();Check(!d::ReadConfig(bad.base,bad.guest,out)&&Cleared(out),
                      "invalid zero/boundary/overflow translation clears every output field");
  }
  SYSTEM_INFO info{};GetSystemInfo(&info);const size_t page=info.dwPageSize;
  Check(page>=108,"native page supports bounded reader fixture");if(page<108)return;
  auto* allocation=static_cast<uint8_t*>(VirtualAlloc(nullptr,2*page,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
  Check(allocation!=nullptr,"allocate only owned two-page synthetic fixture");if(!allocation)return;
  struct OwnedPages {uint8_t* value;~OwnedPages(){Check(VirtualFree(value,0,MEM_RELEASE)!=FALSE,"release only owned page fixture");}} owned{allocation};
  std::memset(allocation,0xcc,2*page);
  // Only +72..+100 is readable: +101..+107 is in the protected next page.
  // A mistaken 108-byte whole-object RPM would fail this positive control.
  auto* edge=allocation+page-101;Populate(edge);
  DWORD previous{};
  const auto protected_ok=VirtualProtect(allocation+page,page,PAGE_NOACCESS,&previous)!=FALSE;
  Check(protected_ok,"protect only owned second page");if(!protected_ok)return;
  out=Dirty();Check(d::ReadConfig(reinterpret_cast<uintptr_t>(edge)-guest,guest,out)&&Exact(out),
                    "actual RPM requests only 29 bytes and succeeds at exact readable edge");
  out=Dirty();Check(!d::ReadConfig(reinterpret_cast<uintptr_t>(allocation+page)-guest,guest,out)&&Cleared(out),
                    "owned PAGE_NOACCESS read fails and clears all stale fields");
  // The requested 29-byte range has 18 readable bytes followed by 11 inaccessible
  // bytes. Windows may report zero or partial copied bytes; neither is accepted.
  auto* straddling=allocation+page-90;
  out=Dirty();Check(!d::ReadConfig(reinterpret_cast<uintptr_t>(straddling)-guest,guest,out)&&Cleared(out),
                    "partially accessible 29-byte range fails without exposing partial bytes");
}
}
REX_EXTERN(__imp__sub_822D2818){Original(ctx,base);}
REX_EXTERN(__imp__sub_822D2548){Original(ctx,base);}
REX_EXTERN(__imp__sub_822D2478){Original(ctx,base);}
REX_EXTERN(__imp__sub_821BD9B8){Original(ctx,base);}
REX_EXTERN(__imp__sub_821BD9F8){Original(ctx,base);}

int main(int argc,char** argv) {
  if(argc==2&&std::string(argv[1])=="--emit") {
    auto s=State();auto c=Context();Chain(*s,c);std::cout<<d::Cut(*s,token)<<'\n';return 0;
  }
  RealConfigReaderTests();
  Check(!api::Enabled(),"default off");Reject([]{api::Freeze(token);},"freeze before begin");
  Check(rex::cvar::SetFlagByName("mcla_timer_config_probe","true"),"explicit init option");
  LARGE_INTEGER qpf{};Check(QueryPerformanceFrequency(&qpf)!=FALSE,"QPF");const auto freq=uint64_t(qpf.QuadPart);
  Reject([&]{api::Begin(100,200,freq,token);},"begin requires install");
  d::installed=true; // Test-only: production requires exact five dispatcher wrappers.
  for(auto bad:{std::string("bad"),std::string(32,'0'),std::string(32,'A')})
    Reject([&]{api::Begin(100,200,freq,bad);},"invalid token");
  Reject([&]{api::Begin(0,200,freq,token);},"zero begin");
  Reject([&]{api::Begin(200,100,freq,token);},"reversed window");
  Reject([&]{api::Begin(100,101+180*freq,freq,token);},"over180seconds");
  Reject([&]{api::Begin(100,200,freq+1,token);},"frequency mismatch");
  {auto c=Context();d::Invoke(nullptr,d::Kind::Constructor,Original,c,Base());
   Check(d::before_begin,"early enabled hook recorded");Reject([&]{api::Begin(100,200,freq,token);},"early hook blocks begin");d::before_begin=false;}
  {
    auto s=State();auto c=Context();Register(*s,c);auto j=Cut(*s);auto r=j["records"][0];
    Check(calls==1&&config_reads==1&&timer_reads==0,"constructor original once, POST only");
    Check(r["config_before"].is_null()&&r["config_after"]["byte100"]==255,"constructor never reads uninitialized PRE");
    Check(r["constructor_registered"]==true&&r["constructor_observation_id"]==1,"successful constructor association");
    Check(r["incoming"]["r3"]==0xffffffff00001000ull&&r["incoming"]["f1_bits"]==0x7ff8000000000001ull,
          "raw integer and NaN FPR bits retained");
    Check(r["incoming"]["f2_bits"]==0x8000000000000000ull,"signed zero opaque bits");
    Check(r["entry_serial"]==1&&r["exit_serial"]==2&&j["counts"]["event_serial"]==2,"entry exit serial conservation");
    for(auto flag:{"native_admitted","configuration_route_qualified","configuration_persistence_proven",
        "object_lifetime_proven","state_coherence_proven","timing_binding_proven","simulation_proven","fps_proven"})
      Check(j[flag]==false,"no promoted proof flags");
    const auto frozen=d::Cut(*s,token);Call(*s,d::Kind::Constructor,c);Check(d::Cut(*s,token)==frozen,"immutable cut after later original");
  }
  {
    auto s=State();auto c=Context();s->clock=EqualClock;Chain(*s,c);auto j=Cut(*s);auto rows=j["records"];
    Check(rows.size()==5&&config_reads==5&&timer_reads==6,"five-hook nested inventory snapshots");
    Check(rows[1]["route_class"]=="unclassified"&&j["observer_failed"]==true,"initializer explicitly unclassified blocker");
    Check(rows[2]["lexical_parent_id"]==2&&rows[3]["lexical_parent_id"]==3&&rows[4]["lexical_parent_id"]==3,"immediate lexical chain");
    Check(rows[2]["route_class"]=="known"&&rows[3]["route_class"]=="known"&&rows[4]["route_class"]=="known","known static apply and setters under exact parent");
    Check(rows[1]["entry_serial"]<rows[2]["entry_serial"]&&rows[2]["exit_serial"]<rows[1]["exit_serial"],"equal QPC nesting carried by serials");
    Check(rows[3]["config_before"].is_null()&&rows[3]["config_after"].is_null(),"setters read timer not config");
    Check(rows[2]["timer_after"].is_object()&&rows[3]["timer_after"].is_object(),"apply and setter POST are distinct retained samples");
  }
  for(auto kind:{d::Kind::Initializer,d::Kind::Apply,d::Kind::RawSetter,d::Kind::FixedSetter}) {
    auto s=State();auto c=Context();c.lr=kind==d::Kind::Apply?0x822D26C4:0x822D24CC;Call(*s,kind,c);auto j=Cut(*s);
    Check(config_reads==0&&timer_reads==0&&j["records"][0]["route_class"]=="unexpected","unregistered/standalone route never reads");
  }
  {
    auto s=State();auto c=Context();Register(*s,c);c.lr=0x822D26C4;Call(*s,d::Kind::Apply,c);auto j=Cut(*s);
    Check(j["records"][1]["route_class"]=="unexpected"&&config_reads==1,"registered but standalone apply rejected");
  }
  for(int failure:{1,2}) {
    auto s=State();auto c=Context();read_mode=failure;Register(*s,c);auto j=Cut(*s);
    Check(j["records"][0]["config_after"].is_null()&&j["counts"]["read_failures"]==1,"failed or throwing reader clears snapshot");
    Check(s->associations_used==0&&j["observer_failed"]==true,"failed constructor read cannot register");
  }
  {
    auto s=State();auto c=Context();mode=3;Call(*s,d::Kind::Constructor,c);auto j=Cut(*s);
    Check(config_reads==0&&s->associations_used==0&&j["counts"]["binding_mismatches"]==1,"changed constructor result conservative rejection");
  }
  {
    auto s=State();auto c=Context();auto expected=c;expected.r7.u64=99;corrupt_context=&c;mode=8;
    const auto csr=_mm_getcsr();SetLastError(77);Call(*s,d::Kind::Constructor,c);
    Check(std::memcmp(&c,&expected,sizeof(c))==0&&GetLastError()==1234&&_mm_getcsr()==(csr^0x4000),"observer preserves all PPC bytes and legitimate original FP/error effects");
    _mm_setcsr(csr);corrupt_context=nullptr;
  }
  {
    auto s=State();auto c=Context();mode=1;const auto csr=_mm_getcsr();bool exact=false;try{Call(*s,d::Kind::Constructor,c);}catch(int n){exact=n==42;}
    Check(exact&&calls==1&&c.r7.u64==99&&GetLastError()==1234&&_mm_getcsr()==(csr^0x4000),"original exception exact once with PPC/error/FP effects propagated");
    _mm_setcsr(csr);
    auto j=Cut(*s);Check(j["counts"]["original_exceptions"]==1&&j["records"][0]["normal_return"]==false&&config_reads==0,"exception closes row without POST read");
    Check(d::stack.depth==0,"exception pops lexical state");
  }
  for(auto clock:{ZeroClock,ThrowClock}) {
    auto s=State();auto c=Context();s->clock=clock;Register(*s,c);auto j=Cut(*s);
    Check(calls==1&&config_reads==0&&j["counts"]["invalid_clock_entries"]==1&&j["clock_failed"]==true,"invalid clock blocks observer not original");
  }
  for(int fault=0;fault<5;++fault) {
    auto s=State();auto c=Context();Register(*s,c);d::Lexical parent;
    parent.Push(s.get(),d::Kind::Initializer,c,Base());parent.frame.id=1;
    parent.frame.route=d::Route::Unclassified;parent.frame.ctor=1;parent.frame.config=c.r3.u32;
    if(fault==0)parent.frame.kind=d::Kind::Constructor;
    if(fault==1)parent.frame.context=nullptr;
    if(fault==2)parent.frame.base=nullptr;
    if(fault==3)parent.frame.tid=0;
    if(fault==4)parent.frame.config+=4;
    c.lr=0x822D26C4;Call(*s,d::Kind::Apply,c);parent.Pop();auto j=Cut(*s);
    Check(j["records"][1]["route_class"]=="unexpected"&&config_reads==1&&timer_reads==0,
          "wrong immediate parent kind/context/base/thread/config never authorizes apply reads");
  }
  {
    auto s=State();auto c=Context();Register(*s,c);
    d::Lexical ancestor,mask;ancestor.Push(s.get(),d::Kind::Apply,c,Base());
    ancestor.frame.id=1;ancestor.frame.route=d::Route::Known;ancestor.frame.config=c.r3.u32;ancestor.frame.ctor=1;
    mask.Push(s.get(),d::Kind::Constructor,c,Base());mask.frame.id=0;
    c.r3.u32=d::kTimer;c.lr=0x822D24CC;Call(*s,d::Kind::RawSetter,c);mask.Pop();ancestor.Pop();auto j=Cut(*s);
    Check(j["records"][1]["route_class"]=="unexpected"&&j["records"][1]["parent_binding_matches"]==false&&timer_reads==0,
          "unadmitted immediate scope masks known ancestor, never nearest-parent search");
  }
  {
    auto s=State();auto c=Context();mode=4;Call(*s,d::Kind::Constructor,c);mode=0;ticks=200;Register(*s,c);auto j=Cut(*s);
    Check(config_reads==0&&j["clock_failed"]==true&&j["records"].size()==2,"sticky rollback forbids subsequent constructor POST reads");
  }
  {
    auto s=State();auto c=Context();ticks=99;Register(*s,c);ticks=s->data.end;Register(*s,c);auto j=Cut(*s);
    Check(j["counts"]["outside_before_entries"]==1&&j["counts"]["outside_after_entries"]==1&&config_reads==0,"window partitions no reads");
  }
  {
    auto s=State();auto c=Context();mode=5;Call(*s,d::Kind::Constructor,c);auto j=Cut(*s);
    Check(j["counts"]["stored_entries"]==8&&j["counts"]["depth_overflow_entries"]==2&&calls==10,"bounded lexical depth does not skip originals");
    Check(d::stack.depth==0&&j["observer_failed"]==true,"depth overflow blocks and unwinds");
  }
  {
    auto s=State();auto c=Context();for(size_t i=0;i<d::kCapacity+1;++i)Register(*s,c);auto j=Cut(*s);
    Check(j["records"].size()==4096&&j["counts"]["overflow_entries"]==1&&calls==4097,"bounded row overflow");
  }
  {
    auto s=State();for(uint32_t i=0;i<129;++i){auto c=Context(0x1000+128*i);Register(*s,c);}auto j=Cut(*s);
    Check(s->associations_used==128&&j["counts"]["registry_overflow"]==1&&j["records"][128]["constructor_registered"]==false,"bounded address registry");
  }
  {
    auto s=State();s->data.counts.observed=UINT64_MAX;auto c=Context();Register(*s,c);auto j=Cut(*s);
    Check(j["counts"]["counter_saturated"]==true&&j["observer_failed"]==true&&config_reads==0,"saturation prevents reads");
  }
  for(bool in_reader:{false,true}) {
    auto s=State();auto c=Context();std::thread worker([&]{mode=in_reader?0:2;read_mode=in_reader?3:0;Call(*s,d::Kind::Constructor,c);});
    Wait([]{return entered.load();});const auto frozen=d::Cut(*s,token);auto j=Json::parse(frozen);
    Check(j["counts"]["pending_entries"]==1&&j["counts"]["event_serial"]==1&&j["observer_failed"]==true,"freeze pending original or unlocked reader");
    release_work=true;worker.join();Check(d::Cut(*s,token)==frozen&&s->associations_used==0,"postfreeze return cannot mutate or register");
  }
  {
    auto s=State();std::thread a([&]{auto c=Context(0x1000);Register(*s,c);});std::thread b([&]{auto c=Context(0x2000);Register(*s,c);});a.join();b.join();auto j=Cut(*s);
    Check(j["counts"]["completed_entries"]==2&&j["counts"]["event_serial"]==4&&s->associations_used==2,"concurrent threads independent lexical stacks and bounded registry");
    Check(j["records"][0]["lexical_parent_id"]==0&&j["records"][1]["lexical_parent_id"]==0,"never borrows another thread parent");
  }
  {
    auto s=State();auto c=Context();Reject([&]{d::Cut(*s,"bad");},"wrong freeze token");Check(!s->frozen,"wrong token nonmutating");
    d::configured=s.get();for(auto fn:{sub_822D2818,sub_822D2548,sub_822D2478,sub_821BD9B8,sub_821BD9F8}){const auto n=calls.load();fn(c,Base());Check(calls==n+1,"each strong wrapper exact once");}d::configured=nullptr;
  }
  {
    LARGE_INTEGER now{};QueryPerformanceCounter(&now);const auto begin=uint64_t(now.QuadPart);
    api::Begin(begin,begin+freq,freq,token);Reject([&]{api::Begin(begin,begin+freq,freq,token);},"second Begin");
    auto* s=d::configured.load();SetLastError(888);api::Disarm();Check(GetLastError()==888&&s->stopped,"terminal disarm preserved error");
    Reject([&]{api::Begin(begin,begin+freq,freq,token);},"begin after disarm");auto frozen=api::Freeze(token);
    Check(Json::parse(frozen)["disarmed_before_freeze"]==true&&api::Freeze(token)==frozen,"public immutable freeze after disarm");
  }
  std::cout<<"timer config: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
