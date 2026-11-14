// Narrow paired-SDK Install diagnostic, not a game/native integration gate.
// Including the exact controller TU here deliberately tests one consumer TU;
// separate production object linkage and every other lifecycle remain pending.
#include "../src/mcla_clock_game_bridge_win.cpp"
#include <iostream>

int main() {
  const auto named = ::GetModuleHandleW(L"rexruntime.dll");
  const auto executable = ::GetModuleHandleW(nullptr);
  HMODULE cpp_owner{}, export_owner{};
  const bool cpp_resolved = ::GetModuleHandleExW(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(&rex::chrono::Clock::QueryGuestTickCount), &cpp_owner) != 0;
  const auto exported = named ? ::GetProcAddress(named,
      "?QueryGuestTickCount@Clock@chrono@rex@@SA_KXZ") : nullptr;
  const bool export_resolved = exported && ::GetModuleHandleExW(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      reinterpret_cast<LPCWSTR>(exported), &export_owner) != 0;
  if (named) {
    const auto path = mcla::clock_game::ModulePath(named);
    // This diagnostic recipe uses an explicitly ASCII workspace root. This
    // display conversion is NOT a general Unicode module-path contract.
    std::cout << "OWNER_LOADED_DLL " << std::string(path.begin(),path.end()) << '\n';
  }
  std::cout << "OWNER named=" << bool(named) << " cpp_resolved=" << cpp_resolved
      << " cpp_is_exe=" << (cpp_owner == executable) << " cpp_is_sdk=" << (cpp_owner == named)
      << " export_resolved=" << export_resolved << " export_is_sdk=" << (export_owner == named)
      << " cpp_equals_export="
      << (reinterpret_cast<uintptr_t>(&rex::chrono::Clock::QueryGuestTickCount) ==
          reinterpret_cast<uintptr_t>(exported)) << '\n';
  bool installed = false;
  try { mcla::clock_game::Install(); installed = true; }
  catch (const std::exception& error) { std::cout << "INSTALL_REJECT " << error.what() << '\n'; }
  catch (...) { std::cout << "INSTALL_REJECT unknown_exception\n"; }
  const bool metadata = named && cpp_resolved && export_resolved && export_owner == named;
  std::cout << "checks=2 failures=" << unsigned(!metadata) + unsigned(!installed)
      << " paired_owner_install_diagnostic_only no_game_no_runtime_admission_no_explicit_guest_clock_query\n";
  return metadata && installed ? 0 : 1;
}
