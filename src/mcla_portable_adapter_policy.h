#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace mcla::portable {

inline bool ValidLocalProfileName(std::string_view name) {
  if (name.empty() || name.size() > 15 || name.front() == ' ' ||
      name.back() == ' ') {
    return false;
  }
  for (unsigned char c : name) {
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-')) {
      return false;
    }
  }
  return true;
}

struct AdapterCandidate {
  uint32_t index;
  std::wstring name;
  bool d3d12_capable;
};

inline bool ValidAdapterName(std::wstring_view name) {
  if (name.empty() || name.size() > 127) {
    return false;
  }
  for (wchar_t character : name) {
    if (character < 0x20 || character == 0x7f) {
      return false;
    }
  }
  return true;
}

inline std::optional<uint32_t> SelectExactAdapter(
    std::span<const AdapterCandidate> adapters, std::wstring_view name,
    std::string &error) {
  if (!ValidAdapterName(name)) {
    error = "adapter name must be a nonempty bounded exact DXGI description";
    return std::nullopt;
  }
  std::optional<uint32_t> selected;
  for (const auto &adapter : adapters) {
    if (adapter.name == name && adapter.d3d12_capable) {
      if (selected) {
        error = "adapter name is ambiguous among D3D12-capable DXGI adapters";
        return std::nullopt;
      }
      selected = adapter.index;
    }
  }
  if (!selected) {
    error = "exact named D3D12-capable adapter is unavailable; no fallback";
  }
  return selected;
}

// Named selection must imply D3D12 rather than leaving backend auto-selection.
inline bool CompatibleNamedBackend(std::wstring_view backend) {
  return backend.empty() || backend == L"d3d12";
}

inline std::string_view TrimGpuConfigSpace(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t' ||
                           text.front() == '\r')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t' ||
                           text.back() == '\r')) {
    text.remove_suffix(1);
  }
  return text;
}

// The host applies TOML after CLI. For explicit launcher GPU choices accept
// only the shipped flat, single-line scalar configuration subset, and forbid
// its protected keys entirely (even an apparently matching numeric index).
// No quoted/escaped/dotted keys, tables, multiline values or include machinery
// can conceal a later override. This is deliberately not a general TOML parser.
inline bool ValidateExplicitGpuConfig(std::string_view text, bool named_adapter,
                                     std::string &error,
                                     bool profile_managed = false,
                                     bool gpu_managed = true) {
  if (text.size() > 1024 * 1024 || text.find('\0') != text.npos) {
    error = "GPU selection config is oversized or contains NUL";
    return false;
  }
  if (text.starts_with("\xEF\xBB\xBF")) {
    text.remove_prefix(3);
  }
  while (!text.empty()) {
    const size_t newline = text.find('\n');
    auto line = TrimGpuConfigSpace(text.substr(0, newline));
    text = newline == text.npos ? std::string_view{} : text.substr(newline + 1);
    if (line.empty() || line.front() == '#') {
      continue;
    }
    const size_t equal = line.find('=');
    if (equal == line.npos) {
      error = "explicit GPU selection requires flat scalar TOML";
      return false;
    }
    const auto key = TrimGpuConfigSpace(line.substr(0, equal));
    if (key.empty()) {
      error = "empty config key";
      return false;
    }
    for (char character : key) {
      if (!((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '_' ||
            character == '-')) {
        error = "explicit GPU selection forbids quoted/dotted/table config keys";
        return false;
      }
    }
    if ((gpu_managed && key == "gpu_backend") ||
        (named_adapter && key == "d3d12_adapter") ||
        (profile_managed && key == "local_user_display_name")) {
      error = "immutable TOML must omit launcher-managed GPU/profile keys";
      return false;
    }
    auto value = TrimGpuConfigSpace(line.substr(equal + 1));
    if (value.empty()) {
      error = "empty scalar config value";
      return false;
    }
    if (value.front() == '"' || value.front() == '\'') {
      const char quote = value.front();
      size_t end = 1;
      bool closed = false;
      for (; end < value.size(); ++end) {
        if (static_cast<unsigned char>(value[end]) < 0x20) {
          break;
        }
        if (value[end] == quote) {
          ++end;
          closed = true;
          break;
        }
        if (quote == '"' && value[end] == '\\') {
          if (++end == value.size() ||
              static_cast<unsigned char>(value[end]) < 0x20) {
            break;
          }
        }
      }
      const auto tail = closed ? TrimGpuConfigSpace(value.substr(end)) : value;
      if (!closed || (!tail.empty() && tail.front() != '#')) {
        error = "explicit GPU selection forbids multiline/compound strings";
        return false;
      }
    } else {
      value = TrimGpuConfigSpace(value.substr(0, value.find('#')));
      if (value.empty()) {
        error = "missing scalar config value";
        return false;
      }
      for (char character : value) {
        if (!((character >= 'a' && character <= 'z') ||
              (character >= 'A' && character <= 'Z') ||
              (character >= '0' && character <= '9') || character == '_' ||
              character == '+' || character == '-' || character == '.')) {
          error = "explicit GPU selection requires single-line scalar values";
          return false;
        }
      }
    }
  }
  return true;
}

} // namespace mcla::portable
