#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace mcla::lease {
inline constexpr size_t kMaximumPublicLeaseBytes = 65536;
inline constexpr uint32_t kPublicLeaseDeadlineMs = 5000;

// One fixed public HTTPS GET; no caller-provided URL, credentials, cookies,
// proxy discovery, redirects or telemetry. Transport bytes are NOT trusted:
// the caller must verify the signed document before using or caching it.
std::expected<std::string, std::wstring> FetchPublicLeases();

// Pure transport-completion validation, also used by offline tests. A missing
// Content-Length is allowed only after WinHTTP signals a successful EOF.
std::expected<void, std::wstring> ValidateHttpBody(
    uint32_t status, std::optional<uint64_t> declared_length,
    std::string_view body, bool successful_eof);
} // namespace mcla::lease
