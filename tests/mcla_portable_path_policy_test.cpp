#include "mcla_portable_path_policy.h"

#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>

namespace {
using mcla::portable::IsUnfinishedLiveDiagnosticPartial;

unsigned checks = 0;
unsigned failures = 0;

void Check(std::string_view label, std::string_view relative,
           bool is_directory, bool expected) {
  ++checks;
  if (IsUnfinishedLiveDiagnosticPartial(relative, is_directory) != expected) {
    ++failures;
    std::cerr << "FAIL " << label << '\n';
  }
}
} // namespace

int main() {
  constexpr std::string_view name = "live-20260905T151532Z-336-5.partial";
  const std::string valid = "diagnostics/live/" + std::string(name);
  Check("unfinished F10 directory", valid, true, true);
  Check("minimum positive identifiers",
        "diagnostics/live/live-20260905T151532Z-1-1.partial", true, true);
  Check("maximum producer identifiers",
        "diagnostics/live/live-20260905T151532Z-4294967295-"
        "18446744073709551615.partial", true, true);
  Check("same name is not an allowed regular file", valid, false, false);
  Check("empty path", "", true, false);
  Check("root partial", "stale.partial", true, false);
  Check("no diagnostics root", name, true, false);
  for (const std::string_view root : {"bin/", "game/", "user/", "results/",
                                      "cache/", "diagnostics/",
                                      "diagnostics/crashes/"}) {
    Check("wrong parent", std::string(root) + std::string(name), true, false);
  }
  Check("session partial is not a live diagnostic",
        "results/session-20260905T151245Z-296.partial", true, false);
  Check("relative escape", "../" + valid, true, false);
  Check("absolute path", "/" + valid, true, false);
  Check("Windows rooted path", "C:/" + valid, true, false);
  Check("noncanonical separator",
        "diagnostics\\live\\live-20260905T151532Z-336-5.partial", true, false);
  Check("dot component", "diagnostics/live/./" + std::string(name), true,
        false);
  Check("parent component", "diagnostics/live/../live/" + std::string(name),
        true, false);
  Check("nested diagnostic", "diagnostics/live/other/" + std::string(name),
        true, false);
  Check("nested partial inside valid package", valid + "/nested.partial",
        true, false);
  Check("nested producer-shaped partial", valid + "/" + std::string(name),
        true, false);
  Check("nested partial inside complete package",
        "diagnostics/live/live-20260905T151532Z-336-5/nested.partial", true,
        false);
  Check("trailing separator", valid + "/", true, false);
  Check("complete directory is not an exception",
        "diagnostics/live/live-20260905T151532Z-336-5", true, false);
  Check("double partial suffix", valid + ".partial", true, false);
  for (const std::string_view body : {
           "", "20260905T151532Z", "20260905T151532Z-336",
           "20260905T151532Z--5", "20260905T151532Z-336-",
           "20260905T151532Z-0-5", "20260905T151532Z-336-0",
           "20260905T151532Z-0336-5", "20260905T151532Z-336-05",
           "20260905T151532Z-4294967296-5",
           "20260905T151532Z-336-18446744073709551616",
           "20260905T151532Z-+336-5", "20260905T151532Z-336--5",
           "20260905T151532Z-33x-5", "20260905T151532Z-336-5x",
           "20260905T151532Z-336-5-6", "20260905T151532Z-336-5/6",
           "20260905T151532Z-336-5\\6", "20260905t151532Z-336-5",
           "20260905T151532z-336-5", "20260905T15153Z-336-5",
           "20260905T1515320Z-336-5", "202x0905T151532Z-336-5",
           "20260905T151x32Z-336-5"}) {
    Check("malformed producer name",
          "diagnostics/live/live-" + std::string(body) + ".partial", true,
          false);
  }
  std::cout << "Portable path policy: " << checks << " checks, " << failures
            << " failures\n";
  return failures ? 1 : 0;
}
