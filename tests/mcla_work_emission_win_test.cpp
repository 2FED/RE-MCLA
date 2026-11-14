// Synthetic host-only tests. No game, GPU, files or guest memory reads.
#include "../src/mcla_work_emission_win.cpp"
#include "mcla_no_phase_boundary_stub.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
namespace a=mcla::work_emission_win;namespace d=a::detail;namespace core=mcla::work_emission;
namespace vd=mcla::virtual_detail;namespace cd=mcla::consumer_detail;
using Json=nlohmann::json;
constexpr char token[]="0123456789abcdef0123456789abcdef";
std::atomic<uint64_t> ticks{100},reads{},calls{};
thread_local d::Access association{};
bool parent_enabled=true;uint32_t body_version=1,tail_version=1;
unsigned checks{},failures{};
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
template<class F>void Rejects(F&& f,const char* label){bool threw=false;try{f();}catch(...){threw=true;}Check(threw,label);}
void Clobber(){SetLastError(1001);_mm_setcsr(_mm_getcsr()^0x6000u);}
uint64_t Clock(){Clobber();return ticks.fetch_add(1);}
uint64_t Zero(){return 0;}
d::Access Read(const PPCContext&,const uint8_t*){++reads;Clobber();return association;}
d::Access ThrowRead(const PPCContext&,const uint8_t*){++reads;Clobber();throw 44;}
d::Access CutRead(const PPCContext&,const uint8_t*){++reads;a::Freeze(token);return association;}
d::Access CutThrow(const PPCContext&,const uint8_t*){++reads;a::Freeze(token);throw 55;}
PPCContext Context(){PPCContext c{};c.lr=core::kCaller;c.r3.u64=0xffeeddcc00112233ull;c.f1.u64=0x7ff8000011223344ull;return c;}
void Bind(const PPCContext& c,uint64_t serial=1,uint64_t epoch=11){
 association={};association.status=mcla::virtual_detail_win::CallStatus::kValid;
 auto& e=association.entry;e.invocation={epoch,serial};e.parent_entry={{22,serial},{33,serial},{44,55,GetCurrentThreadId()},cd::kTarget,cd::kCaller,12,13};
 e.parent_child={vd::kCaller,14,15,0xabcdef008258d1a0ull,vd::kTarget,cd::ChildState::kActive,true};
 e.actual_target=vd::kTarget;e.caller_lr=vd::kCaller;e.r3=14;e.f1_bits=15;
 association.call={core::kCaller,c.r3.u64,c.f1.u64,0xabcdef0082741010ull,core::kTail,vd::CallState::kActive};
}
struct Fixture {
 std::unique_ptr<d::State> s;
 Fixture():s(d::Allocate(100,1000000,100000,token)){ticks=100;reads=0;calls=0;d::local={};d::active=nullptr;d::tail_active=nullptr;association={};d::disarmed=false;s->clock=Clock;s->read=Read;d::configured.store(s.get());}
 ~Fixture(){d::configured.store(nullptr);d::local={};d::active=nullptr;d::tail_active=nullptr;}
};
uint8_t* Base(){return reinterpret_cast<uint8_t*>(1);}
void Original(PPCContext& c,int ordinal,int throwing){++calls;c.r3.u64=ordinal==0?0:0xabcdef0000000001ull;c.f1.u64^=0x10000000000000ull;c.r5.u64+=3;SetLastError(700+ordinal);_mm_setcsr((_mm_getcsr()&~0x6000u)|((unsigned(ordinal)%4u)<<13));if(ordinal==throwing)throw 80+ordinal;}
void Body(PPCContext& c,a::Scope* s,int route,int throwing=-1){
 c.r7.u64=route?1:0;if(s)s->MainGate();if(!route)return;
 c.r9.u64=3;if(s)s->ScalarCount();c.r25.u64=2;c.lr=core::kScalarLr;c.ctr.u64=0xaaaabbbb82300000ull;
 c.f1.u64=0x8000000000000000ull;c.f2.u64=0x7ff8000012345678ull;
 if(s)s->Scalar([&]{Original(c,0,throwing);});else Original(c,0,throwing);
 c.lr=core::kCleanupLr;c.ctr.u64=0xaaaabbbb82300004ull;
 if(s)s->Cleanup([&]{Original(c,1,throwing);});else Original(c,1,throwing);
 // Filtered scalar indices 1 and0 deliberately produce no selected calls.
 c.r10.u64=route==2?1:0;if(s)s->ModeGate();
 if(route==2){c.r10.u64=2;if(s)s->VectorCount();
  for(uint64_t i=2;i>0;--i){c.r30.u64=i-1;c.lr=core::kVectorLr;c.ctr.u64=0xccccdddd82310000ull;
   for(unsigned k=0;k<4;++k)c.v1.u32[k]=0x12340000u+k;
   if(s)s->Vector([&]{Original(c,2+int(i),throwing);});else Original(c,2+int(i),throwing);}
 }else{c.r11.u64=3;if(s)s->JobBound();c.r31.u64=0;c.lr=core::kJobLr;
  c.r3.u64=3;c.r4.u64=4;c.r5.u64=5;c.r6.u64=6;c.r7.u64=7;c.r8.u64=8;
  if(s)s->Job(0x7fc01234u,[&]{Original(c,2,throwing);});else Original(c,2,throwing);
  c.r11.u64=1;if(s)s->JobBound(); // Reload shrinks initial bound, not a fixed planned job count.
  c.r3.u64=1;c.r4.u64=10;c.r5.u64=1;c.lr=core::kWaitLr;
  if(s)s->Wait([&]{Original(c,3,throwing);});else Original(c,3,throwing);
 }
}
void Run(PPCContext& c,int route,bool wrapped=true,int throwing=-1){if(wrapped){a::TailScope tail(c,Base());a::Scope scope(c,Base());Body(c,&scope,route,throwing);}else Body(c,nullptr,route,throwing);}
Json Cut(){return Json::parse(a::Freeze(token));}
void Routes(){for(int route=0;route<3;++route){Fixture f;auto c=Context();Bind(c);Run(c,route);auto j=Cut();
 Check(j["observer_failed"]==false&&j["counts"]["admitted_entries"]==1,"complete route healthy");
 Check(j["threads"][0]["pending_count"]==0&&j["threads"][0]["entries"].size()==1,"entry dictionary and complete stack");
 Check(j["schema"]=="mcla-work-emission-v2"&&j["independent_cut"]==true&&j["prelude_unobserved"]==true,"explicit schema and limitations");
 Check(j["task_completion_proven"]==false&&j["simulation_proven"]==false&&j["native_admitted"]==false,"no completion or simulation proof");
 Check(j["entries_per_thread_capacity"]==2048&&j["events_per_thread_capacity"]==65536&&j["json_limit_bytes"]==64*1024*1024,"fixed limits");
 if(route==1){bool job=false,reload=false,wait=false;for(const auto& e:j["threads"][0]["events"]){
  if(e["event"]==uint8_t(core::EventKind::kJobPre)){job=true;Check(e["fields"][8]==0x7fc01234u&&e["fields"][1]==0,"already-converted descriptor bits and index preserved");}
  if(e["event"]==uint8_t(core::EventKind::kJobReloadBound)){reload=true;Check(e["fields"][0]==1,"dynamic reloaded bound retained");}
  if(e["event"]==uint8_t(core::EventKind::kWaitPre)){wait=true;Check(e["fields"][1]==1&&e["fields"][3]==1,"wait rawargs retained not asynchronous completion");}}
  Check(job&&reload&&wait,"selected job sequence complete");}
 if(route==2){
  unsigned n=0;
  for(const auto& e:j["threads"][0]["events"]){
   if(e["event"]==uint8_t(core::EventKind::kVectorPre)){
    Check(e["fields"][1]==1-n,"reverse vector index");
    for(unsigned k=0;k<4;++k)Check(e["vector"][k]==0x12340000u+k,"deterministic vector host lane order");
    ++n;
   }
  }
  Check(n==2,"two vector emissions");
 }
 }}
void Preservation(){for(int route=1;route<=2;++route)for(int throwing=-1;throwing<5;++throwing){Fixture f;auto seed=Context();PPCContext expected{},actual{};std::memcpy(&expected,&seed,sizeof(seed));std::memcpy(&actual,&seed,sizeof(seed));Bind(actual);
 const auto csr=_mm_getcsr();SetLastError(9);int first=0,second=0;try{Run(expected,route,false,throwing);}catch(int e){first=e;}
 const auto error=GetLastError();const auto aftercsr=_mm_getcsr();const auto directcalls=calls.load();calls=0;SetLastError(9);_mm_setcsr(csr);
 try{Run(actual,route,true,throwing);}catch(int e){second=e;}
 Check(std::memcmp(&expected,&actual,sizeof(actual))==0,"all PPC bytes preserve exact original");Check(GetLastError()==error&&_mm_getcsr()==aftercsr,"LastError/MXCSR preserved around only observation");
 Check(first==second&&calls==directcalls,"original once and exception unchanged");Check(d::active==nullptr&&d::tail_active==nullptr,"TLS restored on exception");
 const auto j=Cut();Check(j["observer_failed"]==false,"original exception does not imply observer failure");_mm_setcsr(csr);
 }}
void Admission(){
 {Fixture f;auto c=Context();Bind(c);{a::Scope no_tail(c,Base());c.r7.u64=0;no_tail.MainGate();}Check(Cut()["counts"]["dropped_entries"]==1,"missing tail rejects actual body");}
 {Fixture f;auto c=Context();Bind(c);{a::TailScope tail(c,Base());{a::Scope first(c,Base());c.r7.u64=0;first.MainGate();}{a::Scope second(c,Base());c.r7.u64=0;second.MainGate();}}
  const auto j=Cut();Check(j["counts"]["admitted_entries"]==1&&j["counts"]["dropped_entries"]==1,"tail eligibility consumed once survives first exit");}
 {Fixture f;auto c=Context();Bind(c);{a::TailScope outer(c,Base());{a::TailScope shadow(c,nullptr);a::Scope body(c,Base());Check(d::tail_active==&shadow,"rejected tail shadows ancestor");}Check(d::tail_active==&outer,"tail stack restored");}Check(Cut()["observer_failed"]==true,"shadow cannot borrow outer eligibility");}
 for(bool after:{false,true}){Fixture f;auto c=Context();Bind(c);ticks=after?f.s->end:1;Run(c,0);ticks=1000001;auto j=Cut();Check(j["counts"][after?"outside_after_entries":"outside_before_entries"]==1&&j["counts"]["admitted_entries"]==0,"outside window explicitly counted");}
 {Fixture f;auto c=Context();ticks=1;Run(c,0);ticks=100;Bind(c);Run(c,0);Check(Cut()["observer_failed"]==false,"unavailable pre-window parent does not poison valid window");}
 {Fixture f;auto c=Context();Bind(c);f.s->clock=Zero;Run(c,0);auto j=Cut();Check(j["clock_failed"]==true&&j["counts"]["dropped_entries"]==1,"invalid entry clock retained");}
 for(unsigned bad=0;bad<6;++bad){Fixture f;auto c=Context();Bind(c);{a::TailScope tail(c,Base());
  if(bad==0)association.call.indirect_ctr^=1ull<<40;
  if(bad==1)association.entry.parent_entry.update.thread_epoch++;
  if(bad==2)c.r3.u64^=1;
  if(bad==3)c.f1.u64^=1;
  if(bad==4)c.lr^=1;
  a::Scope body(c,bad==5?nullptr:Base());c.r7.u64=0;body.MainGate();}
  Check(Cut()["counts"]["dropped_entries"]==1,"tail/fullparent/body operand drift rejected");}
 {Fixture f;auto c=Context();Bind(c);{a::TailScope tail(c,Base());a::Scope body(c,Base());association.call.f1_bits^=1;c.r7.u64=0;body.MainGate();}Check(Cut()["counts"]["binding_mismatches"]!=0,"each boundary rechecks whole saved association");}
 {Fixture f;auto c=Context();Bind(c);f.s->read=ThrowRead;Run(c,0);Check(f.s->failed&&f.s->emergency_failure,"unexpected reader exception sticky");}
 {Fixture f;for(size_t n=0;n<d::kRejected+1;++n){auto c=Context();a::Scope body(c,Base());}auto j=Cut();Check(j["rejected_entries"].size()==256&&j["counts"]["rejection_overflow"]==1,"bounded rejected headers no replacement");}
}
void LimitsAndFreeze(){
 {Fixture f;for(size_t n=0;n<d::kEntries+1;++n){auto c=Context();Bind(c,n+1);Run(c,0);}auto j=Cut();Check(j["threads"][0]["entry_count"]==d::kEntries&&j["threads"][0]["model_fault"]==uint8_t(core::Fault::kEntryCapacity),"entry limit preserves first records");}
 {Fixture f;auto c=Context();Bind(c);{a::TailScope tail(c,Base());a::Scope body(c,Base());
  c.r7.u64=1;body.MainGate();c.r9.u64=0;body.ScalarCount();c.r10.u64=1;body.ModeGate();c.r10.u64=32768;body.VectorCount();
  for(uint64_t n=32768;n>0;--n){c.r30.u64=n-1;c.lr=core::kVectorLr;body.Vector([&]{++calls;});}}
  Check(calls==32768&&f.s->lanes[0]->model.size()==d::kEvents&&f.s->lanes[0]->model.fault()==core::Fault::kEventCapacity,"event capacity never skips remaining originals or overwrites");}
 {Fixture f;auto c=Context();uint64_t serial=0;
  auto nest=[&](auto&& self,unsigned depth)->void{const auto saved=association;const auto savedc=c;Bind(c,++serial);
   {a::TailScope tail(c,Base());a::Scope body(c,Base());c.r7.u64=0;body.MainGate();if(depth)self(self,depth-1);}
   association=saved;c=savedc;};
  nest(nest,8);Check(f.s->lanes[0]->model.fault()==core::Fault::kDepth&&f.s->lanes[0]->model.pending_count()==8,"depth limit retains literal pending stack");}
 {Fixture f;std::array<std::thread,8> workers;
  for(size_t i=0;i<workers.size();++i)workers[i]=std::thread([i]{auto c=Context();Bind(c,1,100+i);
   association.entry.parent_entry.invocation.thread_epoch=200+i;association.entry.parent_entry.router.thread_epoch=300+i;Run(c,0);});
  for(auto& worker:workers)worker.join();
  Check(f.s->used==8&&f.s->admitted==8&&!f.s->failed,"concurrent distinct thread lifetimes admitted independently");
  std::thread extra([]{auto c=Context();Bind(c,1,999);association.entry.parent_entry.invocation.thread_epoch=998;association.entry.parent_entry.router.thread_epoch=997;Run(c,0);});extra.join();
  Check(f.s->used==8&&f.s->dropped==1&&f.s->rejected[0].reason==d::Reject::kLane,"ninth lifetime cannot replace prior lane");}
 {Fixture f;auto c=Context();Bind(c);std::string frozen;{a::TailScope tail(c,Base());a::Scope body(c,Base());f.s->read=CutRead;c.r7.u64=0;body.MainGate();frozen=a::Freeze(token);}Check(a::Freeze(token)==frozen,"freeze during parent read prevents late backfill");}
 {Fixture f;auto c=Context();Bind(c);std::string frozen;{a::TailScope tail(c,Base());a::Scope body(c,Base());f.s->read=CutThrow;c.r7.u64=0;body.MainGate();frozen=a::Freeze(token);}Check(a::Freeze(token)==frozen&&!f.s->emergency_failure,"late throwing read cannot mutate frozen health");}
 {Fixture f;auto c=Context();Bind(c);Run(c,0);const auto frozen=a::Freeze(token);
  f.s->emergency_failure.store(true,std::memory_order_release);
  Check(a::Freeze(token)==frozen,"late mutex-fallback atomic cannot change frozen health");}
 {Fixture f;f.s->emergency_failure.store(true,std::memory_order_release);
  Check(Cut()["observer_failed"]==true,"pre-cut emergency health retained");}
 {Fixture f;auto c=Context();Bind(c);a::Disarm();Run(c,0);Check(reads==0&&Cut()["disarmed_at_cut"]==true,"disarm stops observer not title");}
 {Fixture f;d::Json j;j.out.assign(d::kJson,'x');Rejects([&]{j.add("x");},"hard JSON bound no truncation");}
 {Fixture f;std::mutex mutex;std::condition_variable cv;bool ready=false,resume=false;
  std::thread worker([&]{auto c=Context();Bind(c);{a::TailScope tail(c,Base());a::Scope body(c,Base());c.r7.u64=1;body.MainGate();c.r9.u64=1;body.ScalarCount();c.r25.u64=0;c.lr=core::kScalarLr;
   body.Scalar([&]{++calls;std::unique_lock lock(mutex);ready=true;cv.notify_all();if(!cv.wait_for(lock,std::chrono::seconds(5),[&]{return resume;}))std::abort();c.r3.u64=1;SetLastError(719);});}
   if(GetLastError()!=719)std::abort();});
  {std::unique_lock lock(mutex);if(!cv.wait_for(lock,std::chrono::seconds(5),[&]{return ready;}))std::abort();}
  const auto saved=a::Freeze(token);{std::lock_guard lock(mutex);resume=true;}cv.notify_all();worker.join();
  Check(calls==1&&a::Freeze(token)==saved,"held original once no lock spans original or late backfill");
  Check(Json::parse(saved)["threads"][0]["pending"][0]["stage"]==uint8_t(core::Stage::kScalarActive),"active pending state remains literal");}
}
void ActiveVectorAccessor(){
 using Status=a::VectorCallStatus;
 Check(rex::cvar::SetFlagByName("mcla_work_emission_probe","true"),"enable accessor fixture");
 auto status=[](const a::ActiveVectorCall& value,Status wanted){
  a::ActiveVectorCall empty;empty.status=wanted;
  Check(value==empty,"rejected accessor returns status only");
 };
 auto vector=[](PPCContext& c,a::Scope& body,auto&& action){
  c.r7.u64=1;body.MainGate();c.r9.u64=0;body.ScalarCount();
  c.r10.u64=1;body.ModeGate();c.r10.u64=1;body.VectorCount();
  c.r30.u64=0;c.lr=core::kVectorLr;c.r3.u64=0xaabbccdd12345678ull;
  c.ctr.u64=0xfedcba988259f300ull;c.f1.u64=0x8000000012345678ull;
  c.v1.u32[0]=0;c.v1.u32[1]=0x80000000u;c.v1.u32[2]=0x7fc12345u;c.v1.u32[3]=0x3f800000u;
  body.Vector(action);
 };
 {Fixture f;auto c=Context();Bind(c);status(a::ReadActiveVectorCall(c,Base()),Status::kAbsent);
  {a::TailScope tail(c,Base());a::Scope body(c,Base());
   status(a::ReadActiveVectorCall(c,Base()),Status::kWrongStage);
   vector(c,body,[&]{
    const auto saved=a::ReadActiveVectorCall(c,Base());
    Check(saved.valid()&&saved.ordinal==0&&saved.event_index==5,"exact active vector event identified");
    Check(saved.entry.virtual_entry==association.entry&&saved.entry.active_call0==association.call,"full saved parent entry retained");
    Check(saved.vector.fields[2]==c.r3.u64&&saved.vector.fields[3]==c.f1.u64&&saved.vector.fields[4]==c.ctr.u64,"saved raw scalar operands retained");
    for(unsigned k=0;k<4;++k)Check(saved.vector.vector[k]==c.v1.u32[k],"saved host-order vector lane retained");
    c.r3.u64=7;c.f1.u64=8;c.ctr.u64=9;c.lr=10;c.r30.u64=99;c.v1.u32[2]=11;
    const auto before=d::Serialize(*f.s);const auto clock=ticks.load();PPCContext bytes{};std::memcpy(&bytes,&c,sizeof(c));
    const auto csr=_mm_getcsr();SetLastError(731);
    Check(a::ReadActiveVectorCall(c,Base())==saved,"mutable live arguments never replace saved operands");
    Check(GetLastError()==731&&_mm_getcsr()==csr,"accessor preserves LastError and MXCSR");
    Check(std::memcmp(&bytes,&c,sizeof(c))==0&&ticks==clock&&d::Serialize(*f.s)==before,"accessor preserves all PPC bytes journal and clock");
    auto other=c;status(a::ReadActiveVectorCall(other,Base()),Status::kLaneMismatch);
    status(a::ReadActiveVectorCall(c,nullptr),Status::kLaneMismatch);
    status(a::ReadActiveVectorCall(c,reinterpret_cast<uint8_t*>(2)),Status::kLaneMismatch);
    const auto local=d::local;const auto active=d::active;const auto active_tail=d::tail_active;
    a::ActiveVectorCall cross;
    std::thread thread([&]{d::local=local;d::active=active;d::tail_active=active_tail;
     cross=a::ReadActiveVectorCall(c,Base());d::local={};d::active=nullptr;d::tail_active=nullptr;});thread.join();
    status(cross,Status::kLaneMismatch);
    d::configured=nullptr;status(a::ReadActiveVectorCall(c,Base()),Status::kLaneMismatch);d::configured=f.s.get();
    d::local={};status(a::ReadActiveVectorCall(c,Base()),Status::kLaneMismatch);d::local=local;
    association.call.indirect_ctr^=1ull<<40;status(a::ReadActiveVectorCall(c,Base()),Status::kBindingMismatch);association.call.indirect_ctr^=1ull<<40;
    d::tail_active=nullptr;status(a::ReadActiveVectorCall(c,Base()),Status::kBindingMismatch);d::tail_active=active_tail;
    f.s->stopped=true;{a::Scope shadow(c,Base());f.s->stopped=false;
     status(a::ReadActiveVectorCall(c,Base()),Status::kUnadmitted);}
    Check(a::ReadActiveVectorCall(c,Base())==saved,"unadmitted nested scope shadows then restores exact parent");
    f.s->read=ThrowRead;const auto throwing_before=d::Serialize(*f.s);
    status(a::ReadActiveVectorCall(c,Base()),Status::kObserverException);
    Check(d::Serialize(*f.s)==throwing_before&&ticks==clock,"throwing read does not mutate journal or sample clock");f.s->read=Read;
    Check(rex::cvar::SetFlagByName("mcla_work_emission_probe","false"),"disable active accessor fixture");
    status(a::ReadActiveVectorCall(c,Base()),Status::kStopped);
    Check(rex::cvar::SetFlagByName("mcla_work_emission_probe","true"),"reenable active accessor fixture");
    f.s->failed=true;status(a::ReadActiveVectorCall(c,Base()),Status::kUnhealthy);f.s->failed=false;
    f.s->emergency_failure=true;status(a::ReadActiveVectorCall(c,Base()),Status::kUnhealthy);f.s->emergency_failure=false;
    auto& event=const_cast<core::Event&>(f.s->lanes[0]->model.events()[saved.event_index]);
    event.payload.fields[1]++;status(a::ReadActiveVectorCall(c,Base()),Status::kBindingMismatch);event.payload.fields[1]--;
    event.kind=core::EventKind::kVectorReturn;status(a::ReadActiveVectorCall(c,Base()),Status::kBindingMismatch);event.kind=core::EventKind::kVectorPre;
    Check(a::ReadActiveVectorCall(c,Base())==saved,"restored fixture still has exact saved event");
   });
   status(a::ReadActiveVectorCall(c,Base()),Status::kWrongStage);
  }
  status(a::ReadActiveVectorCall(c,Base()),Status::kAbsent);
 }
 for(unsigned mode=0;mode<5;++mode){Fixture f;auto c=Context();Bind(c);a::TailScope tail(c,Base());a::Scope body(c,Base());
  vector(c,body,[&]{
   if(mode==0){f.s->lanes[0]->model.Freeze();status(a::ReadActiveVectorCall(c,Base()),Status::kUnhealthy);}
   if(mode==1){f.s->lanes[0]->model.MainGate({1,1},0);status(a::ReadActiveVectorCall(c,Base()),Status::kUnhealthy);}
   if(mode==2){a::Disarm();status(a::ReadActiveVectorCall(c,Base()),Status::kStopped);}
   if(mode==3){const auto frozen=a::Freeze(token);status(a::ReadActiveVectorCall(c,Base()),Status::kStopped);Check(a::Freeze(token)==frozen,"frozen accessor never backfills");}
   if(mode==4){f.s->read=CutRead;status(a::ReadActiveVectorCall(c,Base()),Status::kStopped);}
  });
 }
 Check(rex::cvar::SetFlagByName("mcla_work_emission_probe","false"),"restore default work option after accessor fixture");
}
} // namespace
extern "C" uint32_t MclaWorkEmissionCallsiteVersion() noexcept{return body_version;}
extern "C" uint32_t MclaWorkEmissionTailVersion() noexcept{return tail_version;}
namespace mcla::virtual_detail_win {
bool Enabled(){return parent_enabled;}
ActiveCall ReadActiveCall(const PPCContext& c,const uint8_t* b) noexcept {return Read(c,b);}
}
int main(){
 Check(!a::Enabled(),"runtime work option defaults off");a::Begin(0,0,0,"bad");Check(d::configured.load()==nullptr,"disabled no allocation or validation");
 Routes();Preservation();Admission();LimitsAndFreeze();ActiveVectorAccessor();
 LARGE_INTEGER f{};Check(QueryPerformanceFrequency(&f)!=FALSE,"real QPF");const auto freq=uint64_t(f.QuadPart);d::disarmed=false;
 Check(rex::cvar::SetFlagByName("mcla_work_emission_probe","true"),"explicit opt-in");
 body_version=0;Rejects([&]{a::Begin(100,200,freq,token);},"body sentinel required");body_version=1;tail_version=0;
 Rejects([&]{a::Begin(100,200,freq,token);},"tail sentinel required");tail_version=1;parent_enabled=false;
 Rejects([&]{a::Begin(100,200,freq,token);},"parent must be enabled");parent_enabled=true;
 Rejects([&]{a::Begin(100,200,freq+1,token);},"independent QPF must match native");
 a::Begin(100,200,freq,token);Rejects([&]{a::Begin(100,200,freq,token);},"one process lifetime");
 Rejects([]{a::Freeze("bad");},"wrong token rejects before cut");const auto cut=a::Freeze(token);a::Disarm();Check(a::Freeze(token)==cut,"disarm after freeze immutable");
 std::cout<<"work emission adapter: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
