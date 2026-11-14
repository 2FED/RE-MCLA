// Offline exact TimerPlain/HelperPlain replay of every supplied captured row.
// Primary supplies pinned private apply-plain.inc/policy-setters.inc/timer-plain.inc.
// Include the base once, NOT the chain test (which already renames this main).
// Usage: test.exe fixture.json [--mutate-history|--mutate-counter]
// Exit 0=all captured bytes match; 1=mismatch/mutation detected;
// 2=input/execution/output failure; 3=requested mutation was not detected.
// No game, real clock, Runtime, guest memory pointers, or physical-speed claim.
#define main RetainedApplyReplayMain
#include "mcla_timer_apply_replay_win.cpp"
#undef main
#include <limits>

REX_EXTERN(TimerPlain);
REX_EXTERN(HelperPlain);
namespace captured_state {
using namespace apply_replay;
constexpr uint32_t kTickConstant = 0x82011110;
constexpr size_t kMaximumBytes = 8 * 1024 * 1024, kMaximumRows = 4096;
constexpr unsigned kCapturedBytes = 96, kPoisonBytes = 12;
uint32_t coefficient{};
uint64_t timebase{};
unsigned timebase_calls{}, mutated_writes{}, mutation_offset{};
bool timer_active{};
struct RestoreMxcsr {
  unsigned saved = _mm_getcsr();
  ~RestoreMxcsr() noexcept { _mm_setcsr(saved); }
};

void RejectConfigurationAccess(uint32_t at, unsigned n) {
  Need(n && n <= 8, "bounded access width");
  Need(!(uint64_t(at) < uint64_t(kConfig) + 108 && uint64_t(at) + n > kConfig),
       "configuration was not captured: all configuration access forbidden");
}
uint64_t Load(uint32_t at, unsigned n) {
  RejectConfigurationAccess(at, n);
  if (at == kTickConstant) {
    Need(active && n == 4, "explicit fifth constant width");
    ++active->reads; return coefficient;
  }
  return apply_replay::Load(at, n);
}
void Store(uint32_t at, unsigned n, uint64_t value) {
  RejectConfigurationAccess(at, n);
  // Mutate the executed original's history/counter store, never the captured
  // expectation or comparator. Neither offset belongs to the old six-field test.
  if (mutation_offset && timer_active && !mutated_writes &&
      at == kTimer + mutation_offset && n == 4) {
    value ^= 1; ++mutated_writes;
  }
  apply_replay::Store(at, n, value);
}
uint64_t Timebase() {
  Need(active && timer_active && ++timebase_calls == 1,
       "exactly one supplied timebase query, timer only");
  Need(uint32_t(timebase) != 0, "zero low timebase would spin");
  return timebase;
}
uint32_t Word(const Memory& m, unsigned offset) {
  return uint32_t(Big(m.timer.data() + offset, 4));
}
float Float(const Memory& m, unsigned offset) { return std::bit_cast<float>(Word(m, offset)); }
void StateDomain(const Memory& m, bool timer) {
  for (unsigned offset : {8u,12u,16u,20u,24u,28u,32u,36u,40u,80u,84u,88u,92u,96u,100u,104u})
    Need(std::isfinite(Float(m, offset)), "finite captured timer float");
  Need(m.timer[56] == 0 && m.timer[58] == 0 && m.timer[72] == 0,
       "unsupported reset/forced/deferred timer branch");
  Need(Word(m, 84) == 0x3F800000, "only captured unit timer scale");
  Need(Word(m, 60) <= 1, "only raw or fixed timer mode");
  Need(Float(m,40) > 0 && Float(m,40) <= Float(m,36), "positive ordered finite clamp");
  if (Word(m,60) == 0) Need(Word(m,32) == 0, "raw limiter must be disabled");
  else Need(Float(m,32) > 0, "positive fixed timer step");
  Need(m.timer[73] <= 1 && Word(m,76) == (m.timer[73] ? 2u : 0u),
       "exact split/count state");
  if (timer) Need(m.timer[73] == 0 && Word(m,76) == 0, "timer replay requires unsplit state");
  else {
    Need(Float(m,8) > 0 && Float(m,12) > 0, "positive helper factor and reciprocal");
    if (m.timer[73]) Need(Float(m,80) > 0, "positive captured saved split factor");
  }
}
Json ReadFixture(const char* path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  Need(bool(file), "fixture open");
  const auto length = file.tellg();
  Need(length > 0 && length <= std::streamoff(kMaximumBytes), "bounded fixture bytes");
  std::string bytes(size_t(length), '\0'); file.seekg(0);
  file.read(bytes.data(), std::streamsize(bytes.size()));
  Need(bool(file) && file.peek() == std::char_traits<char>::eof(), "complete exact fixture read");
  std::vector<std::set<std::string>> keys;
  auto callback = [&](int, Json::parse_event_t event, Json& value) {
    if (event == Json::parse_event_t::object_start) keys.emplace_back();
    if (event == Json::parse_event_t::key)
      Need(!keys.empty() && keys.back().insert(value.get<std::string>()).second, "duplicate JSON key");
    if (event == Json::parse_event_t::object_end) keys.pop_back();
    return true;
  };
  return Json::parse(bytes, callback);
}
void Incoming(const Json& value) {
  Keys(value, {"r3","r4","r5","f1_bits","f28_bits","f30_bits","f31_bits","loop_r23","loop_r24"});
  for (const char* key : {"r3","r4","r5","f1_bits","f28_bits","f30_bits","f31_bits"}) U(value.at(key), UINT64_MAX);
  U(value.at("loop_r23")); U(value.at("loop_r24"));
  Need(uint32_t(U(value.at("r3"), UINT64_MAX)) == kTimer, "exact captured timer low32 address");
}
void RowScope(const Json& row, const Memory& before, const Memory& after, bool timer) {
  const auto lr = U(row.at("caller_lr"), UINT64_MAX);
  const auto& incoming = row.at("incoming"); Incoming(incoming);
  if (timer) {
    Need(lr == 0x822C21B4 || lr == 0x822C22E4, "known timer caller");
    Need(U(incoming.at("f1_bits"), UINT64_MAX) == 0xBFF0000000000000ull,
         "only exact captured negative-one F1 sentinel");
    Need((U(incoming.at("r5"), UINT64_MAX) & 0xFF) <= 1, "captured secondary timer branch only");
    const auto now = U(row.at("timebase"), UINT64_MAX);
    const auto prior = Big(before.timer.data()+64, 8);
    Need(now == Big(after.timer.data()+64, 8) && now > prior &&
         now-prior <= uint64_t(INT64_MAX) && uint32_t(now), "explicit recorded timebase range");
  } else {
    Need(lr == 0x822C230C || lr == 0x822C246C || lr == 0x822C267C, "known subdivision caller");
    const auto r4 = U(incoming.at("r4"), UINT64_MAX) & 0xFF;
    const auto r5 = uint32_t(U(incoming.at("r5"), UINT64_MAX));
    Need((r4 == 1 && r5 == 2) || (r4 == 0 && r5 == 0), "only two-way split or restore/no-op");
    Need(row.at("timebase").is_null(), "subdivision has no timebase query");
    // F1/F28/F30/F31 are retained opaque bits: this extracted helper does not
    // read them. Do not normalize its captured negative/zero F1 into Timer's -1.
  }
  if (timer || lr != 0x822C246C)
    Need(U(incoming.at("loop_r23")) == 0 && U(incoming.at("loop_r24")) == 0,
         "loop fields only at exact recorded subdivision caller");
  StateDomain(before, timer); StateDomain(after, timer);
}
void Execute(Memory& m, const Json& row, bool timer) {
  RestoreMxcsr restore;
  m.initialized.fill(0); m.stack.fill(0x5A); timebase_calls = 0;
  PPCContext ctx{}; NonvolatileSeed(ctx);
  ctx.r1.u64 = kStack + 224; ctx.lr = U(row.at("caller_lr"), UINT64_MAX);
  const auto& in = row.at("incoming");
  // Preserve all supplied register bits; only the original PPC operations choose
  // low32/low8 semantics. Stack is synthetic and guarded, never a guest pointer.
  ctx.r3.u64 = U(in.at("r3"),UINT64_MAX); ctx.r4.u64 = U(in.at("r4"),UINT64_MAX);
  ctx.r5.u64 = U(in.at("r5"),UINT64_MAX); ctx.f1.u64 = U(in.at("f1_bits"),UINT64_MAX);
  ctx.f28.u64 = U(in.at("f28_bits"),UINT64_MAX); ctx.f30.u64 = U(in.at("f30_bits"),UINT64_MAX);
  ctx.f31.u64 = U(in.at("f31_bits"),UINT64_MAX);
  ctx.r23.u64 = U(in.at("loop_r23")); ctx.r24.u64 = U(in.at("loop_r24"));
  // Source-reviewed external live inputs: Timer r1/r3/r5/F1/timebase; Helper
  // r1/r3/r4/r5. Other volatile values are assigned before use in the pinned
  // bodies. No measured MXCSR exists: use explicit RN/no-FTZ 0x1F80 offline.
  const auto prior = ctx;
  std::array<uint8_t,PPCContext::kNonVolatileSaveSize> saved{}, returned{};
  ctx.SaveNonVolatiles(saved.data());
  _mm_setcsr(0x1F80); ctx.fpscr.csr = 0x1F80;
  active = &m; timer_active = timer;
  timebase = timer ? U(row.at("timebase"), UINT64_MAX) : 0;
  try { if (timer) TimerPlain(ctx,nullptr); else HelperPlain(ctx,nullptr); }
  catch (...) { active=nullptr; timer_active=false; throw; }
  active=nullptr; timer_active=false;
  Need(timebase_calls == unsigned(timer), "exact timebase query count");
  ctx.SaveNonVolatiles(returned.data());
  Need(saved == returned, "all SDK nonvolatile register bytes preserved");
  Need(ctx.r1.u64 == prior.r1.u64 && ctx.lr == prior.lr && ctx.r2.u64 == prior.r2.u64 &&
       ctx.r13.u64 == prior.r13.u64, "stack/link/reserved registers preserved");
  Need(ctx.cr2.raw() == prior.cr2.raw() && ctx.cr3.raw() == prior.cr3.raw() &&
       ctx.cr4.raw() == prior.cr4.raw(), "nonvolatile CR fields preserved");
  for (unsigned i=0;i<108;++i) {
    if (!TimerKnown(i)) Need(m.timer[i] == 0xA5, "unobserved timer poison intact");
    Need(m.config[i] == 0xA5, "uncaptured configuration remains wholly poisoned");
  }
}
unsigned MemoryGuards() {
  Memory m; active=&m; unsigned count=0;
  const auto reject=[&](auto call) {
    bool rejected=false; try {call();} catch(const std::runtime_error&) {rejected=true;}
    Need(rejected,"unknown/unobserved memory access must fail"); ++count;
  };
  try {
    reject([] {Load(kConfig+72,4);}); reject([] {Store(kConfig+84,4,0);});
    reject([] {Load(kTimer+57,1);}); reject([] {Store(kTimer+74,1,0);});
    reject([] {Load(kTimer+107,4);}); reject([] {Load(kStack,8);});
    reject([] {Store(kConstants[1],4,0);}); reject([] {Load(kConstants[1],8);});
    reject([] {Load(kTickConstant,8);}); reject([] {Store(kTickConstant,4,0);});
    reject([] {Load(0xFFFFFFFFu,4);}); reject([] {Load(kConstants[1]+1,1);});
  } catch (...) {active=nullptr;throw;}
  active=nullptr; return count;
}
int Emit(const Json& report,int code) {
  const auto output=report.dump(); Need(output.size() <= 32*1024*1024,"bounded report");
  std::cout << output << '\n'; std::cout.flush();
  return std::cout.good() ? code : 2;
}
} // namespace captured_state

#undef REX_LOAD_U8
#undef REX_LOAD_U32
#undef REX_LOAD_U64
#undef REX_STORE_U8
#undef REX_STORE_U32
#undef REX_STORE_U64
#undef REX_QUERY_TIMEBASE
#define REX_LOAD_U8(x) (static_cast<void>(base),uint8_t(captured_state::Load(uint32_t(x),1)))
#define REX_LOAD_U32(x) (static_cast<void>(base),uint32_t(captured_state::Load(uint32_t(x),4)))
#define REX_LOAD_U64(x) (static_cast<void>(base),captured_state::Load(uint32_t(x),8))
#define REX_STORE_U8(x,y) (static_cast<void>(base),captured_state::Store(uint32_t(x),1,uint8_t(y)))
#define REX_STORE_U32(x,y) (static_cast<void>(base),captured_state::Store(uint32_t(x),4,uint32_t(y)))
#define REX_STORE_U64(x,y) (static_cast<void>(base),captured_state::Store(uint32_t(x),8,uint64_t(y)))
#define REX_QUERY_TIMEBASE() captured_state::Timebase()
#include "timer-plain.inc"

int main(int argc,char** argv) {
  using namespace captured_state;
  RestoreMxcsr restore;
  Json results=Json::array(); uint64_t current_id=0, mismatches=0, timer_rows=0, helper_rows=0;
  uint64_t mutated_row_id=0; bool mutation_detected=false;
  unsigned memory_guards=0;
  Json report{{"schema","mcla-captured-timer-state-replay-v1"},
      {"entry_mxcsr",0x1F80},{"captured_bytes_per_row",kCapturedBytes},{"poison_bytes_per_row",kPoisonBytes},
      {"row_before_states_independent",true},{"historical_source_validation_external",true},
      {"source_completeness_proven",false},{"runtime_mxcsr_observed",false},{"real_clock_used",false},
      {"clock_units_proven",false},{"runtime_association_validated",false},{"continuous_coverage_proven",false},
      {"cross_row_continuity_proven",false},{"object_lifetime_proven",false},{"state_coherence_proven",false},
      {"configuration_to_work_binding_proven",false},{"simulation_proven",false},{"physical_speed_proven",false},
      {"fps_proven",false},{"native_admitted",false}};
  try {
    Need(argc==2 || (argc==3 && (!std::strcmp(argv[2],"--mutate-history") ||
         !std::strcmp(argv[2],"--mutate-counter"))), "fixture.json [--mutate-history|--mutate-counter]");
    mutation_offset=argc==3 ? (!std::strcmp(argv[2],"--mutate-history") ? 20u : 44u) : 0u;
    const auto input=ReadFixture(argv[1]); Keys(input,{"schema","token","constants","rows"});
    Need(input.at("schema")=="mcla-captured-timer-state-input-v1","exact input schema");
    Need(input.at("token").is_string(),"token string"); const auto token=input.at("token").get<std::string>();
    Need(token.size()==32 && token!=std::string(32,'0') &&
         token.find_first_not_of("0123456789abcdef")==std::string::npos,"nonzero lowercase session token");
    report["token"]=token;
    Need(input.at("constants").is_array() && input.at("constants").size()==5,"five explicit constants");
    Memory seed;
    constexpr std::array<uint32_t,5> bits{0,0x3F800000,0x38D1B717,0x3DCCCCCD,0x32AC3AB1};
    for(unsigned i=0;i<5;++i) {
      const auto& c=input.at("constants")[i]; Keys(c,{"address","word"});
      Need(U(c.at("address"))==(i<4?kConstants[i]:kTickConstant) && U(c.at("word"))==bits[i],
           "exact ordered constant address and bits");
      if(i<4)seed.constants[i]=bits[i];else coefficient=bits[i];
    }
    report["input_constants"]=input.at("constants");
    const auto& rows=input.at("rows");
    Need(rows.is_array() && !rows.empty() && rows.size()<=kMaximumRows,"all supplied rows bounded and nonempty");
    report["supplied_rows"]=rows.size(); uint64_t previous_id=0;
    // Validate every row before executing any; unsupported rows are not filtered.
    for(const auto& row:rows) {
      Keys(row,{"id","kind","caller_lr","incoming","timebase","before","after"});
      current_id=U(row.at("id"),kMaximumRows); Need(current_id>previous_id,"increasing unique captured IDs"); previous_id=current_id;
      Need(row.at("kind").is_string() && (row.at("kind")=="timer" || row.at("kind")=="subdivision"),"explicit timer/helper kind only");
      auto before=seed, after=seed; Timer(before.timer,row.at("before")); Timer(after.timer,row.at("after"));
      RowScope(row,before,after,row.at("kind")=="timer");
    }
    memory_guards=MemoryGuards();
    for(const auto& row:rows) {
      current_id=U(row.at("id"),kMaximumRows); const bool timer=row.at("kind")=="timer";
      auto actual=seed, expected=seed; Timer(actual.timer,row.at("before")); Timer(expected.timer,row.at("after"));
      const auto mutation_count_before=mutated_writes;
      Execute(actual,row,timer); timer_rows+=timer; helper_rows+=!timer;
      const bool this_row_mutated=mutated_writes!=mutation_count_before;
      if(this_row_mutated)mutated_row_id=current_id;
      Json different=Json::array(); unsigned captured=0, poison=0;
      for(unsigned i=0;i<108;++i) {
        if(TimerKnown(i)) {
          ++captured;
          if(actual.timer[i]!=expected.timer[i]) {
            different.push_back(Json{{"offset",i},{"actual",actual.timer[i]},{"captured",expected.timer[i]}});
            if(this_row_mutated && i>=mutation_offset && i<mutation_offset+4)mutation_detected=true;
          }
        } else {++poison;Need(actual.timer[i]==0xA5 && expected.timer[i]==0xA5,"both sides preserve unobserved poison");}
      }
      Need(captured==kCapturedBytes && poison==kPoisonBytes,"complete captured byte layout");
      const bool equal=different.empty(); mismatches+=!equal;
      results.push_back(Json{{"id",current_id},{"kind",row.at("kind")},{"caller_lr",row.at("caller_lr")},
          {"incoming",row.at("incoming")},{"timebase",row.at("timebase")},{"equal",equal},
          {"mismatching_bytes",different},{"actual_timer_bytes",actual.timer},{"captured_timer_bytes",expected.timer},
          {"reads",actual.reads},{"writes",actual.writes},{"timebase_calls",timebase_calls}});
    }
    Need(timer_rows+helper_rows==rows.size(),"every supplied row executed exactly once");
    int code=mismatches?1:0;
    const char* status=mismatches?(mutation_offset?"CAPTURED_STATE_MUTATION_DETECTED":"CAPTURED_STATE_MISMATCH"):
        "OFFLINE_CAPTURED_STATE_MATCH";
    if(mutation_offset && (mutated_writes!=1 || !mutation_detected)) {code=3;status="CAPTURED_STATE_MUTATION_NOT_DETECTED";}
    report["status"]=status; report["mutation_offset"]=mutation_offset; report["mutated_actual_writes"]=mutated_writes;
    report["mutated_row_id"]=mutated_row_id;report["mutation_detected_at_target_bytes"]=mutation_detected;
    report["timer_rows"]=timer_rows; report["subdivision_rows"]=helper_rows; report["mismatching_rows"]=mismatches;
    report["checks"]=checks; report["memory_negative_cases"]=memory_guards; report["rows"]=std::move(results);
    _mm_setcsr(restore.saved); return Emit(report,code);
  } catch(const std::exception& e) {
    active=nullptr;timer_active=false;_mm_setcsr(restore.saved);
    report["status"]="INPUT_OR_EXECUTION_REJECTED";report["failure"]=e.what();report["current_row_id"]=current_id;
    report["checks"]=checks;report["executed_rows"]=results.size();report["rows"]=std::move(results);
    std::cerr << "Captured state replay rejected: " << e.what() << '\n';std::cerr.flush();
    try {return Emit(report,2);}catch(...) {return 2;}
  } catch(...) {
    active=nullptr;timer_active=false;_mm_setcsr(restore.saved);
    std::cerr << "Captured state replay rejected: unknown exception\n";std::cerr.flush();return 2;
  }
}
