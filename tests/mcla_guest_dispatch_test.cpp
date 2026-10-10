// Guest-free registration/lookup regression, linked to the real generated corpus.
// No Runtime/ReXApp setup, guest invocation, graphics, window, profile or game I/O.
// SDK Memory::Initialize does reserve guest address space and commit its backing;
// this is not a zero-allocation/source-only check. Logging has no file sink.
// A PASS proves exact mapping/lookup consistency, not the legitimacy of a target
// or recovery of the owner's championship/Pinkslip gameplay route.
#include "mcla_init.h"

#include <rex/system/export_resolver.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/xmemory.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>

namespace {
struct Probe {
  uint32_t guest;
  PPCFunc* known_host;
  std::size_t entries = 0;
  std::size_t nonnull_entries = 0;
  PPCFunc* mapped_host = nullptr;
};

// Keep logging alive until after the dispatcher and Memory have been destroyed.
struct LoggingScope {
  LoggingScope() {
    rex::LogConfig config;
    config.default_level = spdlog::level::err;
    config.log_to_console = true;
    config.log_file = nullptr;
    config.app_name.clear(); // An app name would enable an automatic file sink.
    rex::InitLogging(config);
  }
  ~LoggingScope() { rex::ShutdownLogging(); }
};

int Run() {
  const auto& image = PPCImageConfig;
  if (!image.func_mappings || image.func_mappings != PPCFuncMappings ||
      image.code_base != REX_CODE_BASE || image.code_size != REX_CODE_SIZE ||
      image.image_base != REX_IMAGE_BASE || image.image_size != REX_IMAGE_SIZE ||
      rex::runtime::FunctionDispatcher::kThunkReserveSize != REX_THUNK_RESERVE_SIZE) {
    std::fprintf(stderr, "FAIL: linked image/mappings/header layout disagree\n");
    return 2;
  }

  std::array<Probe, 3> probes{{
      {0x822B88A8u, &sub_822B88A8},
      {0x822B88C8u, &sub_822B88C8},
      {0x822B88B8u, nullptr}, // Required target; never insert a dummy registration.
  }};

  // Match the generated macro's bounds guard before ever dereferencing its slot.
  for (const auto& probe : probes) {
    if ((probe.guest & 3u) != 0 ||
        static_cast<uint32_t>(probe.guest - REX_CODE_BASE) >=
            REX_CODE_SIZE + REX_THUNK_RESERVE_SIZE) {
      std::fprintf(stderr, "FAIL: probe 0x%08X is outside aligned image slots\n",
                   static_cast<unsigned>(probe.guest));
      return 2;
    }
  }

  LoggingScope logging;
  rex::memory::Memory memory;
  if (!memory.Initialize()) {
    std::fprintf(stderr, "FAIL: SDK memory initialization\n");
    return 2;
  }
  rex::runtime::ExportResolver resolver;
  rex::runtime::FunctionDispatcher dispatcher(&memory, &resolver);
  if (!dispatcher.InitializeFunctionTable(image.code_base, image.code_size,
                                          image.image_base, image.image_size,
                                          /*is_entrypoint=*/true)) {
    std::fprintf(stderr, "FAIL: SDK image function-table initialization\n");
    return 2;
  }

  std::size_t entries = 0;
  std::size_t registered = 0;
  std::size_t null_hosts = 0;
  std::size_t duplicates = 0;
  std::size_t rejected = 0;
  // Replay Runtime::Setup's real PPCImageInfo registration sequence, including
  // null-host skipping and duplicate detection. No test-created guest mappings.
  for (const auto* mapping = image.func_mappings; mapping->guest != 0; ++mapping) {
    ++entries;
    const auto guest = static_cast<uint32_t>(mapping->guest);
    auto* host = mapping->host;
    for (auto& probe : probes) {
      if (mapping->guest == probe.guest) {
        ++probe.entries;
        if (host) {
          ++probe.nonnull_entries;
          probe.mapped_host = host;
        }
      }
    }
    if (!host) {
      ++null_hosts;
      continue;
    }
    if (dispatcher.GetFunction(guest)) ++duplicates;
    if (!dispatcher.SetFunction(guest, host)) {
      ++rejected;
    } else {
      ++registered;
    }
  }
  std::printf("registration: entries=%zu registered=%zu null_hosts=%zu "
              "duplicates=%zu rejected=%zu\n",
              entries, registered, null_hosts, duplicates, rejected);

  bool passed = rejected == 0;
  for (const auto& probe : probes) {
    auto* lookup = dispatcher.GetFunction(probe.guest);
    auto* slot = REX_LOOKUP_FUNC(memory.virtual_membase(), probe.guest);
    const bool exact_mapping = probe.entries == 1 && probe.nonnull_entries == 1;
    const bool known_host_matches = !probe.known_host || probe.mapped_host == probe.known_host;
    const bool matches = exact_mapping && known_host_matches && lookup && slot &&
                         lookup == probe.mapped_host && slot == probe.mapped_host;
    std::printf("%s: guest=0x%08X entries=%zu nonnull=%zu mapping=%s "
                "dispatcher=%s slot=%s exact_host_match=%s\n",
                matches ? "PASS" : "FAIL", static_cast<unsigned>(probe.guest),
                probe.entries, probe.nonnull_entries,
                probe.mapped_host ? "NONNULL" : "NULL",
                lookup ? "NONNULL" : "NULL", slot ? "NONNULL" : "NULL",
                matches ? "yes" : "no");
    passed = matches && passed;
  }
  std::printf("%s: generated registration/lookup only; no guest execution or gameplay claim\n",
              passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
} // namespace

int main() {
  int result = 2;
  try {
    result = Run();
  } catch (const std::exception&) {
    // Bounded diagnostic; do not echo arbitrary paths or SDK exception payloads.
    std::fprintf(stderr, "FAIL: SDK registration/lookup test threw an exception\n");
  } catch (...) {
    std::fprintf(stderr, "FAIL: SDK registration/lookup test threw an unknown exception\n");
  }
  return std::fflush(stdout) == 0 ? result : 2;
}
