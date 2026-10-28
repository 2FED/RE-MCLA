#pragma once

// Pure, lexical policy only. A successful plan is NOT permission to launch:
// the runtime must resolve canonical handles, reject unsafe ancestors/reparses,
// check volume/path budgets and permissions, verify the build/game identities,
// and hold a save-root-scoped writer lock before any mutation or child launch.
// ASCII case/slash normalization below is not Windows filesystem identity:
// Unicode case aliases, short names, hardlinks and mount aliases need runtime
// identity checks. No filesystem, profile adoption, copying or writes occur here.

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcla::launcher {

inline constexpr uint32_t kLibraryVersion = 1;
inline constexpr size_t kMaximumProfiles = 64;
inline constexpr size_t kMaximumLabelUnits = 80;
inline constexpr size_t kMaximumPathUnits = 1024;
inline constexpr uint32_t kMaximumFutureFps = 60;

enum class Layout { ManagedExternalV1, LegacyBundleUserReadOnlyV1 };
// Unlocked raises only the guest vblank (M9-003); simulation keeps the raw
// measured-time timer. Fixed60 stays an unvalidated policy value.
enum class TimingMode { Stock, Fixed60, Unlocked };
enum class IssueCode {
  UnsupportedVersion,
  InvalidCount,
  InvalidId,
  InvalidLabel,
  InvalidLayout,
  InvalidPath,
  DuplicateId,
  DuplicateRoot,
  OverlappingProfileRoots,
  MissingSelection,
  UnknownSelection,
  LegacyReadOnly,
  RootOverlap,
  InvalidTiming,
  UnverifiedTiming,
};

struct Issue {
  IssueCode code;
  size_t entry = 0;
  size_t other_entry = 0;
  bool operator==(const Issue &) const = default;
};

struct ProfileEntry {
  // Opaque 128-bit identity, serialized as exactly 32 lowercase hex digits.
  // This is neither the XUID nor an in-game display name.
  std::string id;
  std::u16string label;
  std::u16string root;
  Layout layout = Layout::ManagedExternalV1;
  bool operator==(const ProfileEntry &) const = default;
};

struct ProfileLibrary {
  uint32_t version = kLibraryVersion;
  std::vector<ProfileEntry> entries;
  std::string selected_id;
  bool operator==(const ProfileLibrary &) const = default;
};

struct LaunchRequest {
  std::u16string build_root;
  std::u16string game_root;
  TimingMode timing = TimingMode::Stock;
};

struct LaunchPlan {
  ProfileEntry profile;
  std::u16string build_root;
  std::u16string game_root;
  TimingMode timing = TimingMode::Stock;
  // Cannot be cleared by this policy. A separate runtime gate is mandatory.
  static constexpr bool requires_runtime_validation = true;
};

inline bool ValidOpaqueId(std::string_view id) {
  if (id.size() != 32) return false;
  bool nonzero = false;
  for (const char c : id) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    nonzero |= c != '0';
  }
  return nonzero;
}

namespace detail {
constexpr char16_t FoldAscii(char16_t c) {
  return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + (u'a' - u'A')) : c;
}

inline bool SafeUnicode(std::u16string_view value) {
  for (size_t i = 0; i < value.size(); ++i) {
    const auto c = value[i];
    if (c < 0x20 || (c >= 0x7f && c <= 0x9f) || c == 0x2028 ||
        c == 0x2029 || (c >= 0x202a && c <= 0x202e) ||
        (c >= 0x2066 && c <= 0x2069)) return false;
    if (c >= 0xd800 && c <= 0xdbff) {
      if (++i == value.size() || value[i] < 0xdc00 || value[i] > 0xdfff)
        return false;
    } else if (c >= 0xdc00 && c <= 0xdfff) {
      return false;
    }
  }
  return true;
}

inline bool ReservedComponent(std::u16string_view component) {
  auto base = component.substr(0, component.find(u'.'));
  // Win32 also interprets a device basename with spaces before an extension.
  while (!base.empty() && base.back() == u' ') base.remove_suffix(1);
  if (base == u"con" || base == u"prn" || base == u"aux" || base == u"nul" ||
      base == u"clock$" || base == u"conin$" || base == u"conout$") return true;
  if (base.size() != 4 || (base.substr(0, 3) != u"com" && base.substr(0, 3) != u"lpt"))
    return false;
  return (base[3] >= u'1' && base[3] <= u'9') || base[3] == 0x00b9 ||
         base[3] == 0x00b2 || base[3] == 0x00b3;
}
} // namespace detail

inline bool ValidLabel(std::u16string_view label) {
  return !label.empty() && label.size() <= kMaximumLabelUnits &&
         label.front() != u' ' && label.back() != u' ' && detail::SafeUnicode(label);
}

// Strict absolute drive paths only. Reject ambiguous input rather than silently
// collapsing dot components, duplicated separators or trailing separators.
// Preserve non-ASCII UTF-16 exactly; full alias detection belongs to runtime.
inline std::expected<std::u16string, Issue> NormalizeRoot(std::u16string_view path) {
  if (path.size() < 4 || path.size() > kMaximumPathUnits ||
      !detail::SafeUnicode(path) ||
      !((path[0] >= u'A' && path[0] <= u'Z') || (path[0] >= u'a' && path[0] <= u'z')) ||
      path[1] != u':' || (path[2] != u'/' && path[2] != u'\\'))
    return std::unexpected(Issue{IssueCode::InvalidPath});
  std::u16string normalized(path);
  for (auto &c : normalized) {
    if (c == u'\\') c = u'/';
    c = detail::FoldAscii(c);
  }
  size_t begin = 3;
  while (begin < normalized.size()) {
    const auto end = normalized.find(u'/', begin);
    const auto component = std::u16string_view(normalized).substr(
        begin, end == std::u16string::npos ? end : end - begin);
    if (component.empty() || component.size() > 255 || component == u"." ||
        component == u".." || component.back() == u'.' || component.back() == u' ' ||
        detail::ReservedComponent(component))
      return std::unexpected(Issue{IssueCode::InvalidPath});
    for (const auto c : component) {
      if (c == u':' || c == u'<' || c == u'>' || c == u'"' || c == u'|' ||
          c == u'?' || c == u'*')
        return std::unexpected(Issue{IssueCode::InvalidPath});
    }
    if (end == std::u16string::npos) return normalized;
    begin = end + 1;
  }
  return std::unexpected(Issue{IssueCode::InvalidPath});
}

// Arguments must already be NormalizeRoot outputs.
inline bool RootsOverlap(std::u16string_view a, std::u16string_view b) {
  return a == b || (a.size() < b.size() && b.starts_with(a) && b[a.size()] == u'/') ||
         (b.size() < a.size() && a.starts_with(b) && a[b.size()] == u'/');
}

inline std::optional<Issue> ValidateLibrary(const ProfileLibrary &library) {
  if (library.version != kLibraryVersion) return Issue{IssueCode::UnsupportedVersion};
  if (library.entries.empty() || library.entries.size() > kMaximumProfiles)
    return Issue{IssueCode::InvalidCount};
  if (library.selected_id.empty()) return Issue{IssueCode::MissingSelection};
  if (!ValidOpaqueId(library.selected_id)) return Issue{IssueCode::InvalidId};
  std::vector<std::u16string> roots;
  roots.reserve(library.entries.size());
  bool found_selection = false;
  for (size_t i = 0; i < library.entries.size(); ++i) {
    const auto &entry = library.entries[i];
    if (!ValidOpaqueId(entry.id)) return Issue{IssueCode::InvalidId, i};
    if (!ValidLabel(entry.label)) return Issue{IssueCode::InvalidLabel, i};
    if (entry.layout != Layout::ManagedExternalV1 &&
        entry.layout != Layout::LegacyBundleUserReadOnlyV1)
      return Issue{IssueCode::InvalidLayout, i};
    auto normalized = NormalizeRoot(entry.root);
    if (!normalized) return Issue{IssueCode::InvalidPath, i};
    for (size_t j = 0; j < i; ++j) {
      if (entry.id == library.entries[j].id) return Issue{IssueCode::DuplicateId, i, j};
      if (*normalized == roots[j]) return Issue{IssueCode::DuplicateRoot, i, j};
      if (RootsOverlap(*normalized, roots[j]))
        return Issue{IssueCode::OverlappingProfileRoots, i, j};
    }
    roots.push_back(std::move(*normalized));
    found_selection |= entry.id == library.selected_id;
  }
  if (!found_selection) return Issue{IssueCode::UnknownSelection};
  return std::nullopt;
}

// Pure selection returns a new library. It neither changes the caller's model
// nor adopts, renames, initializes, copies or merges any filesystem profile.
inline std::expected<ProfileLibrary, Issue> SelectProfile(
    const ProfileLibrary &library, std::string_view id) {
  if (!ValidOpaqueId(id)) return std::unexpected(Issue{IssueCode::InvalidId});
  // An explicit user selection may repair a missing/stale selected_id. All
  // entries and the schema are still validated; never choose a fallback ID.
  ProfileLibrary selected = library;
  selected.selected_id = id;
  if (const auto issue = ValidateLibrary(selected)) return std::unexpected(*issue);
  return selected;
}

inline std::expected<LaunchPlan, Issue> MakeLaunchPlan(
    const ProfileLibrary &library, const LaunchRequest &request) {
  if (const auto issue = ValidateLibrary(library)) return std::unexpected(*issue);
  if (request.timing == TimingMode::Fixed60)
    return std::unexpected(Issue{IssueCode::UnverifiedTiming});
  if (request.timing != TimingMode::Stock && request.timing != TimingMode::Unlocked)
    return std::unexpected(Issue{IssueCode::InvalidTiming});
  auto build = NormalizeRoot(request.build_root);
  auto game = NormalizeRoot(request.game_root);
  if (!build) return std::unexpected(build.error());
  if (!game) return std::unexpected(game.error());
  if (RootsOverlap(*build, *game)) return std::unexpected(Issue{IssueCode::RootOverlap});
  const ProfileEntry *selected = nullptr;
  for (size_t i = 0; i < library.entries.size(); ++i) {
    const auto &entry = library.entries[i];
    // Protect even nonselected profiles from using their contents as build/game.
    const auto root = NormalizeRoot(entry.root);
    if (RootsOverlap(*root, *build) || RootsOverlap(*root, *game))
      return std::unexpected(Issue{IssueCode::RootOverlap, i});
    if (entry.id == library.selected_id) selected = &entry;
  }
  if (!selected) return std::unexpected(Issue{IssueCode::UnknownSelection});
  if (selected->layout != Layout::ManagedExternalV1)
    return std::unexpected(Issue{IssueCode::LegacyReadOnly});
  auto profile = *selected;
  profile.root = *NormalizeRoot(profile.root);
  return LaunchPlan{std::move(profile), std::move(*build), std::move(*game), request.timing};
}

} // namespace mcla::launcher
