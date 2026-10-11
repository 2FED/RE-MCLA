#pragma once
#include "mcla_launcher_store_win.h"
#include "mcla_launcher_ui_win.h"
#include <memory>
namespace mcla::launcher {
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
