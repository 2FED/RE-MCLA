#include "mcla_build_lease_policy.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace mcla::lease {
namespace {
constexpr std::string_view kTicketHeader = "MCLA-TEST-BUILD-V1\n";
// Same row as V1; the schema alone selects kill-switch admission (M9-005).
constexpr std::string_view kKillSwitchTicketHeader = "MCLA-TEST-BUILD-V2\n";
constexpr std::string_view kLeasesHeader = "MCLA-TEST-LEASES-V1\n";
constexpr std::string_view kSignatureMarker = "--SIGNATURE--\n";
struct Algorithm {
  BCRYPT_ALG_HANDLE value = nullptr;
  ~Algorithm() { if (value) BCryptCloseAlgorithmProvider(value, 0); }
};
struct Key {
  BCRYPT_KEY_HANDLE value = nullptr;
  ~Key() { if (value) BCryptDestroyKey(value); }
};
int Nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
bool HashValid(std::string_view hash) {
  return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char c) { return Nibble(c) >= 0; });
}
bool DateValid(uint64_t value) { return value >= kMinimumUtc && value <= kMaximumUtc; }
std::expected<uint64_t, std::wstring> Date(std::string_view text) {
  if (text.empty() || text.size() > 20 || (text.size() > 1 && text.front() == '0'))
    return std::unexpected(L"Malformed build lease: noncanonical timestamp");
  uint64_t value = 0;
  for (char c : text) {
    if (c < '0' || c > '9' || value > (std::numeric_limits<uint64_t>::max() - (c - '0')) / 10)
      return std::unexpected(L"Malformed build lease: timestamp digit or overflow");
    value = value * 10 + (c - '0');
  }
  if (!DateValid(value)) return std::unexpected(L"Malformed build lease: timestamp outside admitted range");
  return value;
}
std::optional<std::string_view> Line(std::string_view &text) {
  const auto end = text.find('\n');
  if (end == text.npos) return std::nullopt;
  auto line = text.substr(0, end);
  text.remove_prefix(end + 1);
  return line;
}
std::expected<Grant, std::wstring> Row(std::string_view line) {
  if (line.size() < 66 || line[64] != ' ' || !HashValid(line.substr(0, 64)))
    return std::unexpected(L"Malformed build lease: expected canonical host-hash timestamp row");
  auto timestamp = Date(line.substr(65));
  if (!timestamp) return std::unexpected(timestamp.error());
  return Grant{std::string(line.substr(0, 64)), *timestamp};
}
std::expected<std::array<uint8_t, 32>, std::wstring> Digest(std::span<const uint8_t> bytes) {
  if (bytes.size() > std::numeric_limits<ULONG>::max())
    return std::unexpected(L"SHA256 input exceeds API byte bound");
  Algorithm algorithm;
  if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    return std::unexpected(L"Cannot initialize SHA256");
  std::array<uint8_t, 32> digest{};
  if (BCryptHash(algorithm.value, nullptr, 0, const_cast<PUCHAR>(bytes.data()),
      static_cast<ULONG>(bytes.size()), digest.data(), static_cast<ULONG>(digest.size())) < 0)
    return std::unexpected(L"Cannot compute SHA256");
  return digest;
}
std::expected<std::string_view, std::wstring> AuthenticatedPayload(
    std::string_view envelope, std::span<const uint8_t> public_key) {
  if (envelope.empty() || envelope.size() > kMaximumEnvelopeBytes)
    return std::unexpected(L"Malformed build lease: envelope byte bound");
  for (unsigned char c : envelope)
    if (c != '\n' && (c < 0x20 || c > 0x7E))
      return std::unexpected(L"Malformed build lease: only canonical ASCII and LF are admitted");
  const auto marker = envelope.find(kSignatureMarker);
  if (marker == envelope.npos || marker == 0 || envelope[marker - 1] != '\n' ||
      marker != envelope.rfind(kSignatureMarker))
    return std::unexpected(L"Malformed build lease: signature delimiter");
  const auto encoded = envelope.substr(marker + kSignatureMarker.size());
  if (encoded.size() != 513 || encoded.back() != '\n')
    return std::unexpected(L"Malformed build lease: exact RSA-2048 signature required");
  std::array<uint8_t, 256> signature{};
  for (size_t i = 0; i < signature.size(); ++i) {
    const int hi = Nibble(encoded[i * 2]), lo = Nibble(encoded[i * 2 + 1]);
    if (hi < 0 || lo < 0) return std::unexpected(L"Malformed build lease: signature must be uppercase hex");
    signature[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  if (public_key.size() < sizeof(BCRYPT_RSAKEY_BLOB))
    return std::unexpected(L"Invalid build lease verification key");
  BCRYPT_RSAKEY_BLOB header{};
  std::memcpy(&header, public_key.data(), sizeof(header));
  if (header.Magic != BCRYPT_RSAPUBLIC_MAGIC || header.BitLength != 2048 ||
      header.cbModulus != 256 || header.cbPrime1 || header.cbPrime2 ||
      !header.cbPublicExp || header.cbPublicExp > 8 ||
      public_key.size() != sizeof(header) + header.cbPublicExp + header.cbModulus)
    return std::unexpected(L"Invalid build lease verification key: exact RSA-2048 public blob required");
  const auto exponent = public_key.subspan(sizeof(header), header.cbPublicExp);
  const auto modulus = public_key.subspan(sizeof(header) + header.cbPublicExp);
  uint64_t e = 0;
  for (auto byte : exponent) e = (e << 8) | byte;
  if (!exponent.front() || e < 3 || !(e & 1) || !(modulus.front() & 0x80) || !(modulus.back() & 1))
    return std::unexpected(L"Invalid build lease verification key: noncanonical RSA parameters");
  Algorithm algorithm;
  Key key;
  if (BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_RSA_ALGORITHM, nullptr, 0) < 0 ||
      BCryptImportKeyPair(algorithm.value, nullptr, BCRYPT_RSAPUBLIC_BLOB, &key.value,
          const_cast<PUCHAR>(public_key.data()), static_cast<ULONG>(public_key.size()), 0) < 0)
    return std::unexpected(L"Cannot import build lease verification key");
  const auto payload = envelope.substr(0, marker);
  auto digest = Digest({reinterpret_cast<const uint8_t *>(payload.data()), payload.size()});
  if (!digest) return std::unexpected(digest.error());
  BCRYPT_PKCS1_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM};
  if (BCryptVerifySignature(key.value, &padding, digest->data(), static_cast<ULONG>(digest->size()),
      signature.data(), static_cast<ULONG>(signature.size()), BCRYPT_PAD_PKCS1) < 0)
    return std::unexpected(L"Build lease signature verification failed");
  return payload;
}
bool ModelValid(const Leases &leases) {
  if (!DateValid(leases.issued_utc) || leases.grants.size() > kMaximumGrants) return false;
  std::string_view previous;
  for (const auto &grant : leases.grants) {
    if (!HashValid(grant.host_sha256) || !DateValid(grant.valid_until) ||
        (!previous.empty() && previous >= grant.host_sha256)) return false;
    previous = grant.host_sha256;
  }
  return true;
}
} // namespace

std::expected<std::string, std::wstring> Sha256(std::span<const uint8_t> bytes) {
  auto digest = Digest(bytes);
  if (!digest) return std::unexpected(digest.error());
  std::string result;
  result.reserve(64);
  for (auto byte : *digest) { result += "0123456789ABCDEF"[byte >> 4]; result += "0123456789ABCDEF"[byte & 15]; }
  return result;
}
std::expected<Ticket, std::wstring> ParseTicket(
    std::string_view envelope, std::span<const uint8_t> public_key) {
  auto authenticated = AuthenticatedPayload(envelope, public_key);
  if (!authenticated) return std::unexpected(authenticated.error());
  auto payload = *authenticated;
  const bool kill_switch = payload.starts_with(kKillSwitchTicketHeader);
  if (!kill_switch && !payload.starts_with(kTicketHeader))
    return std::unexpected(L"Malformed build ticket: unknown schema");
  payload.remove_prefix(kill_switch ? kKillSwitchTicketHeader.size() : kTicketHeader.size());
  auto line = Line(payload);
  if (!line || !payload.empty()) return std::unexpected(L"Malformed build ticket: exactly one row required");
  auto row = Row(*line);
  if (!row) return std::unexpected(row.error());
  return Ticket{std::move(row->host_sha256), row->valid_until, kill_switch};
}
std::expected<Leases, std::wstring> ParseLeases(
    std::string_view envelope, std::span<const uint8_t> public_key) {
  auto authenticated = AuthenticatedPayload(envelope, public_key);
  if (!authenticated) return std::unexpected(authenticated.error());
  auto payload = *authenticated;
  if (!payload.starts_with(kLeasesHeader)) return std::unexpected(L"Malformed build leases: unknown schema");
  payload.remove_prefix(kLeasesHeader.size());
  auto issued_line = Line(payload);
  if (!issued_line) return std::unexpected(L"Malformed build leases: missing issued timestamp");
  auto issued = Date(*issued_line);
  if (!issued) return std::unexpected(issued.error());
  Leases result{*issued, {}};
  while (!payload.empty()) {
    if (result.grants.size() >= kMaximumGrants) return std::unexpected(L"Malformed build leases: too many grants");
    auto line = Line(payload);
    if (!line) return std::unexpected(L"Malformed build leases: final LF required");
    auto row = Row(*line);
    if (!row) return std::unexpected(row.error());
    if (!result.grants.empty() && result.grants.back().host_sha256 >= row->host_sha256)
      return std::unexpected(L"Malformed build leases: hosts must be strictly sorted and unique");
    result.grants.push_back(std::move(*row));
  }
  return result;
}
std::expected<uint64_t, std::wstring> Evaluate(
    const Ticket &ticket, std::string_view actual_hash, uint64_t now,
    const std::optional<Leases> &leases) {
  // A V2 ticket must never fall back to the 14-day V1 allowance.
  if (ticket.kill_switch) return std::unexpected(L"Kill-switch build ticket requires kill-switch admission");
  if (!HashValid(ticket.host_sha256) || !DateValid(ticket.created_utc) || !HashValid(actual_hash))
    return std::unexpected(L"Malformed build ticket or measured host hash");
  if (ticket.host_sha256 != actual_hash) return std::unexpected(L"Build ticket does not match the measured host");
  if (!DateValid(now)) return std::unexpected(L"Clock error: current UTC is outside the admitted range");
  if (now < ticket.created_utc) return std::unexpected(L"Clock error: current UTC precedes build creation");
  uint64_t expiry = ticket.created_utc + kGraceSeconds;
  if (leases) {
    if (!ModelValid(*leases)) return std::unexpected(L"Malformed supplied build leases");
    if (leases->issued_utc > now && leases->issued_utc - now > 300)
      return std::unexpected(L"Clock error: signed build leases were issued more than 300 seconds in the future");
    for (const auto &grant : leases->grants)
      if (grant.host_sha256 == actual_hash) expiry = std::max(expiry, grant.valid_until);
  }
  if (now >= expiry) return std::unexpected(L"BUILD EXPIRED: obtain a newer test build or a signed extension.");
  return expiry;
}
std::expected<KillSwitchDecision, std::wstring> EvaluateKillSwitch(
    const Ticket &ticket, std::string_view actual_hash, uint64_t now,
    const std::optional<Leases> &online, const std::optional<CachedCheck> &cached, bool revoked) {
  if (!ticket.kill_switch) return std::unexpected(L"Kill-switch admission requires a V2 build ticket");
  if (!HashValid(ticket.host_sha256) || !DateValid(ticket.created_utc) || !HashValid(actual_hash))
    return std::unexpected(L"Malformed build ticket or measured host hash");
  if (ticket.host_sha256 != actual_hash) return std::unexpected(L"Build ticket does not match the measured host");
  if (!DateValid(now)) return std::unexpected(L"Clock error: current UTC is outside the admitted range");
  if (now < ticket.created_utc) return std::unexpected(L"Clock error: current UTC precedes build creation");
  const auto listed = [&](const Leases &leases) -> const Grant * {
    for (const auto &grant : leases.grants)
      if (grant.host_sha256 == actual_hash) return &grant;
    return nullptr;
  };
  // A verified online list is authoritative. A malformed or future-issued list
  // is treated as no answer: it can neither revoke nor extend.
  if (online && ModelValid(*online) && !(online->issued_utc > now && online->issued_utc - now > 300)) {
    const auto *grant = listed(*online);
    if (grant && grant->valid_until > now)
      return KillSwitchDecision{true, std::min(now + kKillSwitchCheckSeconds, grant->valid_until),
                                KillSwitchCache::Refresh, {}};
    return KillSwitchDecision{false, 0, KillSwitchCache::Revoke,
        L"This test build was withdrawn. Your saves are unchanged. Get the current build from the owner."};
  }
  if (revoked)
    return KillSwitchDecision{false, 0, KillSwitchCache::Keep,
        L"This test build was withdrawn. Your saves are unchanged. Get the current build from the owner."};
  uint64_t expiry = ticket.created_utc + kKillSwitchOfflineSeconds;
  if (cached && ModelValid(cached->leases) && DateValid(cached->checked_utc) && cached->checked_utc <= now) {
    const auto *grant = listed(cached->leases);
    if (grant) expiry = std::max(expiry, std::min(cached->checked_utc + kKillSwitchCheckSeconds, grant->valid_until));
  }
  if (now >= expiry)
    return KillSwitchDecision{false, 0, KillSwitchCache::Keep,
        L"BUILD CHECK NEEDED - connect to the internet once and start again. Your saves are unchanged."};
  return KillSwitchDecision{true, expiry, KillSwitchCache::Keep, {}};
}
} // namespace mcla::lease
