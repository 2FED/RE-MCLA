// mcla - out-of-process native crash capture interface

#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

namespace mcla::diagnostics::native {

bool Start(const std::filesystem::path &diagnostics_root,
           const std::filesystem::path &user_data_root,
           const std::filesystem::path &journal_path, std::string_view version,
           bool show_reporter_dialog, std::string &error);

bool WriteLiveMiniDump(const std::filesystem::path &path,
                       std::chrono::milliseconds timeout, std::string &error);

// Runtime startup (notably Tracy in instrumented builds) may install its own
// top-level filter. Reassert MCLA-R ownership after Runtime::Setup completes.
void RefreshCrashHandlers();

void Stop();

[[noreturn]] void TriggerCrashProbe();

} // namespace mcla::diagnostics::native
