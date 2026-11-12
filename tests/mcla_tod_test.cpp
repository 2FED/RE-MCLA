// M11 time of day (l93): pure parts of the always-on MCLA_TOD log and the
// default-off rate / freeze / start controls (src/mcla_tod.h), and the
// generated code their offsets, addresses and return addresses mirror
// (MCLA_TOD_GENERATED_DIR). l95: the post-fx brightness fields and the
// generated code of the post-fx object and the bright-pass builder.

#include "mcla_tod.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MCLA_TOD_GENERATED_DIR
#error "MCLA_TOD_GENERATED_DIR (generated/default) is set by CMake"
#endif

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

bool Has(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

bool Near(float a, float b, float eps = 1.0e-4f) {
  return a - b <= eps && b - a <= eps;
}

namespace tod = mcla::tod;

void TestLayout() {
  static_assert(tod::kHourOffset == 14540 && tod::kAutoOffset == 14545 &&
                tod::kGarageOffset == 14546 && tod::kRateOffset == 14548 &&
                tod::kWarpFlagOffset == 14552 && tod::kWarpTargetOffset == 14556 &&
                tod::kNightLowOffset == 14564 && tod::kNightHighOffset == 14568);
  static_assert(tod::kRenderSlotOffset == 14532 && tod::kRenderSlotWordBase * 4 == 14532);
  static_assert(tod::kNightHighOffset + 4 <= tod::kManagerSpan);
  static_assert(tod::kManagerPtr == 0x82880000u - 7572u, "lis -32120 / -7572");
  static_assert(tod::kUpdateSlotIndex == 0x82890000u - 17936u, "lis -32119 / -17936");
  static_assert(tod::kPauseState == 0x828D0000u - 12640u, "lis -32115 / -12640");
  static_assert(tod::kHoursPerDayConst == 0x82000000u + 29504u, "lis -32256 / 29504");
  static_assert(tod::kPresetRateConst == 0x82000000u + 30712u, "lis -32256 / 30712");
  static_assert(tod::kMainTimer == 0x827D0000u + 29952u, "lis -32131 / 29952");
  static_assert(tod::kMainTimer + tod::kScaledDeltaOffset == 0x827D7508u);
  static_assert(tod::kMainTimer + tod::kUnscaledDeltaOffset == 0x827D7558u);
  static_assert(tod::RenderSlotAddress(0x40001000u, 0) == 0x40001000u + 14532u);
  static_assert(tod::RenderSlotAddress(0x40001000u, 1) == 0x40001000u + 14536u);
  Check(tod::PointerPlausible(0x40001000u) && tod::PointerPlausible(0xBC000000u) &&
            !tod::PointerPlausible(0) && !tod::PointerPlausible(0x100u) &&
            !tod::PointerPlausible(0x40001002u) && !tod::PointerPlausible(0xFFFFF000u),
        "manager pointer: non-null, aligned, room for the fields");
  Check(tod::SlotIndexValid(0) && tod::SlotIndexValid(1) && !tod::SlotIndexValid(2) &&
            !tod::SlotIndexValid(0xFFFFFFFFu),
        "render slot index 0 or 1");
}

void TestHours() {
  Check(Near(tod::WrapHour(24.0f), 0.0f) && Near(tod::WrapHour(25.5f), 1.5f) &&
            Near(tod::WrapHour(-0.5f), 23.5f) && Near(tod::WrapHour(12.25f), 12.25f) &&
            Near(tod::WrapHour(49.0f), 1.0f),
        "wrap into [0, 24)");
  Check(Near(tod::ForwardDelta(5.0f, 5.01f), 0.01f) &&
            Near(tod::ForwardDelta(23.995f, 0.005f), 0.01f) &&
            Near(tod::ForwardDelta(6.0f, 6.0f), 0.0f) && Near(tod::ForwardDelta(6.0f, 5.0f), 23.0f),
        "forward delta across 24:00");
  Check(tod::StepPlausible(0.0005f) && tod::StepPlausible(0.05f) && !tod::StepPlausible(0.0f) &&
            !tod::StepPlausible(0.06f) && !tod::StepPlausible(23.0f),
        "an ordinary step is positive and at most 0.05 h");
  Check(tod::HourValid(0.0f) && tod::HourValid(23.999f) && !tod::HourValid(24.0f) &&
            !tod::HourValid(-0.001f) && !tod::HourValid(std::nanf("")),
        "a valid hour is in [0, 24)");

  Check(tod::RateFactor(1) == 1 && tod::RateFactor(10) == 10 && tod::RateFactor(30) == 30 &&
            tod::RateFactor(0) == 0 && tod::RateFactor(60) == 60 && tod::RateFactor(61) == 1 &&
            tod::RateFactor(-1) == 1,
        "rate cvar: 1 off, 0 freeze, 2..60 factor, else off");
  // Rate x10 at 05:30 with the dawn band rate 1/360 h/s and a 1/30 s step.
  const float step = 1.0f / 360.0f / 30.0f;
  const auto x10 = tod::FixUp(10, 5.5f, 5.5f + step);
  Check(x10 && Near(*x10, 5.5f + 10.0f * step, 1.0e-5f), "x10 adds nine more steps");
  const auto x30 = tod::FixUp(30, 5.5f, 5.5f + step);
  Check(x30 && Near(*x30, 5.5f + 30.0f * step, 1.0e-5f), "x30 adds twenty-nine more steps");
  // Across midnight: the game wrapped 23.999 -> 0.0004 (+0.0014).
  const auto wrap = tod::FixUp(10, 23.999f, 0.0004f);
  Check(wrap && Near(*wrap, 0.0004f + 9.0f * 0.0014f, 1.0e-4f) && tod::HourValid(*wrap),
        "x10 across 24:00 stays in [0, 24)");
  const auto wrap30 = tod::FixUp(30, 23.99f, 23.999f);
  Check(wrap30 && Near(*wrap30, tod::WrapHour(23.999f + 29.0f * 0.009f), 1.0e-4f) &&
            *wrap30 < 1.0f,
        "x30 that passes 24:00 wraps");
  const auto freeze = tod::FixUp(0, 17.25f, 17.26f);
  Check(freeze && *freeze == 17.25f, "freeze writes the pre-call hour back");
  Check(!tod::FixUp(1, 5.5f, 5.51f), "factor 1 (off) never writes");
  Check(!tod::FixUp(10, 5.5f, 5.5f) && !tod::FixUp(0, 5.5f, 5.5f),
        "no advance (paused, rate 0): nothing to multiply or freeze");
  Check(!tod::FixUp(10, 6.0f, 12.0f) && !tod::FixUp(0, 0.0f, 15.0f),
        "a jump (script, option) is not an ordinary step: left alone");
  Check(!tod::FixUp(10, 6.0f, 5.99f), "a backward move is a 23.99 h forward delta: left alone");
  Check(!tod::FixUp(10, std::nanf(""), 5.5f) && !tod::FixUp(10, 5.5f, 24.5f),
        "invalid hours are left alone");
}

void TestNightAndGate() {
  Check(tod::IsNight(5.99f, 6.0f, 18.0f) && !tod::IsNight(6.0f, 6.0f, 18.0f) &&
            !tod::IsNight(18.0f, 6.0f, 18.0f) && tod::IsNight(18.01f, 6.0f, 18.0f) &&
            tod::IsNight(0.0f, 6.0f, 18.0f) && !tod::IsNight(12.0f, 6.0f, 18.0f),
        "night: hour > high or hour < low (06:00 and 18:00 are day)");
  Check(tod::NightState(5.5f, 6.0f, 18.0f) == 1 && tod::NightState(6.5f, 6.0f, 18.0f) == 0 &&
            tod::NightState(5.5f, 0.0f, 0.0f) == -1 && tod::NightState(5.5f, 18.0f, 6.0f) == -1 &&
            tod::NightState(5.5f, 6.0f, 25.0f) == -1 &&
            tod::NightState(std::nanf(""), 6.0f, 18.0f) == -1,
        "night state uses the manager's thresholds, unknown when they are not sane");
  Check(tod::NightState(4.0f, 4.5f, 19.0f) == 1 && tod::NightState(4.6f, 4.5f, 19.0f) == 0,
        "other thresholds are honoured");
  Check(tod::Paused(1, 0) && !tod::Paused(1, 1) && !tod::Paused(0, 0) && !tod::Paused(0, 1),
        "paused: +4 set and +10 clear");

  Check(tod::MayAct(true, false, false, false) && !tod::MayAct(false, false, false, false) &&
            !tod::MayAct(true, true, false, false) && !tod::MayAct(true, false, true, false) &&
            !tod::MayAct(true, false, false, true),
        "the control acts only with auto on, no warp before or after, not paused");
  Check(tod::Decide(true, true, true) == tod::Gate::kAct &&
            tod::Decide(true, true, false) == tod::Gate::kBlocked &&
            tod::Decide(true, false, false) == tod::Gate::kIdle &&
            tod::Decide(false, true, true) == tod::Gate::kIdle &&
            tod::Decide(true, false, true) == tod::Gate::kIdle,
        "the Arcade gate: blocked until a preset warp was seen, idle when nothing to do");

  Check(tod::StartMinutes(0) == 0 && tod::StartMinutes(270) == 270 &&
            tod::StartMinutes(330) == 330 && tod::StartMinutes(1440) == 0 &&
            tod::StartMinutes(-5) == 0,
        "start cvar: 1..1439 minutes, else off");
  Check(tod::StartHour(270) == 4.5f && tod::StartHour(330) == 5.5f, "04:30 and 05:30");
  Check(tod::ReplaceWarpTarget(tod::kPresetWarpReturn, 330) &&
            !tod::ReplaceWarpTarget(tod::kPresetWarpReturn, 0) &&
            !tod::ReplaceWarpTarget(0x82656494u, 330) &&
            !tod::ReplaceWarpTarget(0x8226C804u, 270) &&
            !tod::ReplaceWarpTarget(0x822D2290u, 270),
        "only the Arcade race preset warp (return 0x8265D61C) is replaced");
  Check(tod::IsPresetWarp(0x8265D61Cu) && !tod::IsPresetWarp(0x82656494u),
        "the gate marker is set by the preset caller only");
}

void TestSchedule() {
  tod::Scheduler s;
  tod::Sample day;
  day.hour = 12.0f;
  day.night = 0;
  Check(s.Observe(day, 1000) == tod::kWhyFirst, "the first update logs");
  Check(s.Observe(day, 2000) == tod::kWhyNone && s.Observe(day, 60999) == tod::kWhyNone,
        "nothing within the minute");
  Check(s.Observe(day, 61000) == tod::kWhyMinute, "one line per real minute");
  Check(s.Observe(day, 62000) == tod::kWhyNone, "the minute restarts");
  tod::Sample warp = day;
  warp.warp = 1;
  Check(s.Observe(warp, 63000) == tod::kWhyWarpStart, "warp start edge");
  Check(s.Observe(warp, 63100) == tod::kWhyNone, "no repeat while warping");
  tod::Sample night = day;
  night.hour = 5.5f;
  night.night = 1;
  Check(s.Observe(night, 63200) == (tod::kWhyWarpEnd | tod::kWhyNight),
        "warp end and the night flag change in one update");
  tod::Sample unknown = night;
  unknown.night = -1;
  Check(s.Observe(unknown, 63300) == tod::kWhyNone, "an unknown night state is not an edge");
  tod::Sample dawn = night;
  dawn.hour = 6.001f;
  dawn.night = 0;
  Check(s.Observe(dawn, 63400) == tod::kWhyNight, "the 06:00 switch (from the last known state)");
  Check(s.Observe(dawn, 120999) == tod::kWhyNone && s.Observe(dawn, 121000) == tod::kWhyMinute,
        "the minute is kept from the last minute line, not from edge lines");
  Check(tod::WhyText(tod::kWhyWarpEnd | tod::kWhyNight) == "night+warp_end" &&
            tod::WhyText(tod::kWhyFirst) == "first" && tod::WhyText(tod::kWhyNone) == "none" &&
            tod::WhyText(tod::kWhyMinute | tod::kWhyWarpStart) == "minute+warp_start",
        "why text");
}

void TestLines() {
  Check(tod::RateLabel(1) == "off" && tod::RateLabel(10) == "rate10" &&
            tod::RateLabel(30) == "rate30" && tod::RateLabel(0) == "freeze",
        "ctl labels");
  Check(tod::StartLabel(0) == "off" && tod::StartLabel(270) == "0430" &&
            tod::StartLabel(330) == "0530",
        "start labels");
  tod::Sample s;
  s.hour = 5.5123f;
  s.rate = 1.0f / 360.0f;
  s.auto_on = 1;
  s.warp = 0;
  s.target = 5.5f;
  s.paused = 0;
  s.night = 1;
  s.night_low = 6.0f;
  s.night_high = 18.0f;
  const std::string line = tod::Line(tod::kWhyWarpEnd, s, "rate10", "0530", true,
                                     tod::kPresetWarpReturn, 3);
  std::printf("example: %s\n", line.c_str());
  Check(line == "MCLA_TOD v=1 why=warp_end hour=05.512 rate_hps=0.002778 auto=1 warp=0 "
                "target=05.500 paused=0 night=1 ctl=rate10 start=0530 gate=1 "
                "warp_lr=0x8265D61C acts=3",
        "the MCLA_TOD line");
  tod::Counters c;
  c.updates = 1800;
  c.lines = 4;
  c.preset_warps = 1;
  c.start_replaced = 1;
  c.acts = 900;
  const std::string json = tod::StateJson(true, s, "rate10", "0530", true,
                                          tod::kPresetWarpReturn, c);
  Check(json.front() == '{' && json.back() == '}' && Has(json, "\"version\": 1") &&
            Has(json, "\"valid\": true") && Has(json, "\"hour\": 5.5123") &&
            Has(json, "\"night\": 1") && Has(json, "\"ctl\": \"rate10\"") &&
            Has(json, "\"start\": \"0530\"") && Has(json, "\"gate\": true") &&
            Has(json, "\"warp_lr\": \"0x8265D61C\"") && Has(json, "\"updates\": 1800") &&
            Has(json, "\"acts\": 900") && Has(json, "\"blocked\": 0") &&
            Has(json, "\"night_low\": 6.000") && json.find('\n') == std::string::npos,
        "the F10 time_of_day object");
  const std::string empty = tod::StateJson(false, tod::Sample{}, "off", "off", false, 0,
                                           tod::Counters{});
  Check(Has(empty, "\"valid\": false") && Has(empty, "\"night\": -1") &&
            Has(empty, "\"ctl\": \"off\"") && Has(empty, "\"warp_lr\": \"0x00000000\""),
        "before the first update");
}

void TestBrightness() {
  static_assert(tod::kPostFxPtr == 0x82900000u + 21664u, "lis -32112 / 21664");
  static_assert(tod::kBpPresetOffset == 340 && tod::kBpWordOffset == 284);
  static_assert(tod::kBpFloatOffsets.size() == 11 && tod::kBpFloatOffsets[0] == 256 &&
                tod::kBpFloatOffsets[10] == 348);
  static_assert(tod::kPostFxSpan >= 348 + 4 && tod::kPostFxSpan >= tod::kBpPresetOffset + 1);
  static_assert(tod::PhysicalToVirtual(0x03098000u) == 0xA3098000u);
  static_assert(tod::PhysicalToVirtual(0x03099000u) == 0xA3099000u);
  static_assert(tod::PhysicalToVirtual(0xE3098000u) == 0xA3098000u);
  static_assert(tod::kWhyBpPreset == 32);
  {
    const uint8_t be[4] = {0x3F, 0x80, 0x00, 0x00};
    static constexpr uint8_t word[4] = {0x12, 0x34, 0x56, 0x78};
    static_assert(tod::BigEndianWord(word) == 0x12345678u);
    Check(tod::FloatFromBits(tod::BigEndianWord(be)) == 1.0f, "big-endian 1.0f");
  }
  const uint8_t hex[3] = {0x00, 0xAB, 0x7F};
  Check(tod::HexBytes(hex, 3) == "00ab7f", "hex bytes");
  Check(tod::FloatText(0.125f) == "0.125" && tod::FloatText(1.0e-6f) == "1e-06" &&
            tod::FloatText(tod::FloatFromBits(0x7FC00000u)) == "nan" &&
            tod::FloatText(tod::FloatFromBits(0xFF800000u)) == "-inf",
        "float text");
  Check(tod::WhyText(tod::kWhyBpPreset) == "bp_preset" &&
            tod::WhyText(tod::kWhyMinute | tod::kWhyBpPreset) == "minute+bp_preset",
        "the bp_preset reason");
  tod::PresetWatch watch;
  Check(!watch.Observe(-1) && !watch.Observe(0) && !watch.Observe(0) && !watch.Observe(-1) &&
            watch.Observe(1) && !watch.Observe(1) && watch.Observe(0),
        "preset watch: edges between valid reads only");

  tod::Brightness none;
  Check(tod::BrightnessFields(none) == " adapt_lum=na lum4_raw=na bp_preset=-1",
        "nothing readable");
  tod::Brightness b;
  b.lum_valid = true;
  b.lum_bits = 0x3E000000u;  // 0.125
  b.lum4_valid = true;
  for (size_t i = 0; i < b.lum4.size(); ++i) {
    b.lum4[i] = uint8_t(i);
  }
  b.bp_valid = true;
  b.bp_object = 0x40123450u;
  b.preset = 1;
  b.word284 = 0x4012AB00u;
  for (size_t i = 0; i < b.bp.size(); ++i) {
    b.bp[i] = float(i) * 0.5f;
  }
  const std::string fields = tod::BrightnessFields(b);
  std::printf("example brightness:%s\n", fields.c_str());
  Check(fields == " adapt_lum=0.125 adapt_lum_bits=0x3E000000 "
                  "lum4_raw=000102030405060708090a0b0c0d0e0f bp_obj=0x40123450 bp_preset=1 "
                  "bp=0,0.5,1,1.5,2,2.5,3,3.5,4,4.5,5 bp284=0x4012AB00",
        "the brightness fields");
  tod::Sample s;
  s.night = 1;
  const std::string line = tod::Line(tod::kWhyBpPreset, s, "off", "off", false, 0, 0, fields);
  Check(line.rfind("MCLA_TOD v=1 why=bp_preset hour=", 0) == 0 &&
            line.size() > fields.size() &&
            line.compare(line.size() - fields.size(), fields.size(), fields) == 0,
        "the fields are appended to the line");
  const std::string json =
      tod::StateJson(true, s, "off", "off", false, 0, tod::Counters{}, tod::BrightnessJson(b));
  Check(json.front() == '{' && json.back() == '}' &&
            Has(json, "\"blocked\": 0, \"brightness\": {\"adapt_lum_valid\": true") &&
            Has(json, "\"adapt_lum_bits\": \"0x3E000000\", \"adapt_lum\": \"0.125\"") &&
            Has(json, "\"lum4_raw\": \"000102030405060708090a0b0c0d0e0f\"") &&
            Has(json, "\"bp_preset\": 1") && Has(json, "{\"off\": 256, \"v\": \"0\"}") &&
            Has(json, "{\"off\": 348, \"v\": \"5\"}]}}") &&
            json.find('\n') == std::string::npos,
        "the F10 brightness member");
  const std::string json_none = tod::StateJson(false, tod::Sample{}, "off", "off", false, 0,
                                               tod::Counters{}, tod::BrightnessJson(none));
  Check(Has(json_none, "\"adapt_lum_valid\": false") && Has(json_none, "\"bp_valid\": false") &&
            json_none.back() == '}',
        "the F10 brightness member without data");
  Check(tod::StateJson(false, tod::Sample{}, "off", "off", false, 0, tod::Counters{}).find(
            "brightness") == std::string::npos,
        "no member without the extra text");
}

// --- generated code --------------------------------------------------------------
std::string ReadFile(const std::string &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::string Body(const std::string &text, const std::string &name) {
  const std::string head = "DEFINE_REX_FUNC(" + name + ") {";
  const size_t begin = text.find(head);
  if (begin == std::string::npos) {
    return {};
  }
  const size_t end = text.find("DEFINE_REX_FUNC(", begin + head.size());
  return text.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

void InOrder(const std::string &body, const std::vector<std::string> &lines, const char *what) {
  size_t from = 0;
  for (const auto &line : lines) {
    const size_t at = body.find(line, from);
    if (at == std::string::npos) {
      std::fprintf(stderr, "missing in %s: %s\n", what, line.c_str());
      Check(false, what);
    }
    from = at + line.size();
  }
  Check(true, what);
}

// The text between `from` and `to` (from included) of body.
std::string Between(const std::string &body, const std::string &from, const std::string &to) {
  const size_t a = body.find(from);
  if (a == std::string::npos) {
    return {};
  }
  const size_t b = body.find(to, a + from.size());
  return b == std::string::npos ? std::string() : body.substr(a, b - a);
}

size_t Count(const std::string &text, const std::string &part) {
  size_t n = 0;
  for (size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + 1)) {
    ++n;
  }
  return n;
}

void TestGenerated() {
  const std::string dir = MCLA_TOD_GENERATED_DIR;
  const std::string g15 = ReadFile(dir + "/mcla_recomp.15.cpp");
  const std::string g16 = ReadFile(dir + "/mcla_recomp.16.cpp");
  const std::string g50 = ReadFile(dir + "/mcla_recomp.50.cpp");
  Check(!g15.empty() && !g16.empty() && !g50.empty(), "generated files readable");

  // The update: auto byte, warp flag, pause state, the advance and the wrap.
  const std::string update = Body(g15, "sub_822F1DB0");
  InOrder(update,
          {"// mr r31,r3", "// lis r10,-32256", "// lis r11,-32131", "// lis r9,-32115",
           "// lbz r8,14545(r31)", "// lfs f3,29504(r10)", "// addi r8,r11,29952",
           "// beq cr6,0x822f1eac", "// lbz r11,14552(r31)", "// bne cr6,0x822f1eac",
           "// lwz r11,-12640(r9)", "// lbz r10,4(r11)", "// lbz r10,10(r11)",
           "// bne cr6,0x822f1eb0", "// lfs f0,14540(r31)", "// stfs f13,14548(r31)",
           "// lfs f13,8(r8)", "// lfs f12,14548(r31)", "// fmadds f0,f12,f13,f0",
           "// stfs f0,14540(r31)", "// fcmpu cr6,f0,f3", "// fsubs f0,f0,f3",
           "// stfs f0,14540(r31)", "loc_822F1EAC:"},
          "update: auto byte, warp flag, pause gate, rate x scaled dt, wrap at f3");
  // The warp: target, unscaled dt, clear the flag on arrival.
  InOrder(update,
          {"loc_822F1EB0:", "// lbz r10,14552(r31)", "// beq cr6,0x822f1fb0",
           "// lfs f0,14556(r31)", "// lfs f13,14540(r31)", "// lfs f11,14548(r31)",
           "// lfs f0,88(r8)", "// stb r7,14552(r31)"},
          "update: the warp towards +14556 at +14548 x unscaled dt");
  // The publish: the render slot 4 * (3633 + *0x8288B9F0), no call after it.
  InOrder(update,
          {"loc_822F1FB0:", "// lis r11,-32119", "// lfs f0,14540(r31)",
           "// lwz r11,-17936(r11)", "// addi r10,r11,3633", "// rlwinm r9,r10,2,0,29",
           "// stfsx f0,r9,r31", "// blr"},
          "update: the hour is published to the render slot");
  {
    const std::string tail = Between(update, "loc_822F1FB0:", "// blr");
    Check(!tail.empty() && !Has(tail, "ctx.lr = 0x") && Count(tail, "REX_STORE_U32(") == 1 &&
              !Has(tail, "REX_STORE_U8("),
          "nothing is called or stored after the publish but the slot");
    Check(Count(update, "ctx.lr = 0x") == 1 && Has(update, "ctx.lr = 0x822F1FAC;"),
          "the update's only call is the warp-path wrap helper");
    Check(Count(update, "ctx.r7.s64 = 0;") == 1 && Count(update, "ctx.r7.s64 =") == 1 &&
              Count(update, "ctx.r7.u64 =") == 0,
          "r7 is 0 throughout (the cleared warp flag)");
  }
  // The only caller loads the manager from 0x8287E26C.
  InOrder(Body(g16, "sub_823045B0"),
          {"// lis r11,-32120", "// lwz r3,-7572(r11)", "// bl 0x822f1db0",
           "ctx.lr = 0x823045D0;", "sub_822F1DB0(ctx, base);"},
          "the update task calls the update with r3 = *0x8287E26C");

  // The warp start.
  InOrder(Body(g15, "sub_822F1518"),
          {"// lfs f0,14540(r3)", "// fcmpu cr6,f1,f0", "// beqlr cr6", "// stfs f2,14548(r3)",
           "// stfs f1,14556(r3)", "// stb r11,14552(r3)", "// blr"},
          "warp start: equal target returns, else rate, target and flag");
  // The Arcade race preset: f1 from the preset table, f2 = 3.0, r3 = manager.
  const std::string preset = Body(g50, "sub_8265D2B8");
  InOrder(preset,
          {"// lis r11,-32120", "// lwz r31,-7572(r11)", "// bl 0x8238eb90",
           "ctx.lr = 0x8265D60C;", "sub_8238EB90(ctx, base);", "// lis r10,-32256",
           "// mr r3,r31", "// lfs f2,30712(r10)", "// bl 0x822f1518", "ctx.lr = 0x8265D61C;",
           "sub_822F1518(ctx, base);"},
          "the preset warp: table hour in f1, rate 3.0 in f2, manager in r3");
  {
    const std::string span = Between(preset, "ctx.lr = 0x8265D60C;", "ctx.lr = 0x8265D61C;");
    Check(!span.empty() && !Has(span, "ctx.f1.") && Count(span, "ctx.lr = 0x") == 1,
          "f1 is the preset table result at the warp call");
  }

  // The night state: getter hour > +14568 or < +14564.
  InOrder(Body(g15, "sub_822F3BD8"),
          {"// bl 0x822f10d0", "// lfs f0,14568(r31)", "// fcmpu cr6,f1,f0",
           "// bgt cr6,0x822f445c", "// bl 0x822f10d0", "// lfs f0,14564(r31)",
           "// fcmpu cr6,f1,f0", "// bge cr6,0x822f4464", "loc_822F445C:", "// li r11,1",
           "loc_822F4464:", "// li r11,0"},
          "night: hour above +14568 or below +14564");
  InOrder(Body(g15, "sub_822F10D0"), {"// lbz r11,14546(r3)"},
          "the getter reads the garage byte +14546");

  // l95: the post-fx object *0x829054A0 and its +340 preset setter.
  InOrder(Body(g16, "sub_823076E8"),
          {"// lis r11,-32112", "// lwz r11,21664(r11)", "// stb r4,340(r11)", "// blr"},
          "the +340 setter stores r4 into *0x829054A0");
  for (const char *native : {"sub_82304680", "sub_82305080"}) {
    Check(Has(Body(g16, native), "sub_823076E8(ctx, base);"),
          "the two script-native wrappers call the +340 setter");
  }
  // The bright-pass builder reads the preset and the fields from its argument.
  const std::string g47 = ReadFile(dir + "/mcla_recomp.47.cpp");
  const std::string builder = Body(g47, "sub_8260DA68");
  InOrder(builder,
          {"// mr r31,r3", "// lbz r11,340(r31)", "// lfs f0,308(r31)", "// lfs f13,256(r31)",
           "// lfs f11,260(r31)", "// lfs f10,264(r31)", "// lfs f0,280(r31)",
           "// lfs f13,268(r31)", "// lfs f11,272(r31)", "// lfs f10,276(r31)",
           "// lfs f0,304(r31)", "// lfs f12,348(r31)"},
          "the bright-pass builder: the preset byte and the two presets, +304, +348");
  Check(Has(builder, "// lwz r5,284(r31)") && Has(builder, "// lfs f5,288(r31)"),
        "the builder reads the word +284 and the float +288");

  // Callers across the whole generated tree: one update caller, seven warp
  // callers, one of them returning to 0x8265D61C.
  size_t update_calls = 0;
  size_t warp_calls = 0;
  size_t preset_calls = 0;
  size_t preset_setter_calls = 0;
  size_t files = 0;
  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    const std::string name = entry.path().filename().string();
    if (name.rfind("mcla_recomp.", 0) != 0 || entry.path().extension() != ".cpp") {
      continue;
    }
    ++files;
    const std::string g = ReadFile(entry.path().string());
    update_calls += Count(g, "\tsub_822F1DB0(ctx, base);");
    warp_calls += Count(g, "\tsub_822F1518(ctx, base);");
    preset_calls += Count(g, "ctx.lr = 0x8265D61C;");
    preset_setter_calls += Count(g, "\tsub_823076E8(ctx, base);");
  }
  Check(files >= 50, "the generated tree is present");
  Check(update_calls == 1, "sub_822F1DB0 has exactly one caller");
  Check(preset_setter_calls == 2, "sub_823076E8 (the +340 setter) has exactly two callers");
  Check(warp_calls == 7 && preset_calls == 1,
        "sub_822F1518 has seven callers; one returns to 0x8265D61C");
}

}  // namespace

int main() {
  TestLayout();
  TestHours();
  TestNightAndGate();
  TestSchedule();
  TestLines();
  TestBrightness();
  TestGenerated();
  std::printf("mcla_tod_test: %d checks passed\n", checks);
  return 0;
}
