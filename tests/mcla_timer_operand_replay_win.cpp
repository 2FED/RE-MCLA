// OFFLINE exact-body replay, default-off test source. No game or real clock.
// Primary supplies pinned timer-plain.inc defining TimerPlain and HelperPlain.
// Input header (28 bytes): magic "MCLATR1\0", LE u32 version=1,count=1..4096,
// then three LE u32 constant bits for 82011110,82001D14,82000ED4 respectively.
// Each 244-byte row: LE u32 kind(1 timer,2 helper); LE u64 originalR3,R4,R5,F1;
// LE u64 controlledBelowTB,controlledAboveTB; before[96],after[96] raw guest
// bytes in ascending object-offset order, excluding 0..7,57,59,74,75.
// Baseline timer timebase is INJECTED from expected after +64, not measured.
// Helper timestamp fields must be zero. Original pointer is identity metadata;
// only its low32 is remapped to 0x1000, high32 preserved; stack is synthetic.
// Usage: test.exe input.bin [--mutate-input|--mutate-store]
// Exit 0=nearest baseline matches and all scenario guards hold;1=mismatch or
// detected mutation;2=invalid input/runtime/scenario;3=undetected mutation.
#include <Windows.h>
#include <immintrin.h>
#include "mcla_init.h"
#include <nlohmann/json.hpp>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

REX_EXTERN(TimerPlain);
REX_EXTERN(HelperPlain);
namespace replay {
using Json=nlohmann::json;
using State=std::array<uint8_t,96>;
constexpr uint32_t kObject=0x1000,kStack=0xeff0;
constexpr std::array<uint32_t,3> kConstants{0x82011110,0x82001d14,0x82000ed4};
constexpr size_t kMaxRows=4096,kRowBytes=244,kHeaderBytes=28;
bool Known(unsigned i){return i>=8&&i<108&&i!=57&&i!=59&&i!=74&&i!=75;}
struct Row {uint32_t kind{};uint64_t r3{},r4{},r5{},f1{},below{},above{};State before{},after{};};
struct Input {std::array<uint32_t,3> constants{};std::vector<Row> rows;};
struct Memory {std::array<uint8_t,108> object{};std::array<uint8_t,8> stack{};
  std::array<uint32_t,3> constants{};uint64_t timebase{};
  unsigned timebase_calls{},reads{},writes{},stack_writes{};
  uint8_t stack_initialized{};
};
Memory* active{};
uint32_t Le32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
uint64_t Le64(const uint8_t* p){return uint64_t(Le32(p))|(uint64_t(Le32(p+4))<<32);}
void Need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
uint64_t Big(const uint8_t* p,unsigned n){uint64_t x=0;for(unsigned i=0;i<n;++i)x=(x<<8)|p[i];return x;}
void Put(uint8_t* p,unsigned n,uint64_t x){for(unsigned i=0;i<n;++i)p[n-1-i]=static_cast<uint8_t>(x>>(8*i));}
void Unpack(Memory& m,const State& s){m.object.fill(0xa5);m.stack.fill(0x5a);size_t j=0;
  for(unsigned i=0;i<108;++i)if(Known(i))m.object[i]=s[j++];Need(j==s.size(),"state allowlist");}
State Pack(const Memory& m){State s{};size_t j=0;for(unsigned i=0;i<108;++i)if(Known(i))s[j++]=m.object[i];return s;}
uint8_t* Address(uint32_t at,unsigned n,bool write){
  Need(active&&n&&n<=8,"active bounded memory access");
  if(at>=kObject&&uint64_t(at)+n<=uint64_t(kObject)+108){
    const unsigned off=at-kObject;for(unsigned i=0;i<n;++i)Need(Known(off+i),"unobserved object gap access");
    return active->object.data()+off;
  }
  if(at>=kStack&&uint64_t(at)+n<=uint64_t(kStack)+8){
    const unsigned offset=at-kStack;
    const auto mask=static_cast<uint8_t>(((1u<<n)-1u)<<offset);
    if(write){++active->stack_writes;active->stack_initialized|=mask;}
    else Need((active->stack_initialized&mask)==mask,"synthetic stack read before write");
    return active->stack.data()+offset;
  }
  throw std::runtime_error("unmapped or read-only guest address");
}
uint64_t Load(uint32_t at,unsigned n){Need(active,"active load");++active->reads;
  for(unsigned i=0;i<3;++i)if(at==kConstants[i]){Need(n==4,"constant width");return active->constants[i];}
  return Big(Address(at,n,false),n);
}
void Store(uint32_t at,unsigned n,uint64_t value){Need(active,"active store");++active->writes;Put(Address(at,n,true),n,value);}
uint64_t Timebase(){Need(active,"active timebase");Need(++active->timebase_calls==1,"timebase retry forbidden");
  Need(uint32_t(active->timebase)!=0,"zero low timebase would loop");return active->timebase;}
Input Read(const char* path){std::ifstream in(path,std::ios::binary|std::ios::ate);Need(bool(in),"input open");
  const auto size=in.tellg();Need(size>=static_cast<std::streamoff>(kHeaderBytes)&&size<=static_cast<std::streamoff>(kHeaderBytes+kRowBytes*kMaxRows),"bounded input size");
  std::vector<uint8_t> b(static_cast<size_t>(size));in.seekg(0);in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
  Need(bool(in)&&in.peek()==std::char_traits<char>::eof(),"complete input read");
  constexpr uint8_t magic[8]={'M','C','L','A','T','R','1',0};const auto count=Le32(b.data()+12);
  Need(!std::memcmp(b.data(),magic,8)&&Le32(b.data()+8)==1&&count&&count<=kMaxRows&&b.size()==kHeaderBytes+kRowBytes*count,"magic/version/count/exact EOF");
  Input result;for(unsigned i=0;i<3;++i)result.constants[i]=Le32(b.data()+16+4*i);
  size_t pos=kHeaderBytes;result.rows.resize(count);
  for(auto& r:result.rows){r.kind=Le32(b.data()+pos);pos+=4;
    for(auto* v:{&r.r3,&r.r4,&r.r5,&r.f1,&r.below,&r.above}){*v=Le64(b.data()+pos);pos+=8;}
    for(auto* s:{&r.before,&r.after}){std::memcpy(s->data(),b.data()+pos,s->size());pos+=s->size();}
    Need((r.kind==1||r.kind==2)&&uint32_t(r.r3)!=0,"kind and original pointer");
    Need(r.kind==1||(!r.below&&!r.above),"helper has no synthetic timebase cases");
  }return result;
}
struct Result {State state{};unsigned mxcsr{},reads{},writes{},stack_writes{},timebase_calls{};uint64_t f1{};};
Result Run(const Row& row,const std::array<uint32_t,3>& constants,unsigned csr,unsigned variant,unsigned mutation){
  Memory m;Unpack(m,row.before);m.constants=constants;
  Memory expected;Unpack(expected,row.after);
  m.timebase=row.kind==1?Big(expected.object.data()+64,8):0;
  if(variant){Need(row.kind==1&&variant<=4,"timer-only bounded variant");
    const double f1=std::bit_cast<double>(row.f1);
    const float limit=std::bit_cast<float>(static_cast<uint32_t>(Big(m.object.data()+32,4)));
    const float scale=std::bit_cast<float>(static_cast<uint32_t>(Big(m.object.data()+84,4)));
    Need(!m.object[56]&&!m.object[58]&&!m.object[72]&&std::isfinite(f1)&&f1<0&&std::isfinite(limit)&&limit>0&&std::isfinite(scale)&&scale>0,"controlled branch eligibility");
    const uint64_t prior=Big(m.object.data()+64,8);
    Need(row.below>prior&&row.above>row.below&&row.above-prior<=0x7fffffffffffffffull&&uint32_t(row.below)&&uint32_t(row.above),"ordered bounded synthetic timestamps");
    m.timebase=variant<=2?row.below:row.above;
    Put(m.object.data()+60,4,(variant==2||variant==4)?1:0);
  }
  // Negative control changes an input constant, not expected state or comparator.
  if(mutation==1)m.constants[1]^=0x00800000u;
  PPCContext ctx{};ctx.r3.u64=(row.r3&0xffffffff00000000ull)|kObject;ctx.r1.u64=kStack+16;
  ctx.r4.u64=row.r4;ctx.r5.u64=row.r5;ctx.f1.u64=row.f1;
  _mm_setcsr(csr);ctx.fpscr.csr=csr;active=&m;
  try{if(row.kind==1)TimerPlain(ctx,nullptr);else HelperPlain(ctx,nullptr);}catch(...){active=nullptr;throw;}
  active=nullptr;
  Need(m.timebase_calls==(row.kind==1?1u:0u),"exact injected timebase calls");
  for(unsigned i=0;i<108;++i)if(!Known(i))Need(m.object[i]==0xa5,"gap poison preservation");
  if(variant){const auto before104=std::bit_cast<float>(static_cast<uint32_t>(Big(m.object.data()+104,4)));
    Memory original;Unpack(original,row.before);const auto limit=std::bit_cast<float>(static_cast<uint32_t>(Big(original.object.data()+32,4)));
    Need(std::isfinite(before104)&&(variant<=2?before104<limit:before104>limit),"actual pre-cap field104 does not bracket configured32");
  }
  // Negative control corrupts actual output after execution, not its expected row.
  if(mutation==2)m.object[11]^=1;
  return {Pack(m),_mm_getcsr(),m.reads,m.writes,m.stack_writes,m.timebase_calls,ctx.f1.u64};
}
Json Output(const Result& r){return Json{{"known_state",r.state},{"exit_mxcsr",r.mxcsr},{"reads",r.reads},
  {"writes",r.writes},{"stack_writes",r.stack_writes},{"timebase_calls",r.timebase_calls},{"exit_f1_bits",r.f1}};}
struct Restore {unsigned csr=_mm_getcsr();~Restore(){_mm_setcsr(csr);}};
}

// Exact body arithmetic/control flow is unchanged. Only its guest memory/time
// access interfaces are redirected into the checked synthetic environment.
#undef REX_LOAD_U8
#undef REX_LOAD_U32
#undef REX_LOAD_U64
#undef REX_STORE_U8
#undef REX_STORE_U32
#undef REX_STORE_U64
#undef REX_QUERY_TIMEBASE
#define REX_LOAD_U8(x) (static_cast<void>(base),static_cast<uint8_t>(replay::Load(static_cast<uint32_t>(x),1)))
#define REX_LOAD_U32(x) (static_cast<void>(base),static_cast<uint32_t>(replay::Load(static_cast<uint32_t>(x),4)))
#define REX_LOAD_U64(x) (static_cast<void>(base),replay::Load(static_cast<uint32_t>(x),8))
#define REX_STORE_U8(x,y) (static_cast<void>(base),replay::Store(static_cast<uint32_t>(x),1,static_cast<uint8_t>(y)))
#define REX_STORE_U32(x,y) (static_cast<void>(base),replay::Store(static_cast<uint32_t>(x),4,static_cast<uint32_t>(y)))
#define REX_STORE_U64(x,y) (static_cast<void>(base),replay::Store(static_cast<uint32_t>(x),8,static_cast<uint64_t>(y)))
#define REX_QUERY_TIMEBASE() replay::Timebase()
#include "timer-plain.inc"

int main(int argc,char** argv){using namespace replay;Restore restore;
  try{unsigned mutation=0;if(argc==3){if(!std::strcmp(argv[2],"--mutate-input"))mutation=1;
      else if(!std::strcmp(argv[2],"--mutate-store"))mutation=2;else throw std::runtime_error("unknown mutation");}
    else Need(argc==2,"usage input.bin [--mutate-input|--mutate-store]");
    const auto input=Read(argv[1]);uint64_t clean=0,timers=0,helpers=0;
    for(const auto& r:input.rows){clean+=Run(r,input.constants,0x1f80,0,0).state==r.after;timers+=r.kind==1;helpers+=r.kind==2;}
    Json report{{"schema","mcla-timer-operand-replay-v1"},{"rows",input.rows.size()},{"timer_rows",timers},
      {"helper_rows",helpers},{"constant_bits",input.constants},{"mutation",mutation},{"clean_nearest_matching_rows",clean},
      {"runtime_mxcsr_observed",false},{"independent_clock_proof",false},{"simulation_proven",false},{"fps_proven",false},
      {"runtime_timing_proven",false},{"state_units_proven",false},
      {"per_row_output_mode","nearest_only"},{"other_modes_aggregate_only",true},
      {"timebase_source","INJECTED_CAPTURED_AFTER64_OR_EXPLICIT_SYNTHETIC_TIMESTAMP"},
      {"variant_order",{"baseline","below_raw60_zero","below_fixed60_one","above_raw60_zero","above_fixed60_one"}},
      {"modes",Json::array()},{"nearest_rows",Json::array()}};
    constexpr std::array<unsigned,4> modes{0x1f80,0x3f80,0x5f80,0x7f80};
    constexpr std::array<const char*,4> names{"nearest","down","up","toward_zero"};
    uint64_t nearest=0;
    for(unsigned mode=0;mode<4;++mode){uint64_t match=0,timer_match=0,helper_match=0,matching_bytes=0;
      std::array<uint64_t,5> changed{};std::map<unsigned,uint64_t> csrs;
      for(size_t i=0;i<input.rows.size();++i){const auto& row=input.rows[i];
        const auto baseline=Run(row,input.constants,modes[mode],0,mutation);++csrs[baseline.mxcsr];
        const bool same=baseline.state==row.after;match+=same;timer_match+=same&&row.kind==1;helper_match+=same&&row.kind==2;
        for(size_t b=0;b<96;++b)matching_bytes+=baseline.state[b]==row.after[b];
        Json variants=Json::array();if(mode==0)variants.push_back(Output(baseline));
        if(row.kind==1)for(unsigned v=1;v<=4;++v){const auto result=Run(row,input.constants,modes[mode],v,mutation);
          ++csrs[result.mxcsr];changed[v]+=result.state!=baseline.state;if(mode==0)variants.push_back(Output(result));}
        if(mode==0)report["nearest_rows"].push_back(Json{{"row_index",i},{"kind",row.kind},
          {"original_r3",row.r3},{"synthetic_r3",(row.r3&0xffffffff00000000ull)|kObject},{"variants",variants}});
      }
      Json csr_counts=Json::array();for(const auto& [csr,n]:csrs)csr_counts.push_back(Json{{"mxcsr",csr},{"calls",n}});
      report["modes"].push_back(Json{{"name",names[mode]},{"entry_mxcsr",modes[mode]},
        {"matching_rows",match},{"matching_timer_rows",timer_match},{"matching_helper_rows",helper_match},
        {"matching_known_bytes",matching_bytes},{"changed_rows_by_variant",changed},{"exit_mxcsr_counts",csr_counts}});
      if(mode==0)nearest=match;
    }
    int code=0;if(clean!=input.rows.size()){code=1;report["status"]="CLEAN_BASELINE_MISMATCH";}
    else if(nearest!=input.rows.size()){code=1;report["status"]=mutation?"MUTATION_DETECTED":"BASELINE_MISMATCH";}
    else if(mutation){code=3;report["status"]="MUTATION_NOT_DETECTED";}
    else report["status"]="OFFLINE_BASELINE_MATCH_SYNTHETIC_VARIANTS_ONLY";
    const auto out=report.dump();Need(out.size()<=32*1024*1024,"bounded JSON exceeded");std::cout<<out<<'\n';return std::cout?code:2;
  }catch(const std::exception& e){std::cerr<<"timer replay rejected: "<<e.what()<<'\n';return 2;}
  catch(...){std::cerr<<"timer replay rejected: unknown exception\n";return 2;}
}
