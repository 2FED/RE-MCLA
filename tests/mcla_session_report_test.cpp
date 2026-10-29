#include "mcla_session_report.h"
#include "mcla_diagnostic_trigger.h"

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
  Check(text.find("  crash/crash-20261028T154000Z-29484-1  crash\n  live/live-20261028T153436Z-29484-1  general\n") !=
            std::string::npos,
        "report lists packages with their category, crash first");
  Check(text.find("\npackages (general = bug report, wheel = wheel/force-feedback bug, crash = crash)\n") !=
            std::string::npos,
        "report explains the categories");

  // M9-010: category from the live package manifest.
  const std::string f10_manifest =
      "{\n  \"schema\": \"mcla-diagnostic-package-v1\",\n  \"kind\": \"live\",\n  \"reason\": \"f10\",\n"
      "  \"trigger\": {\"category\": \"general\", \"source\": \"f10\"},\n  \"capture\": {\"frame\": true}\n}\n";
  const std::string wheel_manifest =
      "{\n  \"kind\": \"live\",\n  \"reason\": \"wheel-l2\",\n"
      "  \"trigger\": {\"category\": \"wheel\", \"source\": \"wheel-l2\"},\n  \"artifacts\": []\n}\n";
  Check(ManifestCategory(f10_manifest) == "general", "general manifest category");
  Check(ManifestCategory(wheel_manifest) == "wheel", "wheel manifest category");
  Check(ManifestCategory("{\"kind\": \"live\", \"reason\": \"f10\"}") == "general",
        "pre-M9-010 package without trigger is general");
  Check(ManifestCategory("") == "general", "unreadable manifest is general");
  Check(ManifestCategory("{\"trigger\": {\"category\": \"crash\", \"source\": \"x\"}}") == "general",
        "unknown category is not trusted");
  Check(ManifestCategory("{\"trigger\": {\"source\": \"x\"}, \"other\": {\"category\": \"wheel\"}}") == "general",
        "category outside the trigger object is ignored");
  Check(ManifestCategory("{\"trigger\": {\"category\":\"wheel\"}}") == "wheel", "compact spacing accepted");

  auto mixed = SessionPackages({L"live-20261029T100000Z-1-1", L"live-20261029T100100Z-1-2",
                                L"live-20261029T100200Z-1-3"},
                               {});
  Check(mixed.size() == 3, "launcher still counts every completed live package");
  mixed[0].category = ManifestCategory(f10_manifest);
  mixed[1].category = ManifestCategory(wheel_manifest);
  const auto mixed_text = ReportText(context, mixed);
  Check(mixed_text.find("  live/live-20261029T100000Z-1-1  general\n"
                        "  live/live-20261029T100100Z-1-2  wheel\n"
                        "  live/live-20261029T100200Z-1-3  general\n") != std::string::npos,
        "report marks the wheel package");
  Check(PromptText(mixed).starts_with(L"3 reports were recorded in this session (1 wheel report)."),
        "prompt counts wheel reports");
  auto with_crash = packages;
  with_crash[1].category = "wheel";
  Check(PromptText(with_crash).starts_with(L"3 reports were recorded in this session (1 crash, 1 wheel report)."),
        "prompt counts crashes and wheel reports");
  Package odd{L"crash-1", true, "wheel"};
  Check(PackageCategory(odd) == "crash", "crash package is always crash");
  for (const auto *question : {"1. What were you doing?", "2. Steps to make it happen again", "3. What went wrong?",
                               "4. What did you expect instead?"})
    Check(text.find(question) != std::string::npos, "report asks the four questions");
  Check(text.find('\r') == std::string::npos && text.find(":\\") == std::string::npos,
        "LF only, no absolute paths");

  // Per-package report.txt: the game writes it with a pending line, the
  // launcher fills only that line; tester text without the line is kept.
  Check(prompt.find(L"Each report folder") != std::wstring::npos &&
        prompt.find(L"has its own report.txt") != std::wstring::npos,
        "prompt says each report folder has its own report.txt");
  Check(text.find("its own report.txt") != std::string::npos, "session index points to the per-package files");
  Check(kPendingSessionLine == mcla::diagnostics::kPackageReportPendingLine,
        "launcher and game agree on the pending line");
  mcla::diagnostics::PackageReportFacts facts;
  facts.package = "live-20261029T120000Z-1-1"; facts.kind = "live"; facts.category = "wheel";
  facts.source = "wheel-l2"; facts.wheel = "unknown";
  const auto host = mcla::diagnostics::BuildPackageReportText(facts);
  const auto filled = FillPackageReport(host, context);
  Check(filled && filled->find(std::string(kPendingSessionLine)) == std::string::npos &&
        filled->find("\nwindow unknown\nbuild " + std::string(64, 'A') +
                     "\nmode unlocked\nrender_target 1280x720\nsession_started_utc 1793200000\n"
                     "session_ended_utc 1793200600\nexit_code 0\n\n1. What were you doing?\n") != std::string::npos,
        "launcher replaces only the pending line with the session facts");
  const auto pending_at = host.find(std::string(kPendingSessionLine));
  Check(filled && pending_at != std::string::npos && filled->starts_with(host.substr(0, pending_at)) &&
        filled->ends_with(host.substr(pending_at + kPendingSessionLine.size())),
        "text around the pending line is unchanged");
  Check(!FillPackageReport(*filled, context), "a filled report is never filled twice");
  Check(!FillPackageReport("MCLA-R TEST REPORT\n\n1. What were you doing?\nDriving\n", context),
        "tester text without the pending line is left alone");
  const std::string twice = "\n" + std::string(kPendingSessionLine) + "\n" + std::string(kPendingSessionLine) + "\n";
  Check(!FillPackageReport(twice, context), "an ambiguous file is left alone");
  const auto fallback = PackageReportText(context, Package{L"crash-20261029T1-9", true});
  Check(fallback.starts_with("MCLA-R TEST REPORT\n") &&
        fallback.find("\npackage crash-20261029T1-9\nkind crash\ncategory crash\nbuild ") != std::string::npos &&
        fallback.find("4. What did you expect instead?") != std::string::npos &&
        !FillPackageReport(fallback, context),
        "fallback package report carries facts and the questions");
  std::cout << "Session report: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
