#include "mcla_launcher_game_win.h"
#include "mcla_launcher_profile_policy.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace mcla::launcher {
namespace {
constexpr uint64_t kFileLimit = uint64_t{2} * 1024 * 1024 * 1024;
constexpr uint64_t kTotalLimit = uint64_t{8} * 1024 * 1024 * 1024;
constexpr size_t kCountLimit = 64;
constexpr std::string_view kSourceIso = "AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB";
// Inventory metadata from the supported CE extraction, independently bound by
// README-AI's source ISO/default.xex identities. These are not game assets.
const std::array<VerifiedGameFile, 15> kSupported = {{
  {L"$SystemUpdate/su20076000_00000000", 7938048, "7375547707B162FA935A56485D299C7EDE1D3628E47CC10E6C5864E96B71C57A"},
  {L"$SystemUpdate/system.manifest", 2100, "CF2A36938F0F566E482391EEAC23BF35C1BFCBB1E361E2854BC4C440664A1C60"},
  {L"attract576_16x9.bik", 63140548, "03812C6EB376F62E37CA570207A767FC58C74B8DD32DFC191B1AB4759EE33924"},
  {L"attract576_4x3.bik", 63136172, "9FEA4CFE759245C18E2071AF787BF5D54C64B7710E5A658FB7915065539A6108"},
  {L"attract720.bik", 63168256, "8967304E343C26DF93B89C7E049F81F52355FCAC788CDA0BF6E3728E848690F1"},
  {L"default.xex", 9252864, "C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432"},
  {L"intro576_16x9.bik", 123833536, "BE55B718C804D6E1B9E1DB97A5F70385BD4FD3295979BF6694655EBC70C36239"},
  {L"intro576_4x3.bik", 123831648, "71E4B92F78E8F9E2C1C21A68A71B75287EA90224C1B12DBF7903E41CD8703AAB"},
  {L"intro720.bik", 123836692, "09435EA27E2117817F2E0E821E3F92C9036B76613B178EB812F65028A1168A79"},
  {L"nxeart", 1425408, "C70ACAB80EC383552C0B821BAEFF8BE7012115FD280A9AF01D0E711523656850"},
  {L"root_directory_padding.pad", 30720, "4C7EEA521D2218C5965FD3666C694A967A939E29A6928D528B6ED1101176B8AC"},
  {L"xarchive_audio.rpf", 1615757312, "5EEA63203E0D5C32C9EC59D68E46F7941171E32283DC56C344A4E31BF4E4726E"},
  {L"xarchive_audlo.rpf", 1463189504, "704B00676EB09E406B5D83ADD366CA6E72D909867E4F627FEE1BCC1C1402BB4C"},
  {L"xarchive_cache.rpf", 2130739200, "F46F9A39E797CC0A2F21730014E18E5762FDC0910595F440031E004CCE2DD420"},
  {L"xarchive_music.rpf", 780304384, "4D391ED71F132760BA4EC9CF5F26A58897FF7BC1BD78EE86ACDA3D84213EFF15"}
}};

struct Handle {
  HANDLE value = INVALID_HANDLE_VALUE;
  explicit Handle(HANDLE h) : value(h) {}
  Handle(Handle &&other) noexcept : value(std::exchange(other.value, INVALID_HANDLE_VALUE)) {}
  ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
  Handle(const Handle &) = delete;
  Handle &operator=(const Handle &) = delete;
};
struct Search {
  HANDLE value;
  ~Search() { if (value != INVALID_HANDLE_VALUE) FindClose(value); }
};
std::wstring Error(std::wstring_view message, DWORD code = GetLastError()) {
  return std::wstring(message) + L" (Win32 " + std::to_wstring(code) + L")";
}
std::wstring Join(std::wstring_view root, std::wstring_view relative) {
  std::wstring path(root);
  if (!relative.empty()) path += L"\\" + std::wstring(relative);
  std::replace(path.begin(), path.end(), L'/', L'\\');
  return path;
}
std::wstring Native(std::wstring_view root, std::wstring_view relative) { return L"\\\\?\\" + Join(root, relative); }
bool Equal(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() && CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
      static_cast<int>(b.size()), TRUE) == CSTR_EQUAL;
}
std::expected<std::string, std::wstring> Utf8(std::wstring_view text) {
  const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (!count) return std::unexpected(L"cannot encode expected game path");
  std::string result(count, '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), count, nullptr, nullptr) != count)
    return std::unexpected(L"cannot encode expected game path");
  return result;
}
bool HashShape(std::string_view text) {
  if (text.size() != 64) return false;
  for (const auto c : text) if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
  return true;
}
std::expected<std::vector<std::wstring>, std::wstring> ValidateExpected(std::span<const VerifiedGameFile> files) {
  if (files.empty() || files.size() > kCountLimit) return std::unexpected(L"expected game inventory count invalid");
  std::vector<std::wstring> directories{L""};
  uint64_t total = 0;
  for (size_t i = 0; i < files.size(); ++i) {
    const auto &file = files[i];
    if (file.relative_path.empty() || file.relative_path.size() > 256 || file.relative_path.find(L'\\') != std::wstring::npos ||
        !NormalizeRoot(u"C:/" + std::u16string(file.relative_path.begin(), file.relative_path.end())) ||
        !file.bytes || file.bytes > kFileLimit || !HashShape(file.sha256))
      return std::unexpected(L"expected game path/size/hash invalid");
    if (file.bytes > kTotalLimit - total) return std::unexpected(L"expected game payload exceeds 8 GiB");
    total += file.bytes;
    for (size_t j = 0; j < i; ++j) {
      if (Equal(files[j].relative_path, file.relative_path)) return std::unexpected(L"expected game paths alias/duplicate");
    }
    size_t begin = 0, depth = 0;
    for (;;) {
      const auto slash = file.relative_path.find(L'/', begin);
      if (slash == std::wstring::npos) break;
      if (++depth > 8) return std::unexpected(L"expected game path nesting exceeds bound");
      const auto parent = file.relative_path.substr(0, slash);
      if (std::none_of(directories.begin(), directories.end(), [&](const auto &dir) { return Equal(dir, parent); })) {
        if (directories.size() >= 128) return std::unexpected(L"expected game directory count exceeds bound");
        directories.push_back(parent);
      }
      begin = slash + 1;
    }
  }
  for (const auto &file : files) {
    for (const auto &dir : directories) {
      if (Equal(file.relative_path, dir)) return std::unexpected(L"expected path cannot be both file and directory");
    }
  }
  return directories;
}

struct Sha {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE handle = nullptr;
  std::vector<uint8_t> object;
  ~Sha() { if (handle) BCryptDestroyHash(handle); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
  bool Start() {
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    DWORD size = 0, result = 0;
    if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&size), sizeof(size), &result, 0) < 0 ||
        result != sizeof(size) || !size || size > 1024 * 1024) return false;
    object.resize(size);
    return BCryptCreateHash(algorithm, &handle, object.data(), size, nullptr, 0, 0) >= 0;
  }
  bool Append(std::span<const uint8_t> bytes) {
    return bytes.size() <= (std::numeric_limits<ULONG>::max)() &&
      BCryptHashData(handle, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0) >= 0;
  }
  std::expected<std::string, std::wstring> Finish() {
    std::array<uint8_t, 32> digest{};
    if (BCryptFinishHash(handle, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0)
      return std::unexpected(L"cannot finish game SHA256");
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (const auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return result;
  }
};
std::expected<uint64_t, std::wstring> Size(HANDLE file) {
  BY_HANDLE_FILE_INFORMATION info{};
  if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info))
    return std::unexpected(Error(L"cannot inspect game file"));
  if ((info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) || info.nNumberOfLinks != 1)
    return std::unexpected(L"game file must be regular, non-reparse, single-link");
  const uint64_t size = (static_cast<uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
  if (!size || size > kFileLimit) return std::unexpected(L"game file byte bound exceeded");
  return size;
}
std::expected<std::string, std::wstring> HashFile(HANDLE file, uint64_t bytes) {
  Sha hash;
  if (!hash.Start()) return std::unexpected(L"cannot initialize game SHA256");
  std::vector<uint8_t> buffer(1024 * 1024);
  uint64_t remaining = bytes;
  while (remaining) {
    const auto count = static_cast<DWORD>(std::min<uint64_t>(buffer.size(), remaining));
    DWORD read = 0;
    if (!::ReadFile(file, buffer.data(), count, &read, nullptr) || !read)
      return std::unexpected(Error(L"game hash read truncated"));
    if (!hash.Append({buffer.data(), read})) return std::unexpected(L"game SHA256 chunk failed");
    remaining -= read;
  }
  uint8_t byte = 0; DWORD read = 0;
  if (!::ReadFile(file, &byte, 1, &read, nullptr) || read) return std::unexpected(L"game file size changed while hashing");
  const auto after = Size(file);
  if (!after || *after != bytes) return std::unexpected(L"game file changed while hashing");
  return hash.Finish();
}
std::expected<void, std::wstring> Inventory(std::wstring_view root, std::span<const VerifiedGameFile> files,
                                          const std::vector<std::wstring> &directories) {
  std::vector<std::wstring> seen;
  for (const auto &dir : directories) {
    WIN32_FIND_DATAW data{};
    Search search{FindFirstFileW(Native(Join(root, dir), L"*").c_str(), &data)};
    if (search.value == INVALID_HANDLE_VALUE) return std::unexpected(Error(L"cannot enumerate game inventory"));
    do {
      const std::wstring_view name(data.cFileName);
      if (name == L"." || name == L"..") continue;
      const std::wstring relative = dir.empty() ? std::wstring(name) : dir + L"/" + std::wstring(name);
      if (seen.size() >= files.size() + directories.size() - 1 || (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return std::unexpected(L"game inventory has excessive or redirected entries");
      for (const auto &prior : seen) if (Equal(prior, relative)) return std::unexpected(L"game inventory has case-aliased entries");
      bool allowed = false;
      if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        for (const auto &expected : directories) allowed |= Equal(relative, expected);
      } else {
        for (const auto &expected : files) allowed |= Equal(relative, expected.relative_path);
      }
      if (!allowed) return std::unexpected(L"unexpected game inventory entry: " + relative);
      seen.push_back(relative);
    } while (FindNextFileW(search.value, &data));
    if (GetLastError() != ERROR_NO_MORE_FILES) return std::unexpected(Error(L"game inventory enumeration failed"));
  }
  if (seen.size() != files.size() + directories.size() - 1) return std::unexpected(L"game inventory has missing entries");
  return {};
}
} // namespace

struct VerifiedGame::State {
  std::vector<fs::DirectoryGuard> directories;
  std::vector<Handle> handles;
  std::vector<VerifiedGameFile> files;
  std::string fingerprint;
};
VerifiedGame::VerifiedGame(std::unique_ptr<State> state) : state_(std::move(state)) {}
VerifiedGame::VerifiedGame(VerifiedGame &&) noexcept = default;
VerifiedGame &VerifiedGame::operator=(VerifiedGame &&) noexcept = default;
VerifiedGame::~VerifiedGame() = default;
const std::wstring &VerifiedGame::path() const {
  static const std::wstring empty;
  return state_ ? state_->directories.front().path() : empty;
}
const std::vector<VerifiedGameFile> &VerifiedGame::files() const {
  static const std::vector<VerifiedGameFile> empty;
  return state_ ? state_->files : empty;
}
const std::string &VerifiedGame::fingerprint() const {
  static const std::string empty;
  return state_ ? state_->fingerprint : empty;
}

struct GameVerifier {
  static std::expected<VerifiedGame, std::wstring> Verify(const fs::DirectoryGuard &directory,
      std::span<const VerifiedGameFile> files, std::string_view source) {
    const auto expected_directories = ValidateExpected(files);
    if (!expected_directories) return std::unexpected(expected_directories.error());
    auto state = std::make_unique<VerifiedGame::State>();
    for (const auto &relative : *expected_directories) {
      auto held = fs::OpenDirectory(Join(directory.path(), relative));
      if (!held) return std::unexpected(held.error());
      state->directories.push_back(std::move(*held));
    }
    const auto &root = state->directories.front().path();
    const auto inventory = Inventory(root, files, *expected_directories);
    if (!inventory) return std::unexpected(inventory.error());
    std::string metadata = "MCLA-VERIFIED-GAME-V1\n" + std::string(source) + "\n";
    for (const auto &file : files) {
      const auto path = Utf8(file.relative_path);
      if (!path) return std::unexpected(path.error());
      metadata += file.sha256 + " " + std::to_string(file.bytes) + " " + *path + "\n";
      Handle held(CreateFileW(Native(root, file.relative_path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                             OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
      if (held.value == INVALID_HANDLE_VALUE) return std::unexpected(Error(L"cannot pin game file " + file.relative_path));
      const auto size = Size(held.value);
      if (!size || *size != file.bytes) return std::unexpected(L"game file size mismatch: " + file.relative_path);
      const auto hash = HashFile(held.value, *size);
      if (!hash) return std::unexpected(hash.error());
      if (*hash != file.sha256) return std::unexpected(L"game file SHA256 mismatch: " + file.relative_path);
      state->handles.push_back(std::move(held));
    }
    const auto final_inventory = Inventory(root, files, *expected_directories);
    if (!final_inventory) return std::unexpected(final_inventory.error());
    Sha fingerprint;
    if (!fingerprint.Start() || !fingerprint.Append({reinterpret_cast<const uint8_t *>(metadata.data()), metadata.size()}))
      return std::unexpected(L"cannot fingerprint verified game metadata");
    auto digest = fingerprint.Finish();
    if (!digest) return std::unexpected(digest.error());
    state->fingerprint = std::move(*digest);
    state->files.assign(files.begin(), files.end());
    return VerifiedGame(std::move(state));
  }
};

std::span<const VerifiedGameFile> SupportedGameFiles() { return kSupported; }
std::expected<VerifiedGame, std::wstring> VerifyGameRoot(const fs::DirectoryGuard &directory) {
  return GameVerifier::Verify(directory, kSupported, kSourceIso);
}
#ifdef MCLA_LAUNCHER_GAME_TESTING
std::expected<VerifiedGame, std::wstring> VerifyGameRootForTest(
    const fs::DirectoryGuard &directory, std::span<const VerifiedGameFile> expected) {
  return GameVerifier::Verify(directory, expected, "SYNTHETIC-TEST-ONLY");
}
#endif

} // namespace mcla::launcher
