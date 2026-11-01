#pragma once

// KI-047 diag v2 host default (owner-internal l54;
// docs/evidence/M10-001-ki047-diag2-build-20261101.md):
//
// - MCLA_LIVERY_DIAG2_DEFAULT builds set the SDK cvar d3d12_livery_diag2=true
//   at graphics-plugin load unless the user set it (log once), so every run
//   of that build logs MCLA_LIVERY_DIAG2 lines.
// - Without the option, or with an SDK that has no such cvar (the pinned
//   volunteer SDK), nothing is written.
//
// The decision is pure and unit-tested (tests/mcla_livery_diag_test.cpp); the
// cvar calls are in mcla_livery_diag.cpp.

#include <string_view>

namespace mcla::livery_diag {

inline constexpr std::string_view kLiveryDiag2Cvar = "d3d12_livery_diag2";

enum class DefaultAction {
  kDisabledByOption,  // build without MCLA_LIVERY_DIAG2_DEFAULT
  kUnavailable,       // SDK without d3d12_livery_diag2
  kUserSet,           // the user set the cvar; left alone
  kEnable,            // write d3d12_livery_diag2=true
};

constexpr DefaultAction DecideDefault(bool option_on, bool sdk_has_cvar, bool user_set) {
  if (!option_on) {
    return DefaultAction::kDisabledByOption;
  }
  if (!sdk_has_cvar) {
    return DefaultAction::kUnavailable;
  }
  return user_set ? DefaultAction::kUserSet : DefaultAction::kEnable;
}

// Applies DecideDefault at graphics-plugin load. Safe to call once.
void OnGraphicsPluginLoaded();

}  // namespace mcla::livery_diag
