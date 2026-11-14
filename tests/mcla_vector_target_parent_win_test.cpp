// Two real private adapters; only upstream virtual association and clocks are
// synthetic. No game, files, guest-memory reads, GPU, or physical proof.
#include "../src/mcla_work_emission_win.cpp"
#include "mcla_no_phase_boundary_stub.h"
#include "../src/mcla_vector_target_win.cpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>

namespace {
namespace wa=mcla::work_emission_win;namespace wd=wa::detail;
namespace ta=mcla::vector_target_win;namespace td=ta::detail;
namespace w=mcla::work_emission;namespace t=mcla::vector_target;
namespace vd=mcla::virtual_detail;namespace cd=mcla::consumer_detail;
using Json=nlohmann::json;
constexpr char token[]="0123456789abcdef0123456789abcdef";
std::atomic<uint64_t> ticks{100};uint64_t originals{};
thread_local wd::Access parent{};
unsigned checks{},failures{};
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
void Clobber(){SetLastError(1001);_mm_setcsr(_mm_getcsr()^0x6000u);}
uint64_t Clock(){Clobber();return ticks.fetch_add(1);}
wd::Access Read(const PPCContext&,const uint8_t*){Clobber();return parent;}
uint8_t* Base(){return reinterpret_cast<uint8_t*>(1);}
PPCContext Context(){PPCContext c{};c.lr=w::kCaller;c.r3.u64=0xaabbccddffffff00ull;c.f1.u64=0x7ff8000012345678ull;return c;}
void Bind(const PPCContext& c){
 parent={};parent.status=mcla::virtual_detail_win::CallStatus::kValid;
 cd::Entry p{{22,1},{33,1},{44,55,GetCurrentThreadId()},cd::kTarget,cd::kCaller,12,13};
 cd::Child child{vd::kCaller,14,15,0xaabbccdd8258d1a0ull,vd::kTarget,cd::ChildState::kActive,true};
 parent.entry={{11,1},p,child,vd::kTarget,vd::kCaller,14,15};
 parent.call={w::kCaller,c.r3.u64,c.f1.u64,0xaabbccdd82741010ull,w::kTail,vd::CallState::kActive};
}
struct Fixture {
 std::unique_ptr<wd::State> work=wd::Allocate(100,1000000,100000,token);
 std::unique_ptr<td::State> target=td::Allocate(100,1000000,100000,token);
 Fixture(){ticks=100;originals=0;parent={};wd::local={};wd::active=nullptr;wd::tail_active=nullptr;wd::disarmed=false;
  td::local={};td::active=nullptr;td::disarmed=false;work->clock=Clock;work->read=Read;target->clock=Clock;
  // Deliberately do NOT replace target->read: exercise real ReadActiveVectorCall.
  wd::configured=work.get();td::configured=target.get();}
 ~Fixture(){wd::configured=nullptr;td::configured=nullptr;wd::local={};td::local={};wd::active=nullptr;wd::tail_active=nullptr;td::active=nullptr;}
};
void VectorArgs(PPCContext& c,uint64_t ordinal,uint64_t ctr=0xaabbccdd8259f300ull){
 c.lr=w::kVectorLr;c.r30.u64=ordinal;c.r3.u64=0xffeeddccffffff00ull;c.f1.u64=0x8000000012345678ull;c.ctr.u64=ctr;
 c.v1.u32[0]=0x7fc01234;c.v1.u32[1]=0x80000000;c.v1.u32[2]=0xffffffff;c.v1.u32[3]=1;
}
void WorkStart(PPCContext& c,wa::Scope& s,uint64_t count){
 c.r7.u64=1;s.MainGate();c.r9.u64=0;s.ScalarCount();c.r10.u64=1;s.ModeGate();c.r10.u64=count;s.VectorCount();
}
// Same deterministic instructions with or without target observations. Hook
// executes inside the first real wrapped callback, never under observer locks.
template<class Hook>void TargetBody(PPCContext& c,ta::Scope* scope,Hook&& hook,int throwing=-1){
 const auto r3=c.r3.u64;const t::Vector v{c.v1.u32[0],c.v1.u32[1],c.v1.u32[2],c.v1.u32[3]};
 c.r31.u64=r3;for(unsigned k=0;k<4;++k)c.v126.u32[k]=0x7fa00000u+k;
 if(scope)scope->InitialV126((c.r31.u32+400u)&~15u);
 auto invoke=[&](t::Site site){c.r3.u64=r3;c.lr=t::Caller(site);c.ctr.u64=0xccccdddd12340000ull+unsigned(site);
  if(site==t::Site::kSlot48||site==t::Site::kSlot28||site==t::Site::kSlot104)for(unsigned k=0;k<4;++k)c.v1.u32[k]=v[k];
  auto original=[&]{++originals;c.r3.u64=100+unsigned(site);c.r5.u64+=3;c.f1.u64^=0x10000000000000ull;
   for(unsigned k=0;k<4;++k)c.v1.u32[k]^=0x12340000u+k+unsigned(site);
   SetLastError(700+unsigned(site));_mm_setcsr((_mm_getcsr()&~0x6000u)|((unsigned(site)%4u)<<13));
   if(site==t::Site::kDirectE790)hook();if(int(site)==throwing)throw 80+throwing;};
  if(scope)scope->Call(site,original);else original();};
 invoke(t::Site::kDirectE790);invoke(t::Site::kSlot48);invoke(t::Site::kSlot28);invoke(t::Site::kDirectD1E8);
 c.r3.u64=1;if(scope)scope->FlagStored();c.r10.u64=0;if(scope)scope->Gate100();invoke(t::Site::kSlot100);invoke(t::Site::kSlot104);
 c.r11.u64=32;if(scope)scope->CounterLoaded();++c.r11.u64;if(scope)scope->CounterStored();invoke(t::Site::kSlot192);
}
void RunTargets(PPCContext& c,unsigned count,bool target_enabled=true,int throwing=-1){
 wa::TailScope tail(c,Base());wa::Scope work(c,Base());WorkStart(c,work,count);
 for(unsigned n=count;n>0;--n){VectorArgs(c,n-1);work.Vector([&]{
  if(target_enabled){ta::Scope target(c,Base());TargetBody(c,&target,[]{},throwing);}else TargetBody(c,nullptr,[]{},throwing);
 });}
}
void ExactParent(){Fixture f;auto c=Context();Bind(c);RunTargets(c,4);
 const auto target_raw=ta::Freeze(token);const auto work_raw=wa::Freeze(token);auto target=Json::parse(target_raw),work=Json::parse(work_raw);
 Check(target["observer_failed"]==false&&work["observer_failed"]==false,"both real journals healthy");
 Check(target["counts"]["admitted_entries"]==4&&work["counts"]["admitted_entries"]==1,"four body ordinals one work body");
 Check(target["freeze_qpc"].get<uint64_t>()<work["freeze_qpc"].get<uint64_t>(),"independent target cut before work cut");
 Check(target["threads"][0]["pending_count"]==0&&work["threads"][0]["pending_count"]==0,"normal stacks complete");
 const auto& entries=target["threads"][0]["entries"];
 for(unsigned n=0;n<4;++n){const auto& entry=entries[n];const auto index=entry["parent_event_index"].get<size_t>();
  const auto& event=work["threads"][0]["events"][index];
  Check(index==5+2*n&&event["event"]==uint8_t(w::EventKind::kVectorPre),"exact immutable parent event selected");
  Check(entry["vector_pre"]["fields"]==event["fields"]&&entry["vector_pre"]["vector"]==event["vector"],"whole saved vector payload equals real journal");
  Check(entry["work_entry"]==work["threads"][0]["entries"][0],"full work ancestry equals real dictionary");
  Check(entry["vector_pre"]["fields"][1]==3-n&&entry["caller_lr"]==w::kVectorLr&&entry["r3"]==event["fields"][2]&&entry["v1"]==event["vector"],"ordinal LR R3 vector linkage exact");
 }
 Check(originals==28,"seven originals per actual body");Check(ta::Freeze(token)==target_raw&&wa::Freeze(token)==work_raw,"cuts immutable");
 Check(target["simulation_proven"]==false&&target["source_target_join_proven"]==false,"no physical or simulation acceptance");
}
void Preservation(){for(int throwing=-1;throwing<7;++throwing){Fixture f;auto seed=Context();PPCContext expected{},actual{};
 std::memcpy(&expected,&seed,sizeof(seed));std::memcpy(&actual,&seed,sizeof(seed));VectorArgs(expected,0);VectorArgs(actual,0);
 const auto csr=_mm_getcsr();SetLastError(9);int first=0,second=0;
 try{TargetBody(expected,nullptr,[]{},throwing);}catch(int e){first=e;}
 const auto error=GetLastError();const auto aftercsr=_mm_getcsr();const auto count=originals;originals=0;SetLastError(9);_mm_setcsr(csr);
 // Real work entry has its own original arguments; VectorArgs restores exactly
 // the tested target arguments before calling the real nested target adapter.
 auto c=Context();Bind(c);
 try{wa::TailScope tail(c,Base());wa::Scope work(c,Base());WorkStart(c,work,1);VectorArgs(c,0);
  work.Vector([&]{ta::Scope target(c,Base());TargetBody(c,&target,[]{},throwing);});}
 catch(int e){second=e;}
 std::memcpy(&actual,&c,sizeof(c));
 // WorkStart and VectorArgs touch r7/r9/r10/r30 before target entry; mirror those
 // preparatory raw values in the direct baseline (r10 subsequently becomes0).
 expected.r7.u64=1;expected.r9.u64=0;expected.r30.u64=0;
 if(throwing>=0&&throwing<=3)expected.r10.u64=1;
 Check(std::memcmp(&expected,&actual,sizeof(actual))==0,"full PPC preservation through real parent accessor");
 Check(GetLastError()==error&&_mm_getcsr()==aftercsr,"host error and MXCSR preservation");Check(first==second&&originals==count,"same callback count and exception");
 Check(td::active==nullptr&&wd::active==nullptr&&wd::tail_active==nullptr,"all lexical scopes restored");
 Check(Json::parse(ta::Freeze(token))["observer_failed"]==false,"target observer healthy on original exception");
 Check(Json::parse(wa::Freeze(token))["observer_failed"]==false,"parent observer healthy on original exception");_mm_setcsr(csr);
 }}
void Negative(){
 {Fixture f;auto c=Context();Bind(c);{wa::TailScope tail(c,Base());wa::Scope work(c,Base());WorkStart(c,work,1);VectorArgs(c,0,0xaabbccdd8259f304ull);
  work.Vector([&]{ta::Scope target(c,Base());TargetBody(c,&target,[]{});});}
  Check(Json::parse(ta::Freeze(token))["counts"]["dropped_entries"]==1,"other requested CTR cannot license actual target");
  Check(Json::parse(wa::Freeze(token))["observer_failed"]==false,"rejected child does not alter valid parent");}
 {Fixture f;auto c=Context();Bind(c);{wa::TailScope tail(c,Base());wa::Scope work(c,Base());WorkStart(c,work,1);VectorArgs(c,0);
  work.Vector([&]{const auto saved=c;{ta::Scope target(c,Base());TargetBody(c,&target,[]{});}c=saved;
   {ta::Scope duplicate(c,Base());TargetBody(c,&duplicate,[]{});}});}
  auto j=Json::parse(ta::Freeze(token));Check(j["counts"]["admitted_entries"]==1&&j["counts"]["dropped_entries"]==1,"same saved parent event admits only one body");}
 {Fixture f;auto c=Context();Bind(c);{wa::TailScope tail(c,Base());wa::Scope work(c,Base());WorkStart(c,work,1);VectorArgs(c,0);
  work.Vector([&]{ta::Scope target(c,Base());TargetBody(c,&target,[&]{wa::Freeze(token);});});}
  auto j=Json::parse(ta::Freeze(token));Check(j["observer_failed"]==true&&j["counts"]["binding_mismatches"].get<unsigned>()>0,"parent freeze invalidates target continuation");}
 {Fixture f;auto c=Context();Bind(c);std::string tc,wc;{wa::TailScope tail(c,Base());wa::Scope work(c,Base());WorkStart(c,work,1);VectorArgs(c,0);
  work.Vector([&]{ta::Scope target(c,Base());TargetBody(c,&target,[&]{tc=ta::Freeze(token);wc=wa::Freeze(token);});});}
  Check(ta::Freeze(token)==tc&&wa::Freeze(token)==wc,"target then parent cut retains pending journals unchanged");
  auto tj=Json::parse(tc),wj=Json::parse(wc);Check(tj["freeze_qpc"].get<uint64_t>()<wj["freeze_qpc"].get<uint64_t>(),"ordered independent cuts");
  Check(tj["threads"][0]["pending"][0]["stage"]==uint8_t(t::Stage::kCallActive)&&wj["threads"][0]["pending"][0]["stage"]==uint8_t(w::Stage::kVectorActive),"literal active parent and child retained");}
 {Fixture f;auto c=Context();VectorArgs(c,0);{ta::Scope target(c,Base());TargetBody(c,&target,[]{});}
  Check(Json::parse(ta::Freeze(token))["counts"]["dropped_entries"]==1,"absent real parent cannot license target");}
}
} // namespace
extern "C" uint32_t MclaWorkEmissionCallsiteVersion() noexcept{return 1;}
extern "C" uint32_t MclaWorkEmissionTailVersion() noexcept{return 1;}
extern "C" uint32_t MclaVectorTargetCallsiteVersion() noexcept{return 1;}
namespace mcla::virtual_detail_win {
bool Enabled(){return true;}
ActiveCall ReadActiveCall(const PPCContext& c,const uint8_t* b) noexcept{return Read(c,b);}
}
int main(){
 Check(rex::cvar::SetFlagByName("mcla_work_emission_probe","true"),"explicit real work accessor enable");
 Check(rex::cvar::SetFlagByName("mcla_vector_target_probe","true"),"explicit target enable");
 ExactParent();Preservation();Negative();
 Check(rex::cvar::SetFlagByName("mcla_vector_target_probe","false"),"restore target default");
 Check(rex::cvar::SetFlagByName("mcla_work_emission_probe","false"),"restore work default");
 std::cout<<"vector target real parent: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
