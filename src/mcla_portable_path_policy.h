#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace mcla::portable {
namespace detail {

// ProcessId()/std::to_string and the capture sequence emit canonical positive
// decimal values. Bound the fields without allocating or accepting overflow.
constexpr bool IsPositiveDecimal(std::string_view text, uint64_t maximum) {
  if (text.empty() || text.front() == '0') {
    return false;
  }
  uint64_t value = 0;
  for (const char character : text) {
    if (character < '0' || character > '9') {
      return false;
    }
    const uint64_t digit = static_cast<uint64_t>(character - '0');
    if (value > (maximum - digit) / 10) {
      return false;
    }
    value = value * 10 + digit;
  }
  return true;
}

} // namespace detail

// A producer-shaped unfinished F10 directory is inert diagnostic evidence,
// not an incomplete executable/save publication. This checks only its direct
// bundle-relative generic path and entry type, not whether its contents are
// complete or trusted. Callers must still traverse it and reject reparses and
// sync conflicts; nothing inside it becomes a completed capture or save seed.
constexpr bool IsUnfinishedLiveDiagnosticPartial(std::string_view relative,
                                                bool is_directory) {
  constexpr std::string_view prefix = "diagnostics/live/live-";
  constexpr std::string_view suffix = ".partial";
  if (!is_directory || relative.size() < prefix.size() + suffix.size() ||
      !relative.starts_with(prefix) || !relative.ends_with(suffix)) {
    return false;
  }
  relative.remove_prefix(prefix.size());
  relative.remove_suffix(suffix.size());
  // YYYYMMDDTHHMMSSZ-PID-SEQUENCE, matching the producer's UtcStamp shape.
  if (relative.size() < 20 || relative[8] != 'T' || relative[15] != 'Z' ||
      relative[16] != '-') {
    return false;
  }
  for (size_t index = 0; index < 16; ++index) {
    if (index != 8 && index != 15 &&
        (relative[index] < '0' || relative[index] > '9')) {
      return false;
    }
  }
  const size_t separator = relative.find('-', 17);
  if (separator == std::string_view::npos) {
    return false;
  }
  return detail::IsPositiveDecimal(relative.substr(17, separator - 17),
                                   (std::numeric_limits<uint32_t>::max)()) &&
         detail::IsPositiveDecimal(relative.substr(separator + 1),
                                   (std::numeric_limits<uint64_t>::max)());
}

} // namespace mcla::portable
