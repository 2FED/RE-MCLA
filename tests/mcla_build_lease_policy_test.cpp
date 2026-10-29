#include "mcla_build_lease_policy.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mcla::lease;
unsigned checks = 0, failures = 0;
constexpr uint64_t created = 1893456000; // 2030-01-01 UTC; deterministic, no wall clock.
const std::string hash_a(64, 'A'), hash_b(64, 'B');
void Check(bool pass, const char *label) {
  ++checks;
  if (!pass) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::array<uint8_t, 32> Digest(std::string_view input) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    throw std::runtime_error("fixture SHA256 initialization");
  std::array<uint8_t, 32> bytes{};
  const auto result = BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char *>(input.data())),
      static_cast<ULONG>(input.size()), bytes.data(), static_cast<ULONG>(bytes.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (result < 0) throw std::runtime_error("fixture SHA256 hashing");
  return bytes;
}
struct SigningKey {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_KEY_HANDLE key = nullptr;
  std::vector<uint8_t> public_blob;
  SigningKey() {
    // This ephemeral fixture key never reaches disk, the network or production.
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGenerateKeyPair(algorithm, &key, 2048, 0) < 0 || BCryptFinalizeKeyPair(key, 0) < 0)
      throw std::runtime_error("ephemeral fixture RSA generation");
    ULONG length = 0;
    if (BCryptExportKey(key, nullptr, BCRYPT_RSAPUBLIC_BLOB, nullptr, 0, &length, 0) < 0)
      throw std::runtime_error("fixture public key sizing");
    public_blob.resize(length);
    if (BCryptExportKey(key, nullptr, BCRYPT_RSAPUBLIC_BLOB, public_blob.data(), length, &length, 0) < 0)
      throw std::runtime_error("fixture public key export");
  }
  ~SigningKey() {
    if (key) BCryptDestroyKey(key);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
  }
  SigningKey(const SigningKey &) = delete;
  SigningKey &operator=(const SigningKey &) = delete;
  std::string Sign(std::string_view payload) const {
    auto digest = Digest(payload);
    BCRYPT_PKCS1_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM};
    std::array<uint8_t, 256> signature{};
    ULONG length = 0;
    if (BCryptSignHash(key, &padding, digest.data(), static_cast<ULONG>(digest.size()), signature.data(),
        static_cast<ULONG>(signature.size()), &length, BCRYPT_PAD_PKCS1) < 0 || length != signature.size())
      throw std::runtime_error("ephemeral fixture signing");
    std::string envelope(payload);
    envelope += "--SIGNATURE--\n";
    for (auto byte : signature) { envelope += "0123456789ABCDEF"[byte >> 4]; envelope += "0123456789ABCDEF"[byte & 15]; }
    return envelope + '\n';
  }
};
std::string TicketPayload(std::string_view hash = hash_a, std::string_view timestamp = "1893456000") {
  return "MCLA-TEST-BUILD-V1\n" + std::string(hash) + " " + std::string(timestamp) + '\n';
}
std::string LeasePayload(std::string_view rows = {}, std::string_view timestamp = "1893456000") {
  return "MCLA-TEST-LEASES-V1\n" + std::string(timestamp) + '\n' + std::string(rows);
}
std::string NumberHash(unsigned value) {
  std::string hash(64, '0');
  for (size_t i = 64; value; value >>= 4) hash[--i] = "0123456789ABCDEF"[value & 15];
  return hash;
}
} // namespace

int main() {
  try {
    SigningKey signer;
    const auto ticket_envelope = signer.Sign(TicketPayload());
    auto parsed_ticket = ParseTicket(ticket_envelope, signer.public_blob);
    Check(parsed_ticket && parsed_ticket->host_sha256 == hash_a && parsed_ticket->created_utc == created,
        "canonical signed ticket authenticates and parses exactly");
    const auto empty_leases = signer.Sign(LeasePayload());
    auto parsed_empty = ParseLeases(empty_leases, signer.public_blob);
    Check(parsed_empty && parsed_empty->issued_utc == created && parsed_empty->grants.empty(), "zero-row remote leases accepted");
    const auto rows = hash_a + " 1896000000\n" + hash_b + " 1897000000\n";
    const auto lease_envelope = signer.Sign(LeasePayload(rows));
    auto parsed_leases = ParseLeases(lease_envelope, signer.public_blob);
    Check(parsed_leases && parsed_leases->grants.size() == 2 && parsed_leases->grants.front().valid_until == 1896000000,
        "sorted distinct signed lease rows parsed");
    Check(!ParseTicket(lease_envelope, signer.public_blob) && !ParseLeases(ticket_envelope, signer.public_blob),
        "signed schemas are not interchangeable");
    SigningKey other;
    Check(!ParseTicket(ticket_envelope, other.public_blob), "valid signature under a different key rejected");
    Check(!ParseTicket(ticket_envelope, {}), "missing public key rejected");
    auto malformed_key = signer.public_blob;
    malformed_key.push_back(0);
    Check(!ParseTicket(ticket_envelope, malformed_key), "key trailing bytes rejected");
    malformed_key = signer.public_blob;
    BCRYPT_RSAKEY_BLOB header{};
    std::memcpy(&header, malformed_key.data(), sizeof(header));
    header.BitLength = 1024;
    std::memcpy(malformed_key.data(), &header, sizeof(header));
    Check(!ParseTicket(ticket_envelope, malformed_key), "non-2048 key rejected");
    for (size_t i = 0; i < ticket_envelope.size(); ++i)
      Check(!ParseTicket(std::string_view(ticket_envelope).substr(0, i), signer.public_blob), "every ticket truncation rejected");
    for (size_t i = 0; i < lease_envelope.size(); ++i)
      Check(!ParseLeases(std::string_view(lease_envelope).substr(0, i), signer.public_blob), "every leases truncation rejected");
    auto tampered = ticket_envelope;
    tampered[18] = tampered[18] == 'A' ? 'B' : 'A';
    Check(!ParseTicket(tampered, signer.public_blob), "changed signed payload rejected");
    tampered = ticket_envelope;
    const auto signature_start = tampered.find("--SIGNATURE--\n") + std::string_view("--SIGNATURE--\n").size();
    tampered[signature_start] = tampered[signature_start] == '0' ? '1' : '0';
    Check(!ParseTicket(tampered, signer.public_blob), "changed signature rejected");
    tampered = ticket_envelope;
    tampered[signature_start] = 'a';
    Check(!ParseTicket(tampered, signer.public_blob), "lowercase signature hex rejected before verification");
    Check(!ParseTicket(ticket_envelope + '\n', signer.public_blob), "trailing newline rejected");
    Check(!ParseTicket(ticket_envelope + "trailing", signer.public_blob), "trailing nonempty bytes rejected");
    Check(!ParseTicket(ticket_envelope + std::string(1, '\0'), signer.public_blob), "embedded NUL rejected");
    Check(!ParseTicket(std::string(kMaximumEnvelopeBytes + 1, 'A'), signer.public_blob), "oversized envelope rejected");
    Check(!ParseTicket(signer.Sign(TicketPayload() + "--SIGNATURE--\n"), signer.public_blob), "duplicate signature marker rejected");
    for (const auto &timestamp : {"", "0", "01893456000", "+1893456000", "-1893456000", "1893456000 ",
        "1893456000\t", "1893456000.0", "1e9", "18446744073709551616", "999999999999999999999",
        "1577836799", "4102444801"}) {
      Check(!ParseTicket(signer.Sign(TicketPayload(hash_a, timestamp)), signer.public_blob), "signed noncanonical/range/overflow ticket date rejected");
      Check(!ParseLeases(signer.Sign(LeasePayload({}, timestamp)), signer.public_blob), "signed noncanonical/range/overflow issue date rejected");
    }
    for (const auto &timestamp : {"1577836800", "4102444800"}) {
      Check(ParseTicket(signer.Sign(TicketPayload(hash_a, timestamp)), signer.public_blob).has_value(), "inclusive admitted ticket date endpoints");
      Check(ParseLeases(signer.Sign(LeasePayload(hash_a + " " + timestamp + '\n', timestamp)), signer.public_blob).has_value(), "inclusive admitted lease date endpoints");
    }
    for (const auto &bad_hash : {std::string(63, 'A'), std::string(65, 'A'), std::string(64, 'a'), std::string(64, 'G')})
      Check(!ParseTicket(signer.Sign(TicketPayload(bad_hash)), signer.public_blob), "signed invalid host hash rejected");
    for (const auto &bad_payload : {std::string("MCLA-TEST-BUILD-V3\n") + hash_a + " 1893456000\n",
        TicketPayload() + '\n', TicketPayload() + hash_b + " 1893456000\n", TicketPayload(hash_a, " 1893456000"),
        TicketPayload(hash_a, "1893456000\r"), TicketPayload(hash_a, "1893456000") + std::string(1, char(0xFF))})
      Check(!ParseTicket(signer.Sign(bad_payload), signer.public_blob), "signed noncanonical ticket grammar rejected");
    Check(!ParseTicket(signer.Sign(TicketPayload().substr(0, TicketPayload().size() - 1)), signer.public_blob), "ticket final LF required");
    for (const auto &bad_rows : {hash_a + " 1896000000\n" + hash_a + " 1897000000\n",
        hash_b + " 1896000000\n" + hash_a + " 1897000000\n", hash_a + " 1896000000\n\n",
        hash_a + " 1896000000", hash_a + " 01896000000\n", hash_a + "  1896000000\n"})
      Check(!ParseLeases(signer.Sign(LeasePayload(bad_rows)), signer.public_blob), "duplicate unsorted or malformed signed lease rows rejected");
    std::string many_rows;
    for (unsigned i = 0; i < kMaximumGrants; ++i) many_rows += NumberHash(i) + " 1896000000\n";
    auto maximum = ParseLeases(signer.Sign(LeasePayload(many_rows)), signer.public_blob);
    Check(maximum && maximum->grants.size() == kMaximumGrants, "exact 256-row bound accepted");
    many_rows += NumberHash(256) + " 1896000000\n";
    Check(!ParseLeases(signer.Sign(LeasePayload(many_rows)), signer.public_blob), "257th row rejected");

    const Ticket ticket{hash_a, created};
    auto expiry = Evaluate(ticket, hash_a, created);
    Check(expiry && *expiry == created + kGraceSeconds, "creation instant admits exact two-week grace");
    Check(Evaluate(ticket, hash_a, created + kGraceSeconds - 1).has_value(), "last second before grace expiry admitted");
    auto expired = Evaluate(ticket, hash_a, created + kGraceSeconds);
    Check(!expired && expired.error().starts_with(L"BUILD EXPIRED"), "grace expiration boundary exclusive and explicit");
    Check(!Evaluate(ticket, hash_b, created), "ticket bound to exact measured host");
    auto future = Evaluate(ticket, hash_a, created - 1);
    Check(!future && future.error().starts_with(L"Clock error"), "clock before creation rejected distinctly");
    Check(!Evaluate(ticket, hash_a, 0) && !Evaluate(ticket, hash_a, kMaximumUtc + 1), "clock range enforced");
    const Leases granted{created, {{hash_a, created + kGraceSeconds + 100}}};
    expiry = Evaluate(ticket, hash_a, created + kGraceSeconds, granted);
    Check(expiry && *expiry == granted.grants[0].valid_until, "only signed exact-host extension extends expired grace");
    Check(!Evaluate(ticket, hash_a, granted.grants[0].valid_until, granted), "extension validity upper bound exclusive");
    expiry = Evaluate(ticket, hash_a, created, Leases{created, {{hash_a, created + 1}}});
    Check(expiry && *expiry == created + kGraceSeconds, "shorter grant does not revoke original grace");
    Check(!Evaluate(ticket, hash_a, created + kGraceSeconds, Leases{created, {{hash_b, kMaximumUtc}}}), "different-host extension cannot grant access");
    Check(Evaluate(ticket, hash_a, created, Leases{created + 300, {}}).has_value(), "exact future issue skew allowance admitted");
    future = Evaluate(ticket, hash_a, created, Leases{created + 301, {}});
    Check(!future && future.error().starts_with(L"Clock error"), "future issue beyond allowance rejects even during grace");
    Check(!Evaluate(ticket, hash_a, created, Leases{0, {}}), "caller-constructed invalid issue date rejected");
    Check(!Evaluate(ticket, hash_a, created, Leases{created, {{hash_a, 0}}}), "caller-constructed invalid grant date rejected");
    Check(!Evaluate(ticket, hash_a, created, Leases{created, {{hash_a, created}, {hash_a, created}}}), "caller-constructed duplicate grant rejected");
    Check(!Evaluate(ticket, hash_a, created, Leases{created, {{hash_b, created}, {hash_a, created}}}), "caller-constructed unsorted grants rejected");
    Check(!Evaluate(Ticket{hash_a, 0}, hash_a, created), "caller-constructed invalid ticket rejected");
    Check(!Evaluate(ticket, std::string(64, 'a'), created), "measured hash canonical encoding required");
    const std::string abc = "abc";
    const auto hashed = Sha256({reinterpret_cast<const uint8_t *>(abc.data()), abc.size()});
    Check(hashed && *hashed == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD", "SHA256 utility known vector");
    const auto empty_hash = Sha256({});
    Check(empty_hash && *empty_hash == "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855", "SHA256 empty vector");
    {
      // M9-005 kill switch: V2 ticket schema and pure admission matrix.
      const auto v2 = ParseTicket(signer.Sign(std::string("MCLA-TEST-BUILD-V2\n") + hash_a + " 1893456000\n"), signer.public_blob);
      Check(v2 && v2->kill_switch && v2->created_utc == 1893456000, "V2 ticket parses as kill-switch build");
      const auto v1 = ParseTicket(signer.Sign(TicketPayload(hash_a, "1893456000")), signer.public_blob);
      Check(v1 && !v1->kill_switch, "V1 ticket keeps the 14-day policy");
      const Ticket ks{hash_a, created, true};
      Check(!Evaluate(ks, hash_a, created), "V2 ticket never falls back to the V1 grace");
      Check(!EvaluateKillSwitch(ticket, hash_a, created, std::nullopt, std::nullopt, false), "V1 ticket rejected by kill-switch admission");
      const uint64_t window = created + kKillSwitchOfflineSeconds;
      const Leases listed{created, {{hash_a, created + 30 * 86400}}}, unlisted{created, {{hash_b, created + 30 * 86400}}};
      auto d = EvaluateKillSwitch(ks, hash_a, created, std::nullopt, std::nullopt, false);
      Check(d && d->allowed && d->expires_utc == window && d->cache == KillSwitchCache::Keep, "offline: 3 days from creation");
      d = EvaluateKillSwitch(ks, hash_a, window, std::nullopt, std::nullopt, false);
      Check(d && !d->allowed && d->cache == KillSwitchCache::Keep, "offline after 3 days refused, nothing revoked");
      d = EvaluateKillSwitch(ks, hash_a, window + 10, listed, std::nullopt, false);
      Check(d && d->allowed && d->expires_utc == window + 10 + kKillSwitchCheckSeconds && d->cache == KillSwitchCache::Refresh,
            "online listed: admitted, 24 h cache refreshed");
      d = EvaluateKillSwitch(ks, hash_a, created + 10, unlisted, std::nullopt, false);
      Check(d && !d->allowed && d->cache == KillSwitchCache::Revoke, "online unlisted: refused and revoked inside 3 days");
      d = EvaluateKillSwitch(ks, hash_a, created + 10, Leases{created, {{hash_a, created + 5}}}, std::nullopt, false);
      Check(d && !d->allowed && d->cache == KillSwitchCache::Revoke, "listed grant past valid_until is withdrawn");
      d = EvaluateKillSwitch(ks, hash_a, created + 10, Leases{created + 100000, {}}, std::nullopt, false);
      Check(d && d->allowed && d->cache == KillSwitchCache::Keep, "future-issued list is no answer: neither revokes nor extends");
      d = EvaluateKillSwitch(ks, hash_a, created + 10, Leases{0, {}}, std::nullopt, false);
      Check(d && d->allowed && d->cache == KillSwitchCache::Keep, "malformed list model is no answer");
      const CachedCheck check{listed, window + 10};
      d = EvaluateKillSwitch(ks, hash_a, window + 10 + kKillSwitchCheckSeconds - 1, std::nullopt, check, false);
      Check(d && d->allowed, "offline within 24 h of the listing check");
      d = EvaluateKillSwitch(ks, hash_a, window + 10 + kKillSwitchCheckSeconds, std::nullopt, check, false);
      Check(d && !d->allowed, "offline 24 h after the check refused");
      d = EvaluateKillSwitch(ks, hash_a, created + 60, std::nullopt, CachedCheck{Leases{created, {{hash_a, created + 120}}}, created + 50}, false);
      Check(d && d->allowed && d->expires_utc == window, "cached grant cannot shorten the creation window");
      d = EvaluateKillSwitch(ks, hash_a, window + 20, std::nullopt, CachedCheck{Leases{created, {{hash_a, window + 30}}}, window + 10}, false);
      Check(d && d->allowed && d->expires_utc == window + 30, "cache window capped by the grant's valid_until");
      d = EvaluateKillSwitch(ks, hash_a, window + 5, std::nullopt, CachedCheck{listed, window + 10}, false);
      Check(d && d->allowed == false, "check time after now (clock rollback) gives no cached window");
      d = EvaluateKillSwitch(ks, hash_a, window + 20, std::nullopt, CachedCheck{unlisted, window + 10}, false);
      Check(d && !d->allowed, "cached list without the host gives no window");
      d = EvaluateKillSwitch(ks, hash_a, created + 10, std::nullopt, std::nullopt, true);
      Check(d && !d->allowed && d->reason.find(L"withdrawn") != std::wstring::npos, "revoked stays refused offline inside 3 days");
      d = EvaluateKillSwitch(ks, hash_a, created + 10, listed, std::nullopt, true);
      Check(d && d->allowed && d->cache == KillSwitchCache::Refresh, "a later listing restores a revoked build");
      Check(!EvaluateKillSwitch(ks, hash_b, created, std::nullopt, std::nullopt, false), "wrong host rejected");
      Check(!EvaluateKillSwitch(ks, hash_a, created - 1, std::nullopt, std::nullopt, false), "clock before creation rejected");
    }
    {
      // M9-005 signed capabilities line: optional, order-insensitive, strict.
      const auto row = hash_a + " 1893456000\n";
      const auto ticket = [&](std::string_view schema, std::string_view extra) {
        return ParseTicket(signer.Sign(std::string(schema) + row + std::string(extra)), signer.public_blob);
      };
      for (const auto *schema : {"MCLA-TEST-BUILD-V1\n", "MCLA-TEST-BUILD-V2\n"}) {
        const auto none = ticket(schema, "");
        Check(none && none->capabilities == 0, "missing capabilities line grants nothing");
        const auto granted = ticket(schema, "capabilities unlocked,1440p\n");
        Check(granted && granted->capabilities == (kGrantUnlocked | kGrant1440p) &&
              granted->host_sha256 == hash_a && granted->created_utc == created,
              "signed capabilities line parses with the row unchanged");
        const auto reordered = ticket(schema, "capabilities 1440p,unlocked\n");
        Check(reordered && reordered->capabilities == granted->capabilities, "token order does not matter");
        const auto all = ticket(schema, "capabilities 2160p,unlocked,1080p,1440p\n");
        Check(all && all->capabilities == kGrantAll, "all four tokens parse");
        const auto single = ticket(schema, "capabilities 1080p\n");
        Check(single && single->capabilities == kGrant1080p, "single token grants only itself");
        for (const auto *bad : {"capabilities\n", "capabilities \n", "capabilities unlocked,unlocked\n",
                                "capabilities 1080p,1440p,1080p\n", "capabilities 720p\n", "capabilities 60fps\n",
                                "capabilities Unlocked\n", "capabilities 1080P,1440p\n", "capabilities unlocked,\n",
                                "capabilities ,unlocked\n", "capabilities unlocked,,1440p\n", "capabilities unlocked 1440p\n",
                                "capabilities unlocked;1440p\n", "capabilities  unlocked\n", "capabilities unlocked \n",
                                "capability unlocked\n", "Capabilities unlocked\n", "capabilities unlocked",
                                "capabilities unlocked\n\n", "capabilities unlocked\ncapabilities 1440p\n",
                                "extra line\n", "\n"})
          Check(!ticket(schema, bad), "malformed, unknown or duplicate capability token rejects the ticket");
        // The line is signed: appending or editing it after signing fails.
        const auto signed_plain = signer.Sign(std::string(schema) + row);
        const auto marker = signed_plain.find("--SIGNATURE--\n");
        Check(!ParseTicket(signed_plain.substr(0, marker) + "capabilities unlocked\n" + signed_plain.substr(marker),
                           signer.public_blob), "a capabilities line added after signing is rejected");
        auto edited = signer.Sign(std::string(schema) + row + "capabilities 1080p\n");
        edited.replace(edited.find("1080p"), 5, "2160p");
        Check(!ParseTicket(edited, signer.public_blob), "a capabilities line edited after signing is rejected");
      }
      const auto v2 = ticket("MCLA-TEST-BUILD-V2\n", "capabilities unlocked,1080p,1440p\n");
      Check(v2 && v2->kill_switch && v2->capabilities == (kGrantUnlocked | kGrant1080p | kGrant1440p),
            "volunteer default grant parses on a V2 ticket");
      Check(ParseTicketCapabilities("unlocked,1080p,1440p") == (kGrantUnlocked | kGrant1080p | kGrant1440p) &&
            !ParseTicketCapabilities("") && !ParseTicketCapabilities("2160p,2160p"),
            "token parser is exposed and strict");
      Check(TicketCapabilitiesText(0).empty() && TicketCapabilitiesText(kGrantAll) == "1080p,1440p,2160p,unlocked" &&
            TicketCapabilitiesText(kGrantUnlocked | kGrant1440p) == "1440p,unlocked", "canonical grant text");
    }
    std::cout << "Build lease policy: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
  } catch (const std::exception &error) {
    std::cerr << "In-memory lease fixture error: " << error.what() << '\n';
    return 2;
  }
}
