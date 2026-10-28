#include "mcla_diagnostic_export_win.h"

#include <iostream>
#include <map>
#include <string>

namespace de = mcla::launcher::diagnostic_export;
namespace fs = mcla::launcher::fs;

int wmain(int argc, wchar_t **argv) {
  try {
    bool preview_only = false;
    std::map<std::wstring, std::wstring> options;
    for (int i = 1; i < argc; ++i) {
      const std::wstring name = argv[i];
      if (name == L"--preview-only" && !preview_only) { preview_only = true; continue; }
      if ((name != L"--settings-root" && name != L"--session" && name != L"--package" && name != L"--destination-parent") ||
          ++i >= argc || !options.emplace(name, argv[i]).second) {
        std::cerr << "Invalid or duplicate option.\n"; return 2;
      }
    }
    if (options.size() != 4) {
      std::cerr << "MCLA metadata export (local files only; never uploads)\n"
          "Usage: MCLA-Export-Metadata --settings-root <existing launcher settings folder>\n"
          "  --session <explicit 32-hex session ID> --package <live-package basename>\n"
          "  --destination-parent <existing separate export folder> [--preview-only]\n"
          "Reads the selected managed profile; requires a completed session.\n"
          "Supports live F10 metadata only, not crash dumps or images.\n";
      return 2;
    }
    auto settings = fs::OpenDirectory(options.at(L"--settings-root"));
    if (!settings) { std::wcerr << settings.error() << L'\n'; return 1; }
    auto bytes = fs::ReadFile(*settings, L"preferences.dat", mcla::launcher::kMaximumPreferencesBytes);
    if (!bytes) { std::wcerr << bytes.error() << L'\n'; return 1; }
    auto preferences = mcla::launcher::DecodePreferences(*bytes);
    if (!preferences) { std::cerr << "Invalid launcher preferences; nothing changed.\n"; return 1; }
    const auto &wide_id = options.at(L"--session");
    for (const auto ch : wide_id) {
      if (!((ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f'))) {
        std::cerr << "Session ID must be lowercase hexadecimal.\n"; return 2;
      }
    }
    const std::string session(wide_id.begin(), wide_id.end());
    auto plan = de::PrepareLiveExport(*preferences, *settings, session,
        options.at(L"--package"), options.at(L"--destination-parent"));
    if (!plan) { std::wcerr << plan.error() << L'\n'; return 1; }
    std::wcout << L"Fresh local destination: " << plan->destination() << L"\n\n";
    std::cout << "Exact file 1: report.json\n" << plan->report_json()
        << "\nExact file 2: EXPORT-COMPLETE.txt\n" << plan->inventory_text()
        << "\nEXCLUDED: saves, snapshots, pictures, dumps, raw logs, personal paths,\n"
        << "profile/session identifiers, timestamps and free-form diagnostic text.\n"
        << "Hardware is historical receipt metadata, not a new hardware scan.\n"
        << "No measured FPS, world-quality verdict, authenticated origin or upload.\n"
        << "Read the exact contents before sharing. Add reproduction steps separately.\n";
    if (preview_only) { std::cout << "PREVIEW ONLY: no files created.\n"; return 0; }
    std::cout << "\nType EXPORT to create these two files locally, or anything else to cancel: " << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer) || answer != "EXPORT") {
      std::cout << "Cancelled; no files created.\n"; return 0;
    }
    const auto result = de::ConfirmExport(*plan, plan->confirmation_token());
    if (!result) { std::wcerr << result.error() << L'\n'; return 1; }
    std::wcout << L"Metadata export complete: " << *result << L"\nNothing uploaded.\n";
    return 0;
  } catch (...) {
    // Do not print parser exception text or raw source content.
    std::cerr << "Metadata export failed unexpectedly; preserve any incomplete output.\n";
    return 1;
  }
}
