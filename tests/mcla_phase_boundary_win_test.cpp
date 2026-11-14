// Real phase wrapper and lexical boundary accessor, synthetic originals/clocks.
// No game, GPU, windows, files, input, guest reads or native admission.
#if defined(MCLA_FRAME_HANDOFF_PROBE)
#error This bounded test deliberately excludes the frame-handoff observer.
#endif
#include "../src/mcla_phase_census_win.cpp"
#include <cstring>
#include <functional>
#include <iostream>
#include <xmmintrin.h>

namespace {
namespace phase = mcla::phase_census;
namespace d = phase::detail;
constexpr char token[] = "0123456789abcdef0123456789abcdef";
constexpr uint64_t begin = 100, end = 200, frequency = 100;
unsigned checks{}, failures{};
uint64_t tick = 99, clock_reads{};
void Check(bool value, const char* message) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << message << '\n'; }
}
uint64_t Clock() { ++clock_reads; return tick; }
uint64_t ThrowClock() { ++clock_reads; throw 13; }
struct Fixture;
Fixture* current{};
struct Fixture {
  std::unique_ptr<d::State> state = std::make_unique<d::State>();
  std::array<uint8_t, 4> memory{};
  std::function<void(PPCContext&, uint8_t*)> body, render;
  unsigned calls{}, depth{};
  Fixture() {
    Check(!current && !d::active_invocation && !d::local.depth,
          "fixture starts with no inherited lexical state");
    current = this; tick = 99; clock_reads = 0; d::local = {};
    state->begin = begin; state->end = end; state->frequency = frequency;
    state->token = token; state->clock = Clock;
    d::configured.store(state.get());
  }
  ~Fixture() {
    Check(!d::active_invocation && !d::local.depth && !d::local.suppressed && !state->active,
          "all lexical scopes and census tickets unwind");
    d::configured.store(nullptr); d::local = {}; current = nullptr;
  }
};
bool Before(const PPCContext& c, const uint8_t* b) {
  return phase::ReadActiveUpdateBeforeWindow(c, b, begin, end, frequency, token).valid();
}
void Update(PPCContext& c, uint8_t* b) { sub_822C1FA8(c, b); }
void PositiveAndReadOnly() {
  Fixture f; PPCContext c{};
  Check(!Before(c, f.memory.data()), "no current invocation means no boundary witness");
  const auto next_id = d::next_call.load(), next_thread = d::next_thread.load();
  f.body = [&](PPCContext& context, uint8_t* base) {
    // The captured entry remains 99: no new timestamp is sampled at query time.
    tick = 150;
    const auto saved = context;
    const auto reads = clock_reads, callbacks = f.state->callback_used;
    const auto csr = _mm_getcsr(); SetLastError(735);
    const bool first = Before(context, base), second = Before(context, base);
    const bool error_preserved = GetLastError() == 735;
    Check(first && second, "positive prebegin entry remains an exact lexical witness after begin");
    Check(phase::ReadActiveUpdateBeforeWindow(context, base, begin, end, frequency, token).entry_qpc == 99,
          "returned provenance retains exact entry timestamp, not current clock or synthetic identity");
    Check(error_preserved && _mm_getcsr() == csr &&
          std::memcmp(&context, &saved, sizeof(context)) == 0,
          "boundary accessor preserves LastError, MXCSR and every context byte");
    Check(clock_reads == reads && f.state->callback_used == callbacks && f.state->used == 0 &&
          d::next_call.load() == next_id && d::next_thread.load() == next_thread,
          "boundary accessor allocates no IDs, clocks, census rows or callbacks");
    Check(!phase::ReadActiveUpdate(context, base).bound(), "boundary witness is never an admitted identity");
  };
  Update(c, f.memory.data());
  Check(f.calls == 1 && !Before(c, f.memory.data()) && f.state->used == 0,
        "original executes once and boundary witness expires on lexical return");
}
void EntryBoundaries() {
  for (uint64_t entry : {0ull, 99ull, 100ull, 199ull, 200ull, uint64_t(INT64_MAX) + 1}) {
    Fixture f; PPCContext c{}; tick = entry;
    f.body = [&](PPCContext& context, uint8_t* base) {
      Check(Before(context, base) == (entry == 99), "only positive strictly-before entry qualifies");
      Check(phase::ReadActiveUpdate(context, base).bound() == (entry >= begin && entry < end),
            "existing admitted identity semantics remain unchanged at boundaries");
    };
    Update(c, f.memory.data());
  }
  Fixture f; PPCContext c{}; f.state->clock = ThrowClock;
  f.body = [](PPCContext& context, uint8_t* base) {
    Check(!Before(context, base), "throwing entry clock cannot create a witness");
  };
  Update(c, f.memory.data());
}
void ExactBindings() {
  Fixture f; PPCContext c{};
  f.body = [&](PPCContext& context, uint8_t* base) {
    PPCContext other{};
    Check(!Before(other, base) && !Before(context, base + 1) && !Before(context, nullptr),
          "wrong context, wrong base and null base reject");
    bool foreign = true;
    std::thread worker([&] { foreign = Before(context, base); }); worker.join();
    Check(!foreign, "another thread cannot borrow an exact pointer's lexical witness");
    Check(!phase::ReadActiveUpdateBeforeWindow(context, base, begin + 1, end, frequency, token).valid() &&
          !phase::ReadActiveUpdateBeforeWindow(context, base, begin, end + 1, frequency, token).valid() &&
          !phase::ReadActiveUpdateBeforeWindow(context, base, begin, end, frequency + 1, token).valid() &&
          !phase::ReadActiveUpdateBeforeWindow(context, base, begin, end, frequency,
                                             "1123456789abcdef0123456789abcdef").valid(),
          "window, frequency and token must exactly match configured state");
    d::State replacement;
    d::configured.store(&replacement);
    Check(!Before(context, base), "matching lexical pointer cannot outlive configured-state identity");
    d::configured.store(f.state.get());
    Check(Before(context, base), "synthetic configuration restoration reveals original exact witness");
  };
  Update(c, f.memory.data());
}
void NestedMasks() {
  for (int mode = 0; mode != 4; ++mode) {
    Fixture f; PPCContext c{};
    f.render = [](PPCContext& context, uint8_t* base) {
      Check(!Before(context, base), "Render cannot expose a prebegin Update ancestor");
    };
    f.body = [&](PPCContext& context, uint8_t* base) {
      if (f.depth > 1) {
        Check(!Before(context, base), "unconfigured, admitted or invalid nested Update masks ancestor");
        return;
      }
      Check(Before(context, base), "outer prebegin Update has witness before nested call");
      if (mode == 0) sub_82131008(context, base);
      if (mode == 1) {
        d::configured.store(nullptr); Update(context, base); d::configured.store(f.state.get());
      }
      if (mode == 2) { tick = begin; Update(context, base); }
      if (mode == 3) { tick = 0; Update(context, base); }
      tick = 110;
      Check(Before(context, base) == (mode != 3),
            "outer witness restores unless nested invalid clock makes health fail closed");
    };
    Update(c, f.memory.data());
  }
}
void NoConfigurationAtEntry() {
  Fixture f; PPCContext c{};
  d::configured.store(nullptr);
  f.body = [&](PPCContext& context, uint8_t* base) {
    d::configured.store(f.state.get());
    Check(!Before(context, base), "configuration appearing after entry cannot invent entry provenance");
  };
  Update(c, f.memory.data());
}
void ReversedClockControls() {
  {
    Fixture f; PPCContext c{}; tick = 150;
    f.body = [&](PPCContext& context, uint8_t* base) {
      if (f.depth == 1) {
        Check(phase::ReadActiveUpdate(context, base).bound(), "rollback parent is a real in-window Update");
        tick = 99; Update(context, base); tick = 160;
      } else {
        Check(!Before(context, base), "positive nested rollback below begin must not create a witness");
        Check(f.state->boundary_clock_failed, "rollback has sticky independent boundary-clock health");
      }
    };
    Update(c, f.memory.data());
    Check(f.state->used == 1 && !f.state->invalid_qpc && !f.state->failed.load(),
          "boundary-clock guard does not rewrite legacy positive-clock census semantics");
  }
  {
    Fixture f; PPCContext c{}; tick = begin;
    f.body = [](PPCContext&, uint8_t*) {};
    Update(c, f.memory.data());
    tick = 90;
    f.body = [](PPCContext& context, uint8_t* base) {
      Check(!Before(context, base), "new lexical invocation cannot hide rollback after a completed Update");
    };
    Update(c, f.memory.data());
    Check(f.state->boundary_clock_failed, "completed-scope clock high-water mark persists");
  }
  {
    Fixture f; PPCContext c{};
    f.body = [&](PPCContext& context, uint8_t* base) {
      Check(Before(context, base), "initial valid witness exists before observer clock rollback");
      tick = 110; phase::ObserveSwap(context, base);
      tick = 105; phase::ObserveSwap(context, base);
      Check(!Before(context, base), "rollback of an existing phase swap sample revokes retained witness");
    };
    Update(c, f.memory.data());
  }
}
void HealthAndCapacity() {
  // Change health only in synthetic originals, never clear it inside a fixture.
  for (int mode = 0; mode != 12; ++mode) {
    Fixture f; PPCContext c{};
    f.body = [&](PPCContext& context, uint8_t* base) {
      Check(Before(context, base), "healthy witness exists before synthetic health transition");
      switch (mode) {
        case 0: f.state->stopped = true; break;
        case 1: f.state->failed = true; break;
        case 2: f.state->drain_incomplete = true; break;
        case 3: f.state->invalid_qpc = true; break;
        case 4: f.state->identity_exhausted = true; break;
        case 5: f.state->dropped_scopes = 1; break;
        case 6: f.state->dropped_callbacks = 1; break;
        case 7: f.state->stack_overflow = 1; break;
        case 8: f.state->scope_overflow = 1; break;
        case 9: f.state->callback_overflow = 1; break;
        case 10: f.state->used = d::kScopes; break;
        case 11: f.state->original_exception = true; break;
      }
      Check(!Before(context, base), "health failure or exhausted census capacity revokes witness");
    };
    Update(c, f.memory.data());
  }
  Fixture f; PPCContext c{};
  f.body = [&](PPCContext& context, uint8_t* base) {
    Check(Before(context, base) == (f.depth <= d::kDepth),
          "prebegin lexical nesting cannot bypass bounded census depth");
    if (f.depth < d::kDepth + 2) Update(context, base);
    Check(Before(context, base) == (f.depth <= d::kDepth),
          "depth suppression masks only current invocation and unwinds exactly");
  };
  Update(c, f.memory.data());
  Check(f.state->used == 0, "prebegin depth test invents no admitted rows");
}
} // namespace

REX_EXTERN(__imp__sub_822C1FA8) {
  ++current->calls; ++current->depth;
  try { if (current->body) current->body(ctx, base); }
  catch (...) { --current->depth; throw; }
  --current->depth;
}
REX_EXTERN(__imp__sub_82305D00) { (void)ctx; (void)base; }
REX_EXTERN(__imp__sub_82131008) { if (current->render) current->render(ctx, base); }
int main() {
  PositiveAndReadOnly(); EntryBoundaries(); ExactBindings(); NestedMasks();
  NoConfigurationAtEntry(); ReversedClockControls(); HealthAndCapacity();
  std::cout << "phase boundary: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
