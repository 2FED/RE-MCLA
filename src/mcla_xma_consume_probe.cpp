// KI-071 step 2: guest-side XMA consume ledger (MCLA_XMA_CONSUME v=1).
// See mcla_xma_consume_probe.h for the guest layout and its citations, and
// docs/evidence/M11-ki071-xma-consume-probe-20261107.md.
//
// One pass-through strong override of sub_823ED700 (the voice pump's
// per-stream consume), active only with the init-only switch
// mcla_xma_consume_probe (default off). It saves r3/r4/r5, calls the ORIGINAL
// UNCHANGED, then reads the stream record through the checked reader
// (mcla_guest_reader.h) and adds r5 to the ledger of the record's hardware
// context id. Guest memory is never written. Once per second (and once per
// minute) the caller that collects the ledger writes one paced INFO line.

#include "mcla_xma_consume_probe.h"

#include "mcla_guest_reader.h"
#include "mcla_init.h"
#include "mcla_logging.h"

#include <rex/cvar.h>

#include <chrono>

REXCVAR_DEFINE_BOOL(mcla_xma_consume_probe, false, "MCLA",
    "Log KI-071 guest XMA consumption per hardware context id once per second "
    "(MCLA_XMA_CONSUME lines; diagnostic, read-only)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

namespace consume = mcla::xma_consume;
using Reader = mcla::guest::Reader;

bool ProbeOn() {
  static const bool on = REXCVAR_GET(mcla_xma_consume_probe);
  return on;
}

uint64_t NowMs() {
  return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch())
                                   .count());
}

consume::Ledger &TheLedger() {
  static consume::Ledger ledger;
  return ledger;
}

}  // namespace

// Consume: r3 = XMA voice object, r4 = stream index, r5 = samples consumed
// (per channel), r6 = out pointer (next read address).
REX_EXTERN(sub_823ED700) {
  if (!ProbeOn()) {
    __imp__sub_823ED700(ctx, base);
    return;
  }
  const uint32_t xma = ctx.r3.u32;
  const uint32_t stream = ctx.r4.u32;
  const uint32_t samples = ctx.r5.u32;
  __imp__sub_823ED700(ctx, base);
  auto &ledger = TheLedger();
  const Reader read = mcla::guest::HookReader(base);
  bool ok = stream < consume::kMaxStreamIndex;
  const uint32_t streams = ok ? read.U32(xma + consume::kStreamsOffset, ok) : 0;
  ok = ok && streams != 0;
  const uint32_t record = ok ? consume::RecordAddress(streams, stream) : 0;
  const uint32_t id = ok ? read.U16(record + consume::kRecordContextId, ok) : 0;
  const uint32_t dword1 = ok ? read.U32(record + consume::kRecordDword1, ok) : 0;
  if (ok) {
    ledger.Add(id, samples, consume::Stereo(dword1));
  } else {
    ledger.AddBad();
  }
  const consume::Lines lines = ledger.Collect(NowMs());
  if (!lines.second.empty()) {
    MCLA_APP_INFO("{}", lines.second);
  }
  if (!lines.minute.empty()) {
    MCLA_APP_INFO("{}", lines.minute);
  }
}
