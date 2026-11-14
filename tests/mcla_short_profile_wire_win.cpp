// SYNTHETIC WIRE FIXTURE ONLY. No Run, game, GPU, window, capture API or file IO.
// Include actual scout serializer/scheduler once, with ONLY
// MCLA_GUEST_SOURCE_SCOUT_PROBE=1. Link the same dependencies as the existing
// mcla_guest_source_scout_win_test. Do not compile scout.cpp separately.
// CLI: <legacy91_10hz|short31_10hz> [--incomplete]
// Stdout: one unmodified native Json serialization. All purported image-file
// metadata below is SYNTHETIC; no files are written. The caller supplies its own
// synthetic BMP bytes for a decoder cross-language test, never runtime evidence.
// RGB pixels are uniformly (17,34,51), X=0, at1280x720, stride5120. A matching
// canonical24bit BMP has BGR bytes(51,34,17), bottom-up rows, total2764854bytes.
// Incomplete mode records2 successes, one API_FAILED slot, then terminal suffix.
#include "../src/mcla_guest_source_scout_win.cpp"
#include <iostream>

int main(int argc, char** argv) {
  namespace scout = mcla::guest_source_scout;
  if (argc < 2 || argc > 3 || (argc == 3 && std::string_view(argv[2]) != "--incomplete")) {
    std::cerr << "Expected finite profile name and optional --incomplete\n";
    return 2;
  }
  try {
    const auto profile = scout::ParseProfile(argv[1]);
    const auto count = scout::detail::SlotCount(profile);
    const bool incomplete = argc == 3;
    constexpr uint64_t frequency = 10000, readiness = 100000;
    constexpr uint32_t width = 1280, height = 720;
    constexpr uint64_t bmp_bytes = 54 + uint64_t(width) * height * 3;
    constexpr char token[] = "0123456789abcdef0123456789abcdef";
    uint64_t now = readiness;
    size_t captures = 0, waits = 0;
    auto sequence = scout::detail::Collect({}, frequency, readiness, width, height, {
        [&] { return now; },
        [&](uint64_t due, std::stop_token) { ++waits; now = std::max(now, due); },
        [&](rex::ui::GuestOutputCapture& output) {
          const auto before = now;
          ++captures;
          now += 50;
          if (incomplete && captures == 3) return rex::ui::GuestCaptureStatus::kFailed;
          output.image.width = width;
          output.image.height = height;
          output.image.stride = width * 4;
          output.image.data.resize(size_t(width) * height * 4);
          for (size_t i = 0; i < output.image.data.size(); i += 4) {
            output.image.data[i] = 17;
            output.image.data[i + 1] = 34;
            output.image.data[i + 2] = 51;
            output.image.data[i + 3] = 0;
          }
          output.provenance = {7, 60 + captures, frequency, before - 50, before - 40,
                              before, before + 10, before + 20, before + 30, before + 40};
          return rex::ui::GuestCaptureStatus::kSuccess;
        }}, profile);
    const auto wanted_calls = incomplete ? size_t(3) : count;
    scout::Require(captures == wanted_calls && waits == wanted_calls, "SYNTHETIC_CALL_COUNT");
    scout::detail::ValidateBufferedRgb(sequence, profile);
    size_t collected = 0;
    uint64_t retained = 0;
    for (size_t i = 0; i < count; ++i) {
      auto& slot = sequence.slots[i];
      if (!slot.has_capture) continue;
      ++collected;
      retained += slot.paired.image.data.capacity();
      slot.image_state = "complete";  // Synthetic declaration, not a filesystem action.
      slot.image_bytes = bmp_bytes;
    }
    scout::Require(collected == (incomplete ? size_t(2) : count) &&
        retained <= scout::detail::kBufferLimitBytes, "SYNTHETIC_RETENTION_BOUND");
    const auto status = incomplete ? "SOURCE_SCOUT_CENSUS_INCOMPLETE_REVIEW_PENDING" :
                                    "SOURCE_SCOUT_CENSUS_REVIEW_PENDING";
    const auto receipt = scout::Json(token, status,
        "FIXED_DELAY_AND_SLOTS_NO_DISPLAY_SIMULATION_OR_TIMING_CLAIM",
        60, 0, readiness, &sequence, profile);
    scout::Require(receipt.size() <= 128 * 1024, "SYNTHETIC_WIRE_BOUND");
    std::cout << receipt;
    return std::cout.good() ? 0 : 2;
  } catch (const std::exception& error) {
    std::cerr << "Synthetic wire fixture rejected: " << error.what() << '\n';
    return 2;
  }
}
