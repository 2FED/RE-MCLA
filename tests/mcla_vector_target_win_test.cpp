// Synthetic host-only adapter checks. No game, GPU, file or guest-memory IO.
#include "../src/mcla_vector_target_win.cpp"
#include "mcla_no_phase_boundary_stub.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
namespace a=mcla::vector_target_win; namespace d=a::detail;
namespace core=mcla::vector_target; namespace w=mcla::work_emission;
namespace vd=mcla::virtual_detail; namespace cd=mcla::consumer_detail;
using Json=nlohmann::json;
constexpr char token[]="0123456789abcdef0123456789abcdef";
std::atomic<uint64_t> ticks{100},reads{},calls{};
thread_local d::Access association{};
thread_local const PPCContext* expected_context{};
bool parent_enabled=true;uint32_t body_version=1;
unsigned checks{},failures{};
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
template<class F>void Rejects(F&& f,const char* label){bool threw=false;try{f();}catch(...){threw=true;}Check(threw,label);}
void Clobber(){SetLastError(1001);_mm_setcsr(_mm_getcsr()^0x6000u);}
uint64_t Clock(){Clobber();return ticks.fetch_add(1);}
uint64_t Zero(){return 0;}
uint64_t ThrowClock(){throw 77;}
d::Access Read(const PPCContext&,const uint8_t*){++reads;Clobber();return association;}
d::Access CheckedRead(const PPCContext& c,const uint8_t* b){
 ++reads;Clobber();if(&c==expected_context&&b==reinterpret_cast<const uint8_t*>(1))return association;
 d::Access absent;absent.status=mcla::work_emission_win::VectorCallStatus::kLaneMismatch;return absent;
}
d::Access ThrowRead(const PPCContext&,const uint8_t*){++reads;Clobber();throw 44;}
d::Access CutRead(const PPCContext&,const uint8_t*){++reads;a::Freeze(token);return association;}
d::Access CutThrow(const PPCContext&,const uint8_t*){++reads;a::Freeze(token);throw 55;}
uint8_t* Base(){return reinterpret_cast<uint8_t*>(1);}
PPCContext Context(){PPCContext c{};c.lr=core::kCaller;c.r3.u64=0xffeeddccffffff00ull;
 c.v1.u32[0]=0x7fc01234;c.v1.u32[1]=0x80000000;c.v1.u32[2]=0xffffffff;c.v1.u32[3]=1;return c;}
void Bind(const PPCContext& c,uint64_t serial=1,uint64_t epoch=11,uint32_t event=10,uint64_t ordinal=3){
 association={};association.status=mcla::work_emission_win::VectorCallStatus::kValid;
 auto& e=association.entry;e.invocation={epoch,serial};
 cd::Entry p{{22,serial},{33,serial},{44,55,GetCurrentThreadId()},cd::kTarget,cd::kCaller,12,13};
 cd::Child child{vd::kCaller,14,15,0xabcdef008258d1a0ull,vd::kTarget,cd::ChildState::kActive,true};
 e.virtual_entry={{21,serial},p,child,vd::kTarget,vd::kCaller,14,15};
 e.active_call0={w::kCaller,16,17,0xaabbccdd82741010ull,w::kTail,vd::CallState::kActive};
 e.actual_target=w::kBody;e.caller_lr=w::kCaller;e.r3=16;e.f1_bits=17;
 association.vector.fields={core::kCaller,ordinal,c.r3.u64,0x7ff8000012345678ull,0xaabbccdd8259f300ull};
 for(unsigned k=0;k<4;++k)association.vector.vector[k]=c.v1.u32[k];
 association.event_index=event;association.ordinal=ordinal;
}
struct Fixture {
 std::unique_ptr<d::State> s;
 Fixture():s(d::Allocate(100,1000000,100000,token)){ticks=100;reads=0;calls=0;d::local={};d::active=nullptr;association={};d::disarmed=false;s->clock=Clock;s->read=Read;d::configured.store(s.get());}
 ~Fixture(){d::configured=nullptr;d::local={};d::active=nullptr;}
};
Json Cut(){return Json::parse(a::Freeze(token));}
void Original(PPCContext& c,core::Site site,int throwing){
 const int i=int(site);++calls;c.r3.u64=0xabcdef0000000000ull+unsigned(i);
 c.r5.u64+=3;c.f1.u64^=0x10000000000000ull;
 for(unsigned k=0;k<4;++k)c.v1.u32[k]^=0x12340000u+unsigned(i)+k;
 SetLastError(700+i);_mm_setcsr((_mm_getcsr()&~0x6000u)|((unsigned(i)%4u)<<13));
 if(i==throwing)throw 80+i;
}
void Body(PPCContext& c,a::Scope* scope,bool skip100,uint32_t loaded,int throwing=-1){
 const auto entry_r3=c.r3.u64;const core::Vector entry_v1{c.v1.u32[0],c.v1.u32[1],c.v1.u32[2],c.v1.u32[3]};
 c.r31.u64=entry_r3;for(unsigned k=0;k<4;++k)c.v126.u32[k]=0x7fc10000u+k;
 if(scope)scope->InitialV126((c.r31.u32+400u)&~15u);
 auto invoke=[&](core::Site site){
  c.r3.u64=entry_r3;c.lr=core::Caller(site);c.ctr.u64=0xaabbccdd12340000ull+unsigned(site);
  if(site==core::Site::kSlot48||site==core::Site::kSlot28||site==core::Site::kSlot104)
   for(unsigned k=0;k<4;++k)c.v1.u32[k]=entry_v1[k];
  if(scope)scope->Call(site,[&]{Original(c,site,throwing);});else Original(c,site,throwing);
 };
 invoke(core::Site::kDirectE790);invoke(core::Site::kSlot48);invoke(core::Site::kSlot28);invoke(core::Site::kDirectD1E8);
 c.r3.u64=1;if(scope)scope->FlagStored();
 c.r10.u64=skip100?16:0;if(scope)scope->Gate100();
 if(!skip100)invoke(core::Site::kSlot100);
 invoke(core::Site::kSlot104);
 c.r11.u64=loaded;if(scope)scope->CounterLoaded();
 c.r11.u64++;if(scope)scope->CounterStored();
 const auto low=c.r11.u32;if(low>32&&low<0x80000000u)invoke(core::Site::kSlot192);
}
void Run(PPCContext& c,bool skip100,uint32_t loaded,bool wrapped=true,int throwing=-1){
 if(wrapped){a::Scope scope(c,Base());Body(c,&scope,skip100,loaded,throwing);}else Body(c,nullptr,skip100,loaded,throwing);
}
void Routes(){for(bool skip:{false,true})for(uint32_t loaded:{0u,31u,32u,0x7fffffffu,0xffffffffu}){
 Fixture f;auto c=Context();Bind(c);Run(c,skip,loaded);auto j=Cut();
 Check(j["observer_failed"]==false&&j["counts"]["admitted_entries"]==1,"complete route healthy");
 Check(j["threads"][0]["pending_count"]==0,"normal complete stack");
 Check(j["schema"]=="mcla-vector-target-v2"&&j["independent_cut"]==true,"separate actual target journal");
 Check(j["native_admitted"]==false&&j["simulation_proven"]==false&&j["total_work_proven"]==false,"no native simulation or totalwork proof");
 Check(j["entries_per_thread_capacity"]==d::kEntries&&j["events_per_thread_capacity"]==d::kEvents&&j["json_limit_bytes"]==d::kJson,"declared bounds exact");
 bool load=false,store=false;unsigned call100=0,call192=0;
 for(const auto& e:j["threads"][0]["events"]){
  if(e["event"]==uint8_t(core::EventKind::kCallPre)){call100+=e["site"]==uint8_t(core::Site::kSlot100);call192+=e["site"]==uint8_t(core::Site::kSlot192);}
  if(e["event"]==uint8_t(core::EventKind::kCounterLoad)){load=true;Check(e["fields"][1]==loaded,"raw load retained");}
  if(e["event"]==uint8_t(core::EventKind::kCounterStore)){store=true;Check(e["fields"][2]==uint64_t(loaded)+1,"full increment including overflow retained");}
 }
 const uint32_t low=loaded+1;Check(load&&store,"separate load and successful store");Check(call100==(!skip?1u:0u)&&call192==((low>32&&low<0x80000000u)?1u:0u),"gates retain actual branch semantics");
 }}
void Preservation(){for(bool skip:{false,true})for(int throwing=-1;throwing<7;++throwing){
 Fixture f;auto seed=Context();PPCContext expected{},actual{};std::memcpy(&expected,&seed,sizeof(seed));std::memcpy(&actual,&seed,sizeof(seed));Bind(actual);
 const auto csr=_mm_getcsr();SetLastError(9);int first=0,second=0;
 try{Run(expected,skip,32,false,throwing);}catch(int e){first=e;}
 const auto error=GetLastError();const auto aftercsr=_mm_getcsr();const auto directcalls=calls.load();calls=0;SetLastError(9);_mm_setcsr(csr);
 try{Run(actual,skip,32,true,throwing);}catch(int e){second=e;}
 Check(std::memcmp(&expected,&actual,sizeof(actual))==0,"all PPC bytes preserved");Check(GetLastError()==error&&_mm_getcsr()==aftercsr,"LastError and MXCSR preserved");
 Check(first==second&&calls==directcalls,"each original once same exception");Check(d::active==nullptr,"TLS restored on all exits");
 Check(Cut()["observer_failed"]==false,"original exception not observation failure");_mm_setcsr(csr);
 }}
void Admission(){
 for(unsigned bad=0;bad<9;++bad){Fixture f;auto c=Context();Bind(c);
  if(bad==0)association.status=mcla::work_emission_win::VectorCallStatus::kAbsent;
  if(bad==1)c.lr++;
  if(bad==2)c.r3.u64++;
  if(bad==3)c.v1.u32[0]++;
  if(bad==4)association.vector.fields[4]++;
  if(bad==5)association.ordinal++;
  if(bad==6)association.entry.virtual_entry.parent_entry.update.native_thread_id++;
  {a::Scope scope(c,bad==7?nullptr:Base(),bad==8?core::kTarget+4:core::kTarget);}
  Check(Cut()["counts"]["dropped_entries"]==1,"invalid body association rejected");
 }
 for(bool wrong_context:{false,true}){Fixture f;auto c=Context();Bind(c);expected_context=&c;f.s->read=CheckedRead;
  auto other=c;{a::Scope s(wrong_context?other:c,wrong_context?Base():reinterpret_cast<uint8_t*>(2));}
  Check(Cut()["counts"]["dropped_entries"]==1,"accessor context/base mismatch not bypassed");expected_context=nullptr;}
 {Fixture f;auto c=Context();Bind(c);Run(c,true,0);c=Context();Run(c,true,0);
  Check(Cut()["counts"]["admitted_entries"]==1,"same exact parentevent cannot be reused");}
 {Fixture f;auto c=Context();Bind(c);Run(c,true,0);c=Context();Bind(c,1,11,12,0);Run(c,true,0);
  Check(Cut()["counts"]["admitted_entries"]==2,"sameparent descendingordinal gap accepted");}
 {Fixture f;auto c=Context();Bind(c);{a::Scope outer(c,Base());f.s->stopped=true;
  {a::Scope shadow(c,Base());f.s->stopped=false;c.r31.u64=c.r3.u64;outer.InitialV126((c.r31.u32+400u)&~15u);Check(d::active==&shadow,"unadmitted scope shadows outer");}
  Check(d::active==&outer,"lexical outer restored");}Check(Cut()["observer_failed"]==true,"cannot observe through shadow");}
 for(bool after:{false,true}){Fixture f;auto c=Context();Bind(c);ticks=after?f.s->end:1;Run(c,true,0);ticks=1000001;auto j=Cut();
  Check(j["counts"][after?"outside_after_entries":"outside_before_entries"]==1&&j["counts"]["admitted_entries"]==0,"outside window counted not admitted");}
 {Fixture f;auto c=Context();ticks=1;Run(c,true,0);ticks=100;c=Context();Bind(c);Run(c,true,0);Check(!f.s->failed,"prewindow absent parent does not poison valid entry");}
 for(auto clock:{Zero,ThrowClock}){Fixture f;auto c=Context();Bind(c);f.s->clock=clock;Run(c,true,0);Check(Cut()["clock_failed"]==true,"clock failure retained");}
 {Fixture f;auto c=Context();Bind(c);{a::Scope s(c,Base());association.vector.fields[3]^=1;c.r31.u64=c.r3.u64;s.InitialV126((c.r31.u32+400u)&~15u);}Check(Cut()["counts"]["binding_mismatches"]!=0,"every boundary rechecks fullparent");}
 {Fixture f;auto c=Context();Bind(c);f.s->read=ThrowRead;Run(c,true,0);Check(f.s->failed&&f.s->emergency_failure,"reader exception sticky");}
 {Fixture f;for(size_t n=0;n<d::kRejected+1;++n){auto c=Context();a::Scope s(c,Base());}auto j=Cut();Check(j["rejected_entries"].size()==d::kRejected&&j["counts"]["rejection_overflow"]==1,"rejected inventory bounded");}
}
void Cuts(){
 for(auto reader:{CutRead,CutThrow}){Fixture f;auto c=Context();Bind(c);std::string frozen;
  {a::Scope s(c,Base());f.s->read=reader;c.r31.u64=c.r3.u64;s.InitialV126((c.r31.u32+400u)&~15u);frozen=a::Freeze(token);}
  Check(a::Freeze(token)==frozen&&!f.s->emergency_failure,"cut during parent read cannot backfill");}
 {Fixture f;auto c=Context();Bind(c);Run(c,true,0);const auto cut=a::Freeze(token);f.s->emergency_failure=true;a::Disarm();Check(a::Freeze(token)==cut,"late error/disarm leaves immutable cut");}
 {Fixture f;f.s->emergency_failure=true;Check(Cut()["observer_failed"]==true,"precut emergency flag retained");}
 {Fixture f;auto c=Context();Bind(c);a::Disarm();Run(c,true,0);Check(reads==0&&calls==5&&Cut()["disarmed_at_cut"]==true,"disarm stops observer not originals");}
 {Fixture f;d::Json j;j.out.assign(d::kJson,'x');Rejects([&]{j.add("x");},"hard JSON bound");}
 {Fixture f;auto c=Context();Bind(c);{a::Scope s(c,Base());ticks=1;c.r31.u64=c.r3.u64;s.InitialV126((c.r31.u32+400u)&~15u);}Check(Cut()["clock_failed"]==true,"backwards clock cannot silently pass");}
 {Fixture f;std::mutex mutex;std::condition_variable cv;bool ready=false,resume=false;
  std::thread worker([&]{auto c=Context();Bind(c);{a::Scope s(c,Base());c.r31.u64=c.r3.u64;s.InitialV126((c.r31.u32+400u)&~15u);c.lr=core::Caller(core::Site::kDirectE790);
   s.Call(core::Site::kDirectE790,[&]{++calls;std::unique_lock lock(mutex);ready=true;cv.notify_all();if(!cv.wait_for(lock,std::chrono::seconds(5),[&]{return resume;}))std::abort();SetLastError(719);});}
   if(GetLastError()!=719)std::abort();});
  {std::unique_lock lock(mutex);if(!cv.wait_for(lock,std::chrono::seconds(5),[&]{return ready;}))std::abort();}
  const auto saved=a::Freeze(token);{std::lock_guard lock(mutex);resume=true;}cv.notify_all();worker.join();
  Check(calls==1&&a::Freeze(token)==saved,"no observer lock spans original; no late backfill");
  Check(Json::parse(saved)["threads"][0]["pending"][0]["stage"]==uint8_t(core::Stage::kCallActive),"pending activecall retained literally");}
}
void Limits(){
 {Fixture f;for(size_t n=0;n<d::kEntries+1;++n){auto c=Context();Bind(c,n+1,11,uint32_t(n+1));try{a::Scope s(c,Base());throw 1;}catch(int){}}
  Check(f.s->lanes[0]->model.entry_count()==d::kEntries&&f.s->lanes[0]->model.fault()==core::Fault::kEntryCapacity,"entry limit preserves old records");}
 {Fixture f;uint64_t expected=0;for(size_t n=0;n<d::kEntries;++n){auto c=Context();Bind(c,n+1,11,uint32_t(n+1));Run(c,false,32);expected+=7;}
  Check(calls==expected&&f.s->lanes[0]->model.size()==d::kEvents&&f.s->lanes[0]->model.fault()==core::Fault::kEventCapacity,"event limit never drops original calls");}
 {Fixture f;auto c=Context();uint64_t serial=0;
  auto nest=[&](auto&& self,unsigned depth)->void{const auto old=association;const auto oldc=c;c=Context();++serial;Bind(c,serial,11,uint32_t(serial));
   try{a::Scope s(c,Base());if(depth)self(self,depth-1);throw 1;}catch(int){}association=old;c=oldc;};
  nest(nest,8);Check(f.s->lanes[0]->model.fault()==core::Fault::kDepth&&f.s->lanes[0]->model.pending_count()==8,"depth exhaustion retains pending stack");}
 {Fixture f;std::array<std::thread,8> workers;
  for(size_t n=0;n<workers.size();++n)workers[n]=std::thread([n]{auto c=Context();Bind(c,1,100+n);Run(c,true,0);});
  for(auto& t:workers)t.join();Check(f.s->used==8&&f.s->admitted==8&&!f.s->failed,"distinct thread lifetimes isolated");
  std::thread extra([]{auto c=Context();Bind(c,1,999);Run(c,true,0);});extra.join();Check(f.s->used==8&&f.s->dropped==1,"ninth lane cannot replace first");}
}
template<class F>void AtSlot28(PPCContext& c,F&& inspect,uint32_t requested=0x8259E768u){
 try {
  a::Scope scope(c,Base());c.r31.u64=c.r3.u64;
  scope.InitialV126((c.r31.u32+400u)&~15u);
  for(auto site:{core::Site::kDirectE790,core::Site::kSlot48}){
   c.lr=core::Caller(site);scope.Call(site,[]{});
  }
  c.lr=core::Caller(core::Site::kSlot28);c.ctr.u64=0xaabbccdd00000000ull|requested;
  scope.Call(core::Site::kSlot28,[&]{inspect(scope);});
  // Stop this deliberately partial synthetic body through an original exception.
  throw 919;
 }catch(int e){if(e!=919)throw;}
}
void Slot28Access(){
 using S=a::Slot28Status;
 Check(rex::cvar::SetFlagByName("mcla_vector_target_probe","true"),"accessor explicit enable");
 {Fixture f;auto c=Context();Bind(c);
  Check(a::ReadActiveSlot28(c,Base()).status==S::kAbsent,"accessor no lexical scope");
  AtSlot28(c,[&](a::Scope&){
   const auto saved=c;const auto before=d::Serialize(*f.s);const auto clock=ticks.load();
   SetLastError(37);const auto csr=_mm_getcsr();const auto access=a::ReadActiveSlot28(c,Base());
   Check(access.valid()&&access.status==S::kValid,"active slot28 admitted");
   Check(access.entry==f.s->lanes[0]->model.entries()[0]&&access.event_index==6,"exact entry and lane-global event index");
   Check(access.call==f.s->lanes[0]->model.events()[6].payload,"exact saved CallPre payload");
   Check(access.call.fields[0]==0x8259F364&&access.call.fields[1]==saved.r3.u64&&
     access.call.fields[2]==0xaabbccdd8259e768ull,"caller object and full CTR retained");
   Check(GetLastError()==37&&_mm_getcsr()==csr&&std::memcmp(&saved,&c,sizeof(c))==0,"accessor preserves LastError MXCSR all PPC bytes");
   Check(ticks==clock&&d::Serialize(*f.s)==before,"accessor no QPC or journal mutation");
   c.lr=1;c.r3.u64=2;c.ctr.u64=3;for(auto& v:c.v1.u32)v^=0x80000000u;
   Check(a::ReadActiveSlot28(c,Base())==access,"changed live registers do not replace saved CallPre");
   auto detached=access;detached.entry.r3++;detached.call.fields[2]++;
   Check(a::ReadActiveSlot28(c,Base())==access,"returned value is detached");c=saved;
   auto other=c;Check(a::ReadActiveSlot28(other,Base()).status==S::kLaneMismatch,"wrong PPC context rejected");
   Check(a::ReadActiveSlot28(c,nullptr).status==S::kLaneMismatch&&
     a::ReadActiveSlot28(c,reinterpret_cast<uint8_t*>(2)).status==S::kLaneMismatch,"wrong base rejected");
   const auto local=d::local;d::local={};Check(a::ReadActiveSlot28(c,Base()).status==S::kLaneMismatch,"wrong TLS lane rejected");d::local=local;
   auto* configured=d::configured.exchange(nullptr);Check(a::ReadActiveSlot28(c,Base()).status==S::kLaneMismatch,"replaced configuration rejected");d::configured=configured;
   const auto parent=association;association.vector.fields[3]^=1;
   Check(a::ReadActiveSlot28(c,Base()).status==S::kBindingMismatch,"changed saved ancestry rejected");association=parent;
   f.s->read=ThrowRead;const auto snapshot=d::Serialize(*f.s);
   Check(a::ReadActiveSlot28(c,Base()).status==S::kObserverException&&d::Serialize(*f.s)==snapshot,"reader throw fails closed without own journal mutation");f.s->read=Read;
   auto* active=d::active;
   std::thread other_thread([&]{
    Check(a::ReadActiveSlot28(c,Base()).status==S::kAbsent,"different thread has no active scope");
    d::active=active;d::local=local;
    Check(a::ReadActiveSlot28(c,Base()).status==S::kLaneMismatch,"same scope cannot be borrowed by another thread");d::active=nullptr;d::local={};
   });other_thread.join();
  });
  Check(a::ReadActiveSlot28(c,Base()).status==S::kAbsent,"accessor absent after unwind");
  c=Context();Bind(c,2,11,12,2);
  AtSlot28(c,[&](a::Scope&){const auto access=a::ReadActiveSlot28(c,Base());
   Check(access.valid()&&access.event_index>6&&access.entry.invocation.serial==2,"event index is lane-global not invocation-relative");});
 }
 {Fixture f;auto c=Context();Bind(c);AtSlot28(c,[&](a::Scope&){const auto access=a::ReadActiveSlot28(c,Base());
   Check(access.status==S::kOtherTarget&&access.entry==core::Entry{}&&access.call==core::Payload{}&&access.event_index==0,"other requested target has status-only result");
   auto& meta=f.s->lanes[0]->metadata[6];meta.binding_matches=false;
   Check(a::ReadActiveSlot28(c,Base()).status==S::kBindingMismatch,"other target cannot hide malformed binding");meta.binding_matches=true;
  },0x8259E76C);}
 {Fixture f;auto c=Context();Bind(c);try{a::Scope s(c,Base());
  Check(a::ReadActiveSlot28(c,Base()).status==S::kWrongStage,"entry stage not active slot28");
  c.r31.u64=c.r3.u64;s.InitialV126((c.r31.u32+400u)&~15u);c.lr=core::Caller(core::Site::kDirectE790);c.ctr.u64=0x8259E768;
  s.Call(core::Site::kDirectE790,[&]{Check(a::ReadActiveSlot28(c,Base()).status==S::kWrongStage,"correct target at wrong site rejected");});throw 919;
 }catch(int){} }
 for(unsigned bad=0;bad<7;++bad){Fixture f;auto c=Context();Bind(c);AtSlot28(c,[&](a::Scope&){
  if(bad==0)f.s->failed=true;if(bad==1)f.s->clock_failed=true;if(bad==2)f.s->saturated=true;
  if(bad==3)f.s->emergency_failure=true;if(bad==4)f.s->lanes[0]->model.Freeze();
  if(bad==5)f.s->stopped=true;if(bad==6)d::disarmed=true;
  Check(a::ReadActiveSlot28(c,Base()).status==(bad<5?S::kUnhealthy:S::kStopped),"unhealthy frozen or stopped accessor rejected");
 });}
 {Fixture f;auto c=Context();Bind(c);AtSlot28(c,[&](a::Scope&){
  const auto cut=a::Freeze(token);Check(a::ReadActiveSlot28(c,Base()).status==S::kStopped&&a::Freeze(token)==cut,"frozen accessor cannot backfill");});}
 {Fixture f;auto c=Context();Bind(c);AtSlot28(c,[&](a::Scope&){
  f.s->read=CutRead;Check(a::ReadActiveSlot28(c,Base()).status==S::kStopped,"cut during parent read wins");});}
 {Fixture f;auto c=Context();Bind(c);AtSlot28(c,[&](a::Scope&){
  f.s->stopped=true;{a::Scope shadow(c,Base());f.s->stopped=false;
   Check(a::ReadActiveSlot28(c,Base()).status==S::kUnadmitted,"unadmitted lexical shadow blocks outer accessor");}
  Check(a::ReadActiveSlot28(c,Base()).valid(),"exact lexical parent restored after shadow");});}
 for(unsigned bad=0;bad<8;++bad){Fixture f;auto c=Context();Bind(c);AtSlot28(c,[&](a::Scope&){
  auto& meta=f.s->lanes[0]->metadata[6];const auto old=meta;
  if(bad==0)meta.tid++;if(bad==1)meta.clock_valid=false;if(bad==2)meta.binding_matches=false;
  if(bad==3)meta.lane_matches=false;if(bad==4)meta.lexical_top=false;if(bad==5)meta.context_nonnull=false;if(bad==6)meta.base_nonnull=false;
  if(bad==7)meta.accessor_status=uint32_t(mcla::work_emission_win::VectorCallStatus::kAbsent);
  Check(a::ReadActiveSlot28(c,Base()).status==S::kBindingMismatch,"invalid saved event metadata rejected");meta=old;
 });}
 for(unsigned bad=0;bad<6;++bad){Fixture f;auto c=Context();Bind(c);AtSlot28(c,[&](a::Scope&){
  auto& event=const_cast<core::Event&>(f.s->lanes[0]->model.events()[6]);const auto old=event;
  if(bad==0)event.kind=core::EventKind::kCallReturn;if(bad==1)event.site=core::Site::kSlot48;
  if(bad==2)event.payload.fields[0]++;if(bad==3)event.payload.fields[1]++;
  if(bad==4)event.payload.fields[3]=1;if(bad==5)event.payload.vector[0]++;
  Check(a::ReadActiveSlot28(c,Base()).status==S::kBindingMismatch,"last event must be exact saved slot28 CallPre");event=old;
 });}
 Check(rex::cvar::SetFlagByName("mcla_vector_target_probe","false"),"accessor disable restored");
 {Fixture f;auto c=Context();Bind(c);AtSlot28(c,[&](a::Scope&){Check(a::ReadActiveSlot28(c,Base()).status==S::kStopped,"disabled accessor rejected");});}
}
} // namespace
extern "C" uint32_t MclaVectorTargetCallsiteVersion() noexcept{return body_version;}
namespace mcla::work_emission_win {
bool Enabled(){return parent_enabled;}
ActiveVectorCall ReadActiveVectorCall(const PPCContext& c,const uint8_t* b) noexcept{return Read(c,b);}
}
int main(){
 Check(!a::Enabled(),"default disabled");a::Begin(0,0,0,"bad");Check(d::configured.load()==nullptr,"disabled no allocation bypass");
 Routes();Preservation();Admission();Cuts();Limits();Slot28Access();
 LARGE_INTEGER f{};Check(QueryPerformanceFrequency(&f)!=FALSE,"real QPF");const auto freq=uint64_t(f.QuadPart);d::disarmed=false;
 Check(rex::cvar::SetFlagByName("mcla_vector_target_probe","true"),"explicit enable");
 body_version=0;Rejects([&]{a::Begin(100,200,freq,token);},"actualbody sentinel required");body_version=1;parent_enabled=false;
 Rejects([&]{a::Begin(100,200,freq,token);},"ancestor must be enabled");parent_enabled=true;
 Rejects([&]{a::Begin(100,200,freq+1,token);},"native clock frequency required");
 a::Begin(100,200,freq,token);Rejects([&]{a::Begin(100,200,freq,token);},"one process lifetime only");
 Rejects([]{a::Freeze("bad");},"wrongtoken rejected before cut");const auto cut=a::Freeze(token);a::Disarm();Check(a::Freeze(token)==cut,"postcut disarm immutable");
 std::cout<<"vector target adapter: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
