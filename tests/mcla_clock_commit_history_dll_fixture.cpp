// SYNTHETIC DLL prerequisite only: prescribed POD metadata, no SDK clock,
// source clock mutex, game, timing, file, input or complete-history proof.
// The consumer owns no recorder. Keep this DLL/process storage alive forever;
// mode2 must destroy the held Call before the consumer exits normally.
#include "../src/mcla_clock_commit_history_sdk_bridge.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <new>

/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2019 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */
// The checked recipe extracts the EXACT two accepted native guard classes
// from the parent declaration bytes, rather than reimplementing them here.
#include "mcla_clock_history_native_guard.inc"
#include "mcla_clock_history_declarations.inc"
#if defined(MCLA_CLOCK_HISTORY_FFI_DESCRIPTOR_MUTANT)
// Private negative include: ONLY successful Info.record_size is XORed with1.
// Normal production exports remain byte-identical to the preparer constants.
#include "mcla_clock_history_exports_descriptor_mutant.inc"
#else
#include "mcla_clock_history_exports.inc"
#endif

namespace fixture {
namespace history = ::mcla::clock_commit_history;
namespace sdk = history::sdk;
namespace witness = ::mcla::clock_read_witness::bridge;
constexpr uint64_t kHigh = 0x123456789ABCDEF0ull;
constexpr uint64_t kLow = 0xFEDCBA9876543210ull;
// This separate old witness runtime supplies one REAL accepted synthetic
// Frame, not another history recorder and not a guessed/history binding.
constinit witness::Runtime binding_runtime;
alignas(sdk::Call) unsigned char held_storage[sizeof(sdk::Call)]{};
sdk::Call* held = nullptr;
uint32_t stage = 0;  // Single external fixture caller; never reset/rearmed.

void Query(uint32_t kind, uint64_t sample, uint64_t pre_host,
           uint64_t pre_guest, uint64_t post_host, uint64_t post_guest) noexcept {
  sdk::Call call(false);
  call.Sampled(sample);
  call.LockedBefore(kind, 5, 1, pre_host, pre_guest);
  call.LockedAfter(5, 1, post_host, post_guest, post_guest);
}
uint32_t EmitAndHold() noexcept {
  if (stage != 0 || held) return MCLA_CH_REJECTED;
  if (!binding_runtime.Arm(kHigh, kLow)) return MCLA_CH_REJECTED;
  stage = 1;
  Query(uint32_t(history::Kind::Advance), 101, 100, 500, 101, 505);
  const MclaClockWitnessBindingV1 binding = {
      MCLA_CW_ABI_V1, sizeof(binding), kHigh, kLow, 7, 0, 11, 13, 17, 1, 0, 0};
  MclaClockWitnessStatusV1 status{};
  const auto result = binding_runtime.QueryBound(&binding, &status, [] {
    // Order matters: history copies the accepted binding BEFORE Witness
    // consumes it. All values are prescribed synthetic POD, not clock reads.
    sdk::Call call(true);
    witness::Witness source;
    call.Sampled(102); source.Sampled(102);
    call.LockedBefore(uint32_t(history::Kind::Advance), 5, 1, 101, 505);
    source.LockedBefore(5, 1, 101, 505);
    call.LockedAfter(5, 1, 102, 510, 510); source.Advanced(102, 510);
    return source.Returned(510);
  });
  if (result != 510 || status.admission != MCLA_CW_ACCEPTED ||
      status.completion != MCLA_CW_PUBLISHED || status.issues != 0)
    return MCLA_CH_REJECTED;
  Query(uint32_t(history::Kind::Cached), 99, 102, 510, 102, 510);
  {
    sdk::Call ratio(false);
    ratio.RatioBefore(5, 1, 102, 510);
    ratio.RatioAfter(3, 2, 102, 510);
  }
  {
    sdk::Call unordered(false);
    unordered.Sampled(104); unordered.UnscaledBefore(3, 2);
    unordered.UnscaledAfter(156);
  }
  {
    // EXPLICIT fixture-only direct-core row. Deliberately invalid absent
    // binding.reserved and metadata.reserved must cross the ABI unaltered.
    // No claim that the real SDK source emits these reserved-field values.
    history::Binding malformed{}; malformed.reserved = 29;
    auto lease = sdk::runtime.BeginLocked(history::Kind::Advance, malformed);
    const history::Metadata metadata{
        103, 504, 100, 500, 103, 504, 3, 2, 3, 2,
        history::kQueryFields, 0xAABBu};
    if (!lease.accepted() || !lease.Publish(metadata)) return MCLA_CH_REJECTED;
  }
  held = ::new (static_cast<void*>(held_storage)) sdk::Call(false);
  held->Sampled(105);
  held->LockedBefore(uint32_t(history::Kind::Advance), 3, 2, 104, 512);
  {
    sdk::Call aborted(false);
    aborted.Sampled(106);
    aborted.LockedBefore(uint32_t(history::Kind::Advance), 3, 2, 105, 513);
    // No completion: destruction MUST retain an aborted/unavailable row.
  }
  return MCLA_CH_OK;
}
uint32_t ReleaseHeld() noexcept {
  if (stage != 1 || !held) return MCLA_CH_REJECTED;
  held->LockedAfter(3, 2, 105, 513, 513);
  held->~Call(); held = nullptr; stage = 2;
  binding_runtime.Stop();  // No exposed V2 API; all of its calls have finished.
  return MCLA_CH_OK;
}
uint32_t LateAttempt() noexcept {
  // Used before Arm/default-OFF and after Stop. This is a complete bounded
  // POD adapter call even when recording rejects; no new lifetime/reset.
  sdk::Call call(false);
  call.Sampled(999);
  call.LockedBefore(uint32_t(history::Kind::Advance), 3, 2, 998, 42);
  call.LockedAfter(3, 2, 999, 43, 43);
  return MCLA_CH_OK;
}
} // namespace fixture

// The ONLY fixture-specific export; never part of the production ABI/module.
// Fixed modes1/2/3, no callbacks, Query, reset, configuration or clock source.
extern "C" __declspec(dllexport) uint32_t MCLA_CH_CALL
MclaClockHistoryFixtureV1(uint32_t mode) noexcept {
  const MclaClockWitnessSdkPreserveIncoming preserve;
  switch (mode) {
    case 1: return fixture::EmitAndHold();
    case 2: return fixture::ReleaseHeld();
    case 3: return fixture::LateAttempt();
    default: return MCLA_CH_REJECTED;
  }
}
