// Supplemental actual-core concurrency coverage, no SDK/game/native clocks.
#include "../src/mcla_timer_observer_core.h"
#include <iostream>
#include <latch>
#include <thread>
#include <set>
namespace obs = mcla::timer_observer;
namespace journal = mcla::timer_journal;
struct Context { obs::ScopeBinding binding; unsigned calls{}; };
struct Memory {};
int main() {
  unsigned checks=0, failures=0;
  auto check=[&](bool ok){++checks;if(!ok)++failures;};
  obs::Observer<2> core({100,200,1000},true);
  std::atomic<uint64_t> clock{110};
  std::latch entered(2), release(1);
  const obs::Operations<Context,Memory> operations{
    [&]{return clock.fetch_add(1);},
    [](const Context& c,const Memory&,journal::State& s){s.words[0]=c.calls;return true;},
    [](const Context& c,const Memory&){return c.binding;}};
  Context one{{41,11,1}},two{{42,12,2}};Memory a,b;
  auto run=[&](Context& c,Memory& m){
    core.invoke({0x1000,0x82130004,c.binding.native_thread_id},c,m,operations,0,0,
      [&](Context& original,Memory&){++original.calls;entered.count_down();release.wait();});
  };
  std::thread t1([&]{run(one,a);}),t2([&]{run(two,b);});
  entered.wait();
  auto snapshot=std::make_unique<obs::Observer<2>::Snapshot>();
  check(!core.try_freeze_into(*snapshot));
  release.count_down();t1.join();t2.join();
  check(core.try_freeze_into(*snapshot));
  check(one.calls==1 && two.calls==1 && clock==114);
  check(snapshot->records().size()==2 && snapshot->counts().completed_entries==2 &&
        !snapshot->counts().observer_failed && !snapshot->counts().overflow_entries);
  std::set<uint64_t> calls,ordinals;
  for(const auto& row:snapshot->records()){
    const auto& wanted=row.observation.identity.thread_identity==1?one.binding:two.binding;
    check(row.observation.identity.thread_identity==wanted.native_thread_id &&
          row.entry_binding==wanted && row.exit_binding==wanted && row.callback_issues==0);
    check(row.observation.before.words[0]==0 && row.observation.after.words[0]==1);
    calls.insert(row.entry_binding.phase_call_id);ordinals.insert(row.entry_ordinal);
  }
  check(calls==std::set<uint64_t>{41,42} && ordinals==std::set<uint64_t>{1,2});
  std::cout<<"checks="<<checks<<" failures="<<failures<<'\n';
  return failures?1:0;
}
