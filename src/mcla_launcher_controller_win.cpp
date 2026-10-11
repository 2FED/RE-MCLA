#include "mcla_launcher_controller_win.h"
#include "mcla_launcher_build_win.h"
#include "mcla_launcher_game_win.h"
#include "mcla_launcher_process_win.h"
#include "mcla_launcher_snapshot_win.h"
#include "mcla_launcher_writable_root_win.h"
#include "mcla_launcher_import_policy.h"
#include "mcla_managed_config_policy.h"
#include "mcla_build_lease_runtime_win.h"
#include "mcla_portable_adapter_policy.h"
#include <windows.h>
#include <shellapi.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <bcrypt.h>
#include <algorithm>
#include <filesystem>
#include <optional>

namespace mcla::launcher {
namespace {
std::wstring Wide(std::u16string_view s) { return {s.begin(),s.end()}; }
std::u16string U16(std::wstring_view s) { return {s.begin(),s.end()}; }
std::string Utf8(std::wstring_view s) {
  const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0,nullptr,nullptr);
  if(n<=0) return {};
  std::string out(n,'\0');
  WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),out.data(),n,nullptr,nullptr);
  return out;
}
std::wstring Native(std::wstring_view s) { return L"\\\\?\\" + std::wstring(s); }
std::expected<void,std::wstring> Text(const fs::DirectoryGuard &dir, std::wstring_view name, std::string_view value) {
  return fs::AtomicWrite(dir,name,{reinterpret_cast<const uint8_t *>(value.data()),value.size()});
}
std::expected<std::wstring,std::wstring> UniqueId() {
  std::array<unsigned char,16> bytes{};
  if (BCryptGenRandom(nullptr,bytes.data(),static_cast<ULONG>(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)
    return std::unexpected(L"Cannot generate session identity");
  std::wstring s;
  for(auto b:bytes) { s+=L"0123456789abcdef"[b>>4]; s+=L"0123456789abcdef"[b&15]; }
  return s;
}
std::expected<fs::DirectoryGuard,std::wstring> NewDirectory(const fs::DirectoryGuard &parent,std::wstring_view name) {
  const auto path=parent.path()+L"\\"+std::wstring(name);
  if (!NormalizeRoot(U16(path)) || name.find_first_of(L"/\\")!=name.npos)
    return std::unexpected(L"New directory path is invalid");
  if (!CreateDirectoryW(Native(path).c_str(),nullptr)) return std::unexpected(L"Cannot create exclusive new directory: "+path);
  return fs::OpenDirectory(path);
}
struct Adapter { std::wstring name; unsigned index; };
std::vector<Adapter> Adapters() {
  using Microsoft::WRL::ComPtr;
  ComPtr<IDXGIFactory1> factory;
  std::vector<Adapter> result;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return result;
  for (unsigned i=0;;++i) {
    ComPtr<IDXGIAdapter1> adapter;
    if (factory->EnumAdapters1(i,&adapter)!=S_OK) break;
    DXGI_ADAPTER_DESC1 desc{};
    if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
    if (FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),nullptr))) continue;
    result.push_back({desc.Description,i});
  }
  return result;
}
std::expected<unsigned,std::wstring> AdapterIndex(std::wstring_view name) {
#ifdef MCLA_LAUNCHER_CONTROLLER_TESTING
  if(name==L"Fixture") return 0;
#endif
  auto adapters=Adapters();
  std::optional<unsigned> found;
  for(const auto &adapter:adapters) if(adapter.name==name) {
    if(found) return std::unexpected(L"GPU name is ambiguous; no fallback adapter was selected");
    found=adapter.index;
  }
  if(!found) return std::unexpected(L"Select an available D3D12 GPU; no fallback adapter was selected");
  return *found;
}
const ProfileEntry *Selected(const Preferences &p) {
  for(const auto &e:p.library.entries) if(e.id==p.library.selected_id) return &e;
  return nullptr;
}
std::expected<std::wstring,std::wstring> ReadDisplayName(const fs::DirectoryGuard &profile) {
  auto user=fs::OpenDirectory(profile.path()+L"\\user"); if(!user) return std::unexpected(user.error());
  const auto name_root_path=user->path()+L"\\B13EBABEBABEBABE";
  const auto name_path=name_root_path+L"\\local-display-name.txt";
  if(GetFileAttributesW(Native(name_path).c_str())==INVALID_FILE_ATTRIBUTES) {
    if(GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND) return L"User";
    return std::unexpected(L"Cannot inspect in-game profile name");
  }
  auto name_root=fs::OpenDirectory(name_root_path); if(!name_root) return std::unexpected(name_root.error());
  auto bytes=fs::ReadFile(*name_root,L"local-display-name.txt",15); if(!bytes) return std::unexpected(bytes.error());
  std::string name(bytes->begin(),bytes->end());
  if(!mcla::portable::ValidLocalProfileName(name)) return std::unexpected(L"Stored in-game name is invalid; preserved for recovery");
  return std::wstring(name.begin(),name.end());
}
struct LegacyLock {
  HANDLE handle=INVALID_HANDLE_VALUE;
  ~LegacyLock() { if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
std::expected<VerifiedGame,std::wstring> VerifySelectedGame(const fs::DirectoryGuard &root) {
#ifdef MCLA_LAUNCHER_CONTROLLER_TESTING
  const std::array<VerifiedGameFile,1> fixture{{{L"default.xex",3,"BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"}}};
  return VerifyGameRootForTest(root,fixture);
#else
  return VerifyGameRoot(root);
#endif
}
}

struct LauncherController::State {
  explicit State(PreferenceStore s): store(std::move(s)) {}
  PreferenceStore store;
  std::wstring status=L"Choose build and game folders; add or select your personal save profile. Nothing is selected automatically.";
  std::vector<Adapter> adapters=Adapters();
  struct Session {
    // Process is declared last, so it is destroyed before roots and locks.
    fs::DirectoryGuard profile, user, directory, build_root, game;
    fs::WriterLock writer;
    VerifiedBuild build;
    VerifiedGame verified_game;
    std::vector<fs::DirectoryGuard> children;
    std::string profile_id;
    OwnedProcess process;
  };
  std::optional<Session> session;
  bool completion_failed=false;

  std::expected<void,std::wstring> VerifyRoots(const UiRequest &r) {
    auto build_root=fs::OpenDirectory(r.build_root); if(!build_root) return std::unexpected(build_root.error());
    auto game_root=fs::OpenDirectory(r.game_root); if(!game_root) return std::unexpected(game_root.error());
    auto separate=fs::CompareDisjoint(*build_root,*game_root); if(!separate) return separate;
    auto build=VerifyBuild(*build_root); if(!build) return std::unexpected(build.error());
    auto game=VerifySelectedGame(*game_root); if(!game) return std::unexpected(game.error());
    const auto hash=build->fingerprint();
    status=L"Integrity verified (not gameplay PASS). Build SHA256: "+std::wstring(hash.begin(),hash.end())+
        L". Supported game revision verified. Nothing saved or launched; Play verifies again.";
    return {};
  }

  std::expected<void,std::wstring> Settings(const UiRequest &r) {
    auto p=store.preferences();
    p.build_root=U16(r.build_root); p.game_root=U16(r.game_root); p.gpu_name=U16(r.gpu_name);
    p.resolution_scale=r.resolution_scale; p.vsync=r.vsync; p.fullscreen=r.fullscreen;
    if(!p.vsync) return std::unexpected(L"VSync Off changes guest vblank timing in this SDK and is not validated. Enable VSync for the stock preset.");
    if(!ValidPreferences(p)) return std::unexpected(L"Settings contain an invalid path, GPU name or scale");
    // Revalidate all physical roots even if a save is not selected yet.
    std::vector<fs::DirectoryGuard> roots;
    for(const auto &path:{p.build_root,p.game_root}) if(!path.empty()) {
      auto dir=fs::OpenDirectory(Wide(path)); if(!dir) return std::unexpected(dir.error());
      auto distinct=fs::CompareDisjoint(*dir,store.directory()); if(!distinct) return std::unexpected(distinct.error());
      for(const auto &other:roots) { auto d=fs::CompareDisjoint(*dir,other); if(!d) return std::unexpected(d.error()); }
      roots.push_back(std::move(*dir));
    }
    for(const auto &entry:p.library.entries) {
      auto dir=fs::OpenDirectory(Wide(entry.root)); if(!dir) return std::unexpected(dir.error());
      for(const auto &other:roots) { auto d=fs::CompareDisjoint(*dir,other); if(!d) return std::unexpected(d.error()); }
    }
    if(!r.gpu_name.empty()) { auto gpu=AdapterIndex(r.gpu_name); if(!gpu) return std::unexpected(gpu.error()); }
    return store.Save(p);
  }

  std::expected<void,std::wstring> Register(std::wstring_view path,std::wstring_view label) {
    auto next=RegisterManagedProfile(store.preferences(),path,U16(label),store.directory());
    if(!next) return std::unexpected(next.error());
    return store.Save(*next);
  }
  std::expected<void,std::wstring> Import(const UiRequest &r) {
    const std::filesystem::path source_path(r.legacy_source_root);
    if(source_path.filename()!=L"user") return std::unexpected(L"Choose the old portable bundle's user folder, not one .sav file");
    auto source=fs::OpenDirectory(source_path.wstring()); if(!source) return std::unexpected(source.error());
    auto legacy=fs::OpenDirectory(source_path.parent_path().wstring()); if(!legacy) return std::unexpected(legacy.error());
    auto safe=ValidateWritableRoot(*legacy); if(!safe) return std::unexpected(safe.error());
    auto identity=fs::ReadFile(*legacy,L"bundle-id.txt",128); if(!identity) return std::unexpected(L"Legacy bundle identity missing; source not adopted");
    auto manifest=fs::ReadFile(*legacy,L"bundle-manifest.json",1024*1024); if(!manifest) return std::unexpected(L"Legacy bundle manifest missing; source not adopted");
    LegacyLock lock;
    lock.handle=CreateFileW(Native(legacy->path()+L"\\bundle.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,
        OPEN_ALWAYS,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(lock.handle==INVALID_HANDLE_VALUE) return std::unexpected(L"Old bundle is busy. Close its game/launcher before importing");
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(lock.handle,&info) || info.nNumberOfLinks!=1 ||
       (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
      return std::unexpected(L"Unsafe legacy bundle lock");
    // Re-read immutable metadata after acquiring the legacy launcher's lock.
    identity=fs::ReadFile(*legacy,L"bundle-id.txt",128);
    manifest=fs::ReadFile(*legacy,L"bundle-manifest.json",1024*1024);
    if(!identity || !manifest) return std::unexpected(L"Legacy metadata became unreadable");
    auto legacy_id=ValidateLegacyMetadata(std::string(identity->begin(),identity->end()),std::string(manifest->begin(),manifest->end()));
    if(!legacy_id) return std::unexpected(L"Unrecognized legacy metadata: "+std::wstring(legacy_id.error().begin(),legacy_id.error().end()));
    auto save_dir=fs::OpenDirectory(source->path()+L"\\B13EBABEBABEBABE\\545407F8\\00000001\\mc4.sav");
    auto header_dir=fs::OpenDirectory(source->path()+L"\\B13EBABEBABEBABE\\545407F8\\Headers\\00000001");
    if(!save_dir || !header_dir) return std::unexpected(L"Legacy source lacks the complete expected save/header pair");
    auto save=fs::ReadFile(*save_dir,L"mc4.sav",64*1024*1024);
    auto header=fs::ReadFile(*header_dir,L"mc4.sav.header",1024*1024);
    if(!save || !header || save->empty() || header->empty()) return std::unexpected(L"Legacy save/header pair is missing, empty or unsafe");
    // Protect the entire source bundle before creating any destination child.
    auto protected_preferences=store.preferences();
    auto source_id=UniqueId(); if(!source_id) return std::unexpected(source_id.error());
    protected_preferences.library.entries.push_back({std::string(source_id->begin(),source_id->end()),
        u"Import source (temporary guard)",U16(legacy->path()),Layout::LegacyBundleUserReadOnlyV1});
    auto entry=CreateImportProfile(protected_preferences,r.profile_root,U16(r.profile_label),store.directory());
    if(!entry) return std::unexpected(entry.error());
    auto profile=fs::OpenDirectory(Wide(entry->root)); if(!profile) return std::unexpected(profile.error());
    auto writer=fs::AcquireWriterLock(*profile); if(!writer) return std::unexpected(writer.error());
    auto distinct=fs::CompareDisjoint(*profile,*legacy); if(!distinct) return std::unexpected(distinct.error());
    const auto final=Native(profile->path()+L"\\managed-profile.dat");
    const auto pending=Native(profile->path()+L"\\import-in-progress.dat");
    auto user=fs::OpenDirectory(profile->path()+L"\\user"); if(!user) return std::unexpected(user.error());
    auto copied=CopySaveTree(*source,*user); if(!copied) return std::unexpected(copied.error()+L"; incomplete new profile: "+profile->path());
    auto saved=Text(*profile,L"import-receipt.txt",*copied); if(!saved) return saved;
    saved=fs::AtomicWrite(*profile,L"import-source-manifest.json",*manifest); if(!saved) return saved;
    saved=Text(*profile,L"import-source.txt","MCLA-LEGACY-IMPORT-V1\nbundle "+*legacy_id+"\nsource "+Utf8(source->path())+"\n"); if(!saved) return saved;
    if(!MoveFileExW(pending.c_str(),final.c_str(),MOVEFILE_WRITE_THROUGH)) return std::unexpected(L"Cannot complete import; original unchanged");
    return Register(profile->path(),r.profile_label);
  }

  std::expected<void,std::wstring> Start();
  std::expected<void,std::wstring> Poll();
  std::expected<void,std::wstring> Branch(const UiRequest &r,bool restore) {
    const auto &prefs=store.preferences();
    const auto *selected=Selected(prefs);
    if(!selected || selected->layout!=Layout::ManagedExternalV1) return std::unexpected(L"Select the managed source profile first");
    auto original=fs::OpenDirectory(Wide(selected->root)); if(!original) return std::unexpected(original.error());
    auto safe=ValidateWritableRoot(*original); if(!safe) return safe;
    auto lock=fs::AcquireWriterLock(*original); if(!lock) return std::unexpected(lock.error());
    auto others=prefs;
    std::erase_if(others.library.entries,[&](const auto &entry){return entry.id==selected->id;});
    auto admitted=RegisterManagedProfile(others,original->path(),selected->label,store.directory());
    if(!admitted || admitted->library.entries.back().id!=selected->id)
      return std::unexpected(L"Source profile marker identity/layout is invalid");
    std::wstring source_path=original->path()+L"\\user";
    std::optional<std::string> expected_snapshot;
    std::optional<fs::DirectoryGuard> archive_guard;
    if(restore) {
      auto snapshot=fs::OpenDirectory(r.legacy_source_root); if(!snapshot) return std::unexpected(snapshot.error());
      const auto relative=std::filesystem::path(snapshot->path()).lexically_relative(std::filesystem::path(original->path()));
      std::vector<std::wstring> parts;
      for(const auto &part:relative) parts.push_back(part.wstring());
      if(parts.size()!=3 || (parts[0]!=L"sessions" && parts[0]!=L"backups") ||
         (parts[2]!=L"before-user" && parts[2]!=L"after-user"))
        return std::unexpected(L"Choose before-user or after-user from this selected profile's session/rename archive");
      const auto id=parts[0]==L"backups" && parts[1].starts_with(L"rename-") ? parts[1].substr(7):parts[1];
      if(!ValidOpaqueId(Utf8(id)) || (parts[0]==L"backups" && !parts[1].starts_with(L"rename-")))
        return std::unexpected(L"Snapshot archive identity is invalid");
      auto archive=fs::OpenDirectory(std::filesystem::path(snapshot->path()).parent_path().wstring());
      if(!archive) return std::unexpected(archive.error());
      auto manifest=fs::ReadFile(*archive,parts[2]==L"before-user"?L"before-snapshot.txt":L"after-snapshot.txt",32*1024*1024);
      if(!manifest) return std::unexpected(L"Snapshot has no complete inventory receipt; not eligible for restore");
      expected_snapshot=std::string(manifest->begin(),manifest->end());
      if(!expected_snapshot->starts_with("MCLA-SAVE-SNAPSHOT-V1\n")) return std::unexpected(L"Unknown snapshot receipt version");
      source_path=snapshot->path(); archive_guard.emplace(std::move(*archive));
    } else {
      const auto pending=original->path()+L"\\pending-operation.dat";
      if(GetFileAttributesW(Native(pending).c_str())!=INVALID_FILE_ATTRIBUTES || GetLastError()!=ERROR_FILE_NOT_FOUND)
        return std::unexpected(L"Source has an unfinished operation. Restore a verified snapshot instead of copying uncertain live data");
      auto sessions=fs::OpenDirectory(original->path()+L"\\sessions"); if(!sessions) return std::unexpected(sessions.error());
      size_t count=0;
      for(const auto &item:std::filesystem::directory_iterator(sessions->path())) {
        if(++count>1024) return std::unexpected(L"Source session history exceeds bound");
        auto record=fs::OpenDirectory(item.path().wstring()); if(!record) return std::unexpected(record.error());
        auto state=fs::ReadFile(*record,L"state.txt",256);
        if(!state || !std::string_view(reinterpret_cast<const char*>(state->data()),state->size()).starts_with("complete\n"))
          return std::unexpected(L"Source has an unfinished session. Choose a verified snapshot for recovery");
      }
    }
    auto source=fs::OpenDirectory(source_path); if(!source) return std::unexpected(source.error());
    auto entry=CreateImportProfile(prefs,r.profile_root,U16(r.profile_label),store.directory()); if(!entry) return std::unexpected(entry.error());
    auto target=fs::OpenDirectory(Wide(entry->root)); if(!target) return std::unexpected(target.error());
    auto target_lock=fs::AcquireWriterLock(*target); if(!target_lock) return std::unexpected(target_lock.error());
    auto user=fs::OpenDirectory(target->path()+L"\\user"); if(!user) return std::unexpected(user.error());
    auto copied=CopySaveTree(*source,*user); if(!copied) return std::unexpected(copied.error());
    if(expected_snapshot && *copied!=*expected_snapshot)
      return std::unexpected(L"Snapshot content differs from its archived receipt. Original and incomplete new branch preserved: "+target->path());
    auto written=Text(*target,L"import-receipt.txt",*copied); if(!written) return written;
    written=Text(*target,L"branch-receipt.txt",std::string("MCLA-PROFILE-BRANCH-V1\nmode ")+(restore?"restore":"duplicate")+
        "\nsource-profile "+selected->id+"\nnew-profile "+entry->id+"\nsource "+Utf8(source->path())+"\n"); if(!written) return written;
    if(!MoveFileExW(Native(target->path()+L"\\import-in-progress.dat").c_str(),
        Native(target->path()+L"\\managed-profile.dat").c_str(),MOVEFILE_WRITE_THROUGH))
      return std::unexpected(L"Cannot complete new branch; original untouched and incomplete copy retained");
    return Register(target->path(),r.profile_label);
  }
  std::expected<void,std::wstring> RenameName(std::wstring_view name) {
    const auto value=Utf8(name);
    if(!mcla::portable::ValidLocalProfileName(value)) return std::unexpected(L"In-game name requires 1-15 ASCII letters, digits, spaces, _ or -, without edge spaces");
    const auto *entry=Selected(store.preferences()); if(!entry) return std::unexpected(L"Select a profile first");
    if(entry->layout!=Layout::ManagedExternalV1) return std::unexpected(L"Import the legacy profile before editing its in-game name");
    auto profile=fs::OpenDirectory(Wide(entry->root)); if(!profile) return std::unexpected(profile.error());
    auto safe=ValidateWritableRoot(*profile); if(!safe) return safe;
    auto lock=fs::AcquireWriterLock(*profile); if(!lock) return std::unexpected(lock.error());
    auto others=store.preferences();
    std::erase_if(others.library.entries,[&](const auto &e) { return e.id==entry->id; });
    auto admitted=RegisterManagedProfile(others,profile->path(),entry->label,store.directory());
    if(!admitted || admitted->library.entries.back().id!=entry->id)
      return std::unexpected(L"Selected profile identity/layout no longer matches the library");
    const auto pending=profile->path()+L"\\pending-operation.dat";
    if(GetFileAttributesW(Native(pending).c_str())!=INVALID_FILE_ATTRIBUTES || GetLastError()!=ERROR_FILE_NOT_FOUND)
      return std::unexpected(L"An unfinished profile operation needs review before renaming");
    auto sessions=fs::OpenDirectory(profile->path()+L"\\sessions"); if(!sessions) return std::unexpected(sessions.error());
    size_t session_count=0;
    for(const auto &item:std::filesystem::directory_iterator(sessions->path())) {
      if(++session_count>1024) return std::unexpected(L"Session history exceeds bound; inspect before renaming");
      auto record=fs::OpenDirectory(item.path().wstring()); if(!record) return std::unexpected(record.error());
      auto state=fs::ReadFile(*record,L"state.txt",256);
      if(!state || !std::string_view(reinterpret_cast<const char*>(state->data()),state->size()).starts_with("complete\n"))
        return std::unexpected(L"An unfinished session needs recovery before editing the in-game name");
    }
    auto old=ReadDisplayName(*profile); if(!old) return std::unexpected(old.error());
    if(*old==name) return {};
    auto user=fs::OpenDirectory(profile->path()+L"\\user"); if(!user) return std::unexpected(user.error());
    auto backups=fs::OpenDirectory(profile->path()+L"\\backups"); if(!backups) return std::unexpected(backups.error());
    size_t count=0;
    for(const auto &ignored:std::filesystem::directory_iterator(backups->path()))
      if(++count>=1024) return std::unexpected(L"Profile backup history capacity reached; archive explicitly");
    auto id=UniqueId(); if(!id) return std::unexpected(id.error());
    auto archive=NewDirectory(*backups,L"rename-"+*id); if(!archive) return std::unexpected(archive.error());
    auto before=NewDirectory(*archive,L"before-user"); if(!before) return std::unexpected(before.error());
    auto snapshot=CopySaveTree(*user,*before); if(!snapshot) return std::unexpected(snapshot.error());
    auto written=Text(*archive,L"before-snapshot.txt",*snapshot); if(!written) return written;
    written=Text(*profile,L"pending-operation.dat","rename\n"+Utf8(*id)+"\n"); if(!written) return written;
    const auto name_path=user->path()+L"\\B13EBABEBABEBABE";
    if(!CreateDirectoryW(Native(name_path).c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
      return std::unexpected(L"Cannot prepare stable local profile directory; before-copy preserved");
    auto name_root=fs::OpenDirectory(name_path); if(!name_root) return std::unexpected(name_root.error());
    written=Text(*name_root,L"local-display-name.txt",value); if(!written) return written;
    auto after=NewDirectory(*archive,L"after-user"); if(!after) return std::unexpected(after.error());
    snapshot=CopySaveTree(*user,*after); if(!snapshot) return std::unexpected(snapshot.error());
    written=Text(*archive,L"after-snapshot.txt",*snapshot); if(!written) return written;
    written=Text(*archive,L"rename-receipt.txt","MCLA-PROFILE-NAME-V1\nprofile "+entry->id+"\nold "+Utf8(*old)+"\nnew "+value+"\n"); if(!written) return written;
    if(!MoveFileExW(Native(pending).c_str(),Native(archive->path()+L"\\completed.txt").c_str(),MOVEFILE_WRITE_THROUGH))
      return std::unexpected(L"Name changed, but completion receipt failed. Before/after archives preserved; inspect before continuing");
    return {};
  }
  UiState View() const {
    const auto &p=store.preferences();
    UiState ui;
    ui.library=p.library; ui.build_root=Wide(p.build_root); ui.game_root=Wide(p.game_root);
    ui.gpu_name=Wide(p.gpu_name); ui.resolution_scale=static_cast<int>(p.resolution_scale);
    ui.vsync=p.vsync; ui.fullscreen=p.fullscreen; ui.running=session.has_value(); ui.status=status;
    if(const auto *entry=Selected(p)) {
      auto profile=fs::OpenDirectory(Wide(entry->root));
      if(profile) { auto name=ReadDisplayName(*profile); if(name) ui.in_game_name=*name; }
    }
    for(const auto &a:adapters) ui.gpu_names.push_back(a.name);
    return ui;
  }
};

std::expected<void,std::wstring> LauncherController::State::Start() {
  const auto &p=store.preferences();
  auto plan=MakeLaunchPlan(p.library,{p.build_root,p.game_root,TimingMode::Stock});
  if(!plan) return std::unexpected(L"Launch plan invalid: explicitly select a managed profile and separate build/game roots");
  const auto *entry=Selected(p);
  auto profile=fs::OpenDirectory(Wide(entry->root)); if(!profile) return std::unexpected(profile.error());
  auto safe=ValidateWritableRoot(*profile); if(!safe) return safe;
  auto writer=fs::AcquireWriterLock(*profile); if(!writer) return std::unexpected(writer.error());
  const auto pending=profile->path()+L"\\pending-operation.dat";
  if(GetFileAttributesW(Native(pending).c_str())!=INVALID_FILE_ATTRIBUTES || GetLastError()!=ERROR_FILE_NOT_FOUND)
    return std::unexpected(L"An unfinished profile operation needs review; automatic launch blocked");
  // Re-register against a temporary list to validate the marker and physical
  // roots again without changing the saved library or selection.
  auto others=p;
  std::erase_if(others.library.entries,[&](const auto &e) { return e.id==entry->id; });
  auto admission=RegisterManagedProfile(others,profile->path(),entry->label,store.directory());
  if(!admission) return std::unexpected(admission.error());
  if(admission->library.entries.back().id!=entry->id) return std::unexpected(L"Selected profile marker identity changed");
  auto build_root=fs::OpenDirectory(Wide(p.build_root)); if(!build_root) return std::unexpected(build_root.error());
  auto game=fs::OpenDirectory(Wide(p.game_root)); if(!game) return std::unexpected(game.error());
  for(const auto *a:{&*profile,&*build_root,&*game}) {
    auto separate=fs::CompareDisjoint(*a,store.directory()); if(!separate) return separate;
  }
  auto separate=fs::CompareDisjoint(*profile,*build_root); if(!separate) return separate;
  separate=fs::CompareDisjoint(*profile,*game); if(!separate) return separate;
  separate=fs::CompareDisjoint(*build_root,*game); if(!separate) return separate;
  auto build=VerifyBuild(*build_root); if(!build) return std::unexpected(build.error());
#ifndef MCLA_LAUNCHER_CONTROLLER_TESTING
  // Before any new session/user snapshot or child. Synthetic controller tests
  // use fake executables; expiry itself has separate cryptographic/runtime tests.
  const auto host=std::find_if(build->files().begin(),build->files().end(),[](const auto &file) { return file.name==L"mcla.exe"; });
  if(host==build->files().end()) return std::unexpected(L"Verified build has no host");
  auto lease=mcla::lease::CheckBuildLease(*build_root,host->sha256);
  if(!lease) return std::unexpected(lease.error());
#endif
  auto verified_game=VerifySelectedGame(*game);
  if(!verified_game) return std::unexpected(verified_game.error());
  auto gpu=AdapterIndex(Wide(p.gpu_name)); if(!gpu) return std::unexpected(gpu.error());
  auto user=fs::OpenDirectory(profile->path()+L"\\user"); if(!user) return std::unexpected(user.error());
  std::vector<fs::DirectoryGuard> children;
  for(const auto *name:{L"cache",L"logs",L"diagnostics",L"sessions",L"backups",L"metadata",L"metadata\\update"}) {
    auto child=fs::OpenDirectory(profile->path()+L"\\"+name); if(!child) return std::unexpected(child.error());
    children.push_back(std::move(*child));
  }
  auto &sessions=children[3];
  size_t history=0;
  for(const auto &item:std::filesystem::directory_iterator(sessions.path())) {
    if(++history>=1024) return std::unexpected(L"Session history capacity reached; preserve/archive explicitly before launching");
    auto previous=fs::OpenDirectory(item.path().wstring()); if(!previous) return std::unexpected(previous.error());
    auto state=fs::ReadFile(*previous,L"state.txt",256);
    if(!state || !std::string_view(reinterpret_cast<const char*>(state->data()),state->size()).starts_with("complete\n"))
      return std::unexpected(L"An unfinished session needs review. No automatic rollback or new launch: "+previous->path());
  }
  std::wstring display_name=L"User";
  const auto profile_name_root=user->path()+L"\\B13EBABEBABEBABE";
  const auto name_path=profile_name_root+L"\\local-display-name.txt";
  const auto attrs=GetFileAttributesW(Native(name_path).c_str());
  if(attrs!=INVALID_FILE_ATTRIBUTES) {
    auto name_root=fs::OpenDirectory(profile_name_root); if(!name_root) return std::unexpected(name_root.error());
    auto bytes=fs::ReadFile(*name_root,L"local-display-name.txt",15); if(!bytes) return std::unexpected(bytes.error());
    std::string name(bytes->begin(),bytes->end());
    if(!mcla::portable::ValidLocalProfileName(name)) return std::unexpected(L"Stored in-game profile name is invalid; original retained");
    display_name.assign(name.begin(),name.end());
  } else if(GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND)
    return std::unexpected(L"Cannot inspect in-game profile name");
  auto id=UniqueId(); if(!id) return std::unexpected(id.error());
  auto directory=NewDirectory(sessions,*id); if(!directory) return std::unexpected(directory.error());
  auto state=Text(*directory,L"state.txt","prepared\n"); if(!state) return state;
  auto before=NewDirectory(*directory,L"before-user"); if(!before) return std::unexpected(before.error());
  auto snapshot=CopySaveTree(*user,*before); if(!snapshot) return std::unexpected(snapshot.error());
  auto receipt=Text(*directory,L"before-snapshot.txt",*snapshot); if(!receipt) return receipt;
  auto config=Text(*directory,L"managed.toml",mcla::managed_config::kSentinel); if(!config) return config;
  auto diagnostics=NewDirectory(children[2],*id); if(!diagnostics) return std::unexpected(diagnostics.error());
  auto cache_path=children[0].path()+L"\\"+std::wstring(build->fingerprint().begin(),build->fingerprint().end());
  if(!CreateDirectoryW(Native(cache_path).c_str(),nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)
    return std::unexpected(L"Cannot prepare build-specific cache");
  auto cache=fs::OpenDirectory(cache_path); if(!cache) return std::unexpected(cache.error());
  const auto flag=[](std::wstring_view name,std::wstring_view value) { return L"--"+std::wstring(name)+L"="+std::wstring(value); };
  const auto boolean=[&](std::wstring_view name,bool value) { return flag(name,value?L"true":L"false"); };
  std::vector<std::wstring> args{
    flag(L"game_data_root",game->path()),flag(L"user_data_root",user->path()),
    flag(L"cache_root",cache->path()),flag(L"metadata_root",children[5].path()),
    flag(L"update_data_root",children[6].path()),flag(L"log_file",directory->path()+L"\\mcla.log"),
    flag(L"mcla_diagnostics_root",diagnostics->path()),flag(L"mcla_managed_launch_config",directory->path()+L"\\managed.toml"),
    flag(L"local_user_display_name",display_name),flag(L"d3d12_adapter",std::to_wstring(*gpu)),
    flag(L"resolution_scale",std::to_wstring(p.resolution_scale)),boolean(L"vsync",p.vsync),boolean(L"fullscreen",p.fullscreen),
    L"--gpu_backend=d3d12",L"--xam_user_signin_state=1",L"--input_backend=sdl",L"--mnk_mode=false",
    L"--video_mode_width=1280",L"--video_mode_height=720",L"--video_mode_refresh_rate=60",
    L"--mcla_diagnostics_enabled=true",L"--mcla_crash_reporter_dialog=true"};
#ifdef MCLA_LAUNCHER_CONTROLLER_TESTING
  args.push_back(L"--mcla_launcher_fixture=true");
#endif
#ifdef MCLA_LAUNCHER_HOST_LIFECYCLE_TEST
  args.push_back(L"--mcla_lifecycle_probe=true");
  args.push_back(L"--mcla_crash_reporter_dialog=false");
#endif
  const auto build_hash=build->fingerprint();
  auto provenance=Text(*directory,L"launch-receipt.txt","MCLA-MANAGED-SESSION-V1\nprofile "+entry->id+
      "\nbuild "+build_hash+"\ngame "+verified_game->fingerprint()+"\ntiming stock\ngpu "+Utf8(Wide(p.gpu_name))+
      "\nadapter "+std::to_string(*gpu)+"\nscale "+std::to_string(p.resolution_scale)+
      "\nvsync "+std::to_string(p.vsync)+"\nfullscreen "+std::to_string(p.fullscreen)+"\n");
  if(!provenance) return provenance;
  children.push_back(std::move(*before)); children.push_back(std::move(*diagnostics)); children.push_back(std::move(*cache));
  state=Text(*directory,L"state.txt","launching\n"); if(!state) return state;
  auto process=StartOwnedProcess(std::filesystem::path(build->path())/L"mcla.exe",args,directory->path());
  if(!process) return std::unexpected(process.error());
  // From here ownership cannot be lost on a failed receipt write.
  session.emplace(Session{std::move(*profile),std::move(*user),std::move(*directory),std::move(*build_root),
      std::move(*game),std::move(*writer),std::move(*build),std::move(*verified_game),std::move(children),entry->id,std::move(*process)});
  auto running=Text(session->directory,L"state.txt","running\npid "+std::to_string(session->process.ProcessId())+"\n");
  status=running?L"Game running. This profile is locked across builds. Closing this window leaves the game running.":
      L"Game running, but session receipt update failed. Profile lock remains held; finish the game normally.";
  return {};
}

std::expected<void,std::wstring> LauncherController::State::Poll() {
  if(!session) return {};
  auto polled=session->process.Poll(); if(!polled) return std::unexpected(polled.error());
  if(!polled->exit_code || polled->active_job_processes!=0) return {};
  auto after=NewDirectory(session->directory,L"after-user");
  std::expected<void,std::wstring> result;
  if(!after) result=std::unexpected(after.error());
  else {
    auto snapshot=CopySaveTree(session->user,*after);
    if(!snapshot) result=std::unexpected(snapshot.error());
    else result=Text(session->directory,L"after-snapshot.txt",*snapshot);
  }
  if(result) result=Text(session->directory,L"state.txt","complete\nexit "+std::to_string(*polled->exit_code)+"\n");
  status=result?L"Session finished; before/after saves retained. Exit code "+std::to_wstring(*polled->exit_code)+L" (not a gameplay PASS).":
      L"Game stopped, but final snapshot failed. Session preserved for review; automatic relaunch blocked.";
  completion_failed=!result || *polled->exit_code!=0;
  session.reset();
  return result;
}

LauncherController::LauncherController(PreferenceStore store):state_(std::make_unique<State>(std::move(store))) {}
LauncherController::~LauncherController()=default;
bool LauncherController::running() const { return state_->session.has_value(); }
bool LauncherController::completion_failed() const { return state_->completion_failed; }
UiState LauncherController::Dispatch(UiAction action,const UiRequest &request) {
  auto &s=*state_;
  std::expected<void,std::wstring> result;
  if(s.session && action!=UiAction::Refresh && action!=UiAction::OpenSave && action!=UiAction::OpenDiagnostics) {
    s.status=L"Game session still owns this profile. Settings and profile changes are blocked."; return s.View();
  }
  switch(action) {
    case UiAction::Refresh: result=s.Poll(); break;
    case UiAction::VerifyRoots: result=s.VerifyRoots(request); break;
    case UiAction::SaveSettings: result=s.Settings(request); if(result) s.status=L"Local settings saved outside the build."; break;
    case UiAction::SelectProfile: {
      auto next=SelectProfile(s.store.preferences().library,request.selected_profile_id);
      if(!next) { result=std::unexpected(L"Select a valid profile explicitly"); break; }
      auto p=s.store.preferences(); p.library=std::move(*next); result=s.store.Save(p);
      if(result) s.status=L"Profile selected. No saves copied, merged or created.";
      break;
    }
    case UiAction::RegisterManaged:
      result=s.Register(request.profile_root,request.profile_label);
      if(result) s.status=L"Existing managed profile registered. Select it explicitly before playing.";
      break;
    case UiAction::CreateProfile: {
      auto entry=CreateManagedProfile(s.store.preferences(),request.profile_root,U16(request.profile_label),s.store.directory());
      if(!entry) result=std::unexpected(entry.error());
      else result=s.Register(Wide(entry->root),request.profile_label);
      if(result) s.status=L"New empty profile created and registered. Select it explicitly.";
      break;
    }
    case UiAction::ImportLegacy:
      result=s.Import(request); if(result) s.status=L"Complete legacy user tree copied to a NEW profile; original retained. Select the new profile explicitly."; break;
    case UiAction::RenameLabel: {
      if(!ValidLabel(U16(request.profile_label))) { result=std::unexpected(L"Enter a valid library label"); break; }
      auto p=s.store.preferences();
      auto entry=std::find_if(p.library.entries.begin(),p.library.entries.end(),[&](const auto &e) { return e.id==p.library.selected_id; });
      if(entry==p.library.entries.end()) { result=std::unexpected(L"Select a profile first"); break; }
      entry->label=U16(request.profile_label); result=s.store.Save(p);
      if(result) s.status=L"Library label changed. Profile ID, XUID, save path and in-game name are unchanged.";
      break;
    }
    case UiAction::RenameInGameName:
      result=s.RenameName(request.in_game_name);
      if(result) s.status=L"In-game name saved with before/after archives. Save paths and XUID are unchanged.";
      break;
    case UiAction::DuplicateProfile: case UiAction::RestoreSnapshot:
      result=s.Branch(request,action==UiAction::RestoreSnapshot);
      if(result) s.status=L"New independent profile registered. Original unchanged; select the new profile explicitly to use it.";
      break;
    case UiAction::Play:
      result=s.Settings(request); if(result) result=s.Start(); break;
    case UiAction::OpenSave: case UiAction::OpenDiagnostics: {
      const auto *entry=Selected(s.store.preferences());
      if(!entry) { result=std::unexpected(L"No profile selected"); break; }
      auto dir=fs::OpenDirectory(Wide(entry->root)+(action==UiAction::OpenSave?L"\\user":L"\\diagnostics"));
      if(!dir) { result=std::unexpected(dir.error()); break; }
      if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",dir->path().c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)
        result=std::unexpected(L"Cannot open selected directory");
      break;
    }
  }
  if(!result) s.status=result.error();
  auto view=s.View();
  if(action==UiAction::VerifyRoots) {
    // Read-only verification must not erase unsaved form edits or persist them.
    view.build_root=request.build_root; view.game_root=request.game_root;
    view.gpu_name=request.gpu_name; view.resolution_scale=request.resolution_scale;
    view.vsync=request.vsync; view.fullscreen=request.fullscreen;
    view.in_game_name=request.in_game_name;
  }
  return view;
}
}
