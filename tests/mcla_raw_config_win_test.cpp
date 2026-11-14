// SOURCE-ONLY synthetic regression fixture. Build with both private macros.
// No game, GPU, save, desktop, filesystem, or production process is launched.
#if !defined(MCLA_RAW_CONFIG_EXPERIMENT)
#error Raw wrapper test requires its separate explicit experiment macro.
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
namespace edit=mcla::raw_config_word_edit;
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
enum class EditMode { Good, Reject, Mismatch, Ambiguous, ReadFault, ReadMismatch, Throw, ThrowAfterWrite, BadVerified };
struct Fixture {
  std::unique_ptr<d::State> state=std::make_unique<d::State>();
  d::Config memory{};
  std::atomic<uint64_t> ticks{100};
  std::atomic<unsigned> constructors{},initializers{},applies{},setters{},edits{},fresh_reads{};
  std::atomic<bool> in_edit{},release_edit{},in_original{},release_original{};
  std::array<EditMode,2> edit_modes{EditMode::Good,EditMode::Good};
  unsigned bad_read{},bad_read_kind{},clock_failure_at{},clock_failure_kind{};
  std::atomic<unsigned> clock_calls{};
  bool block_second{},between_registry_drift{};
  bool fresh_failure{},fresh_throw{},fresh_drift{},fresh_outside{},block_edit{},block_original{};
  bool throw_original{},disturb{},pre_failure{},freeze_before_apply{},stop_before_apply{};
  bool post_edit_rollback{},post_edit_outside{},registry_drift{};
  int lexical_fault{};
  PPCContext* live_context{};
  uint32_t rate_seen{},count_seen{},mode_seen{},ceiling_seen{};
  explicit Fixture(bool experiment=true);
};
uint8_t* Base(){return reinterpret_cast<uint8_t*>(0x10000);}
PPCContext Context(){PPCContext c{};c.r3.u64=0x1000;c.lr=0x822D29C0;c.r31.u64=77;return c;}
d::Config Stock(){d::Config c{};c.words={0,17,0x3F800000,0x41F00000,0x41F00000,1,2};c.byte100=1;return c;}
uint64_t Clock(){
  auto& f=*current;const auto call=++f.clock_calls;
  if(f.clock_failure_at==call)return f.clock_failure_kind==0?0:f.clock_failure_kind==1?1:f.state->data.end;
  return f.ticks.fetch_add(1);
}
void Disturb(){SetLastError(888);_mm_setcsr(_mm_getcsr()^0x2000);if(current->live_context)current->live_context->r31.u64=0xBAD;}
bool ReadConfig(uintptr_t,uint32_t,d::Config& out){out=current->memory;if(current->disturb)Disturb();return !current->pre_failure;}
bool ReadTimer(uintptr_t,uint32_t,mcla::step_timer_memory::State& out){out.count=2;if(current->disturb)Disturb();return !current->pre_failure;}
bool Fresh(uintptr_t,uint32_t,d::Config& out) {
  auto& f=*current;const auto number=++f.fresh_reads;out=f.memory;
  if(f.bad_read==number){
    if(f.bad_read_kind==0)return false;
    if(f.bad_read_kind==1)throw 19;
    if(f.bad_read_kind==2)out.words[1]^=1;
    if(f.bad_read_kind==3)f.ticks=f.state->data.end;
  }
  if(f.between_registry_drift&&number==2)f.state->associations[0].ctor+=1;
  if(f.disturb)Disturb();return true;
}
edit::Result Verified(edit::Stage stage) {
  edit::Result r;r.outcome=edit::Outcome::ChangedVerified;
  r.query_attempted=r.region_accepted=r.cas_attempted=r.cas_completed=true;
  r.observed_available=r.changed=r.readback_attempted=r.readback_available=true;
  r.observed_guest_bits=edit::Expected(stage);r.readback_guest_bits=edit::Replacement(stage);return r;
}
edit::Result Edit(bool enabled,edit::Stage stage,uintptr_t base,uint32_t address) {
  auto& f=*current;const auto number=++f.edits;const auto index=stage==edit::Stage::ClearCeiling88?0u:1u;
  Check(number==index+1,"ordered exact two stage calls");
  Check(enabled&&base==reinterpret_cast<uintptr_t>(Base())&&address==0x1000,"helper exact enabled/address arguments");
  if(f.block_edit&&((index==1)==f.block_second)){f.in_edit=true;Wait([&]{return f.release_edit.load();});}
  if(f.disturb)Disturb();const auto mode=f.edit_modes[index];
  if(mode==EditMode::Throw)throw 18;
  if(mode==EditMode::Reject){edit::Result r;r.outcome=edit::Outcome::RejectedRegion;r.query_attempted=true;return r;}
  if(mode==EditMode::Mismatch){edit::Result r;r.outcome=edit::Outcome::CompareMismatch;
    r.query_attempted=r.region_accepted=r.cas_attempted=r.cas_completed=r.observed_available=true;
    r.observed_guest_bits=0x42F00000;return r;}
  if(mode==EditMode::Ambiguous){edit::Result r;r.outcome=edit::Outcome::CasFaultAmbiguous;
    r.query_attempted=r.region_accepted=r.cas_attempted=true;r.exception_code=EXCEPTION_ACCESS_VIOLATION;return r;}
  f.memory.words[index==0?4:0]=edit::Replacement(stage);auto result=Verified(stage);
  if(mode==EditMode::ThrowAfterWrite)throw 18;
  if(mode==EditMode::ReadFault){result.outcome=edit::Outcome::ChangedReadbackFault;result.readback_available=false;
    result.readback_guest_bits=0;result.exception_code=EXCEPTION_ACCESS_VIOLATION;}
  if(mode==EditMode::ReadMismatch){result.outcome=edit::Outcome::ChangedReadbackMismatch;result.readback_guest_bits=0x41F00000;}
  if(mode==EditMode::BadVerified)result.cas_completed=false;
  return result;
}
Fixture::Fixture(bool experiment) {
  memory=Stock();state->data.begin=100;state->data.end=100000;state->data.frequency=1000;
  state->data.token=token;state->data.raw=experiment;state->clock=Clock;
  state->config_reader=ReadConfig;state->timer_reader=ReadTimer;
  state->raw_config_reader=Fresh;state->raw_edit_word=Edit;
}
void Ctor(PPCContext&,uint8_t*){++current->constructors;}
void Setter(PPCContext&,uint8_t*){++current->setters;}
void Apply(PPCContext& ctx,uint8_t* base) {
  auto& f=*current;++f.applies;f.rate_seen=f.memory.words[3];f.count_seen=f.memory.words[6];f.mode_seen=f.memory.words[0];f.ceiling_seen=f.memory.words[4];
  if(f.block_original){f.in_original=true;Wait([&]{return f.release_original.load();});}
  if(f.throw_original){ctx.r7.u64=123;SetLastError(1234);_mm_setcsr(0x5F80);throw 42;}
  const auto r3=ctx.r3.u64;ctx.r3.u32=d::kTimer;
  ctx.lr=0x822D24CC;d::Invoke(f.state.get(),d::Kind::RawSetter,Setter,ctx,base);
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
void Call(Fixture& f,PPCContext& ctx,uint64_t lr=0x822C696C){current=&f;f.live_context=&ctx;ctx.lr=lr;d::Invoke(f.state.get(),d::Kind::Initializer,Initializer,ctx,Base());}
Json Cut(Fixture& f){current=&f;return Json::parse(d::Cut(*f.state,token));}

bool Attempt(Fixture& f,PPCContext& ctx,uint64_t lr=0x822C696C) {
  try{Call(f,ctx,lr);return false;}catch(const std::runtime_error&){return true;}
}
void PositiveAndDisabled(){
  Fixture f;auto c=Context();Register(f,c);
  for(auto lr:{0x822C696Cull,0x822C6C9Cull}){
    f.memory=Stock();f.edits=0;f.fresh_reads=0;Check(!Attempt(f,c,lr),"legitimate raw chain proceeds");
  }
  auto j=Cut(f);
  Check(f.applies==2&&f.setters==2&&f.mode_seen==1&&f.ceiling_seen==0&&
    f.rate_seen==0x41F00000&&f.count_seen==2,"original sees raw/configured ceiling only");
  auto expected=Stock();expected.words[0]=1;expected.words[4]=0;
  Check(f.memory.words==expected.words&&f.memory.byte100==expected.byte100,"only two declared config fields differ");
  Check(j["schema"]=="mcla-timer-config-raw-v1"&&!j["observer_failed"].get<bool>()&&
    j["raw_interventions"].size()==2&&j["raw_intervention_serial"]==2,"separate healthy diagnostic wire");
  for(const auto& r:j["raw_interventions"]){
    Check(r["outcome"]=="verified"&&r["original_permitted"]==true&&r["edit_started"]==true&&
      r["may_have_changed"]==true&&r["policy_admitted"]==true,"verified conservative receipt");
    Check(r["reads"].size()==3&&r["stages"].size()==2,"fixed slot counts");
    Check(r["reads"][0]["config"]["words"][0]==0&&r["reads"][0]["config"]["words"][4]==0x41F00000&&
      r["reads"][1]["config"]["words"][0]==0&&r["reads"][1]["config"]["words"][4]==0&&
      r["reads"][2]["config"]["words"][0]==1&&r["reads"][2]["config"]["words"][4]==0,"three exact sampled configurations");
    uint64_t previous=r["decision_qpc"];
    for(unsigned i=0;i<3;++i){
      const auto& read=r["reads"][i];Check(read["before_qpc"].get<uint64_t>()>=previous&&
        read["after_qpc"].get<uint64_t>()>=read["before_qpc"].get<uint64_t>(),"read clock bracket");
      previous=read["after_qpc"];
      if(i<2){const auto& stage=r["stages"][i];Check(stage["helper_called"]==true&&stage["helper_returned"]==true&&
        stage["prewrite_qpc"].get<uint64_t>()>=previous&&stage["completion_qpc"].get<uint64_t>()>=stage["prewrite_qpc"].get<uint64_t>(),"ordered helper clocks");
        previous=stage["completion_qpc"];}
    }
  }
  for(auto flag:{"native_admitted","configuration_route_qualified","configuration_persistence_proven",
    "object_lifetime_proven","state_coherence_proven","timing_binding_proven","simulation_proven","fps_proven"})
    Check(j[flag]==false,"no proof promotion");
  Fixture off(false);c=Context();Register(off,c);Check(!Attempt(off,c),"disabled original once");
  auto legacy=Cut(off);
  Check(off.applies==1&&off.edits==0&&off.fresh_reads==0&&off.mode_seen==0,"disabled no fresh IO/edit");
  Check(legacy["schema"]=="mcla-timer-config-v1"&&!legacy.contains("policy")&&!legacy.contains("raw_interventions"),"legacy schema unchanged");
  Check(legacy["records"][1]["route_class"]=="unclassified","legacy classification unchanged");
  Fixture both;c=Context();Register(both,c);both.state->data.fixed60=true;
  Check(Attempt(both,c)&&both.applies==0&&both.edits==0,"both immutable selections reject before edit");
}
void Preflight(){
  for(size_t i:{0u,2u,3u,4u,5u,6u}){Fixture f;auto c=Context();Register(f,c);f.memory.words[i]^=1;
    Check(Attempt(f,c)&&f.applies==0&&f.edits==0&&f.state->data.failed,"policy rejects without stock fallback");}
  for(int fault=1;fault<=9;++fault){Fixture f;auto c=Context();Register(f,c);f.lexical_fault=fault;
    Check(Attempt(f,c)&&f.applies==0&&f.edits==0&&d::stack.depth==0,"exact lexical negative abort");}
  {Fixture f;auto c=Context();Register(f,c);Check(Attempt(f,c,0x11111111)&&f.applies==0&&f.edits==0,"unclassified initializer abort");}
  {Fixture f;auto c=Context();Check(Attempt(f,c)&&f.applies==0&&f.edits==0,"unregistered config abort");}
  for(unsigned fault=0;fault<9;++fault){Fixture f;auto c=Context();Register(f,c);
    if(fault==0)f.registry_drift=true;if(fault==1)f.state->raw_edit_word=nullptr;
    if(fault==2)f.state->data.token=std::string(32,'0');if(fault==3)f.pre_failure=true;
    if(fault==4)f.state->data.failed=true;if(fault==5)f.state->emergency=true;
    if(fault==6)f.state->data.clock_failed=true;if(fault==7)f.state->data.counts.saturated=true;
    if(fault==8)f.state->raw_config_reader=nullptr;
    Check(Attempt(f,c)&&f.applies==0&&f.edits==0,"health/read/helper preflight abort");
  }
  for(bool freeze:{false,true}){Fixture f;auto c=Context();Register(f,c);
    f.freeze_before_apply=freeze;f.stop_before_apply=!freeze;
    Check(Attempt(f,c)&&f.applies==0&&f.edits==0,"frozen/stopped Apply must abort");d::disarmed=false;
  }
  {Fixture f;auto c=Context();Register(f,c);f.state->data.raw_count=d::kRawInterventionCapacity;
    Check(Attempt(f,c)&&f.applies==0&&f.edits==0&&f.state->data.raw_unrecorded==1,"receipt capacity before writes");}
  for(auto stored:{d::kCapacity-2,d::kCapacity-1,d::kCapacity}){Fixture f;auto c=Context();Register(f,c);f.state->data.counts.stored=stored;
    Check(Attempt(f,c)&&f.applies==0&&f.edits==0,"row capacity/untracked ticket abort");}
  {Fixture f;auto c=Context();Register(f,c);Check(!Attempt(f,c),"first application accepted");
    Check(Attempt(f,c)&&f.applies==1&&f.edits==2,"already raw rejected without reedit/original");}
}
void SnapshotsAndClocks(){
  for(unsigned read=1;read<=3;++read)for(unsigned kind=0;kind<4;++kind){
    Fixture f;auto c=Context();Register(f,c);f.bad_read=read;f.bad_read_kind=kind;
    Check(Attempt(f,c)&&f.applies==0&&f.edits==read-1,"each fresh read failure/drift/clock aborts without later edit");
    auto j=Cut(f);const auto& r=j["raw_interventions"][0];
    Check(r["original_permitted"]==false&&r["edit_started"]==(read>1)&&
      r["outcome"]==(read>1?"partial_or_ambiguous_abort":"rejected_abort"),"snapshot failure retains stage/partial distinction");
  }
  {Fixture f;auto c=Context();Register(f,c);f.between_registry_drift=true;
    Check(Attempt(f,c)&&f.applies==0&&f.edits==1&&f.memory.words[4]==0,"between-stage association drift preserves partial and aborts");}
  // 1 initializer entry;2 Apply entry;3 decision;4/5 before-read;6/7 first
  // helper;8/9 between-read;10/11 second helper;12/13 final-read.
  for(unsigned at=1;at<=13;++at)for(unsigned kind=0;kind<3;++kind){
    Fixture f;auto c=Context();Register(f,c);f.clock_calls=0;f.clock_failure_at=at;f.clock_failure_kind=kind;
    Check(Attempt(f,c)&&f.applies==0,"every pre-original zero/reversed/outside sampled clock aborts");
    Check(d::stack.depth==0,"clock rejection unwinds lexical scopes");
  }
}
void HelperOutcomes(){
  for(unsigned slot=0;slot<2;++slot)for(auto mode:{EditMode::Reject,EditMode::Mismatch,EditMode::Ambiguous,
      EditMode::ReadFault,EditMode::ReadMismatch,EditMode::Throw,EditMode::ThrowAfterWrite,EditMode::BadVerified}){
    Fixture f;auto c=Context();Register(f,c);f.edit_modes[slot]=mode;
    Check(Attempt(f,c)&&f.edits==slot+1&&f.applies==0&&f.setters==0&&d::stack.depth==0,"all nonverified helper outcomes abort before original");
    auto j=Cut(f);const auto& r=j["raw_interventions"][0];
    Check(r["edit_started"]==true&&r["may_have_changed"]==true&&r["original_permitted"]==false&&
      r["outcome"]=="partial_or_ambiguous_abort"&&j["counts"]["pending_entries"]==1,"sticky state precedes callback and pending Apply retained");
    if(slot)Check(f.memory.words[4]==0,"second proven-no-write cannot roll back or forget first change");
    if(mode==EditMode::Throw||mode==EditMode::ThrowAfterWrite)
      Check(r["stages"][slot]["helper"].is_null()&&r["stages"][slot]["helper_returned"]==false,"unknown throw has no fabricated return");
  }
}
void OriginalAndPreservation(){
  {Fixture f;auto c=Context();Register(f,c);f.throw_original=true;const auto csr=_mm_getcsr();bool caught=false;
    try{Call(f,c);}catch(int value){caught=value==42;}
    Check(caught&&f.applies==1&&f.edits==2&&c.r7.u64==123&&GetLastError()==1234&&_mm_getcsr()==0x5F80,"original effects and exception propagated");
    _mm_setcsr(csr);auto j=Cut(f);Check(j["counts"]["original_exceptions"]==2&&j["counts"]["pending_entries"]==0,"original exception normal unwind accounting");}
  {Fixture f;auto c=Context();Register(f,c);f.disturb=true;const auto csr=_mm_getcsr();SetLastError(77);
    Check(!Attempt(f,c)&&f.edits==2&&c.r31.u64==77&&GetLastError()==77&&_mm_getcsr()==csr,"observer stages preserve PPC LastError MXCSR");}
}
void FreezeAndDisarm(){
  for(bool disarm:{false,true})for(bool second:{false,true}){
    Fixture f;auto c=Context();Register(f,c);f.block_edit=f.block_original=true;f.block_second=second;
    std::thread worker([&]{current=&f;Call(f,c);});
    Wait([&]{return f.in_edit.load();});const bool acquired=f.state->mutex.try_lock();if(acquired)f.state->mutex.unlock();
    Check(!acquired,"both helper slots serialize freeze/disarm");
    std::atomic<bool> started{},done{};std::string frozen;d::configured=f.state.get();
    std::thread cutter([&]{current=&f;started=true;if(disarm)api::Disarm();else frozen=d::Cut(*f.state,token);done=true;});
    Wait([&]{return started.load();});Check(!done,"cut cannot cross edit");
    f.release_edit=true;Wait([&]{return f.in_original.load();});Wait([&]{return done.load();});cutter.join();
    if(disarm){current=&f;frozen=d::Cut(*f.state,token);}
    auto j=Json::parse(frozen);
    Check(f.edits==2&&j["counts"]["pending_entries"]==2&&j["observer_failed"]==true&&
      j["raw_interventions"][0]["outcome"]=="verified","cut retains completed intervention with pending original");
    f.release_original=true;worker.join();current=&f;
    Check(d::Cut(*f.state,token)==frozen&&f.applies==1,"late originals cannot mutate frozen cut");
    d::configured=nullptr;d::disarmed=false;
  }
}
void FlagSelection(){
  REXCVAR_SET(mcla_raw_config_experiment,true);
  Check(api::RawExperimentEnabled(),"raw flag explicitly enabled only for selection test");
  api::ValidateExperimentSelection();
#if defined(MCLA_FIXED60_CONFIG_EXPERIMENT)
  REXCVAR_SET(mcla_fixed60_config_experiment,true);
  bool rejected=false;
  try{api::ValidateExperimentSelection();}catch(const std::runtime_error&){rejected=true;}
  Check(rejected,"actual both flags reject before install/begin");
  REXCVAR_SET(mcla_fixed60_config_experiment,false);
#endif
  REXCVAR_SET(mcla_raw_config_experiment,false);
  Check(!api::RawExperimentEnabled(),"selection test restores default off");
}
} // namespace
REX_EXTERN(__imp__sub_822D2818){Ctor(ctx,base);}
REX_EXTERN(__imp__sub_822D2548){Initializer(ctx,base);}
REX_EXTERN(__imp__sub_822D2478){Apply(ctx,base);}
REX_EXTERN(__imp__sub_821BD9B8){Setter(ctx,base);}
REX_EXTERN(__imp__sub_821BD9F8){Setter(ctx,base);}
int main(int argc,char** argv){
  if(argc==2&&(std::string(argv[1])=="--emit-abort"||std::string(argv[1])=="--emit-rejected")){
    Fixture f;auto c=Context();Register(f,c);
    if(std::string(argv[1])=="--emit-abort")f.edit_modes[1]=EditMode::ThrowAfterWrite;
    else f.memory.words[4]^=1;
    const bool aborted=Attempt(f,c);current=&f;
    std::cout<<d::Cut(*f.state,token)<<'\n';return aborted&&!failures?0:1;
  }
  if(argc==2&&std::string(argv[1])=="--emit"){
    Fixture f;auto c=Context();Register(f,c);Call(f,c);f.memory=Stock();f.edits=0;f.fresh_reads=0;Call(f,c,0x822C6C9C);
    current=&f;std::cout<<d::Cut(*f.state,token)<<'\n';return failures?1:0;
  }
  Check(!api::RawExperimentEnabled()&&!api::ExperimentEnabled(),"both flags default OFF");
  api::ValidateExperimentSelection();
  PositiveAndDisabled();Preflight();SnapshotsAndClocks();HelperOutcomes();OriginalAndPreservation();FreezeAndDisarm();FlagSelection();
  std::cout<<"raw config: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
