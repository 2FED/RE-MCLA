#include "mcla_motion_config_policy.h"
#include <cstdio>
#include <stdexcept>
#include <string>

int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool value) { ++checks; if (!value) ++failures; };
  for (unsigned bits = 0; bits < 64; ++bits) {
    check(mcla::motion_config::Admitted(bits & 1, bits & 2, bits & 4,
          bits & 8, bits & 16, bits & 32) == (bits == 7));
  }
  // Full standalone matrix including explicit compile support. Old callers
  // above cannot accidentally opt in merely because a raw flag exists.
  for (unsigned bits = 0; bits < 256; ++bits) {
    const bool prerequisites = (bits & 7) == 7;
    const bool stationary = (bits & (16 | 32)) != 0;
    const bool raw = (bits & 8) != 0, fixed = (bits & 64) != 0;
    const bool compiled_raw = (bits & 128) != 0;
    const bool expected = prerequisites && !stationary &&
        (!raw || (compiled_raw && !fixed));
    check(mcla::motion_config::Admitted(bits & 1, bits & 2, bits & 4,
          raw, bits & 16, bits & 32, fixed, compiled_raw) == expected);
  }
  for (int nav = 0; nav < 3; ++nav)
    for (int motion = 0; motion < 3; ++motion)
      for (bool cut_throw : {false, true}) {
        std::string order;
        const auto result = mcla::motion_config::Run(
            [&] { order += 'N'; if (nav == 2) throw std::runtime_error("nav"); return nav == 1; },
            [&] { order += 'M'; if (motion == 2) throw std::runtime_error("motion"); return motion == 1; },
            [&] { order += 'C'; if (cut_throw) throw std::runtime_error("cut"); },
            [&]() noexcept { order += 'D'; },
            [&]() noexcept { order += 'Z'; });
        const bool phases_ok = nav == 1 && motion == 1;
        check(order == std::string(nav == 1 ? "NM" : "N") +
            (phases_ok ? "" : "Z") + "C" + (cut_throw ? "Z" : "") + "D");
        check(result.navigation == (nav == 1));
        check(result.motion == (nav == 1 && motion == 1));
        check(result.cut == !cut_throw);
        check((result.failure[0] == 0) == (nav == 1 && motion == 1 && !cut_throw));
      }
  std::printf("motion config policy: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
