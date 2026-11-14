#include "../scripts/native/guest-capture/guest_capture_provenance.h"
#include <iostream>

int main() {
  using namespace rex::ui;
  unsigned checks = 0, failed = 0;
  auto check = [&](bool b) { ++checks; if (!b) ++failed; };
  GuestCaptureProvenance p{1, 7, 10000000, 10, 20, 19, 21, 22, 30, 31};
  auto valid = [](const auto& m) { return ValidGuestCaptureProvenance(m, 32, 16, 128, 2048); };
  check(valid(p));
  auto q = p; q.acquire_before_qpc = 5; check(valid(q));
  q = p; q.refresh_cpu_end_qpc = q.acquire_after_qpc; check(valid(q));
  q = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; check(valid(q));
  for (auto field : {&GuestCaptureProvenance::presenter_epoch,
                     &GuestCaptureProvenance::acquired_sequence,
                     &GuestCaptureProvenance::qpc_frequency,
                     &GuestCaptureProvenance::refresh_cpu_begin_qpc,
                     &GuestCaptureProvenance::acquire_before_qpc}) {
    q = p; q.*field = 0; check(!valid(q));
  }
  q = p; q.refresh_cpu_begin_qpc = 21; check(!valid(q));
  q = p; q.refresh_cpu_end_qpc = 22; check(!valid(q));
  q = p; q.acquire_before_qpc = 22; check(!valid(q));
  q = p; q.acquire_after_qpc = 23; check(!valid(q));
  q = p; q.copy_submit_before_qpc = 31; check(!valid(q));
  q = p; q.copy_completion_observed_qpc = 32; check(!valid(q));
  q = {1, 1, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX,
       UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX}; check(!valid(q));
  q = p; q.qpc_frequency = uint64_t(INT64_MAX) + 1; check(!valid(q));
  for (auto n : {size_t(0), size_t(2047), size_t(2049), SIZE_MAX})
    check(!ValidGuestCaptureProvenance(p, 32, 16, 128, n));
  check(!ValidGuestCaptureProvenance(p, 0, 16, 0, 0));
  check(!ValidGuestCaptureProvenance(p, 32, 0, 128, 0));
  check(!ValidGuestCaptureProvenance(p, 16385, 16, 65540, 1048640));
  check(!ValidGuestCaptureProvenance(p, 32, 16385, 128, 2097280));
  check(!ValidGuestCaptureProvenance(p, 32, 16, 129, 2064));
  check(ValidGuestCaptureProvenance(p, 4096, 4096, 16384, 64u * 1024u * 1024u));
  check(!ValidGuestCaptureProvenance(p, 4096, 4097, 16384, 4097u * 16384u));
  for (unsigned mask = 0; mask != 64; ++mask) {
    GuestCaptureSubmissionChecks s{bool(mask & 1), bool(mask & 2), bool(mask & 4),
                                   bool(mask & 8), bool(mask & 16), bool(mask & 32)};
    check(s.valid() == (mask == 63));
  }
  for (unsigned mask = 0; mask != 8; ++mask)
    for (auto done : {uint64_t(0), uint64_t(8), uint64_t(9), uint64_t(10), UINT64_MAX})
      check(ValidGuestCaptureFence(9, done, mask & 1, mask & 2, mask & 4) ==
            (mask == 7 && done == 9));
  check(!ValidGuestCaptureFence(0, 0, true, true, true));
  check(!ValidGuestCaptureFence(UINT64_MAX, UINT64_MAX, true, true, true));
  // Same identity may legitimately be returned twice; no cadence inference.
  check(valid(p) && valid(p));
  std::cout << "guest capture metadata CPU checks=" << checks << " failed=" << failed << '\n';
  return failed ? 1 : 0;
}
