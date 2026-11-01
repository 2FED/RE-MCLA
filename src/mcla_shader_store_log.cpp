// M10-003 host store counts. See mcla_shader_store_log.h.

#include "mcla_shader_store_log.h"

#include "mcla_logging.h"

#include <fstream>
#include <system_error>
#include <vector>

namespace mcla::shader_store {

void LogStartupStore(const std::filesystem::path &cache_root) {
  if (cache_root.empty()) return;
  try {
    const auto directory = cache_root / "shaders" / "shareable";
    int64_t records[kFamilyCount] = {};
    uint64_t total = 0;
    for (size_t i = 0; i < kFamilyCount; ++i) {
      const auto path = directory / std::wstring(kFamilies[i].file);
      std::error_code ec;
      if (!std::filesystem::is_regular_file(path, ec)) {
        records[i] = (ec || std::filesystem::exists(path, ec)) ? -2 : -1;
        continue;
      }
      const auto size = std::filesystem::file_size(path, ec);
      std::ifstream in(path, std::ios::binary);
      if (ec || size > kMaxFileBytes || !in) {
        records[i] = -2;
        continue;
      }
      std::vector<uint8_t> bytes(static_cast<size_t>(size));
      in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
      bytes.resize(static_cast<size_t>(in.gcount()));
      total += bytes.size();
      const auto parsed = Parse(static_cast<Family>(i), bytes);
      records[i] = parsed.header == HeaderStatus::kValid ? static_cast<int64_t>(parsed.records.size()) : 0;
    }
    MCLA_APP_INFO("{}", StartupLine(records, total));
  } catch (const std::exception &) {
    MCLA_APP_INFO("MCLA shader store at startup: not counted (error)");
  }
}

}  // namespace mcla::shader_store
