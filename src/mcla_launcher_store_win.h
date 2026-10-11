#pragma once

#include "mcla_launcher_fs_win.h"
#include "mcla_launcher_preferences.h"

namespace mcla::launcher {

inline constexpr std::wstring_view kManagedProfileMarker = L"managed-profile.dat";
inline constexpr std::wstring_view kProfileInitializationMarker = L"initialization-started.dat";

class PreferenceStore {
 public:
  struct State;
  PreferenceStore(PreferenceStore &&) noexcept;
  PreferenceStore &operator=(PreferenceStore &&) noexcept;
  ~PreferenceStore();
  PreferenceStore(const PreferenceStore &) = delete;
  PreferenceStore &operator=(const PreferenceStore &) = delete;
  const Preferences &preferences() const;
  const fs::DirectoryGuard &directory() const;
  // Invalid/corrupt/concurrently modified state is never overwritten. The
  // previous valid bytes are backed up before atomic primary publication.
  std::expected<void, std::wstring> Save(const Preferences &preferences);
 private:
  explicit PreferenceStore(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
  friend std::expected<PreferenceStore, std::wstring> OpenPreferenceStore(std::wstring_view);
};

// Caller supplies an existing local directory. A missing preferences.dat is
// first-run only if this directory is otherwise empty (except its writer lock).
// Corruption never selects preferences.previous.dat or an arbitrary fallback.
std::expected<PreferenceStore, std::wstring> OpenPreferenceStore(std::wstring_view existing_directory);

// Reads a completed managed marker, guards every existing configured root and
// rejects physical/canonical overlap. Returns a copy with one entry appended;
// selected_id is deliberately unchanged, including empty/stale selections.
std::expected<Preferences, std::wstring> RegisterManagedProfile(
    const Preferences &preferences, std::wstring_view root,
    std::u16string_view label, const fs::DirectoryGuard &local_store);

// Creates a unique NEW child of the explicit parent, never adopts/empties the
// parent. Rejects a child inside protected profile/build/game/store roots before
// any write. Sibling profiles under the same parent are allowed. No registration
// or selection occurs. Failed children remain incomplete and are not deleted.
// The initialization-started record remains as provenance after completion;
// only a final managed-profile.dat marks the layout complete.
std::expected<ProfileEntry, std::wstring> CreateManagedProfile(
    const Preferences &preferences, std::wstring_view parent,
    std::u16string_view label, const fs::DirectoryGuard &local_store);

// Same safe child/layout creation, but never publishes managed-profile.dat.
// import-in-progress.dat contains the eventual exact managed marker bytes.
// The import owner must finish copying/receipt verification and atomically
// rename that pending marker to managed-profile.dat only after success.
std::expected<ProfileEntry, std::wstring> CreateImportProfile(
    const Preferences &preferences, std::wstring_view parent,
    std::u16string_view label, const fs::DirectoryGuard &local_store);

} // namespace mcla::launcher
