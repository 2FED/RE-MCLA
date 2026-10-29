#pragma once

// Post-session report prompt (M9-005). Pure policy: which diagnostic packages
// a finished session produced, the prompt text, and the prefilled report.txt.
// No filesystem, clock or UI access here; nothing is sent anywhere.

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::launcher::report {

inline constexpr std::wstring_view kReportFile = L"report.txt";
inline constexpr size_t kMaximumPackages = 64;

struct Package {
  std::wstring name;
  bool crash = false;
  // Live packages: "general" or "wheel" from manifest.json (M9-010); empty
  // means not read yet and is reported as "general".
  std::string category;
  bool operator==(const Package &) const = default;
};

// The live-package category recorded by the host in manifest.json
// ("trigger": {"category": "...", ...}). Packages written before M9-010 have
// no trigger and are general bug reports; unknown values are not trusted.
inline std::string ManifestCategory(std::string_view manifest) {
  const auto trigger = manifest.find("\"trigger\"");
  if (trigger == std::string_view::npos) return "general";
  const auto close = manifest.find('}', trigger);
  const auto key = manifest.find("\"category\"", trigger);
  if (key == std::string_view::npos || close == std::string_view::npos || key > close) return "general";
  const auto colon = manifest.find(':', key + 10);
  const auto open = colon == std::string_view::npos ? colon : manifest.find('"', colon);
  if (open == std::string_view::npos || open > close) return "general";
  const auto end = manifest.find('"', open + 1);
  if (end == std::string_view::npos || end > close) return "general";
  return manifest.substr(open + 1, end - open - 1) == "wheel" ? "wheel" : "general";
}

inline std::string PackageCategory(const Package &package) {
  if (package.crash) return "crash";
  return package.category == "wheel" ? "wheel" : "general";
}

// Package names are the immediate child directories of <session>/crash and
// <session>/live. Unfinished ".partial" packages and unexpected names are
// skipped. Crashes come first (a crash usually ended the session), then by name.
inline std::vector<Package> SessionPackages(const std::vector<std::wstring> &live,
                                            const std::vector<std::wstring> &crash) {
  const auto usable = [](const std::wstring &name) {
    if (name.empty() || name.size() > 128 || name.ends_with(L".partial")) return false;
    return std::all_of(name.begin(), name.end(), [](wchar_t c) {
      return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
             c == L'-' || c == L'_' || c == L'.';
    });
  };
  std::vector<Package> result;
  for (const auto *names : {&crash, &live})
    for (const auto &name : *names)
      if (usable(name)) result.push_back({name, names == &crash});
  std::sort(result.begin(), result.end(), [](const Package &a, const Package &b) {
    return a.crash != b.crash ? a.crash : a.name < b.name;
  });
  if (result.size() > kMaximumPackages) result.resize(kMaximumPackages);
  return result;
}

inline std::wstring PromptText(const std::vector<Package> &packages) {
  const auto crashes = std::count_if(packages.begin(), packages.end(), [](const Package &p) { return p.crash; });
  std::wstring text = std::to_wstring(packages.size()) +
      (packages.size() == 1 ? L" report was" : L" reports were") + L" recorded in this session";
  const auto wheels = std::count_if(packages.begin(), packages.end(),
                                    [](const Package &p) { return PackageCategory(p) == "wheel"; });
  if (crashes || wheels) {
    text += L" (";
    if (crashes) text += std::to_wstring(crashes) + (crashes == 1 ? L" crash" : L" crashes");
    if (crashes && wheels) text += L", ";
    if (wheels) text += std::to_wstring(wheels) + (wheels == 1 ? L" wheel report" : L" wheel reports");
    text += L")";
  }
  text += L".\n\nOpen the folder now? Fill in report.txt there, then send the whole folder "
          L"to the owner by hand. Nothing is uploaded automatically.";
  return text;
}

// Launcher-known facts only: no paths, profile names or hardware identifiers.
struct Context {
  std::string build;          // 64 uppercase hex build fingerprint
  std::string timing;         // "stock" or "unlocked"
  std::string render_target;  // e.g. "1280x720"
  uint64_t started_utc = 0;
  uint64_t ended_utc = 0;
  int64_t exit_code = 0;
};

inline std::string ReportText(const Context &context, const std::vector<Package> &packages) {
  std::string text = "MCLA-R TEST REPORT\n\n"
      "Written by the launcher after the session. Please answer the questions below,\n"
      "save this file, and send the whole folder to the owner. Nothing is uploaded.\n\n"
      "build " + context.build + "\nmode " + context.timing + "\nrender_target " + context.render_target +
      "\nsession_started_utc " + std::to_string(context.started_utc) +
      "\nsession_ended_utc " + std::to_string(context.ended_utc) +
      "\nexit_code " + std::to_string(context.exit_code) +
      "\npackages (general = bug report, wheel = wheel/force-feedback bug, crash = crash)\n";
  for (const auto &package : packages)
    text += std::string(package.crash ? "  crash/" : "  live/") +
            std::string(package.name.begin(), package.name.end()) + "  " + PackageCategory(package) + "\n";
  text += "\n1. What were you doing?\n\n\n"
          "2. Steps to make it happen again (if you know):\n\n\n"
          "3. What went wrong?\n\n\n"
          "4. What did you expect instead?\n\n";
  return text;
}

} // namespace mcla::launcher::report
