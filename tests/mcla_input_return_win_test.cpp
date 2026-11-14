// Guest-free: actual strong wrapper, synthetic retained original/context and
// owned pages only. Install's dispatcher binding predicate is tested separately;
// this executable never constructs Runtime, resumes a guest or writes evidence.
#include "../src/mcla_input_return_win.cpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
namespace api=mcla::input_return;
namespace d=api::detail;
using Json=nlohmann::json;
constexpr char token[]="0123456789abcdef0123456789abcdef";
std::atomic<uint64_t> tick{100},calls{},reads{};
std::atomic<bool> entered{},release_original{};
int original_mode{},read_mode{};
thread_local PPCContext* disturb_context{};
unsigned checks{},failures{};
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
template<class F> void Reject(F&& f,const char* label){bool caught=false;try{f();}catch(...){caught=true;}Check(caught,label);}
template<class F> void Wait(F&& f) {
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!f()){if(std::chrono::steady_clock::now()>=end)std::abort();std::this_thread::yield();}
}
void Block(){entered=true;Wait([]{return release_original.load();});}
uint64_t Clock(){SetLastError(991);return tick.fetch_add(1);}
uint64_t ThrowClock(){throw 70;}
bool Reader(uintptr_t,uint32_t address,d::ReturnedState& out) {
  ++reads;out={0x01020304,0x9010};SetLastError(992);_mm_setcsr(_mm_getcsr()^0x2000);
  if(disturb_context)disturb_context->r31.u64=0xbad;
  if(address!=0x1000)throw 72;
  if(read_mode==2)throw 71;
  if(read_mode==3)Block();
  return read_mode!=1;
}
std::unique_ptr<d::State> State() {
  auto s=std::make_unique<d::State>();s->data.token=token;s->data.begin=100;
  s->data.end=30100;s->last_clock=100;s->clock=Clock;s->reader=Reader;
  tick=100;calls=reads=0;entered=false;release_original=false;
  original_mode=read_mode=0;disturb_context=nullptr;return s;
}
PPCContext Context(uint32_t address=0x1000,uint64_t lr=0x82465F0C,uint32_t user=0) {
  PPCContext c{};c.lr=lr;c.r3.u64=0x1122334400000000ull|user;
  c.r4.u64=0xaabbccdd00000000ull|address;c.r31.u64=99;return c;
}
void Original(PPCContext& ctx,uint8_t*) {
  ++calls;
  ctx.r5.u64=ctx.r4.u64;ctx.r4.s64=1;ctx.r3.u64=original_mode==1?1167:0;
  ctx.r7.u64=0xabc;SetLastError(1234);_mm_setcsr(_mm_getcsr()^0x4000);
  if(original_mode==2)throw 42;
  if(original_mode==3)Block();
  if(original_mode==4)tick=99;
  if(original_mode==5)tick=30100;
}
void Call(d::State* s,PPCContext& ctx) {
  d::configured.store(s);disturb_context=&ctx;
  sub_8275CB78(ctx,reinterpret_cast<uint8_t*>(1));
  disturb_context=nullptr;d::configured.store(nullptr);
}
Json Cut(d::State& s){return Json::parse(d::Cut(s,token));}
void CompareOriginal(bool enabled) {
  auto s=State();auto actual=Context(),expected=actual;
  const auto csr=_mm_getcsr();SetLastError(19);Original(expected,nullptr);
  const auto expected_error=GetLastError();
  const auto expected_csr=_mm_getcsr();calls=0;
  _mm_setcsr(csr);SetLastError(19);Call(enabled?s.get():nullptr,actual);
  Check(calls==1,"retained original exactly once");
  Check(std::memcmp(&actual,&expected,sizeof(actual))==0,"all PPC output bytes preserved");
  Check(GetLastError()==expected_error,"original LastError preserved");
  Check(_mm_getcsr()==expected_csr,"original MXCSR preserved");_mm_setcsr(csr);
  if(enabled) {
    const auto j=Cut(*s);const auto& r=j["rows"][0];
    Check(j["observer_failed"]==false&&j["pending"]==0,"healthy completed cut");
    Check(r["packet"]==0x01020304&&r["buttons"]==0x9010&&r["start_down"]==true,"BE state and START retained");
    Check(r["output_is_null"]==false&&r["flags"]==1&&r["user_index"]==0,"entry arguments precede thunk mutation");
    Check(r["status"]==0&&r["normal_return"]==true&&reads==1,"successful status copied once");
    Check(j["frontend_consumption_proven"]==false&&j["whole_xam_coverage_proven"]==false&&
          j["scene_proven"]==false&&j["physical_speed_proven"]==false&&j["fps_proven"]==false,"no downstream proof claims");
  }else Check(reads==0,"disabled never reads output");
}
void InventoryTests() {
  {auto s=State();auto c=Context(0);Call(s.get(),c);auto j=Cut(*s);
   Check(j["rows"][0]["output_is_null"]==true&&reads==0,"null query recorded without read");
   Check(j["rows"][0]["packet"].is_null()&&j["rows"][0]["start_down"].is_null(),"null query has no invented state");}
  {auto s=State();original_mode=1;auto c=Context();Call(s.get(),c);auto j=Cut(*s);
   Check(j["rows"][0]["status"]==1167&&reads==0&&j["observer_failed"]==false,"nonzero return preserves status without stale buffer");}
  for(const auto user:{1u,3u,255u,UINT32_MAX}) {
    auto s=State();auto c=Context(0x1000,0x82614D60,user);Call(s.get(),c);auto j=Cut(*s);
    Check(j["rows"][0]["user_index"]==user&&j["rows"][0]["caller_known"]==true,"all users preserved, second exact caller known");
  }
  {auto s=State();auto c=Context(0x1000,0x1000000082465F0Cull);Call(s.get(),c);auto j=Cut(*s);
   Check(j["counts"]["unknown_callers"]==1&&j["observer_failed"]==true,"unknown high LR not truncated into known route");
   Check(j["rows"][0]["caller_lr"]==0x1000000082465F0Cull,"raw unknown LR retained");}
  for(int failure:{1,2}) {
    auto s=State();read_mode=failure;auto c=Context();Call(s.get(),c);auto j=Cut(*s);
    Check(calls==1&&j["counts"]["read_failures"]==1&&j["observer_failed"]==true,"failed or throwing reader never retries original");
    Check(j["rows"][0]["state_available"]==false&&j["rows"][0]["buttons"].is_null(),"failed read never leaks partially filled values");
  }
  {auto s=State();original_mode=2;auto c=Context();const auto csr=_mm_getcsr();bool exact=false;
   try{Call(s.get(),c);}catch(int e){exact=e==42;}d::configured=nullptr;disturb_context=nullptr;
   Check(exact&&calls==1&&c.r7.u64==0xabc,"same original exception and mutations propagate");
   Check(GetLastError()==1234&&_mm_getcsr()==(csr^0x4000),"exception LastError/MXCSR preserved");_mm_setcsr(csr);
   auto j=Cut(*s);Check(j["counts"]["exception_returns"]==1&&j["rows"][0]["status"].is_null()&&reads==0,"exception row has no returned status/state");}
  {auto s=State();s->clock=ThrowClock;auto c=Context();Call(s.get(),c);auto j=Cut(*s);
   Check(calls==1&&j["emergency"]==true&&j["observer_failed"]==true,"observer clock throw never prevents original");}
}
void CutTests() {
  for(int blocked:{0,1}) {
    auto s=State();if(blocked)read_mode=3;else original_mode=3;
    std::thread worker([&]{auto c=Context();Call(s.get(),c);});Wait([]{return entered.load();});
    const auto first=d::Cut(*s,token);const auto j=Json::parse(first);
    Check(j["pending"]==1&&j["observer_failed"]==true&&j["rows"][0]["exit_ms"].is_null(),"cut retains original/reader in-flight as pending");
    release_original=true;worker.join();
    Check(first==d::Cut(*s,token)&&calls==1,"late completion cannot alter immutable cut");
  }
  {auto s=State();s->data.end=1000000;for(size_t i=0;i<d::kCapacity+1;++i){auto c=Context();Call(s.get(),c);}
   auto j=Cut(*s);Check(j["rows"].size()==d::kCapacity&&j["counts"]["overflow"]==1,"bounded rows and explicit overflow");
   Check(calls==d::kCapacity+1&&j["counts"]["observed"]==d::kCapacity+1,"overflow still passes every original exactly once");}
  {auto s=State();tick=99;auto c=Context();Call(s.get(),c);auto j=Cut(*s);
   Check(j["counts"]["invalid_entry"]==1&&j["clock_failed"]==true&&j["rows"].empty(),"entry rollback fails without fabricated row");}
  {auto s=State();original_mode=4;auto c=Context();Call(s.get(),c);auto j=Cut(*s);
   Check(j["counts"]["invalid_exit"]==1&&j["rows"][0]["exit_ms"]==99,"actual rollback exit retained");}
  {auto s=State();original_mode=5;auto c=Context();Call(s.get(),c);auto j=Cut(*s);
   Check(j["expired"]==true&&j["counts"]["exit_outside"]==1,"postdeadline return explicit not shifted");}
  {auto s=State();tick=30100;auto c=Context();Call(s.get(),c);auto j=Cut(*s);
   Check(j["expired"]==true&&j["counts"]["outside_after"]==1&&calls==1,"expired entry remains passthrough");}
  {auto s=State();Reject([&]{d::Cut(*s,"bad");},"wrong token rejects without cut");
   Check(!s->stopped,"wrong token does not freeze");}
}
void RealReaderTests() {
  SYSTEM_INFO info{};GetSystemInfo(&info);const size_t page=info.dwPageSize;
  auto* memory=static_cast<uint8_t*>(VirtualAlloc(nullptr,2*page,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
  if(!memory)std::abort();
  constexpr uint32_t guest=0x1000;auto* exact=memory+page-16;
  exact[0]=0x89;exact[1]=0xab;exact[2]=0xcd;exact[3]=0xef;exact[4]=0x80;exact[5]=0x10;
  d::ReturnedState out{};const auto base=reinterpret_cast<uintptr_t>(exact)-guest;
  Check(d::ReadState(base,guest,out)&&out.packet==0x89abcdef&&out.buttons==0x8010,"real exact 16-byte boundary read");
  DWORD old{};if(!VirtualProtect(memory+page,page,PAGE_NOACCESS,&old))std::abort();
  out={123,123};Check(!d::ReadState(base+1,guest,out)&&!out.packet&&!out.buttons,"partial protected read zeroes output");
  out={123,123};Check(!d::ReadState(reinterpret_cast<uintptr_t>(memory+page)-guest,guest,out)&&!out.packet&&!out.buttons,"fully protected read contained");
  Check(!d::ReadState(0,guest,out)&&!d::ReadState(base,0,out)&&!d::ReadState(base,UINT32_MAX,out),"invalid base/null/overflow rejected");
  Check(!d::ReadState(base,0xdffffff8,out),"physical-view crossing rejected");
  Check(!d::ReadState(UINTPTR_MAX-2,guest,out),"host pointer overflow rejected");
  const uint32_t physical=0xe0000000;
  Check(d::ReadState(reinterpret_cast<uintptr_t>(exact)-physical-0x1000,physical,out)&&out.packet==0x89abcdef,"physical guest view uses SDK 4096 offset");
  Check(VirtualFree(memory,0,MEM_RELEASE)!=0,"only owned allocation released");
}
void LifecycleTests() {
  Check(!api::Enabled(),"default disabled");
  Reject([&]{api::Begin(token,30000);},"Begin requires Enable and Install");
  Reject([&]{api::Freeze(token);},"no invented receipt before Begin");
  d::VerifyBinding(sub_8275CB78);
  Reject([&]{d::VerifyBinding(__imp__sub_8275CB78);},"retained body is not strong wrapper binding");
  Reject([&]{d::VerifyBinding(nullptr);},"missing dispatcher binding rejected");
  api::Enable();Check(api::Enabled(),"explicit Enable");
  Reject([&]{api::Enable();},"Enable one shot");
  Reject([&]{api::Begin(token,30000);},"Begin still requires Install");
  // Only bypass Runtime construction in this guest-free API-ordering test.
  // The production Install itself is not claimed tested by these assignments.
  d::installed=true;
  Reject([&]{api::Begin("bad",30000);},"token strict");
  Reject([&]{api::Begin(token,29999);},"duration exact 30000");
  auto c=Context();Call(nullptr,c);Check(d::configured.load()==nullptr,"admitted pre-Begin calls remain unrecorded");
  api::Begin(token,30000);Reject([&]{api::Begin(token,30000);},"Begin one shot");
  const auto text=api::Freeze(token);auto j=Json::parse(text);
  Check(j["end_ms"].get<uint64_t>()-j["begin_ms"].get<uint64_t>()==30000,"actual API exact window");
  api::Disarm();Check(text==api::Freeze(token),"Disarm after cut preserves receipt");
  Reject([&]{api::Begin(token,30000);},"Disarm terminal");
  // A separate synthetic State verifies disarm-before-cut marking.
  auto s=State();d::configured=s.get();api::Disarm();auto cut=Cut(*s);
  Check(cut["disarmed_before_cut"]==true&&cut["observer_failed"]==true,"early Disarm cannot masquerade healthy");
  d::configured=nullptr;
}
}
REX_EXTERN(__imp__sub_8275CB78){Original(ctx,base);}
int main(int argc,char** argv) {
  if(argc==2&&std::string(argv[1])=="--emit") {
    auto s=State();auto c=Context();Call(s.get(),c);std::cout<<d::Cut(*s,token)<<'\n';return 0;
  }
  if(argc!=1)return 2;
  CompareOriginal(false);CompareOriginal(true);InventoryTests();CutTests();RealReaderTests();LifecycleTests();
  std::cout<<"INPUT_RETURN_SYNTHETIC checks="<<checks<<" failures="<<failures<<'\n';
  return failures?1:0;
}
