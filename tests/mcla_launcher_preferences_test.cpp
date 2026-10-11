#include "mcla_launcher_preferences.h"
#include <iostream>
int main() {
  using namespace mcla::launcher;
  unsigned count = 0, failures = 0;
  auto check = [&](bool ok) { ++count; if (!ok) ++failures; };
  Preferences p;
  auto bytes = EncodePreferences(p); check(bytes.has_value());
  if (!bytes) return 1;
  auto parsed = DecodePreferences(*bytes); check(parsed && *parsed == p);
  p.build_root = u"C:/Build with spaces"; p.game_root = u"D:/\u0413\u0440\u0430";
  p.gpu_name = u"NVIDIA GPU"; p.resolution_scale = 2; p.fullscreen = true;
  bytes = EncodePreferences(p); check(bytes.has_value());
  if (!bytes) return 1;
  parsed = DecodePreferences(*bytes); check(parsed && *parsed == p);
  for (size_t i = 0; i < bytes->size(); ++i)
    check(!DecodePreferences(std::span(*bytes).first(i)));
  auto bad = *bytes; bad.push_back(0); check(!DecodePreferences(bad));
  bad = *bytes; bad.back() = 1; check(!DecodePreferences(bad));
  bad = *bytes; bad[8] = 0xff; bad[9] = 0xff; bad[10] = 0xff; bad[11] = 0xff;
  check(!DecodePreferences(bad));
  p.resolution_scale = 3; check(!EncodePreferences(p));
  p.resolution_scale = 1; p.build_root = u"relative"; check(!EncodePreferences(p));
  p.build_root.clear(); p.gpu_name = u"bad\nGPU"; check(!EncodePreferences(p));
  std::cout << "Preferences: " << count << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
