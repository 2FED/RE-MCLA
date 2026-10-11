#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mcla::lease {
inline constexpr uint64_t kMinimumUtc = 1577836800;
inline constexpr uint64_t kMaximumUtc = 4102444800;
inline constexpr uint64_t kGraceSeconds = 1209600;
inline constexpr size_t kMaximumEnvelopeBytes = 64 * 1024;
inline constexpr size_t kMaximumGrants = 256;
struct Ticket { std::string host_sha256; uint64_t created_utc; };
struct Grant { std::string host_sha256; uint64_t valid_until; };
struct Leases { uint64_t issued_utc; std::vector<Grant> grants; };

// Key is a BCrypt BCRYPT_RSAPUBLIC_BLOB for exactly RSA-2048. Nothing is
// persisted and no network/clock access occurs here. Envelope authentication
// binds every canonical payload byte, including its final LF.
std::expected<Ticket, std::wstring> ParseTicket(
    std::string_view envelope, std::span<const uint8_t> public_key);
std::expected<Leases, std::wstring> ParseLeases(
    std::string_view envelope, std::span<const uint8_t> public_key);
std::expected<std::string, std::wstring> Sha256(std::span<const uint8_t> bytes);

// Call with authenticated Parse* results and the freshly measured host hash.
// Public model fields are revalidated, but Evaluate does not reauthenticate
// caller-constructed models. Supplied malformed/future-issued leases fail,
// rather than silently degrading to the grace period. Expiry is exclusive.
std::expected<uint64_t, std::wstring> Evaluate(
    const Ticket &ticket, std::string_view actual_hash, uint64_t now,
    const std::optional<Leases> &leases = std::nullopt);
} // namespace mcla::lease
