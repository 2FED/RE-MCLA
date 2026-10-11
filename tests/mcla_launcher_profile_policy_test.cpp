#include "mcla_launcher_profile_policy.h"

#include <iostream>
#include <string>
#include <string_view>

namespace {
using namespace mcla::launcher;
unsigned checks = 0;
unsigned failures = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
constexpr std::string_view main_id = "1234567890abcdef1234567890abcdef";
constexpr std::string_view clean_id = "2234567890abcdef1234567890abcdef";
constexpr std::string_view legacy_id = "3234567890abcdef1234567890abcdef";
ProfileLibrary Library() {
  return {1, {{std::string(main_id), u"MAIN", u"D:\\Saves\\Main", Layout::ManagedExternalV1},
              {std::string(clean_id), u"Clean", u"D:/Saves/Clean", Layout::ManagedExternalV1},
              {std::string(legacy_id), u"Subtitle test", u"E:/Legacy/s2/user", Layout::LegacyBundleUserReadOnlyV1}},
          std::string(main_id)};
}
void ExpectIssue(const ProfileLibrary &library, IssueCode expected) {
  const auto issue = ValidateLibrary(library);
  Check(issue && issue->code == expected, "library typed rejection");
}
void ExpectPlanIssue(const ProfileLibrary &library, const LaunchRequest &request, IssueCode expected) {
  const auto result = MakeLaunchPlan(library, request);
  Check(!result && result.error().code == expected, "plan typed rejection");
}
} // namespace

int main() {
  using namespace mcla::launcher;
  auto library = Library();
  const auto original = library;
  Check(!ValidateLibrary(library), "valid library");
  const auto selected = SelectProfile(library, clean_id);
  Check(selected && selected->selected_id == clean_id, "select exact Clean ID");
  Check(library == original, "selection leaves all input unchanged");
  Check(selected && selected->entries == original.entries, "selection copies only pointer, not profiles");
  const auto legacy = SelectProfile(library, legacy_id);
  Check(legacy && legacy->selected_id == legacy_id, "legacy can be listed/selected for inspection");
  const LaunchRequest request{u"C:/Builds/MCLA", u"F:/Games/MCLA", TimingMode::Stock};
  const auto plan = MakeLaunchPlan(library, request);
  Check(plan && plan->profile.id == main_id && plan->profile.root == u"d:/saves/main", "stock lexical plan");
  Check(plan && plan->requires_runtime_validation, "plan still requires runtime gate");
  Check(library == original, "planning never edits library");
  if (legacy) ExpectPlanIssue(*legacy, request, IssueCode::LegacyReadOnly);
  auto changed_request = request;
  changed_request.timing = TimingMode::Fixed60;
  ExpectPlanIssue(library, changed_request, IssueCode::UnverifiedTiming);
  for (int invalid : {-1, 2, 90, 120}) {
    changed_request.timing = static_cast<TimingMode>(invalid);
    ExpectPlanIssue(library, changed_request, IssueCode::InvalidTiming);
  }
  Check(kMaximumFutureFps == 60, "no future cap above 60");

  for (std::u16string_view path : {u"C:/Profiles/Main", u"c:\\Profiles\\Main", u"D:/\u0421\u0435\u0439\u0432\u0438/\u041e\u0441\u043d\u043e\u0432\u043d\u0438\u0439", u"D:/Racing \U0001F3CE/Main"})
    Check(NormalizeRoot(path).has_value(), "valid absolute Unicode path");
  Check(NormalizeRoot(u"C:\\Profiles\\MAIN") == NormalizeRoot(u"c:/profiles/main"), "ASCII case/slash normalization");
  for (std::u16string_view path : {u"", u"C:/", u"C:relative", u"/root", u"\\\\server\\share", u"\\\\?\\C:\\Data", u"\\\\.\\C:\\Data", u"C:/a/../b", u"C:/./b", u"C:/a//b", u"C:/a/", u"C:/a ", u"C:/a.", u"C:/x:stream", u"C:/a?b", u"C:/a*b", u"C:/a<b", u"C:/a>b", u"C:/a|b", u"C:/a\"b", u"C:/a\nb", u"C:/CON", u"C:/CON.txt", u"C:/Con .txt", u"C:/NUL", u"C:/aux.bin", u"C:/PRN", u"C:/COM1", u"C:/LPT9.txt", u"C:/COM\u00b9", u"C:/LPT\u00b2.bin", u"C:/COM\u00b3", u"C:/CONIN$", u"C:/CONOUT$", u"C:/CLOCK$"})
    Check(!NormalizeRoot(path), "unsafe path rejected");
  Check(!NormalizeRoot(std::u16string(u"C:/a\0b", 7)), "embedded NUL path");
  Check(!NormalizeRoot(u"C:/" + std::u16string(256, u'a')), "component bound");
  Check(NormalizeRoot(u"C:/" + std::u16string(255, u'a')).has_value(), "component boundary");
  Check(!NormalizeRoot(u"C:/" + std::u16string(kMaximumPathUnits, u'a')), "total path bound");
  Check(!NormalizeRoot(u"C:/" + std::u16string(1, 0xd800)), "unpaired high surrogate");
  Check(!NormalizeRoot(u"C:/" + std::u16string(1, 0xdc00)), "unpaired low surrogate");
  Check(NormalizeRoot(u"C:/COM10").has_value(), "non-device COM10");
  Check(!RootsOverlap(u"c:/a", u"c:/ab"), "sibling prefix is not ancestor");
  Check(RootsOverlap(u"c:/a", u"c:/a/b") && RootsOverlap(u"c:/a/b", u"c:/a"), "ancestor symmetric");

  for (std::string_view id : {"", "main", "1234567890ABCDEF1234567890abcdef", "00000000000000000000000000000000", "1234567890abcdef1234567890abcdeg"})
    Check(!ValidOpaqueId(id), "invalid ID");
  Check(ValidLabel(u"\u041c\u0456\u0439 \u0441\u0435\u0439\u0432 \U0001F3CE"), "Unicode label independent of game name");
  Check(ValidLabel(u"MAIN / copy (test)"), "label is not a path");
  Check(ValidLabel(std::u16string(kMaximumLabelUnits, u'a')), "label boundary");
  for (std::u16string_view label : {u"", u" MAIN", u"MAIN ", u"A\nB", u"A\u202eB", u"A\u2066B"})
    Check(!ValidLabel(label), "unsafe label");
  Check(!ValidLabel(std::u16string(kMaximumLabelUnits + 1, u'a')), "label bound");

  auto bad = library; bad.version = 2; ExpectIssue(bad, IssueCode::UnsupportedVersion);
  bad = library; bad.entries.clear(); ExpectIssue(bad, IssueCode::InvalidCount);
  bad = library; bad.entries.resize(kMaximumProfiles + 1); ExpectIssue(bad, IssueCode::InvalidCount);
  bad = library; bad.selected_id.clear(); ExpectIssue(bad, IssueCode::MissingSelection);
  bad = library; bad.selected_id = "4234567890abcdef1234567890abcdef"; ExpectIssue(bad, IssueCode::UnknownSelection);
  bad = library; bad.entries[0].id = "bad"; ExpectIssue(bad, IssueCode::InvalidId);
  bad = library; bad.entries[1].id = bad.entries[0].id; ExpectIssue(bad, IssueCode::DuplicateId);
  bad = library; bad.entries[1].root = u"d:/SAVES/main"; ExpectIssue(bad, IssueCode::DuplicateRoot);
  bad = library; bad.entries[1].root = u"d:/Saves/Main/nested"; ExpectIssue(bad, IssueCode::OverlappingProfileRoots);
  bad = library; bad.entries[0].label.clear(); ExpectIssue(bad, IssueCode::InvalidLabel);
  bad = library; bad.entries[0].layout = static_cast<Layout>(9); ExpectIssue(bad, IssueCode::InvalidLayout);
  bad = library; bad.entries[0].root = u"relative"; ExpectIssue(bad, IssueCode::InvalidPath);
  const auto missing = SelectProfile(library, "4234567890abcdef1234567890abcdef");
  Check(!missing && missing.error().code == IssueCode::UnknownSelection, "missing selection never falls back");
  Check(library == original, "failed selection never mutates input");
  for (const std::string old_selection : {std::string{}, std::string("stale"),
       std::string("4234567890abcdef1234567890abcdef")}) {
    bad = library; bad.selected_id = old_selection;
    const auto before = bad;
    Check(ValidateLibrary(bad).has_value(), "stale selection never launches");
    const auto repaired = SelectProfile(bad, clean_id);
    Check(repaired && repaired->selected_id == clean_id &&
          repaired->entries == bad.entries, "explicit selection repairs stale pointer only");
    Check(bad == before, "explicit repair preserves input");
  }
  bad = library; bad.version = 2;
  Check(!SelectProfile(bad, clean_id), "explicit selection cannot repair unknown schema");
  bad = library; bad.entries.clear();
  Check(!SelectProfile(bad, clean_id), "empty first-run library requires explicit setup");
  bad = library; bad.entries[0].root = u"D:/Saves/Clean/nested";
  ExpectIssue(bad, IssueCode::OverlappingProfileRoots);
  changed_request = request; changed_request.build_root = u"E:/Legacy/s2";
  ExpectPlanIssue(library, changed_request, IssueCode::RootOverlap);
  changed_request = request; changed_request.game_root = u"relative";
  ExpectPlanIssue(library, changed_request, IssueCode::InvalidPath);
  const auto boundary_path = u"C:/" + std::u16string(255, u'a') + u"/" +
      std::u16string(255, u'b') + u"/" + std::u16string(255, u'c') + u"/" +
      std::u16string(253, u'd');
  Check(boundary_path.size() == kMaximumPathUnits && NormalizeRoot(boundary_path),
        "total path boundary across legal components");
  Check(!NormalizeRoot(boundary_path + u"e"), "total path boundary plus one");
  bad = library; bad.entries.clear();
  for (size_t i = 0; i < kMaximumProfiles; ++i) {
    auto entry = library.entries[0];
    const auto suffix = std::to_string(i);
    entry.id.replace(entry.id.size() - suffix.size(), suffix.size(), suffix);
    entry.root += std::u16string(suffix.begin(), suffix.end());
    bad.entries.push_back(std::move(entry));
  }
  bad.selected_id = bad.entries.front().id;
  Check(!ValidateLibrary(bad), "64 valid profiles accepted");
  for (std::u16string_view root : {u"F:/Games/MCLA", u"F:/Games", u"F:/Games/MCLA/bin", u"D:/Saves/Main", u"D:/Saves/Clean"}) {
    changed_request = request; changed_request.build_root = root;
    ExpectPlanIssue(library, changed_request, IssueCode::RootOverlap);
  }
  changed_request = request; changed_request.game_root = u"D:/Saves/Main/game";
  ExpectPlanIssue(library, changed_request, IssueCode::RootOverlap);
  changed_request = request; changed_request.build_root = u"relative";
  ExpectPlanIssue(library, changed_request, IssueCode::InvalidPath);
  std::cout << "Launcher profile policy: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
