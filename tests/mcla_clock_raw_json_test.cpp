// Pure after-drain serializer fixture; snapshots are synthetic heap objects.
// No live recorder, SDK, OS, clock, files or game. RAW_JSON is one bounded
// representative for an independent JSON parser, NOT a real run receipt.
#include "../src/mcla_clock_raw_json.h"
#include <iostream>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>

namespace {
namespace json = mcla::clock_read_witness::raw_json;
namespace host = mcla::clock_read_witness::host;
namespace journal = mcla::clock_read_witness::host_journal;
constexpr auto M = std::numeric_limits<uint64_t>::max();
constexpr auto U = std::numeric_limits<uint32_t>::max();
const std::string kToken = "0123456789abcdef0123456789abcdef";
unsigned checks{}, failures{};
void Check(bool condition, const char* label) {
  ++checks; if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
size_t Count(const std::string& text, const std::string& needle) {
  size_t count = 0, offset = 0;
  while ((offset = text.find(needle,offset)) != std::string::npos) { ++count; offset += needle.size(); }
  return count;
}
template<class Exception, class Function> void Rejects(Function function, const char* label) {
  bool right = false;
  try { function(); } catch (const Exception&) { right = true; } catch (...) {}
  Check(right,label);
}
MclaClockWitnessBindingV1 MaxBinding() { return {U,U,M,M,M,M,M,M,U,U,U,U}; }
host::Receipt MaxReceipt() {
  return {MaxBinding(),MaxBinding(),{U,U,U,U,U,U},M,M,U,U,U,U,U,U};
}
MclaClockWitnessRecordV1 MaxSdkRecord() {
  return {M,MaxBinding(),M,M,M,M,M,M,M,M,U,U,U,U};
}
void MaxHostRecord(journal::Record& r, uint32_t reads) {
  r.storage_ordinal = M; r.invocation = {M,M,M,M,U,U,U,U};
  r.disposition = static_cast<journal::Disposition>(U);
  r.outcome = static_cast<journal::Outcome>(U); r.flags = U; r.reserved = U;
  r.trace.observed_reads = M; r.trace.lost_rows = M; r.trace.size = reads;
  r.trace.overflow = U; r.trace.ordinal_saturated = U; r.trace.counter_saturated = U;
  for (uint32_t i = 0; i < reads; ++i) r.trace.rows[i] = MaxReceipt();
}
void MaxHeaders(json::HostSnapshot& h, MclaClockWitnessSnapshotV2& s) {
  h.run = {M,M-1}; h.counts = {M,M,M,M,M,M,M,M,M,M,M,M,U,U};
  s.abi_version = U; s.struct_size = U; s.capacity = U;
  s.run_high = M; s.run_low = M;
  s.observed_bound_reads = M; s.stored_entries = M; s.published_entries = M; s.aborted_entries = M;
  s.overflow_entries = M; s.invalid_bindings = M; s.diagnostic_entries = M; s.saturated_rejections = M;
  s.missing_witnesses = M; s.duplicate_witnesses = M; s.counter_saturated = U; s.proof_flags = U;
}

void EmptyAndTokens() {
  auto h = std::make_unique<json::HostSnapshot>();
  auto s = std::make_unique<MclaClockWitnessSnapshotV2>();
  const auto empty = json::Serialize(*h,*s,kToken);
  Check(empty.starts_with("{\"schema\":\"mcla-clock-read-raw-v1\",\"token\":\"" + kToken + "\""),
      "schema and exact validated token are emitted");
  Check(Count(empty,"\"rows\":[]") == 2 && Count(empty,"\"size\":0") == 1,
      "empty inventories retain both objects and zero headers without fabricated rows");
  Check(Count(empty,":false") == 6 && empty.find(":true") == std::string::npos,
      "all six static engineering claims remain false");
  Check(empty.find('\\') == std::string::npos && empty.find('\n') == std::string::npos &&
      empty.find('\r') == std::string::npos,"static strings and token need no escaping or text translation");
  const std::string invalid[] = {"",std::string(31,'1'),std::string(33,'1'),std::string(32,'0'),
      "0123456789ABCDEF0123456789abcdef",std::string(32,'g'),std::string(32,'\"'),
      std::string(32,'\\'),std::string(32,'\n'),std::string(32,'\0')};
  for (const auto& token : invalid)
    Rejects<std::invalid_argument>([&] { json::Serialize(*h,*s,token); },"invalid token rejects instead of escaping or repairing");
  const auto nonzero_tail = json::Serialize(*h,*s,std::string(31,'0') + "1");
  Check(nonzero_tail.find(std::string(31,'0') + "1") != std::string::npos,
      "one nonzero lowercase hex digit is sufficient without identity guessing");
}

void NaturalBoundsOnly() {
  auto h = std::make_unique<json::HostSnapshot>();
  auto s = std::make_unique<MclaClockWitnessSnapshotV2>();
  for (const uint64_t size : {uint64_t{1025},M}) {
    h->size = size;
    Rejects<std::invalid_argument>([&] { json::Serialize(*h,*s,kToken); },"host physical bound rejects without reading rows");
  }
  h->size = 1; h->rows[0].trace.size = 17;
  Rejects<std::invalid_argument>([&] { json::Serialize(*h,*s,kToken); },"trace physical bound rejects without truncation");
  h->size = 0; // Invalid unused trace lies outside the declared inventory.
  s->row_count = 4097;
  Rejects<std::invalid_argument>([&] { json::Serialize(*h,*s,kToken); },"SDK physical bound rejects without truncation");
  s->row_count = U;
  Rejects<std::invalid_argument>([&] { json::Serialize(*h,*s,kToken); },"maximum raw row count cannot index the SDK array");
  s->row_count = 1; s->capacity = 0; s->abi_version = 99; s->struct_size = 3;
  s->rows[0].binding.phase_present = 42; s->rows[0].ratio_denominator = 0;
  const auto raw = json::Serialize(*h,*s,kToken);
  Check(raw.find("\"capacity\":0,\"row_count\":1") != std::string::npos &&
      raw.find("\"phase_present\":42") != std::string::npos && raw.find("\"ratio_denominator\":0") != std::string::npos,
      "invalid ABI capacity phase and zero denominator survive as raw values");
}

void RepresentativeAndExactFields() {
  auto h = std::make_unique<json::HostSnapshot>();
  auto s = std::make_unique<MclaClockWitnessSnapshotV2>();
  MaxHeaders(*h,*s); h->size = 1; MaxHostRecord(h->rows[0],2);
  auto& a = h->rows[0].trace.rows[0];
  a.scope_binding.run_high = 17; a.scope_binding.timer_invocation = 0;
  a.request.run_high = 18; a.request.timer_invocation = 1234;
  a.status.admission = MCLA_CW_INVALID_BINDING; a.status.completion = MCLA_CW_ABORTED;
  a.result_available = 0; a.used_query = 0; a.trace_ordinal = 0;
  h->rows[1].storage_ordinal = 9223372036854775806ull; // Unused sentinel.
  h->rows[1].trace.size = U;
  s->row_count = 2; s->capacity = 0; s->run_high = 20; s->run_low = 21;
  s->rows[0] = MaxSdkRecord(); s->rows[1] = MaxSdkRecord(); s->rows[0].binding.run_high = 19;
  s->rows[2].admission_ordinal = 9223372036854775805ull; // Unused sentinel.
  const auto text = json::Serialize(*h,*s,kToken);
  Check(text == json::Serialize(*h,*s,kToken),"unchanged raw input serializes deterministically");
  Check(text.find("18446744073709551615") != std::string::npos &&
      text.find("4294967295") != std::string::npos && text.find("18446744073709551614") != std::string::npos &&
      text.find("e+") == std::string::npos,"unsigned maxima retain exact decimal integers without floating conversion");
  Check(text.find("\"run_high\":17") != std::string::npos && text.find("\"run_high\":18") != std::string::npos &&
      text.find("\"run_high\":19") != std::string::npos && text.find("\"run_high\":20") != std::string::npos &&
      text.find("\"timer_invocation\":0") != std::string::npos && text.find("\"timer_invocation\":1234") != std::string::npos,
      "host scope request SDK row and SDK header identities are never equated or repaired");
  Check(text.find("\"admission\":4,\"completion\":3") != std::string::npos &&
      text.find("\"result\":18446744073709551615,\"trace_ordinal\":0") != std::string::npos &&
      text.find("\"result_available\":0") != std::string::npos && text.find("\"used_query\":0") != std::string::npos,
      "aborted unavailable contradictory receipts retain values rather than favorable filtering");
  Check(text.find("\"proof_flags\":4294967295") != std::string::npos && Count(text,":false") == 6,
      "raw nonzero SDK proof flags do not overwrite or get replaced by static false claims");
  Check(text.find("9223372036854775806") == std::string::npos && text.find("9223372036854775805") == std::string::npos,
      "unused rows and C++ padding remain outside declared inventory");

  struct Expected { const char* name; size_t count; };
  const Expected fields[] = {
    {"schema",1},{"token",1},{"runtime_admitted",1},{"complete_clock_history_proven",1},
    {"source_frame_binding_proven",1},{"simulation_proven",1},{"fps_proven",1},{"actual_clock_cause_proven",1},
    {"host",1},{"sdk",1},{"run",1},{"high",1},{"low",1},{"counts",1},{"size",2},{"rows",3},
    {"observed_scopes",1},{"stored_scopes",1},{"completed_scopes",1},{"normal_scopes",1},
    {"unwound_scopes",1},{"aborted_scopes",1},{"zero_read_scopes",1},{"lost_scopes",1},
    {"trace_loss_scopes",1},{"ordinal_saturated_scopes",1},{"trace_counter_saturated_scopes",1},
    {"diagnostic_scopes",1},{"counter_saturated",3},{"inconclusive",1},
    {"storage_ordinal",1},{"invocation",1},{"disposition",3},{"outcome",1},{"flags",7},{"reserved",9},{"trace",1},
    {"timer_ordinal",1},{"phase_call_id",7},{"phase_thread_epoch",7},{"native_thread_identity",1},
    {"timer_address",1},{"caller_address",1},{"phase_native_thread_id",7},{"phase_present",7},
    {"observed_reads",1},{"lost_rows",1},{"overflow",1},{"ordinal_saturated",1},
    {"scope_binding",2},{"request",2},{"status",2},{"result",2},{"trace_ordinal",2},
    {"decisions",2},{"result_available",2},{"threw",2},{"used_query",2},{"read_ordinal_available",2},{"status_available",2},
    {"abi_version",9},{"struct_size",9},{"run_high",7},{"run_low",7},{"timer_invocation",6},{"read_ordinal",6},
    {"admission",2},{"completion",2},{"issues",4},
    {"capacity",1},{"row_count",1},{"observed_bound_reads",1},{"stored_entries",1},{"published_entries",1},
    {"aborted_entries",1},{"overflow_entries",1},{"invalid_bindings",1},{"diagnostic_entries",1},
    {"saturated_rejections",1},{"missing_witnesses",1},{"duplicate_witnesses",1},{"proof_flags",1},
    {"admission_ordinal",2},{"binding",2},{"host_sample",2},{"returned_guest",2},
    {"ratio_numerator",2},{"ratio_denominator",2},{"pre_last_host",2},{"pre_last_guest",2},
    {"post_last_host",2},{"post_last_guest",2},{"presence",2},{"branch",2},
  };
  size_t total = 0;
  for (const auto& field : fields) {
    const auto actual = Count(text,std::string("\"") + field.name + "\":");
    if (actual != field.count) std::cerr << "FIELD " << field.name << " expected=" << field.count << " actual=" << actual << '\n';
    Check(actual == field.count,"exact raw JSON field inventory count"); total += field.count;
  }
  Check(Count(text,"\":") == total,"no undeclared renamed or duplicate JSON field remains outside inventory");
  std::cout << "RAW_JSON " << text << '\n';
}

void AllStatusesAndOutcomes() {
  auto h = std::make_unique<json::HostSnapshot>();
  auto s = std::make_unique<MclaClockWitnessSnapshotV2>();
  h->size = 4; unsigned index = 0;
  for (uint32_t admission = 0; admission <= MCLA_CW_INVALID_ABI; ++admission)
    for (uint32_t completion = 0; completion <= MCLA_CW_DUPLICATE_WITNESS; ++completion) {
      auto& record = h->rows[index / 16]; auto& receipt = record.trace.rows[index % 16];
      ++record.trace.size; receipt.status = {1,24,admission,completion,0,0};
      receipt.result = index; ++index;
    }
  for (uint32_t i = 0; i < 4; ++i) {
    h->rows[i].disposition = static_cast<journal::Disposition>(i);
    h->rows[i].outcome = static_cast<journal::Outcome>(i);
  }
  s->row_count = 5;
  for (uint32_t i = 0; i < 5; ++i) { s->rows[i].disposition = i; s->rows[i].branch = i; }
  const auto text = json::Serialize(*h,*s,kToken);
  Check(index == 54 && Count(text,"\"status\":") == 54,"all54 admission completion combinations retain distinct rows");
  for (uint32_t admission = 0; admission <= MCLA_CW_INVALID_ABI; ++admission)
    for (uint32_t completion = 0; completion <= MCLA_CW_DUPLICATE_WITNESS; ++completion)
      Check(Count(text,"\"admission\":" + std::to_string(admission) + ",\"completion\":" + std::to_string(completion) + ",") == 1,
          "no stopped unbound invalid aborted missing or duplicate status is filtered");
  for (unsigned i = 0; i < 4; ++i)
    Check(text.find("\"outcome\":" + std::to_string(i)) != std::string::npos,
        "all raw host outcomes including unfinished and abandoned are retained");
}

void DistinctFieldMapping() {
  // Distinct small values catch swapped member wiring that all-MAX fixtures
  // cannot distinguish. These helper objects use the same emission functions.
  const MclaClockWitnessBindingV1 binding{1,2,3,4,5,6,7,8,9,10,11,12};
  const std::string binding_json = "{\"abi_version\":1,\"struct_size\":2,\"run_high\":3,\"run_low\":4,"
      "\"timer_invocation\":5,\"read_ordinal\":6,\"phase_call_id\":7,\"phase_thread_epoch\":8,"
      "\"phase_native_thread_id\":9,\"phase_present\":10,\"flags\":11,\"reserved\":12}";
  json::detail::Writer b; json::detail::Binding(b,binding);
  Check(b.Take() == binding_json,"binding members map to exact named decimal fields");
  const MclaClockWitnessStatusV1 status{1,2,3,4,5,6};
  const std::string status_json = "{\"abi_version\":1,\"struct_size\":2,\"admission\":3,\"completion\":4,\"issues\":5,\"reserved\":6}";
  json::detail::Writer st; json::detail::Status(st,status);
  Check(st.Take() == status_json,"status members map to exact fields without health interpretation");
  const journal::Invocation invocation{1,2,3,4,5,6,7,8};
  json::detail::Writer inv; json::detail::Invocation(inv,invocation);
  Check(inv.Take() == "{\"timer_ordinal\":1,\"phase_call_id\":2,\"phase_thread_epoch\":3,\"native_thread_identity\":4,"
      "\"timer_address\":5,\"caller_address\":6,\"phase_native_thread_id\":7,\"phase_present\":8}",
      "invocation storage fields do not replace exact timer or phase identities");
  const journal::Counts counts{1,2,3,4,5,6,7,8,9,10,11,12,13,14};
  json::detail::Writer c; json::detail::Counts(c,counts);
  Check(c.Take() == "{\"observed_scopes\":1,\"stored_scopes\":2,\"completed_scopes\":3,\"normal_scopes\":4,"
      "\"unwound_scopes\":5,\"aborted_scopes\":6,\"zero_read_scopes\":7,\"lost_scopes\":8,\"trace_loss_scopes\":9,"
      "\"ordinal_saturated_scopes\":10,\"trace_counter_saturated_scopes\":11,\"diagnostic_scopes\":12,"
      "\"counter_saturated\":13,\"inconclusive\":14}","all host counters retain their own distinct values");
  const host::Receipt receipt{binding,binding,status,1,2,3,4,5,6,7,8};
  json::detail::Writer r; json::detail::Receipt(r,receipt);
  Check(r.Take() == "{\"scope_binding\":" + binding_json + ",\"request\":" + binding_json + ",\"status\":" + status_json +
      ",\"result\":1,\"trace_ordinal\":2,\"decisions\":3,\"result_available\":4,\"threw\":5,\"used_query\":6,"
      "\"read_ordinal_available\":7,\"status_available\":8}","all receipt raw values and availability flags have exact mapping");
  const MclaClockWitnessRecordV1 record{1,binding,2,3,4,5,6,7,8,9,10,11,12,13};
  json::detail::Writer sdk_record; json::detail::SdkRecord(sdk_record,record);
  Check(sdk_record.Take() == "{\"admission_ordinal\":1,\"binding\":" + binding_json +
      ",\"host_sample\":2,\"returned_guest\":3,\"ratio_numerator\":4,\"ratio_denominator\":5,"
      "\"pre_last_host\":6,\"pre_last_guest\":7,\"post_last_host\":8,\"post_last_guest\":9,"
      "\"presence\":10,\"branch\":11,\"disposition\":12,\"issues\":13}","all SDK sample branch state and return fields map exactly");
  auto sdk = std::make_unique<MclaClockWitnessSnapshotV2>();
  sdk->abi_version = 1; sdk->struct_size = 2; sdk->capacity = 3; sdk->row_count = 0;
  sdk->run_high = 5; sdk->run_low = 6; sdk->observed_bound_reads = 7; sdk->stored_entries = 8;
  sdk->published_entries = 9; sdk->aborted_entries = 10; sdk->overflow_entries = 11;
  sdk->invalid_bindings = 12; sdk->diagnostic_entries = 13; sdk->saturated_rejections = 14;
  sdk->missing_witnesses = 15; sdk->duplicate_witnesses = 16; sdk->counter_saturated = 17; sdk->proof_flags = 18;
  json::detail::Writer sdk_header; json::detail::Sdk(sdk_header,*sdk);
  Check(sdk_header.Take() == "{\"abi_version\":1,\"struct_size\":2,\"capacity\":3,\"row_count\":0,\"run_high\":5,\"run_low\":6,"
      "\"observed_bound_reads\":7,\"stored_entries\":8,\"published_entries\":9,\"aborted_entries\":10,"
      "\"overflow_entries\":11,\"invalid_bindings\":12,\"diagnostic_entries\":13,\"saturated_rejections\":14,"
      "\"missing_witnesses\":15,\"duplicate_witnesses\":16,\"counter_saturated\":17,\"proof_flags\":18,\"rows\":[]}",
      "all SDK headers counters and raw proof bits retain distinct values");
}

void MaximumPhysicalInventoryAndCap() {
  auto h = std::make_unique<json::HostSnapshot>();
  auto s = std::make_unique<MclaClockWitnessSnapshotV2>();
  MaxHeaders(*h,*s); h->size = 1024; s->row_count = MCLA_CW_CAPACITY_V2;
  for (auto& record : h->rows) MaxHostRecord(record,16);
  for (auto& record : s->rows) record = MaxSdkRecord();
  h->rows[1023].storage_ordinal = 9876543210ull;
  h->rows[1023].trace.rows[15].result = 9876543211ull;
  s->rows[4095].admission_ordinal = 9876543212ull;
  const auto text = json::Serialize(*h,*s,kToken);
  Check(text.size() <= json::kMaxOutputBytes && Count(text,"\"storage_ordinal\":") == 1024 &&
      Count(text,"\"trace_ordinal\":") == 16384 && Count(text,"\"admission_ordinal\":") == 4096,
      "full physical inventory with maximum width integers fits cap without dropped rows");
  Check(text.find("9876543210") != std::string::npos && text.find("9876543211") != std::string::npos &&
      text.find("9876543212") != std::string::npos,"last host scope last read and last SDK row survive full inventory");
  std::cout << "MAX_JSON_BYTES " << text.size() << '\n';
}
void OutputCapGuard() {
  // Synthetic Writer boundary, not a claim that valid fixed-capacity snapshots
  // can reach arbitrary output sizes. The same checked append owns every byte.
  json::detail::Writer exact;
  exact.Add(std::string(json::kMaxOutputBytes,'x'));
  Rejects<std::length_error>([&] { exact.Add("y"); },"append beyond24MiB rejects without truncating prior bytes");
  Check(exact.Take().size() == json::kMaxOutputBytes,"exact24MiB append is permitted");
  json::detail::Writer oversized;
  Rejects<std::length_error>([&] { oversized.Add(std::string(json::kMaxOutputBytes + 1,'x')); },
      "single oversized append rejects rather than silently truncating");
  Check(oversized.Take().empty(),"rejected first append leaves output empty");
}
}  // namespace

int main() {
  EmptyAndTokens(); NaturalBoundsOnly(); RepresentativeAndExactFields();
  AllStatusesAndOutcomes(); DistinctFieldMapping(); MaximumPhysicalInventoryAndCap(); OutputCapGuard();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
