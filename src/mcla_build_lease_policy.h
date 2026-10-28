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
// Kill-switch builds (ticket V2, M9-005): offline start is admitted for 3 days
// from creation, or for 24 hours after an online check that listed the host.
inline constexpr uint64_t kKillSwitchOfflineSeconds = 259200;
inline constexpr uint64_t kKillSwitchCheckSeconds = 86400;
struct Ticket { std::string host_sha256; uint64_t created_utc; bool kill_switch = false; };
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

// Kill-switch admission (V2 tickets only). `online` is the authenticated list
// from this check, or nullopt when the network failed, timed out or returned
// an untrusted response: those never revoke and never extend. `cached` is the
// list stored by the last check that listed this host, with that check's local
// UTC; `revoked` is set when a verified online list last omitted the host.
enum class KillSwitchCache { Keep, Refresh, Revoke };
struct CachedCheck { Leases leases; uint64_t checked_utc; };
struct KillSwitchDecision {
  bool allowed = false;
  uint64_t expires_utc = 0;          // last admitted second + 1 when allowed
  KillSwitchCache cache = KillSwitchCache::Keep;
  std::wstring reason;               // set when refused
};
std::expected<KillSwitchDecision, std::wstring> EvaluateKillSwitch(
    const Ticket &ticket, std::string_view actual_hash, uint64_t now,
    const std::optional<Leases> &online, const std::optional<CachedCheck> &cached, bool revoked);
} // namespace mcla::lease
