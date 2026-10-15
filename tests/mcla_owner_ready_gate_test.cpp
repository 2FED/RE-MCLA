#include "mcla_owner_ready_gate_win.h"
#include <iostream>
#include <thread>
#include <vector>
namespace gate = mcla::owner_ready;
unsigned checks{}, failed{};
void Check(bool ok, const char* label) { ++checks; if (!ok) { ++failed; std::cerr << label << '\n'; } }
template<class F> void Reject(F f, const char* reason) {
  bool rejected = false;
  try { f(); } catch (const std::runtime_error& e) { rejected = std::string_view(e.what()) == reason; }
  Check(rejected, reason);
}
struct Fake {
  uint64_t clock=1000, ready_at=71000;
  unsigned announcements{}, polls{}, sleeps{};
  bool announced{}, fail_announce{}, fail_poll{}, prewaiting{};
  std::stop_source stop;
  gate::Window window{1000,181000,1000};
  gate::Operations Ops() {
    return {[&] { return clock; }, [&] { ++polls; return fail_poll ? gate::Poll::kFailed :
      clock>=ready_at ? gate::Poll::kSignaled : gate::Poll::kEmpty; },
      [&] { return prewaiting ? gate::Poll::kSignaled : gate::Poll::kEmpty; },
      [&] { ++announcements; announced=true; return !fail_announce; },
      [&] { ++sleeps; clock+=1000; }};
  }
  gate::Evidence Run() { return gate::Wait(window,stop.get_token(),Ops()); }
};
int main() {
  Fake f; const auto e=f.Run();
  Check(e.ready_after==71000 && e.ready_before==71000 && f.announcements==1 && f.sleeps==70,
        "no admission from early presentation: explicit readiness required");
  Check(e.deadline==121000 && e.window.end==181000, "unchanged journal horizon");
  gate::ValidateSource(e,71000); gate::ValidateCut(e,120100);
  Reject([&] { gate::ValidateSource(e,10000); },"OWNER_READY_SOURCE_OUTSIDE_BUDGET");
  Reject([&] { gate::ValidateSource(e,121000); },"OWNER_READY_SOURCE_OUTSIDE_BUDGET");
  Reject([&] { gate::ValidateCut(e,181000); },"OWNER_READY_CUT_OUTSIDE_JOURNAL");
  for (auto w:std::vector<gate::Window>{{0,180000,1000},{1000,181001,1000},{1,2,0},{1,2,UINT64_MAX},{UINT64_MAX,1,1000}})
    Reject([&] { gate::Deadline(w); },"OWNER_READY_INVALID_JOURNAL_WINDOW");
  Fake timeout; timeout.ready_at=121000;
  Reject([&] { timeout.Run(); },"OWNER_READY_CLOCK_OR_DEADLINE");
  Fake cancelled; cancelled.stop.request_stop();
  Reject([&] { cancelled.Run(); },"OWNER_READY_CANCELLED");
  Check(cancelled.polls==0 && cancelled.announcements==0,"cancel does not announce");
  Fake pre; pre.ready_at=1; Reject([&] { pre.Run(); },"OWNER_READY_OBJECT_NOT_FRESH");
  Fake stale; stale.prewaiting=true; Reject([&] { stale.Run(); },"OWNER_READY_OBJECT_NOT_FRESH");
  Fake bad; bad.fail_announce=true; Reject([&] { bad.Run(); },"OWNER_READY_ANNOUNCE_FAILED");
  Fake late; auto op=late.Ops(); op.pause=[&] { late.clock=999; };
  Reject([&] { gate::Wait(late.window,late.stop.get_token(),op); },"OWNER_READY_CLOCK_OR_DEADLINE");
  Fake waitfail; auto broken=waitfail.Ops(); broken.pause=[&] { waitfail.fail_poll=true; };
  Reject([&] { gate::Wait(waitfail.window,waitfail.stop.get_token(),broken); },"OWNER_READY_POLL_FAILED");
  Fake stopafter; auto stopping=stopafter.Ops(); stopping.pause=[&] { stopafter.stop.request_stop(); };
  Reject([&] { gate::Wait(stopafter.window,stopafter.stop.get_token(),stopping); },"OWNER_READY_CANCELLED");
  Reject([] { gate::EventName("../wrong",false); },"OWNER_READY_INVALID_TOKEN");
  // Actual kernel-event rendezvous, no game, UI, input, saves or capture.
  const auto token=std::string(24,'a')+[] { char b[9]; snprintf(b,sizeof(b),"%08lx",GetCurrentProcessId()); return std::string(b); }();
  LARGE_INTEGER fq{}; QueryPerformanceFrequency(&fq);
  gate::Window w{gate::Clock(),0,uint64_t(fq.QuadPart)}; w.end=w.begin+180*w.frequency;
  Reject([&] { gate::WaitNative(token,w,{}); },"OWNER_READY_OPEN_FAILED");
  auto create=[&](bool waiting) {
    HANDLE h=CreateEventW(nullptr,TRUE,FALSE,gate::EventName(token,waiting).c_str());
    if(!h || GetLastError()==ERROR_ALREADY_EXISTS) throw std::runtime_error("fresh fixture event required");
    return h;
  };
  gate::EventHandle ready(create(false)), waiting(create(true));
  std::jthread sender([&] {
    if(WaitForSingleObject(waiting.value,3000)==WAIT_OBJECT_0) SetEvent(ready.value);
  });
  std::stop_source cancellation;
  std::jthread watchdog([&](std::stop_token stop) {
    for(unsigned i=0;i<200 && !stop.stop_requested();++i) std::this_thread::sleep_for(std::chrono::milliseconds(25));
    if(!stop.stop_requested()) cancellation.request_stop();
  });
  const auto native=gate::WaitNative(token,w,cancellation.get_token());
  watchdog.request_stop();
  Check(native.opened<=native.waiting_before && native.waiting_before<=native.waiting_after &&
        native.waiting_after<=native.ready_before && native.ready_before<=native.ready_after,
        "native ordered real clock brackets");
  Check(native.ready_after<native.deadline,"native bounded admission");
  Reject([&] { gate::WaitNative(token,w,{}); },"OWNER_READY_OBJECT_NOT_FRESH");
  std::cout << "owner-ready checks=" << checks << " failed=" << failed << '\n';
  return failed ? 1 : 0;
}
