// Offline exact Apply -> Timer -> Helper(1,2), Helper(1,2), Helper(0,0).
// Primary supplies pinned private includes; no proprietary bodies are copied here.
// Usage: test.exe fixture.json [--mutate-store]
// Exit 0=bounded oracle matches; 1=mismatch/detected mutation; 2=invalid input;
// 3=requested mutation was not detected. No game, real clock or host guest pointers.
#define main MclaAcceptedApplyReplayProgram
#include "mcla_timer_apply_replay_win.cpp"
#undef main
#include <limits>

REX_EXTERN(TimerPlain);
REX_EXTERN(HelperPlain);
namespace chain {
using namespace apply_replay;
constexpr uint32_t kTickConstant=0x82011110;
constexpr std::array<uint64_t,5> kDeltas{833333,2500000,833333,7500000,500};
constexpr std::array<const char*,5> kLabels{"nominal","dip","recovery","large","tiny"};
uint32_t coefficient{};
uint64_t stamp{};
unsigned timebase_calls{},mutation_count{};
bool mutate{},timer_active{};
uint64_t Load(uint32_t at,unsigned n){
  if(at==kTickConstant){Need(active&&n==4,"explicit fifth constant width");++active->reads;return coefficient;}
  return apply_replay::Load(at,n);
}
void Store(uint32_t at,unsigned n,uint64_t value){
  // Fault injection affects the actual extracted body's first factor write only.
  // It never changes the oracle, comparator, or expected captured fixture.
  if(mutate&&timer_active&&!mutation_count&&at==kTimer+8&&n==4){value^=1;++mutation_count;}
  apply_replay::Store(at,n,value);
}
uint64_t Timebase(){Need(active&&++timebase_calls==1,"one controlled timebase query");
  Need(uint32_t(stamp)!=0,"zero low timebase would spin");return stamp;}
float F(const Memory& m,unsigned off){return std::bit_cast<float>(uint32_t(Big(m.timer.data()+off,4)));}
uint32_t W(const Memory& m,unsigned off){return uint32_t(Big(m.timer.data()+off,4));}
void SW(Memory& m,unsigned off,uint32_t v){Put(m.timer.data()+off,4,v);}
void SF(Memory& m,unsigned off,float v){SW(m,off,std::bit_cast<uint32_t>(v));}
float R(double v){return static_cast<float>(v);}
float Mul(float a,float b){return R(double(a)*double(b));}
float Div(float a,float b){return R(double(a)/double(b));}
void Add(Memory& m,unsigned off,float v){SF(m,off,R(double(F(m,off))+double(v)));}
float C(const Memory& m,unsigned i){return std::bit_cast<float>(m.constants[i]);}
float CF(const Memory& m,unsigned off){return std::bit_cast<float>(uint32_t(Big(m.config.data()+off,4)));}
void SCF(Memory& m,unsigned off,float v){Put(m.config.data()+off,4,std::bit_cast<uint32_t>(v));}
float Clamp(float v,float lo,float hi){
  const float raised=R(double(v)-double(lo))>=0?v:lo;
  return R(double(raised)-double(hi))>=0?hi:raised;
}
void Eligible(const Memory& m){
  const auto mode=Big(m.config.data()+72,4);
  Need(mode<=1,"only raw/fixed Apply mode");
  Need(CF(m,80)==1.0f&&CF(m,84)>0&&std::isfinite(CF(m,84)),"explicit positive Apply rate and unit config scale");
  Need(Big(m.config.data()+92,4)==1&&Big(m.config.data()+96,4)==2,"one divisor and two substeps");
  Need(std::isfinite(CF(m,88)),"finite config clamp request");
  for(unsigned off:{8u,12u,16u,20u,24u,28u,32u,36u,40u,80u,84u,88u,92u,96u,100u,104u})
    Need(std::isfinite(F(m,off)),"finite known timer float seed");
  Need(F(m,84)>0,"positive synthetic timer scale");
  Need(!m.timer[73]&&W(m,76)==0,"unsplit initial timer");
  for(unsigned off:{56u,58u,72u})Need(m.timer[off]<=1,"boolean flag scope");
}
void ApplyOracle(Memory& m){
  const float derived=Div(CF(m,84),CF(m,80));SCF(m,76,derived);
  if(Big(m.config.data()+72,4)==1){
    SF(m,28,derived);SW(m,60,0);SF(m,32,C(m,0));
  }else{
    SF(m,28,derived);SW(m,52,1);SW(m,60,1);
    const float step=Div(C(m,1),derived);
    for(unsigned off:{8u,32u,88u})SF(m,off,step);
    SF(m,12,Div(C(m,1),step));
  }
  SF(m,40,C(m,2));
  const bool capped=Big(m.config.data()+72,4)==1&&CF(m,88)>0;
  SF(m,36,capped?Div(C(m,1),CF(m,88)):C(m,3));
}
void TimerOracle(Memory& m,uint64_t now,bool advance_secondary){
  const auto prior=Big(m.timer.data()+64,8);
  Need(now>prior&&now-prior<=uint64_t(INT64_MAX)&&uint32_t(now),"positive bounded controlled timebase");
  const float ticks=R(double(int64_t(now-prior)));
  const float raw=Mul(ticks,std::bit_cast<float>(coefficient)),scale=F(m,84),scaled=Mul(raw,scale);
  Put(m.timer.data()+64,8,now);
  SF(m,92,raw);SF(m,100,raw);SF(m,96,scaled);SF(m,88,raw);SF(m,104,scaled);SF(m,8,scaled);
  bool deferred=m.timer[72]!=0;
  if(!m.timer[56]){
    SF(m,16,F(m,20));
    if(m.timer[58]||W(m,60)!=0){
      const float fixed=Mul(F(m,32),scale);SF(m,88,F(m,32));SF(m,8,fixed);
      Add(m,20,fixed);if(advance_secondary)Add(m,24,fixed);deferred=false;
    }else{
      if(!m.timer[72]){Add(m,20,scaled);if(advance_secondary)Add(m,24,scaled);}
      const float limit=F(m,32);
      if(limit!=C(m,0)){
        if(scaled<limit)SW(m,52,1);
        else{SF(m,88,limit);SF(m,8,Mul(limit,scale));}
      }
    }
  }else{
    float reset=F(m,32);
    if(W(m,60)==0&&reset==C(m,0))reset=Div(C(m,1),F(m,28));
    SF(m,8,reset);
    if(!m.timer[72]){Add(m,20,reset);if(advance_secondary)Add(m,24,reset);}
    SF(m,88,reset);m.timer[56]=0;
  }
  SW(m,44,W(m,44)+1u);if(advance_secondary)SW(m,48,W(m,48)+1u);
  SF(m,8,Clamp(F(m,8),F(m,40),F(m,36)));
  SF(m,88,Clamp(F(m,88),F(m,40),F(m,36)));
  if(deferred){Add(m,20,F(m,8));if(advance_secondary)Add(m,24,F(m,8));}
  SF(m,12,Div(C(m,1),F(m,8)));
}
void HelperOracle(Memory& m,bool split){
  if(split){
    if(m.timer[73]){SF(m,8,F(m,80));SF(m,12,Div(C(m,1),F(m,80)));}
    const float whole=F(m,8);SF(m,80,whole);m.timer[73]=1;SW(m,76,2);
    SF(m,8,Div(whole,2.0f));SF(m,12,Div(C(m,1),F(m,8)));
  }else if(m.timer[73]){
    SF(m,8,F(m,80));m.timer[73]=0;SW(m,76,0);SF(m,12,Div(C(m,1),F(m,8)));
  }
}
void Execute(Memory& m,unsigned stage,bool secondary){
  Need(stage<3,"timer or split/restore stage");
  if(stage==0){const auto before=Big(m.timer.data()+64,8);Need(stamp>before&&stamp-before<=uint64_t(INT64_MAX)&&uint32_t(stamp),"input clock eligibility");}
  m.initialized.fill(0);m.stack.fill(0x5A);timebase_calls=0;
  PPCContext ctx{};NonvolatileSeed(ctx);ctx.r1.u64=kStack+224;ctx.r3.u64=kTimer;ctx.lr=0x12345678;
  ctx.f1.f64=-1.0;ctx.r4.u64=stage==1?1:0;ctx.r5.u64=stage==0?(secondary?1:0):(stage==1?2:0);
  const auto before=ctx;std::array<uint8_t,PPCContext::kNonVolatileSaveSize> a{},b{};ctx.SaveNonVolatiles(a.data());
  _mm_setcsr(0x1F80);ctx.fpscr.csr=0x1F80;active=&m;timer_active=stage==0;
  try{if(stage==0)TimerPlain(ctx,nullptr);else HelperPlain(ctx,nullptr);}
  catch(...){active=nullptr;timer_active=false;throw;}
  active=nullptr;timer_active=false;
  Need(timebase_calls==(stage==0?1u:0u),"exact controlled timebase query count");
  ctx.SaveNonVolatiles(b.data());Need(a==b,"all nonvolatile GPR/FPR/vector bytes preserved");
  Need(ctx.r1.u64==before.r1.u64&&ctx.lr==before.lr&&ctx.r2.u64==before.r2.u64&&ctx.r13.u64==before.r13.u64,"reserved/stack/link preservation");
  Need(ctx.cr2.raw()==before.cr2.raw()&&ctx.cr3.raw()==before.cr3.raw()&&ctx.cr4.raw()==before.cr4.raw(),"nonvolatile CR preservation");
  for(unsigned i=0;i<108;++i){if(!TimerKnown(i))Need(m.timer[i]==0xA5,"timer poison intact");if(!ConfigKnown(i))Need(m.config[i]==0xA5,"config poison intact");}
}
Json State(const Memory& m){return Json{{"config",m.config},{"timer",m.timer}};}
unsigned Guards(Memory seed){unsigned total=0;
  for(unsigned i=0;i<9;++i){auto m=seed;
    switch(i){case 0:Put(m.config.data()+72,4,2);break;case 1:SCF(m,80,0);break;case 2:SCF(m,84,0);break;
      case 3:SCF(m,84,std::numeric_limits<float>::quiet_NaN());break;case 4:Put(m.config.data()+92,4,2);break;
      case 5:Put(m.config.data()+96,4,0);break;case 6:SCF(m,88,std::numeric_limits<float>::infinity());break;
      case 7:m.timer[56]=2;break;case 8:m.timer[73]=1;break;}
    const auto before=m;bool rejected=false;try{Eligible(m);}catch(const std::runtime_error&){rejected=true;}
    Need(rejected,"unsupported policy input rejected before original");Equal(m,before);++total;
  }
  return total;
}
} // namespace chain

#undef REX_LOAD_U8
#undef REX_LOAD_U32
#undef REX_LOAD_U64
#undef REX_STORE_U8
#undef REX_STORE_U32
#undef REX_STORE_U64
#undef REX_QUERY_TIMEBASE
#define REX_LOAD_U8(x) (static_cast<void>(base),uint8_t(chain::Load(uint32_t(x),1)))
#define REX_LOAD_U32(x) (static_cast<void>(base),uint32_t(chain::Load(uint32_t(x),4)))
#define REX_LOAD_U64(x) (static_cast<void>(base),chain::Load(uint32_t(x),8))
#define REX_STORE_U8(x,y) (static_cast<void>(base),chain::Store(uint32_t(x),1,uint8_t(y)))
#define REX_STORE_U32(x,y) (static_cast<void>(base),chain::Store(uint32_t(x),4,uint32_t(y)))
#define REX_STORE_U64(x,y) (static_cast<void>(base),chain::Store(uint32_t(x),8,uint64_t(y)))
#define REX_QUERY_TIMEBASE() chain::Timebase()
#include "timer-plain.inc"

int main(int argc,char** argv){using namespace chain;const auto saved=_mm_getcsr();
  try{
    Need(argc==2||(argc==3&&!std::strcmp(argv[2],"--mutate-store")),"usage fixture.json [--mutate-store]");mutate=argc==3;
    const auto input=Read(argv[1]);Keys(input,{"schema","constants","rows"});
    Need(input.at("schema")=="mcla-timer-apply-chain-input-v1","chain fixture schema");
    Need(input.at("constants").is_array()&&input.at("constants").size()==5,"five explicit constants");Memory seed;
    for(unsigned i=0;i<5;++i){const auto& c=input.at("constants")[i];Keys(c,{"address","word"});
      Need(U(c.at("address"))==(i<4?kConstants[i]:kTickConstant),"ordered constant identity");
      const auto bits=uint32_t(U(c.at("word")));if(i<4)seed.constants[i]=bits;else coefficient=bits;
    }
    Need(seed.constants==std::array<uint32_t,4>{0,0x3F800000,0x38D1B717,0x3DCCCCCD},"exact four reviewed Apply constant bits");
    // 32AC3AB1 is independently bound to the existing MCLATR1 fixture header's
    // explicit 82011110 word. It is not a host clock conversion or an FPS claim.
    Need(coefficient==0x32AC3AB1,"exact reviewed fifth constant bits");
    Need(std::isfinite(C(seed,2))&&C(seed,2)>0&&C(seed,2)<=Div(1.0f,60.0f),"positive known lower clamp");
    Need(std::isfinite(std::bit_cast<float>(coefficient))&&std::bit_cast<float>(coefficient)>0,"positive explicit timebase coefficient");
    // Bracket the two reviewed raw-mode ceilings in the scale-one arithmetic
    // domain. These synthetic integer inputs do not establish physical units.
    _mm_setcsr(0x1F80);
    const auto converted=[&](uint64_t delta){return Mul(R(double(int64_t(delta))),std::bit_cast<float>(coefficient));};
    const float ceiling30=Div(C(seed,1),30.0f),ceiling_default=C(seed,3);
    Need(converted(kDeltas[0])>C(seed,2)&&converted(kDeltas[0])<ceiling30,"nominal input below raw30 ceiling");
    Need(converted(kDeltas[1])>ceiling30&&converted(kDeltas[1])<ceiling_default,"dip input between raw30 and default ceilings");
    Need(converted(kDeltas[2])==converted(kDeltas[0]),"recovery repeats nominal input");
    Need(converted(kDeltas[3])>ceiling_default,"large input exceeds default ceiling");
    Need(converted(kDeltas[4])>0&&converted(kDeltas[4])<C(seed,2),"tiny input below lower clamp");
    Need(input.at("rows").is_array()&&input.at("rows").size()==2,"two captured seed rows");
    Json scenarios=Json::array();unsigned negative_count=0;uint64_t comparisons=0,mismatches=0;
    for(unsigned index=0;index<2;++index){const auto& row=input.at("rows")[index];Keys(row,{"id","config_before","config_after","timer_before","timer_after"});
      Need(U(row.at("id"))==(index?6u:3u),"captured row order");auto initial=seed;Config(initial.config,row.at("config_before"));Timer(initial.timer,row.at("timer_before"));
      auto expected=seed;Config(expected.config,row.at("config_after"));Timer(expected.timer,row.at("timer_after"));
      auto baseline=initial;Run(baseline);Equal(baseline,expected); // Actual accepted fixed30 Apply control.
      initial.timer[73]=0;SW(initial,76,0);SF(initial,84,1);for(unsigned off:{56u,58u,72u})initial.timer[off]=0;
      Eligible(initial);negative_count+=Guards(initial);
      for(unsigned policy=0;policy<4;++policy)for(unsigned flags=0;flags<8;++flags)
      for(unsigned secondary=0;secondary<2;++secondary)for(float scale:{0.5f,1.0f,2.0f}){
        auto actual=initial;Put(actual.config.data()+72,4,policy==3?0:1);SCF(actual,84,policy==3?60.0f:30.0f);
        SCF(actual,88,policy==0?30.0f:policy==1?0.0f:-1.0f);SF(actual,84,scale);
        for(unsigned bit=0;bit<3;++bit)actual.timer[std::array<unsigned,3>{56,58,72}[bit]]=uint8_t((flags>>bit)&1);
        // Counter wrap is exercised as unsigned guest state, not host overflow.
        SW(actual,44,UINT32_MAX);SW(actual,48,UINT32_MAX);Eligible(actual);auto oracle=actual;Json stages=Json::array();
        auto compare=[&](const char* name,unsigned frame){const bool equal=actual.config==oracle.config&&actual.timer==oracle.timer;
          ++comparisons;mismatches+=!equal;stages.push_back(Json{{"stage",name},{"frame",frame},{"equal",equal},{"actual",State(actual)},{"expected",State(oracle)}});};
        _mm_setcsr(0x1F80);ApplyOracle(oracle);Run(actual);compare("apply",0);
        for(unsigned frame=0;frame<kDeltas.size();++frame){const auto prior=Big(actual.timer.data()+64,8);
          Need(prior<=UINT64_MAX-kDeltas[frame],"timebase addition bounded");stamp=prior+kDeltas[frame];
          _mm_setcsr(0x1F80);TimerOracle(oracle,stamp,secondary!=0);Execute(actual,0,secondary!=0);compare("timer",frame);
          _mm_setcsr(0x1F80);HelperOracle(oracle,true);Execute(actual,1,false);compare("split_first",frame);
          _mm_setcsr(0x1F80);HelperOracle(oracle,true);Execute(actual,1,false);compare("split_second",frame);
          _mm_setcsr(0x1F80);HelperOracle(oracle,false);Execute(actual,2,false);compare("restore",frame);
          _mm_setcsr(0x1F80);HelperOracle(oracle,false);Execute(actual,2,false);compare("restore_noop",frame);
        }
        scenarios.push_back(Json{{"captured_seed_id",U(row.at("id"))},{"policy_index",policy},{"initial_flags56_58_72_mask",flags},
          {"r5_low_byte",secondary},{"timer84_bits",std::bit_cast<uint32_t>(scale)},{"stages",stages}});
      }
    }
    const auto memory_negatives=MemoryNegatives();int code=mismatches?1:0;
    const char* status=mismatches?(mutate?"CHAIN_MUTATION_DETECTED":"CHAIN_ORACLE_MISMATCH"):"OFFLINE_APPLY_CHAIN_ORACLE_MATCH";
    if(mutate&&!mismatches){code=3;status="CHAIN_MUTATION_NOT_DETECTED";}
    Json report{{"schema","mcla-timer-apply-chain-replay-v1"},{"status",status},{"checks",checks},{"comparisons",comparisons},
      {"mismatching_stages",mismatches},{"mutation",mutate},{"mutated_actual_writes",mutation_count},{"negative_policy_cases",negative_count},
      {"memory_negative_cases",memory_negatives},{"delta_labels",kLabels},{"integer_timebase_deltas",kDeltas},
      {"delta_labels_are_relative_only",true},{"input_constants",input.at("constants")},
      {"policy_order",{"raw_config88_30","raw_config88_zero","raw_config88_negative","fixed60"}},{"scenarios",scenarios},
      {"scope","FINITE_EXPLICIT_FLAG56_58_72_COMBINATIONS_NEGATIVE_F1_TWO_SUBSTEPS"},
      {"initializer_executed",false},{"real_clock_used",false},{"clock_units_proven",false},{"runtime_association_validated",false},
      {"runtime_mxcsr_observed",false},{"object_lifetime_proven",false},{"simulation_proven",false},{"fps_proven",false},{"native_admitted",false}};
    const auto output=report.dump();Need(output.size()<=32*1024*1024,"bounded chain report");_mm_setcsr(saved);std::cout<<output<<'\n';return std::cout?code:2;
  }catch(const std::exception& e){active=nullptr;timer_active=false;_mm_setcsr(saved);std::cerr<<"Apply chain rejected: "<<e.what()<<'\n';return 2;}
  catch(...){active=nullptr;timer_active=false;_mm_setcsr(saved);std::cerr<<"Apply chain rejected: unknown exception\n";return 2;}
}
