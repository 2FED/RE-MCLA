#include "mcla_build_lease_runtime_win.h"
#include "mcla_build_lease_policy.h"

#ifndef MCLA_BUILD_LEASE_RUNTIME_TESTING
#error Build this isolated fixture with MCLA_BUILD_LEASE_RUNTIME_TESTING.
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>

// Only CheckLeaseForTest is called. All keys are ephemeral, fetches are injected
// lambdas, and every filesystem path belongs to a unique preserved TEMP tree.
namespace {
using namespace mcla::lease;
namespace fs = mcla::launcher::fs;
namespace path = std::filesystem;
using Response = std::expected<std::string, std::wstring>;
unsigned checks = 0, failures = 0;
constexpr uint64_t created = 1893456000;
constexpr uint64_t grace_end = created + kGraceSeconds;
constexpr uint64_t extension_end = grace_end + 3600;
const std::string hash_a(64, 'A'), hash_b(64, 'B');
const std::wstring cache_name = std::wstring(64, L'A') + L".lease";
void Check(bool value, const char *label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::array<uint8_t, 32> Digest(std::string_view bytes) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
    throw std::runtime_error("fixture SHA256 setup");
  std::array<uint8_t, 32> result{};
  const auto status = BCryptHash(algorithm, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char *>(bytes.data())),
      static_cast<ULONG>(bytes.size()), result.data(), static_cast<ULONG>(result.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (status < 0) throw std::runtime_error("fixture SHA256 hash");
  return result;
}
struct SigningKey {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_KEY_HANDLE key = nullptr;
  std::vector<uint8_t> public_blob;
  SigningKey() {
    try {
      if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_RSA_ALGORITHM, nullptr, 0) < 0 ||
          BCryptGenerateKeyPair(algorithm, &key, 2048, 0) < 0 || BCryptFinalizeKeyPair(key, 0) < 0)
        throw std::runtime_error("fixture RSA setup");
      ULONG size = 0;
      if (BCryptExportKey(key, nullptr, BCRYPT_RSAPUBLIC_BLOB, nullptr, 0, &size, 0) < 0)
        throw std::runtime_error("fixture public-key sizing");
      public_blob.resize(size);
      if (BCryptExportKey(key, nullptr, BCRYPT_RSAPUBLIC_BLOB, public_blob.data(), size, &size, 0) < 0)
        throw std::runtime_error("fixture public-key export");
    } catch (...) {
      if (key) BCryptDestroyKey(key);
      if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
      throw;
    }
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
    ULONG size = 0;
    if (BCryptSignHash(key, &padding, digest.data(), static_cast<ULONG>(digest.size()), signature.data(),
        static_cast<ULONG>(signature.size()), &size, BCRYPT_PAD_PKCS1) < 0 || size != signature.size())
      throw std::runtime_error("fixture RSA signing");
    std::string result(payload);
    result += "--SIGNATURE--\n";
    for (auto byte : signature) { result += "0123456789ABCDEF"[byte >> 4]; result += "0123456789ABCDEF"[byte & 15]; }
    return result + '\n';
  }
  std::string Ticket(uint64_t when = created) const {
    return Sign("MCLA-TEST-BUILD-V1\n" + hash_a + " " + std::to_string(when) + '\n');
  }
  std::string Lease(uint64_t expiry, uint64_t issued = created, std::string_view host = hash_a) const {
    return Sign("MCLA-TEST-LEASES-V1\n" + std::to_string(issued) + '\n' + std::string(host) + " " + std::to_string(expiry) + '\n');
  }
};
fs::DirectoryGuard NewCache(const path::path &root, const wchar_t *name) {
  const auto child = root / name;
  if (!CreateDirectoryW(child.c_str(), nullptr)) throw std::runtime_error("fixture cache directory creation");
  auto guard = fs::OpenDirectory(child.wstring());
  if (!guard) throw std::runtime_error("fixture cache guard admission");
  return std::move(*guard);
}
void Write(const fs::DirectoryGuard &guard, std::string_view bytes) {
  auto result = fs::AtomicWrite(guard, cache_name,
      {reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()});
  if (!result) throw std::runtime_error("fixture cache bytes setup");
}
std::string Read(const fs::DirectoryGuard &guard) {
  auto bytes = fs::ReadFile(guard, cache_name, kMaximumEnvelopeBytes);
  if (!bytes) throw std::runtime_error("fixture cache read");
  return {bytes->begin(), bytes->end()};
}
bool CacheExists(const fs::DirectoryGuard &guard) { return path::exists(path::path(guard.path()) / cache_name); }
std::set<path::path> Children(const fs::DirectoryGuard &guard) {
  std::set<path::path> result;
  for (const auto &entry : path::directory_iterator(guard.path())) result.insert(entry.path().filename());
  return result;
}
uint64_t CacheIdentity(const fs::DirectoryGuard &guard) {
  const auto file_path = path::path(guard.path()) / cache_name;
  HANDLE file = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
  if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("fixture identity open");
  BY_HANDLE_FILE_INFORMATION info{};
  const bool result = GetFileInformationByHandle(file, &info) != FALSE;
  CloseHandle(file);
  if (!result) throw std::runtime_error("fixture identity query");
  return (uint64_t(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
}
} // namespace

int main() {
  try {
    std::array<wchar_t, 32768> temp{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(temp.size()), temp.data());
    if (!length || length >= temp.size()) return 2;
    std::array<uint8_t, 16> random{};
    if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
    std::wstring suffix;
    for (auto byte : random) { suffix += L"0123456789abcdef"[byte >> 4]; suffix += L"0123456789abcdef"[byte & 15]; }
    const auto root = path::path(temp.data()) / (L"mcla-lease-runtime-" + suffix);
    if (!CreateDirectoryW(root.c_str(), nullptr)) return 2;
    std::wcout << L"Synthetic cache fixture preserved: " << root.wstring() << L'\n';
    SigningKey signer, untrusted;
    const auto ticket = signer.Ticket(), proof = signer.Lease(extension_end);
    unsigned fetches = 0;
    const auto offline = [&]() -> Response { ++fetches; return std::unexpected(L"fixture offline, no network attempted"); };
    const auto online = [&]() -> Response { ++fetches; return proof; };
    auto fresh = NewCache(root, L"fresh");
    auto result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, created, &fresh, offline);
    Check(result && result->expires_utc == grace_end && !result->extended, "fresh offline ticket admitted for exact fourteen-day grace");
    Check(fetches == 1 && !CacheExists(fresh), "offline attempt creates no lease cache file");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end - 1, &fresh, offline);
    Check(result && result->expires_utc == grace_end, "last grace second admitted offline");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &fresh, offline);
    Check(!result && result.error().starts_with(L"BUILD EXPIRED"), "exact grace boundary fails without online or cached proof");
    Check(!CacheExists(fresh), "expiration does not invent a cache allowance");

    auto good = NewCache(root, L"good");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &good, online);
    Check(result && result->extended && result->expires_utc == extension_end, "valid online exact-host extension admitted");
    Check(CacheExists(good) && Read(good) == proof, "cache stores exact signed envelope bytes");
    const auto identity = CacheIdentity(good);
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end + 1, &good, offline);
    Check(result && result->extended && result->expires_utc == extension_end, "offline replay of verified cache permits extension");
    Check(Read(good) == proof && CacheIdentity(good) == identity, "offline cache read does not rewrite proof");
    const auto preserve_good = [&](const std::string &response, const char *label) {
      const auto admitted = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &good,
          [&]() -> Response { ++fetches; return response; });
      Check(admitted && admitted->expires_utc == extension_end && Read(good) == proof &&
          CacheIdentity(good) == identity, label);
    };
    preserve_good(signer.Lease(grace_end + 1800), "shorter signed response cannot reduce or overwrite good cache");
    preserve_good(signer.Sign("MCLA-TEST-LEASES-V1\n" + std::to_string(grace_end) + '\n'), "omitted grant preserves still-valid cached extension");
    preserve_good(proof, "identical replay does not rewrite cached envelope");
    preserve_good(signer.Lease(grace_end + 1800, created - 1), "older-issued shorter replay does not roll back cache");
    preserve_good(untrusted.Lease(extension_end + 10000), "untrusted signing key cannot replace good cached proof");
    preserve_good(signer.Lease(extension_end + 10000, grace_end + 301), "future-issued response cannot replace good cached proof");
    preserve_good(signer.Lease(extension_end + 10000, created, hash_b), "different-host grant cannot overwrite this host cache");
    preserve_good("not a signed envelope", "malformed response cannot replace good cached proof");

    const auto unchanged = Children(good);
    const auto fetches_before = fetches;
    result = CheckLeaseForTest(ticket, hash_b, signer.public_blob, created, &good, online);
    Check(!result && fetches == fetches_before && Children(good) == unchanged && Read(good) == proof,
        "wrong host rejected before fetch or cache mutation");
    result = CheckLeaseForTest(signer.Ticket(created + 1), hash_a, signer.public_blob, created, &good, online);
    Check(!result && fetches == fetches_before && Children(good) == unchanged && Read(good) == proof,
        "future-created ticket rejected before fetch or cache mutation");
    result = CheckLeaseForTest("invalid ticket", hash_a, signer.public_blob, created, &good, online);
    Check(!result && fetches == fetches_before && Children(good) == unchanged && Read(good) == proof,
        "malformed ticket rejected before fetch or cache mutation");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, 0, &good, online);
    Check(!result && fetches == fetches_before && Read(good) == proof, "invalid clock rejected before fetch or cache mutation");

    auto corrupt = NewCache(root, L"corrupt");
    Write(corrupt, "unsigned corrupt fixture");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &corrupt, offline);
    Check(!result && result.error().starts_with(L"BUILD EXPIRED") && Read(corrupt) == "unsigned corrupt fixture",
        "corrupt cache has no authority and is not deleted offline");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, created, &corrupt, offline);
    Check(result && !result->extended && result->expires_utc == grace_end && Read(corrupt) == "unsigned corrupt fixture",
        "corrupt cache does not deny valid built-in grace");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &corrupt, online);
    Check(result && result->expires_utc == extension_end && Read(corrupt) == proof, "authenticated online proof repairs nonauthoritative corrupt cache");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, nullptr, online);
    Check(result && result->extended && result->expires_utc == extension_end, "valid online extension works when cache is unavailable");
    Check(result && result->note.find(L"unavailable") != std::wstring::npos, "cache-unavailable allowance reports lack of offline persistence");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, nullptr, offline);
    Check(!result && result.error().starts_with(L"BUILD EXPIRED"), "unavailable cache cannot fabricate offline extension");

    auto expired = NewCache(root, L"expired");
    const auto expired_proof = signer.Lease(grace_end + 10);
    Write(expired, expired_proof);
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end + 10, &expired, offline);
    Check(!result && result.error().starts_with(L"BUILD EXPIRED") && Read(expired) == expired_proof,
        "expired signed cache fails closed at its exact boundary and remains preserved");

    auto busy = NewCache(root, L"busy");
    Write(busy, proof);
    const auto busy_identity = CacheIdentity(busy);
    {
      auto lock = fs::AcquireWriterLock(busy);
      Check(lock.has_value(), "hold fixture cache writer lock");
      if (!lock) return 2;
      const auto longer = signer.Lease(extension_end + 3600);
      result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &busy, [&]() -> Response { return longer; });
      Check(result && result->expires_utc == extension_end + 3600, "busy cache does not prevent valid online admission");
      Check(Read(busy) == proof && CacheIdentity(busy) == busy_identity, "busy writer lock prevents cache overwrite");
      result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &busy, offline);
      Check(!result && Read(busy) == proof, "busy cache is not consumed without lock and does not grant offline access");
    }
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &busy, offline);
    Check(result && result->expires_utc == extension_end, "original cached proof usable once writer lock released");
    const auto before_preferred_cache = fetches;
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &good, offline, true);
    Check(result && result->expires_utc == extension_end && fetches == before_preferred_cache && Read(good) == proof,
        "prefer-cached validates usable signed proof without calling network fetch");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, &fresh, offline, true);
    Check(!result && fetches == before_preferred_cache + 1 && !CacheExists(fresh),
        "prefer-cached still fetches when grace expired and no cached proof exists");
    const auto short_proof = signer.Lease(grace_end + 5);
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, nullptr,
        [&]() -> Response { return short_proof; }, false, [] { return grace_end + 10; });
    Check(!result && result.error().starts_with(L"BUILD EXPIRED"),
        "admission rechecks time after fetch and rejects proof that expired meanwhile");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end, nullptr,
        online, false, [] { return grace_end - 1; });
    Check(!result, "clock rollback during fetch rejects admission despite otherwise valid proof");
    auto read_only = NewCache(root, L"read-only");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end,
        &read_only, online, false, {}, true);
    Check(result && result->expires_utc == extension_end, "read-only online signed extension admitted");
    Check(Children(read_only).empty(), "read-only online check creates neither lease nor lock file");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end,
        &read_only, offline, false, {}, true);
    Check(!result && Children(read_only).empty(), "read-only expired empty cache remains unchanged");
    Write(read_only, proof); // Fixture setup only, not admission.
    const auto ro_identity = CacheIdentity(read_only);
    const auto ro_children = Children(read_only);
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end,
        &read_only, offline, false, {}, true);
    Check(result && result->expires_utc == extension_end, "read-only consumes authenticated existing cache without creating lock");
    const auto longer_ro = signer.Lease(extension_end + 7200);
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end,
        &read_only, [&]() -> Response { return longer_ro; }, false, {}, true);
    Check(result && result->expires_utc == extension_end + 7200, "read-only newer online extension can authorize without persistence");
    Check(Read(read_only) == proof && CacheIdentity(read_only) == ro_identity &&
        Children(read_only) == ro_children, "read-only does not replace cached bytes identity or inventory");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, extension_end,
        &read_only, offline, false, {}, true);
    Check(!result && result.error().starts_with(L"BUILD EXPIRED"), "read-only proof expires at exact boundary");
    result = CheckLeaseForTest(ticket, hash_b, signer.public_blob, grace_end,
        &read_only, online, false, {}, true);
    Check(!result && Read(read_only) == proof && Children(read_only) == ro_children,
          "read-only rejects mismatched host and preserves cache");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, extension_end,
        &read_only, [&]() -> Response { return untrusted.Lease(extension_end + 1000); }, false, {}, true);
    Check(!result && Read(read_only) == proof && Children(read_only) == ro_children,
          "read-only untrusted online signer cannot extend expired proof");
    result = CheckLeaseForTest(ticket, hash_a, signer.public_blob, grace_end,
        &read_only, online, false, [] { return extension_end; }, true);
    Check(!result && Read(read_only) == proof && Children(read_only) == ro_children,
          "read-only rechecks signed expiry after fetch without mutation");
    std::cout << "Build lease runtime: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
  } catch (const std::exception &error) {
    std::cerr << "Synthetic lease runtime fixture error: " << error.what() << '\n';
    return 2;
  }
}
