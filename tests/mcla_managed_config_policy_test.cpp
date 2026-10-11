#include "mcla_managed_config_policy.h"

#include <iostream>

namespace {
unsigned checks = 0;
unsigned failures = 0;
void Check(bool result, const char *label) {
  ++checks;
  if (!result) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
} // namespace

int main() {
  using namespace mcla::managed_config;
  constexpr std::u16string_view selector = u"D:/Profiles/Main/sessions/one/config.toml";
  const RequiredPaths paths{u"F:/Games/MCLA", u"D:/Profiles/Main/user",
      u"D:/Profiles/Main/cache/build1", u"D:/Profiles/Main/update",
      u"D:/Profiles/Main/metadata", u"D:/Profiles/Main/logs/session.log",
      u"D:/Profiles/Main/diagnostics"};
  Check(IsSentinel(kSentinel), "exact sentinel");
  for (const std::string bytes : {std::string{}, std::string(kSentinel) + "\n",
       std::string(kSentinel) + "log_file = 'C:/elsewhere'\n",
       std::string("\xEF\xBB\xBF") + std::string(kSentinel),
       std::string(kSentinel.substr(0, kSentinel.size() - 1)) + "\r\n",
       std::string(kSentinel) + std::string(1, '\0')})
    Check(!IsSentinel(bytes), "noncanonical config rejected");
  Check(!Validate(selector, paths), "explicit absolute managed paths");
  Check(!Validate(u"D:/\u041f\u0440\u043e\u0444\u0456\u043b\u0456/My session/config.toml", paths),
        "Unicode and spaces");
  for (std::u16string_view bad : {u"", u"config.toml", u"C:config.toml", u"C:/",
       u"//server/share/config.toml", u"C:/a/../config.toml", u"C:/NUL"})
    Check(Validate(bad, paths) == Issue::InvalidSelector, "unsafe selector");
  const std::array<std::u16string RequiredPaths::*, 7> members{
      &RequiredPaths::game, &RequiredPaths::user, &RequiredPaths::cache,
      &RequiredPaths::update, &RequiredPaths::metadata, &RequiredPaths::log_file,
      &RequiredPaths::diagnostics};
  for (const auto member : members) {
    auto changed = paths;
    changed.*member = u"";
    Check(Validate(selector, changed) == Issue::InvalidRequiredPath, "required path cannot default");
    changed.*member = u"relative";
    Check(Validate(selector, changed) == Issue::InvalidRequiredPath, "required path cannot be relative");
  }
  for (size_t i = 1; i < members.size(); ++i) {
    for (std::u16string_view overlap : {u"F:/Games/MCLA", u"F:/Games", u"f:\\games\\mcla\\nested"}) {
      auto changed = paths;
      changed.*members[i] = overlap;
      Check(Validate(selector, changed) == Issue::GameRootOverlap, "game cannot contain mutable roots");
    }
  }
  Check(Validate(u"F:/Games/MCLA/config.toml", paths) == Issue::GameRootOverlap,
        "config outside game");
  auto sibling = paths;
  sibling.game = u"D:/Profiles/Main/game";
  Check(!Validate(selector, sibling), "lexically disjoint siblings allowed; runtime admission still required");
  std::cout << "Managed config policy: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
