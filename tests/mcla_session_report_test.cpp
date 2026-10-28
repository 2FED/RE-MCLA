#include "mcla_session_report.h"

#include <iostream>

namespace {
using namespace mcla::launcher::report;
unsigned checks = 0, failures = 0;
void Check(bool ok, const char *label) {
  ++checks;
  if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
} // namespace

int main() {
  Check(SessionPackages({}, {}).empty(), "no packages, no prompt");
  const auto packages = SessionPackages(
      {L"live-20261028T153729Z-29484-2", L"live-20261028T153436Z-29484-1", L"live-20261028T153924Z-29484-3.partial"},
      {L"crash-20261028T154000Z-29484-1", L"crash-20261028T154100Z-29484-2.partial"});
  Check(packages.size() == 3, "unfinished .partial packages are skipped");
  Check(packages.size() == 3 && packages[0] == Package{L"crash-20261028T154000Z-29484-1", true} &&
        packages[1] == Package{L"live-20261028T153436Z-29484-1", false} &&
        packages[2] == Package{L"live-20261028T153729Z-29484-2", false},
        "crash first, then live packages in name order");
  Check(SessionPackages({L"", L"..\\escape", L"name with space", L"x/y", std::wstring(129, L'a')}, {}).empty(),
        "empty, traversal, spaced, slashed and oversized names are skipped");
  std::vector<std::wstring> many;
  for (int i = 0; i < 100; ++i) many.push_back(L"live-" + std::to_wstring(1000 + i));
  Check(SessionPackages(many, {}).size() == kMaximumPackages, "package list is bounded");

  const auto prompt = PromptText(packages);
  Check(prompt.starts_with(L"3 reports were recorded in this session (1 crash)."), "prompt counts reports and crashes");
  Check(prompt.find(L"Nothing is uploaded") != std::wstring::npos, "prompt states that nothing is uploaded");
  Check(PromptText({{L"live-1", false}}).starts_with(L"1 report was recorded in this session."),
        "singular wording without a crash note");

  const Context context{std::string(64, 'A'), "unlocked", "1280x720", 1793200000, 1793200600, 0};
  const auto text = ReportText(context, packages);
  Check(text.starts_with("MCLA-R TEST REPORT\n"), "report header");
  Check(text.find("\nbuild " + std::string(64, 'A') + "\nmode unlocked\nrender_target 1280x720\n") != std::string::npos,
        "report carries build, mode and resolution");
  Check(text.find("\nsession_started_utc 1793200000\nsession_ended_utc 1793200600\nexit_code 0\n") != std::string::npos,
        "report carries session times and exit code");
  Check(text.find("  crash/crash-20261028T154000Z-29484-1\n  live/live-20261028T153436Z-29484-1\n") != std::string::npos,
        "report lists packages, crash first");
  for (const auto *question : {"1. What were you doing?", "2. Steps to make it happen again", "3. What went wrong?",
                               "4. What did you expect instead?"})
    Check(text.find(question) != std::string::npos, "report asks the four questions");
  Check(text.find('\r') == std::string::npos && text.find(":\\") == std::string::npos,
        "LF only, no absolute paths");
  std::cout << "Session report: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
