// Offline exact-body operand replay; no game, save, GPU or observer execution.
// Caller supplies reviewed store-plain.inc defining BodyPlain and its pinned
// compiler/dependencies. This source contains no extracted proprietary body.
// Input: magic bytes {'M','C','L','A','S','R','1',0}, LE u32 version=1,
// LE u32 count (1..4096), then count rows of seven vectors, four LE u32 each:
// incoming V1, old, rate, computed, auxiliary, scaled, stored. Exact EOF required.
// These are register u32 lane order, not guest memory order. No implicit paths.
// Usage: test.exe input.bin [--mutate-input|--mutate-store]
// stdout JSON <=32 MiB. Exit 0=nearest clean baseline matches; 1=actual baseline
// mismatch (including detected mutant); 2=input/runtime failure; 3=undetected
// requested mutation. Other rounding modes are diagnostic, not baseline gates.
#include <Windows.h>
#include <immintrin.h>
#include "mcla_init.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

REX_EXTERN(BodyPlain);
namespace {
using Vec=std::array<uint32_t,4>;
using Row=std::array<Vec,7>;
using Json=nlohmann::json;
constexpr size_t kMemory=65536,kMaxRows=4096,kHeader=16,kRowBytes=112;
constexpr uint32_t kObject=0x1000,kOld=0x1100,kRate=0x1110,kAux=0x1120;
struct Result { Vec old{},rate{},computed{},aux{},scaled{},stored{};
  unsigned tails{},mxcsr{}; bool memory_unchanged_elsewhere{}; };
Result* active{};
unsigned mutation{};
bool Finite(uint32_t b){return (b&0x7f800000u)!=0x7f800000u;}
bool Nan(uint32_t b){return (b&0x7f800000u)==0x7f800000u&&(b&0x7fffffu);}
Vec Bits(const PPCVRegister& r){return {r.u32[0],r.u32[1],r.u32[2],r.u32[3]};}
void Put(uint8_t* base,uint32_t at,const Vec& v){
  for(unsigned i=0;i<4;++i){const uint32_t x=_byteswap_ulong(v[3-i]);std::memcpy(base+at+4*i,&x,4);}
}
Vec Get(const uint8_t* base,uint32_t at){Vec v{};
  for(unsigned i=0;i<4;++i){uint32_t x{};std::memcpy(&x,base+at+4*i,4);v[3-i]=_byteswap_ulong(x);}return v;
}
uint32_t U32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
std::vector<Row> Read(const char* path){
  std::ifstream in(path,std::ios::binary|std::ios::ate);
  if(!in)throw std::runtime_error("cannot open input");
  const auto n=in.tellg();
  if(n<static_cast<std::streamoff>(kHeader)||n>static_cast<std::streamoff>(kHeader+kMaxRows*kRowBytes))
    throw std::runtime_error("bounded input size");
  std::vector<uint8_t> b(static_cast<size_t>(n));in.seekg(0);
  in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));
  if(!in||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("complete exact input read");
  constexpr uint8_t magic[8]={'M','C','L','A','S','R','1',0};
  const uint32_t count=U32(b.data()+12);
  if(std::memcmp(b.data(),magic,8)||U32(b.data()+8)!=1||!count||count>kMaxRows||b.size()!=kHeader+count*kRowBytes)
    throw std::runtime_error("magic/version/count/exact EOF");
  std::vector<Row> rows(count);size_t p=kHeader;
  for(auto& row:rows)for(auto& v:row)for(auto& x:v){x=U32(b.data()+p);p+=4;}
  return rows;
}
Vec Factor(const Vec& incoming,unsigned variant){
  if(variant==0)return incoming;
  if(variant==3)return Vec{}; // Explicit positive zero, including NaN inputs.
  alignas(16) Vec out{};
  const auto v=_mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(incoming.data())));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(out.data()),_mm_castps_si128(_mm_mul_ps(v,_mm_set1_ps(variant==1?0.5f:2.0f))));
  return out;
}
Result Run(uint8_t* base,const Row& row,const Vec& factor,unsigned csr,unsigned fault){
  std::memset(base,0xa5,kMemory);
  auto old=row[1];
  if(fault==1){ // Mutate an actual input, not the expected output/comparator.
    bool changed=false;
    for(auto& x:old)if(Finite(x)&&(x&0x7fffffffu)){x^=0x80000000u;changed=true;break;}
    if(!changed)old[0]^=0x00800000u;
  }
  Put(base,kOld,old);Put(base,kRate,row[2]);Put(base,kAux,row[4]);
  std::array<uint8_t,kMemory> before{};std::memcpy(before.data(),base,kMemory);
  PPCContext ctx{};ctx.r3.u64=kObject;ctx.r1.u64=0xf000;ctx.lr=0x8259F364;
  for(unsigned i=0;i<4;++i)ctx.v1.u32[i]=factor[i];
  _mm_setcsr(csr);ctx.fpscr.csr=csr;
  Result result{};active=&result;mutation=fault;
  try{BodyPlain(ctx,base);}catch(...){active=nullptr;throw;}
  active=nullptr;
  result.memory_unchanged_elsewhere=true;
  for(size_t i=0;i<kMemory;++i)if((i<kOld||i>=kOld+16)&&base[i]!=before[i])result.memory_unchanged_elsewhere=false;
  if(result.tails!=1||!result.memory_unchanged_elsewhere||result.old!=old||result.rate!=row[2]||result.aux!=row[4])
    throw std::runtime_error("body/tail/load/memory confinement invariant");
  return result;
}
struct Counts {
  uint64_t rows{},computed_lanes{},scaled_lanes{},stored_lanes{},all_match_rows{},finite_expected_lanes{},nan_expected_lanes{},other_expected_lanes{};
  std::array<uint64_t,4> finite_matches{},nan_matches{};
  std::array<uint64_t,4> changed_computed_rows{},changed_scaled_rows{},changed_stored_rows{};
  uint64_t zero_eligible_finite_lanes{},zero_old_raw_preserved{},zero_old_numeric_preserved{};
};
void Accumulate(Counts& c,const Row& row,const std::array<Result,4>& rr){
  ++c.rows;const auto& r=rr[0];
  c.all_match_rows+=(r.computed==row[3]&&r.scaled==row[5]&&r.stored==row[6]);
  for(unsigned i=0;i<4;++i){c.computed_lanes+=r.computed[i]==row[3][i];c.scaled_lanes+=r.scaled[i]==row[5][i];c.stored_lanes+=r.stored[i]==row[6][i];}
  for(unsigned v=0;v<4;++v){
    c.changed_computed_rows[v]+=rr[v].computed!=r.computed;
    c.changed_scaled_rows[v]+=rr[v].scaled!=r.scaled;
    c.changed_stored_rows[v]+=rr[v].stored!=r.stored;
    const std::array<Vec,3> expected{row[3],row[5],row[6]},actual{rr[v].computed,rr[v].scaled,rr[v].stored};
    for(unsigned k=0;k<3;++k)for(unsigned i=0;i<4;++i){const auto x=expected[k][i];
      if(v==0){c.finite_expected_lanes+=Finite(x);c.nan_expected_lanes+=Nan(x);c.other_expected_lanes+=!Finite(x)&&!Nan(x);}
      c.finite_matches[v]+=Finite(x)&&actual[k][i]==x;c.nan_matches[v]+=Nan(x)&&actual[k][i]==x;
    }
  }
  for(unsigned i=0;i<4;++i)if(Finite(row[1][i])&&Finite(row[2][i])){
    ++c.zero_eligible_finite_lanes;const auto x=rr[3].stored[i],old=row[1][i];
    c.zero_old_raw_preserved+=x==old;
    c.zero_old_numeric_preserved+=x==old||(!(x&0x7fffffffu)&&!(old&0x7fffffffu));
  }
}
Json Summary(const Counts& c){return Json{{"rows",c.rows},{"original_computed_matching_lanes",c.computed_lanes},
  {"original_scaled_matching_lanes",c.scaled_lanes},{"original_stored_matching_lanes",c.stored_lanes},
  {"original_all_match_rows",c.all_match_rows},{"finite_expected_lanes",c.finite_expected_lanes},
  {"nan_expected_lanes",c.nan_expected_lanes},{"infinite_expected_lanes",c.other_expected_lanes},
  {"finite_raw_matches_by_variant",c.finite_matches},{"nan_raw_matches_by_variant",c.nan_matches},
  {"computed_changed_rows_by_variant",c.changed_computed_rows},{"scaled_changed_rows_by_variant",c.changed_scaled_rows},
  {"stored_changed_rows_by_variant",c.changed_stored_rows},{"zero_eligible_finite_lanes",c.zero_eligible_finite_lanes},
  {"zero_old_raw_preserved",c.zero_old_raw_preserved},{"zero_old_numeric_preserved",c.zero_old_numeric_preserved}};}
struct RestoreCsr {unsigned csr=_mm_getcsr();~RestoreCsr(){_mm_setcsr(csr);}};
struct FreeMemory {void operator()(uint8_t* p)const{if(p)VirtualFree(p,0,MEM_RELEASE);}};
}

REX_EXTERN(sub_8259D030){
  if(!active)throw std::runtime_error("tail without active replay");
  ++active->tails;active->old=Bits(ctx.v0);active->rate=Bits(ctx.v13);active->aux=Bits(ctx.v63);
  active->computed=Bits(ctx.v12);active->scaled=Bits(ctx.v1);active->mxcsr=_mm_getcsr();
  if(mutation==2)base[kOld]^=1; // Corrupt synthetic stored bytes, never expected data.
  active->stored=Get(base,kOld);
}
#include "store-plain.inc"

int main(int argc,char** argv){
  RestoreCsr restore;
  try{
    unsigned fault=0;
    if(argc==3){if(!std::strcmp(argv[2],"--mutate-input"))fault=1;
      else if(!std::strcmp(argv[2],"--mutate-store"))fault=2;else throw std::runtime_error("unknown mutation");}
    else if(argc!=2)throw std::runtime_error("usage: input.bin [--mutate-input|--mutate-store]");
    const auto rows=Read(argv[1]);
    std::unique_ptr<uint8_t,FreeMemory> memory(static_cast<uint8_t*>(VirtualAlloc(nullptr,kMemory,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)));
    if(!memory)throw std::runtime_error("synthetic mapping failed");
    uint64_t clean_matches=0;
    for(const auto& row:rows){const auto r=Run(memory.get(),row,row[0],0x1f80,0);
      clean_matches+=r.computed==row[3]&&r.scaled==row[5]&&r.stored==row[6];}
    Json report{{"schema","mcla-vector-store-operand-replay-v1"},{"rows",rows.size()},
      {"mutation",fault},{"clean_nearest_matching_rows",clean_matches},{"runtime_mxcsr_observed",false},
      {"simulation_proven",false},{"state_units_proven",false},{"player_identity_proven",false},
      {"variant_order",{"original","half","double","positive_zero"}},
      {"factor_preparation_mxcsr",0x1f80},{"modes",Json::array()},{"nearest_rows",Json::array()}};
    const std::array<unsigned,4> modes{0x1f80,0x3f80,0x5f80,0x7f80};
    const std::array<const char*,4> names{"nearest","down","up","toward_zero"};
    uint64_t nearest_matches=0;
    for(unsigned m=0;m<4;++m){Counts counts{};size_t index=0;std::map<unsigned,uint64_t> tail_csr_counts;
      for(const auto& row:rows){std::array<Result,4> rr{};Json variants=Json::array();
        for(unsigned v=0;v<4;++v){_mm_setcsr(0x1f80);const auto factor=Factor(row[0],v);
          rr[v]=Run(memory.get(),row,factor,modes[m],fault);
          ++tail_csr_counts[rr[v].mxcsr];
          if(m==0)variants.push_back(Json{{"factor",factor},{"computed",rr[v].computed},
            {"scaled",rr[v].scaled},{"stored",rr[v].stored},{"tail_mxcsr",rr[v].mxcsr},{"tail_calls",rr[v].tails}});
        }
        Accumulate(counts,row,rr);
        if(m==0)report["nearest_rows"].push_back(Json{{"row_index",index},{"variants",std::move(variants)}});
        ++index;
      }
      if(m==0)nearest_matches=counts.all_match_rows;
      Json csr_inventory=Json::array();for(const auto& [csr,count]:tail_csr_counts)
        csr_inventory.push_back(Json{{"mxcsr",csr},{"calls",count}});
      report["modes"].push_back(Json{{"name",names[m]},{"entry_mxcsr",modes[m]},
        {"tail_mxcsr_counts",std::move(csr_inventory)},{"counts",Summary(counts)}});
    }
    int code=0;
    if(clean_matches!=rows.size()){code=1;report["status"]="CLEAN_BASELINE_MISMATCH";}
    else if(nearest_matches!=rows.size()){code=1;report["status"]=fault?"MUTATION_DETECTED":"BASELINE_MISMATCH";}
    else if(fault){code=3;report["status"]="MUTATION_NOT_DETECTED";}
    else report["status"]="OFFLINE_BASELINE_MATCH_VARIANTS_DIAGNOSTIC_ONLY";
    const auto output=report.dump();if(output.size()>32*1024*1024)throw std::runtime_error("bounded JSON exceeded");
    std::cout<<output<<'\n';if(!std::cout)return 2;return code;
  }catch(const std::exception& e){std::cerr<<"operand replay rejected: "<<e.what()<<'\n';return 2;}
  catch(...){std::cerr<<"operand replay rejected: unknown exception\n";return 2;}
}
