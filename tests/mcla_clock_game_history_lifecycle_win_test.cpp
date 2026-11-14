// ONE-TU white-box controller prerequisite with the ACTUAL paired SDK.
// Each mode is a fresh process; no game, Runtime, reset, unload or source-frame
// claim. Injected pointer/descriptor/raw controls are explicitly NOT SDK bugs.
// Missing-API RED was NOT executed; primary owns reviewed native negatives.
#include "../src/mcla_clock_game_bridge_win.cpp"
#include "../src/mcla_timer_observer_core.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include <string_view>
#include <thread>

namespace {
namespace cg = mcla::clock_game;
namespace obs = mcla::timer_observer;
namespace tj = mcla::timer_journal;
namespace hj = mcla::clock_read_witness::host_journal;
using Observer = obs::Observer<4>;
constexpr tj::Window window{100,200,1000}; // Synthetic admission metadata, NOT QPC measurement.
constexpr tj::Identity timer_identity{0x827D7500,0x822C21B4,31};
const std::string token = "123456789abcdef0fedcba9876543210";
constexpr uint64_t high=0x123456789ABCDEF0ull, low=0xFEDCBA9876543210ull;
unsigned checks{}, failures{}, injected_calls{};
bool injected_snapshot_saw_both_stops{};
struct Native {
  uint32_t error, mxcsr;
  static Native Capture() noexcept {const auto e=GetLastError();return {e,_mm_getcsr()};}
  void Restore() const noexcept {_mm_setcsr(mxcsr);SetLastError(error);}
  bool operator==(const Native&) const = default;
};
void Check(bool value,const char* label) {
  const auto saved=Native::Capture();Native{0,0x1F80}.Restore();
  ++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}saved.Restore();
}
template<class Call> bool Rejected(Call&& call) {
  try{call();return false;}catch(const std::exception&){return true;}
}
cg::State* Owned(){return cg::state.load(std::memory_order_acquire);}
void Begin(bool history=true) {
  if(history)cg::InstallWithHistory();else cg::Install();
  cg::Begin(window.begin_qpc,window.end_qpc,window.qpc_frequency,token);
}
hj::Invocation Raw(uint64_t ordinal){return {ordinal,700,9,31,timer_identity.timer_address,timer_identity.caller_address,31,1};}
void Emit(const char* prefix,const std::string& text){std::cout<<prefix<<' '<<text<<'\n';}
struct Signal{};
struct Sync {std::atomic<bool> entered{},release{},done{},unexpected{};};
struct Body {
  PPCContext context{};uint8_t bytes[4]{};uint8_t* base=bytes;
  uint64_t results[2]{},ordinal{};unsigned originals{},reads{};
  const void *actual_context{},*actual_base{};
  Native incoming{},produced{},external{};
  bool unwind{},caught{};Sync* sync{};
};
constinit thread_local Body* active_body=nullptr;
__declspec(noinline) void Original(PPCContext& context,uint8_t* base) {
  auto& b=*active_body;++b.originals;b.actual_context=&context;b.actual_base=base;
  b.incoming=Native::Capture();
  for(unsigned i=0;i<2;++i) {
    b.results[i]=cg::Read(context,base);++b.reads;
    if(i==0 && b.sync) {
      b.sync->entered.store(true,std::memory_order_release);
      while(!b.sync->release.load(std::memory_order_acquire))std::this_thread::yield();
    }
  }
  ++context.r20.u32;++base[0];b.produced=Native::Capture();
  if(b.unwind)throw Signal{};
}
obs::Operations<PPCContext,uint8_t*> Operations(uint64_t& qpc) {
  return {[&qpc]{return qpc++;},[](const PPCContext& c,uint8_t* const&,tj::State& s){s.words[0]=c.r20.u32;return true;},
      [](const PPCContext&,uint8_t* const&){return obs::ScopeBinding{700,9,31};}};
}
void Primer(Observer& observer,uint64_t& qpc) {
  auto b=std::make_unique<Body>();auto ops=Operations(qpc);
  observer.invoke(timer_identity,b->context,b->base,ops,0,0,[](PPCContext&,uint8_t*&){ });
}
void Observed(Observer& observer,uint64_t& qpc,Body& b) {
  auto ops=Operations(qpc);active_body=&b;const Native incoming{0x13572468,0x3F80};
  Native hook{};bool captured=false;
  try {
    observer.invoke_with_ordinal(timer_identity,b.context,b.base,ops,0,0,
        [&](PPCContext& context,uint8_t*& base,uint64_t ordinal){
          b.ordinal=ordinal;incoming.Restore();
          try{cg::Invoke(Original,context,base,Raw(ordinal));}
          catch(...){hook=Native::Capture();captured=true;throw;}
          hook=Native::Capture();captured=true;
        });
  }catch(const Signal&){b.caught=true;}
  if(captured)hook.Restore();b.external=Native::Capture();active_body=nullptr;
  Check(b.originals==1 && b.reads==2 && b.actual_context==&b.context && b.actual_base==b.bytes &&
      b.context.r20.u32==1 && b.bytes[0]==1,"original exact context/base and two reads occur once");
  Check(b.incoming==incoming && b.caught==b.unwind && captured && b.external==hook,
      "original incoming and captured return/unwind state survive timer Completion");
}
bool Drain(Observer& observer) {
  observer.stop_admissions();auto snapshot=std::make_unique<Observer::Snapshot>();
  const bool ready=observer.try_freeze_into(*snapshot);
  Check(ready,"actual admitted timer Completions drain before successful controller cut");
  if(ready)std::cout<<"TIMER_DRAIN admitted="<<snapshot->counts().admitted_entries
      <<" completed="<<snapshot->counts().completed_entries<<'\n';
  return ready;
}
bool Arithmetic(const MclaClockHistoryRecordV1& r,uint64_t result) {
  const auto& m=r.metadata;
  if(m.presence!=63 || !m.pre_ratio_denominator || m.pre_ratio_numerator!=m.post_ratio_numerator ||
      m.pre_ratio_denominator!=m.post_ratio_denominator || m.returned_guest!=result || m.returned_guest!=m.post_guest)return false;
  if(r.kind==MCLA_CH_KIND_ADVANCE) {
    const uint64_t delta=m.host_sample>m.pre_host?m.host_sample-m.pre_host:0;
    return m.post_host==m.host_sample && m.post_guest==m.pre_guest+(delta*m.pre_ratio_numerator)/m.pre_ratio_denominator;
  }
  return r.kind==MCLA_CH_KIND_CACHED && m.pre_host==m.post_host && m.pre_guest==m.post_guest;
}
void CheckHistory(const Body& b,uint64_t unbound,uint64_t frequency) {
  const auto& h=*Owned()->history_snapshot;const auto& v=*Owned()->sdk_snapshot;
  Check(h.row_count==4 && h.run_high==high && h.run_low==low && h.observed_attempts==4 && h.mutex_ordinals==4 &&
      !h.unordered_attempts && h.stored_entries==4 && h.published_entries==4 && !h.aborted_entries && !h.overflow_entries &&
      !h.diagnostic_entries && !h.saturated_rejections && !h.counter_saturated && !h.proof_flags,
      "history retains two bound queries plus one unbound query and one ratio commit");
  Check(v.row_count==2 && Owned()->host_snapshot->size==1 && b.ordinal==2,
      "V2 and host inventories remain separate from global history and primer storage");
  for(unsigned i=0;i<4;++i)Check(h.rows[i].storage_ordinal==i+1 && h.rows[i].mutex_ordinal==i+1 &&
      h.rows[i].ordering==MCLA_CH_ORDERING_MUTEX && h.rows[i].disposition==MCLA_CH_PUBLISHED && !h.rows[i].issues,
      "all serial source admission identities and raw dispositions retained");
  for(unsigned i=0;i<2;++i) {
    const auto& r=h.rows[i];const auto& request=v.rows[i].binding;
    Check(r.binding.present==1 && r.binding.run_high==high && r.binding.run_low==low &&
        r.binding.timer_invocation==b.ordinal && r.binding.read_ordinal==i && r.binding.phase_call_id==700 &&
        r.binding.phase_thread_epoch==9 && r.binding.phase_native_thread_id==31 && r.binding.phase_present==1 &&
        !r.binding.reserved && request.timer_invocation==b.ordinal && request.read_ordinal==i &&
        v.rows[i].returned_guest==b.results[i] && Arithmetic(r,b.results[i]),
        "history exact bound request and returned value join V2 without nearest time");
  }
  Check(!h.rows[2].binding.present && Arithmetic(h.rows[2],unbound),"plain actual SDK query is retained unbound");
  const auto& ratio=h.rows[3];const auto& m=ratio.metadata;
  const uint64_t numerator=50000000ull*10,denominator=frequency*10,g=std::gcd(numerator,denominator);
  Check(ratio.kind==MCLA_CH_KIND_RATIO && !ratio.binding.present && m.presence==60 &&
      !m.host_sample && !m.returned_guest && m.pre_host==m.post_host && m.pre_guest==m.post_guest &&
      m.post_ratio_numerator==numerator/g && m.post_ratio_denominator==denominator/g,
      "effective ratio setter is a raw unbound clock-preserving source commit");
}

// Injectable function pointers are WHITE-BOX consumer controls only. No hooks,
// replacements or extra exports are added to the SDK or production controller.
MclaClockWitnessStopFnV2 real_v2_stop{};
MclaClockHistoryStopFnV1 real_history_stop{};
MclaClockHistorySnapshotFnV1 real_history_snapshot{};
uint32_t MCLA_CW_CALL BadV2Snapshot(MclaClockWitnessSnapshotV2*) {
  ++injected_calls;const auto* owned=Owned();
  injected_snapshot_saw_both_stops=owned->host_frozen && owned->v2_stop_requested && owned->v2_stop_succeeded &&
      owned->history_stop_requested && owned->history_stop_succeeded;
  return MCLA_CW_BAD_ABI;
}
uint32_t MCLA_CH_CALL PendingHistory(MclaClockHistorySnapshotV1*){++injected_calls;return MCLA_CH_PENDING;}
void MCLA_CW_CALL ThrowV2Stop(){++injected_calls;throw Signal{};}
void MCLA_CH_CALL ThrowHistoryStop(){++injected_calls;throw Signal{};}
uint32_t MCLA_CH_CALL RawHistory(MclaClockHistorySnapshotV1* out) {
  ++injected_calls;const auto status=real_history_snapshot(out);if(status!=MCLA_CH_OK)return status;
  // Deliberate raw contradictions, NOT asserted as real SDK emissions.
  out->overflow_entries=17;out->proof_flags=65535;out->counts_reserved=123;out->reserved=456;
  out->rows[0].kind=57005;out->rows[0].binding.reserved=29;
  out->rows[0].metadata.presence=UINT32_MAX;out->rows[0].metadata.reserved=0xAABB;
  out->rows[0].issues=UINT32_MAX;return status;
}
uint32_t MCLA_CH_CALL CapacityHistory(MclaClockHistorySnapshotV1* out) {
  ++injected_calls;const auto status=real_history_snapshot(out);if(status!=MCLA_CH_OK)return status;
  // Synthetic wire expansion ONLY, not4096 actual SDK operations/coverage.
  const auto seed=out->rows[0];
  for(uint32_t i=0;i<4096;++i) {
    out->rows[i]=seed;out->rows[i].storage_ordinal=uint64_t(i)+1;
    out->rows[i].binding.read_ordinal=i;out->rows[i].metadata.returned_guest=UINT64_MAX-i;
  }
  out->row_count=4096;out->overflow_entries=19;return status;
}
uint32_t MCLA_CH_CALL BoundsHistory(MclaClockHistorySnapshotV1* out) {
  ++injected_calls;const auto status=real_history_snapshot(out);if(status==MCLA_CH_OK)out->row_count=4097;return status;
}
void WorkAndCut(std::string_view mode) {
  const bool on=mode!="off";Begin(on);auto* owned=Owned();
  Check(owned->history_enabled==on && owned->history_armed==on &&
      (on?owned->history_snapshot!=nullptr:owned->history_snapshot==nullptr),"history opt-in and init-preallocated storage are explicit");
  auto observer=std::make_unique<Observer>(window,true);uint64_t qpc=110;Primer(*observer,qpc);
  auto b=std::make_unique<Body>();b->unwind=mode=="normal-unwind";Observed(*observer,qpc,*b);
  const uint64_t unbound=rex::chrono::Clock::QueryGuestTickCount();
  const uint64_t frequency=rex::chrono::Clock::QueryHostTickFrequency();
  Check(frequency && frequency<=UINT64_MAX/20,"actual frequency permits bounded source integer ratio oracle");
  if(!frequency || frequency>UINT64_MAX/20)return;
  rex::chrono::Clock::set_guest_tick_frequency(50000000);
  if(!Drain(*observer))return;
  real_v2_stop=owned->api.stop;real_history_stop=owned->history_api.stop;real_history_snapshot=owned->history_api.snapshot;
  if(mode=="v2-snapshot-injected")owned->api.snapshot=BadV2Snapshot;
  if(mode=="v2-stop-injected")owned->api.stop=ThrowV2Stop;
  if(mode=="history-stop-injected")owned->history_api.stop=ThrowHistoryStop;
  if(mode=="history-pending-injected")owned->history_api.snapshot=PendingHistory;
  if(mode=="raw-injected")owned->history_api.snapshot=RawHistory;
  if(mode=="capacity-injected")owned->history_api.snapshot=CapacityHistory;
  if(mode=="bounds-injected")owned->history_api.snapshot=BoundsHistory;
  const bool v2_fails=mode=="v2-snapshot-injected" || mode=="v2-stop-injected";
  bool v2_rejected=false;std::string v2;
  try{v2=cg::FinishAfterTimerDrain(token);}catch(...){v2_rejected=true;}
  Check(v2_rejected==v2_fails && owned->host_frozen && owned->host_drain_attested && owned->v2_stop_requested,
      "shared exact host cut precedes independent SDK stop/snapshot outcomes");
  if(!v2.empty()) {
    Check(v2.find("\"schema\":\"mcla-clock-read-raw-v1\"")!=std::string::npos,
        "old V2 raw schema remains byte-contract compatible");Emit("RAW_JSON",v2);
  }
  if(!on) {
    Check(!owned->history_arm_attempted && !owned->history_stop_requested && !owned->history_finish_attempted,
        "Install OFF never arms stops or snapshots history");
    Check(Rejected([&]{cg::FinishHistoryAfterTimerDrain(token);}),"OFF history output explicitly rejects rather than guessing schema");
    return;
  }
  Check(owned->history_stop_requested && owned->history_stop_succeeded==(mode!="history-stop-injected") &&
      owned->history_stop_failed==(mode=="history-stop-injected") &&
      owned->v2_stop_succeeded==(mode!="v2-stop-injected") && owned->v2_stop_failed==(mode=="v2-stop-injected"),
      "both Stop attempts retain independent requested succeeded failed states");
  if(mode=="v2-snapshot-injected")Check(injected_snapshot_saw_both_stops,
      "both SDK Stops must finish before the first V2 snapshot attempt");
  const bool history_fails=mode=="history-stop-injected" || mode=="history-pending-injected";
  std::unique_ptr<MclaClockHistorySnapshotV1> before_pending;
  if(mode=="history-pending-injected") {
    before_pending=std::make_unique<MclaClockHistorySnapshotV1>();
    std::memcpy(before_pending.get(),owned->history_snapshot,sizeof(*before_pending));
  }
  std::string history;const bool rejected=Rejected([&]{history=cg::FinishHistoryAfterTimerDrain(token);});
  Check(rejected==history_fails && owned->history_finish_attempted,"independent one-attempt history output never invents pending drain");
  if(before_pending)Check(owned->history_snapshot_attempted && owned->history_snapshot_result_available &&
      owned->history_snapshot_result==MCLA_CH_PENDING && !owned->history_snapshot_succeeded &&
      !std::memcmp(before_pending.get(),owned->history_snapshot,sizeof(*before_pending)),
      "injected PENDING retains exact status and full unchanged destination without polling");
  if(!rejected) {
    Check(history.find("\"schema\":\"mcla-clock-history-v1\"")!=std::string::npos &&
        history.find("\"physical_row_slots\":4096")!=std::string::npos &&
        history.find("\"interval_kind\":\"source-routed-lexical\"")!=std::string::npos &&
        history.size()<=mcla::clock_commit_history::raw_json::kMaxOutputBytes,
        "accepted ALL4096-slot history schema and16MiB cap used without V2 projection");
    Check(owned->history_snapshot_succeeded && owned->history_snapshot->row_count==
        (mode=="capacity-injected"?4096u:mode=="bounds-injected"?4097u:4u),
        "history raw remains available after any independent V2 failure");
    if(mode=="raw-injected") {
      Check(history.find("\"overflow_entries\":17")!=std::string::npos && history.find("\"kind\":57005")!=std::string::npos &&
          history.find("\"presence\":4294967295")!=std::string::npos && history.find("\"proof_flags\":65535")!=std::string::npos &&
          history.find("\"counts_reserved\":123")!=std::string::npos && history.find("\"reserved\":456")!=std::string::npos,
          "injected raw contradictions survive history serializer without health repair");
    }else if(mode=="capacity-injected" || mode=="bounds-injected") {
      size_t count=0,position=0;
      while((position=history.find("\"storage_ordinal\":",position))!=std::string::npos){++count;++position;}
      Check(count==4096,"ALL4096 physical wire slots emitted independent of declared row_count");
      if(mode=="capacity-injected")Check(history.find("\"read_ordinal\":4095")!=std::string::npos &&
          history.find("\"returned_guest\":18446744073709547520")!=std::string::npos && history.find("\"overflow_entries\":19")!=std::string::npos,
          "all4096 injected raw rows and overflow survive without truncation or normalization");
      else Check(history.find("\"row_count\":4097")!=std::string::npos,
          "impossible declared row_count retained safely without interpreting it as an array bound");
    }else if(!v2_fails)CheckHistory(*b,unbound,frequency);
    Emit("RAW_HISTORY",history);
  }
  Check(Rejected([&]{cg::FinishHistoryAfterTimerDrain(token);}),"history Finish is consumed even after pending or failed output");
  if(mode=="v2-stop-injected" || mode=="history-stop-injected") {
    std::cout<<"TEST_ONLY_STOP_THROW_CLEANUP NOT_CONTROLLER_RETRY_OR_ACTUAL_SDK_THROW\n";
    // The injected throw did NOT call its real Stop. No admitted Call remains;
    // explicitly close it for test cleanup, never retry controller Finish.
    if(mode=="v2-stop-injected")real_v2_stop();else real_history_stop();
  }
  if(mode.find("injected")!=std::string_view::npos)Check(injected_calls==1,"injected control invoked exactly once without blind retries");
}
void InvalidToken() {
  cg::InstallWithHistory();
  Check(Rejected([&]{cg::Begin(100,200,1000,std::string(32,'0'));}),"zero run token rejected before either Arm");
  Check(!Owned()->history_arm_attempted && !Owned()->history_armed && !Owned()->sdk_armed &&
      cg::route.load()==cg::Route::Terminal,"invalid token is terminal and cannot arm a recorder");
  Check(Rejected([&]{cg::Begin(100,200,1000,token);}),"invalid token cannot be repaired by a second Begin");
}
void DescriptorCopy() {
  cg::InstallWithHistory();MclaClockHistoryInfoV1 info{};info.abi_version=1;info.struct_size=sizeof(info);
  Check(Owned()->history_api.info(&info)==MCLA_CH_OK && cg::ValidHistoryInfo(info),"actual paired history descriptor is accepted");
  info.record_size^=1;Check(!cg::ValidHistoryInfo(info),"injected descriptor record-size mismatch is rejected by Install validator");
  info.record_size^=1;info.proof_flags=1;Check(!cg::ValidHistoryInfo(info),"injected descriptor proof flag is rejected by Install validator");
  std::cout<<"DESCRIPTOR_COPY_INJECTION ONLY_VALIDATOR_NOT_MALFORMED_DLL_INSTALL_PROOF\n";
}
void PartialArm() {
  cg::InstallWithHistory();auto* owned=Owned();
  Check(owned->api.arm(high,low)==MCLA_CW_OK,"test-only preconsumption of actual V2 Arm");
  Check(Rejected([&]{cg::Begin(100,200,1000,token);}),"partial Begin fails after history Arm when V2 Arm was consumed");
  Check(owned->history_arm_attempted && owned->history_armed && !owned->sdk_armed && !owned->begin_succeeded &&
      !owned->v2_stop_requested && !owned->history_stop_requested && cg::route.load()==cg::Route::Terminal,
      "partial Arm remains terminal without fallback rearm or premature SDK Stop");
  Check(owned->history_api.snapshot(owned->history_snapshot)==MCLA_CH_PENDING,"partial failure retains open history source storage");
  Check(Rejected([&]{cg::Begin(100,200,1000,token);}) && Rejected([&]{cg::FinishHistoryAfterTimerDrain(token);}),
      "partial Arm cannot be repaired or falsely cut by history accessor");
  std::cout<<"TEST_ONLY_PARTIAL_ARM_CLEANUP NOT_SUCCESSFUL_CONTROLLER_CUT\n";owned->api.stop();owned->history_api.stop();
}
void WrongToken() {
  Begin();auto observer=std::make_unique<Observer>(window,true);uint64_t qpc=110;Primer(*observer,qpc);
  auto b=std::make_unique<Body>();Observed(*observer,qpc,*b);if(!Drain(*observer))return;
  auto* owned=Owned();const std::string wrong="223456789abcdef0fedcba9876543210";
  Check(Rejected([&]{cg::FinishAfterTimerDrain(wrong);}) && !owned->v2_stop_requested && !owned->history_stop_requested,
      "wrong Finish token stops neither SDK family");
  Check(Rejected([&]{cg::FinishHistoryAfterTimerDrain(token);}) && !owned->history_stop_requested,
      "history accessor cannot repair missing shared cutoff after wrong token");
  std::cout<<"TEST_ONLY_WRONG_TOKEN_CLEANUP NOT_FINISH_RETRY\n";owned->api.stop();owned->history_api.stop();
}
bool Await(const std::atomic<bool>& flag) {
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!flag.load(std::memory_order_acquire)){if(std::chrono::steady_clock::now()>=deadline)return false;std::this_thread::yield();}
  return true;
}
void HostPending() {
  Begin();auto observer=std::make_unique<Observer>(window,true);uint64_t qpc=110;Primer(*observer,qpc);
  auto b=std::make_unique<Body>();auto sync=std::make_unique<Sync>();b->sync=sync.get();auto* owned=Owned();
  std::thread worker([&]{try{Observed(*observer,qpc,*b);}catch(...){sync->unexpected.store(true);}sync->done.store(true,std::memory_order_release);});
  struct Cleanup{Sync& s;std::thread& t;~Cleanup(){s.release.store(true,std::memory_order_release);if(t.joinable())t.join();}}cleanup{*sync,worker};
  if(!Await(sync->entered)){Check(false,"blocked worker ready before pending probe");return;}
  std::cout<<"INTENTIONAL_EXTERNAL_TIMER_DRAIN_PRECONDITION_VIOLATION NOT_ACCEPTANCE\n";
  Check(Rejected([&]{cg::FinishAfterTimerDrain(token);}) && !owned->host_frozen &&
      !owned->v2_stop_requested && !owned->history_stop_requested,"pending host lease stops neither SDK family");
  Check(Rejected([&]{cg::FinishHistoryAfterTimerDrain(token);}),"pending host cut cannot produce history output");
  sync->release.store(true,std::memory_order_release);worker.join();
  Check(!sync->unexpected.load() && sync->done.load() && b->reads==2,"existing bound Scope completes read1 after failed Finish");
  if(!Drain(*observer))return;
  std::cout<<"TEST_ONLY_AFTER_JOIN_STOP NOT_CONTROLLER_FINISH_RETRY\n";owned->api.stop();owned->history_api.stop();
  Check(owned->history_api.snapshot(owned->history_snapshot)==MCLA_CH_OK && owned->history_snapshot->row_count==2,
      "after-join inspection retains both actual source reads without truncation");
}
} // namespace

int main(int argc,char** argv) {
  const auto incoming=Native::Capture();std::cout<<std::unitbuf;std::cerr<<std::unitbuf;
  if(argc!=2){std::cerr<<"one fresh-process history lifecycle mode required\n";return 2;}
  try {
    try{throw Signal{};}catch(const Signal&){} // EH first touch outside originals.
    const std::string_view mode=argv[1];
    if(mode=="normal" || mode=="normal-unwind" || mode=="off" || mode=="v2-snapshot-injected" ||
        mode=="history-pending-injected" || mode=="v2-stop-injected" || mode=="history-stop-injected" || mode=="raw-injected" ||
        mode=="capacity-injected" || mode=="bounds-injected")WorkAndCut(mode);
    else if(mode=="invalid-token")InvalidToken();else if(mode=="descriptor-copy-injected")DescriptorCopy();
    else if(mode=="partial-arm")PartialArm();else if(mode=="wrong-token")WrongToken();else if(mode=="host-pending")HostPending();
    else{std::cerr<<"unknown mode\n";return 2;}
  }catch(const std::exception& error){Check(false,"unexpected history lifecycle exception");std::cerr<<error.what()<<'\n';}
  catch(...){Check(false,"unexpected nonstandard history lifecycle exception");}
  std::cout<<"checks="<<checks<<" failures="<<failures
      <<" history_controller_prerequisite_only no_game_no_complete_history_no_frame_no_FPS_no_cause\n";
  incoming.Restore();return failures?1:0;
}
