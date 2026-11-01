// KI-047 diag v2 host default. See mcla_livery_diag.h.

#include "mcla_livery_diag.h"

#include "mcla_logging.h"

#include <rex/cvar.h>

#include <string>

#ifndef MCLA_LIVERY_DIAG2_DEFAULT
#define MCLA_LIVERY_DIAG2_DEFAULT 0
#endif

namespace mcla::livery_diag {

void OnGraphicsPluginLoaded() {
  const std::string cvar(kLiveryDiag2Cvar);
  const bool sdk_has_cvar = rex::cvar::GetFlagInfo(cvar) != nullptr;
  const bool user_set = sdk_has_cvar && rex::cvar::HasNonDefaultValue(cvar);
  switch (DecideDefault(MCLA_LIVERY_DIAG2_DEFAULT != 0, sdk_has_cvar, user_set)) {
    case DefaultAction::kDisabledByOption:
      break;
    case DefaultAction::kUnavailable:
      MCLA_GPU_INFO("MCLA livery diag2: {} unavailable in this SDK", cvar);
      break;
    case DefaultAction::kUserSet:
      MCLA_GPU_INFO("MCLA livery diag2: {} set by the user ({}), left alone", cvar,
                    rex::cvar::Query<bool>(cvar));
      break;
    case DefaultAction::kEnable:
      if (rex::cvar::SetFlagByName(cvar, "true")) {
        MCLA_GPU_INFO("MCLA livery diag2: build default {}=true", cvar);
      } else {
        MCLA_GPU_ERROR("MCLA livery diag2: could not set {}=true", cvar);
      }
      break;
  }
}

}  // namespace mcla::livery_diag
