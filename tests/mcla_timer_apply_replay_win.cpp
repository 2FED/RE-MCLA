// Offline exact Apply + setters replay. No game, guest pointers, real clock,
// observer installation, or runtime admission. Primary supplies pinned exact
// renamed apply-plain.inc and policy-setters.inc. Usage: test.exe fixture.json
// Exit 0 = bounded offline checks pass; 2 = invalid input or any failed check.
#include <Windows.h>
#include <immintrin.h>
#include "mcla_init.h"
#include "mcla_fixed60_experiment_policy.h"
#include <nlohmann/json.hpp>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

REX_EXTERN(ApplyPlain);
REX_EXTERN(FixedSetterPlain);
REX_EXTERN(RawSetterPlain);
namespace apply_replay {
using Json = nlohmann::json;
namespace policy = mcla::fixed60_experiment;
constexpr uint32_t kConfig=0x2000, kTimer=0x827D7500, kStack=0xEF00;
constexpr std::array<unsigned,18> kTimerWords{
  8,16,20,24,28,32,36,40,44,48,52,60,84,88,92,96,100,104};
constexpr std::array<unsigned,3> kTimerBytes{56,58,72};
constexpr std::array<uint32_t,4> kConstants{0x82000ED4,0x82001D14,0x820048B4,0x82004288};
unsigned checks{};
void Need(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
uint64_t Big(const uint8_t* p,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;++i)v=(v<<8)|p[i];return v;}
void Put(uint8_t* p,unsigned n,uint64_t v){for(unsigned i=0;i<n;++i)p[n-1-i]=uint8_t(v>>(8*i));}
bool TimerKnown(unsigned i){return i>=8&&i<108&&i!=57&&i!=59&&i!=74&&i!=75;}
bool ConfigKnown(unsigned i){return i>=72&&i<=100;}
struct Memory {
  std::array<uint8_t,108> config{},timer{};
  std::array<uint8_t,256> stack{},initialized{};
  std::array<uint32_t,4> constants{};
  unsigned reads{},writes{};
  Memory(){config.fill(0xA5);timer.fill(0xA5);stack.fill(0x5A);}
};
Memory* active{};
uint8_t* Address(uint32_t at,unsigned n,bool write){
  Need(active&&n&&n<=8,"active bounded access");
  for(auto item:{std::pair{kConfig,&active->config},std::pair{kTimer,&active->timer}}){
    if(at>=item.first&&uint64_t(at)+n<=uint64_t(item.first)+108){
      const auto off=at-item.first;
      for(unsigned i=0;i<n;++i)Need(item.first==kConfig?ConfigKnown(off+i):TimerKnown(off+i),"unobserved object gap access");
      return item.second->data()+off;
    }
  }
  if(at>=kStack&&uint64_t(at)+n<=uint64_t(kStack)+256){
    const auto off=at-kStack;
    for(unsigned i=0;i<n;++i){if(write)active->initialized[off+i]=1;else Need(active->initialized[off+i],"stack read before write");}
    return active->stack.data()+off;
  }
  throw std::runtime_error("unmapped or constant-write address");
}
uint64_t Load(uint32_t at,unsigned n){Need(active,"active load");++active->reads;
  for(unsigned i=0;i<4;++i)if(at==kConstants[i]){Need(n==4,"constant width");return active->constants[i];}
  return Big(Address(at,n,false),n);
}
void Store(uint32_t at,unsigned n,uint64_t v){Need(active,"active store");++active->writes;Put(Address(at,n,true),n,v);}
void Keys(const Json& j,std::initializer_list<const char*> names){Need(j.is_object()&&j.size()==names.size(),"exact object schema");for(auto n:names)Need(j.contains(n),"missing key");}
uint64_t U(const Json& j,uint64_t max=UINT32_MAX){Need(j.is_number_unsigned(),"unsigned integer required");const auto v=j.get<uint64_t>();Need(v<=max,"integer bound");return v;}
void Config(std::array<uint8_t,108>& out,const Json& j){Keys(j,{"words","byte100"});Need(j.at("words").is_array()&&j.at("words").size()==7,"seven config words");
  for(unsigned i=0;i<7;++i)Put(out.data()+72+i*4,4,U(j.at("words")[i]));out[100]=uint8_t(U(j.at("byte100"),255));}
void Timer(std::array<uint8_t,108>& out,const Json& j){Keys(j,{"words","bytes","qword64","reciprocal_bits","count","saved_unsplit_bits","split_state"});
  Need(j.at("words").is_array()&&j.at("words").size()==18&&j.at("bytes").is_array()&&j.at("bytes").size()==3,"timer arrays");
  for(unsigned i=0;i<18;++i)Put(out.data()+kTimerWords[i],4,U(j.at("words")[i]));
  for(unsigned i=0;i<3;++i)out[kTimerBytes[i]]=uint8_t(U(j.at("bytes")[i],255));
  Put(out.data()+64,8,U(j.at("qword64"),UINT64_MAX));Put(out.data()+12,4,U(j.at("reciprocal_bits")));
  Put(out.data()+76,4,U(j.at("count")));Put(out.data()+80,4,U(j.at("saved_unsplit_bits")));out[73]=uint8_t(U(j.at("split_state"),255));
}
Json Read(const char* path){std::ifstream f(path,std::ios::binary|std::ios::ate);Need(bool(f),"fixture open");const auto size=f.tellg();Need(size>0&&size<=65536,"fixture size bound");
  std::string text(size_t(size),'\0');f.seekg(0);f.read(text.data(),std::streamsize(text.size()));Need(bool(f)&&f.peek()==std::char_traits<char>::eof(),"exact fixture read");
  std::vector<std::set<std::string>> keys;
  auto callback=[&](int,Json::parse_event_t e,Json& value){
    if(e==Json::parse_event_t::object_start)keys.emplace_back();
    if(e==Json::parse_event_t::key)Need(!keys.empty()&&keys.back().insert(value.get<std::string>()).second,"duplicate JSON key");
    if(e==Json::parse_event_t::object_end)keys.pop_back();return true;
  };
  return Json::parse(text,callback);
}
policy::Config Values(const Memory& m){auto w=[&](unsigned at){return uint32_t(Big(m.config.data()+at,4));};return {w(72),w(80),w(84),w(92),w(96)};}
policy::Association Association(){return {0x822D26C4,kConfig,kConfig,true,true,true,true,true,true};}
bool Edit(Memory& m,bool enabled,const policy::Association& a){const auto d=policy::Evaluate(enabled,Values(m),a);
  if(d.status==policy::Status::Rejected)return false;
  if(d.status==policy::Status::Admitted)Put(m.config.data()+84,4,d.replacement_rate84_bits);
  return true;
}
void Oracle(Memory& m,bool candidate){
  // Independent finite branch oracle, not calls to extracted Apply/setter.
  // Candidate changes only +84; original Apply recomputes config +76.
  const float rate=candidate?60.0f:30.0f;
  Put(m.config.data()+84,4,std::bit_cast<uint32_t>(rate));Put(m.config.data()+76,4,std::bit_cast<uint32_t>(rate));
  const float one=std::bit_cast<float>(m.constants[1]);
  const float product=float(double(1.0f)*double(rate));
  const float step=float(double(one)/double(product));
  const float inverse=float(double(one)/double(step));
  for(unsigned at:{8u,32u,88u})Put(m.timer.data()+at,4,std::bit_cast<uint32_t>(step));
  Put(m.timer.data()+12,4,std::bit_cast<uint32_t>(inverse));Put(m.timer.data()+28,4,std::bit_cast<uint32_t>(rate));
  Put(m.timer.data()+52,4,1);Put(m.timer.data()+60,4,1);
  Put(m.timer.data()+40,4,m.constants[2]);Put(m.timer.data()+36,4,m.constants[3]);
}
void NonvolatileSeed(PPCContext& c){
  std::array<uint8_t,PPCContext::kNonVolatileSaveSize> bytes{};
  bytes.fill(0x6D);
  for(unsigned i=0;i<18;++i){
    const uint64_t gpr=0x1234000000000000ull+i,fpr=0x3FF0000000000000ull+i;
    std::memcpy(bytes.data()+i*8,&gpr,8);std::memcpy(bytes.data()+144+i*8,&fpr,8);
  }
  c.RestoreNonVolatiles(bytes.data());
  c.r2.u64=0x22222222;c.r13.u64=0x13131313;c.cr2.set_raw(5);c.cr3.set_raw(6);c.cr4.set_raw(9);
}
void Run(Memory& m){PPCContext c{};NonvolatileSeed(c);c.r1.u64=kStack+224;c.r3.u64=kConfig;c.lr=0x822D26C4;
  const auto before=c;std::array<uint8_t,PPCContext::kNonVolatileSaveSize> nv_before{},nv_after{};
  c.SaveNonVolatiles(nv_before.data());_mm_setcsr(0x1F80);c.fpscr.csr=0x1F80;active=&m;
  try{ApplyPlain(c,nullptr);}catch(...){active=nullptr;throw;}active=nullptr;
  Need(c.r1.u64==before.r1.u64&&c.lr==before.lr&&c.r2.u64==before.r2.u64&&c.r13.u64==before.r13.u64,"stack/link/reserved PPC preservation");
  c.SaveNonVolatiles(nv_after.data());Need(nv_before==nv_after,"all SDK nonvolatile GPR/FPR/vector bytes preserved");
  Need(c.cr2.raw()==before.cr2.raw()&&c.cr3.raw()==before.cr3.raw()&&c.cr4.raw()==before.cr4.raw(),"nonvolatile CR fields");
  for(unsigned i=0;i<108;++i){if(!ConfigKnown(i))Need(m.config[i]==0xA5,"config poison");if(!TimerKnown(i))Need(m.timer[i]==0xA5,"timer poison");}
}
void Equal(const Memory& a,const Memory& b){Need(a.config==b.config,"all config bytes match");Need(a.timer==b.timer,"all timer bytes match");}
unsigned Negatives(const Memory& seed){unsigned count=0;
  for(unsigned n=0;n<15;++n){auto m=seed;auto a=Association();
    switch(n){case 0:a.caller_lr^=4;break;case 1:Put(m.config.data()+72,4,1);break;
      case 2:Put(m.config.data()+80,4,0x40000000);break;case 3:Put(m.config.data()+96,4,1);break;
      case 4:Put(m.config.data()+92,4,2);break;case 5:a.config_address+=4;break;
      case 6:a.initializer_is_immediate_parent=false;break;case 7:a.context_matches=false;break;
      case 8:a.base_matches=false;break;case 9:a.config_matches=false;break;
      case 10:a.constructor_association_observed=false;break;case 11:a.thread_matches=false;break;
      case 12:Put(m.config.data()+84,4,0x42700000);break;case 13:a.config_address=a.expected_config_address=0;break;
      case 14:Put(m.config.data()+80,4,0x7FC00000);break;}
    const auto before=m;Need(!Edit(m,true,a),"negative admission rejected");Equal(m,before);
    Need(m.stack==before.stack&&m.reads==before.reads&&m.writes==before.writes,"rejection has no execution or memory effect");
    Need(Edit(m,false,a),"disabled bypass is identity");Equal(m,before);++count;
  }return count;
}
unsigned MemoryNegatives(){Memory m;active=&m;unsigned count=0;
  auto reject=[&](auto call){bool rejected=false;try{call();}catch(const std::runtime_error&){rejected=true;}Need(rejected,"memory negative rejected");++count;};
  try{reject([]{Load(kConfig+68,4);});reject([]{Load(kTimer+57,1);});reject([]{Load(kTimer+107,4);});
    reject([]{Load(kStack,8);});reject([]{Store(kConstants[0],4,0);});reject([]{Load(kConstants[1],8);});
    reject([]{Load(0xFFFFFFFF,4);});reject([]{Store(kConfig+100,4,0);});
  }catch(...){active=nullptr;throw;}active=nullptr;return count;
}
} // namespace apply_replay

#undef REX_LOAD_U8
#undef REX_LOAD_U32
#undef REX_LOAD_U64
#undef REX_STORE_U8
#undef REX_STORE_U32
#undef REX_STORE_U64
#define REX_LOAD_U8(x) (static_cast<void>(base),uint8_t(apply_replay::Load(uint32_t(x),1)))
#define REX_LOAD_U32(x) (static_cast<void>(base),uint32_t(apply_replay::Load(uint32_t(x),4)))
#define REX_LOAD_U64(x) (static_cast<void>(base),apply_replay::Load(uint32_t(x),8))
#define REX_STORE_U8(x,y) (static_cast<void>(base),apply_replay::Store(uint32_t(x),1,uint8_t(y)))
#define REX_STORE_U32(x,y) (static_cast<void>(base),apply_replay::Store(uint32_t(x),4,uint32_t(y)))
#define REX_STORE_U64(x,y) (static_cast<void>(base),apply_replay::Store(uint32_t(x),8,uint64_t(y)))
#include "policy-setters.inc"
#include "apply-plain.inc"

int main(int argc,char** argv){using namespace apply_replay;const auto saved=_mm_getcsr();
  try{Need(argc==2,"usage fixture.json");const auto input=Read(argv[1]);Keys(input,{"schema","constants","rows"});
    Need(input.at("schema")=="mcla-timer-apply-replay-input-v1","fixture schema");
    Need(input.at("constants").is_array()&&input.at("constants").size()==4,"four fixed-branch constants");
    Memory seed;for(unsigned i=0;i<4;++i){const auto& c=input.at("constants")[i];Keys(c,{"address","word"});Need(U(c.at("address"))==kConstants[i],"exact ordered constant address");seed.constants[i]=uint32_t(U(c.at("word")));}
    Need(seed.constants[0]==0&&seed.constants[1]==0x3F800000,"exact zero and one constants");
    const float low=std::bit_cast<float>(seed.constants[2]),high=std::bit_cast<float>(seed.constants[3]);
    Need(std::isfinite(low)&&std::isfinite(high)&&low>0&&high>=low,"finite ordered clamp constants");
    Need(input.at("rows").is_array()&&input.at("rows").size()==2,"exact two captured Apply rows");
    Json scenarios=Json::array();unsigned negatives=0;
    Memory repeated=seed;
    for(unsigned index=0;index<2;++index){const auto& row=input.at("rows")[index];Keys(row,{"id","config_before","config_after","timer_before","timer_after"});
      Need(U(row.at("id"))==(index?6u:3u),"captured row identity and order");
      auto initial=seed;Config(initial.config,row.at("config_before"));Timer(initial.timer,row.at("timer_before"));
      auto expected=seed;Config(expected.config,row.at("config_after"));Timer(expected.timer,row.at("timer_after"));
      Need(policy::Evaluate(true,Values(initial),Association()).status==policy::Status::Admitted,"captured fixed30/two-substep seed");
      _mm_setcsr(0x1F80);auto oracle=initial;Oracle(oracle,false);Equal(oracle,expected);
      auto stock=initial;Need(Edit(stock,false,Association()),"disabled policy accepted");Equal(stock,initial);Run(stock);Equal(stock,expected);
      auto candidate=initial;Need(Edit(candidate,true,Association()),"candidate admitted");
      auto edited_only=initial;Put(edited_only.config.data()+84,4,0x42700000);Equal(candidate,edited_only);
      Run(candidate);auto candidate_expected=initial;_mm_setcsr(0x1F80);Oracle(candidate_expected,true);Equal(candidate,candidate_expected);
      Need(candidate.config!=stock.config&&candidate.timer!=stock.timer,"candidate must differ from stock");
      // Model repeated initializer defaults without executing initializer/options:
      // reset only captured config, retaining the previous synthetic timer.
      if(!index)repeated=initial;else Config(repeated.config,row.at("config_before"));
      auto repeated_expected=repeated;Oracle(repeated_expected,true);Need(Edit(repeated,true,Association()),"reinitialization seed admitted");Run(repeated);Equal(repeated,repeated_expected);
      negatives+=Negatives(initial);
      scenarios.push_back(Json{{"captured_id",U(row.at("id"))},{"stock_snapshot_match",true},{"candidate_oracle_match",true},
        {"candidate_config84_bits",Big(candidate.config.data()+84,4)},{"candidate_config76_bits",Big(candidate.config.data()+76,4)},
        {"candidate_timer8_bits",Big(candidate.timer.data()+8,4)},{"candidate_timer12_bits",Big(candidate.timer.data()+12,4)},
        {"candidate_timer32_bits",Big(candidate.timer.data()+32,4)},{"config96_unchanged",Big(candidate.config.data()+96,4)==2}});
    }
    const auto memory_negatives=MemoryNegatives();
    Json report{{"schema","mcla-timer-apply-replay-v1"},{"status","OFFLINE_APPLY_ORACLE_MATCH_NO_RUNTIME_ADMISSION"},
      {"checks",checks},{"policy_negative_cases",negatives},{"memory_negative_cases",memory_negatives},{"scenarios",scenarios},
      {"synthetic_config_address",kConfig},{"fixed_timer_address",kTimer},{"entry_mxcsr",0x1F80},
      {"initializer_executed",false},{"runtime_association_validated",false},{"runtime_mxcsr_observed",false},
      {"raw_branch_tested",false},{"substep_execution_tested",false},{"object_lifetime_proven",false},
      {"simulation_proven",false},{"fps_proven",false},{"native_admitted",false}};
    _mm_setcsr(saved);std::cout<<report.dump()<<'\n';return std::cout?0:2;
  }catch(const std::exception& e){active=nullptr;_mm_setcsr(saved);std::cerr<<"Apply replay rejected: "<<e.what()<<'\n';return 2;}
  catch(...){active=nullptr;_mm_setcsr(saved);std::cerr<<"Apply replay rejected: unknown exception\n";return 2;}
}
