// mcla - privacy-bounded live diagnostics and crash package orchestration

#include "mcla_diagnostics.h"

#include "mcla_frame_signal.h"
#include "mcla_native_crash.h"
#if defined(MCLA_EXPERIMENTAL_F10_WGC)
#include "mcla_diagnostic_image_client_win.h"
#include "mcla_image_helper_pin.h"
#endif

#include <rex/crypto/sha256.h>
#include <rex/filesystem/file.h>
#include <rex/logging.h>
#include <rex/logging/sink.h>

#include <spdlog/sinks/rotating_file_sink.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>

#include <Psapi.h>
#include <TlHelp32.h>
#include <bcrypt.h>
#else
#include <unistd.h>
#endif

#ifndef MCLA_VERSION
#define MCLA_VERSION "0.0.0.0"
#endif

namespace mcla::diagnostics {
namespace {

constexpr uint64_t kMaxFrameBytes = 64ull * 1024 * 1024;
constexpr uint64_t kMaxSaveBytes = 64ull * 1024 * 1024;
constexpr uint64_t kMaxSaveFileBytes = 16ull * 1024 * 1024;
constexpr uint32_t kMaxSaveEntries = 4096;
constexpr uint64_t kMaxLiveDumpBytes = 128ull * 1024 * 1024;
constexpr size_t kMaxLogEntries = 512;
constexpr size_t kMaxLogBytes = 256 * 1024;
constexpr size_t kLiveRetentionCount = 10;
constexpr uint64_t kLiveRetentionBytes = 256ull * 1024 * 1024;
constexpr uint64_t kJournalFileBytes = 8ull * 1024 * 1024;
constexpr size_t kJournalRotatedFiles = 2;
constexpr size_t kRuntimeRetentionCount = 12;
constexpr uint64_t kRuntimeRetentionBytes = 64ull * 1024 * 1024;
constexpr auto kPartialRetentionAge = std::chrono::hours(24);

struct NativeThreadSample {
  uint32_t id = 0;
  int32_t base_priority = 0;
  uint64_t kernel_time_100ns = 0;
  uint64_t user_time_100ns = 0;
  std::string name;
};

#if defined(_WIN32)
uint64_t FileTimeTicks(const FILETIME &value) {
  ULARGE_INTEGER ticks{};
  ticks.LowPart = value.dwLowDateTime;
  ticks.HighPart = value.dwHighDateTime;
  return ticks.QuadPart;
}

std::string ThreadDescription(HANDLE thread) {
  using GetThreadDescriptionFunction = HRESULT(WINAPI *)(HANDLE, PWSTR *);
  const auto function =
      reinterpret_cast<GetThreadDescriptionFunction>(GetProcAddress(
          GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription"));
  if (!function) {
    return {};
  }
  PWSTR description = nullptr;
  if (FAILED(function(thread, &description)) || !description) {
    return {};
  }
  const int bytes =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, description, -1,
                          nullptr, 0, nullptr, nullptr);
  std::string result;
  if (bytes > 1) {
    result.resize(static_cast<size_t>(bytes));
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, description, -1,
                        result.data(), bytes, nullptr, nullptr);
    result.pop_back();
  }
  LocalFree(description);
  return result;
}
#endif

std::string PathUtf8(const std::filesystem::path &path) {
#if defined(_WIN32)
  const auto value = path.u8string();
  return std::string(reinterpret_cast<const char *>(value.data()),
                     value.size());
#else
  return path.string();
#endif
}

std::string GenericPathUtf8(const std::filesystem::path &path) {
  const auto value = path.generic_u8string();
  return std::string(reinterpret_cast<const char *>(value.data()),
                     value.size());
}

std::string JsonEscape(std::string_view value) {
  std::string result;
  result.reserve(value.size() + 16);
  for (const unsigned char c : value) {
    switch (c) {
    case '\\':
      result += "\\\\";
      break;
    case '"':
      result += "\\\"";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (c < 0x20) {
        char escaped[7]{};
        std::snprintf(escaped, sizeof(escaped), "\\u%04x", c);
        result += escaped;
      } else {
        result.push_back(static_cast<char>(c));
      }
    }
  }
  return result;
}

bool WriteText(const std::filesystem::path &path, std::string_view text) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream.write(text.data(), static_cast<std::streamsize>(text.size()));
  stream.flush();
  return static_cast<bool>(stream);
}

uint32_t ProcessId();

bool WriteTextAtomic(const std::filesystem::path &path, std::string_view text) {
  auto temporary = path;
  temporary += ".tmp-" + std::to_string(ProcessId());
  if (!WriteText(temporary, text)) {
    return false;
  }
#if defined(_WIN32)
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    std::error_code ec;
    std::filesystem::remove(temporary, ec);
    return false;
  }
  return true;
#else
  std::error_code ec;
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    std::filesystem::remove(temporary, ec);
    return false;
  }
  return true;
#endif
}

std::string UtcStamp() {
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y%m%dT%H%M%SZ");
  return stream.str();
}

uint32_t ProcessId() {
#if defined(_WIN32)
  return GetCurrentProcessId();
#else
  return static_cast<uint32_t>(getpid());
#endif
}

bool IsReparsePoint(const std::filesystem::path &path) {
#if defined(_WIN32)
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes == INVALID_FILE_ATTRIBUTES ||
         (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
  std::error_code ec;
  const bool link =
      std::filesystem::is_symlink(std::filesystem::symlink_status(path, ec));
  return ec || link;
#endif
}

uint64_t DirectoryBytes(const std::filesystem::path &root) {
  uint64_t bytes = 0;
  std::error_code ec;
  for (std::filesystem::recursive_directory_iterator
           it(root, std::filesystem::directory_options::skip_permission_denied,
              ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (IsReparsePoint(it->path())) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file(ec)) {
      bytes += it->file_size(ec);
    }
  }
  return bytes;
}

void PruneDirectories(const std::filesystem::path &root, size_t keep_count,
                      uint64_t keep_bytes) {
  struct Entry {
    std::filesystem::path path;
    std::filesystem::file_time_type time;
    uint64_t bytes;
  };
  std::vector<Entry> entries;
  std::error_code ec;
  for (const auto &item : std::filesystem::directory_iterator(root, ec)) {
    if (ec) {
      break;
    }
    if (!item.is_directory(ec) || IsReparsePoint(item.path()) ||
        item.path().filename().string().rfind("live-", 0) != 0) {
      continue;
    }
    if (item.path().extension() == ".partial") {
      const auto write_time = item.last_write_time(ec);
      if (!ec && write_time < std::filesystem::file_time_type::clock::now() -
                                  kPartialRetentionAge) {
        std::filesystem::remove_all(item.path(), ec);
      }
      ec.clear();
      continue;
    }
    entries.push_back(
        {item.path(), item.last_write_time(ec), DirectoryBytes(item.path())});
  }
  std::sort(entries.begin(), entries.end(),
            [](const Entry &a, const Entry &b) { return a.time > b.time; });
  uint64_t retained = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    retained += entries[i].bytes;
    if (i == 0 || (i < keep_count && retained <= keep_bytes)) {
      continue;
    }
    std::filesystem::remove_all(entries[i].path, ec);
    ec.clear();
  }
}

void PruneRuntimeFiles(const std::filesystem::path &root) {
  struct Entry {
    std::filesystem::path path;
    std::filesystem::file_time_type time;
    uint64_t bytes;
  };
  std::vector<Entry> entries;
  std::error_code ec;
  for (const auto &item : std::filesystem::directory_iterator(root, ec)) {
    if (ec) {
      break;
    }
    if (!item.is_regular_file(ec) || IsReparsePoint(item.path()) ||
        item.path().filename().string().rfind("runtime-", 0) != 0) {
      continue;
    }
    entries.push_back(
        {item.path(), item.last_write_time(ec), item.file_size(ec)});
  }
  std::sort(entries.begin(), entries.end(),
            [](const Entry &a, const Entry &b) { return a.time > b.time; });
  uint64_t retained = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    retained += entries[i].bytes;
    if (i == 0 ||
        (i < kRuntimeRetentionCount && retained <= kRuntimeRetentionBytes)) {
      continue;
    }
    std::filesystem::remove(entries[i].path, ec);
    ec.clear();
  }
}

void WriteLe16(std::ofstream &stream, uint16_t value) {
  const char bytes[] = {static_cast<char>(value),
                        static_cast<char>(value >> 8)};
  stream.write(bytes, sizeof(bytes));
}

void WriteLe32(std::ofstream &stream, uint32_t value) {
  const char bytes[] = {static_cast<char>(value), static_cast<char>(value >> 8),
                        static_cast<char>(value >> 16),
                        static_cast<char>(value >> 24)};
  stream.write(bytes, sizeof(bytes));
}

bool WriteBgraBmp(const std::filesystem::path &path, uint32_t width,
                  uint32_t height, uint32_t stride,
                  std::span<const uint8_t> pixels) {
  if (!width || !height || width > 8192 || height > 8192) {
    return false;
  }
  const uint64_t row_bytes = uint64_t(width) * 4;
  const uint64_t payload_bytes = row_bytes * height;
  if (stride < row_bytes || payload_bytes > kMaxFrameBytes ||
      stride > std::numeric_limits<size_t>::max() / height) {
    return false;
  }
  const uint64_t required_bytes =
      height == 0 ? 0 : stride * (height - 1) + row_bytes;
  if (required_bytes > pixels.size() || payload_bytes > UINT32_MAX - 54) {
    return false;
  }

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream.put('B');
  stream.put('M');
  WriteLe32(stream, static_cast<uint32_t>(54 + payload_bytes));
  WriteLe16(stream, 0);
  WriteLe16(stream, 0);
  WriteLe32(stream, 54);
  WriteLe32(stream, 40);
  WriteLe32(stream, width);
  WriteLe32(stream, static_cast<uint32_t>(-static_cast<int32_t>(height)));
  WriteLe16(stream, 1);
  WriteLe16(stream, 32);
  WriteLe32(stream, 0);
  WriteLe32(stream, static_cast<uint32_t>(payload_bytes));
  WriteLe32(stream, 2835);
  WriteLe32(stream, 2835);
  WriteLe32(stream, 0);
  WriteLe32(stream, 0);
  for (uint32_t y = 0; y < height; ++y) {
    stream.write(reinterpret_cast<const char *>(pixels.data() + stride * y),
                 static_cast<std::streamsize>(row_bytes));
  }
  stream.flush();
  return static_cast<bool>(stream);
}

struct WindowFrameCapture {
  bool written = false;
  FrameSignal signal;
  const char *source = "win32-client-bitblt";
  const char *freshness = "not-established";
#if defined(MCLA_EXPERIMENTAL_F10_WGC)
  image::CaptureResult helper;
#endif
};

WindowFrameCapture CaptureWindowFrame(const std::filesystem::path &path,
                                     const UiState &ui, std::stop_token token) {
#if defined(MCLA_EXPERIMENTAL_F10_WGC)
  WindowFrameCapture frame;
  frame.source = "win32-client-wgc-image-only";
  std::array<wchar_t, 32768> module{};
  const DWORD length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
  if (!length || length >= module.size()) return frame;
  const auto helper = std::filesystem::path(std::wstring_view(module.data(), length)).parent_path() /
                      L"mcla_diagnostic_image_helper.exe";
  frame.helper = image::CaptureOwnedWindow(ui.native_window_handle, helper,
                                          MCLA_IMAGE_HELPER_SHA256, token);
  if (!frame.helper.captured || token.stop_requested()) return frame; // No GDI fallback.
  const auto width = static_cast<uint32_t>(frame.helper.metadata.request.client_width);
  const auto height = static_cast<uint32_t>(frame.helper.metadata.request.client_height);
  const std::span<const uint8_t> pixels(
      reinterpret_cast<const uint8_t *>(frame.helper.pixels.data()), frame.helper.pixels.size());
  const auto signal = MeasureBgraFrame(pixels, width, height, width * 4);
  if (signal.classification != FrameRgbClass::kUnavailable &&
      WriteBgraBmp(path, width, height, width * 4, pixels)) {
    frame.written = true;
    frame.signal = signal;
  } else {
    std::error_code ignored;
    std::filesystem::remove(path, ignored); // Only this request's partial BMP.
  }
  std::vector<std::byte>().swap(frame.helper.pixels); // Retain status, release accepted bytes.
  return frame;
#elif defined(_WIN32)
  (void)token;
  const HWND window = reinterpret_cast<HWND>(ui.native_window_handle);
  if (!window || !IsWindow(window)) {
    return {};
  }
  RECT client{};
  if (!GetClientRect(window, &client)) {
    return {};
  }
  const LONG signed_width = client.right - client.left;
  const LONG signed_height = client.bottom - client.top;
  if (signed_width <= 0 || signed_height <= 0 || signed_width > 8192 ||
      signed_height > 8192) {
    return {};
  }
  const uint32_t width = static_cast<uint32_t>(signed_width);
  const uint32_t height = static_cast<uint32_t>(signed_height);
  const uint64_t bytes = uint64_t(width) * height * 4;
  if (bytes > kMaxFrameBytes || bytes > std::numeric_limits<size_t>::max()) {
    return {};
  }

  HDC source = GetDC(window);
  if (!source) {
    return {};
  }
  HDC memory = CreateCompatibleDC(source);
  HBITMAP bitmap =
      memory ? CreateCompatibleBitmap(source, signed_width, signed_height)
             : nullptr;
  HGDIOBJ previous = bitmap ? SelectObject(memory, bitmap) : nullptr;
  bool captured = false;
  std::vector<uint8_t> pixels;
  if (previous && BitBlt(memory, 0, 0, signed_width, signed_height, source, 0,
                         0, SRCCOPY | CAPTUREBLT)) {
    SelectObject(memory, previous);
    previous = nullptr;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = signed_width;
    info.bmiHeader.biHeight = -signed_height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    pixels.resize(static_cast<size_t>(bytes));
    captured = GetDIBits(memory, bitmap, 0, height, pixels.data(), &info,
                         DIB_RGB_COLORS) == static_cast<int>(height);
  }
  if (previous) {
    SelectObject(memory, previous);
  }
  if (bitmap) {
    DeleteObject(bitmap);
  }
  if (memory) {
    DeleteDC(memory);
  }
  ReleaseDC(window, source);
  if (!captured) {
    return {};
  }
  const FrameSignal signal = MeasureBgraFrame(pixels, width, height, width * 4);
  if (signal.classification == FrameRgbClass::kUnavailable ||
      !WriteBgraBmp(path, width, height, width * 4, pixels)) {
    return {};
  }
  // A written GDI bitmap is not proof of a fresh or visible renderer output.
  // Preserve even all-black bytes as evidence without failing the package.
  return {true, signal};
#else
  (void)path;
  (void)ui;
  (void)token;
  return {};
#endif
}

// KI-042: the game's own presented output, read back on a helper thread. The
// readback waits on a GPU fence without a bound, so the package worker waits at
// most kGuestFrameWait and otherwise falls back; a stuck helper is left behind
// (the host hard-exits at shutdown). Returns a written frame or nothing.
constexpr auto kGuestFrameWait = std::chrono::milliseconds(1500);
WindowFrameCapture CaptureGuestFrame(const std::filesystem::path &path,
                                     const GuestFrameProvider &provider,
                                     std::stop_token token) {
  if (!provider) return {};
  struct Shared {
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    bool ok = false;
    GuestFrame frame;
  };
  auto shared = std::make_shared<Shared>();
  try {
    std::thread([shared, provider]() {
      GuestFrame frame;
      bool ok = false;
      try { ok = provider(frame); } catch (...) { ok = false; }
      std::lock_guard lock(shared->mutex);
      shared->ok = ok;
      shared->frame = std::move(frame);
      shared->done = true;
      shared->cv.notify_all();
    }).detach();
  } catch (...) {
    return {};
  }
  std::unique_lock lock(shared->mutex);
  const auto deadline = std::chrono::steady_clock::now() + kGuestFrameWait;
  while (!shared->done && !token.stop_requested() &&
         shared->cv.wait_until(lock, deadline) != std::cv_status::timeout) {
  }
  if (!shared->done || !shared->ok) {
    REXLOG_WARN("MCLA diagnostics: guest frame readback {}; using window capture",
                shared->done ? "failed" : "timed out");
    return {};
  }
  const auto &image = shared->frame;
  const uint64_t row = uint64_t(image.width) * 4;
  if (!image.width || !image.height || image.width > 8192 || image.height > 8192 ||
      image.stride < row || image.rgbx.size() < image.stride * (image.height - 1) + row ||
      row * image.height > kMaxFrameBytes) {
    return {};
  }
  std::vector<uint8_t> bgra(static_cast<size_t>(row * image.height));
  for (uint32_t y = 0; y < image.height; ++y) {
    const uint8_t *src = image.rgbx.data() + size_t(y) * image.stride;
    uint8_t *dst = bgra.data() + size_t(y) * row;
    for (uint32_t x = 0; x < image.width; ++x) {
      dst[x * 4 + 0] = src[x * 4 + 2];
      dst[x * 4 + 1] = src[x * 4 + 1];
      dst[x * 4 + 2] = src[x * 4 + 0];
      dst[x * 4 + 3] = 0xFF;
    }
  }
  WindowFrameCapture frame;
  frame.signal = MeasureBgraFrame(bgra, image.width, image.height, static_cast<uint32_t>(row));
  if (frame.signal.classification == FrameRgbClass::kUnavailable ||
      !WriteBgraBmp(path, image.width, image.height, static_cast<uint32_t>(row), bgra)) {
    return {};
  }
  frame.written = true;
  frame.source = "guest-output-readback";
  frame.freshness = "presented-guest-output";
  return frame;
}

uint64_t FileDigest64(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  uint64_t hash = 1469598103934665603ULL; // FNV-1a
  std::array<char, 65536> buffer{};
  while (stream.read(buffer.data(), buffer.size()) || stream.gcount() > 0) {
    for (std::streamsize i = 0; i < stream.gcount(); ++i) {
      hash ^= static_cast<uint8_t>(buffer[static_cast<size_t>(i)]);
      hash *= 1099511628211ULL;
    }
  }
  return hash;
}

void ReplaceAll(std::string &text, std::string_view needle,
                std::string_view replacement) {
  if (needle.empty()) {
    return;
  }
  size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    text.replace(position, needle.size(), replacement);
    position += replacement.size();
  }
}

std::string Sanitize(std::string text,
                     const std::filesystem::path &user_data_root) {
  const std::string user_root = PathUtf8(user_data_root);
  ReplaceAll(text, user_root, "<user-data>");
  std::string windows_root = user_root;
  std::replace(windows_root.begin(), windows_root.end(), '/', '\\');
  ReplaceAll(text, windows_root, "<user-data>");
#if defined(_WIN32)
  wchar_t profile[1024]{};
  size_t count = 0;
  if (_wgetenv_s(&count, profile, L"USERPROFILE") == 0 && count > 1) {
    ReplaceAll(text, PathUtf8(profile), "<user-profile>");
  }
#endif
  return text;
}

struct SaveCopyStats {
  struct File {
    std::string relative_path;
    uint64_t bytes = 0;
    std::string sha256;
  };

  uint64_t bytes = 0;
  uint32_t files = 0;
  uint32_t skipped = 0;
  bool found = false;
  std::vector<File> inventory;
};

std::string Sha256File(const std::filesystem::path &path);

bool CopyStableFile(const std::filesystem::path &source,
                    const std::filesystem::path &destination,
                    const std::filesystem::path &relative,
                    SaveCopyStats &stats) {
  std::error_code ec;
  for (int attempt = 0; attempt < 3; ++attempt) {
    const auto size_before = std::filesystem::file_size(source, ec);
    if (ec || size_before > kMaxSaveFileBytes ||
        stats.bytes + size_before > kMaxSaveBytes) {
      ++stats.skipped;
      return false;
    }
    const auto time_before = std::filesystem::last_write_time(source, ec);
    if (ec) {
      ++stats.skipped;
      return false;
    }
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
      ++stats.skipped;
      return false;
    }
    std::filesystem::copy_file(
        source, destination, std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec) {
      ec.clear();
      continue;
    }
    const auto size_after = std::filesystem::file_size(source, ec);
    const auto time_after = std::filesystem::last_write_time(source, ec);
    if (!ec && size_before == size_after && time_before == time_after) {
      const std::string hash = Sha256File(destination);
      if (hash.empty()) {
        std::filesystem::remove(destination, ec);
        ++stats.skipped;
        return false;
      }
      stats.bytes += size_after;
      ++stats.files;
      stats.inventory.push_back({GenericPathUtf8(relative), size_after, hash});
      return true;
    }
  }
  ++stats.skipped;
  std::filesystem::remove(destination, ec);
  return false;
}

SaveCopyStats CopySaveTrees(const std::filesystem::path &user_data_root,
                            const std::filesystem::path &destination_root) {
  SaveCopyStats stats;
  uint32_t visited_entries = 0;
  std::error_code ec;
  for (const auto &profile :
       std::filesystem::directory_iterator(user_data_root, ec)) {
    if (++visited_entries > kMaxSaveEntries) {
      ++stats.skipped;
      break;
    }
    if (ec) {
      break;
    }
    if (!profile.is_directory(ec) || IsReparsePoint(profile.path())) {
      continue;
    }
    const auto title_root = profile.path() / "545407F8";
    if (!std::filesystem::is_directory(title_root, ec) ||
        IsReparsePoint(title_root)) {
      ec.clear();
      continue;
    }
    stats.found = true;
    for (std::filesystem::recursive_directory_iterator
             it(title_root,
                std::filesystem::directory_options::skip_permission_denied, ec),
         end;
         it != end; it.increment(ec)) {
      if (++visited_entries > kMaxSaveEntries) {
        ++stats.skipped;
        break;
      }
      if (ec) {
        ec.clear();
        ++stats.skipped;
        continue;
      }
      if (IsReparsePoint(it->path())) {
        it.disable_recursion_pending();
        ++stats.skipped;
        continue;
      }
      const auto relative =
          std::filesystem::relative(it->path(), user_data_root, ec);
      if (ec || relative.empty()) {
        ec.clear();
        ++stats.skipped;
        continue;
      }
      const auto destination = destination_root / relative;
      if (it->is_directory(ec)) {
        std::filesystem::create_directories(destination, ec);
      } else if (it->is_regular_file(ec)) {
        CopyStableFile(it->path(), destination, relative, stats);
      }
    }
  }
  return stats;
}

std::string BuildSaveInventory(const SaveCopyStats &save) {
  std::ostringstream out;
  out << "{\n"
      << "  \"schema\": \"mcla-private-save-inventory-v1\",\n"
      << "  \"safe_to_share\": false,\n"
      << "  \"files\": [\n";
  for (size_t i = 0; i < save.inventory.size(); ++i) {
    const auto &file = save.inventory[i];
    out << "    {\"path\": \"" << JsonEscape(file.relative_path)
        << "\", \"bytes\": " << file.bytes << ", \"sha256\": \"" << file.sha256
        << "\"}" << (i + 1 == save.inventory.size() ? "\n" : ",\n");
  }
  out << "  ]\n}\n";
  return out.str();
}

struct Artifact {
  std::string name;
  uint64_t bytes = 0;
  std::string sha256;
  bool safe_to_share = true;
};

std::string Sha256File(const std::filesystem::path &path) {
#if defined(_WIN32)
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  DWORD object_size = 0;
  DWORD result_size = 0;
  std::vector<UCHAR> object;
  std::array<UCHAR, 32> digest{};
  auto cleanup = [&]() {
    if (hash) {
      BCryptDestroyHash(hash);
    }
    if (algorithm) {
      BCryptCloseAlgorithmProvider(algorithm, 0);
    }
  };
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0 ||
      BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&object_size),
                        sizeof(object_size), &result_size, 0) < 0) {
    cleanup();
    return {};
  }
  object.resize(object_size);
  if (BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0,
                       0) < 0) {
    cleanup();
    return {};
  }
  std::ifstream stream(path, std::ios::binary);
  std::array<char, 64 * 1024> buffer{};
  while (stream) {
    stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto count = stream.gcount();
    if (count > 0 &&
        BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                       static_cast<ULONG>(count), 0) < 0) {
      cleanup();
      return {};
    }
  }
  if (!stream.eof() ||
      BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()),
                       0) < 0) {
    cleanup();
    return {};
  }
  std::ostringstream out;
  out << std::hex << std::uppercase << std::setfill('0');
  for (const UCHAR byte : digest) {
    out << std::setw(2) << static_cast<unsigned>(byte);
  }
  cleanup();
  return out.str();
#else
  return rex::crypto::sha256_file(path);
#endif
}

bool AddArtifact(std::vector<Artifact> &artifacts,
                 const std::filesystem::path &root, std::string name,
                 bool safe_to_share) {
  const auto path = root / name;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) {
    return false;
  }
  Artifact artifact;
  artifact.name = std::move(name);
  artifact.bytes = std::filesystem::file_size(path, ec);
  artifact.sha256 = Sha256File(path);
  if (artifact.sha256.empty()) {
    return false;
  }
  artifact.safe_to_share = safe_to_share;
  artifacts.push_back(std::move(artifact));
  return true;
}

std::string
BuildStateJson(std::string_view reason, const UiState &ui,
               const RuntimeState &runtime, uint64_t working_set,
               uint32_t handles, uint64_t creation_time_100ns,
               const std::vector<NativeThreadSample> &native_threads) {
  std::ostringstream out;
  out << "{\n"
      << "  \"schema\": \"mcla-diagnostic-state-v1\",\n"
      << "  \"reason\": \"" << JsonEscape(reason) << "\",\n"
      << "  \"process\": {\"pid\": " << ProcessId()
      << ", \"creation_time_100ns\": " << creation_time_100ns
      << ", \"working_set_bytes\": " << working_set
      << ", \"handle_count\": " << handles
      << ", \"native_thread_count\": " << native_threads.size() << "},\n"
      << "  \"window\": {\"logical_width\": " << ui.logical_width
      << ", \"logical_height\": " << ui.logical_height
      << ", \"physical_width\": " << ui.physical_width
      << ", \"physical_height\": " << ui.physical_height
      << ", \"focused\": " << (ui.focused ? "true" : "false")
      << ", \"fullscreen\": " << (ui.fullscreen ? "true" : "false") << "},\n"
      << "  \"runtime\": {\"available\": "
      << (runtime.runtime_available ? "true" : "false")
      << ", \"title_id\": " << runtime.title_id
      << ", \"guest_output_sequence\": " << runtime.guest_output_sequence
      << ", \"guest_vblank_sequence\": " << runtime.guest_vblank_sequence
      << "},\n"
      << "  \"graphics\": {\"backend\": " << runtime.graphics_backend
      << ", \"guest_frames_opened\": " << runtime.graphics_guest_frames_opened
      << ", \"guest_frames_closed\": " << runtime.graphics_guest_frames_closed
      << ", \"issue_swap_calls\": " << runtime.graphics_issue_swap_calls
      << ", \"guest_output_refresh_successes\": "
      << runtime.graphics_guest_output_refresh_successes
      << ", \"guest_output_refresh_failures\": "
      << runtime.graphics_guest_output_refresh_failures
      << ", \"async_placeholder_skips\": "
      << runtime.graphics_async_placeholder_skips
      << ", \"swap_source_missing\": " << runtime.graphics_swap_source_missing
      << ", \"submission_count\": " << runtime.graphics_submission_count
      << ", \"frame_fence_wait_us\": " << runtime.graphics_frame_fence_wait_us
      << ", \"pipeline_end_submission_us\": "
      << runtime.graphics_pipeline_end_submission_us
      << ", \"deferred_execute_us\": " << runtime.graphics_deferred_execute_us
      << ", \"queue_submit_us\": " << runtime.graphics_queue_submit_us
      << ", \"guest_frame_wall_us\": " << runtime.graphics_guest_frame_wall_us
      << ", \"command_ring_idle_us\": " << runtime.graphics_command_ring_idle_us
      << ", \"command_wait_reg_mem_us\": " << runtime.graphics_command_wait_reg_mem_us
      << ", \"memory_non_mmio_write_fault_count\": "
      << runtime.graphics_memory_non_mmio_write_fault_count
      << ", \"memory_non_mmio_write_fault_us\": "
      << runtime.graphics_memory_non_mmio_write_fault_us
      << ", \"command_primary_buffer_count\": " << runtime.graphics_command_primary_buffer_count
      << ", \"command_primary_buffer_us\": " << runtime.graphics_command_primary_buffer_us
      << ", \"command_draw_packet_count\": " << runtime.graphics_command_draw_packet_count
      << ", \"command_draw_packet_us\": " << runtime.graphics_command_draw_packet_us
      << ", \"command_swap_packet_count\": " << runtime.graphics_command_swap_packet_count
      << ", \"command_swap_packet_us\": " << runtime.graphics_command_swap_packet_us
      << ", \"command_interrupt_packet_count\": " << runtime.graphics_command_interrupt_packet_count
      << ", \"command_interrupt_packet_us\": " << runtime.graphics_command_interrupt_packet_us
      << ", \"vulkan_fence_call_count\": " << runtime.graphics_vulkan_fence_call_count
      << ", \"vulkan_fence_call_us\": " << runtime.graphics_vulkan_fence_call_us
      << ", \"memory_write_fault_lock_wait_us\": " << runtime.graphics_memory_write_fault_lock_wait_us
      << ", \"memory_write_fault_query_us\": " << runtime.graphics_memory_write_fault_query_us
      << ", \"memory_write_fault_callback_us\": " << runtime.graphics_memory_write_fault_callback_us
      << ", \"memory_write_fault_already_cleared_count\": " << runtime.graphics_memory_write_fault_already_cleared_count
      << ", \"memory_watch_arm_calls\": " << runtime.graphics_memory_watch_arm_calls
      << ", \"memory_watch_arm_us\": " << runtime.graphics_memory_watch_arm_us
      << ", \"memory_watch_arm_lock_wait_us\": " << runtime.graphics_memory_watch_arm_lock_wait_us
      << ", \"memory_watch_disarm_calls\": " << runtime.graphics_memory_watch_disarm_calls
      << ", \"memory_watch_disarm_us\": " << runtime.graphics_memory_watch_disarm_us
      << ", \"memory_watch_protect_failures\": " << runtime.graphics_memory_watch_protect_failures
      << ", \"memory_watch_arm_gap_splits\": " << runtime.graphics_memory_watch_arm_gap_splits
      << ", \"memory_watch_disarm_safe_gap_splits\": " << runtime.graphics_memory_watch_disarm_safe_gap_splits
      << ", \"memory_watch_disarm_gap_lock_misses\": " << runtime.graphics_memory_watch_disarm_gap_lock_misses
      << ", \"vulkan_resolve_count\": " << runtime.graphics_vulkan_resolve_count
      << ", \"vulkan_resolve_us\": " << runtime.graphics_vulkan_resolve_us
      << ", \"vulkan_resolve_readback_copy_count\": " << runtime.graphics_vulkan_resolve_readback_copy_count
      << ", \"vulkan_resolve_readback_bytes\": " << runtime.graphics_vulkan_resolve_readback_bytes
      << ", \"vulkan_resolve_readback_copy_us\": " << runtime.graphics_vulkan_resolve_readback_copy_us
      << ", \"vulkan_resolve_readback_wait_count\": " << runtime.graphics_vulkan_resolve_readback_wait_count
      << ", \"vulkan_resolve_readback_wait_us\": " << runtime.graphics_vulkan_resolve_readback_wait_us
      << ", \"vulkan_binding_update_count\": " << runtime.graphics_vulkan_binding_update_count
      << ", \"vulkan_binding_update_us\": " << runtime.graphics_vulkan_binding_update_us
      << ", \"vulkan_texture_descriptor_reuse_count\": " << runtime.graphics_vulkan_texture_descriptor_reuse_count
      << ", \"vulkan_texture_descriptor_write_count\": " << runtime.graphics_vulkan_texture_descriptor_write_count
      << ", \"vulkan_descriptor_update_count\": " << runtime.graphics_vulkan_descriptor_update_count
      << ", \"vulkan_descriptor_update_us\": " << runtime.graphics_vulkan_descriptor_update_us
      << "},\n"
      << "  \"race_back_trace\": {\"sequence\": " << runtime.race_back_sequence
      << ", \"handler_calls\": " << runtime.race_back_handler_calls
      << ", \"apply_calls\": " << runtime.race_back_apply_calls << "},\n";
  if (!runtime.guest_ki026_json.empty()) {
    out << "  \"guest_ki026\": " << runtime.guest_ki026_json << ",\n";
  }
  out << "  \"native_threads\": [";
  for (size_t i = 0; i < native_threads.size(); ++i) {
    const auto &thread = native_threads[i];
    if (i) {
      out << ',';
    }
    out << "{\"id\": " << thread.id
        << ", \"base_priority\": " << thread.base_priority
        << ", \"kernel_time_100ns\": " << thread.kernel_time_100ns
        << ", \"user_time_100ns\": " << thread.user_time_100ns
        << ", \"name\": \"" << JsonEscape(thread.name) << "\"}";
  }
  out << "]\n"
      << "}\n";
  return out.str();
}

} // namespace

class Manager::Impl {
public:
  bool Start(const std::filesystem::path &user_data_root,
             const std::filesystem::path &diagnostics_root,
             CaptureProvider provider, bool show_crash_reporter_dialog) {
    user_data_root_ =
        std::filesystem::absolute(user_data_root).lexically_normal();
    diagnostics_root_ =
        diagnostics_root.empty()
            ? user_data_root_ / "diagnostics"
            : std::filesystem::absolute(diagnostics_root).lexically_normal();
    live_root_ = diagnostics_root_ / "live";
    runtime_root_ = diagnostics_root_ / "runtime";
    std::error_code ec;
    std::filesystem::create_directories(live_root_, ec);
    std::filesystem::create_directories(diagnostics_root_ / "crash", ec);
    std::filesystem::create_directories(runtime_root_, ec);
    if (ec) {
      REXLOG_ERROR("MCLA diagnostics: failed to create diagnostics root: {}",
                   ec.message());
      return false;
    }
    PruneDirectories(live_root_, kLiveRetentionCount, kLiveRetentionBytes);
    PruneRuntimeFiles(runtime_root_);
    provider_ = std::move(provider);
    capture_sink_ = std::make_shared<rex::LogCaptureSink>();
    rex::AddSink(capture_sink_);
    journal_path_ = runtime_root_ / ("runtime-" + UtcStamp() + "-" +
                                     std::to_string(ProcessId()) + ".log");
    try {
      journal_sink_ = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
          journal_path_.native(), kJournalFileBytes, kJournalRotatedFiles,
          true);
      journal_sink_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
      rex::AddSink(journal_sink_);
    } catch (const std::exception &e) {
      REXLOG_WARN("MCLA diagnostics: journal unavailable: {}", e.what());
    }

    std::string native_error;
    native_ready_ =
        native::Start(diagnostics_root_, user_data_root_, journal_path_,
                      MCLA_VERSION, show_crash_reporter_dialog, native_error);
    if (!native_ready_) {
      REXLOG_WARN("MCLA diagnostics: native crash helper unavailable: {}",
                  native_error);
    }
    worker_ =
        std::jthread([this](std::stop_token token) { WorkerMain(token); });
    snapshot_status_.store(SnapshotStatus::kIdle, std::memory_order_release);
    started_.store(true, std::memory_order_release);
    REXLOG_INFO("MCLA diagnostics: ready; F10 snapshot root is {}",
                GenericPathUtf8(live_root_));
    return true;
  }

  void SetGuestFrameProvider(GuestFrameProvider provider) {
    std::lock_guard lock(request_mutex_);
    guest_frame_provider_ = std::move(provider);
  }
  GuestFrameProvider CurrentGuestFrameProvider() {
    std::lock_guard lock(request_mutex_);
    return guest_frame_provider_;
  }

  bool RequestSnapshot(std::string reason, const UiState &ui_state) {
    if (!started_.load(std::memory_order_acquire) ||
        stopping_.load(std::memory_order_acquire)) {
      return false;
    }
    bool expected = false;
    if (!busy_.compare_exchange_strong(expected, true,
                                       std::memory_order_acq_rel)) {
      REXLOG_WARN(
          "MCLA diagnostics: snapshot request ignored; capture already active");
      return false;
    }
    {
      std::lock_guard lock(request_mutex_);
      requested_reason_ = std::move(reason);
      requested_ui_state_ = ui_state;
      request_pending_ = true;
    }
    snapshot_status_.store(SnapshotStatus::kCapturing,
                           std::memory_order_release);
    request_cv_.notify_one();
    REXLOG_INFO("MCLA diagnostics: snapshot queued");
    return true;
  }

  bool WaitForIdle(std::chrono::milliseconds timeout) {
    std::unique_lock lock(idle_mutex_);
    return idle_cv_.wait_for(lock, timeout, [this]() {
      return !busy_.load(std::memory_order_acquire);
    });
  }

  SnapshotStatus snapshot_status() const {
    return snapshot_status_.load(std::memory_order_acquire);
  }

  void Stop() {
    if (!started_.exchange(false, std::memory_order_acq_rel)) {
      return;
    }
    stopping_.store(true, std::memory_order_release);
    if (worker_.joinable()) {
      worker_.request_stop();
      request_cv_.notify_all();
      worker_.join();
    }
    native::Stop();
    if (journal_sink_) {
      journal_sink_->flush();
      rex::RemoveSink(journal_sink_);
      journal_sink_.reset();
      PruneRuntimeFiles(runtime_root_);
    }
    if (capture_sink_) {
      rex::RemoveSink(capture_sink_);
      capture_sink_.reset();
    }
  }

  void WorkerMain(std::stop_token token) {
    while (!token.stop_requested()) {
      std::string reason;
      UiState ui;
      {
        std::unique_lock lock(request_mutex_);
        request_cv_.wait(lock, token, [this]() { return request_pending_; });
        if (token.stop_requested()) {
          return;
        }
        reason = std::move(requested_reason_);
        ui = requested_ui_state_;
        request_pending_ = false;
      }
      bool captured = false;
      try {
        captured = Capture(reason, ui, token);
      } catch (const std::exception &e) {
        REXLOG_ERROR("MCLA diagnostics: snapshot worker failed: {}", e.what());
      } catch (...) {
        REXLOG_ERROR(
            "MCLA diagnostics: snapshot worker failed with unknown exception");
      }
      snapshot_status_.store(captured ? SnapshotStatus::kComplete
                                      : SnapshotStatus::kFailed,
                             std::memory_order_release);
      busy_.store(false, std::memory_order_release);
      idle_cv_.notify_all();
    }
  }

  bool Capture(std::string_view reason, const UiState &ui, std::stop_token token) {
    if (token.stop_requested()) return false;
    const uint64_t sequence = sequence_.fetch_add(1) + 1;
    const std::string name = "live-" + UtcStamp() + "-" +
                             std::to_string(ProcessId()) + "-" +
                             std::to_string(sequence);
    const auto partial = live_root_ / (name + ".partial");
    const auto complete = live_root_ / name;
    std::error_code ec;
    std::filesystem::remove_all(partial, ec);
    ec.clear();
    std::filesystem::create_directories(partial, ec);
    if (ec) {
      REXLOG_ERROR(
          "MCLA diagnostics: cannot create snapshot staging directory: {}",
          ec.message());
      return false;
    }

    RuntimeState runtime_state;
    try {
      if (provider_) {
        runtime_state = provider_();
      }
    } catch (const std::exception &e) {
      REXLOG_ERROR("MCLA diagnostics: state provider failed: {}", e.what());
    } catch (...) {
      REXLOG_ERROR(
          "MCLA diagnostics: state provider failed with unknown exception");
    }

    // No in-process GPU readback/fence wait. The experimental image helper is
    // isolated/cancellable; this does not bound all package or OS operations.
    WindowFrameCapture frame =
        CaptureGuestFrame(partial / "frame.bmp", CurrentGuestFrameProvider(), token);
    if (!frame.written && !token.stop_requested()) {
      frame = CaptureWindowFrame(partial / "frame.bmp", ui, token);
      // A window capture identical to the previous one in this process is the
      // stale flip-model image (KI-042): drop it rather than ship a wrong frame.
      if (frame.written) {
        const uint64_t digest = FileDigest64(partial / "frame.bmp");
        if (digest == last_window_frame_digest_) {
          std::error_code ignored;
          std::filesystem::remove(partial / "frame.bmp", ignored);
          frame.written = false;
          frame.freshness = "stale-duplicate-dropped";
        }
        last_window_frame_digest_ = digest;
      }
    }
    if (token.stop_requested()) return false;
    const bool frame_written = frame.written;

    uint64_t working_set = 0;
    uint64_t creation_time_100ns = 0;
    uint32_t handles = 0;
    std::vector<NativeThreadSample> native_threads;
#if defined(_WIN32)
    FILETIME process_creation{}, process_exit{}, process_kernel{}, process_user{};
    if (GetProcessTimes(GetCurrentProcess(), &process_creation, &process_exit,
                        &process_kernel, &process_user)) {
      creation_time_100ns = FileTimeTicks(process_creation);
    }
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&counters),
            sizeof(counters))) {
      working_set = counters.WorkingSetSize;
    }
    DWORD handle_count = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handle_count)) {
      handles = handle_count;
    }
    HANDLE thread_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (thread_snapshot != INVALID_HANDLE_VALUE) {
      THREADENTRY32 entry{};
      entry.dwSize = sizeof(entry);
      if (Thread32First(thread_snapshot, &entry)) {
        do {
          if (entry.th32OwnerProcessID == GetCurrentProcessId()) {
            NativeThreadSample sample;
            sample.id = entry.th32ThreadID;
            sample.base_priority = entry.tpBasePri;
            HANDLE thread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE,
                                       entry.th32ThreadID);
            if (thread) {
              FILETIME creation{}, exit{}, kernel{}, user{};
              if (GetThreadTimes(thread, &creation, &exit, &kernel, &user)) {
                sample.kernel_time_100ns = FileTimeTicks(kernel);
                sample.user_time_100ns = FileTimeTicks(user);
              }
              sample.name = ThreadDescription(thread);
              CloseHandle(thread);
            }
            native_threads.push_back(std::move(sample));
          }
        } while (Thread32Next(thread_snapshot, &entry));
      }
      CloseHandle(thread_snapshot);
    }
    std::sort(native_threads.begin(), native_threads.end(),
              [](const NativeThreadSample &a, const NativeThreadSample &b) {
                return a.id < b.id;
              });
#endif
    if (!WriteText(partial / "state.json",
                   BuildStateJson(reason, ui, runtime_state, working_set,
                                  handles, creation_time_100ns, native_threads))) {
      REXLOG_ERROR("MCLA diagnostics: state write failed");
      return false;
    }

    std::vector<rex::LogEntry> entries;
    if (capture_sink_) {
      capture_sink_->CopyEntries(entries);
    }
    const size_t begin =
        entries.size() > kMaxLogEntries ? entries.size() - kMaxLogEntries : 0;
    std::string log_tail;
    for (size_t i = begin; i < entries.size(); ++i) {
      std::string line = "[" + entries[i].category + "] " + entries[i].text;
      line = Sanitize(std::move(line), user_data_root_);
      if (log_tail.size() + line.size() + 1 > kMaxLogBytes) {
        break;
      }
      log_tail += line;
      log_tail.push_back('\n');
    }
    if (!WriteText(partial / "log-tail.txt", log_tail)) {
      REXLOG_ERROR("MCLA diagnostics: log-tail write failed");
      return false;
    }
    if (journal_sink_) {
      journal_sink_->flush();
    }

    if (token.stop_requested()) return false;
    const SaveCopyStats save =
        CopySaveTrees(user_data_root_, partial / "save-private");
    std::ostringstream save_metadata;
    save_metadata << "{\n"
                  << "  \"schema\": \"mcla-save-snapshot-metadata-v1\",\n"
                  << "  \"found\": " << (save.found ? "true" : "false") << ",\n"
                  << "  \"files\": " << save.files << ",\n"
                  << "  \"bytes\": " << save.bytes << ",\n"
                  << "  \"skipped_or_unstable_files\": " << save.skipped
                  << ",\n"
                  << "  \"safe_to_share\": false\n"
                  << "}\n";
    if (!WriteText(partial / "save-metadata.json", save_metadata.str())) {
      REXLOG_ERROR("MCLA diagnostics: save metadata write failed");
      return false;
    }
    if (!WriteText(partial / "save-files-private.json",
                   BuildSaveInventory(save))) {
      REXLOG_ERROR("MCLA diagnostics: private save inventory write failed");
      return false;
    }

    if (token.stop_requested()) return false;
    std::string dump_error;
    bool dump_written =
        native_ready_ &&
        native::WriteLiveMiniDump(partial / "process-private.dmp",
                                  std::chrono::seconds(20), dump_error);
    if (dump_written) {
      const auto dump_bytes =
          std::filesystem::file_size(partial / "process-private.dmp", ec);
      if (ec || dump_bytes > kMaxLiveDumpBytes) {
        dump_written = false;
        dump_error = "minidump exceeded the 128 MiB privacy bound";
        ec.clear();
        std::filesystem::remove(partial / "process-private.dmp", ec);
        ec.clear();
      }
    }

    std::vector<Artifact> artifacts;
    if (!AddArtifact(artifacts, partial, "state.json", true) ||
        !AddArtifact(artifacts, partial, "log-tail.txt", false) ||
        !AddArtifact(artifacts, partial, "save-metadata.json", true) ||
        !AddArtifact(artifacts, partial, "save-files-private.json", false)) {
      REXLOG_ERROR("MCLA diagnostics: required artifact hashing failed");
      return false;
    }
    if (frame_written) {
      if (!AddArtifact(artifacts, partial, "frame.bmp", false)) {
        REXLOG_ERROR("MCLA diagnostics: frame hashing failed");
        return false;
      }
    }
    if (dump_written) {
      if (!AddArtifact(artifacts, partial, "process-private.dmp", false)) {
        REXLOG_ERROR("MCLA diagnostics: minidump hashing failed");
        return false;
      }
    }

    std::ostringstream manifest;
    manifest << "{\n"
             << "  \"schema\": \"mcla-diagnostic-package-v1\",\n"
             << "  \"kind\": \"live\",\n"
             << "  \"reason\": \"" << JsonEscape(reason) << "\",\n"
             << "  \"created_utc\": \"" << UtcStamp() << "\",\n"
             << "  \"mcla_version\": \"" << MCLA_VERSION << "\",\n"
             << "  \"platform\": \""
#if defined(_WIN32)
             << "windows"
#else
             << "portable"
#endif
             << "\",\n"
             << "  \"privacy\": {\"automatic_upload\": false, "
                "\"package_safe_to_share\": false, \"private_artifacts\": "
                "[\"process-private.dmp\", \"log-tail.txt\", \"frame.bmp\", "
                "\"save-files-private.json\", \"save-private\"]},\n"
             << "  \"capture\": {\"frame\": "
             << (frame_written ? "true" : "false")
             << ", \"frame_signal\": {\"source\": \"" << frame.source << "\", "
                "\"freshness\": \"" << frame.freshness << "\", "
                "\"classification\": \""
             << FrameRgbClassName(frame.signal.classification)
             << "\", \"width\": " << frame.signal.width
             << ", \"height\": " << frame.signal.height << "}"
#if defined(MCLA_EXPERIMENTAL_F10_WGC)
             << ", \"image_helper\": {\"protocol_version\": 1, \"sha256\": \""
             << MCLA_IMAGE_HELPER_SHA256 << "\", \"admitted\": "
             << (frame.helper.captured ? "true" : "false")
             << ", \"cleanup_complete\": " << (frame.helper.cleanup_complete ? "true" : "false")
             << ", \"client_failure\": " << static_cast<unsigned>(frame.helper.failure)
             << ", \"helper_exit\": " << frame.helper.helper_exit
             << ", \"helper_failure\": " << static_cast<unsigned>(frame.helper.metadata.failure) << "}"
#endif
             << ", \"minidump\": " << (dump_written ? "true" : "false")
             << ", \"save_found\": " << (save.found ? "true" : "false")
             << ", \"save_files\": " << save.files
             << ", \"save_bytes\": " << save.bytes << ", \"minidump_error\": \""
             << JsonEscape(dump_error) << "\"},\n"
             << "  \"artifacts\": [\n";
    for (size_t i = 0; i < artifacts.size(); ++i) {
      const auto &artifact = artifacts[i];
      manifest << "    {\"name\": \"" << JsonEscape(artifact.name)
               << "\", \"bytes\": " << artifact.bytes << ", \"sha256\": \""
               << artifact.sha256 << "\", \"safe_to_share\": "
               << (artifact.safe_to_share ? "true" : "false") << "}";
      manifest << (i + 1 == artifacts.size() ? "\n" : ",\n");
    }
    manifest << "  ]\n}\n";
    if (token.stop_requested()) return false;
    if (!WriteText(partial / "manifest.json", manifest.str())) {
      REXLOG_ERROR("MCLA diagnostics: manifest write failed");
      return false;
    }
    if (token.stop_requested()) return false;
    std::filesystem::rename(partial, complete, ec);
    if (ec) {
      REXLOG_ERROR("MCLA diagnostics: snapshot publish failed: {}",
                   ec.message());
      return false;
    }
    const bool pointer_written =
        WriteTextAtomic(diagnostics_root_ / "latest-live.txt", name + "\n");
    PruneDirectories(live_root_, kLiveRetentionCount, kLiveRetentionBytes);
    if (pointer_written) {
      REXLOG_INFO("MCLA diagnostics: snapshot complete: "
                  "<user-data>/diagnostics/live/{}",
                  name);
    } else {
      REXLOG_ERROR("MCLA diagnostics: snapshot package completed but latest "
                   "pointer update failed");
    }
    return pointer_written;
  }

  std::filesystem::path user_data_root_;
  std::filesystem::path diagnostics_root_;
  std::filesystem::path live_root_;
  std::filesystem::path runtime_root_;
  std::filesystem::path journal_path_;
  CaptureProvider provider_;
  GuestFrameProvider guest_frame_provider_;
  uint64_t last_window_frame_digest_ = 0;
  std::shared_ptr<rex::LogCaptureSink> capture_sink_;
  spdlog::sink_ptr journal_sink_;
  std::jthread worker_;
  std::mutex request_mutex_;
  std::condition_variable_any request_cv_;
  std::string requested_reason_;
  UiState requested_ui_state_;
  bool request_pending_ = false;
  std::mutex idle_mutex_;
  std::condition_variable idle_cv_;
  std::atomic<bool> started_{false};
  std::atomic<bool> stopping_{false};
  std::atomic<bool> busy_{false};
  std::atomic<SnapshotStatus> snapshot_status_{SnapshotStatus::kIdle};
  std::atomic<uint64_t> sequence_{0};
  bool native_ready_ = false;
};

Manager::Manager() : impl_(std::make_unique<Impl>()) {}
Manager::~Manager() { impl_->Stop(); }

bool Manager::Start(const std::filesystem::path &user_data_root,
                    const std::filesystem::path &diagnostics_root,
                    CaptureProvider provider, bool show_crash_reporter_dialog) {
  return impl_->Start(user_data_root, diagnostics_root, std::move(provider),
                      show_crash_reporter_dialog);
}

bool Manager::RequestSnapshot(std::string reason, const UiState &ui_state) {
  return impl_->RequestSnapshot(std::move(reason), ui_state);
}

void Manager::SetGuestFrameProvider(GuestFrameProvider provider) {
  impl_->SetGuestFrameProvider(std::move(provider));
}

SnapshotStatus Manager::snapshot_status() const {
  return impl_->snapshot_status();
}

bool Manager::WaitForIdle(std::chrono::milliseconds timeout) {
  return impl_->WaitForIdle(timeout);
}

void Manager::Stop() { impl_->Stop(); }

} // namespace mcla::diagnostics
