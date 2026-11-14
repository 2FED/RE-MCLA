// Host-only synthetic adapter checks. No game, GPU or guest-memory dereferences.
#include "../src/mcla_vector_store_win.cpp"
#include "mcla_no_phase_boundary_stub.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
namespace a=mcla::vector_store_win;namespace d=a::detail;namespace core=mcla::vector_store;
using Json=nlohmann::json;using core::Vector;using core::kCaller;using core::kTarget;
using Status=mcla::vector_target_win::Slot28Status;
constexpr char token[]="0123456789abcdef0123456789abcdef";
static const Vector v{0x7fc01234,0x80000000,0xffffffff,1};
static core::Entry Make(uint64_t serial=1,uint64_t parent=1,uint32_t event=7) {
  namespace w=mcla::work_emission;namespace d=mcla::consumer_detail;namespace t=mcla::virtual_detail;namespace q=mcla::vector_target;
  d::Entry p{{27,parent},{37,parent},{99,47,500},d::kTarget,d::kCaller,0x11,0x22};
  d::Child c{t::kCaller,0xffffffffBDF9D620ull,0x7ff80000deadbeefull,0xaabbccdd8258D1A0ull,t::kTarget,d::ChildState::kActive,true};
  t::Entry virt{{17,parent},p,c,t::kTarget,t::kCaller,c.r3,c.f1_bits};
  t::Call call{w::kCaller,c.r3,0x8000000000000000ull,0xaabbccdd82741010ull,w::kTail,t::CallState::kActive};
  w::Entry work{{7,parent},virt,call,w::kBody,w::kCaller,call.r3,call.f1_bits};
  w::Payload pre;pre.fields={q::kCaller,3,0xffff0000ffffff0full,0x7ff80000deadbeefull,0xaabbccdd8259F300ull};pre.vector=v;
  q::Entry target{{8,parent},work,pre,5,q::kTarget,q::kCaller,pre.fields[2],v};
  q::Payload slot;slot.fields={kCaller,target.r3,0xaabbccdd8259E768ull,0};slot.vector=v;
  return {{9,serial},target,slot,event,kTarget,kCaller,target.r3,v};
}

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
 d::Access absent;absent.status=Status::kLaneMismatch;return absent;
}
d::Access ThrowRead(const PPCContext&,const uint8_t*){++reads;Clobber();throw 44;}
d::Access CutRead(const PPCContext&,const uint8_t*){++reads;a::Freeze(token);return association;}
d::Access CutThrow(const PPCContext&,const uint8_t*){++reads;a::Freeze(token);throw 55;}
uint8_t* Base(){return reinterpret_cast<uint8_t*>(1);}
void Set(PPCVRegister& r,const Vector& value){for(unsigned i=0;i<4;++i)r.u32[i]=value[i];}
PPCContext Context(){PPCContext c{};const auto e=Make();c.lr=e.caller_lr;c.r3.u64=e.r3;Set(c.v1,e.v1);return c;}
void Bind(const PPCContext& c,uint64_t serial=1,uint64_t epoch=8,uint32_t event=7){
 auto e=Make(serial,serial,event);e.target_entry.invocation.thread_epoch=epoch;
 e.target_entry.r3=c.r3.u64;e.target_entry.v1={c.v1.u32[0],c.v1.u32[1],c.v1.u32[2],c.v1.u32[3]};
 e.target_entry.vector_pre.fields[2]=c.r3.u64;e.target_entry.vector_pre.vector=e.target_entry.v1;
 e.target_entry.work_entry.virtual_entry.parent_entry.update.native_thread_id=GetCurrentThreadId();
 association={};association.status=Status::kValid;association.entry=e.target_entry;
 association.call.fields={core::kCaller,c.r3.u64,0xaabbccdd8259e768ull,0};association.call.vector=e.target_entry.v1;association.event_index=event;
}
struct Fixture {
 std::unique_ptr<d::State> s;
 Fixture():s(d::Allocate(100,1000000,100000,token)){ticks=100;reads=0;calls=0;d::local={};d::active=nullptr;association={};d::disarmed=false;s->clock=Clock;s->read=Read;d::configured.store(s.get());}
 ~Fixture(){d::configured=nullptr;d::local={};d::active=nullptr;}
};
Json Cut(){return Json::parse(a::Freeze(token));}
void Original(PPCContext& c,bool throwing){
 ++calls;c.r3.u64^=0x12345678;c.r5.u64+=3;c.f1.u64^=0x10000000000000ull;
 for(unsigned i=0;i<4;++i)c.v1.u32[i]^=0x12340000u+i;
 SetLastError(719);_mm_setcsr((_mm_getcsr()&~0x6000u)|0x4000u);
 if(throwing)throw 80;
}
void Body(PPCContext& c,a::Scope* scope,bool throwing=false,int fail_at=-1){
 const uint32_t object=c.r3.u32;
 auto fail=[&](int at){if(at==fail_at)throw 20+at;};
 fail(0);Set(c.v0,{0,0x80000000,0x7fc12345,1});if(scope)scope->OldLoaded((object+256u)&~15u);
 fail(1);Set(c.v13,{3,2,1,0});if(scope)scope->RateLoaded((object+272u)&~15u);
 fail(2);Set(c.v12,{0x7fcfffff,1,0,0x80000000});if(scope)scope->Computed();
 fail(3);Set(c.v63,{0,1,2,3});if(scope)scope->AuxLoaded((object+288u)&~15u);
 fail(4);Set(c.v1,{4,5,6,7});if(scope)scope->Scaled();
 fail(5);if(scope)scope->Stored((object+256u)&~15u);
 fail(6);if(scope)scope->Tail([&]{Original(c,throwing);});else Original(c,throwing);
}
void Run(PPCContext& c,bool wrapped=true,bool throwing=false,int fail_at=-1){
 if(wrapped){a::Scope scope(c,Base());Body(c,&scope,throwing,fail_at);}else Body(c,nullptr,throwing,fail_at);
}
void Normal(){
 Fixture f;auto c=Context();Bind(c);Run(c);auto j=Cut();
 Check(j["schema"]=="mcla-vector-store-v2"&&j["observer_failed"]==false,"complete exact store grammar");
 Check(j["counts"]["admitted_entries"]==1&&j["threads"][0]["event_count"]==10&&j["threads"][0]["pending_count"]==0,"ten complete events");
 Check(j["native_admitted"]==false&&j["simulation_proven"]==false&&j["total_work_proven"]==false,"no physical or total proof");
 Check(j["entries_per_thread_capacity"]==d::kEntries&&j["events_per_thread_capacity"]==d::kEvents&&j["json_limit_bytes"]==d::kJson,"declared bounds");
 const auto& ev=j["threads"][0]["events"];
 Check(ev[1]["fields"][0]==0&&ev[2]["fields"][0]==16&&ev[4]["fields"][0]==32&&ev[6]["fields"][0]==0,"original effective wrapped aligned addresses");
 Check(ev[3]["vector"]==ev[6]["vector"]&&ev[5]["vector"]==ev[7]["vector"],"raw computed/store scaled/tail operands consistent");
 Check(ev[7]["fields"][2]==core::kTail&&ev[7]["fields"][0]==core::kCaller,"direct tail target not stale CTR");
 Check(j["threads"][0]["entries"][0]["target_entry"]["invocation"]["serial"]==1,"full parent retained");
}
void Preservation(){
 for(int fail=-1;fail<7;++fail)for(bool throwing:{false,true}){
  Fixture f;auto seed=Context();PPCContext expected{},actual{};std::memcpy(&expected,&seed,sizeof(seed));std::memcpy(&actual,&seed,sizeof(seed));Bind(actual);
  const auto csr=_mm_getcsr();SetLastError(9);int first=0,second=0;
  try{Run(expected,false,throwing,fail);}catch(int e){first=e;}
  const auto error=GetLastError();const auto aftercsr=_mm_getcsr();const auto directcalls=calls.load();calls=0;SetLastError(9);_mm_setcsr(csr);
  try{Run(actual,true,throwing,fail);}catch(int e){second=e;}
  Check(std::memcmp(&expected,&actual,sizeof(actual))==0,"full PPC bytes unchanged");
  Check(GetLastError()==error&&_mm_getcsr()==aftercsr,"LastError/MXCSR unchanged");
  Check(first==second&&calls==directcalls,"once-only tail and exact exception");
  Check(d::active==nullptr,"TLS restored after normal and exceptional exit");
  const auto j=Cut();Check(j["observer_failed"]==false&&j["threads"][0]["pending_count"]==0,"original exceptions not observer failures");
  Check(j["counts"]["original_exceptions"]==(second?1:0),"exception count literal");
  _mm_setcsr(csr);
 }
}
void Admission(){
 for(unsigned bad=0;bad<10;++bad){Fixture f;auto c=Context();Bind(c);
  if(bad==0)association.status=Status::kUnadmitted;
  if(bad==1)c.lr++;if(bad==2)c.r3.u64++;if(bad==3)c.v1.u32[0]++;
  if(bad==4)association.call.fields[2]++;if(bad==5)association.event_index=0;
  if(bad==6)association.entry.work_entry.virtual_entry.parent_entry.update.native_thread_id++;
  if(bad==9)association.status=Status::kObserverException;
  {a::Scope scope(c,bad==7?nullptr:Base(),bad==8?core::kTarget+4:core::kTarget);}
  Check(Cut()["counts"]["dropped_entries"]==1,"invalid selected binding rejected");
 }
 for(auto status:{Status::kAbsent,Status::kWrongStage,Status::kOtherTarget}){
  Fixture f;auto c=Context();association.status=status;Run(c);auto j=Json::parse(d::Serialize(*f.s));
  Check(calls==1&&j["counts"]["unselected_entries"]==1&&j["counts"]["admitted_entries"]==0&&j["counts"]["dropped_entries"]==0,"unselected executes original without fabricated parent");
  Check(j["observer_failed"]==false&&j["threads"].empty(),"unselected does not claim lane or poison selected capture");
  c=Context();Bind(c);Run(c);Check(!f.s->failed&&f.s->admitted==1,"later selected call remains usable");
 }
 for(auto status:{Status::kLaneMismatch,Status::kStopped,Status::kUnhealthy,Status::kBindingMismatch}){
  Fixture f;auto c=Context();Bind(c);association.status=status;Run(c);Check(Cut()["observer_failed"]==true,"ambiguous selected failure not unselected");
 }
 for(bool wrong_context:{false,true}){Fixture f;auto c=Context();Bind(c);expected_context=&c;f.s->read=CheckedRead;
  auto other=c;{a::Scope s(wrong_context?other:c,wrong_context?Base():reinterpret_cast<uint8_t*>(2));}
  Check(Cut()["counts"]["dropped_entries"]==1,"context/base mismatch not bypassed");expected_context=nullptr;}
 {Fixture f;auto c=Context();Bind(c);Run(c);c=Context();Run(c);Check(Cut()["counts"]["admitted_entries"]==1,"exact parent cannot be reused");}
 {Fixture f;auto c=Context();Bind(c);Run(c);c=Context();Bind(c,2,8,80);Run(c);Check(Cut()["counts"]["admitted_entries"]==2,"strict later parent accepted");}
 {Fixture f;auto c=Context();Bind(c);{a::Scope outer(c,Base());f.s->stopped=true;
  {a::Scope shadow(c,Base());f.s->stopped=false;outer.OldLoaded((c.r3.u32+256)&~15u);Check(d::active==&shadow,"unadmitted shadows outer");}
  Check(d::active==&outer,"lexical outer restored");}Check(Cut()["observer_failed"]==true,"cannot observe through shadow");}
 for(bool after:{false,true}){Fixture f;auto c=Context();Bind(c);ticks=after?f.s->end:1;Run(c);ticks=1000001;auto j=Cut();
  Check(j["counts"][after?"outside_after_entries":"outside_before_entries"]==1&&j["counts"]["admitted_entries"]==0,"outside window not admitted");}
 {Fixture f;auto c=Context();Bind(c);{a::Scope s(c,Base());association.call.fields[2]^=0x100000000ull;s.OldLoaded((c.r3.u32+256)&~15u);}
  Check(Cut()["counts"]["binding_mismatches"]!=0,"even high rawCTR drift retained");}
 {Fixture f;auto c=Context();Bind(c);f.s->read=ThrowRead;Run(c);Check(f.s->failed&&f.s->emergency_failure,"reader exception sticky");}
 {Fixture f;for(size_t n=0;n<d::kRejected+1;++n){auto c=Context();Bind(c);association.status=Status::kBindingMismatch;a::Scope s(c,Base());}
  auto j=Cut();Check(j["rejected_entries"].size()==d::kRejected&&j["counts"]["rejection_overflow"]==1,"bounded rejected inventory");}
}
void Cuts(){
 for(auto clock:{Zero,ThrowClock}){Fixture f;auto c=Context();Bind(c);f.s->clock=clock;Run(c);Check(Cut()["clock_failed"]==true,"clock failure retained");}
 for(auto reader:{CutRead,CutThrow}){Fixture f;auto c=Context();Bind(c);std::string frozen;
  {a::Scope s(c,Base());f.s->read=reader;s.OldLoaded((c.r3.u32+256)&~15u);frozen=a::Freeze(token);}
  Check(a::Freeze(token)==frozen&&!f.s->emergency_failure,"cut during accessor cannot backfill");}
 {Fixture f;auto c=Context();Bind(c);Run(c);const auto cut=a::Freeze(token);f.s->emergency_failure=true;a::Disarm();Check(a::Freeze(token)==cut,"late error/disarm cannot mutate cut");}
 {Fixture f;f.s->emergency_failure=true;Check(Cut()["observer_failed"]==true,"precut emergency retained");}
 {Fixture f;auto c=Context();Bind(c);a::Disarm();Run(c);Check(reads==0&&calls==1&&Cut()["disarmed_at_cut"]==true,"disarm stops observer not original");}
 {Fixture f;d::Json j;j.out.assign(d::kJson,'x');Rejects([&]{j.add("x");},"JSON cap");}
 {Fixture f;auto c=Context();Bind(c);{a::Scope s(c,Base());ticks=1;s.OldLoaded((c.r3.u32+256)&~15u);}Check(Cut()["clock_failed"]==true,"backward clock fails");}
 {Fixture f;std::mutex mutex;std::condition_variable cv;bool ready=false,resume=false;
  std::thread worker([&]{auto c=Context();Bind(c);{a::Scope s(c,Base());const auto object=c.r3.u32;
   Set(c.v0,v);s.OldLoaded((object+256)&~15u);Set(c.v13,v);s.RateLoaded((object+272)&~15u);
   Set(c.v12,v);s.Computed();Set(c.v63,v);s.AuxLoaded((object+288)&~15u);Set(c.v1,v);s.Scaled();s.Stored((object+256)&~15u);
   s.Tail([&]{++calls;std::unique_lock lock(mutex);ready=true;cv.notify_all();if(!cv.wait_for(lock,std::chrono::seconds(5),[&]{return resume;}))std::abort();SetLastError(719);});}
   if(GetLastError()!=719)std::abort();});
  {std::unique_lock lock(mutex);if(!cv.wait_for(lock,std::chrono::seconds(5),[&]{return ready;}))std::abort();}
  const auto saved=a::Freeze(token);{std::lock_guard lock(mutex);resume=true;}cv.notify_all();worker.join();
  Check(calls==1&&a::Freeze(token)==saved,"no lock spans original and no late backfill");
  Check(Json::parse(saved)["threads"][0]["pending"][0]["stage"]==uint8_t(core::Stage::kTailActive),"pending tail retained");
 }
}
void StoreGuards(){
 for(unsigned bad=0;bad<5;++bad){Fixture f;auto c=Context();Bind(c);
  {a::Scope s(c,Base());const auto object=c.r3.u32;Set(c.v0,v);
   s.OldLoaded(((object+256u)&~15u)+(bad==0?16u:0u));Set(c.v13,v);s.RateLoaded((object+272u)&~15u);
   Set(c.v12,v);s.Computed();Set(c.v63,v);s.AuxLoaded((object+288u)&~15u);Set(c.v1,v);s.Scaled();
   if(bad==1)c.v12.u32[0]^=1;s.Stored((object+256u)&~15u);
   if(bad==2)c.v1.u32[0]^=1;if(bad==3)c.lr++;if(bad==4)c.r3.u64++;
   s.Tail([&]{Original(c,false);});}
  Check(Cut()["observer_failed"]==true&&calls==1,"invalid store slice fails but tail executes once");
 }
 {Fixture f;f.s->observed=UINT64_MAX;auto c=Context();Bind(c);Run(c);Check(Cut()["counter_saturated"]==true,"saturated counter remains visible");}
 for(unsigned stage=0;stage<7;++stage){Fixture f;auto c=Context();Bind(c);std::string saved;
  {a::Scope s(c,Base());const auto object=c.r3.u32;
   if(stage>=1){Set(c.v0,v);s.OldLoaded((object+256u)&~15u);}
   if(stage>=2){Set(c.v13,v);s.RateLoaded((object+272u)&~15u);}
   if(stage>=3){Set(c.v12,v);s.Computed();}
   if(stage>=4){Set(c.v63,v);s.AuxLoaded((object+288u)&~15u);}
   if(stage>=5){Set(c.v1,v);s.Scaled();}
   if(stage>=6)s.Stored((object+256u)&~15u);
   saved=a::Freeze(token);
  }
  const auto j=Json::parse(saved);Check(j["threads"][0]["pending"][0]["stage"]==stage&&a::Freeze(token)==saved,"every partial store stage retained without backfill");
 }
}
void Limits(){
 {Fixture f;for(size_t n=0;n<d::kEntries+1;++n){auto c=Context();Bind(c,n+1,8,uint32_t(n+1));try{a::Scope s(c,Base());throw 1;}catch(int){}}
  Check(f.s->lanes[0]->model.entry_count()==d::kEntries&&f.s->lanes[0]->model.fault()==core::Fault::kEntryCapacity,"entry exhaustion no overwrite");}
 // A successful body has ten events, so entry capacity is reached before event
 // capacity; portable small-capacity tests cover the event guard directly.
 {Fixture f;auto c=Context();uint64_t serial=0;
  auto nest=[&](auto&& self,unsigned depth)->void{const auto old=association;const auto oldc=c;c=Context();++serial;Bind(c,serial,8,uint32_t(serial));
   try{a::Scope s(c,Base());if(depth)self(self,depth-1);throw 1;}catch(int){}association=old;c=oldc;};
  nest(nest,8);Check(f.s->lanes[0]->model.fault()==core::Fault::kDepth&&f.s->lanes[0]->model.pending_count()==8,"depth exhaustion retains pending");
 }
 {Fixture f;std::array<std::thread,8> workers;
  for(size_t n=0;n<workers.size();++n)workers[n]=std::thread([n]{auto c=Context();Bind(c,1,100+n);Run(c);});
  for(auto& t:workers)t.join();Check(f.s->used==8&&f.s->admitted==8&&!f.s->failed,"eight distinct parent lifetimes");
  std::thread extra([]{auto c=Context();Bind(c,1,999);Run(c);});extra.join();Check(f.s->used==8&&f.s->dropped==1,"ninth lane cannot replace old");
 }
}
} // namespace
extern "C" uint32_t MclaVectorStoreCallsiteVersion() noexcept{return body_version;}
namespace mcla::vector_target_win {
bool Enabled(){return parent_enabled;}
ActiveSlot28 ReadActiveSlot28(const PPCContext& c,const uint8_t* b) noexcept{return Read(c,b);}
}
int main(){
 Check(!a::Enabled(),"disabled default");a::Begin(0,0,0,"bad");Check(d::configured.load()==nullptr,"disabled allocation avoided");
 Normal();Preservation();Admission();Cuts();StoreGuards();Limits();
 LARGE_INTEGER f{};Check(QueryPerformanceFrequency(&f)!=FALSE,"native QPF");const auto freq=uint64_t(f.QuadPart);d::disarmed=false;
 Check(rex::cvar::SetFlagByName("mcla_vector_store_probe","true"),"explicit enable");
 body_version=0;Rejects([&]{a::Begin(100,200,freq,token);},"actual body sentinel");body_version=1;parent_enabled=false;
 Rejects([&]{a::Begin(100,200,freq,token);},"parent must be enabled");parent_enabled=true;
 Rejects([&]{a::Begin(100,200,freq+1,token);},"exact frequency");
 a::Begin(100,200,freq,token);Rejects([&]{a::Begin(100,200,freq,token);},"one lifetime");
 Rejects([]{a::Freeze("bad");},"wrong token");const auto cut=a::Freeze(token);a::Disarm();Check(a::Freeze(token)==cut,"immutable after cut");
 std::cout<<"vector store adapter: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
