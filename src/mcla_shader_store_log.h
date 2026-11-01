#pragma once

// M10-003: one host log line at startup with the valid record counts of the
// shader store the session starts with (after the launcher seeding, before
// the SDK reads it). See docs/evidence/M10-prewarm-union-store-20261101.md.

#include "mcla_shader_store_seed.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace mcla::shader_store {

// Pure: the line for per-family valid record counts (-1 = file absent,
// -2 = unreadable; a file with another header counts 0).
inline std::string StartupLine(const int64_t (&records)[kFamilyCount], uint64_t bytes) {
  std::string line = "MCLA shader store at startup:";
  for (size_t i = 0; i < kFamilyCount; ++i) {
    line += " ";
    line += kFamilies[i].label;
    line += "=";
    line += records[i] == -1 ? std::string("absent")
            : records[i] == -2 ? std::string("unreadable")
                               : std::to_string(records[i]);
  }
  line += " bytes=" + std::to_string(bytes);
  return line;
}

// Reads <cache_root>/shaders/shareable/<store files> (bounded, read-only) and
// logs "MCLA shader store at startup: ...". Does nothing for an empty root.
void LogStartupStore(const std::filesystem::path &cache_root);

}  // namespace mcla::shader_store
