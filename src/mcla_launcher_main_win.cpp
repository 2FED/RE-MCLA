#include "mcla_launcher_controller_win.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
  using namespace mcla::launcher;
  const auto fail=[](const std::wstring &message) {
    MessageBoxW(nullptr,message.c_str(),L"MCLA-R launcher",MB_OK|MB_ICONERROR); return 1;
  };
  PWSTR local=nullptr;
  if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_DEFAULT,nullptr,&local)))
    return fail(L"Cannot locate your local application-data directory");
  const std::filesystem::path base(local); CoTaskMemFree(local);
  auto base_guard=fs::OpenDirectory(base.wstring());
  if(!base_guard) return fail(base_guard.error());
  const auto state_path=base/L"MCLA-R-Launcher-v1";
  if(!CreateDirectoryW(state_path.c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
    return fail(L"Cannot create your local launcher settings directory");
  auto store=OpenPreferenceStore(state_path.wstring());
  if(!store) return fail(store.error());
  LauncherController controller(std::move(*store));
  const int result=RunLauncherUi([&](UiAction action,const UiRequest &request) { return controller.Dispatch(action,request); });
  // Closing the launcher window is not a request to stop a running game.
  // Remain as the hidden session owner until all owned children finish.
  bool hidden_session=controller.running();
  UiState last;
  while(controller.running()) {
    last=controller.Dispatch(UiAction::Refresh,{});
    Sleep(250);
  }
  if(hidden_session && controller.completion_failed()) return fail(last.status);
  return result;
}
