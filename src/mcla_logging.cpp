// mcla - project-owned structured logging categories

#include "mcla_logging.h"

#include <array>

namespace mcla::logging {

namespace {

rex::LogCategoryId Register(const char *name) { return rex::RegisterLogCategory(name); }

}  // namespace

rex::LogCategoryId App() {
  static const auto id = Register("app");
  return id;
}

rex::LogCategoryId Ppc() {
  static const auto id = Register("ppc");
  return id;
}

rex::LogCategoryId Kernel() {
  static const auto id = Register("kernel");
  return id;
}

rex::LogCategoryId Xam() {
  static const auto id = Register("xam");
  return id;
}

rex::LogCategoryId Vfs() {
  static const auto id = Register("vfs");
  return id;
}

rex::LogCategoryId Gpu() {
  static const auto id = Register("gpu");
  return id;
}

rex::LogCategoryId Audio() {
  static const auto id = Register("audio");
  return id;
}

rex::LogCategoryId Input() {
  static const auto id = Register("input");
  return id;
}

rex::LogCategoryId Patches() {
  static const auto id = Register("patches");
  return id;
}

std::span<const CategoryDescriptor> Categories() {
  static const std::array categories = {
      CategoryDescriptor{"app", App()},         CategoryDescriptor{"ppc", Ppc()},
      CategoryDescriptor{"kernel", Kernel()},   CategoryDescriptor{"xam", Xam()},
      CategoryDescriptor{"vfs", Vfs()},         CategoryDescriptor{"gpu", Gpu()},
      CategoryDescriptor{"audio", Audio()},     CategoryDescriptor{"input", Input()},
      CategoryDescriptor{"patches", Patches()},
  };
  return categories;
}

void EmitSchemaProbe() {
  for (const auto &category : Categories()) {
    REXLOG_CAT_INFO(category.id, "MCLA_LOG_SCHEMA schema=1 category={} event=probe", category.name);
  }
}

}  // namespace mcla::logging
