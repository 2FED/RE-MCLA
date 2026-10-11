#include "mcla_build_lease_network_win.h"
#include <iostream>
#include <limits>

int main() {
  using namespace mcla::lease;
  unsigned checks = 0, failures = 0;
  const auto check = [&](bool pass, const char *label) {
    ++checks;
    if (!pass) { ++failures; std::cerr << "FAIL " << label << '\n'; }
  };
  check(ValidateHttpBody(200, 3, "abc", true).has_value(), "exact completed body");
  check(ValidateHttpBody(200, std::nullopt, "abc", true).has_value(), "chunked successful EOF");
  check(!ValidateHttpBody(200, 3, "abc", false), "interrupt is not completion even with expected byte count");
  check(!ValidateHttpBody(200, std::nullopt, "abc", false), "chunked interruption rejected");
  for (const unsigned status : {0u, 100u, 204u, 206u, 301u, 302u, 307u, 308u, 401u, 403u, 404u, 500u})
    check(!ValidateHttpBody(status, 3, "abc", true), "non200 status rejected");
  check(!ValidateHttpBody(200, 4, "abc", true), "truncated body rejected");
  check(!ValidateHttpBody(200, 2, "abc", true), "overlong body rejected");
  check(!ValidateHttpBody(200, 0, "", true), "empty declared body rejected");
  check(!ValidateHttpBody(200, std::nullopt, "", true), "empty chunked body rejected");
  const std::string maximum(kMaximumPublicLeaseBytes, 'x');
  check(ValidateHttpBody(200, maximum.size(), maximum, true).has_value(), "exact65536 boundary accepted");
  check(ValidateHttpBody(200, std::nullopt, maximum, true).has_value(), "chunked boundary accepted");
  check(!ValidateHttpBody(200, std::nullopt, maximum + 'x', true), "chunked oversized body rejected");
  check(!ValidateHttpBody(200, kMaximumPublicLeaseBytes + 1, maximum, true), "oversized declared length rejected");
  check(!ValidateHttpBody(200, std::numeric_limits<uint64_t>::max(), "abc", true), "huge declared length rejected");
  check(kPublicLeaseDeadlineMs == 5000, "single total deadline fixed to five seconds");
  std::cout << "Public lease transport validation: " << checks << " checks, " << failures << " failures; no network request executed\n";
  return failures ? 1 : 0;
}
