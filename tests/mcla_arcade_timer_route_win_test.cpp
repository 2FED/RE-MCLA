// Actual strong wrapper + actual Arcade module, include-source synthetic TU.
// Compile with MCLA_UPDATE_TIMING_PROBE and MCLA_ARCADE_TIMER_PROBE.
// Do NOT also link either included .cpp as a separate object in this test.
// This is not the later production separate-TU/link-map identity gate.
// No game, GPU, window, file, input injection or dispatcher replacement.
#include "../src/mcla_update_timing_win.cpp"
#include "../src/mcla_arcade_timer_win.cpp"
#include <nlohmann/json.hpp>
#include <array>
#include <cstring>
#include <iostream>
#include <string_view>

namespace {
namespace old = mcla::update_timing;
namespace od = old::detail;
namespace arcade = mcla::arcade_timer;
namespace ad = arcade::detail;
using Json = nlohmann::json;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
unsigned checks{}, failures{}, original_calls{}, other_calls{};
DWORD original_error{};
unsigned original_mxcsr{};
bool throw_original{};
uint64_t legacy_tick=100;
unsigned legacy_reads{};

void Check(bool value,const char* label) {
  ++checks;if(!value){++failures;std::cerr<<"FAIL "<<label<<'\n';}
}
void Original(PPCContext& ctx,uint8_t* base) {
  ++original_calls;original_error=GetLastError();original_mxcsr=_mm_getcsr();
  base[ctx.r3.u32+8]^=0x5A;
  base[7]^=0x39;
  ctx.r3.u64=0xFEEDFACE12345678ull;ctx.r31.u64^=0x8877665544332211ull;
  ctx.r5.u64^=0xDEADBEEFull;ctx.f1.u64^=0x1122334455667788ull;
  ctx.f28.u64^=0x8000000000000000ull;ctx.lr=0x82221100;
  _mm_setcsr(0x3F80);SetLastError(0xABCD1234);
  if(throw_original)throw std::runtime_error("route original exception");
}
PPCContext Context() {
  PPCContext ctx{};ctx.r3.u32=128;ctx.r5.u64=0xAABBCCDD;ctx.r31.u64=0x12456789;
  ctx.f1.u64=0x0123456789ABCDEFull;ctx.f28.u64=0x8000000000000000ull;
  ctx.lr=0x822C21B4;return ctx;
}
void Case(bool throws,unsigned incoming_mxcsr) {
  std::array<uint8_t,512> direct_memory{},routed_memory{};
  for(size_t i=0;i<direct_memory.size();++i)direct_memory[i]=uint8_t(i);
  routed_memory=direct_memory;
  auto direct=Context();PPCContext routed{};
  std::memcpy(&routed,&direct,sizeof direct);
  throw_original=throws;const auto before=original_calls;
  auto run=[&](PPCContext& ctx,uint8_t* base,bool route) {
    _mm_setcsr(incoming_mxcsr);SetLastError(0x24681357);bool caught=false;
    try {
      if(route)sub_821BDA90(ctx,base);else __imp__sub_821BDA90(ctx,base);
    }catch(const std::runtime_error& error){caught=std::string_view(error.what())=="route original exception";}
    const auto produced_error=GetLastError();const auto produced_mxcsr=_mm_getcsr();
    Check(caught==throws,"direct and strong route propagate same exception");
    Check(original_error==0x24681357 && original_mxcsr==incoming_mxcsr,
          "actual original sees incoming LastError and MXCSR");
    Check(produced_error==0xABCD1234 && produced_mxcsr==0x3F80,
          "actual strong route preserves original-produced LastError and MXCSR");
  };
  run(direct,direct_memory.data(),false);run(routed,routed_memory.data(),true);
  Check(original_calls==before+2,"one original each direct/strong route");
  Check(std::memcmp(&direct,&routed,sizeof direct)==0,"actual strong route full PPC context differential");
  Check(direct_memory==routed_memory,"actual strong route full memory differential");
  Check(other_calls==0,"timer route never invokes unrelated generated implementations");
}
void SetArcade(bool enabled) {
  Check(rex::cvar::SetFlagByName("mcla_arcade_timer_probe",enabled?"true":"false"),
        "test-only pre-init CVar selection");
  Check(arcade::Enabled()==enabled,"actual Enabled matches selected route");
}
}

namespace mcla::phase_census {
bool Enabled(){return true;}
ActiveUpdate ReadActiveUpdate(const PPCContext&,const uint8_t* base) noexcept {
  return base?ActiveUpdate{17,3,GetCurrentThreadId()}:ActiveUpdate{};
}
}
namespace mcla::guest_source_scout {bool Enabled(){return true;}}
REX_EXTERN(__imp__sub_821BDA90){Original(ctx,base);}
REX_EXTERN(__imp__sub_821BD910){++other_calls;ctx.r0.u64=1;}
REX_EXTERN(__imp__sub_822C52C8){++other_calls;ctx.r0.u64=2;}

int main() {
  const auto initial=ad::HostState::Capture();
  Check(!arcade::Enabled(),"Arcade flag defaults OFF");
  Check(!old::Enabled(),"legacy flag defaults OFF");
  auto legacy=std::make_unique<od::State>();
  legacy->data.begin=100;legacy->data.end=100000;legacy->data.frequency=10000;legacy->data.token=token;
  legacy->clock=[]{SetLastError(0x99887766);return legacy_tick++;};
  legacy->reader=[](uintptr_t,uint32_t,mcla::step_timer_memory::State& state){
    ++legacy_reads;state={};return true;};
  // Local synthetic observer publication only. Production Install still needs
  // its real dispatcher/phase/scout checks; this does not claim those checks.
  od::configured.store(legacy.get());

  Case(false,0x1F80);Case(true,0x5F80);
  Check(legacy->data.counts.observed==2 && legacy->data.counts.completed==2,
        "OFF reaches actual legacy observer exactly once per routed call");
  Check(legacy->data.counts.exceptions==1,"OFF legacy unwind recorded");

  SetArcade(true);
  ad::InstallState(*ad::configured,true,sub_821BDA90); // Exact-target seam, no Runtime instance.
  Case(false,0x5F80);Case(true,0x1F80);
  Check(legacy->data.counts.observed==2,"ON pre-Begin bypasses legacy observer");
  Check(ad::configured->observer.load()==nullptr,"ON pre-Begin creates no implicit observer");

  LARGE_INTEGER frequency{};
  Check(QueryPerformanceFrequency(&frequency)!=FALSE && frequency.QuadPart>0,"native QPF available");
  const auto begin=ad::Qpc(),qpf=uint64_t(frequency.QuadPart);
  arcade::Begin(begin,begin+10*qpf,qpf,token);
  const auto window=arcade::ObservationWindow(token);
  Check(window.begin_qpc==begin && window.end_qpc==begin+10*qpf && window.qpc_frequency==qpf,
        "actual public Begin exact native interval");
  Case(false,0x1F80);Case(true,0x5F80);
  Check(legacy->data.counts.observed==2,"ON observed strong route never nests legacy observer");

  // Pre-init synthetic selection is deliberately varied to test both compiled
  // branches; production init-only policy is not bypassed in a running game.
  SetArcade(false);
  Case(false,0x5F80);Case(true,0x1F80);
  Check(legacy->data.counts.observed==4 && legacy->data.counts.completed==4,
        "OFF with allocated sidecar still routes solely to legacy");
  SetArcade(true);arcade::Disarm();
  Case(false,0x1F80);Case(true,0x5F80);
  Check(legacy->data.counts.observed==4,"ON disarmed route does not fall back to legacy");

  auto snapshot=std::make_unique<ad::Observer::Snapshot>();
  Check(ad::configured->observer.load()->try_freeze_into(*snapshot),"actual sidecar drains after synthetic calls");
  Check(snapshot->records().size()==2 && snapshot->counts().observed_entries==2 &&
        snapshot->counts().completed_entries==2,"only two ON begun calls observed");
  if(snapshot->records().size()==2) {
    for(size_t i=0;i<2;++i) {
      const auto& row=snapshot->records()[i];
      Check(row.observation.identity.timer_address==128 && row.observation.identity.caller_address==0x822C21B4,
            "strong route records incoming identity");
      Check(row.observation.before_available && row.observation.after_available,
            "actual self-process allowlist reads retained");
      Check((row.observation.before.words[0]^row.observation.after.words[0])==0x5A000000,
            "strong route reads initial timer address after changed r3");
      Check(bool(row.callback_issues&mcla::timer_observer::kOriginalUnwound)==(i==1),
            "sidecar normal/unwind distinction preserved");
    }
  }
  const auto json=Json::parse(ad::Serialize(*snapshot,token));
  Check(json["schema"]=="mcla-arcade-timer-observer-v1" && json["native_admitted"]==false,
        "route test is not native game admission");
  Check(legacy_reads==0,"small synthetic r3 never mistaken for legacy fixed timer address");
  Check(original_calls==20 && other_calls==0,"all five stage pairs original exactly once");
  od::configured.store(nullptr); // Synthetic pointer detached only after all work completes.
  initial.Restore();
  std::cout<<"checks="<<checks<<" failures="<<failures<<'\n';return failures?1:0;
}
