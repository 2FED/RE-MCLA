// Real actual-target and state-store adapters. Only the target's upstream work
// accessor and clocks are synthetic. No game, guest memory, files or GPU IO.
#include "../src/mcla_vector_target_win.cpp"
#include "mcla_no_phase_boundary_stub.h"
#include "../src/mcla_vector_store_win.cpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>

namespace {
namespace ta=mcla::vector_target_win;namespace td=ta::detail;
namespace sa=mcla::vector_store_win;namespace sd=sa::detail;
namespace t=mcla::vector_target;namespace s=mcla::vector_store;
namespace w=mcla::work_emission;namespace vd=mcla::virtual_detail;namespace cd=mcla::consumer_detail;
using Json=nlohmann::json;
constexpr char token[]="0123456789abcdef0123456789abcdef";
std::atomic<uint64_t> ticks{100};uint64_t tail_calls{},other_calls{};
thread_local mcla::work_emission_win::ActiveVectorCall parent{};
unsigned checks{},failures{};
void Check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<name<<'\n';}}
void Clobber(){SetLastError(1001);_mm_setcsr(_mm_getcsr()^0x6000u);}
uint64_t Clock(){Clobber();return ticks.fetch_add(1);}
td::Access Read(const PPCContext&,const uint8_t*){Clobber();return parent;}
uint8_t* Base(){return reinterpret_cast<uint8_t*>(1);}
void Set(PPCVRegister& r,const t::Vector& v){for(unsigned i=0;i<4;++i)r.u32[i]=v[i];}
PPCContext Context(){PPCContext c{};c.lr=t::kCaller;c.r3.u64=0xffeeddccffffff00ull;
 Set(c.v1,{0x7fc01234,0x80000000,0xffffffff,1});return c;}
void Bind(const PPCContext& c,uint64_t serial=1,uint32_t event=10){
 parent={};parent.status=mcla::work_emission_win::VectorCallStatus::kValid;
 cd::Entry p{{22,serial},{33,serial},{44,55,GetCurrentThreadId()},cd::kTarget,cd::kCaller,12,13};
 cd::Child child{vd::kCaller,14,15,0xaabbccdd8258d1a0ull,vd::kTarget,cd::ChildState::kActive,true};
 auto& e=parent.entry;e.invocation={11,serial};
 e.virtual_entry={{21,serial},p,child,vd::kTarget,vd::kCaller,14,15};
 e.active_call0={w::kCaller,16,17,0xaabbccdd82741010ull,w::kTail,vd::CallState::kActive};
 e.actual_target=w::kBody;e.caller_lr=w::kCaller;e.r3=16;e.f1_bits=17;
 parent.vector.fields={t::kCaller,3,c.r3.u64,0x7ff8000012345678ull,0xaabbccdd8259f300ull};
 parent.vector.vector={c.v1.u32[0],c.v1.u32[1],c.v1.u32[2],c.v1.u32[3]};parent.event_index=event;parent.ordinal=3;
}
struct Fixture {
 std::unique_ptr<td::State> target=td::Allocate(100,1000000,100000,token);
 std::unique_ptr<sd::State> store=sd::Allocate(100,1000000,100000,token);
 Fixture(){ticks=100;tail_calls=other_calls=0;parent={};
  td::local={};td::active=nullptr;td::disarmed=false;target->clock=Clock;target->read=Read;td::configured=target.get();
  sd::local={};sd::active=nullptr;sd::disarmed=false;store->clock=Clock;sd::configured=store.get();
  // store->read deliberately remains the real ReadActiveSlot28 accessor.
 }
 ~Fixture(){sd::configured=nullptr;td::configured=nullptr;sd::local={};td::local={};sd::active=nullptr;td::active=nullptr;}
};
struct NoHook {void operator()(unsigned) const noexcept{}};
template<class Hook>void StoreBody(PPCContext& c,sa::Scope* scope,Hook&& hook,bool throwing=false){
 const auto object=c.r3.u32;hook(0);
 Set(c.v0,{0,1,0x80000000,0x7fc12345});if(scope)scope->OldLoaded((object+256u)&~15u);
 Set(c.v13,{1,2,3,4});if(scope)scope->RateLoaded((object+272u)&~15u);
 Set(c.v12,{0x7fcfffff,0x80000000,1,0});if(scope)scope->Computed();
 Set(c.v63,{4,3,2,1});if(scope)scope->AuxLoaded((object+288u)&~15u);
 Set(c.v1,{9,8,7,6});if(scope)scope->Scaled();if(scope)scope->Stored((object+256u)&~15u);hook(1);
 auto tail=[&]{++tail_calls;hook(2);c.r3.u64^=0x12345678;c.r5.u64+=3;c.f1.u64^=0x10000000000000ull;
  Set(c.v1,{1,2,3,4});SetLastError(719);_mm_setcsr((_mm_getcsr()&~0x6000u)|0x4000u);if(throwing)throw 80;};
 if(scope)scope->Tail(tail);else tail();
}
template<class Hook>void RunStore(PPCContext& c,bool wrapped,Hook&& hook,bool throwing=false,uint8_t* base=Base()){
 if(wrapped){sa::Scope store(c,base);StoreBody(c,&store,hook,throwing);}else StoreBody(c,nullptr,hook,throwing);
}
template<class Slot>void TargetBody(PPCContext& c,ta::Scope& target,Slot&& slot,uint32_t requested=s::kTarget){
 const auto object=c.r3.u64;const t::Vector v{c.v1.u32[0],c.v1.u32[1],c.v1.u32[2],c.v1.u32[3]};
 c.r31.u64=object;Set(c.v126,{0x7fa00000,1,2,3});target.InitialV126((c.r31.u32+400u)&~15u);
 auto invoke=[&](t::Site site){c.r3.u64=object;c.lr=t::Caller(site);c.ctr.u64=0xaabbccdd00000000ull|
    (site==t::Site::kSlot28?requested:0x12340000u+unsigned(site));
  if(site==t::Site::kSlot48||site==t::Site::kSlot28||site==t::Site::kSlot104)Set(c.v1,v);
  target.Call(site,[&]{if(site==t::Site::kSlot28){slot();return;}++other_calls;
   c.r3.u64=100+unsigned(site);c.r5.u64++;SetLastError(700+unsigned(site));});};
 invoke(t::Site::kDirectE790);invoke(t::Site::kSlot48);invoke(t::Site::kSlot28);invoke(t::Site::kDirectD1E8);
 c.r3.u64=1;target.FlagStored();c.r10.u64=0;target.Gate100();invoke(t::Site::kSlot100);invoke(t::Site::kSlot104);
 c.r11.u64=32;target.CounterLoaded();++c.r11.u64;target.CounterStored();invoke(t::Site::kSlot192);
}
template<class Slot>void RunTarget(PPCContext& c,Slot&& slot,uint32_t requested=s::kTarget){ta::Scope target(c,Base());TargetBody(c,target,slot,requested);}
void ExactParent(){Fixture f;
 for(unsigned n=0;n<2;++n){auto c=Context();Bind(c,n+1,10+2*n);RunTarget(c,[&]{
  const auto access=ta::ReadActiveSlot28(c,Base());Check(access.valid(),"real Slot28 accessor valid inside original callback");
  Check(access.event_index==6+21*n&&access.entry.invocation.serial==n+1,"exact lane-global parent pre index");
  RunStore(c,true,NoHook{});
 });}
 const auto sr=sa::Freeze(token),tr=ta::Freeze(token);const auto sj=Json::parse(sr),tj=Json::parse(tr);
 Check(sj["observer_failed"]==false&&tj["observer_failed"]==false,"both actual adapters healthy");
 Check(sj["counts"]["admitted_entries"]==2&&tj["counts"]["admitted_entries"]==2,"two distinct parent and store bodies");
 Check(sj["threads"][0]["event_count"]==20&&sj["threads"][0]["pending_count"]==0,"two ten-event complete store slices");
 Check(sj["freeze_qpc"].get<uint64_t>()<tj["freeze_qpc"].get<uint64_t>(),"store cut precedes target cut");
 for(unsigned n=0;n<2;++n){const auto& entry=sj["threads"][0]["entries"][n];
  Check(entry["target_entry"]==tj["threads"][0]["entries"][n],"full real target entry retained");
  const auto index=entry["parent_event_index"].get<size_t>();const auto& ev=tj["threads"][0]["events"][index];
  Check(ev["event"]==uint8_t(t::EventKind::kCallPre)&&ev["site"]==uint8_t(t::Site::kSlot28),"binding points to exact real Slot28 CallPre");
  Check(entry["slot28_pre"]["fields"]==ev["fields"]&&entry["slot28_pre"]["vector"]==ev["vector"],"saved parent call operands equal real journal");
 }
 Check(tail_calls==2&&other_calls==12,"all originals once");
 Check(sj["simulation_proven"]==false&&sj["total_work_proven"]==false,"no simulation or total work acceptance");
}
void Preservation(){for(bool throwing:{false,true}){
 const auto seed=Context();PPCContext expected{},actual{};std::memcpy(&expected,&seed,sizeof(seed));std::memcpy(&actual,&seed,sizeof(seed));
 DWORD error{};unsigned fp{},caught{},calls{};const auto start=_mm_getcsr();
 {Fixture f;Bind(expected);SetLastError(29);try{RunTarget(expected,[&]{RunStore(expected,false,NoHook{},throwing);});}catch(int e){caught=e;}
  error=GetLastError();fp=_mm_getcsr();calls=unsigned(tail_calls);}
 _mm_setcsr(start);
 {Fixture f;Bind(actual);SetLastError(29);unsigned actual_exception=0;
  try{RunTarget(actual,[&]{RunStore(actual,true,NoHook{},throwing);});}catch(int e){actual_exception=e;}
  Check(std::memcmp(&expected,&actual,sizeof(actual))==0,"full PPC preserved through real parent accessor");
  Check(GetLastError()==error&&_mm_getcsr()==fp,"LastError and MXCSR preserved");
  Check(actual_exception==caught&&tail_calls==calls,"original tail and exception unchanged");
  Check(sd::active==nullptr&&td::active==nullptr,"both lexical scopes restored");
  Check(Json::parse(sa::Freeze(token))["observer_failed"]==false&&Json::parse(ta::Freeze(token))["observer_failed"]==false,"normal and original-exception journals healthy");
 }_mm_setcsr(start);
}}
void Negatives(){
 for(bool wrong_context:{false,true}){Fixture f;auto c=Context();Bind(c);RunTarget(c,[&]{
  auto other=c;if(wrong_context)RunStore(other,true,NoHook{});else RunStore(c,true,NoHook{},false,reinterpret_cast<uint8_t*>(2));
 });auto j=Json::parse(sa::Freeze(token));Check(j["counts"]["dropped_entries"]==1&&j["observer_failed"]==true,"real context/base mismatch rejects store");}
 {Fixture f;auto c=Context();Bind(c);RunTarget(c,[&]{RunStore(c,true,[&](unsigned stage){if(stage==0)ta::Freeze(token);});});
  auto j=Json::parse(sa::Freeze(token));Check(j["observer_failed"]==true&&j["counts"]["binding_mismatches"].get<unsigned>()>0,"premature target cut invalidates store continuation");}
 {Fixture f;auto c=Context();Bind(c);RunTarget(c,[&]{RunStore(c,true,[&](unsigned stage){if(stage==0)parent.vector.fields[3]^=1;});});
  auto j=Json::parse(sa::Freeze(token));Check(j["observer_failed"]==true&&j["counts"]["binding_mismatches"].get<unsigned>()>0,"lost upstream association invalidates real store accessor");}
 {Fixture f;auto c=Context();Bind(c);std::string sr,tr;RunTarget(c,[&]{RunStore(c,true,[&](unsigned stage){if(stage==2){sr=sa::Freeze(token);tr=ta::Freeze(token);}});});
  Check(sa::Freeze(token)==sr&&ta::Freeze(token)==tr,"ordered cuts retain both journals without backfill");
  const auto sj=Json::parse(sr),tj=Json::parse(tr);
  Check(sj["threads"][0]["pending"][0]["stage"]==uint8_t(s::Stage::kTailActive)&&tj["threads"][0]["pending"][0]["stage"]==uint8_t(t::Stage::kCallActive),"literal pending store tail and target Slot28 retained");
  Check(sj["freeze_qpc"].get<uint64_t>()<tj["freeze_qpc"].get<uint64_t>(),"independent cuts strictly ordered");}
 {Fixture f;auto c=Context();Bind(c);RunTarget(c,[&]{const auto saved=c;RunStore(c,true,NoHook{});c=saved;RunStore(c,true,NoHook{});});
  auto j=Json::parse(sa::Freeze(token));Check(j["counts"]["admitted_entries"]==1&&j["counts"]["dropped_entries"]==1,"same real parent Slot28 cannot license duplicate body");}
 {Fixture f;auto c=Context();Bind(c);RunTarget(c,[&]{RunStore(c,true,NoHook{});},s::kTarget+4);
  auto j=Json::parse(sa::Freeze(token));Check(j["counts"]["unselected_other_target_entries"]==1&&j["counts"]["admitted_entries"]==0&&!j["observer_failed"].get<bool>(),"other requested target remains explicit unselected context");}
 {Fixture f;auto c=Context();Bind(c);try{ta::Scope target(c,Base());c.r31.u64=c.r3.u64;
   target.InitialV126((c.r31.u32+400u)&~15u);c.lr=t::Caller(t::Site::kDirectE790);
   target.Call(t::Site::kDirectE790,[&]{RunStore(c,true,NoHook{});});throw 919;
  }catch(int e){if(e!=919)throw;}
  auto j=Json::parse(sa::Freeze(token));Check(j["counts"]["unselected_wrong_stage_entries"]==1&&tail_calls==1&&!j["observer_failed"].get<bool>(),"body under another real target site is explicitly unselected");}
 {Fixture f;auto c=Context();c.lr=s::kCaller;RunStore(c,true,NoHook{});auto j=Json::parse(sa::Freeze(token));
  Check(j["counts"]["unselected_absent_entries"]==1&&tail_calls==1&&!j["observer_failed"].get<bool>(),"body outside real target is counted unselected");}
}
} // namespace
extern "C" uint32_t MclaVectorTargetCallsiteVersion() noexcept{return 1;}
extern "C" uint32_t MclaVectorStoreCallsiteVersion() noexcept{return 1;}
namespace mcla::work_emission_win {
bool Enabled(){return true;}
ActiveVectorCall ReadActiveVectorCall(const PPCContext& c,const uint8_t* b) noexcept{return Read(c,b);}
}
int main(){
 Check(rex::cvar::SetFlagByName("mcla_vector_target_probe","true"),"enable real target accessor");
 Check(rex::cvar::SetFlagByName("mcla_vector_store_probe","true"),"enable store observer");
 ExactParent();Preservation();Negatives();
 Check(rex::cvar::SetFlagByName("mcla_vector_store_probe","false"),"restore store default");
 Check(rex::cvar::SetFlagByName("mcla_vector_target_probe","false"),"restore target default");
 std::cout<<"vector store real parent: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
