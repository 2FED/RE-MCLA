// Cold synthetic ABI-only serializer test, no core, DLL, SDK, clocks or game.
// Primary owns native compilation/run. Complete representative and max JSON
// are emitted for independent whole strict parsing, NOT actual runtime proof.
#include "../src/mcla_clock_commit_history_raw_json.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

namespace {
namespace j = mcla::clock_commit_history::raw_json;
constexpr uint64_t M = std::numeric_limits<uint64_t>::max();
constexpr uint32_t U = std::numeric_limits<uint32_t>::max();
constexpr std::string_view token = "0123456789abcdef0123456789abcdef";
unsigned checks{}, failures{};
void Check(bool ok, const char* label) {
  ++checks; if (!ok) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
size_t Count(std::string_view text, std::string_view needle) {
  size_t count = 0, position = 0;
  while ((position = text.find(needle, position)) != std::string_view::npos) { ++count; position += needle.size(); }
  return count;
}
template<class Exception, class Operation> void Reject(Operation operation, const char* label) {
  bool expected = false;
  try { operation(); } catch (const Exception&) { expected = true; } catch (...) {}
  Check(expected, label);
}
// Independent literal vocabulary/order, NOT production Writer/row helpers.
const std::string top =
    "{\"schema\":\"mcla-clock-history-v1\",\"token\":\"0123456789abcdef0123456789abcdef\","
    "\"physical_row_slots\":4096,\"interval_kind\":\"source-routed-lexical\",\"engineering_proof\":{"
    "\"runtime_admitted\":false,\"source_routed_coverage_proven\":false,\"complete_clock_history_proven\":false,"
    "\"universal_writer_coverage_proven\":false,\"exact_timer_wall_interval_proven\":false,"
    "\"original_semantics_proven\":false,\"native_state_preservation_proven\":false,"
    "\"actual_clock_cause_proven\":false,\"simulation_proven\":false,\"fps_proven\":false},\"snapshot\":";
const std::string zero_header =
    "{\"abi_version\":0,\"struct_size\":0,\"capacity\":0,\"row_count\":0,\"run_high\":0,\"run_low\":0,"
    "\"observed_attempts\":0,\"mutex_ordinals\":0,\"unordered_attempts\":0,\"stored_entries\":0,\"published_entries\":0,"
    "\"aborted_entries\":0,\"overflow_entries\":0,\"diagnostic_entries\":0,\"saturated_rejections\":0,"
    "\"counter_saturated\":0,\"counts_reserved\":0,\"proof_flags\":0,\"reserved\":0,\"rows\":[";
const std::string zero_row =
    "{\"storage_ordinal\":0,\"mutex_ordinal\":0,\"kind\":0,\"ordering\":0,\"binding\":{\"run_high\":0,\"run_low\":0,"
    "\"timer_invocation\":0,\"read_ordinal\":0,\"phase_call_id\":0,\"phase_thread_epoch\":0,\"phase_native_thread_id\":0,"
    "\"phase_present\":0,\"present\":0,\"reserved\":0},\"metadata\":{\"host_sample\":0,\"returned_guest\":0,"
    "\"pre_host\":0,\"pre_guest\":0,\"post_host\":0,\"post_guest\":0,\"pre_ratio_numerator\":0,\"pre_ratio_denominator\":0,"
    "\"post_ratio_numerator\":0,\"post_ratio_denominator\":0,\"presence\":0,\"reserved\":0},\"disposition\":0,\"issues\":0}";
const std::string sample_header =
    "{\"abi_version\":7,\"struct_size\":11,\"capacity\":13,\"row_count\":0,\"run_high\":17,\"run_low\":19,"
    "\"observed_attempts\":23,\"mutex_ordinals\":29,\"unordered_attempts\":31,\"stored_entries\":37,\"published_entries\":41,"
    "\"aborted_entries\":43,\"overflow_entries\":47,\"diagnostic_entries\":53,\"saturated_rejections\":59,"
    "\"counter_saturated\":61,\"counts_reserved\":67,\"proof_flags\":71,\"reserved\":73,\"rows\":[";
const std::string sample_row =
    "{\"storage_ordinal\":101,\"mutex_ordinal\":103,\"kind\":107,\"ordering\":109,\"binding\":{\"run_high\":113,\"run_low\":127,"
    "\"timer_invocation\":131,\"read_ordinal\":137,\"phase_call_id\":139,\"phase_thread_epoch\":149,\"phase_native_thread_id\":151,"
    "\"phase_present\":157,\"present\":163,\"reserved\":167},\"metadata\":{\"host_sample\":173,\"returned_guest\":179,"
    "\"pre_host\":181,\"pre_guest\":191,\"post_host\":193,\"post_guest\":197,\"pre_ratio_numerator\":199,\"pre_ratio_denominator\":211,"
    "\"post_ratio_numerator\":223,\"post_ratio_denominator\":227,\"presence\":229,\"reserved\":233},\"disposition\":239,\"issues\":241}";
const std::string max_header =
    "{\"abi_version\":4294967295,\"struct_size\":4294967295,\"capacity\":4294967295,\"row_count\":4294967295,"
    "\"run_high\":18446744073709551615,\"run_low\":18446744073709551615,\"observed_attempts\":18446744073709551615,"
    "\"mutex_ordinals\":18446744073709551615,\"unordered_attempts\":18446744073709551615,\"stored_entries\":18446744073709551615,"
    "\"published_entries\":18446744073709551615,\"aborted_entries\":18446744073709551615,\"overflow_entries\":18446744073709551615,"
    "\"diagnostic_entries\":18446744073709551615,\"saturated_rejections\":18446744073709551615,"
    "\"counter_saturated\":4294967295,\"counts_reserved\":4294967295,\"proof_flags\":4294967295,\"reserved\":4294967295,\"rows\":[";
const std::string max_row =
    "{\"storage_ordinal\":18446744073709551615,\"mutex_ordinal\":18446744073709551615,\"kind\":4294967295,\"ordering\":4294967295,"
    "\"binding\":{\"run_high\":18446744073709551615,\"run_low\":18446744073709551615,\"timer_invocation\":18446744073709551615,"
    "\"read_ordinal\":18446744073709551615,\"phase_call_id\":18446744073709551615,\"phase_thread_epoch\":18446744073709551615,"
    "\"phase_native_thread_id\":4294967295,\"phase_present\":4294967295,\"present\":4294967295,\"reserved\":4294967295},"
    "\"metadata\":{\"host_sample\":18446744073709551615,\"returned_guest\":18446744073709551615,\"pre_host\":18446744073709551615,"
    "\"pre_guest\":18446744073709551615,\"post_host\":18446744073709551615,\"post_guest\":18446744073709551615,"
    "\"pre_ratio_numerator\":18446744073709551615,\"pre_ratio_denominator\":18446744073709551615,"
    "\"post_ratio_numerator\":18446744073709551615,\"post_ratio_denominator\":18446744073709551615,"
    "\"presence\":4294967295,\"reserved\":4294967295},\"disposition\":4294967295,\"issues\":4294967295}";
std::string Expected(const std::string& header, const std::string& first, const std::string& middle, const std::string& last) {
  std::string text = top + header + first;
  for (size_t i = 1; i < 4095; ++i) { text += ','; text += middle; }
  text += ','; text += last; text += "]}}"; return text;
}
void DefaultsAndToken() {
  std::unique_ptr<MclaClockHistorySnapshotV1> snapshot(new MclaClockHistorySnapshotV1);
  std::memset(snapshot.get(), 0, sizeof(*snapshot));
  const auto text = j::Serialize(*snapshot, token);
  Check(text == Expected(zero_header, zero_row, zero_row, zero_row), "all zero physical slots and every zero header match independent literal");
  Check(Count(text, "\"storage_ordinal\":") == 4096 && Count(text, "\"binding\":") == 4096 &&
      Count(text, "\"metadata\":") == 4096 && Count(text, "\"reserved\":") == 8193,
      "all physical row fields present even with zero declared inventory");
  Check(Count(text, ":false") == 10 && text.find(":true") == std::string::npos &&
      text.find("source-routed-lexical") != std::string::npos && text.find("\"exact_timer_wall_interval_proven\":false") != std::string::npos,
      "all engineering claims false and source-routed lexical interval explicit");
  const std::string invalid[]{"", std::string(31, '1'), std::string(33, '1'), std::string(32, '0'),
      "0123456789ABCDEF0123456789abcdef", std::string(32, 'g'), std::string(32, '\"'), std::string(32, '\\'),
      std::string(32, '\n'), std::string(32, '\0'), std::string(32, char(0xFF))};
  for (const auto& value : invalid)
    Reject<std::invalid_argument>([&] { j::Serialize(*snapshot, std::string_view(value.data(), value.size())); }, "invalid exact token rejects without repairing or escaping");
  const std::string surrounded = "xx" + std::string(token) + "yy";
  Check(j::Serialize(*snapshot, std::string_view(surrounded.data()+2, 32)) == text, "token is exact string_view not NUL-terminated storage");
  const std::string last = std::string(31, '0') + "f";
  Check(j::Serialize(*snapshot, last).find("\"token\":\""+last+"\"") != std::string::npos, "one nonzero lowercase hex digit suffices");
}
void Representative() {
  std::unique_ptr<MclaClockHistorySnapshotV1> s(new MclaClockHistorySnapshotV1);
  std::unique_ptr<MclaClockHistorySnapshotV1> before(new MclaClockHistorySnapshotV1);
  std::memset(s.get(), 0, sizeof(*s));
  s->abi_version = 7; s->struct_size = 11; s->capacity = 13; s->row_count = 0; s->run_high = 17; s->run_low = 19;
  s->observed_attempts = 23; s->mutex_ordinals = 29; s->unordered_attempts = 31; s->stored_entries = 37;
  s->published_entries = 41; s->aborted_entries = 43; s->overflow_entries = 47; s->diagnostic_entries = 53;
  s->saturated_rejections = 59; s->counter_saturated = 61; s->counts_reserved = 67; s->proof_flags = 71; s->reserved = 73;
  s->rows[0] = {101, 103, 107, 109, {113, 127, 131, 137, 139, 149, 151, 157, 163, 167},
      {173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233}, 239, 241};
  s->rows[4095].metadata.returned_guest = M;
  std::memcpy(before.get(), s.get(), sizeof(*s));
  auto text = j::Serialize(*s, token);
  std::string last = zero_row;
  const auto offset = last.find("\"returned_guest\":0");
  last.replace(offset, std::string("\"returned_guest\":0").size(), "\"returned_guest\":18446744073709551615");
  const auto expected = Expected(sample_header, sample_row, zero_row, last);
  Check(text == j::Serialize(*s, token) && !std::memcmp(s.get(), before.get(), sizeof(*s)), "serialization deterministic and entire typed input unchanged");
  Check(text.find("\"row_count\":0") != std::string::npos && text.find(sample_row) != std::string::npos && text.find(last) != std::string::npos,
      "raw contradictory zero row_count and nonzero first last physical slots all retained");
  Check(text.find("\"proof_flags\":71") != std::string::npos && Count(text, ":false") == 10,
      "raw nonzero proof and reserved fields never replace static false claims");
  std::cout << "RAW_HISTORY_REPRESENTATIVE " << text << '\n';
#if defined(MCLA_CLOCK_HISTORY_RAW_JSON_FIELD_MUTANT)
  // TEST-ONLY after-return corruption of exactly one serialized field. This
  // probes the unchanged comparator, not a live producer or source bug fix.
  const auto at = text.find("\"returned_guest\":179");
  text.replace(at, std::string("\"returned_guest\":179").size(), "\"returned_guest\":178");
#endif
  Check(text == expected, "history raw JSON returned_guest mapping lost raw value");
}
void MaximumAndMarkers() {
  std::unique_ptr<MclaClockHistorySnapshotV1> s(new MclaClockHistorySnapshotV1);
  // ABI has only fixed-width unsigned integers and exact known offsets; no
  // pointer/enum/padding is read here. All FF produces every uint64/uint32 MAX.
  std::memset(s.get(), 0xFF, sizeof(*s));
  Check(s->row_count == U && s->run_high == M && s->rows[4095].issues == U, "synthetic maximum typed fields initialized");
  const auto text = j::Serialize(*s, token);
  const auto expected = Expected(max_header, max_row, max_row, max_row);
  Check(text == expected, "all maximum envelope and all 4096 complete maximum rows match literal independently");
  const size_t exact_bound = top.size() + max_header.size() + 4096 * max_row.size() + 4095 + 3;
  Check(text.size() == exact_bound && text.size() <= j::kMaxOutputBytes && j::kMaxOutputBytes == 16u*1024u*1024u,
      "worst-case unsigned decimal widths fit exact calculated 16MiB output bound");
  Check(Count(text, "\"storage_ordinal\":18446744073709551615") == 4096 &&
      Count(text, "\"issues\":4294967295") == 4096 && Count(text, "\"row_count\":4294967295") == 1,
      "overlarge raw row_count is retained without selecting physical bound or dropping slots");
  std::cout << "MAX_HISTORY_JSON_BYTES " << text.size() << '\n';
  std::cout << "RAW_HISTORY_MAX " << text << '\n';
  for (uint64_t i = 0; i < 4096; ++i) {
    s->rows[i].storage_ordinal = i + 1; s->rows[i].mutex_ordinal = M - i;
    s->rows[i].binding.timer_invocation = i + 5001; s->rows[i].metadata.returned_guest = M - i;
  }
  const auto marked = j::Serialize(*s, token);
  size_t position = 0; bool all = true;
  for (uint64_t i = 0; i < 4096; ++i) {
    const std::string marker = "{\"storage_ordinal\":" + std::to_string(i+1) + ",\"mutex_ordinal\":" + std::to_string(M-i) + ',';
    const auto next = marked.find(marker, position);
    all = all && next != std::string::npos;
    if (next == std::string::npos) break;
    const auto end = marked.find("\"issues\":", next);
    const auto timer = marked.find("\"timer_invocation\":" + std::to_string(i+5001) + ',', next);
    const auto result = marked.find("\"returned_guest\":" + std::to_string(M-i) + ',', next);
    all = all && timer < end && result < end;
    position = end;
  }
  Check(all && Count(marked, "\"storage_ordinal\":") == 4096, "all 4096 unique physical row markers retain exact order and distinct mapped values");
}
void BudgetGuard() {
  j::detail::Writer writer;
  const std::string at_limit(j::kMaxOutputBytes, 'x');
  writer.Add(at_limit);
  Reject<std::length_error>([&] { writer.Add("x"); }, "string budget rejects one excess byte before partial append");
  Check(writer.Take() == at_limit, "budget rejection preserves already written bytes");
}
} // namespace
int main() {
  DefaultsAndToken(); Representative(); MaximumAndMarkers(); BudgetGuard();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
