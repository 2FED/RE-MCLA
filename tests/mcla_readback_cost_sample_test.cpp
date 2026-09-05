#include "mcla_readback_cost_sample.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>

namespace {
using mcla::diagnostics::RuntimeState;
using mcla::diagnostics::WriteReadbackCostSample;

unsigned checks = 0;
unsigned failures = 0;

void Check(std::string_view label, bool condition) {
  ++checks;
  if (!condition) {
    ++failures;
    std::cerr << "FAIL " << label << '\n';
  }
}

struct CounterField {
  std::string_view key;
  uint64_t RuntimeState::*member;
};

constexpr CounterField counter_fields[] = {
    {"guest_output_sequence", &RuntimeState::guest_output_sequence},
    {"guest_vblank_sequence", &RuntimeState::guest_vblank_sequence},
    {"frames_closed", &RuntimeState::graphics_guest_frames_closed},
    {"draw_packet_count", &RuntimeState::graphics_command_draw_packet_count},
    {"draw_packet_us", &RuntimeState::graphics_command_draw_packet_us},
    {"primary_buffer_count", &RuntimeState::graphics_command_primary_buffer_count},
    {"primary_buffer_us", &RuntimeState::graphics_command_primary_buffer_us},
    {"resolve_count", &RuntimeState::graphics_vulkan_resolve_count},
    {"resolve_us", &RuntimeState::graphics_vulkan_resolve_us},
    {"resolve_readback_copy_count",
     &RuntimeState::graphics_vulkan_resolve_readback_copy_count},
    {"resolve_readback_bytes", &RuntimeState::graphics_vulkan_resolve_readback_bytes},
    {"resolve_readback_copy_us",
     &RuntimeState::graphics_vulkan_resolve_readback_copy_us},
    {"resolve_readback_wait_count",
     &RuntimeState::graphics_vulkan_resolve_readback_wait_count},
    {"resolve_readback_wait_us",
     &RuntimeState::graphics_vulkan_resolve_readback_wait_us},
    {"watch_arm_calls", &RuntimeState::graphics_memory_watch_arm_calls},
    {"watch_arm_us", &RuntimeState::graphics_memory_watch_arm_us},
    {"watch_disarm_calls", &RuntimeState::graphics_memory_watch_disarm_calls},
    {"watch_disarm_us", &RuntimeState::graphics_memory_watch_disarm_us},
    {"non_mmio_write_fault_count",
     &RuntimeState::graphics_memory_non_mmio_write_fault_count},
    {"non_mmio_write_fault_us",
     &RuntimeState::graphics_memory_non_mmio_write_fault_us},
    {"watch_protect_failures",
     &RuntimeState::graphics_memory_watch_protect_failures},
    {"binding_update_count", &RuntimeState::graphics_vulkan_binding_update_count},
    {"binding_update_us", &RuntimeState::graphics_vulkan_binding_update_us},
};

bool ContainsNumber(const std::string &text, std::string_view key, uint64_t value) {
  const std::string field =
      "\"" + std::string(key) + "\":" + std::to_string(value);
  const auto offset = text.find(field);
  if (offset == std::string::npos) {
    return false;
  }
  const auto end = offset + field.size();
  return end < text.size() && (text[end] == ',' || text[end] == '}') &&
         text.find("\"" + std::string(key) + "\":", end) == std::string::npos;
}

class GroupedNumbers final : public std::numpunct<char> {
protected:
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};

class ShortWriteBuffer final : public std::streambuf {
public:
  std::string bytes;

protected:
  std::streamsize xsputn(const char *data, std::streamsize size) override {
    const auto accepted = std::min<std::streamsize>(size, 16);
    bytes.append(data, static_cast<size_t>(accepted));
    return accepted;
  }
};
} // namespace

int main() {
  RuntimeState state;
  state.runtime_available = true;
  state.title_id = 1234567890;
  state.graphics_backend = 2;
  uint64_t value = 100;
  for (const auto &field : counter_fields) {
    state.*field.member = value++;
  }

  std::ostringstream representative;
  Check("representative write",
        WriteReadbackCostSample(representative, "begin", "fast", 1234567, state));
  const std::string expected =
      "{\"schema\":\"mcla-readback-cost-sample-v1\","
      "\"diagnostic_only\":true,\"audio_counters_available\":false,"
      "\"edge\":\"begin\",\"mode\":\"fast\",\"monotonic_us\":1234567,"
      "\"title_id\":1234567890,\"backend\":2,\"guest_output_sequence\":100,"
      "\"guest_vblank_sequence\":101,\"frames_closed\":102,"
      "\"draw_packet_count\":103,\"draw_packet_us\":104,"
      "\"primary_buffer_count\":105,\"primary_buffer_us\":106,"
      "\"resolve_count\":107,\"resolve_us\":108,"
      "\"resolve_readback_copy_count\":109,\"resolve_readback_bytes\":110,"
      "\"resolve_readback_copy_us\":111,\"resolve_readback_wait_count\":112,"
      "\"resolve_readback_wait_us\":113,\"watch_arm_calls\":114,"
      "\"watch_arm_us\":115,\"watch_disarm_calls\":116,\"watch_disarm_us\":117,"
      "\"non_mmio_write_fault_count\":118,\"non_mmio_write_fault_us\":119,"
      "\"watch_protect_failures\":120,\"binding_update_count\":121,"
      "\"binding_update_us\":122}\n";
  Check("exact JSON schema and member mapping", representative.str() == expected);

  for (const std::string_view edge : {"begin", "end"}) {
    for (const std::string_view mode : {"fast", "some"}) {
      std::ostringstream output;
      Check("all valid edge/mode combinations",
            WriteReadbackCostSample(output, edge, mode, 1, state));
      const std::string line = output.str();
      Check("one JSON line", std::count(line.begin(), line.end(), '\n') == 1);
    }
  }

  struct InvalidInput {
    std::string_view label;
    std::string_view edge;
    std::string_view mode;
    uint64_t time;
    bool available;
  };
  const InvalidInput invalid[] = {
      {"empty edge", "", "fast", 1, true},
      {"unknown edge", "middle", "fast", 1, true},
      {"edge case", "BEGIN", "fast", 1, true},
      {"edge whitespace", "begin ", "fast", 1, true},
      {"edge quote injection", "begin\",\"extra\":true", "fast", 1, true},
      {"edge newline injection", "begin\nend", "fast", 1, true},
      {"edge embedded NUL", std::string_view("begin\0end", 9), "fast", 1, true},
      {"empty mode", "begin", "", 1, true},
      {"disabled mode", "begin", "none", 1, true},
      {"full mode", "begin", "full", 1, true},
      {"unsupported auto", "begin", "auto", 1, true},
      {"mode case", "begin", "FAST", 1, true},
      {"mode whitespace", "begin", "some ", 1, true},
      {"mode quote injection", "begin", "fast\",\"extra\":true", 1, true},
      {"mode newline injection", "begin", "some\nfast", 1, true},
      {"mode embedded NUL", "begin", std::string_view("fast\0some", 9), 1, true},
      {"zero time", "begin", "fast", 0, true},
      {"runtime unavailable", "end", "some", 1, false},
      {"all invalid", "bad", "bad", 0, false},
  };
  for (const auto &input : invalid) {
    RuntimeState invalid_state = state;
    invalid_state.runtime_available = input.available;
    std::ostringstream output;
    output << "unchanged-prefix";
    Check(input.label, !WriteReadbackCostSample(output, input.edge, input.mode,
                                              input.time, invalid_state));
    Check("invalid input writes no partial record",
          output.str() == "unchanged-prefix");
    Check("invalid input leaves stream healthy", output.good());
  }

  std::ostringstream formatted;
  formatted.imbue(std::locale(std::locale::classic(), new GroupedNumbers));
  formatted << std::hex << std::showbase << std::boolalpha << std::setw(40)
            << std::setfill('*');
  const auto prior_flags = formatted.flags();
  Check("write with hostile output formatting",
        WriteReadbackCostSample(formatted, "begin", "fast", 1234567, state));
  Check("locale and flags cannot corrupt JSON", formatted.str() == expected);
  Check("destination flags unchanged", formatted.flags() == prior_flags);
  Check("destination width unchanged", formatted.width() == 40);

  const auto maximum = std::numeric_limits<uint64_t>::max();
  RuntimeState maximum_state;
  maximum_state.runtime_available = true;
  maximum_state.title_id = std::numeric_limits<uint32_t>::max();
  maximum_state.graphics_backend = std::numeric_limits<uint32_t>::max();
  for (const auto &field : counter_fields) {
    maximum_state.*field.member = maximum;
  }
  std::ostringstream maximum_output;
  Check("maximum values serialize", WriteReadbackCostSample(
                                        maximum_output, "end", "some", maximum,
                                        maximum_state));
  Check("maximum timestamp exact",
        ContainsNumber(maximum_output.str(), "monotonic_us", maximum));
  for (const auto &field : counter_fields) {
    Check(field.key, ContainsNumber(maximum_output.str(), field.key, maximum));
  }
  Check("maximum title exact",
        ContainsNumber(maximum_output.str(), "title_id", 4294967295ULL));
  Check("unknown backend preserved",
        ContainsNumber(maximum_output.str(), "backend", 4294967295ULL));

  RuntimeState zero_state;
  zero_state.runtime_available = true;
  std::ostringstream zero_output;
  Check("zero counters are allowed",
        WriteReadbackCostSample(zero_output, "begin", "fast", 1, zero_state));
  for (const auto &field : counter_fields) {
    Check(field.key, ContainsNumber(zero_output.str(), field.key, 0));
  }

  std::ostringstream bad_output;
  bad_output << "unchanged-prefix";
  bad_output.setstate(std::ios::badbit);
  Check("preexisting bad stream fails",
        !WriteReadbackCostSample(bad_output, "begin", "fast", 1, state));
  Check("preexisting bad stream untouched", bad_output.str() == "unchanged-prefix");

  ShortWriteBuffer short_buffer;
  std::ostream short_output(&short_buffer);
  Check("short write fails",
        !WriteReadbackCostSample(short_output, "begin", "fast", 1, state));
  Check("partial output is not a complete record",
        short_buffer.bytes.size() == 16 &&
            short_buffer.bytes.find('\n') == std::string::npos);

  ShortWriteBuffer throwing_buffer;
  std::ostream throwing_output(&throwing_buffer);
  throwing_output.exceptions(std::ios::badbit | std::ios::failbit);
  bool returned_false = false;
  bool threw = false;
  try {
    returned_false =
        !WriteReadbackCostSample(throwing_output, "begin", "fast", 1, state);
  } catch (...) {
    threw = true;
  }
  Check("stream exception reports failure", returned_false && !threw);

  std::cout << "Readback cost sample: " << checks << " checks, " << failures
            << " failures\n";
  return failures ? 1 : 0;
}
