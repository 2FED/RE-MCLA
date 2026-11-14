// Synthetic consumer of the ACTUAL paired SDK, not a game/timer integration.
// ONE-TU controller inclusion is deliberate; production separate-TU linkage,
// runtime admission, source frames, simulation, history and FPS remain unproved.
// Each argv mode MUST run in its own fresh process: no reset/rearm/DLL unload.
#include "../src/mcla_clock_game_bridge_win.cpp"
#include "../src/mcla_timer_observer_core.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

namespace {
namespace cg = mcla::clock_game;
namespace host = mcla::clock_read_witness::host;
namespace journal = mcla::clock_read_witness::host_journal;
namespace observer = mcla::timer_observer;
namespace timer = mcla::timer_journal;
using HostSnapshot = journal::Journal<1024>::Snapshot;
using Observer = observer::Observer<4>;
const std::string kToken="123456789abcdef0fedcba9876543210";
constexpr uint64_t kHigh=0x123456789ABCDEF0ull,kLow=0xFEDCBA9876543210ull;
// Synthetic shared window/phase IDs test metadata ONLY; no QPC measurement or
// genuine phase dispatcher/configuration/frequency inference is performed.
constexpr timer::Window kWindow{100,200,1000};
constexpr timer::Identity kTimer{0x827D7500,0x822C21B4,31};
struct Native {
  uint32_t error{},mxcsr{};
  static Native Capture() noexcept { const auto e=GetLastError();return {e,_mm_getcsr()}; }
  void Restore() const noexcept { _mm_setcsr(mxcsr);SetLastError(error); }
  bool operator==(const Native&) const = default;
};
struct Signal { unsigned code; };
struct Sync { std::atomic<bool> entered{},release{},done{},exception{}; };
struct Body {
  PPCContext context{};
  uint8_t bytes[4]{};
  uint8_t* memory=bytes;
  const void *actual_context{},*actual_base{};
  Native incoming{},before_read[2]{},after_read[2]{},before_fp{},after_fp{},produced{},hook_produced{},
      raw_after_observer{},external{};
  uint64_t values[2]{},ordinal{};
  volatile float operand=1.75f;
  int fp{};
  unsigned originals{},reads{},signal{},unexpected{},hook_captured{};
  bool unwind{};
  Sync* sync{};
};
constinit thread_local Body* current_body=nullptr;
unsigned checks{},failures{};
void Check(bool ok,const char* label) {
  const auto saved=Native::Capture();Native{0,0x1F80}.Restore();
  ++checks;if(!ok){++failures;std::cerr<<"FAIL "<<label<<'\n';}saved.Restore();
}
bool Await(const std::atomic<bool>& flag) {
  // Harness deadline only, outside SDK clock work and the observed original.
  const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(!flag.load(std::memory_order_acquire)) {
    if(std::chrono::steady_clock::now()>=end)return false;
    std::this_thread::yield();
  }
  return true;
}
journal::Invocation Raw(uint64_t ordinal) noexcept {return {ordinal,700,9,31,kTimer.timer_address,kTimer.caller_address,31,1};}
cg::State* Owned() noexcept {return cg::state.load(std::memory_order_acquire);}
void Begin() {cg::Install();cg::Begin(kWindow.begin_qpc,kWindow.end_qpc,kWindow.qpc_frequency,kToken);}
void PrefixSnapshot(MclaClockWitnessSnapshotV2& output) noexcept {output.abi_version=2;output.struct_size=sizeof(output);}

// One shared original body, no captured automatic observation variables. A
// volatile heap operand and externally read heap stores survive exception exit.
__declspec(noinline) void Original(PPCContext& context,uint8_t* base) {
  auto& b=*current_body;++b.originals;b.actual_context=&context;b.actual_base=base;
  b.incoming=Native::Capture();
  for(unsigned i=0;i<2;++i) {
    b.before_read[i]=Native::Capture();b.values[i]=cg::Read(context,base);++b.reads;
    b.after_read[i]=Native::Capture();
    if(i==0 && b.sync) {
      // The first SDK QueryBound/Frame has ALREADY returned before blocking.
      b.sync->entered.store(true,std::memory_order_release);
      while(!b.sync->release.load(std::memory_order_acquire))std::this_thread::yield();
    }
  }
  b.before_fp=Native::Capture();const float operand=b.operand;
  b.fp=_mm_cvtss_si32(_mm_set_ss(operand));b.after_fp=Native::Capture();
  ++context.r20.u32;++base[0];b.produced=Native::Capture();
  if(b.unwind)throw Signal{43};
}
observer::Operations<PPCContext,uint8_t*> Operations(uint64_t& fakeQpc) {
  return {[&fakeQpc]{return fakeQpc++;},
      [](const PPCContext& context,uint8_t* const&,timer::State& output) {
        output.words[0]=context.r20.u32;return true;
      },[](const PPCContext&,uint8_t* const&){return observer::ScopeBinding{700,9,31};}};
}
void Primer(Observer& timerObserver,uint64_t& fakeQpc) {
  auto b=std::make_unique<Body>();auto ops=Operations(fakeQpc);
  timerObserver.invoke(kTimer,b->context,b->memory,ops,0,0,[](PPCContext&,uint8_t*&){ });
}
void Observed(Observer& timerObserver,uint64_t& fakeQpc,Body& b,PPCFunc* original=Original) {
  auto ops=Operations(fakeQpc);current_body=&b;
  const Native incoming{0x13572468u,0x3F80u};incoming.Restore();
  try {
    timerObserver.invoke_with_ordinal(kTimer,b.context,b.memory,ops,0xA5,0x0123456789ABCDEFull,
        [&](PPCContext& context,uint8_t*& base,uint64_t ordinal) {
          b.ordinal=ordinal;incoming.Restore();
          try {cg::Invoke(original,context,base,Raw(ordinal));}
          catch(...){b.hook_produced=Native::Capture();b.hook_captured=1;throw;}
          b.hook_produced=Native::Capture();b.hook_captured=1;
        });
    b.raw_after_observer=Native::Capture();
  }catch(const Signal& signal){b.raw_after_observer=Native::Capture();b.signal=signal.code;}
  catch(...){b.raw_after_observer=Native::Capture();b.unexpected=1;}
  // This outer hook owns preservation ACROSS Observer Completion callbacks.
  // Controller only promises its own entry/exit state, not other instrumentation.
  if(b.hook_captured)b.hook_produced.Restore();
  b.external=Native::Capture();current_body=nullptr;
}
bool Drain(Observer& timerObserver) {
  auto snapshot=std::make_unique<Observer::Snapshot>();
  timerObserver.stop_admissions();const bool drained=timerObserver.try_freeze_into(*snapshot);
  if(drained) {
    const auto saved=Native::Capture();Native{0,0x1F80}.Restore();
    const auto counts=snapshot->counts();
    std::cout<<"TIMER_DRAIN admitted="<<counts.admitted_entries<<" completed="<<counts.completed_entries
        <<" retained="<<snapshot->records().size()<<'\n';
    for(const auto& row:snapshot->records())
      std::cout<<"TIMER_ROW ordinal="<<row.entry_ordinal<<" callback_issues="<<row.callback_issues<<'\n';
    saved.Restore();
  }
  return drained;
}
void CheckBody(const Body& b) {
  Check(b.originals==1 && b.reads==2 && b.actual_context==&b.context && b.actual_base==b.bytes &&
      b.context.r20.u32==1 && b.bytes[0]==1,"exact original context/base and mutations occur once");
  Check(b.incoming==Native{0x13572468u,0x3F80u} && b.before_read[0]==b.incoming,
      "controller preserves original incoming native state");
  Check(b.fp==1 && (b.before_fp.mxcsr&0x6000u)==0x2000u &&
      (b.after_fp.mxcsr&0x6000u)==0x2000u,"actual SDK reads leave round-down state for runtime volatile FP");
  // For unwind, before_throw is NOT assumed equal to a catch after the C++ EH
  // runtime. Direct-versus-adapted throw equivalence has its separate fixture.
  Check(b.hook_captured && b.external==b.hook_produced && (b.unwind || b.hook_produced==b.produced),
      "captured controller return or unwind native state survives outer Completion restoration");
  Check(!b.unexpected && b.signal==(b.unwind?43u:0u),"exact original exception outcome preserved");
}
void PrintBody(const Body& b) {
  const auto saved=Native::Capture();Native{0,0x1F80}.Restore();
  std::cout<<"BODY timer="<<b.ordinal<<" originals="<<b.originals<<" reads="<<b.reads
      <<" value0="<<b.values[0]<<" value1="<<b.values[1]<<" fp="<<b.fp<<" signal="<<b.signal
      <<" incomingError="<<b.incoming.error<<" beforeFPError="<<b.before_fp.error
      <<" producedError="<<b.produced.error<<" catchError="<<b.raw_after_observer.error
      <<" incomingMXCSR="<<b.incoming.mxcsr<<" beforeFPMXCSR="<<b.before_fp.mxcsr
      <<" afterFPMXCSR="<<b.after_fp.mxcsr<<" producedMXCSR="<<b.produced.mxcsr
      <<" restoredMXCSR="<<b.external.mxcsr<<'\n';saved.Restore();
}
bool Arithmetic(const MclaClockWitnessRecordV1& r) {
  if(!r.ratio_numerator || !r.ratio_denominator || !(r.presence&1) || !(r.presence&2) || !(r.presence&4))return false;
  if(r.branch==1) {
    const uint64_t delta=r.host_sample>r.pre_last_host?r.host_sample-r.pre_last_host:0;
    return (r.presence&8) && r.post_last_host==r.host_sample &&
        r.post_last_guest==r.pre_last_guest+delta*r.ratio_numerator/r.ratio_denominator && r.returned_guest==r.post_last_guest;
  }
  if(r.branch==2)return (r.presence&8) && r.pre_last_host==r.post_last_host &&
      r.pre_last_guest==r.post_last_guest && r.returned_guest==r.post_last_guest;
  if(r.branch==3)return !(r.presence&8) && !r.pre_last_host && !r.pre_last_guest && !r.post_last_host && !r.post_last_guest &&
      r.returned_guest==r.host_sample*r.ratio_numerator/r.ratio_denominator;
  return false;
}
void Joined(const HostSnapshot& h,const MclaClockWitnessSnapshotV2& s,unsigned scopes,unsigned reads) {
  if(h.size>1024 || s.row_count>4096){Check(false,"raw snapshot bounds required before fixture identity join");return;}
  Check(h.size==scopes && h.counts.observed_scopes==scopes && h.counts.completed_scopes==scopes &&
      !h.counts.lost_scopes && !h.counts.aborted_scopes,"host retains exact completed scope inventory without overflow");
  Check(s.row_count==reads && s.observed_bound_reads==reads && s.stored_entries==reads &&
      s.published_entries==reads && !s.aborted_entries && !s.overflow_entries && !s.invalid_bindings &&
      !s.diagnostic_entries && !s.saturated_rejections && !s.missing_witnesses && !s.duplicate_witnesses &&
      !s.counter_saturated && !s.proof_flags,"SDK exact bound inventory retained without invented unbound rows");
  Check(h.run.high==kHigh && h.run.low==kLow && s.run_high==kHigh && s.run_low==kLow,
      "both complete128bit run tokens retained without inferred identity");
  auto seen=std::make_unique<bool[]>(s.row_count);unsigned hostReads=0;
  for(uint64_t i=0;i<h.size && i<1024;++i) {
    const auto& scope=h.rows[i];
    Check(scope.trace.size<=16,"raw trace physical bound retained before join");
    Check(scope.storage_ordinal==i+1 && scope.invocation.timer_ordinal!=scope.storage_ordinal,
        "host storage ordinal remains separate from actual primer-adjusted timer ordinal");
    for(uint32_t j=0;j<scope.trace.size && j<16;++j) {
      const auto& receipt=scope.trace.rows[j];++hostReads;unsigned matches=0,found=0;
      for(uint32_t k=0;k<s.row_count && k<4096;++k)
        if(std::memcmp(&receipt.request,&s.rows[k].binding,sizeof(receipt.request))==0){++matches;found=k;}
      Check(matches==1,"host request joins exactly one SDK raw binding without nearest-time mapping");
      if(matches==1 && s.row_count<=4096) {
        Check(!seen[found] && s.rows[found].returned_guest==receipt.result &&
            s.rows[found].issues==receipt.status.issues && s.rows[found].disposition==MCLA_CW_PUBLISHED,
            "exact identity joins unique actual return and status tuple");seen[found]=true;
      }
      Check(receipt.request.timer_invocation==scope.invocation.timer_ordinal && receipt.request.read_ordinal==j &&
          receipt.result_available && receipt.status_available && receipt.used_query && !receipt.threw &&
          !receipt.decisions && receipt.status.admission==MCLA_CW_ACCEPTED && receipt.status.completion==MCLA_CW_PUBLISHED,
          "every actual read retains its own ordinal and healthy explicit query status");
    }
  }
  Check(hostReads==reads,"all retained host reads join without filtering");
  for(uint32_t i=0;i<s.row_count && i<4096;++i) {
    Check(seen[i] && s.rows[i].admission_ordinal==i+1 && Arithmetic(s.rows[i]),
        "all SDK rows retain exact admission and source unsigned branch arithmetic");
    Check(s.rows[i].ratio_numerator==1 && s.rows[i].ratio_denominator==1 &&
        (s.rows[i].branch==1 || s.rows[i].branch==2),"actual SDK fixed default configuration is not silently changed");
  }
}
void Emit(const std::string& text) {
  const auto saved=Native::Capture();Native{0,0x1F80}.Restore();std::cout<<"RAW_JSON "<<text<<'\n';saved.Restore();
}

void NormalUnwind() {
  Begin();auto timerObserver=std::make_unique<Observer>(kWindow,true);uint64_t qpc=110;
  Primer(*timerObserver,qpc);auto normal=std::make_unique<Body>();auto unwind=std::make_unique<Body>();unwind->unwind=true;
  Observed(*timerObserver,qpc,*normal);Observed(*timerObserver,qpc,*unwind);
  CheckBody(*normal);CheckBody(*unwind);PrintBody(*normal);PrintBody(*unwind);
  Check(normal->ordinal==2 && unwind->ordinal==3,"existing observer admission ordinals forwarded after primer");
  const bool drained=Drain(*timerObserver);Check(drained,"actual Observer Completions drain before happy Finish");
  if(!drained)return;
  const auto text=cg::FinishAfterTimerDrain(kToken);auto* owned=Owned();
  Joined(*owned->host_snapshot,*owned->sdk_snapshot,2,4);
  Check(owned->host_snapshot->counts.normal_scopes==1 && owned->host_snapshot->counts.unwound_scopes==1 &&
      owned->host_snapshot->counts.inconclusive,"original unwind stays an explicit host diagnostic rather than healthy-summary proof");
  for(unsigned i=0;i<2;++i)for(unsigned j=0;j<2;++j)
    Check(owned->host_snapshot->rows[i].trace.rows[j].result==(i?unwind->values[j]:normal->values[j]),
        "caller actual Read results match their retained host receipts");
  Emit(text);
  bool rejected=false;try{cg::FinishAfterTimerDrain(kToken);}catch(const std::exception&){rejected=true;}
  Check(rejected,"successful Finish cannot be repeated or rearmed");
  Check(mcla::clock_read_witness::raw_json::Serialize(*owned->host_snapshot,*owned->sdk_snapshot,kToken)==text,
      "rejected repeat Finish leaves frozen raw snapshots unchanged");
}

struct Nested {
  PPCContext context{};uint8_t bytes[4]{};uint8_t* memory=bytes;
  uint64_t outer[2]{},child{};unsigned outer_calls{},child_calls{},exact{},child_inert{};
};
constinit thread_local Nested* current_nested=nullptr;
__declspec(noinline) void Child(PPCContext& context,uint8_t* base) {
  auto& n=*current_nested;++n.child_calls;n.exact+=&context==&n.context && base==n.bytes;
  const auto* active=host::detail::current;
  n.child_inert=active && !active->enabled && active->identity.timer_invocation==999;
  n.child=cg::Read(context,base);
}
__declspec(noinline) void Outer(PPCContext& context,uint8_t* base) {
  auto& n=*current_nested;++n.outer_calls;n.exact+=&context==&n.context && base==n.bytes;
  n.outer[0]=cg::Read(context,base);cg::Disarm();cg::Invoke(Child,context,base,Raw(999));
  n.outer[1]=cg::Read(context,base);
}
void NestedDisarm() {
  Begin();auto timerObserver=std::make_unique<Observer>(kWindow,true);uint64_t qpc=110;Primer(*timerObserver,qpc);
  auto n=std::make_unique<Nested>();current_nested=n.get();
  auto ops=Operations(qpc);uint64_t ordinal=0;
  timerObserver->invoke_with_ordinal(kTimer,n->context,n->memory,ops,0,0,
      [&](PPCContext& c,uint8_t*& b,uint64_t id){ordinal=id;cg::Invoke(Outer,c,b,Raw(id));});
  current_nested=nullptr;
  Check(n->outer_calls==1 && n->child_calls==1 && n->exact==2 && n->child_inert,
      "post-Disarm nested original executes once in inert shadow rather than inheriting parent");
  Check(ordinal==2 && host::detail::current==nullptr,"nested test forwards actual observer ordinal and restores TLS");
  const bool drained=Drain(*timerObserver);Check(drained,"nested outer Completion drained before Finish");if(!drained)return;
  const auto text=cg::FinishAfterTimerDrain(kToken);auto* owned=Owned();Joined(*owned->host_snapshot,*owned->sdk_snapshot,1,2);
  Check(owned->host_snapshot->rows[0].trace.rows[0].result==n->outer[0] &&
      owned->host_snapshot->rows[0].trace.rows[1].result==n->outer[1] && owned->sdk_snapshot->row_count==2,
      "only two outer reads bind SDK while child original still returns a real clock value");Emit(text);
}

void Blocked(bool failedFinish) {
  Begin();auto timerObserver=std::make_unique<Observer>(kWindow,true);uint64_t qpc=110;Primer(*timerObserver,qpc);
  auto b=std::make_unique<Body>();auto sync=std::make_unique<Sync>();b->sync=sync.get();
  auto sdk=std::make_unique<MclaClockWitnessSnapshotV2>();auto hostSnapshot=std::make_unique<HostSnapshot>();
  auto timerSnapshot=std::make_unique<Observer::Snapshot>();
  auto* owned=Owned();
  std::thread worker([&]{
    try{Observed(*timerObserver,qpc,*b);}catch(...){sync->exception.store(true,std::memory_order_release);}
    sync->done.store(true,std::memory_order_release);
  });
  struct JoinOnExit {
    Sync& sync;std::thread& worker;
    ~JoinOnExit(){sync.release.store(true,std::memory_order_release);if(worker.joinable())worker.join();}
  } cleanup{*sync,worker};
  const bool entered=Await(sync->entered);Check(entered,"worker completes first SDK read before deliberate block");
  if(!entered){sync->release.store(true,std::memory_order_release);worker.join();return;}
  Check(!timerObserver->try_freeze_into(*timerSnapshot),"actual timer Completion remains pending while original is blocked");
  PrefixSnapshot(*sdk);Check(owned->api.snapshot(sdk.get())==MCLA_CW_PENDING,
      "SDK Snapshot remains pending while recorder open and first Query has already completed");
  if(failedFinish) {
    // Deliberate INVALID caller precondition, NOT a successful drain claim.
    std::cout<<"PENDING_FINISH_INTENTIONAL_PRECONDITION_VIOLATION NOT_ACCEPTANCE\n";
    bool rejected=false;try{cg::FinishAfterTimerDrain(kToken);}catch(const std::exception&){rejected=true;}
    Check(rejected && owned->finish_attempted,"premature Finish rejects active host lease and consumes attempt");
    PrefixSnapshot(*sdk);Check(owned->api.snapshot(sdk.get())==MCLA_CW_PENDING,
        "failed host freeze did not Stop otherwise-quiescent SDK recorder");
  }else{
    // White-box lifecycle mutex held on the main thread. Disarm MUST return on
    // another thread without waiting for either it or the blocked original.
    std::unique_lock held(cg::lifecycle);std::atomic<bool> returned=false;
    std::thread disarmer([&]{cg::Disarm();returned.store(true,std::memory_order_release);});
    const bool nonblocking=Await(returned);held.unlock();disarmer.join();
    Check(nonblocking && !sync->done.load(std::memory_order_acquire),
        "Disarm returns while lifecycle mutex held and original still blocked");
    PrefixSnapshot(*sdk);Check(owned->api.snapshot(sdk.get())==MCLA_CW_PENDING,"Disarm alone never Stops SDK");
  }
  sync->release.store(true,std::memory_order_release);worker.join();
  Check(!sync->exception.load(std::memory_order_acquire) && sync->done.load(std::memory_order_acquire),
      "worker joins after existing Scope performs its second read");CheckBody(*b);PrintBody(*b);
  const bool drained=Drain(*timerObserver);Check(drained,"real pending timer Completion drains after join");if(!drained)return;
  if(failedFinish) {
    // No retry of controller Finish. Test-only inspection of retained state
    // after joined originals: explicit SDK Stop/snapshot is NOT production API.
    Check(owned->receipts.TryFreezeInto(*hostSnapshot),"failed Finish retained host storage for white-box after-join inspection");
    std::cout<<"TEST_ONLY_AFTER_JOIN_INSPECTION NOT_CONTROLLER_FINISH_RETRY\n";
    owned->api.stop();PrefixSnapshot(*sdk);Check(owned->api.snapshot(sdk.get())==MCLA_CW_OK,"test-only after-join SDK drain succeeds");
    Joined(*hostSnapshot,*sdk,1,2);
    Check(hostSnapshot->rows[0].trace.rows[0].result==b->values[0] &&
        hostSnapshot->rows[0].trace.rows[1].result==b->values[1],
        "both actual returns retained and existing Scope read1 stays bound after rejected Finish");
    Emit(mcla::clock_read_witness::raw_json::Serialize(*hostSnapshot,*sdk,kToken));
  }else{
    const auto text=cg::FinishAfterTimerDrain(kToken);Joined(*owned->host_snapshot,*owned->sdk_snapshot,1,2);
    Check(owned->host_snapshot->rows[0].trace.rows[0].result==b->values[0] &&
        owned->host_snapshot->rows[0].trace.rows[1].result==b->values[1],
        "Disarm preserves both actual original read results inside existing Scope");Emit(text);
  }
}

void CheapConsumed(bool invalidBegin) {
  cg::Install();bool rejected=false;
  if(invalidBegin) {
    try{cg::Begin(100,100,1000,kToken);}catch(const std::exception&){rejected=true;}
    Check(rejected && Owned()->begin_attempted && !Owned()->begin_succeeded && !Owned()->sdk_armed,
        "invalid window consumes Begin without arming SDK");
    rejected=false;try{cg::Begin(100,200,1000,kToken);}catch(const std::exception&){rejected=true;}
    Check(rejected,"invalid Begin cannot be retried with repaired parameters");
  }else{
    try{cg::Install();}catch(const std::exception&){rejected=true;}
    Check(rejected,"Install is once-only and repeated attempt terminally rejects");
    rejected=false;try{cg::Begin(100,200,1000,kToken);}catch(const std::exception&){rejected=true;}
    Check(rejected && !Owned()->sdk_armed,"consumed installation cannot become Ready by Begin");
  }
  Check(cg::route.load(std::memory_order_acquire)==cg::Route::Terminal,"failed lifecycle gate remains terminal");
  // Original still runs through inert Scope; no host entry or bound SDK row.
  auto b=std::make_unique<Body>();current_body=b.get();Native{0x13572468u,0x3F80u}.Restore();
  cg::Invoke(Original,b->context,b->bytes,Raw(2));b->hook_produced=Native::Capture();b->hook_captured=1;b->external=b->hook_produced;
  current_body=nullptr;CheckBody(*b);
  auto h=std::make_unique<HostSnapshot>();auto s=std::make_unique<MclaClockWitnessSnapshotV2>();
  Check(Owned()->receipts.TryFreezeInto(*h) && h->size==0,"terminal unarmed controller has no fabricated host scope");
  Owned()->api.stop();PrefixSnapshot(*s);
  Check(Owned()->api.snapshot(s.get())==MCLA_CW_OK && s->row_count==0 && !s->observed_bound_reads,
      "test-only SDK snapshot retains zero bound rows after rejected gates");
  Emit(mcla::clock_read_witness::raw_json::Serialize(*h,*s,kToken));
}
}  // namespace

int main(int argc,char** argv) {
  const auto initial=Native::Capture();std::cout<<std::unitbuf;std::cerr<<std::unitbuf;
  if(argc!=2){std::cerr<<"one fresh-process mode required\n";return 2;}
  try {
    // Initialize this consumer's typed-EH first touch outside observed originals.
    try{throw Signal{0};}catch(const Signal&){}
    const std::string_view mode=argv[1];
    if(mode=="normal-unwind")NormalUnwind();
    else if(mode=="nested-disarm")NestedDisarm();
    else if(mode=="pending-finish")Blocked(true);
    else if(mode=="disarm-blocked")Blocked(false);
    else if(mode=="invalid-begin")CheapConsumed(true);
    else if(mode=="consumed-install")CheapConsumed(false);
    else{std::cerr<<"unknown mode\n";return 2;}
  }catch(const std::exception& error){Check(false,"unexpected lifecycle consumer exception");std::cerr<<error.what()<<'\n';}
  catch(...){Check(false,"unexpected nonstandard lifecycle consumer exception");}
  std::cout<<"checks="<<checks<<" failures="<<failures
      <<" controller_lifecycle_prerequisite_only no_game_no_full_clock_history_no_frame_no_simulation_no_FPS\n";
  initial.Restore();return failures?1:0;
}
