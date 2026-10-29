#pragma once
#include "mcla_launcher_store_win.h"
#include "mcla_launcher_ui_win.h"
#include <cstdint>
#include <memory>
#include <vector>
namespace mcla::launcher {
#ifdef MCLA_LAUNCHER_CONTROLLER_TESTING
// Synthetic fixtures only: the public key that authenticates staged build
// tickets (M9-005 capability grant). Production always uses the embedded key.
void SetBuildTicketKeyForTest(std::vector<uint8_t> key);
#endif
class LauncherController {
 public:
  explicit LauncherController(PreferenceStore store);
  ~LauncherController();
  UiState Dispatch(UiAction action, const UiRequest &request);
  bool running() const;
  bool completion_failed() const;
 private:
  struct State;
  std::unique_ptr<State> state_;
};
}
