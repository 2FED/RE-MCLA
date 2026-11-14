/* SAME file compiled as C11 and C++23. Synthetic DLL FFI prerequisite ONLY.
 * Only the C ABI lives in this executable: no core, TLS, transport or SDK.
 * The checked parent recipe MUST pin DLL/EXE and dependency bytes immediately
 * before process launch. Path/owner checks below are NOT a file-hash verifier.
 * All Calls finish and the loaded DLL stays process-pinned; no unload claim.
 */
#include "../src/mcla_clock_commit_history_abi.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <xmmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

typedef uint32_t (MCLA_CH_CALL *FixtureFn)(uint32_t);
typedef struct Api {
  MclaClockHistoryInfoFnV1 info;
  MclaClockHistoryArmFnV1 arm;
  MclaClockHistoryStopFnV1 stop;
  MclaClockHistorySnapshotFnV1 snapshot;
  FixtureFn fixture;
} Api;
typedef struct NativeState { DWORD error; unsigned int mxcsr; } NativeState;
static unsigned int checks, failures, native_sequence;
static const uint64_t high = UINT64_C(0x123456789ABCDEF0);
static const uint64_t low = UINT64_C(0xFEDCBA9876543210);
static void Check(int value, const char* label) {
  ++checks;
  if (!value) { ++failures; printf("FAIL %s\n", label); fflush(stdout); }
}
static NativeState Capture(void) {
  NativeState value; value.error = GetLastError(); value.mxcsr = _mm_getcsr();
  return value;
}
static NativeState Before(void) {
  NativeState value;
  value.error = 0x13572000u + native_sequence++;
  value.mxcsr = (native_sequence & 1u) ? 0x3F80u : 0x5F80u;
  SetLastError(value.error); _mm_setcsr(value.mxcsr); return value;
}
static void NativeEqual(NativeState before, NativeState after) {
  Check(before.error == after.error && before.mxcsr == after.mxcsr,
        "history export changed incoming LastError/MXCSR");
}
static uint32_t Info(const Api* api, MclaClockHistoryInfoV1* out) {
  NativeState before = Before(), after;
  uint32_t result = api->info(out); after = Capture();
  NativeEqual(before, after); return result;
}
static uint32_t Arm(const Api* api, uint64_t hi, uint64_t lo) {
  NativeState before = Before(), after;
  uint32_t result = api->arm(hi, lo); after = Capture();
  NativeEqual(before, after); return result;
}
static void Stop(const Api* api) {
  NativeState before = Before(), after;
  api->stop(); after = Capture(); NativeEqual(before, after);
}
static uint32_t Snapshot(const Api* api, MclaClockHistorySnapshotV1* out) {
  NativeState before = Before(), after;
  uint32_t result = api->snapshot(out); after = Capture();
  NativeEqual(before, after); return result;
}
static int Zero(const void* data, size_t size) {
  const unsigned char* p = (const unsigned char*)data;
  size_t index;
  for (index = 0; index < size; ++index) if (p[index]) return 0;
  return 1;
}
static void Prefix(MclaClockHistorySnapshotV1* out) {
  memset(out, 0, sizeof(*out));
  out->abi_version = MCLA_CH_ABI_V1; out->struct_size = sizeof(*out);
}
static FARPROC Resolve(HMODULE module, const char* name) {
  FARPROC result = GetProcAddress(module, name);
  HMODULE owner = NULL;
  MEMORY_BASIC_INFORMATION memory;
  int executable = 0;
  Check(result != NULL, name);
  if (!result) return NULL;
  Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      (LPCWSTR)(uintptr_t)result, &owner) && owner == module,
      "export owner is not the exact loaded DLL");
  memset(&memory, 0, sizeof(memory));
  if (VirtualQuery((const void*)(uintptr_t)result, &memory, sizeof(memory))) {
    DWORD protection = memory.Protect & 0xFFu;
    executable = protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
  }
  Check(executable && memory.AllocationBase == module &&
      !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)), "export is not executable DLL storage");
  return result;
}
static int Load(const char* path, Api* api) {
  /* Owned Windows UTF8 DOS absolute path, no current-directory DLL search.
   * Heap buffers avoid the normal giant-Windows-path automatic allocation. */
  wchar_t* wide = (wchar_t*)calloc(32768, sizeof(wchar_t));
  wchar_t* full = (wchar_t*)calloc(32768, sizeof(wchar_t));
  wchar_t* actual = (wchar_t*)calloc(32768, sizeof(wchar_t));
  HMODULE module = NULL, pinned = NULL;
  int count;
  DWORD full_size, actual_size;
  if (!wide || !full || !actual) { Check(0, "path allocation failed"); goto done; }
  count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, 32768);
  Check(count > 3 && wide[1] == L':' && (wide[2] == L'\\' || wide[2] == L'/'),
        "DLL argument must be an absolute UTF8 DOS path");
  if (failures) goto done;
  full_size = GetFullPathNameW(wide, 32768, full, NULL);
  Check(full_size > 0 && full_size < 32768, "DLL canonical path bounded");
  if (failures) goto done;
  module = LoadLibraryExW(full, NULL,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
  Check(module != NULL, "restricted absolute LoadLibraryExW");
  if (!module) goto done;
  actual_size = GetModuleFileNameW(module, actual, 32768);
  Check(actual_size > 0 && actual_size < 32768 && _wcsicmp(full, actual) == 0,
        "loaded DLL path differs from exact argument");
  api->info = (MclaClockHistoryInfoFnV1)Resolve(module, "MclaClockHistoryInfoV1Export");
  api->arm = (MclaClockHistoryArmFnV1)Resolve(module, "MclaClockHistoryArmV1Export");
  api->stop = (MclaClockHistoryStopFnV1)Resolve(module, "MclaClockHistoryStopV1Export");
  api->snapshot = (MclaClockHistorySnapshotFnV1)Resolve(module, "MclaClockHistorySnapshotV1Export");
  api->fixture = (FixtureFn)Resolve(module, "MclaClockHistoryFixtureV1");
  Check(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
      GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)(uintptr_t)api->info, &pinned) && pinned == module,
      "DLL process-lifetime pin");
  Check(GetProcAddress(module, "MclaClockHistoryQueryV1Export") == NULL &&
      GetProcAddress(module, "MclaClockHistoryResetV1Export") == NULL,
      "no history Query/reset export");
done:
  free(wide); free(full); free(actual);
  /* Intentionally NO FreeLibrary, even for a rejected descriptor. */
  return failures == 0;
}
static int Descriptor(const Api* api) {
  MclaClockHistoryInfoV1 info;
  memset(&info, 0, sizeof(info)); info.abi_version = 1; info.struct_size = sizeof(info);
  /* FIRST history API call / first guard touch in this fresh process. */
  Check(Info(api, &info) == MCLA_CH_OK, "first-touch Info returned OK");
  Check(info.abi_version == 1 && info.struct_size == 72, "history descriptor envelope mismatch");
  Check(info.binding_version == 1 && info.binding_size == 64, "history descriptor binding mismatch");
  Check(info.metadata_version == 1 && info.metadata_size == 88, "history descriptor metadata mismatch");
  Check(info.record_version == 1 && info.record_size == 184,
        "history descriptor record size mismatch");
  Check(info.snapshot_version == 1 && info.snapshot_size == 753784,
        "history descriptor snapshot mismatch");
  Check(info.rows_offset == 120 && info.capacity == 4096, "history descriptor capacity mismatch");
  Check(info.proof_flags == 0 && info.reserved == 0 &&
      info.reserved_words[0] == 0 && info.reserved_words[1] == 0,
      "history descriptor proof/reserved mismatch");
  return failures == 0; /* Fail closed BEFORE Arm/fixture if wire is unsupported. */
}
static void InvalidEnvelopes(const Api* api, MclaClockHistorySnapshotV1* out,
                             MclaClockHistorySnapshotV1* before) {
  MclaClockHistoryInfoV1 info, info_before;
  unsigned char* unaligned = (unsigned char*)malloc(sizeof(*out) + 8);
  uint32_t prefix[2];
  Check(Info(api, NULL) == MCLA_CH_BAD_ABI, "Info null rejected");
  memset(&info, 0xA7, sizeof(info)); info.abi_version = 99; info.struct_size = sizeof(info);
  memcpy(&info_before, &info, sizeof(info));
  Check(Info(api, &info) == MCLA_CH_BAD_ABI && memcmp(&info, &info_before, sizeof(info)) == 0,
        "Info wrong prefix unchanged");
  info.abi_version = 1; info.struct_size = 8; memcpy(&info_before, &info, sizeof(info));
  Check(Info(api, &info) == MCLA_CH_BAD_ABI && memcmp(&info, &info_before, sizeof(info)) == 0,
        "Info wrong size unchanged");
  Check(Snapshot(api, NULL) == MCLA_CH_BAD_ABI, "Snapshot null rejected");
  Prefix(out); out->abi_version = 99; memcpy(before, out, sizeof(*out));
  Check(Snapshot(api, out) == MCLA_CH_BAD_ABI && memcmp(out, before, sizeof(*out)) == 0,
        "Snapshot wrong prefix unchanged");
  Prefix(out); out->struct_size = 8; memcpy(before, out, sizeof(*out));
  Check(Snapshot(api, out) == MCLA_CH_BAD_ABI && memcmp(out, before, sizeof(*out)) == 0,
        "Snapshot wrong size unchanged");
  Check(unaligned != NULL, "unaligned envelope allocation");
  if (unaligned) {
    /* Deliberately malformed Win64 ABI address; neither side dereferences a
     * misaligned typed object. The DLL's alignment check precedes prefix read. */
    memset(unaligned, 0xD3, sizeof(*out) + 8);
    prefix[0] = 1; prefix[1] = sizeof(info); memcpy(unaligned + 1, prefix, sizeof(prefix));
    memcpy(before, unaligned + 1, sizeof(*out));
    Check(Info(api, (MclaClockHistoryInfoV1*)(void*)(unaligned + 1)) == MCLA_CH_BAD_ABI &&
        memcmp(unaligned + 1, before, sizeof(*out)) == 0, "Info unaligned bytes unchanged");
    prefix[1] = sizeof(*out); memcpy(unaligned + 1, prefix, sizeof(prefix));
    memcpy(before, unaligned + 1, sizeof(*out));
    Check(Snapshot(api, (MclaClockHistorySnapshotV1*)(void*)(unaligned + 1)) == MCLA_CH_BAD_ABI &&
        memcmp(unaligned + 1, before, sizeof(*out)) == 0, "Snapshot unaligned bytes unchanged");
    free(unaligned);
  }
}
static void QueryRow(MclaClockHistoryRecordV1* row, uint64_t storage, uint64_t mutex,
    uint32_t kind, uint64_t sample, uint64_t pre_host, uint64_t pre_guest,
    uint64_t post_host, uint64_t post_guest, uint64_t num, uint64_t den) {
  row->storage_ordinal = storage; row->mutex_ordinal = mutex;
  row->kind = kind; row->ordering = MCLA_CH_ORDERING_MUTEX;
  row->metadata.host_sample = sample; row->metadata.returned_guest = post_guest;
  row->metadata.pre_host = pre_host; row->metadata.pre_guest = pre_guest;
  row->metadata.post_host = post_host; row->metadata.post_guest = post_guest;
  row->metadata.pre_ratio_numerator = num; row->metadata.pre_ratio_denominator = den;
  row->metadata.post_ratio_numerator = num; row->metadata.post_ratio_denominator = den;
  row->metadata.presence = MCLA_CH_QUERY_FIELDS; row->disposition = MCLA_CH_PUBLISHED;
}
static void Expected(MclaClockHistorySnapshotV1* expected, int stopped_before_arm) {
  Prefix(expected); expected->capacity = 4096;
  if (stopped_before_arm) return;
  expected->row_count = 8; expected->run_high = high; expected->run_low = low;
  expected->observed_attempts = 8; expected->mutex_ordinals = 7;
  expected->unordered_attempts = 1; expected->stored_entries = 8;
  expected->published_entries = 7; expected->aborted_entries = 1;
  expected->diagnostic_entries = 2;
  QueryRow(&expected->rows[0], 1, 1, MCLA_CH_KIND_ADVANCE, 101, 100, 500, 101, 505, 5, 1);
  QueryRow(&expected->rows[1], 2, 2, MCLA_CH_KIND_ADVANCE, 102, 101, 505, 102, 510, 5, 1);
  expected->rows[1].binding.run_high = high; expected->rows[1].binding.run_low = low;
  expected->rows[1].binding.timer_invocation = 7; expected->rows[1].binding.read_ordinal = 0;
  expected->rows[1].binding.phase_call_id = 11; expected->rows[1].binding.phase_thread_epoch = 13;
  expected->rows[1].binding.phase_native_thread_id = 17;
  expected->rows[1].binding.phase_present = 1; expected->rows[1].binding.present = 1;
  QueryRow(&expected->rows[2], 3, 3, MCLA_CH_KIND_CACHED, 99, 102, 510, 102, 510, 5, 1);
  QueryRow(&expected->rows[3], 4, 4, MCLA_CH_KIND_RATIO, 0, 102, 510, 102, 510, 5, 1);
  expected->rows[3].metadata.returned_guest = 0;
  expected->rows[3].metadata.post_ratio_numerator = 3; expected->rows[3].metadata.post_ratio_denominator = 2;
  expected->rows[3].metadata.presence = MCLA_CH_RATIO_FIELDS;
  expected->rows[4].storage_ordinal = 5; expected->rows[4].kind = MCLA_CH_KIND_NO_SCALING;
  expected->rows[4].ordering = MCLA_CH_ORDERING_UNORDERED;
  expected->rows[4].metadata.host_sample = 104; expected->rows[4].metadata.returned_guest = 156;
  expected->rows[4].metadata.pre_ratio_numerator = 3; expected->rows[4].metadata.pre_ratio_denominator = 2;
  expected->rows[4].metadata.presence = MCLA_CH_UNORDERED_FIELDS;
  expected->rows[4].disposition = MCLA_CH_PUBLISHED;
  QueryRow(&expected->rows[5], 6, 5, MCLA_CH_KIND_ADVANCE, 103, 100, 500, 103, 504, 3, 2);
  expected->rows[5].binding.reserved = 29; expected->rows[5].metadata.reserved = 0xAABBu;
  expected->rows[5].issues = MCLA_CH_ISSUE_INVALID_BINDING | MCLA_CH_ISSUE_NONZERO_RESERVED;
  QueryRow(&expected->rows[6], 7, 6, MCLA_CH_KIND_ADVANCE, 105, 104, 512, 105, 513, 3, 2);
  expected->rows[7].storage_ordinal = 8; expected->rows[7].mutex_ordinal = 7;
  expected->rows[7].kind = MCLA_CH_KIND_ADVANCE; expected->rows[7].ordering = MCLA_CH_ORDERING_MUTEX;
  expected->rows[7].disposition = MCLA_CH_ABORTED;
  expected->rows[7].issues = MCLA_CH_ISSUE_ABORTED | MCLA_CH_ISSUE_METADATA_UNAVAILABLE;
}
static void Pending(const Api* api, MclaClockHistorySnapshotV1* out,
                    MclaClockHistorySnapshotV1* copy) {
  Prefix(out); memcpy(copy, out, sizeof(*out));
  Check(Snapshot(api, out) == MCLA_CH_PENDING && memcmp(out, copy, sizeof(*out)) == 0,
        "pending full destination changed or false drain success");
}
static void Normal(const Api* api, MclaClockHistorySnapshotV1* out,
                   MclaClockHistorySnapshotV1* expected) {
  Check(api->fixture(3) == MCLA_CH_OK, "default-OFF bounded fixture call");
  Pending(api, out, expected);
  Check(Arm(api, 0, 0) == MCLA_CH_REJECTED, "zero run rejected without consuming Arm");
  Check(Arm(api, high, low) == MCLA_CH_OK, "first nonzero Arm");
  Check(Arm(api, high, low) == MCLA_CH_REJECTED, "second Arm rejected");
  Check(api->fixture(1) == MCLA_CH_OK, "emit eight rows and keep one admitted Call alive");
  Pending(api, out, expected);
  Stop(api); /* Must return while the SAME admitted Call is still alive. */
  Pending(api, out, expected);
  Check(api->fixture(2) == MCLA_CH_OK, "complete and destroy held Call after Stop");
  Prefix(out); Check(Snapshot(api, out) == MCLA_CH_OK, "Stop plus drained Snapshot");
  Expected(expected, 0);
  Check(out->row_count == 8 && out->observed_attempts == 8 && out->mutex_ordinals == 7 &&
      out->unordered_attempts == 1 && out->stored_entries == 8 &&
      out->published_entries == 7 && out->aborted_entries == 1 && out->diagnostic_entries == 2,
      "exact fixture raw counts");
  Check(memcmp(out, expected, sizeof(*out)) == 0,
        "all history raw fields differ from independent literal fixture oracle");
  Check(Zero(out->rows + 8, sizeof(out->rows) - 8 * sizeof(out->rows[0])),
        "unused history rows are not zero");
  memcpy(expected, out, sizeof(*out));
  Check(Snapshot(api, out) == MCLA_CH_OK && memcmp(out, expected, sizeof(*out)) == 0,
        "repeat drained Snapshot not byte stable");
  Check(api->fixture(3) == MCLA_CH_OK, "late postStop bounded fixture call");
  Check(Arm(api, low, high) == MCLA_CH_REJECTED, "no postStop reset/rearm");
  Stop(api);
  Check(Snapshot(api, out) == MCLA_CH_OK && memcmp(out, expected, sizeof(*out)) == 0,
        "late admission or repeated Stop mutated frozen rows/counts");
  Check(api->fixture(2) == MCLA_CH_REJECTED, "held lifetime already completed exactly once");
}
static void StopBeforeArm(const Api* api, MclaClockHistorySnapshotV1* out,
                          MclaClockHistorySnapshotV1* expected) {
  Stop(api);
  Check(Arm(api, high, low) == MCLA_CH_REJECTED, "cold Stop consumes lifecycle without Arm");
  Prefix(out); Check(Snapshot(api, out) == MCLA_CH_OK, "cold stopped Snapshot");
  Expected(expected, 1);
  Check(memcmp(out, expected, sizeof(*out)) == 0, "cold stopped complete snapshot not zero/empty");
  Check(api->fixture(3) == MCLA_CH_OK, "cold stopped bounded fixture call");
  Check(Snapshot(api, out) == MCLA_CH_OK && memcmp(out, expected, sizeof(*out)) == 0,
        "cold Stop late call mutated frozen inventory");
  Stop(api);
  Check(Snapshot(api, out) == MCLA_CH_OK && memcmp(out, expected, sizeof(*out)) == 0,
        "cold Stop repeat snapshot not byte stable");
}
int main(int argc, char** argv) {
  Api api;
  MclaClockHistorySnapshotV1* out = NULL;
  MclaClockHistorySnapshotV1* expected = NULL;
  int cold;
  setvbuf(stdout, NULL, _IONBF, 0);
  memset(&api, 0, sizeof(api));
  if (argc != 3 || (strcmp(argv[2], "normal") && strcmp(argv[2], "stop-before-arm"))) {
    printf("usage: consumer ABSOLUTE_DLL_PATH normal|stop-before-arm\n"); return 2;
  }
  cold = strcmp(argv[2], "stop-before-arm") == 0;
  if (!Load(argv[1], &api) || !Descriptor(&api)) goto done;
  out = (MclaClockHistorySnapshotV1*)malloc(sizeof(*out));
  expected = (MclaClockHistorySnapshotV1*)malloc(sizeof(*expected));
  Check(out != NULL && expected != NULL, "heap snapshot allocation");
  if (!out || !expected) goto done;
  InvalidEnvelopes(&api, out, expected);
  if (cold) StopBeforeArm(&api, out, expected); else Normal(&api, out, expected);
done:
  free(out); free(expected);
  printf("checks=%u failures=%u synthetic_history_FFI_only mode=%s "
         "sdk_clock_proven=false complete_history_proven=false fps_proven=false\n",
         checks, failures, cold ? "stop-before-arm" : "normal");
  return failures ? 1 : 0;
}
