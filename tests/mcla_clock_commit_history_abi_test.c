/* Standalone C11/C++23 POD vocabulary/layout prerequisite. No SDK, loader,
 * original clock, mutex discipline, process writer coverage or game claim. */
#if defined(MCLA_CH_TEST_PACK1) && defined(MCLA_CH_TEST_PACK4)
#error Select one packed negative fixture.
#endif
#if defined(MCLA_CH_TEST_PACK1)
#pragma pack(push, 1)
#elif defined(MCLA_CH_TEST_PACK4)
#pragma pack(push, 4)
#endif
#include "../src/mcla_clock_commit_history_abi.h"
#if defined(MCLA_CH_TEST_PACK1) || defined(MCLA_CH_TEST_PACK4)
#pragma pack(pop)
#endif
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
static MclaClockHistorySnapshotV1 snapshot; /* BSS, never a huge stack object. */
static void Check(int ok, const char* message) {
  ++checks;
  if (!ok) { ++failures; fprintf(stderr, "FAIL %s\n", message); }
}
static int Zero(const void* data, size_t bytes) {
  const unsigned char* p = (const unsigned char*)data;
  size_t i;
  for (i = 0; i < bytes; ++i) if (p[i]) return 0;
  return 1;
}
static void Vocabulary(void) {
  Check(MCLA_CH_ABI_V1 == 1 && MCLA_CH_BINDING_VERSION_V1 == 1 &&
        MCLA_CH_METADATA_VERSION_V1 == 1 && MCLA_CH_RECORD_VERSION_V1 == 1 &&
        MCLA_CH_SNAPSHOT_VERSION_V1 == 1, "additive history V1 versions");
  Check(MCLA_CH_CAPACITY_V1 == 4096 && MCLA_CH_PROOF_FLAGS == 0, "fixed capacity and no proof bits");
  Check(MCLA_CH_OK == 0 && MCLA_CH_BAD_ABI == 1 && MCLA_CH_REJECTED == 2 && MCLA_CH_PENDING == 3,
        "exact history result values, not witness V2 reinterpretation");
  Check(MCLA_CH_KIND_UNKNOWN == 0 && MCLA_CH_KIND_ADVANCE == 1 && MCLA_CH_KIND_CACHED == 2 &&
        MCLA_CH_KIND_RATIO == 3 && MCLA_CH_KIND_NO_SCALING == 4, "raw core kind vocabulary");
  Check(MCLA_CH_ORDERING_MUTEX == 1 && MCLA_CH_ORDERING_UNORDERED == 2, "raw ordering vocabulary");
  Check(MCLA_CH_HOST_SAMPLE == 1 && MCLA_CH_RETURNED == 2 && MCLA_CH_PRE_STATE == 4 &&
        MCLA_CH_POST_STATE == 8 && MCLA_CH_PRE_RATIO == 16 && MCLA_CH_POST_RATIO == 32,
        "individual raw presence bits");
  Check(MCLA_CH_QUERY_FIELDS == 63 && MCLA_CH_RATIO_FIELDS == 60 && MCLA_CH_UNORDERED_FIELDS == 19,
        "branch availability masks, not invented locked state");
  {
    const uint32_t values[] = {
      MCLA_CH_ISSUE_INVALID_BINDING, MCLA_CH_ISSUE_INVALID_KIND, MCLA_CH_ISSUE_WRONG_ORDERING,
      MCLA_CH_ISSUE_UNKNOWN_PRESENCE, MCLA_CH_ISSUE_MISSING_FIELDS, MCLA_CH_ISSUE_UNEXPECTED_FIELDS,
      MCLA_CH_ISSUE_INVALID_RATIO, MCLA_CH_ISSUE_RETURNED_MISMATCH, MCLA_CH_ISSUE_ADVANCE_HOST_MISMATCH,
      MCLA_CH_ISSUE_CACHED_STATE_CHANGED, MCLA_CH_ISSUE_RATIO_STATE_CHANGED, MCLA_CH_ISSUE_QUERY_RATIO_CHANGED,
      MCLA_CH_ISSUE_NONZERO_RESERVED, MCLA_CH_ISSUE_ABORTED, MCLA_CH_ISSUE_METADATA_UNAVAILABLE
    };
    size_t i;
    for (i = 0; i < sizeof(values)/sizeof(values[0]); ++i)
      Check(values[i] == (UINT32_C(1) << i), "all fifteen core issue bits unchanged");
  }
  Check(MCLA_CH_EMPTY == 0 && MCLA_CH_IN_FLIGHT == 1 && MCLA_CH_PUBLISHED == 2 && MCLA_CH_ABORTED == 3,
        "raw disposition vocabulary");
}
static void RoundTrip(void) {
  MclaClockHistoryRecordV1 source = {0}, copied = {0};
  unsigned char bytes[184];
  source.storage_ordinal = UINT64_MAX;
  source.mutex_ordinal = UINT64_MAX - UINT64_C(1);
  source.kind = UINT32_MAX; source.ordering = UINT32_MAX;
  source.binding.run_high = UINT64_MAX;
  source.binding.run_low = UINT64_MAX - UINT64_C(2);
  source.binding.timer_invocation = UINT64_MAX - UINT64_C(3);
  source.binding.read_ordinal = 0; /* Valid first ordinal, not absence. */
  source.binding.phase_call_id = UINT64_MAX - UINT64_C(4);
  source.binding.phase_thread_epoch = UINT64_MAX - UINT64_C(5);
  source.binding.phase_native_thread_id = UINT32_MAX;
  source.binding.phase_present = UINT32_MAX;
  source.binding.present = UINT32_MAX; source.binding.reserved = UINT32_MAX;
  source.metadata.host_sample = UINT64_MAX - UINT64_C(6);
  source.metadata.returned_guest = UINT64_MAX - UINT64_C(7);
  source.metadata.pre_host = UINT64_MAX - UINT64_C(8);
  source.metadata.pre_guest = UINT64_MAX - UINT64_C(9);
  source.metadata.post_host = UINT64_MAX - UINT64_C(10);
  source.metadata.post_guest = UINT64_MAX - UINT64_C(11);
  source.metadata.pre_ratio_numerator = UINT64_MAX - UINT64_C(12);
  source.metadata.pre_ratio_denominator = 0; /* Raw contradiction retained. */
  source.metadata.post_ratio_numerator = UINT64_MAX - UINT64_C(13);
  source.metadata.post_ratio_denominator = UINT64_MAX - UINT64_C(14);
  source.metadata.presence = UINT32_MAX; source.metadata.reserved = UINT32_MAX;
  source.disposition = UINT32_MAX; source.issues = UINT32_MAX;
  memcpy(bytes, &source, sizeof(source));
  memcpy(&copied, bytes, sizeof(copied));
  Check(memcmp(&source, &copied, sizeof(source)) == 0, "full184-byte row representation roundtrip");
  Check(copied.storage_ordinal == UINT64_MAX && copied.mutex_ordinal == UINT64_MAX - UINT64_C(1),
        "distinct storage/mutex ordinals survive max values");
  Check(copied.binding.run_high == UINT64_MAX && copied.binding.read_ordinal == 0 &&
        copied.binding.present == UINT32_MAX && copied.binding.reserved == UINT32_MAX,
        "full128 binding, first read and unknown binding metadata survive");
  Check(copied.metadata.pre_ratio_denominator == 0 && copied.metadata.presence == UINT32_MAX &&
        copied.metadata.reserved == UINT32_MAX && copied.issues == UINT32_MAX,
        "unavailable/malformed numeric metadata not normalized");
  Check(copied.metadata.host_sample == UINT64_MAX - UINT64_C(6) &&
        copied.metadata.post_ratio_denominator == UINT64_MAX - UINT64_C(14),
        "metadata unsigned64 endpoints survive");
}
static void EmptyEnvelopes(void) {
  MclaClockHistoryInfoV1 info = {0};
  MclaClockHistoryCountsV1 counts = {0};
  MclaClockHistoryInfoFnV1 info_fn = 0;
  MclaClockHistoryArmFnV1 arm_fn = 0;
  MclaClockHistoryStopFnV1 stop_fn = 0;
  MclaClockHistorySnapshotFnV1 snapshot_fn = 0;
  Check(!info_fn && !arm_fn && !stop_fn && !snapshot_fn, "four optional function typedefs without implementations");
  Check(Zero(&snapshot, sizeof(snapshot)), "entire static snapshot initially zero including all4096 unused rows");
  info.abi_version = MCLA_CH_ABI_V1; info.struct_size = (uint32_t)sizeof(info);
  snapshot.abi_version = MCLA_CH_ABI_V1; snapshot.struct_size = (uint32_t)sizeof(snapshot);
  Check(info.abi_version == 1 && info.struct_size == 72 && info.proof_flags == 0 &&
        info.reserved == 0 && info.reserved_words[0] == 0 && info.reserved_words[1] == 0,
        "exact zero-initialized first Info input prefix");
  Check(snapshot.abi_version == 1 && snapshot.struct_size == 753784 &&
        snapshot.row_count == 0 && snapshot.proof_flags == 0 && snapshot.counts_reserved == 0 && snapshot.reserved == 0,
        "exact zero-initialized first Snapshot input prefix");
  Check(Zero(snapshot.rows, sizeof(snapshot.rows)), "unused rows remain zero without fabricated records");
  counts.observed_attempts = UINT64_MAX; counts.mutex_ordinals = UINT64_MAX;
  counts.unordered_attempts = UINT64_MAX; counts.stored_entries = UINT64_MAX;
  counts.published_entries = UINT64_MAX; counts.aborted_entries = UINT64_MAX;
  counts.overflow_entries = UINT64_MAX; counts.diagnostic_entries = UINT64_MAX;
  counts.saturated_rejections = UINT64_MAX; counts.counter_saturated = UINT32_MAX;
  Check(counts.observed_attempts == UINT64_MAX && counts.saturated_rejections == UINT64_MAX &&
        counts.counter_saturated == UINT32_MAX && counts.reserved == 0,
        "raw count widths and reserved zero distinct from semantic consistency");
}
int main(void) {
  Vocabulary(); RoundTrip(); EmptyEnvelopes();
  printf("checks=%u failures=%u declaration_only_no_SDK_no_runtime_no_history_no_FPS\n", checks, failures);
  fflush(stdout); fflush(stderr);
  return failures ? 1 : 0;
}
