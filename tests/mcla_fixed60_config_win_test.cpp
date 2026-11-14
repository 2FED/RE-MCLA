// SOURCE-ONLY synthetic regression fixture. Build with both private macros.
// No game, GPU, save, desktop, filesystem, or production process is launched.
#if !defined(MCLA_FIXED60_CONFIG_EXPERIMENT)
#error Fixed60 wrapper test requires its separate explicit experiment macro.
#endif
#include "../src/mcla_timer_config_win.cpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
namespace api=mcla::timer_config;
namespace d=api::detail;
namespace edit=mcla::fixed60_word_edit;
using Json=nlohmann::json;
constexpr char token[]="0123456789abcdef0123456789abcdef";
unsigned checks{},failures{};
void Check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}}
template<class F> void Wait(F&& predicate) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!predicate()){if(std::chrono::steady_clock::now()>deadline)std::abort();std::this_thread::yield();}
}
struct Fixture;
thread_local Fixture* current{};
enum class EditMode { Good, Reject, Mismatch, Ambiguous, ReadFault, ReadMismatch, Throw, BadVerified };
struct Fixture {
  std::unique_ptr<d::State> state=std::make_unique<d::State>();
  d::Config memory{};
  std::atomic<uint64_t> ticks{100};
  std::atomic<unsigned> constructors{},initializers{},applies{},setters{},edits{},fresh_reads{};
  std::atomic<bool> in_edit{},release_edit{},in_original{},release_original{};
  EditMode edit_mode=EditMode::Good;
  bool fresh_failure{},fresh_throw{},fresh_drift{},fresh_outside{},block_edit{},block_original{};
  bool throw_original{},disturb{},pre_failure{},freeze_before_apply{},stop_before_apply{};
  bool post_edit_rollback{},post_edit_outside{},registry_drift{};
  int lexical_fault{};
  PPCContext* live_context{};
  uint32_t rate_seen{},count_seen{};
  explicit Fixture(bool experiment=true);
};
uint8_t* Base(){return reinterpret_cast<uint8_t*>(0x10000);}
PPCContext Context(){PPCContext c{};c.r3.u64=0x1000;c.lr=0x822D29C0;c.r31.u64=77;return c;}
d::Config Stock(){d::Config c{};c.words={0,17,0x3F800000,0x41F00000,19,1,2};c.byte100=1;return c;}
uint64_t Clock(){return current->ticks.fetch_add(1);}
void Disturb(){SetLastError(888);_mm_setcsr(_mm_getcsr()^0x2000);if(current->live_context)current->live_context->r31.u64=0xBAD;}
bool ReadConfig(uintptr_t,uint32_t,d::Config& out){out=current->memory;if(current->disturb)Disturb();return !current->pre_failure;}
bool ReadTimer(uintptr_t,uint32_t,mcla::step_timer_memory::State& out){out.count=2;if(current->disturb)Disturb();return !current->pre_failure;}
bool Fresh(uintptr_t,uint32_t,d::Config& out) {
  auto& f=*current;++f.fresh_reads;out=f.memory;
  if(f.fresh_throw)throw 19;
  if(f.fresh_drift)out.words[1]^=1;
  if(f.fresh_outside)f.ticks=f.state->data.end;
  if(f.disturb)Disturb();
  return !f.fresh_failure;
}
edit::Result Verified() {
  edit::Result r;r.outcome=edit::Outcome::ChangedVerified;
  r.query_attempted=r.region_accepted=r.cas_attempted=r.cas_completed=true;
  r.observed_available=r.changed=r.readback_attempted=r.readback_available=true;
  r.observed_guest_bits=0x41F00000;r.readback_guest_bits=0x42700000;return r;
}
edit::Result Edit(bool enabled,uintptr_t base,uint32_t address) {
  auto& f=*current;++f.edits;
  Check(enabled&&base==reinterpret_cast<uintptr_t>(Base())&&address==0x1000,"helper exact enabled/address arguments");
  if(f.block_edit){f.in_edit=true;Wait([&]{return f.release_edit.load();});}
  if(f.disturb)Disturb();
  if(f.edit_mode==EditMode::Throw)throw 18;
  if(f.edit_mode==EditMode::Reject){edit::Result r;r.outcome=edit::Outcome::RejectedRegion;r.query_attempted=true;return r;}
  if(f.edit_mode==EditMode::Mismatch){edit::Result r;r.outcome=edit::Outcome::CompareMismatch;
    r.query_attempted=r.region_accepted=r.cas_attempted=r.cas_completed=r.observed_available=true;
    r.observed_guest_bits=0x42F00000;return r;}
  if(f.edit_mode==EditMode::Ambiguous){edit::Result r;r.outcome=edit::Outcome::CasFaultAmbiguous;
    r.query_attempted=r.region_accepted=r.cas_attempted=true;r.exception_code=EXCEPTION_ACCESS_VIOLATION;return r;}
  f.memory.words[3]=0x42700000;auto result=Verified();
  if(f.post_edit_rollback)f.ticks=1;
  if(f.post_edit_outside)f.ticks=f.state->data.end;
  if(f.edit_mode==EditMode::ReadFault){result.outcome=edit::Outcome::ChangedReadbackFault;result.readback_available=false;
    result.readback_guest_bits=0;result.exception_code=EXCEPTION_ACCESS_VIOLATION;}
  if(f.edit_mode==EditMode::ReadMismatch){result.outcome=edit::Outcome::ChangedReadbackMismatch;result.readback_guest_bits=0x41F00000;}
  if(f.edit_mode==EditMode::BadVerified)result.cas_completed=false;
  return result;
}
Fixture::Fixture(bool experiment) {
  memory=Stock();state->data.begin=100;state->data.end=100000;state->data.frequency=1000;
  state->data.token=token;state->data.fixed60=experiment;state->clock=Clock;
  state->config_reader=ReadConfig;state->timer_reader=ReadTimer;
  state->fresh_config_reader=Fresh;state->edit_word=Edit;
}
void Ctor(PPCContext&,uint8_t*){++current->constructors;}
void Setter(PPCContext&,uint8_t*){++current->setters;}
void Apply(PPCContext& ctx,uint8_t* base) {
  auto& f=*current;++f.applies;f.rate_seen=f.memory.words[3];f.count_seen=f.memory.words[6];
  if(f.block_original){f.in_original=true;Wait([&]{return f.release_original.load();});}
  if(f.throw_original){ctx.r7.u64=123;SetLastError(1234);throw 42;}
  const auto r3=ctx.r3.u64;ctx.r3.u32=d::kTimer;
  ctx.lr=0x822D24CC;d::Invoke(f.state.get(),d::Kind::RawSetter,Setter,ctx,base);
  ctx.lr=0x822D24DC;d::Invoke(f.state.get(),d::Kind::FixedSetter,Setter,ctx,base);
  ctx.r3.u64=r3;
}
void Initializer(PPCContext& ctx,uint8_t* base) {
  auto& f=*current;++f.initializers;ctx.lr=0x822D26C4;
  if(f.freeze_before_apply)d::Cut(*f.state,token);
  if(f.stop_before_apply){d::configured=f.state.get();api::Disarm();d::configured=nullptr;}
  if(f.registry_drift)f.state->associations[0].ctor+=1;
  auto& frame=*d::stack.frames[d::stack.depth-1];
  if(f.lexical_fault==1)frame.context=nullptr;
  if(f.lexical_fault==2)frame.base=nullptr;
  if(f.lexical_fault==3)frame.tid=0;
  if(f.lexical_fault==4)frame.config+=4;
  if(f.lexical_fault==5)frame.ctor+=1;
  if(f.lexical_fault==6)frame.kind=d::Kind::Constructor;
  if(f.lexical_fault==7)frame.state=nullptr;
  if(f.lexical_fault==8)ctx.lr=0x822D26C0;
  if(f.lexical_fault==9){d::Lexical mask;mask.Push(f.state.get(),d::Kind::Constructor,ctx,base);
    d::Invoke(f.state.get(),d::Kind::Apply,Apply,ctx,base);mask.Pop();return;}
  d::Invoke(f.state.get(),d::Kind::Apply,Apply,ctx,base);
}
void Register(Fixture& f,PPCContext& ctx){current=&f;f.live_context=&ctx;ctx.lr=0x822D29C0;d::Invoke(f.state.get(),d::Kind::Constructor,Ctor,ctx,Base());}
void Run(Fixture& f,PPCContext& ctx,uint64_t lr=0x822C696C){current=&f;f.live_context=&ctx;ctx.lr=lr;d::Invoke(f.state.get(),d::Kind::Initializer,Initializer,ctx,Base());}
Json Cut(Fixture& f){current=&f;return Json::parse(d::Cut(*f.state,token));}
void PositiveAndDisabled() {
  Fixture f;auto ctx=Context();Register(f,ctx);
  for(auto lr:{0x822C696Cull,0x822C6C9Cull}){f.memory=Stock();Run(f,ctx,lr);}
  auto j=Cut(f);
  Check(f.edits==2&&f.applies==2&&f.setters==4,"both registered initializer routes independently intervene once");
  Check(f.rate_seen==0x42700000&&f.count_seen==2,"original Apply sees60 and unchanged count2");
  auto expected=Stock();expected.words[3]=0x42700000;
  Check(f.memory.words==expected.words&&f.memory.byte100==expected.byte100,"only configuration+84 changed");
  Check(j["schema"]=="mcla-timer-config-fixed60-v1"&&j["observer_failed"]==false,"enabled separate healthy diagnostic schema");
  Check(j["interventions"].size()==2&&j["intervention_serial"]==2,"bounded separate intervention serials");
  for(const auto& r:j["interventions"]){
    Check(r["outcome"]=="changed_verified"&&r["policy_admitted"]==true,"verified receipt without proof promotion");
    const auto id=r["apply_observation_id"].get<size_t>();
    Check(j["records"][id-1]["config_before"]["words"][3]==0x41F00000&&
      j["records"][id-1]["config_after"]["words"][3]==0x42700000,"original PRE retained separately from fresh edit receipt");
    Check(r["token"]==token&&r["helper"]["observed_guest_bits"]==0x41F00000&&r["helper"]["readback_guest_bits"]==0x42700000,"receipt linked token and raw readback");
  }
  for(auto flag:{"native_admitted","configuration_route_qualified","configuration_persistence_proven",
      "object_lifetime_proven","state_coherence_proven","timing_binding_proven","simulation_proven","fps_proven"})Check(j[flag]==false,"no proof promotion");
  Fixture off(false);ctx=Context();Register(off,ctx);Run(off,ctx);auto legacy=Cut(off);
  Check(off.edits==0&&off.fresh_reads==0&&off.rate_seen==0x41F00000,"disabled identity has no helper or fresh read");
  Check(legacy["schema"]=="mcla-timer-config-v1"&&!legacy.contains("policy")&&!legacy.contains("interventions"),"disabled exact legacy schema surface");
  Check(legacy["records"][1]["route_class"]=="unclassified"&&legacy["observer_failed"]==true,"disabled original route classification preserved");
}
void RejectsAndFaults() {
  for(size_t index:{0u,2u,3u,5u,6u}){
    Fixture f;auto c=Context();Register(f,c);f.memory.words[index]^=1;Run(f,c);auto j=Cut(f);
    Check(f.edits==0&&f.applies==1&&j["observer_failed"]==true,"policy mismatch no edit, original once");
  }
  for(int fault=1;fault<=9;++fault){Fixture f;auto c=Context();Register(f,c);f.lexical_fault=fault;Run(f,c);
    Check(f.edits==0&&f.applies==1&&f.state->data.failed,"lexical/current route mismatch rejected without skipping original");}
  {Fixture f;auto c=Context();Register(f,c);Run(f,c,0x11111111);auto j=Cut(f);
   Check(f.edits==0&&f.applies==1&&j["counts"]["unclassified_routes"]==1,"unknown initializer remains sticky unclassified");}
  {Fixture f;auto c=Context();Run(f,c);Check(f.edits==0&&f.applies==1&&f.state->data.failed,
    "missing constructor association cannot authorize edit");}
  {Fixture f;auto c=Context();Register(f,c);f.registry_drift=true;Run(f,c);
   Check(f.edits==0&&f.applies==1&&f.state->data.failed,"constructor registry drift cannot borrow old initializer association");}
  {Fixture f;auto c=Context();Register(f,c);f.state->edit_word=nullptr;Run(f,c);
   Check(f.edits==0&&f.applies==1&&f.state->data.failed,"absent helper fails closed before write");}
  {Fixture f;auto c=Context();Register(f,c);f.state->data.token=std::string(32,'0');Run(f,c);
   Check(f.edits==0&&f.applies==1&&f.state->data.failed,"invalid captured token cannot authorize edit");}
  for(int failure=0;failure<8;++failure){Fixture f;auto c=Context();Register(f,c);
    if(failure==0)f.fresh_failure=true;
    if(failure==1)f.fresh_throw=true;
    if(failure==2)f.fresh_drift=true;
    if(failure==3)f.fresh_outside=true;
    if(failure==4)f.pre_failure=true;
    if(failure==5)f.state->data.failed=true;
    if(failure==6)f.state->emergency=true;
    if(failure==7)f.state->data.clock_failed=f.state->data.failed=true;
    Run(f,c);Check(f.edits==0&&f.applies==1&&f.state->data.failed,"read/clock/sticky health rejection preserves original");
  }
  for(bool freeze:{false,true}){Fixture f;auto c=Context();Register(f,c);
    f.freeze_before_apply=freeze;f.stop_before_apply=!freeze;Run(f,c);
    Check(f.edits==0&&f.applies==1,"stopped or frozen gate cannot edit");d::disarmed=false;
  }
  {Fixture f;auto c=Context();Register(f,c);f.state->data.intervention_count=d::kInterventionCapacity;
   Run(f,c);Check(f.edits==0&&f.applies==1&&f.state->data.intervention_unrecorded==1,"receipt capacity reserves before any edit");}
  {Fixture f;auto c=Context();Register(f,c);f.state->data.counts.stored=d::kCapacity-2;
   Run(f,c);Check(f.edits==0&&f.applies==1&&f.state->data.failed,"no edit without room for downstream observations");}
  for(auto mode:{EditMode::Reject,EditMode::Mismatch}){Fixture f;auto c=Context();Register(f,c);f.edit_mode=mode;Run(f,c);auto j=Cut(f);
    Check(f.edits==1&&f.applies==1&&f.memory.words[3]==0x41F00000,"helper prewrite reject invokes original once unchanged");
    Check(j["interventions"][0]["outcome"]=="rejected"&&j["observer_failed"]==true,"prewrite rejection sticky and retained");}
  for(auto mode:{EditMode::Ambiguous,EditMode::ReadFault,EditMode::ReadMismatch,EditMode::Throw,EditMode::BadVerified}){
    Fixture f;auto c=Context();Register(f,c);f.edit_mode=mode;bool caught=false;
    try{Run(f,c);}catch(const std::runtime_error&){caught=true;}
    auto j=Cut(f);Check(caught&&f.edits==1&&f.applies==0&&d::stack.depth==0,"possible postwrite abort before original and unwind");
    Check(j["interventions"][0]["outcome"]=="ambiguous_abort"&&j["counts"]["pending_entries"]==1,"ambiguous receipt retains uncalled pending Apply");
    if(mode==EditMode::ReadFault||mode==EditMode::ReadMismatch||mode==EditMode::BadVerified)
      Check(f.memory.words[3]==0x42700000,"possible mutation is never rolled back");
  }
  for(bool rollback:{false,true}){Fixture f;auto c=Context();Register(f,c);
    f.post_edit_rollback=rollback;f.post_edit_outside=!rollback;bool caught=false;
    try{Run(f,c);}catch(const std::runtime_error&){caught=true;}
    auto j=Cut(f);Check(caught&&f.edits==1&&f.applies==0&&f.memory.words[3]==0x42700000,
      "post-CAS bad clock aborts before original without rollback");
    Check(j["interventions"][0]["outcome"]=="ambiguous_abort"&&j["observer_failed"]==true,
      "verified word alone cannot bypass completion clock gate");
  }
  {Fixture f;auto c=Context();Register(f,c);f.throw_original=true;bool caught=false;
   try{Run(f,c);}catch(int value){caught=value==42;}
   Check(caught&&f.applies==1&&f.edits==1&&c.r7.u64==123&&GetLastError()==1234,"original exception preserved exactly once after verified edit");
   auto j=Cut(f);Check(j["counts"]["original_exceptions"]==2&&j["counts"]["pending_entries"]==0,"original throws close both lexical rows without deadlock");}
  {Fixture f;auto c=Context();Register(f,c);f.disturb=true;const auto csr=_mm_getcsr();SetLastError(77);Run(f,c);
   Check(f.edits==1&&c.r31.u64==77&&GetLastError()==77&&_mm_getcsr()==csr,"observer helper preserves PPC LastError MXCSR");}
}
void FreezeAndDisarmRaces() {
  for(bool disarm:{false,true}) {
    Fixture f;auto c=Context();Register(f,c);f.block_edit=f.block_original=true;
    std::thread worker([&]{current=&f;Run(f,c);});
    Wait([&]{return f.in_edit.load();});
    const bool acquired=f.state->mutex.try_lock();if(acquired)f.state->mutex.unlock();
    Check(!acquired,"CAS stage holds shared freeze/disarm lock");
    std::atomic<bool> cutter_started{},cut_done{};std::string frozen;
    d::configured=f.state.get();
    std::thread cutter([&]{current=&f;cutter_started=true;
      if(disarm)api::Disarm();else frozen=d::Cut(*f.state,token);cut_done=true;});
    Wait([&]{return cutter_started.load();});Check(!cut_done,"cut cannot complete across in-flight edit");
    f.release_edit=true;Wait([&]{return f.in_original.load();});Wait([&]{return cut_done.load();});cutter.join();
    if(disarm){current=&f;frozen=d::Cut(*f.state,token);}
    auto j=Json::parse(frozen);
    Check(f.edits==1&&j["counts"]["pending_entries"]==2&&j["observer_failed"]==true,
          "freeze after CAS before original return retains two pending rows and failure");
    Check(j["interventions"][0]["outcome"]=="changed_verified","linearized completed intervention survives pending original cut");
    f.release_original=true;worker.join();current=&f;
    Check(d::Cut(*f.state,token)==frozen&&f.applies==1,"late original completion cannot mutate frozen receipt");
    d::configured=nullptr;d::disarmed=false;
  }
}
} // namespace
REX_EXTERN(__imp__sub_822D2818){Ctor(ctx,base);}
REX_EXTERN(__imp__sub_822D2548){Initializer(ctx,base);}
REX_EXTERN(__imp__sub_822D2478){Apply(ctx,base);}
REX_EXTERN(__imp__sub_821BD9B8){Setter(ctx,base);}
REX_EXTERN(__imp__sub_821BD9F8){Setter(ctx,base);}
int main(int argc,char** argv) {
  if(argc==2&&std::string(argv[1])=="--emit") {
    Fixture f;auto c=Context();Register(f,c);Run(f,c);f.memory=Stock();Run(f,c,0x822C6C9C);
    current=&f;std::cout<<d::Cut(*f.state,token)<<'\n';return failures?1:0;
  }
  Check(!api::ExperimentEnabled(),"independent experiment default OFF");
  PositiveAndDisabled();RejectsAndFaults();FreezeAndDisarmRaces();
  std::cout<<"fixed60 config: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
