// mcla - project-owned structured logging categories

#pragma once

#include <rex/logging.h>

#include <span>
#include <string_view>

namespace mcla::logging {

struct CategoryDescriptor {
  std::string_view name;
  rex::LogCategoryId id;
};

rex::LogCategoryId App();
rex::LogCategoryId Ppc();
rex::LogCategoryId Kernel();
rex::LogCategoryId Xam();
rex::LogCategoryId Vfs();
rex::LogCategoryId Gpu();
rex::LogCategoryId Audio();
rex::LogCategoryId Input();
rex::LogCategoryId Patches();

std::span<const CategoryDescriptor> Categories();
void EmitSchemaProbe();

}  // namespace mcla::logging

#define MCLA_APP_INFO(...) REXLOG_CAT_INFO(::mcla::logging::App(), __VA_ARGS__)
#define MCLA_APP_ERROR(...) REXLOG_CAT_ERROR(::mcla::logging::App(), __VA_ARGS__)
#define MCLA_PPC_INFO(...) REXLOG_CAT_INFO(::mcla::logging::Ppc(), __VA_ARGS__)
#define MCLA_PPC_ERROR(...) REXLOG_CAT_ERROR(::mcla::logging::Ppc(), __VA_ARGS__)
#define MCLA_VFS_INFO(...) REXLOG_CAT_INFO(::mcla::logging::Vfs(), __VA_ARGS__)
#define MCLA_VFS_ERROR(...) REXLOG_CAT_ERROR(::mcla::logging::Vfs(), __VA_ARGS__)
#define MCLA_GPU_INFO(...) REXLOG_CAT_INFO(::mcla::logging::Gpu(), __VA_ARGS__)
#define MCLA_GPU_ERROR(...) REXLOG_CAT_ERROR(::mcla::logging::Gpu(), __VA_ARGS__)
