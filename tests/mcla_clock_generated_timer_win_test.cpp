// Synthetic prerequisite ONLY: primary supplies an exact, renamed private body
// mcla-clock-generated-timer.inc extracted from frozen transformed TU5. No game
// body is copied into this public fixture. No real clock/SDK QueryBound, window,
// save, input, observer installation, complete history, simulation or FPS proof.
// The actual mcla_init.h/PPCContext/prologue and private body's retry/store/math
// remain unchanged; ONLY generated memory macros use the bounded memory below.
// NativePolicy is explicitly synthetic, not hardware LastError/MXCSR evidence.
#include "mcla_init.h"
#include "../src/mcla_clock_host_invoke.h"
#include <immintrin.h>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#ifndef MCLA_CLOCK_GENERATED_TIMER_PRIVATE_INPUTS
#error Primary must pin and supply the private extracted generated timer body.
#endif

REX_EXTERN(TimerClockBody);
namespace generated_clock_test {
namespace host = mcla::clock_read_witness::host;
namespace journal = mcla::clock_read_witness::host_journal;
namespace invoke = mcla::clock_read_witness::host_invoke;
constexpr uint32_t kTimer=0x1000, kStack=0x2000;
constexpr uint32_t kScale=0x82011110, kOne=0x82001D14, kZero=0x82000ED4;
constexpr uint64_t kRunHigh=17, kRunLow=18, kTimerOrdinal=7;
unsigned checks{};
bool mutate_store{}, mutation_used{};
void Need(bool value,const char* message) {
  ++checks; if(!value) throw std::runtime_error(message);
}
uint64_t Big(const uint8_t* p,unsigned n) {
  uint64_t result=0; for(unsigned i=0;i<n;++i) result=(result<<8)|p[i]; return result;
}
void Put(uint8_t* p,unsigned n,uint64_t value) {
  for(unsigned i=0;i<n;++i) p[n-1-i]=uint8_t(value>>(8*i));
}
struct Native {
  uint64_t value;
  static inline thread_local uint64_t current=0;
  static Native Capture() noexcept { return {current}; }
  void Restore() const noexcept { current=value; }
};
struct Signal { uint32_t code; };
enum class EventKind : uint32_t { Query, Return, QueryThrow, Load, Store, MemoryThrow };
struct Event {
  EventKind kind; uint32_t address,width; uint64_t value;
  bool operator==(const Event&) const = default;
};
struct Memory {
  std::array<uint8_t,16> before{},after{};
  std::array<uint8_t,108> timer{};
  std::array<uint8_t,256> stack{},initialized{};
  Memory() {
    before.fill(0xA5); after.fill(0x5A); stack.fill(0xCD);
    Put(timer.data()+64,8,3);
    Put(timer.data()+84,4,0x3F800000); // 1.0 scale in timer.
    Put(timer.data()+32,4,0x3D088889); // Representative fixed 1/30.
    Put(timer.data()+36,4,0x3DCCCCCD); // Upper clamp ~0.1.
    Put(timer.data()+40,4,0x3A83126F); // Lower clamp ~0.001.
    Put(timer.data()+20,4,0x3F800000); Put(timer.data()+24,4,0x40000000);
    Put(timer.data()+60,4,1); timer[56]=0; timer[58]=0; timer[72]=0;
  }
};
struct Scenario {
  const char* name; std::array<uint64_t,4> returns; unsigned size;
  int throw_query_at; bool throw_memory, fallback;
};
struct Result {
  PPCContext context{};
  alignas(32) std::array<uint8_t,32> base{};
  Memory memory;
  host::Trace<16> trace{};
  std::array<Event,128> events{};
  Scenario scenario{};
  unsigned event_count{},calls{},returns{},query_calls{},fallback_calls{};
  unsigned original_calls{},read_calls{},timer_reads{},timer_writes{},stores64{};
  uint32_t exception{};
  uint64_t produced_native{};
  bool adapted{},journal_complete{},journal_unwound{};
};
Result* active{};
void EventAt(EventKind kind,uint32_t address,unsigned width,uint64_t value) {
  Need(active && active->event_count<active->events.size(),"bounded active event log");
  active->events[active->event_count++]={kind,address,width,value};
}
unsigned SuccessfulReadsRequired(const Scenario& s) {
  return s.throw_query_at<0 ? s.size : unsigned(s.throw_query_at);
}
void TimerAccess(bool write) {
  Need(active->scenario.throw_query_at<0 && active->returns==active->scenario.size &&
       uint32_t(active->scenario.returns[active->scenario.size-1])!=0,
       "timer accessed before final nonzero-low32 query return");
  if(write) ++active->timer_writes; else ++active->timer_reads;
}
uint8_t* Address(uint32_t at,unsigned width,bool write) {
  Need(active && width && width<=8,"bounded active memory access");
  if(at>=kTimer && uint64_t(at)+width<=uint64_t(kTimer)+108) {
    TimerAccess(write); return active->memory.timer.data()+(at-kTimer);
  }
  if(at>=kStack && uint64_t(at)+width<=uint64_t(kStack)+256) {
    const auto offset=at-kStack;
    for(unsigned i=0;i<width;++i) {
      if(write) active->memory.initialized[offset+i]=1;
      else Need(active->memory.initialized[offset+i],"guarded stack read before write");
    }
    return active->memory.stack.data()+offset;
  }
  throw std::runtime_error("unmapped synthetic memory or constant write");
}
uint64_t Load(uint32_t at,unsigned width) {
  Need(active,"active memory load");
  if(at==kScale || at==kOne || at==kZero) {
    Need(width==4,"read-only constant exact width");
    const uint64_t value=at==kScale ? 0x32AC3AB1 : at==kOne ? 0x3F800000 : 0;
    EventAt(EventKind::Load,at,width,value); return value;
  }
  auto* p=Address(at,width,false);
  if(active->scenario.throw_memory && at==kTimer+84) {
    Need(active->stores64==1,"memory throw occurs only after actual +64 store");
    Native::current=0xBAD23;
    EventAt(EventKind::MemoryThrow,at,width,23); throw Signal{23};
  }
  const auto value=Big(p,width); EventAt(EventKind::Load,at,width,value); return value;
}
void Store(uint32_t at,unsigned width,uint64_t value) {
  auto* p=Address(at,width,true);
  if(at==kTimer+64) {
    Need(width==8,"actual +64 store remains full uint64"); ++active->stores64;
    // Explicit fixture-only RED seam. It never changes the private body or
    // comparator, and flips exactly ONE actual store across the whole process.
    if(mutate_store && !mutation_used && active->adapted) { value^=1; mutation_used=true; }
  }
  Put(p,width,value); EventAt(EventKind::Store,at,width,value);
}
uint64_t Scheduled() {
  Need(active && active->calls<active->scenario.size,"finite predetermined query schedule");
  const auto ordinal=active->calls++;
  EventAt(EventKind::Query,0,0,ordinal); Native::current=0xCA1100+ordinal;
  if(int(ordinal)==active->scenario.throw_query_at) {
    EventAt(EventKind::QueryThrow,0,0,ordinal); throw Signal{13};
  }
  const auto value=active->scenario.returns[ordinal]; ++active->returns;
  EventAt(EventKind::Return,0,0,value); return value;
}
uint64_t MCLA_CW_CALL Query(const MclaClockWitnessBindingV1* binding,MclaClockWitnessStatusV1* status) {
  Need(active && binding && status,"fixture query pointers"); ++active->query_calls;
  Need(binding->abi_version==1 && binding->struct_size==72 &&
       binding->run_high==kRunHigh && binding->run_low==kRunLow &&
       binding->timer_invocation==kTimerOrdinal && binding->read_ordinal==active->calls,
       "actual mftb evaluation needs distinct exact caller read ordinal");
  *status={1,sizeof(*status),MCLA_CW_ACCEPTED,MCLA_CW_PUBLISHED,0,0};
  return Scheduled();
}
uint64_t Fallback() { Need(active,"fixture fallback active"); ++active->fallback_calls; return Scheduled(); }
} // namespace generated_clock_test

namespace mcla::clock_game {
uint64_t Read(PPCContext& context,uint8_t* base) {
  using namespace generated_clock_test;
  Need(active && &context==&active->context && base==active->base.data(),
       "generated body preserves actual context/base references");
  ++active->read_calls;
  return host::Read<Native>(&context,base,Fallback);
}
}

#undef REX_LOAD_U8
#undef REX_LOAD_U32
#undef REX_LOAD_U64
#undef REX_STORE_U8
#undef REX_STORE_U32
#undef REX_STORE_U64
#define REX_LOAD_U8(x) (uint8_t(generated_clock_test::Load(uint32_t(x),1)))
#define REX_LOAD_U32(x) (uint32_t(generated_clock_test::Load(uint32_t(x),4)))
#define REX_LOAD_U64(x) (generated_clock_test::Load(uint32_t(x),8))
#define REX_STORE_U8(x,y) (generated_clock_test::Store(uint32_t(x),1,uint8_t(y)))
#define REX_STORE_U32(x,y) (generated_clock_test::Store(uint32_t(x),4,uint32_t(y)))
#define REX_STORE_U64(x,y) (generated_clock_test::Store(uint32_t(x),8,uint64_t(y)))
#include "mcla-clock-generated-timer.inc"

namespace generated_clock_test {
std::unique_ptr<Result> Run(const Scenario& scenario,bool adapted) {
  auto result=std::make_unique<Result>(); result->scenario=scenario; result->adapted=adapted;
  auto journal_owner=std::make_unique<journal::Journal<2>>();
  auto snapshot=std::make_unique<journal::Journal<2>::Snapshot>();
  result->context.r1.u64=kStack+224; result->context.r3.u64=kTimer;
  result->context.r5.u64=1; result->context.f1.f64=-1.0;
  result->context.lr=0x822C21B4; result->context.fpscr.csr=0x1F80;
  const host::Identity identity{kRunHigh,kRunLow,kTimerOrdinal,9,10,11,1};
  const journal::Invocation invocation{kTimerOrdinal,9,10,11,kTimer,0x822C21B4,11,1};
  Need(journal_owner->Arm({kRunHigh,kRunLow}),"fresh synthetic journal Arm");
  const auto query=scenario.fallback ? nullptr : Query;
  Native::current=0xABC; _mm_setcsr(0x1F80); active=result.get();
  auto original=[&](PPCContext& context,uint8_t* base) {
    Need(&context==&result->context && base==result->base.data(),"once original exact references");
    ++result->original_calls; TimerClockBody(context,base);
  };
  try {
    if(adapted) invoke::Invoke<Native>(journal_owner.get(),identity,invocation,
        result->context,result->base.data(),query,true,original);
    else { host::Scope<Native> scope(identity,&result->context,result->base.data(),query,result->trace,true);
           original(result->context,result->base.data()); }
    result->produced_native=Native::current;
  } catch(const Signal& signal) {
    result->produced_native=Native::current; result->exception=signal.code;
  } catch(...) { active=nullptr; throw; }
  active=nullptr;
  journal_owner->Stop(); Need(journal_owner->TryFreezeInto(*snapshot),"scope/lease fully closed before snapshot");
  if(adapted) {
    Need(snapshot->size==1 && snapshot->rows[0].storage_ordinal==1 &&
         snapshot->rows[0].invocation.timer_ordinal==kTimerOrdinal,
         "storage admission is distinct from exact timer ordinal");
    result->trace=snapshot->rows[0].trace;
    result->journal_complete=snapshot->rows[0].disposition==journal::Disposition::Completed;
    result->journal_unwound=snapshot->rows[0].outcome==journal::Outcome::Unwound;
  }
  return result;
}
void Dump(const Result& r) {
  std::cout<<"CASE "<<r.scenario.name<<" route="<<(r.adapted?"adapter":"direct")
      <<" originals="<<r.original_calls<<" reads="<<r.read_calls<<" query="<<r.query_calls
      <<" fallback="<<r.fallback_calls<<" returns="<<r.returns<<" exception="<<r.exception
      <<" stores64="<<r.stores64<<" final64="<<Big(r.memory.timer.data()+64,8)
      <<" trace="<<r.trace.size<<" native="<<r.produced_native<<'\n';
  std::cout<<"EVENTS";
  for(unsigned i=0;i<r.event_count;++i) {
    const auto& e=r.events[i]; std::cout<<' '<<uint32_t(e.kind)<<':'<<e.address<<':'<<e.width<<':'<<e.value;
  }
  std::cout<<'\n'<<std::flush;
}
std::array<uint8_t,108> Oracle(const Scenario& s) {
  Memory seed; auto expected=seed.timer;
  if(s.throw_query_at>=0) return expected;
  const auto final=s.returns[s.size-1]; Put(expected.data()+64,8,final);
  if(s.throw_memory) return expected;
  // Independent finite STOCK-PATH oracle. It does not reproduce the retry loop
  // or call the generated body; unexecuted paths/entire game remain unclaimed.
  const auto delta=final-3;
  const float converted=float(double(std::bit_cast<int64_t>(delta)));
  const float scale=std::bit_cast<float>(uint32_t(0x32AC3AB1));
  const float elapsed=float(double(converted)*double(scale));
  const float step=std::bit_cast<float>(uint32_t(0x3D088889));
  const auto bits=[](float value) { return std::bit_cast<uint32_t>(value); };
  for(unsigned offset:{92u,96u,100u,104u}) Put(expected.data()+offset,4,bits(elapsed));
  for(unsigned offset:{8u,88u}) Put(expected.data()+offset,4,bits(step));
  Put(expected.data()+16,4,0x3F800000);
  Put(expected.data()+20,4,bits(float(1.0+double(step))));
  Put(expected.data()+24,4,bits(float(2.0+double(step))));
  Put(expected.data()+44,4,1); Put(expected.data()+48,4,1);
  Put(expected.data()+12,4,bits(float(1.0/double(step))));
  return expected;
}
void Verify(const Result& r) {
  const auto& s=r.scenario;
  const unsigned expected_calls=s.throw_query_at<0 ? s.size : unsigned(s.throw_query_at)+1;
  const uint32_t exception=s.throw_query_at>=0 ? 13 : s.throw_memory ? 23 : 0;
  Need(r.original_calls==1,"generated original executed exactly once");
  Need(r.calls==expected_calls && r.read_calls==expected_calls &&
       r.returns==SuccessfulReadsRequired(s),"all required retry queries/returns retained");
  Need(r.query_calls==(s.fallback?0:expected_calls) &&
       r.fallback_calls==(s.fallback?expected_calls:0),"one query OR one fallback per evaluation");
  Need(r.exception==exception,"same exact original exception propagates");
  const auto final=s.throw_query_at>=0 ? uint64_t(3) : s.returns[s.size-1];
  Need(Big(r.memory.timer.data()+64,8)==final,
       "final timer +64 does not equal final successful full64 query");
  Need(r.stores64==(s.throw_query_at<0?1u:0u),"exactly one actual full64 store after retries");
  if(s.throw_query_at>=0) Need(!r.timer_reads && !r.timer_writes,"query throw before any timer memory access");
  _mm_setcsr(0x1F80); Need(r.memory.timer==Oracle(s),"complete108 timer bytes match independent stock-path oracle");
  for(unsigned i=0;i<16;++i) Need(r.memory.before[i]==0xA5 && r.memory.after[i]==0x5A,"timer guard bytes unchanged");
  Need(r.trace.size==expected_calls && r.trace.observed_reads==expected_calls &&
       !r.trace.lost_rows && !r.trace.overflow,"one retained receipt per actual mftb evaluation");
  for(unsigned i=0;i<expected_calls;++i) {
    const auto& receipt=r.trace.rows[i];
    Need(receipt.scope_binding.read_ordinal==i && receipt.request.read_ordinal==i &&
         receipt.request.timer_invocation==kTimerOrdinal && receipt.trace_ordinal==i+1,
         "generated retry read ordinal reused or inferred from final store");
    const bool threw=int(i)==s.throw_query_at;
    Need(receipt.threw==uint32_t(threw) && receipt.result_available==uint32_t(!threw) &&
         receipt.used_query==uint32_t(!s.fallback),"raw retry/throw/fallback receipts preserved");
    if(!threw) Need(receipt.result==s.returns[i],"full64 query result retained even low32 zero");
  }
  if(r.adapted) Need(r.journal_complete && r.journal_unwound==(exception!=0),"adapter publishes normal/unwind after scope destruction");
}
void Compare(const Result& a,const Result& b) {
  Need(a.memory.timer==b.memory.timer && a.memory.stack==b.memory.stack &&
       a.memory.initialized==b.memory.initialized,"direct/adapter complete synthetic memory equality");
  Need(a.event_count==b.event_count,"direct/adapter event count equality");
  for(unsigned i=0;i<a.event_count;++i) Need(a.events[i]==b.events[i],"direct/adapter all memory/query events equal");
  static_assert(std::is_trivially_copyable_v<PPCContext>);
  Need(std::memcmp(&a.context,&b.context,sizeof(PPCContext))==0,"direct/adapter complete actual PPC context equality");
  Need(a.produced_native==b.produced_native,"synthetic original-produced native state equality");
}
} // namespace generated_clock_test

int main(int argc,char** argv) {
  using namespace generated_clock_test;
  std::cout<<std::unitbuf; std::cerr<<std::unitbuf;
  const auto saved=_mm_getcsr();
  try {
    Need(argc==1 || (argc==2 && std::string_view(argv[1])=="--mutate-final-store"),"unknown argument");
    mutate_store=argc==2;
    const Scenario scenarios[]={
      {"zero-low32-retries",{0,0x100000000ull,0x100000007ull,0},3,-1,false,false},
      {"one-read",{0x100000009ull,0,0,0},1,-1,false,false},
      {"nonzero-full64-zero-low32",{0x100000000ull,0x10000000Bull,0,0},2,-1,false,false},
      {"query-throw-after-zero",{0,0x10000000Dull,0,0},2,1,false,false},
      {"memory-throw-after64",{0x10000000Full,0,0,0},1,-1,true,false},
      {"fallback-one-read",{0x100000011ull,0,0,0},1,-1,false,true},
    };
    for(const auto& scenario:scenarios) {
      auto direct=Run(scenario,false); auto adapted=Run(scenario,true);
      Dump(*direct); Dump(*adapted); Verify(*direct); Verify(*adapted); Compare(*direct,*adapted);
    }
    Need(!mutate_store || mutation_used,"negative control store seam was exercised");
    _mm_setcsr(saved);
    std::cout<<"checks="<<checks<<" failures=0 synthetic_generated_timer_only runtime=false SDKclock=false history=false simulation=false FPS=false\n";
    return 0;
  } catch(const std::exception& e) {
    active=nullptr; _mm_setcsr(saved); std::cerr<<"FAIL "<<e.what()<<'\n';
  } catch(...) {
    active=nullptr; _mm_setcsr(saved); std::cerr<<"FAIL unexpected fixture exception\n";
  }
  std::cout<<"checks="<<checks<<" failures=1 synthetic_generated_timer_only runtime=false SDKclock=false history=false simulation=false FPS=false\n";
  return 1;
}
