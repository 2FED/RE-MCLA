#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcla::launcher {

// CommandLineToArgvW / Microsoft CRT compatible quoting, including empty
// arguments, embedded quotes and trailing backslashes. No command shell used.
std::wstring QuoteWindowsArgument(std::wstring_view argument);

using EnvironmentEntry = std::pair<std::wstring, std::wstring>;
// Pure filtering seam. Only documented personal/system directory variables
// survive; PATH/SystemRoot/WINDIR come from the supplied trusted OS directories.
std::expected<std::vector<wchar_t>, std::wstring> MakeOwnedEnvironment(
    std::span<const EnvironmentEntry> source, std::wstring_view windows_directory,
    std::wstring_view system_directory);
// Reads the allowed environment entries, never modifies the parent environment.
std::expected<std::vector<wchar_t>, std::wstring> BuildOwnedEnvironment();

struct ProcessStatus {
  std::optional<uint32_t> exit_code;
  uint32_t active_job_processes = 0;
};

// Single ownership of an exact created process and its non-breakaway job.
// Keep this object (and the separate profile lock) alive while the child runs.
// Destruction terminates only this owned job and bounds its final root wait.
class OwnedProcess {
public:
  OwnedProcess() noexcept;
  ~OwnedProcess();
  OwnedProcess(OwnedProcess &&) noexcept;
  OwnedProcess &operator=(OwnedProcess &&) noexcept;
  OwnedProcess(const OwnedProcess &) = delete;
  OwnedProcess &operator=(const OwnedProcess &) = delete;

  [[nodiscard]] uint32_t ProcessId() const noexcept;
  std::expected<ProcessStatus, std::wstring> Poll() const;
  std::expected<void, std::wstring> TerminateAndWait(uint32_t timeout_ms = 5000);
#if defined(MCLA_LAUNCHER_PROCESS_TEST_SEAM)
  std::expected<std::vector<uint32_t>, std::wstring> JobProcessIdsForTest() const;
#endif

private:
  struct State;
  std::unique_ptr<State> state_;
  friend std::expected<OwnedProcess, std::wstring> StartOwnedProcess(
      const std::filesystem::path &, const std::vector<std::wstring> &,
      const std::filesystem::path &);
};

// Caller owns canonical root validation, binary identity, settings validation
// and the profile writer lock. This function does not grant game/save admission.
// No inherited handles or stdout/stderr capture in this initial interface.
std::expected<OwnedProcess, std::wstring> StartOwnedProcess(
    const std::filesystem::path &executable,
    const std::vector<std::wstring> &arguments,
    const std::filesystem::path &working_directory);

} // namespace mcla::launcher
