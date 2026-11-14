// Stdlib-only actual Observer tests. No SDK phase headers, game, native clock,
// OS input, real threads or files. Primary owns RED/GREEN compilation/execution.
#include "../src/mcla_timer_observer_core.h"

#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
namespace observer = mcla::timer_observer;
namespace journal = mcla::timer_journal;
using Binding = observer::ScopeBinding;
struct Context { Binding current{}; unsigned effects{}; };
struct Memory { unsigned effects{}; };
using Ops = observer::Operations<Context,Memory>;
constexpr journal::Window window{100,200,1000};
constexpr journal::Identity identity{0x1000,0x82130004,7};
constexpr Binding stable{41,12,7};
unsigned checks{},failures{};
void Check(bool value,const char* message) {
  ++checks;
  if(!value){++failures;std::cerr<<"FAIL "<<message<<'\n';}
}
template<size_t N> auto Freeze(observer::Observer<N>& core) {
  auto value=std::make_unique<typename observer::Observer<N>::Snapshot>();
  if(!core.try_freeze_into(*value)) value.reset();
  Check(bool(value),"completed observer drains");
  return value;
}
Ops Basic(uint64_t& tick) {
  return {[&]{return tick++;},[](const Context& c,const Memory& m,journal::State& state){
    state.words[0]=c.effects;state.words[1]=m.effects;return true;
  }};
}

void DefaultAndOrder() {
  Check(Binding{}==Binding{0,0,0},"default binding completely unknown");
  Check(!(stable==Binding{42,12,7}) && !(stable==Binding{41,13,7}) &&
        !(stable==Binding{41,12,8}),"equality checks all three fields");
  Check(observer::kEntryBindingCallbackFailed==32 && observer::kExitBindingCallbackFailed==64 &&
        observer::kOriginalUnwound==16,"new issue bits do not change legacy bits");
  for(bool enabled:{false,true}) {
    observer::Observer<2> core(window,true);Context context{stable};Memory memory;
    std::vector<std::string_view> events;unsigned clocks{},states{},bindings{},originals{};
    Ops ops{[&]{events.push_back(clocks++ ? "exit-clock":"entry-clock");return uint64_t(110+clocks);},
      [&](const Context&,const Memory&,journal::State& state){
        events.push_back(states++ ? "after-state":"before-state");state.words[0]=states;return true;
      }};
    Check(!ops.read_binding,"legacy two-field Operations leaves binding callback empty");
    if(enabled) ops.read_binding=[&](const Context& c,const Memory& m){
      Check(&c==&context && &m==&memory,"binding sees exact original reference objects");
      events.push_back(bindings++ ? "exit-binding":"entry-binding");return c.current;
    };
    Context& returned=core.invoke(identity,context,memory,ops,1,0xBFF0000000000000ull,
      [&](Context& c,Memory& m)->Context&{++originals;++c.effects;++m.effects;events.push_back("original");return c;});
    auto frozen=Freeze(core);if(!frozen)continue;
    Check(originals==1 && &returned==&context && context.effects==1 && memory.effects==1,
          "original once with same effects and return reference");
    const std::vector<std::string_view> expected=enabled ?
      std::vector<std::string_view>{"entry-clock","entry-binding","before-state","original","after-state","exit-binding","exit-clock"} :
      std::vector<std::string_view>{"entry-clock","before-state","original","after-state","exit-clock"};
    Check(events==expected,"optional binding reads lie within original outer clock bracket");
    const auto& row=frozen->records()[0];
    Check(row.entry_binding==(enabled?stable:Binding{}) && row.exit_binding==row.entry_binding &&
          row.callback_issues==0 && row.entry_ordinal==1,"same raw ordinal owns both bindings");
    Check(row.observation.entry_qpc==111 && row.observation.exit_qpc==112 &&
          row.observation.before.words[0]==1 && row.observation.after.words[0]==2,
          "legacy observation fields unchanged by optional metadata");
    Check(frozen->counts().observed_entries==1 && frozen->counts().stored_entries==1 &&
          frozen->classify_offline()->raw_callback_issues==0,"healthy metadata no fabricated diagnostic");
  }
}

void RawValuesAndUnwind() {
  for(const Binding value:{Binding{},Binding{1,0,0},Binding{0,2,0},Binding{0,0,3},stable}) {
    observer::Observer<1> core(window,true);uint64_t tick=110;auto ops=Basic(tick);
    Context context{value};Memory memory;
    ops.read_binding=[](const Context& c,const Memory&){return c.current;};
    core.invoke(identity,context,memory,ops,0,0,[](Context&,Memory&){});
    auto frozen=Freeze(core);if(!frozen)continue;
    Check(frozen->records()[0].entry_binding==value && frozen->records()[0].exit_binding==value &&
          frozen->records()[0].callback_issues==0,"unknown and partial bindings preserved without normalization");
  }
  for(bool unwind:{false,true}) {
    observer::Observer<1> core(window,true);uint64_t tick=110;auto ops=Basic(tick);
    Context context{stable};Memory memory;unsigned originals{};bool caught=false;
    const Binding changed{99,17,8};
    ops.read_binding=[](const Context& c,const Memory&){return c.current;};
    try {
      core.invoke(identity,context,memory,ops,0,0,[&](Context& c,Memory& m){
        ++originals;++c.effects;++m.effects;c.current=changed;
        if(unwind)throw std::runtime_error("original exact error");
      });
    }catch(const std::runtime_error& e){caught=std::string_view(e.what())=="original exact error";}
    auto frozen=Freeze(core);if(!frozen)continue;const auto& row=frozen->records()[0];
    Check(originals==1 && caught==unwind && context.effects==1 && memory.effects==1,
          "original mismatch/unwind once and exception preserved");
    Check(row.entry_binding==stable && row.exit_binding==changed && row.entry_ordinal==1,
          "core retains differing endpoint bindings even on unwind");
    Check(row.callback_issues==(unwind?observer::kOriginalUnwound:0),
          "binding mismatch is not classified or repaired by core");
  }
}

void BindingFailures() {
  for(unsigned mask:{1u,2u,3u})for(bool unwind:{false,true}) {
    observer::Observer<1> core(window,true);uint64_t tick=110;auto ops=Basic(tick);
    Context context{stable};Memory memory;unsigned reads{},originals{};bool caught=false;
    ops.read_binding=[&](const Context& c,const Memory&)->Binding {
      const unsigned bit=1u<<reads++;Binding temporary=c.current;temporary.phase_call_id=999;
      if(mask&bit)throw std::runtime_error("binding partial temporary");
      return c.current;
    };
    try{core.invoke(identity,context,memory,ops,0,0,[&](Context&,Memory&){
      ++originals;if(unwind)throw std::runtime_error("original survives");
    });}catch(const std::runtime_error& e){caught=std::string_view(e.what())=="original survives";}
    auto frozen=Freeze(core);if(!frozen)continue;const auto& row=frozen->records()[0];
    const uint32_t issues=((mask&1)?32u:0u)|((mask&2)?64u:0u)|(unwind?16u:0u);
    Check(originals==1 && reads==2 && caught==unwind,"binding callback errors never skip or replace original");
    Check(row.entry_binding==((mask&1)?Binding{}:stable) && row.exit_binding==((mask&2)?Binding{}:stable),
          "throwing return-value callbacks retain zero, not partial or prior binding");
    Check(row.callback_issues==issues && frozen->classify_offline()->raw_callback_issues==issues,
          "binding issue bits retained and aggregated offline");
    Check(frozen->classify_offline()->issues()&observer::kRecordedInvocationDiagnostic,
          "binding failure cannot yield healthy classification");
  }
}

void NestedScopeAndDrain() {
  observer::Observer<3> core(window,true);uint64_t tick=110;auto ops=Basic(tick);
  Context outer{stable},inner{Binding{42,12,7}};Memory outerMemory,innerMemory;
  unsigned reads{},originals{};const Binding innerValue=inner.current;
  ops.read_binding=[&](const Context& c,const Memory&){++reads;return c.current;};
  core.invoke(identity,outer,outerMemory,ops,0,0,[&](Context&,Memory&){
    ++originals;
    core.invoke(identity,inner,innerMemory,ops,0,0,[&](Context&,Memory&){++originals;});
  });
  auto frozen=Freeze(core);if(!frozen)return;
  Check(originals==2 && reads==4 && frozen->records().size()==2,"nested calls keep their own four binding observations");
  const auto& first=frozen->records()[0];const auto& second=frozen->records()[1];
  Check(first.entry_ordinal==2 && first.entry_binding==innerValue && first.exit_binding==innerValue &&
        second.entry_ordinal==1 && second.entry_binding==stable && second.exit_binding==stable,
        "nested completion order does not join or overwrite scope metadata");
  observer::Observer<1> draining(window,true);tick=130;auto pending=std::make_unique<observer::Observer<1>::Snapshot>();
  draining.invoke(identity,outer,outerMemory,ops,0,0,[&](Context&,Memory&){
    Check(!draining.try_freeze_into(*pending) && pending->records().empty(),"in-flight drain remains pending without partial binding record");
  });
  Check(draining.try_freeze_into(*pending) && pending->records()[0].entry_binding==stable &&
        pending->records()[0].exit_binding==stable && pending->counts().completed_entries==1,
        "stop during original still retains exit binding and completes drain");
}

void CapacityAndWindow() {
  observer::Observer<1> core(window,true);uint64_t tick=110;auto ops=Basic(tick);
  Context context{stable};Memory memory;unsigned bindings{},originals{};
  ops.read_binding=[&](const Context& c,const Memory&){++bindings;return c.current;};
  auto original=[&](Context&,Memory&){++originals;};
  core.invoke(identity,context,memory,ops,0,0,original);core.invoke(identity,context,memory,ops,0,0,original);
  auto frozen=Freeze(core);if(!frozen)return;
  Check(originals==2 && bindings==2 && tick==112 && frozen->counts().overflow_entries==1,
        "capacity overflow calls original without clocks states or bindings");
  core.invoke(identity,context,memory,ops,0,0,original);
  Check(originals==3 && bindings==2,"after freeze no binding callbacks");
  observer::Observer<1> disabled(window);disabled.invoke(identity,context,memory,ops,0,0,original);
  Check(originals==4 && bindings==2,"default-disabled observer never reads bindings");
  for(unsigned scenario=0;scenario<4;++scenario){
    observer::Observer<1> scoped(window,true);unsigned clocks{},reads{};
    auto current=Basic(tick);
    current.read_qpc=[&]{++clocks;return scenario==0?(clocks==1?uint64_t(90):uint64_t(210)):uint64_t(10+clocks);};
    current.read_binding=[&](const Context&,const Memory&)->Binding{++reads;if(scenario==2)throw std::runtime_error("outside binding");return stable;};
    scoped.invoke(scenario==1?journal::Identity{}:identity,context,memory,current,0,0,original);
    auto result=Freeze(scoped);if(!result)continue;
    if(scenario==3){
      Check(result->records().empty() && result->counts().outside_before_entries==1 && reads==2,
            "healthy outside path preserves legacy discard counting");
    }else{
      Check(result->records().size()==1 && result->records()[0].entry_ordinal==1 && reads==2,
            "straddle unknown identity and outside callback errors remain retained");
      if(!result->records().empty()) Check(result->records()[0].entry_binding==(scenario==2?Binding{}:stable) &&
        result->records()[0].exit_binding==(scenario==2?Binding{}:stable),"retained exceptional row owns both exact bindings");
      if(scenario==2)Check(result->records()[0].callback_issues==96 && result->counts().outside_before_entries==0,
                          "binding exceptions prevent healthy-outside discard");
    }
  }
}
void OutsideBindingRetention() {
  const Binding cases[][2]={{Binding{},Binding{}},{stable,stable},
    {Binding{1,0,0},Binding{1,0,0}},{stable,Binding{42,12,7}},
    {Binding{},stable},{Binding{0,1,7},Binding{0,1,7}}};
  for(unsigned index=0;index<6;++index){
    observer::Observer<1> core(window,true);uint64_t tick=10;auto ops=Basic(tick);
    Context context;Memory memory;unsigned reads{},originals{};
    ops.read_binding=[&](const Context&,const Memory&){return cases[index][reads++];};
    core.invoke(identity,context,memory,ops,0,0,[&](Context&,Memory&){++originals;});
    auto frozen=Freeze(core);if(!frozen)continue;
    Check(originals==1 && reads==2 && frozen->counts().completed_entries==1,
          "outside binding metadata does not change original or drain");
    if(index<2)Check(frozen->records().empty() && frozen->counts().outside_before_entries==1,
                     "canonical equal positive or unknown tuples permit outside discard");
    else{
      Check(frozen->records().size()==1 && frozen->counts().outside_before_entries==0,
            "partial or mismatched successful outside bindings are retained");
      if(!frozen->records().empty())Check(frozen->records()[0].entry_binding==cases[index][0] &&
        frozen->records()[0].exit_binding==cases[index][1] && frozen->records()[0].callback_issues==0,
        "outside malformed raw metadata remains exact without fabricated issue bit");
    }
  }
}
}
int main(int argc,char**){
  if(argc!=1){std::cerr<<"No arguments\n";std::cerr.flush();return 2;}
  try{DefaultAndOrder();RawValuesAndUnwind();BindingFailures();NestedScopeAndDrain();CapacityAndWindow();OutsideBindingRetention();}
  catch(const std::exception& e){++failures;std::cerr<<"FAIL unexpected "<<e.what()<<'\n';}
  std::cout<<"checks="<<checks<<" failures="<<failures<<" synthetic_scope_binding_only\n";
  std::cout.flush();std::cerr.flush();if(!std::cout||!std::cerr)return 2;
  return failures?1:0;
}
