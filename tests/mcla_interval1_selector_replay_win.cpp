// Synthetic exact-body selector seam only: no game, GPU, files or real clocks.
// Primary supplies pinned interval1-selector-bodies.inc defining REX_EXTERN
// OriginalSelector/CandidateSelector from the exact same containing function,
// with symbol renaming and the reviewed single literal change only. Keep all
// original guest-PC stores and prologues. No proprietary body belongs here.
// All other callees below are deterministic MOCKS, not their runtime behavior.
// Compile with matching frozen mcla_init/SDK headers, C++23 and original O3 ISA
// flags. No observer macros needed. Run with no arguments or --mutate-payload.
// Exit0=clean bounded seam PASS;1=mismatch/detected mutation;2=invalid/runtime;
// exit3=requested mutation went undetected. Clean full matrix always runs first.
#include "mcla_init.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace selector_replay {
constexpr uint32_t kSize=65536,kObject=0x1000,kStack=0xF000;
constexpr uint32_t kCommand=0x6000,kRefilled=0x6400,kNewLimit=0x7400;
constexpr uint64_t kCaller=0x123456789ABCDEF0ull;
constexpr uint64_t kPacket=static_cast<uint64_t>(int64_t(-2109603840)-26600);
struct Case { uint32_t selector,low,fallback,mode; uint64_t overlay; };
enum class Kind { Save,Lock,Prepare,Refill,Submit,Restore };
struct Call {
  Kind kind{}; uint64_t lr{},sp{}; uint32_t pc{},function{};
  std::array<uint64_t,5> args{};
  bool operator==(const Call&) const = default;
};
struct Memory {
  std::array<uint8_t,kSize> bytes{},initialized{};
  std::vector<Call> calls;
  std::array<uint64_t,4> saved{};
  uint64_t saved_lr{};
  bool saved_once{},restored{},mutate{};
  uint32_t mode{},submitted_at{};
  Memory(){ bytes.fill(0xA5); calls.reserve(10); }
  static void Need(bool ok,const char* why){ if(!ok)throw std::runtime_error(why); }
  static bool ObjectRead(uint32_t a,unsigned n){
    if(n==1)return a==kObject+10941;
    if(n!=4)return false;
    for(auto offset:{48u,56u,10900u,11852u,13596u})if(a==kObject+offset)return true;
    return a==kStack-128+80;
  }
  static bool Writable(uint32_t a,unsigned n){
    return (n==1&&a==kObject+10941)||(n==4&&
      (a==kObject+48||a==kObject+56||a==kStack-128||
       (a>=kCommand&&a<=0x8000-4&&(a&3)==0)));
  }
  void Put(uint32_t a,unsigned n,uint64_t value){
    Need(n&&n<=8&&a<=kSize-n,"synthetic initialization bounds");
    for(unsigned i=0;i<n;++i){bytes[a+n-1-i]=uint8_t(value>>(8*i));initialized[a+n-1-i]=1;}
  }
  uint64_t Read(uint32_t a,unsigned n)const{
    Need(a<=kSize-n&&ObjectRead(a,n),"unexpected guest read");
    uint64_t value=0;
    for(unsigned i=0;i<n;++i){Need(initialized[a+i]!=0,"uninitialized guest read");value=(value<<8)|bytes[a+i];}
    return value;
  }
  void Write(uint32_t a,unsigned n,uint64_t value){
    Need(a<=kSize-n&&Writable(a,n),"unexpected guest write");Put(a,n,value);
  }
  void Trace(Kind kind,const PPCContext& c){
    Need(calls.size()<10,"bounded call trace");
    calls.push_back({kind,c.lr,c.r1.u64,c.guest_pc,c.current_function,
                    {c.r3.u64,c.r4.u64,c.r5.u64,c.r6.u64,c.r7.u64}});
  }
};
Memory* active{};
Memory& Current(){Memory::Need(active!=nullptr,"no active synthetic memory");return *active;}
uint64_t Load(uint32_t a,unsigned n){return Current().Read(a,n);}
void Store(uint32_t a,unsigned n,uint64_t value){Current().Write(a,n,value);}
}

#undef REX_LOAD_U8
#undef REX_LOAD_U32
#undef REX_STORE_U8
#undef REX_STORE_U32
#define REX_LOAD_U8(a) static_cast<uint8_t>(selector_replay::Load(static_cast<uint32_t>(a),1))
#define REX_LOAD_U32(a) static_cast<uint32_t>(selector_replay::Load(static_cast<uint32_t>(a),4))
#define REX_STORE_U8(a,v) selector_replay::Store(static_cast<uint32_t>(a),1,static_cast<uint8_t>(v))
#define REX_STORE_U32(a,v) selector_replay::Store(static_cast<uint32_t>(a),4,static_cast<uint32_t>(v))

REX_EXTERN(__savegprlr_28){
  (void)base;auto& m=selector_replay::Current();m.Trace(selector_replay::Kind::Save,ctx);
  m.Need(!m.saved_once,"single mocked save");m.saved_once=true;
  m.saved={ctx.r28.u64,ctx.r29.u64,ctx.r30.u64,ctx.r31.u64};m.saved_lr=ctx.r12.u64;
}
REX_EXTERN(__restgprlr_28){
  (void)base;auto& m=selector_replay::Current();m.Trace(selector_replay::Kind::Restore,ctx);
  m.Need(m.saved_once&&!m.restored&&ctx.r1.u64==selector_replay::kStack,"balanced mocked restore");
  ctx.r28.u64=m.saved[0];ctx.r29.u64=m.saved[1];ctx.r30.u64=m.saved[2];ctx.r31.u64=m.saved[3];
  ctx.lr=m.saved_lr;m.restored=true;
}
REX_EXTERN(sub_82429570){
  (void)base;selector_replay::Current().Trace(selector_replay::Kind::Lock,ctx);
}
REX_EXTERN(sub_8241BD08){
  (void)base;selector_replay::Current().Trace(selector_replay::Kind::Prepare,ctx);
}
REX_EXTERN(sub_82412710){
  (void)base;auto& m=selector_replay::Current();m.Trace(selector_replay::Kind::Refill,ctx);
  m.Write(selector_replay::kObject+56,4,selector_replay::kNewLimit);
  ctx.r3.u64=selector_replay::kRefilled;
}
REX_EXTERN(sub_82411F68){
  (void)base;auto& m=selector_replay::Current();
  if(m.mutate)ctx.r7.u64^=0x100; // Actual output mutation; expected oracle remains untouched.
  m.Trace(selector_replay::Kind::Submit,ctx);
  m.submitted_at=ctx.r4.u32+4;m.Write(m.submitted_at,4,ctx.r7.u32);
  ctx.r3.u64=m.mode==3?0x7800:m.submitted_at;
}
REX_EXTERN(OriginalSelector);
REX_EXTERN(CandidateSelector);
#include "interval1-selector-bodies.inc"

namespace selector_replay {
struct Result {
  PPCContext context{};
  std::array<uint8_t,kSize> memory{};
  std::vector<Call> calls;
  uint32_t submitted_at{};
};
// PPCContext has alignment/XER/tail padding. Its implicitly generated copies
// need not preserve those bytes. Compare every named state field, including
// reserved/breadcrumb fields and all raw FP/vector bits, but never padding.
static_assert(std::is_standard_layout_v<PPCContext> && std::is_trivially_copyable_v<PPCContext>);
static_assert(sizeof(PPCRegister)==8 && sizeof(PPCXERRegister)==3 && sizeof(PPCCRRegister)==4);
static_assert(offsetof(PPCContext,r31)-offsetof(PPCContext,r3)==31*sizeof(PPCRegister));
static_assert(offsetof(PPCContext,cr7)-offsetof(PPCContext,cr0)==7*sizeof(PPCCRRegister));
static_assert(offsetof(PPCContext,f31)-offsetof(PPCContext,f0)==31*sizeof(PPCRegister));
static_assert(offsetof(PPCContext,v127)-offsetof(PPCContext,v0)==127*sizeof(PPCVRegister));
constexpr std::array<std::pair<size_t,size_t>,15> kContextRanges{{
  {offsetof(PPCContext,r3),32*sizeof(PPCRegister)},
  {offsetof(PPCContext,lr),sizeof(uint64_t)},
  {offsetof(PPCContext,ctr),sizeof(PPCRegister)},
  {offsetof(PPCContext,xer),sizeof(PPCXERRegister)},
  {offsetof(PPCContext,reserved),sizeof(PPCRegister)},
  {offsetof(PPCContext,msr),sizeof(uint32_t)},
  {offsetof(PPCContext,cr0),8*sizeof(PPCCRRegister)},
  {offsetof(PPCContext,fpscr),sizeof(PPCFPSCRRegister)},
  {offsetof(PPCContext,vscr_sat),sizeof(uint8_t)},
  {offsetof(PPCContext,last_indirect_target),sizeof(uint32_t)},
  {offsetof(PPCContext,guest_pc),sizeof(uint32_t)},
  {offsetof(PPCContext,current_function),sizeof(uint32_t)},
  {offsetof(PPCContext,last_import),sizeof(const char*)},
  {offsetof(PPCContext,f0),32*sizeof(PPCRegister)},
  {offsetof(PPCContext,v0),128*sizeof(PPCVRegister)}
}};
constexpr size_t ContextStateBytes(){size_t n=0;for(const auto& range:kContextRanges)n+=range.second;return n;}
bool ContextEqual(const PPCContext& a,const PPCContext& b){
  const auto* x=reinterpret_cast<const uint8_t*>(&a);const auto* y=reinterpret_cast<const uint8_t*>(&b);
  for(const auto& [offset,size]:kContextRanges)if(std::memcmp(x+offset,y+offset,size)!=0)return false;
  return true;
}
bool ContextComparisonTests(){
  PPCContext original{};std::array<uint8_t,sizeof(PPCContext)> covered{};
  for(const auto& [offset,size]:kContextRanges)for(size_t i=offset;i<offset+size;++i){
    if(i>=covered.size()||covered[i])return false;covered[i]=1;
    auto changed=original;reinterpret_cast<uint8_t*>(&changed)[i]^=1;
    if(ContextEqual(original,changed))return false;
  }
  for(size_t i=0;i<covered.size();++i)if(!covered[i]){
    auto changed=original;reinterpret_cast<uint8_t*>(&changed)[i]^=1;
    if(!ContextEqual(original,changed))return false;
  }
  return true;
}
PPCContext Initial(const Case& c){
  PPCContext ctx{};ctx.r1.u64=kStack;ctx.r3.u64=kObject;ctx.r4.u64=c.overlay;
  ctx.r5.u64=0x5555555555555555;ctx.r6.u64=0x6666666666666666;ctx.r7.u64=0x7777777777777777;
  ctx.r28.u64=0x2828282828282828;ctx.r29.u64=0x2929292929292929;
  ctx.r30.u64=0x3030303030303030;ctx.r31.u64=0x3131313131313131;
  ctx.lr=kCaller;ctx.ctr.u64=0x4444444444444444;
  ctx.guest_pc=0x1234;ctx.current_function=0x5678;return ctx;
}
Result Run(PPCFunc* body,const Case& c,bool mutation=false){
  auto m=std::make_unique<Memory>();m->mode=c.mode;m->mutate=mutation;
  m->Put(kObject+48,4,c.mode==1?0x7100:kCommand);
  m->Put(kObject+56,4,c.mode==2?0x6010:0x7000);
  m->Put(kObject+10900,4,0x12345000);
  m->Put(kObject+10941,1,0xA3);
  // Noise outside the seven selected bits proves it is not simply a low mask.
  m->Put(kObject+11852,4,(c.low<<23)|0x80000001u);
  m->Put(kObject+13596,4,c.selector);
  m->Put(kStack-128+80,4,c.fallback);
  auto ctx=Initial(c);alignas(32) uint8_t unused_base[32]{};
  Memory::Need(active==nullptr,"non-nested replay");active=m.get();
  try{body(ctx,unused_base);}catch(...){active=nullptr;throw;}active=nullptr;
  Memory::Need(m->restored,"original body did not reach mocked restore");
  Result r;r.context=ctx;r.memory=m->bytes;r.calls=std::move(m->calls);r.submitted_at=m->submitted_at;return r;
}
uint64_t Payload(const Case& c,bool candidate){
  uint32_t selected=c.fallback;
  if(c.selector==0||c.selector==1)selected=1;
  else if(c.selector==2)selected=candidate?1:2;
  else if(c.selector==4)selected=3;
  else if(c.selector==0x80000000u)selected=0;
  return uint64_t((selected<<8)&0xffffff00u)|c.low|c.overlay;
}
uint32_t Word(const Result& r,uint32_t a){
  return (uint32_t(r.memory[a])<<24)|(uint32_t(r.memory[a+1])<<16)|
         (uint32_t(r.memory[a+2])<<8)|r.memory[a+3];
}
struct Oracle {
  std::vector<Kind> order;
  std::vector<uint64_t> lrs;
  uint32_t cursor{},limit{},submit_cursor{},final_cursor{},refills{};
};
Oracle Expected(const Case& c){
  Oracle o;o.cursor=c.mode==1?0x7100:kCommand;o.limit=c.mode==2?0x6010:0x7000;
  auto add=[&](Kind k,uint64_t lr){o.order.push_back(k);o.lrs.push_back(lr);};
  auto refill=[&](uint64_t lr){add(Kind::Refill,lr);o.cursor=kRefilled;o.limit=kNewLimit;++o.refills;};
  add(Kind::Save,0x824199B8);add(Kind::Lock,0x824199CC);add(Kind::Prepare,0x824199D8);
  if(uint32_t(c.overlay)==0){if(o.cursor>o.limit)refill(0x824199FC);o.cursor+=32;}
  if(o.cursor>o.limit)refill(0x82419AE0);
  o.submit_cursor=o.cursor;add(Kind::Submit,0x82419AFC);o.cursor=c.mode==3?0x7800:o.cursor+4;
  if(uint32_t(c.overlay)==0){if(o.cursor>o.limit)refill(0x82419B1C);o.cursor+=24;}
  o.final_cursor=o.cursor;add(Kind::Lock,0x82419B78);add(Kind::Restore,0x82419B78);return o;
}
bool PayloadFlow(const Result& r,const Case& c,bool candidate){
  const auto payload=Payload(c,candidate);
  if(r.context.r7.u64!=payload)return false;
  bool submitted=false;
  for(const auto& call:r.calls){
    if(call.kind==Kind::Submit){if(submitted)return false;submitted=true;}
    if(submitted&&call.args[4]!=payload)return false;
  }
  return submitted;
}
bool Valid(const Result& r,const Case& c,bool candidate){
  const auto o=Expected(c);const auto payload=Payload(c,candidate);const auto initial=Initial(c);
  if(!PayloadFlow(r,c,candidate)||r.calls.size()!=o.order.size()||r.submitted_at!=o.submit_cursor+4||
     Word(r,kObject+48)!=o.final_cursor||Word(r,kObject+56)!=o.limit||
     Word(r,r.submitted_at)!=uint32_t(payload)||r.memory[kObject+10941]!=0xB3||
     Word(r,kStack-128)!=kStack||Word(r,kStack-128+80)!=c.fallback)return false;
  const auto& ctx=r.context;
  if(ctx.r1.u64!=kStack||ctx.lr!=kCaller||ctx.r28.u64!=initial.r28.u64||
     ctx.r29.u64!=initial.r29.u64||ctx.r30.u64!=initial.r30.u64||ctx.r31.u64!=initial.r31.u64||
     ctx.guest_pc!=initial.guest_pc||ctx.current_function!=initial.current_function)return false;
  unsigned locks=0,submits=0;
  for(size_t i=0;i<r.calls.size();++i){
    const auto& call=r.calls[i];
    if(call.kind!=o.order[i]||call.lr!=o.lrs[i]||call.function!=0x824199B0)return false;
    if(call.sp!=(call.kind==Kind::Save||call.kind==Kind::Restore?kStack:kStack-128))return false;
    if(call.kind==Kind::Lock){if(call.args[0]!=kObject||call.args[1]!=(locks++==0?1u:0u))return false;}
    if(call.kind==Kind::Prepare&&(call.args[0]!=kObject||call.args[1]!=0))return false;
    if(call.kind==Kind::Refill&&call.args[0]!=kObject)return false;
    if(call.kind==Kind::Submit){
      ++submits;
      if(call.args!=std::array<uint64_t,5>{kObject,o.submit_cursor,0,kPacket,payload}||call.pc!=0x82419AE4)return false;
    }
  }
  return locks==2&&submits==1;
}
bool Pair(const Result& original,const Result& candidate,const Case& c){
  // Validate BOTH payload flows before normalizing. Otherwise normalization
  // could erase a candidate corruption after an otherwise-correct submission.
  if(!PayloadFlow(original,c,false)||!PayloadFlow(candidate,c,true))return false;
  if(c.selector!=2)return original.calls==candidate.calls&&original.memory==candidate.memory&&
    ContextEqual(original.context,candidate.context);
  // Only the payload is allowed to differ. It survives in volatile R7 through
  // the final lock and mocked restore; nonvolatile R30 is restored identically.
  auto adjusted=candidate;const auto old_payload=Payload(c,false);
  bool after_submit=false;
  for(auto& call:adjusted.calls){
    if(call.kind==Kind::Submit)after_submit=true;
    if(after_submit)call.args[4]=old_payload;
  }
  adjusted.context.r7.u64=old_payload;
  for(unsigned i=0;i<4;++i)adjusted.memory[adjusted.submitted_at+i]=uint8_t(old_payload>>(24-8*i));
  return original.calls==adjusted.calls&&original.memory==adjusted.memory&&
    ContextEqual(original.context,adjusted.context);
}
bool PayloadNegativeTests(){
  const Case c{2,37,0x89abcdefu,0,1};
  const auto original=Run(OriginalSelector,c);const auto candidate=Run(CandidateSelector,c);
  const bool valid_original=Valid(original,c,false),valid_candidate=Valid(candidate,c,true);
  const bool clean_pair=Pair(original,candidate,c);
  if(!valid_original||!valid_candidate||!clean_pair){
    std::cerr<<"Late-payload clean precondition: original="<<valid_original
             <<" candidate="<<valid_candidate<<" pair="<<clean_pair<<'\n';return false;
  }
  unsigned rejected=0;
  for(bool alter_candidate:{false,true}){
    auto bad=alter_candidate?candidate:original;
    bad.context.r7.u64^=1; // Submission trace and submitted memory remain intact.
    if(!Valid(bad,c,alter_candidate)&&
       !(alter_candidate?Pair(original,bad,c):Pair(bad,candidate,c)))++rejected;
    bad=alter_candidate?candidate:original;
    bool submitted=false,changed=false;
    for(auto& call:bad.calls){
      if(submitted){call.args[4]^=1;changed=true;break;}
      if(call.kind==Kind::Submit)submitted=true;
    }
    if(changed&&!Valid(bad,c,alter_candidate)&&
       !(alter_candidate?Pair(original,bad,c):Pair(bad,candidate,c)))++rejected;
  }
  if(rejected!=4)std::cerr<<"Late-payload rejected controls="<<rejected<<" expected=4\n";
  return rejected==4;
}
bool BoundaryTests(){
  Memory m;unsigned rejected=0;
  auto deny=[&](auto f){try{f();}catch(const std::runtime_error&){++rejected;}};
  deny([&]{m.Read(kObject+48,4);}); // recognized but not initialized
  deny([&]{m.Read(kSize-1,4);});
  deny([&]{m.Read(kObject+49,4);});
  deny([&]{m.Write(kObject+13596,4,2);}); // selector inputs read-only in body
  deny([&]{m.Write(0x8000,4,1);});
  deny([&]{m.Write(kCommand+1,4,1);});
  return rejected==6;
}
}

int main(int argc,char** argv){
  using namespace selector_replay;using Json=nlohmann::json;
  if(argc>2||(argc==2&&std::string(argv[1])!="--mutate-payload"))return 2;
  const bool mutation=argc==2;
  try{
    Memory::Need(BoundaryTests(),"synthetic memory negative controls");
    Memory::Need(ContextComparisonTests(),"all named context bytes compared, only padding excluded");
    constexpr std::array<uint32_t,8> selectors{0,1,2,4,0x80000000u,3,5,0xffffffffu};
    constexpr std::array<uint64_t,8> overlays{0,1,0x100,0x200,0x300,0x100000000ull,0x8000000000000001ull,0xffffffffull};
    uint64_t rows=0,changed=0,masked=0,nonselector2=0,refills=0;
    for(auto selected:selectors)for(uint32_t low=0;low<128;++low)for(auto overlay:overlays)for(uint32_t mode=0;mode<4;++mode){
      const uint32_t fallback=selected==3?0:selected==5?0x89abcdefu:0xffffffffu;
      const Case c{selected,low,fallback,mode,overlay};
      const auto a=Run(OriginalSelector,c);const auto b=Run(CandidateSelector,c);
      ++rows;refills+=Expected(c).refills;
      if(!Valid(a,c,false)||!Valid(b,c,true)||!Pair(a,b,c)){
        std::cout<<Json{{"schema","mcla-interval1-selector-replay-v1"},{"status","BASELINE_MISMATCH"},
          {"rows_checked",rows},{"selector",selected},{"low",low},{"overlay",overlay},{"mode",mode},
          {"physical_acceptance",false},{"gpu_behavior_proven",false},{"fps_proven",false}}.dump()<<'\n';return 1;
      }
      if(selected!=2)++nonselector2;
      else if(Payload(c,false)==Payload(c,true))++masked;else ++changed;
    }
    Memory::Need(PayloadNegativeTests(),"late payload corruption negative controls");
    bool detected=false;
    if(mutation){
      // A selector4 case must remain identical: no candidate oracle exception.
      const Case c{4,37,0x89abcdefu,0,1};
      const auto a=Run(OriginalSelector,c);const auto mutant=Run(CandidateSelector,c,true);
      detected=!Valid(mutant,c,true)||!Pair(a,mutant,c);
    }
    std::cout<<Json{{"schema","mcla-interval1-selector-replay-v1"},
      {"status",mutation?(detected?"MUTATION_DETECTED":"MUTATION_UNDETECTED"):"SYNTHETIC_SEAM_PASS"},
      {"clean_matching_rows",rows},{"nonselector2_equal_rows",nonselector2},
      {"selector2_changed_payload_rows",changed},{"selector2_or_masked_rows",masked},
      {"expected_refill_calls_per_variant",refills},{"memory_negative_controls",6},
      {"late_payload_negative_controls",4},
      {"context_state_byte_negative_controls",ContextStateBytes()},
      {"context_padding_bytes_excluded",sizeof(PPCContext)-ContextStateBytes()},
      {"mocked_callees",true},{"actual_gpu_executed",false},{"actual_clock_observed",false},
      {"physical_acceptance",false},{"gpu_behavior_proven",false},{"fps_proven",false}}.dump()<<'\n';
    return mutation?(detected?1:3):0;
  }catch(const std::exception& e){active=nullptr;std::cerr<<"Selector replay error: "<<e.what()<<'\n';return 2;}
}
