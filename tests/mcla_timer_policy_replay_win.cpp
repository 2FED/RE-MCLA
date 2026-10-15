// Offline policy experiment only. Reuses the strict bounded input reader and
// sparse checked memory from the accepted operand replay. No game/real clock.
// Primary supplies exact renamed bodies in timer-plain.inc and policy-setters.inc.
// Input is the same 1398-row captured operand fixture, not live configuration.
// Usage: test.exe rows.bin [--mutate-store]
// Exit0=all baseline/oracle checks match;1=actual mismatch/detected mutation;
// exit2=invalid input/runtime;exit3=requested mutation was not detected.
#define main MclaTimerOperandProgram
#include "mcla_timer_operand_replay_win.cpp"
#undef main
#include <functional>
#include <limits>

REX_EXTERN(FixedSetterPlain);
REX_EXTERN(RawSetterPlain);
#include "policy-setters.inc"

namespace policy {
using namespace replay;
constexpr std::array<uint64_t,6> kDeltas{500,500000,833333,1666667,3000000,10000000};
constexpr std::array<size_t,3> kSeedIndices{0,116,232};
constexpr std::array<const char*,4> kPolicies{"fixed30","fixed60","raw_uncapped","raw_cap30"};
constexpr std::array<const char*,6> kStages{"setter","timer","split_first","split_second","restore_unsplit","post_restore_noop"};
float Round(double x){return static_cast<float>(x);}
uint32_t Word(const Memory& m,unsigned at){return static_cast<uint32_t>(Big(m.object.data()+at,4));}
float Float(const Memory& m,unsigned at){return std::bit_cast<float>(Word(m,at));}
void SetFloat(Memory& m,unsigned at,float v){Put(m.object.data()+at,4,std::bit_cast<uint32_t>(v));}
void SetWord(Memory& m,unsigned at,uint32_t v){Put(m.object.data()+at,4,v);}
float Constant(const Memory& m,unsigned index){return std::bit_cast<float>(m.constants[index]);}
void Add(Memory& m,unsigned at,float value){SetFloat(m,at,Round(double(Float(m,at))+double(value)));}
float Product(float a,float b){return Round(double(a)*double(b));}
float Quotient(float a,float b){return Round(double(a)/double(b));}
float Clamp(float x,float low,float high){
  // Deliberately reproduces finite single-precision subtract/select ordering,
  // without calling any extracted body. NaN/general reset paths are not covered.
  const float lower=Round(double(x)-double(low));const float raised=lower>=0?x:low;
  const float upper=Round(double(raised)-double(high));return upper>=0?high:raised;
}
void Eligible(const Memory& m){
  Need(!m.object[56]&&!m.object[58]&&!m.object[72],"seed outside admitted flag branch");
  for(unsigned at:{8u,12u,16u,20u,24u,28u,32u,36u,40u,80u,84u,88u,92u,96u,100u,104u})
    Need(std::isfinite(Float(m,at)),"finite seed prerequisite");
  Need(Float(m,84)>0&&Float(m,40)>0&&Float(m,36)>=Float(m,40),"positive scale and ordered clamps");
  Need(std::isfinite(Constant(m,0))&&Constant(m,0)>0&&Constant(m,1)==1.0f&&Constant(m,2)==0.0f,"reviewed positive coefficient/one/zero constants");
}
void SetterOracle(Memory& m,unsigned p){
  if(p<=1){const float rate=p==0?30.0f:60.0f;
    SetFloat(m,28,rate);SetWord(m,52,1);SetWord(m,60,1);
    const float step=Quotient(Constant(m,1),Product(1.0f,rate));
    SetFloat(m,32,step);SetFloat(m,8,step);SetFloat(m,88,step);
    SetFloat(m,12,Quotient(Constant(m,1),step));
  }else{
    // Explicit F2=30 bypasses the raw setter's unobserved default-F2 branch.
    SetFloat(m,28,30.0f);SetWord(m,60,0);
    SetFloat(m,32,p==2?Constant(m,2):Quotient(Constant(m,1),30.0f));
  }
}
void TimerOracle(Memory& m,uint64_t stamp){
  const uint64_t prior=Big(m.object.data()+64,8);
  Need(stamp>prior&&stamp-prior<=uint64_t(INT64_MAX),"positive bounded injected difference");
  const float ticks=Round(static_cast<double>(static_cast<int64_t>(stamp-prior)));
  const float raw=Product(ticks,Constant(m,0));const float scale=Float(m,84);
  const float scaled=Product(scale,raw);Put(m.object.data()+64,8,stamp);
  SetFloat(m,92,raw);SetFloat(m,100,raw);SetFloat(m,96,scaled);
  SetFloat(m,88,raw);SetFloat(m,104,scaled);SetFloat(m,8,scaled);
  SetFloat(m,16,Float(m,20));
  const float limit=Float(m,32);
  if(Word(m,60)!=0){
    const float fixed=Product(limit,scale);SetFloat(m,88,limit);SetFloat(m,8,fixed);
    Add(m,20,fixed);Add(m,24,fixed);
  }else{
    // In this admitted branch raw histories advance BEFORE optional cap/clamp.
    Add(m,20,scaled);Add(m,24,scaled);
    if(limit!=Constant(m,2)){
      if(scaled<limit)SetWord(m,52,1);
      else{SetFloat(m,88,limit);SetFloat(m,8,Product(limit,scale));}
    }
  }
  SetWord(m,44,Word(m,44)+1u);SetWord(m,48,Word(m,48)+1u);
  SetFloat(m,8,Clamp(Float(m,8),Float(m,40),Float(m,36)));
  SetFloat(m,88,Clamp(Float(m,88),Float(m,40),Float(m,36)));
  SetFloat(m,12,Quotient(Constant(m,1),Float(m,8)));
}
void HelperOracle(Memory& m,bool split){
  if(split){
    if(m.object[73]){const float saved=Float(m,80);SetFloat(m,8,saved);SetFloat(m,12,Quotient(Constant(m,1),saved));}
    const float unsplit=Float(m,8);SetFloat(m,80,unsplit);m.object[73]=1;SetWord(m,76,2);
    const float half=Quotient(unsplit,2.0f);SetFloat(m,8,half);SetFloat(m,12,Quotient(Constant(m,1),half));
  }else if(m.object[73]){
    const float unsplit=Float(m,80);SetFloat(m,8,unsplit);m.object[73]=0;SetWord(m,76,0);
    SetFloat(m,12,Quotient(Constant(m,1),unsplit));
  }
}
Result Execute(Memory& m,const Row& seed,unsigned p,unsigned stage){
  m.timebase_calls=m.reads=m.writes=m.stack_writes=0;m.stack_initialized=0;m.stack.fill(0x5a);
  PPCContext ctx{};ctx.r3.u64=(seed.r3&0xffffffff00000000ull)|kObject;ctx.r1.u64=kStack+16;
  if(stage==0){ctx.r5.u64=1;ctx.f1.f64=p==0?30.0:p==1?60.0:p==2?0.0:30.0;ctx.f2.f64=30.0;}
  else if(stage==1){ctx.r5.u64=1;ctx.f1.f64=-1.0;}
  else{ctx.r4.u64=(stage==2||stage==3)?1:0;ctx.r5.u64=(stage==2||stage==3)?2:0;}
  _mm_setcsr(0x1f80);ctx.fpscr.csr=0x1f80;active=&m;
  try{if(stage==0){if(p<=1)FixedSetterPlain(ctx,nullptr);else RawSetterPlain(ctx,nullptr);}
    else if(stage==1)TimerPlain(ctx,nullptr);else HelperPlain(ctx,nullptr);
  }catch(...){active=nullptr;throw;}
  active=nullptr;Need(m.timebase_calls==(stage==1?1u:0u),"exact timebase calls per function");
  for(unsigned i=0;i<108;++i)if(!Known(i))Need(m.object[i]==0xa5,"policy gap poison unchanged");
  return {Pack(m),_mm_getcsr(),m.reads,m.writes,m.stack_writes,m.timebase_calls,ctx.f1.u64};
}
unsigned BoundaryTests(){Memory m;active=&m;unsigned n=0;
  auto reject=[&](const std::function<void()>& f){bool threw=false;try{f();}catch(const std::runtime_error&){threw=true;}Need(threw,"boundary negative did not reject");++n;};
  try{
    reject([]{static_cast<void>(Load(kObject,1));});
    reject([]{static_cast<void>(Load(kObject+56,4));});
    reject([]{Store(kConstants[0],4,0);});
    reject([]{static_cast<void>(Load(kStack,8));});
    reject([]{static_cast<void>(Load(kObject+107,4));});
    m.timebase=1;m.timebase_calls=0;static_cast<void>(Timebase());reject([]{static_cast<void>(Timebase());});
    m.timebase=0;m.timebase_calls=0;reject([]{static_cast<void>(Timebase());});
  }catch(...){active=nullptr;throw;}active=nullptr;return n;
}
}

int main(int argc,char** argv){using namespace policy;Restore restore;
  try{
    Need(argc==2||(argc==3&&!std::strcmp(argv[2],"--mutate-store")),"usage rows.bin [--mutate-store]");
    const bool mutation=argc==3;const auto input=Read(argv[1]);
    Need(input.rows.size()==1398,"exact all-captured baseline1398 required");
    std::vector<size_t> timer_rows;uint64_t clean=0,helpers=0;
    for(size_t i=0;i<input.rows.size();++i){const auto& r=input.rows[i];
      clean+=Run(r,input.constants,0x1f80,0,0).state==r.after;
      if(r.kind==1)timer_rows.push_back(i);else ++helpers;
    }
    Need(timer_rows.size()==233&&helpers==1165,"full233timer1165helper cohort");
    Need(clean==input.rows.size(),"clean captured nearest baseline mismatch; no policy scenarios");
    const auto boundary_checks=BoundaryTests();
    Json report{{"schema","mcla-timer-policy-replay-v1"},{"captured_baseline_rows",input.rows.size()},
      {"clean_nearest_matching_rows",clean},{"boundary_negative_checks",boundary_checks},
      {"seed_timer_indices",kSeedIndices},{"deltas",kDeltas},{"policy_order",kPolicies},{"stage_order",kStages},
      {"entry_mxcsr",0x1f80},{"runtime_mxcsr_observed",false},{"runtime_configuration_reconstructed",false},
      {"caller822D2478_executed",false},{"clock_units_proven",false},{"simulation_proven",false},{"fps_proven",false},
      {"timebase_source","SYNTHETIC_BEFORE64_PLUS_DECLARED_INTEGER_DELTA"},
      {"oracle_scope","FINITE_FLAG56_58_72_ZERO_EXPLICIT_RAW_F2_30_BRANCH_ONLY"},
      {"raw_default_f2_zero_branch_tested",false},{"mutation",mutation},{"scenarios",Json::array()}};
    uint64_t mismatches=0,comparisons=0,first_mismatch=UINT64_MAX;
    for(size_t selected:kSeedIndices){const size_t index=timer_rows[selected];const auto& seed=input.rows[index];
      for(unsigned p=0;p<4;++p)for(const uint64_t delta:kDeltas){
        Memory actual,expected;Unpack(actual,seed.before);actual.constants=input.constants;Eligible(actual);expected=actual;
        const uint64_t prior=Big(actual.object.data()+64,8);Need(prior<=UINT64_MAX-delta,"synthetic timebase addition overflow");
        actual.timebase=prior+delta;Need(uint32_t(actual.timebase)!=0,"synthetic timebase low32 must be nonzero");
        Json stages=Json::array();
        for(unsigned stage=0;stage<6;++stage){
          _mm_setcsr(0x1f80);
          if(stage==0)SetterOracle(expected,p);else if(stage==1)TimerOracle(expected,actual.timebase);
          else HelperOracle(expected,stage==2||stage==3);
          auto result=Execute(actual,seed,p,stage);
          if(mutation&&stage==1){actual.object[11]^=1;result.state=Pack(actual);}
          const auto oracle=Pack(expected);uint64_t bad=0;for(size_t b=0;b<96;++b)bad+=oracle[b]!=result.state[b];
          if(bad&&first_mismatch==UINT64_MAX)first_mismatch=comparisons;
          mismatches+=bad!=0;++comparisons;
          stages.push_back(Json{{"stage",kStages[stage]},{"result",Output(result)},
            {"oracle_known_state",oracle},{"matching_known_bytes",96-bad}});
        }
        report["scenarios"].push_back(Json{{"seed_timer_index",selected},{"input_row_index",index},
          {"policy",kPolicies[p]},{"delta",delta},{"stages",stages}});
      }
    }
    report["stage_comparisons"]=comparisons;report["mismatching_stages"]=mismatches;
    report["first_mismatch_stage_ordinal"]=first_mismatch==UINT64_MAX?Json(nullptr):Json(first_mismatch);
    int code=0;if(mismatches){code=1;report["status"]=mutation?"MUTATION_DETECTED":"POLICY_ORACLE_MISMATCH";}
    else if(mutation){code=3;report["status"]="MUTATION_NOT_DETECTED";}
    else report["status"]="OFFLINE_POLICY_ORACLE_MATCH_NO_RUNTIME_ADMISSION";
    const auto output=report.dump();Need(output.size()<=32*1024*1024,"bounded policy JSON");
    std::cout<<output<<'\n';return std::cout?code:2;
  }catch(const std::exception& e){std::cerr<<"timer policy rejected: "<<e.what()<<'\n';return 2;}
  catch(...){std::cerr<<"timer policy rejected: unknown exception\n";return 2;}
}
