// Private, bounded RenderDoc 1.45 replay instrumentation, not a graphics oracle.
// Build only against the pinned v1.45 replay headers. No Qt or eager DLL import.
// The parent must impose an owned-process/job deadline and memory limit.
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#ifndef RENDERDOC_PLATFORM_WIN32
#define RENDERDOC_PLATFORM_WIN32
#endif
#include <apidefs.h>
#undef RENDERDOC_API
#define RENDERDOC_API
#include <renderdoc_replay.h>

REPLAY_PROGRAM_MARKER()

namespace {
pRENDERDOC_AllocArrayMem alloc_array = nullptr;
pRENDERDOC_FreeArrayMem free_array = nullptr;
}
extern "C" void* RENDERDOC_CC RENDERDOC_AllocArrayMem(uint64_t bytes) {
  if (!alloc_array) std::terminate();
  return alloc_array(bytes);
}
extern "C" void RENDERDOC_CC RENDERDOC_FreeArrayMem(void* p) {
  if (!p) return;
  if (!free_array) std::terminate();
  free_array(p);
}

namespace {
namespace fs = std::filesystem;
constexpr wchar_t kDll[] = L"C:\\Program Files\\RenderDoc\\renderdoc.dll";
constexpr char kDllSha[] = "313F450BFA1F7F7D5F50CC7EE80D9F99B68D3FE873E25EB865FF0433E359F7B3";
constexpr char kCommit[] = "2fc0bc04cb95499635f63986a55bc6f67849dd9f";
constexpr uint64_t kOutputBudget = 256ull * 1024 * 1024;
constexpr size_t kRows = 200000, kTextures = 8192, kTextBudget = 16 * 1024 * 1024;
void Require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
bool EqualPath(std::wstring_view a, std::wstring_view b) {
  return a.size() == b.size() && CompareStringOrdinal(a.data(), int(a.size()),
      b.data(), int(b.size()), TRUE) == CSTR_EQUAL;
}
bool PlainPath(const fs::path& p) {
  auto s = p.wstring();
  if (s.size() < 3 || s.size() > 30000 || s[1] != L':' || s[2] != L'\\' ||
      !p.is_absolute() || s.find(L'\0') != s.npos ||
      !((s[0] >= L'A' && s[0] <= L'Z') || (s[0] >= L'a' && s[0] <= L'z'))) return false;
  for (const auto& part : p.relative_path()) {
    auto c = part.wstring();
    if (c.empty() || c == L"." || c == L".." || c.back() == L' ' || c.back() == L'.' ||
        c.find_first_of(L":*?\"<>|/") != c.npos) return false;
  }
  return true;
}
std::wstring FinalPath(HANDLE h) {
  std::wstring s(32768, L'\0');
  DWORD n = GetFinalPathNameByHandleW(h, s.data(), DWORD(s.size()), FILE_NAME_NORMALIZED);
  Require(n && n < s.size(), "final path failed"); s.resize(n);
  if (s.compare(0, 4, L"\\\\?\\") == 0) s.erase(0, 4);
  return s;
}
struct Handle {
  HANDLE h = INVALID_HANDLE_VALUE;
  explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : h(value) {}
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  Handle(Handle&& v) noexcept : h(v.h) { v.h = INVALID_HANDLE_VALUE; }
  ~Handle() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
std::string Utf8(std::wstring_view s) {
  int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), int(s.size()), nullptr, 0, nullptr, nullptr);
  Require(n > 0, "UTF8 length failed"); std::string out(n, '\0');
  Require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), int(s.size()), out.data(), n, nullptr, nullptr) == n, "UTF8 failed");
  return out;
}
std::string Hex(const unsigned char* p, size_t n) {
  std::string s; s.reserve(n * 2);
  for (size_t i = 0; i < n; ++i) { s += "0123456789ABCDEF"[p[i] >> 4]; s += "0123456789ABCDEF"[p[i] & 15]; }
  return s;
}
std::string Hash(HANDLE h, uint64_t cap) {
  LARGE_INTEGER zero{}; Require(SetFilePointerEx(h, zero, nullptr, FILE_BEGIN), "hash seek failed");
  BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
  Require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0, "hash provider failed");
  bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
  std::array<unsigned char, 65536> data{}; uint64_t total = 0;
  while (ok) {
    DWORD got = 0; if (!ReadFile(h, data.data(), DWORD(data.size()), &got, nullptr)) { ok = false; break; }
    if (!got) break;
    total += got; if (total > cap || BCryptHashData(hash, data.data(), got, 0) < 0) ok = false;
  }
  std::array<unsigned char, 32> digest{};
  if (ok) ok = BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0) >= 0;
  if (hash) BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(algorithm, 0);
  Require(ok, "hash read/budget failed"); return Hex(digest.data(), digest.size());
}
uint64_t Number(std::wstring_view s, uint64_t max) {
  Require(!s.empty() && s.size() <= 20 && (s.size() == 1 || s.front() != L'0'), "noncanonical integer");
  uint64_t v = 0;
  for (wchar_t c : s) { Require(c >= L'0' && c <= L'9', "invalid integer");
    auto d = uint64_t(c - L'0'); Require(d <= max && v <= (max - d) / 10, "integer overflow"); v = v * 10 + d; }
  return v;
}
std::vector<uint64_t> PreviewIds(std::wstring_view csv) {
  Require(!csv.empty() && csv.size() <= 335, "preview list size rejected");
  std::vector<uint64_t> ids;
  size_t start = 0;
  for (;;) {
    const auto comma = csv.find(L',', start);
    const auto part = csv.substr(start, comma == csv.npos ? csv.size() - start : comma - start);
    const auto id = Number(part, UINT64_MAX);
    Require(id != 0 && ids.size() < 16 && std::find(ids.begin(), ids.end(), id) == ids.end(), "preview IDs must be distinct positive integers, maximum 16");
    ids.push_back(id);
    if (comma == csv.npos) return ids;
    start = comma + 1;
  }
}
bool PreviewBudget(uint64_t bytes, uint64_t already, uint64_t limit = kOutputBudget) {
  return already <= limit && bytes <= limit - already;
}
CompType PreviewCast(const TextureDescription& t) {
  Require(t.dimension == 2 && t.width > 0 && t.height > 0 && t.width <= 16384 && t.height <= 16384 &&
      t.depth == 1 && t.arraysize == 1 && t.msSamp == 1 && t.mips > 0 &&
      t.byteSize > 0 && t.byteSize <= kOutputBudget / 2, "preview requires bounded single-slice non-MSAA 2D color texture");
  Require(!(uint32_t(t.creationFlags) & uint32_t(TextureCategory::DepthTarget)), "depth-target preview rejected");
  const auto& f = t.format;
  const bool regular = f.type == ResourceFormatType::Regular && f.compByteWidth == 1 && f.compCount >= 1 && f.compCount <= 4;
  const bool compressed = f.type == ResourceFormatType::BC1 || f.type == ResourceFormatType::BC2 ||
      f.type == ResourceFormatType::BC3 || f.type == ResourceFormatType::BC4 ||
      f.type == ResourceFormatType::BC5 || f.type == ResourceFormatType::BC7;
  Require((regular || compressed) && (f.compType == CompType::Typeless || f.compType == CompType::UNorm || f.compType == CompType::UNormSRGB),
      "preview color format/component interpretation not allowlisted");
  const bool compressed_components =
      (f.type == ResourceFormatType::BC1 && (f.compCount == 1 || f.compCount == 4)) ||
      ((f.type == ResourceFormatType::BC2 || f.type == ResourceFormatType::BC3 || f.type == ResourceFormatType::BC7) && f.compCount == 4) ||
      (f.type == ResourceFormatType::BC4 && f.compCount == 1) ||
      (f.type == ResourceFormatType::BC5 && f.compCount == 2);
  Require(!compressed || compressed_components, "compressed preview component metadata inconsistent");
  // Viewing convention only: interpret typeless color bytes as UNorm, never
  // claim this is the game's view format. Typed UNorm/SRGB keep native handling.
  return f.compType == CompType::Typeless ? CompType::UNorm : CompType::Typeless;
}
bool PreviewAlpha(const ResourceFormat& f) {
  // SaveTexture validates channelExtract against the original component count
  // before compressed-format conversion. Never infer a fourth channel here.
  return f.compCount == 4;
}
void AdmitColorTarget(const TextureDescription& t, uint32_t slice) {
  Require(slice == 0 && t.dimension == 2 && t.depth == 1 && t.arraysize == 1 && t.mips == 1 && t.msSamp == 4 &&
      t.width > 0 && t.height > 0 && t.width <= 8192 && t.height <= 8192 &&
      t.byteSize > 0 && t.byteSize <= kOutputBudget / 2 &&
      bool(t.creationFlags & TextureCategory::ColorTarget) && !bool(t.creationFlags & TextureCategory::DepthTarget),
      "target preview requires bounded single-slice mip0 4xMSAA color target");
  Require(t.format.type == ResourceFormatType::Regular && t.format.compCount == 4 &&
      t.format.compByteWidth == 2 && t.format.compType == CompType::Float && !t.format.BGRAOrder() && !t.format.SRGBCorrected(),
      "target preview supports only native RGBA16 Float");
}
bool SelectedDraw(uint32_t actual, ActionFlags flags, uint32_t wanted) {
  return actual == wanted && wanted != 0 && bool(flags & ActionFlags::Drawcall);
}
bool SelectedDispatch(uint32_t actual, ActionFlags flags, uint32_t wanted) {
  return actual == wanted && wanted != 0 && bool(flags & ActionFlags::Dispatch) && !bool(flags & ActionFlags::Drawcall);
}
bool CopyLinkMode(int argc, std::wstring_view mode) { return argc == 6 && mode == L"--copy-link"; }
bool FetchBiasMode(int argc, std::wstring_view mode) { return argc == 5 && mode == L"--fetch-bias"; }
int32_t FetchBiasUnits(uint32_t z) {
  const uint32_t bits = (z >> 12) & 1023;
  return bits < 512 ? int32_t(bits) : int32_t(bits) - 1024;
}
bool FetchBiasBounds(uint64_t offset, uint64_t descriptor_bytes, uint64_t buffer_bytes) {
  // Validate the entire reflected 48 uint4 block, but read only vector 5.
  return descriptor_bytes >= 768 && offset <= buffer_bytes &&
      descriptor_bytes <= buffer_bytes - offset && 768 <= buffer_bytes - offset;
}
bool SelectedCopy(uint32_t actual, ActionFlags flags, uint32_t wanted, uint64_t source, uint64_t destination, uint64_t expected) {
  return actual == wanted && wanted && source && destination && destination == expected &&
      bool(flags & ActionFlags::Copy) && !bool(flags & (ActionFlags::Drawcall | ActionFlags::Dispatch));
}
bool CopyUsageBudget(size_t total, size_t eligible) { return total <= 4096 && eligible <= total; }
bool ComputeBindingStage(uint64_t shader, ShaderStage stage) { return shader != 0 && stage == ShaderStage::Compute; }
bool BindingResourceType(ShaderStage stage, DescriptorType type) {
  return IsReadOnlyDescriptor(type) || (stage == ShaderStage::Compute && IsReadWriteDescriptor(type));
}
bool HistoryPoint(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t sample) {
  return width && height && x < width && y < height && sample == 0;
}
bool HistoryCandidate(ActionFlags flags, const PixelModification& p) {
  return bool(flags & ActionFlags::Drawcall) && p.Passed() && !p.unboundPS && !p.directShaderWrite && !p.predicationSkipped;
}
bool HistoryUsage(uint32_t usage_event, uint32_t requested_event) {
  return usage_event > 0 && usage_event <= requested_event;
}
bool HistoryBudget(size_t usages, size_t rows) { return usages <= 4096 && rows <= 8192; }
bool DebugBudget(size_t calls, size_t states, size_t changes) {
  return calls <= 64 && states <= 16384 && changes <= 131072;
}
bool DebugSelection(uint32_t event, uint64_t resource, uint32_t x, uint32_t y, uint32_t sample, uint32_t primitive) {
  return event > 0 && resource > 0 && x < 8192 && y < 8192 && sample == 0 && primitive != UINT32_MAX;
}
bool DebugStep(uint32_t actual, size_t expected) { return expected < 16384 && actual == expected; }
bool DebugTypelessRegister(std::string_view name) {
  if (name.size() < 2 || name.size() > 5 || (name[0] != 'r' && name[0] != 'o')) return false;
  const auto digits = name.substr(1);
  if (digits.size() > 1 && digits[0] == '0') return false;
  unsigned index = 0;
  for (const auto c : digits) { if (c < '0' || c > '9') return false; index = index * 10 + unsigned(c - '0'); }
  return index <= (name[0] == 'r' ? 4095u : 31u);
}
bool DebugAbsentVariable(std::string_view name, VarType type, unsigned rows, unsigned columns, size_t members) {
  return type == VarType::Unknown && name.empty() && rows == 0 && columns == 0 && members == 0;
}
bool DebugVariableShape(std::string_view name, VarType type, unsigned rows, unsigned columns, size_t members) {
  if (members) return false;
  if (type == VarType::Unknown)
    return DebugAbsentVariable(name, type, rows, columns, members) || (rows == 1 && columns == 4 && DebugTypelessRegister(name));
  if (type != VarType::Float && type != VarType::UInt && type != VarType::SInt) return false;
  if (rows != 1 || columns < 1 || columns > 4 || name.empty() || name.size() > 64) return false;
  for (const unsigned char c : name) if (c < 32 || c > 126 || c == '\\') return false;
  return true;
}
bool DebugResourceRef(VarType type, uint32_t tag, DescriptorCategory category, uint32_t index,
    size_t reflected_count, uint32_t element, uint32_t array_size) {
  return tag == 0 && ((type == VarType::ReadOnlyResource && category == DescriptorCategory::ReadOnlyResource) ||
      (type == VarType::ReadWriteResource && category == DescriptorCategory::ReadWriteResource)) &&
      index < reflected_count && array_size > 0 && element < array_size;
}
std::string DebugHex(uint32_t bits) {
  std::string result(8, '0');
  for (unsigned i = 0; i < 8; ++i) result[7-i] = "0123456789ABCDEF"[(bits >> (i*4)) & 15];
  return result;
}
std::string DebugFloat(uint32_t bits) {
  float value; std::memcpy(&value, &bits, 4);
  if (std::isnan(value)) return "nan";
  if (std::isinf(value)) return std::signbit(value) ? "-inf" : "+inf";
  std::ostringstream stream; stream.imbue(std::locale::classic());
  stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
  return stream.str();
}
bool CandidateReadBounds(uint64_t offset, uint64_t bytes, uint64_t buffer_size) {
  return bytes > 0 && bytes <= 1024 && bytes % 4 == 0 && offset <= buffer_size && bytes <= buffer_size - offset;
}
uint64_t LogTailOffset(uint64_t size, uint64_t cap = 1024 * 1024) { return size > cap ? size - cap : 0; }
bool CandidateDescriptorOffset(uint32_t table, uint32_t range_offset, uint32_t range_register,
    uint32_t range_count, uint32_t shader_register, uint32_t array_size, uint32_t candidate,
    uint32_t first, uint32_t stride, uint32_t count, uint32_t& output) {
  if (!stride || !count || !array_size || candidate >= array_size || shader_register < range_register) return false;
  const uint64_t relative = uint64_t(shader_register) - range_register + candidate;
  if (relative >= range_count) return false;
  const uint64_t base = uint64_t(table) + range_offset;
  if (base > UINT32_MAX || base < first || (base - first) % stride) return false;
  const uint64_t ordinal = (base - first) / stride + relative;
  if (ordinal >= count || ordinal > (UINT32_MAX - uint64_t(first)) / stride) return false;
  output = uint32_t(first + ordinal * stride); return true;
}
uint64_t Id(ResourceId id) {
  static_assert(sizeof(ResourceId) == sizeof(uint64_t) && std::is_trivially_copyable_v<ResourceId>);
  // Pinned ABI representation only; never construct/forge an input ResourceId.
  uint64_t v = 0; std::memcpy(&v, &id, sizeof(v)); return v;
}
std::string Escape(std::string_view v) {
  Require(v.size() <= 16384, "label budget exceeded"); std::string out;
  for (unsigned char c : v) {
    if (c == '\\') out += "\\\\";
    else if (c < 32 || c == 127) { out += "\\x"; out += Hex(&c, 1); }
    else out += char(c);
  }
  return out;
}
struct Text {
  std::string data; size_t rows = 0;
  void Add(const std::string& line) {
    Require(++rows <= kRows && line.size() < kTextBudget && data.size() <= kTextBudget - line.size() - 1, "TSV budget exceeded");
    data += line; data += '\n';
  }
};
template<typename... T> std::string Row(const T&... args) {
  std::ostringstream s; bool first = true;
  ((s << (std::exchange(first, false) ? "" : "\t") << args), ...); return s.str();
}
std::string DebugVariableFailure(uint32_t step, uint32_t next, size_t change, std::string_view phase,
    unsigned type, unsigned rows, unsigned columns, size_t members, std::string_view name) {
  std::ostringstream text; text.imbue(std::locale::classic());
  text << "debug variable unsupported: step=" << step << " next=" << next << " change=" << change
       << " phase=" << Escape(phase.substr(0, 8)) << " type=" << type << " rows=" << rows
       << " columns=" << columns << " members=" << members << " name_length=" << name.size()
       << " name_prefix=" << Escape(name.substr(0, 64)) << " name_truncated=" << (name.size() > 64);
  auto result = text.str();
  // Metadata only, no rejected value bytes; remains inside Failure's 512-byte cap.
  if (result.size() > 480) result.resize(480);
  return result;
}
enum class DebugFailureCode { Validation, ZeroStates, CallBudget };
const char* DebugFailureName(DebugFailureCode code) {
  switch (code) {
    case DebugFailureCode::ZeroStates: return "zero_states";
    case DebugFailureCode::CallBudget: return "call_budget";
    default: return "validation_or_output_failure";
  }
}
std::string DebugFailureProgress(DebugFailureCode code, size_t calls, size_t states, size_t changes,
    size_t input_count, uint32_t last_next) {
  return Row(DebugFailureName(code), calls, states, changes, input_count,
      states ? std::to_string(states-1) : "none", states ? std::to_string(last_next) : "none",
      "validated_state_headers_only_incomplete_not_success");
}
std::string TargetSelection(uint32_t event, uint64_t resource, const TextureDescription& t, uint32_t slice) {
  return "event\tresource\tmip\tslice\tsamples\tresolve\twidth\theight\tarraysize\tformat_type\tcomponents\tcomponent_bytes\tcomponent_type\ttype_cast\tblack_point\twhite_point\tchannel\talpha\tfile\n" +
      Row(event, resource, 0, slice, t.msSamp, "average", t.width, t.height, t.arraysize,
          unsigned(t.format.type), unsigned(t.format.compCount), unsigned(t.format.compByteWidth), unsigned(t.format.compType),
          "native", 0, 1, "rgba", "preserve", "target.png") + "\n";
}
constexpr const char* kDebugVariableHeader =
    "step\tnext_instruction\tchange\tphase\tpresent\tname\ttype\trows\tcolumns\tflags\tcomponent\traw32\tfloat_bits";
void DebugVariableRows(Text& rows, const ShaderVariable& variable, uint32_t step, uint32_t next,
    size_t change, const char* phase) {
  const std::string_view name(variable.name.c_str(), variable.name.size());
  if (!DebugVariableShape(name, variable.type, variable.rows, variable.columns, variable.members.size()))
    throw std::runtime_error(DebugVariableFailure(step, next, change, phase, unsigned(variable.type),
        variable.rows, variable.columns, variable.members.size(), name));
  if (DebugAbsentVariable(name, variable.type, variable.rows, variable.columns, variable.members.size())) {
    Require(variable.flags == ShaderVariableFlags::NoFlags, "uninitialised debug variable flags rejected");
    rows.Add(Row(step, next, change, phase, 0, "none", unsigned(variable.type), 0, 0, 0, "none", "none", "none")); return;
  }
  for (unsigned component = 0; component < variable.columns; ++component) {
    const uint32_t bits = variable.value.u32v[component];
    rows.Add(Row(step, next, change, phase, 1, std::string(name), unsigned(variable.type), unsigned(variable.rows),
        unsigned(variable.columns), uint32_t(variable.flags), component, DebugHex(bits), DebugFloat(bits)));
  }
}
void DebugResourceRow(Text& rows, const ShaderVariable& variable, const ShaderReflection& reflection,
    uint32_t step, uint32_t next, size_t change, const char* phase) {
  const std::string_view name(variable.name.c_str(), variable.name.size());
  if (variable.type == VarType::Unknown) {
    Require(DebugAbsentVariable(name, variable.type, variable.rows, variable.columns, variable.members.size()) &&
        variable.flags == ShaderVariableFlags::NoFlags, "empty debug resource reference rejected");
    rows.Add(Row(step, next, change, phase, 0, "none", 255, 0, 0, 0, "none", "none", "none", "none")); return;
  }
  Require(variable.rows == 1 && variable.columns == 1 && variable.members.empty() &&
      DebugVariableShape(name, VarType::UInt, 1, 1, 0) && variable.value.u32v[3] == 0,
      "debug resource reference shape/direct access rejected");
  const auto binding = variable.GetBindIndex();
  const auto& resources = variable.type == VarType::ReadOnlyResource ? reflection.readOnlyResources : reflection.readWriteResources;
  Require(resources.size() <= 128 && binding.index < resources.size(), "debug resource reflection index rejected");
  const auto array_size = resources[binding.index].bindArraySize;
  Require(DebugResourceRef(variable.type, variable.value.u32v[3], binding.category, binding.index, resources.size(), binding.arrayElement, array_size),
      "debug binding reference category/array rejected");
  rows.Add(Row(step, next, change, phase, 1, std::string(name), unsigned(variable.type), 1, 1, uint32_t(variable.flags),
      unsigned(binding.category), binding.index, binding.arrayElement, array_size));
}
struct State {
  std::vector<Handle> pins;
  std::array<SOCKET, 8> sockets;
  HMODULE module = nullptr;
  fs::path output;
  std::string nonce, input_sha;
  uint64_t output_bytes = 0;
  std::vector<std::string> output_names;
  using GetIdent = void(__cdecl*)(uint32_t*);
  GetIdent ident = nullptr;
  State() { sockets.fill(INVALID_SOCKET); }
  // Intentionally process lifetime: DLL, reservations and path pins never released early.
  void PinDirs(const fs::path& p) {
    Require(PlainPath(p) && GetDriveTypeW(p.root_path().c_str()) == DRIVE_FIXED, "directory path rejected");
    auto pin = [&](const fs::path& q) {
      Handle h(CreateFileW(q.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
          nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
      BY_HANDLE_FILE_INFORMATION i{};
      Require(h.h != INVALID_HANDLE_VALUE && GetFileInformationByHandle(h.h, &i) &&
          (i.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(i.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
          EqualPath(FinalPath(h.h), q.wstring()), "directory pin rejected"); pins.push_back(std::move(h));
    };
    auto q = p.root_path(); pin(q);
    for (const auto& part : p.relative_path()) { q /= part; pin(q); }
  }
  Handle PinFile(const fs::path& p, uint64_t limit) {
    Require(PlainPath(p), "file path rejected"); PinDirs(p.parent_path());
    Handle h(CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION i{}; LARGE_INTEGER n{};
    Require(h.h != INVALID_HANDLE_VALUE && GetFileInformationByHandle(h.h, &i) &&
        !(i.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
        i.nNumberOfLinks == 1 && EqualPath(FinalPath(h.h), p.wstring()) && GetFileSizeEx(h.h, &n) &&
        n.QuadPart >= 0 && uint64_t(n.QuadPart) <= limit, "file pin rejected");
    return h;
  }
  void Reserve() {
    WSADATA w{}; Require(WSAStartup(MAKEWORD(2, 2), &w) == 0, "Winsock failed");
    for (size_t i = 0; i < sockets.size(); ++i) {
      sockets[i] = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT);
      Require(sockets[i] != INVALID_SOCKET, "reservation socket failed"); BOOL yes = TRUE;
      Require(setsockopt(sockets[i], SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&yes), sizeof(yes)) == 0, "exclusive socket failed");
      sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(u_short(38920 + i));
      Require(bind(sockets[i], reinterpret_cast<const sockaddr*>(&a), sizeof(a)) == 0, "wildcard reservation failed");
      // Never listen or connect. Keep all eight reservations until process exit.
    }
  }
  uint32_t Ident() { uint32_t v = UINT32_MAX; if (ident) ident(&v); return v; }
  template<typename T> T Bind(const char* name) {
    auto p = GetProcAddress(module, name); Require(p != nullptr, "required DLL export absent"); return reinterpret_cast<T>(p);
  }
  void Fresh(const fs::path& p) {
    Require(PlainPath(p) && p.has_filename() && p != p.root_path(), "output path rejected"); PinDirs(p.parent_path());
    Require(CreateDirectoryW(p.c_str(), nullptr), "output directory must be fresh"); output = p; PinDirs(output);
    std::array<unsigned char, 16> n{}; Require(BCryptGenRandom(nullptr, n.data(), ULONG(n.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0, "nonce failed");
    nonce = Hex(n.data(), n.size());
  }
  void Write(const std::string& name, const void* p, size_t bytes) {
    Require(bytes <= kOutputBudget - output_bytes, "total output budget exceeded");
    auto path = output / fs::path(name);
    Handle h(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    Require(h.h != INVALID_HANDLE_VALUE, "output collision/open failed");
    DWORD wrote = 0; Require(WriteFile(h.h, p, DWORD(bytes), &wrote, nullptr) && wrote == bytes && FlushFileBuffers(h.h), "output write failed");
    output_bytes += bytes; output_names.push_back(name);
  }
  void Write(const std::string& name, const std::string& data) { Write(name, data.data(), data.size()); }
  void Load() {
    Require(GetProcAddress(GetModuleHandleW(nullptr), "renderdoc__replay__marker") != nullptr, "replay marker absent");
    Require(!GetModuleHandleW(L"renderdoc.dll"), "refuse preloaded RenderDoc");
    auto dll = PinFile(kDll, 512ull * 1024 * 1024); Require(Hash(dll.h, 512ull * 1024 * 1024) == kDllSha, "DLL SHA mismatch");
    pins.push_back(std::move(dll)); Reserve();
    // Child-only environment/cwd, before DllMain can create its initial log.
    Require(SetEnvironmentVariableW(L"TEMP", output.c_str()) && SetEnvironmentVariableW(L"TMP", output.c_str()) &&
        SetCurrentDirectoryW(output.c_str()), "owned temp environment failed");
    module = LoadLibraryExW(kDll, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    Require(module != nullptr, "DLL load failed");
    wchar_t resolved[32768]{}; DWORD n = GetModuleFileNameW(module, resolved, DWORD(std::size(resolved)));
    Require(n && n < std::size(resolved) && EqualPath(std::wstring_view(resolved, n), kDll), "loaded module path mismatch");
    ident = Bind<GetIdent>("INTERNAL_GetTargetControlIdent"); Require(Ident() == 0, "target listener detected");
    alloc_array = Bind<pRENDERDOC_AllocArrayMem>("RENDERDOC_AllocArrayMem");
    free_array = Bind<pRENDERDOC_FreeArrayMem>("RENDERDOC_FreeArrayMem");
    auto version = Bind<decltype(&RENDERDOC_GetVersionString)>("RENDERDOC_GetVersionString");
    auto commit = Bind<decltype(&RENDERDOC_GetCommitHash)>("RENDERDOC_GetCommitHash");
    Require(std::string(version()) == "1.45" && std::string(commit()) == kCommit, "version/commit mismatch");
    auto set_log = Bind<decltype(&RENDERDOC_SetDebugLogFile)>("RENDERDOC_SetDebugLogFile");
    set_log(Utf8((output / L"renderdoc.log").wstring()).c_str());
  }
};
struct Replay {
  State& s;
  bool initialized = false;
  ICaptureFile* file = nullptr;
  IReplayController* controller = nullptr;
  decltype(&RENDERDOC_ShutdownReplay) shutdown = nullptr;
  explicit Replay(State& state) : s(state) {
    shutdown = s.Bind<decltype(shutdown)>("RENDERDOC_ShutdownReplay");
    auto init = s.Bind<decltype(&RENDERDOC_InitialiseReplay)>("RENDERDOC_InitialiseReplay");
    GlobalEnvironment env{}; env.enumerateGPUs = false;
    { rdcarray<rdcstr> args; init(env, args); } initialized = true;
    if (s.Ident() != 0) { Close(); throw std::runtime_error("listener after init"); }
  }
  void Close() {
    if (controller) { auto* p = controller; controller = nullptr; p->Shutdown(); }
    if (file) { auto* p = file; file = nullptr; p->Shutdown(); }
    if (initialized) { initialized = false; shutdown(); }
  }
  ~Replay() { Close(); }
  void Open(const fs::path& p) {
    file = s.Bind<decltype(&RENDERDOC_OpenCaptureFile)>("RENDERDOC_OpenCaptureFile")(); Require(file != nullptr, "capture handle absent");
    Require(file->OpenFile(Utf8(p.wstring()).c_str(), "", nullptr).OK(), "RDC OpenFile failed");
    Require(file->LocalReplaySupport() == ReplaySupport::Supported, "local replay unsupported; remote forbidden");
    auto opened = file->OpenCapture(ReplayOptions{}, nullptr); controller = opened.second;
    Require(opened.first.OK() && controller != nullptr, "local OpenCapture failed");
  }
};
void Inventory(State& s, IReplayController& c, uint32_t wanted_event, bool selecting, bool require_draw = false, bool require_dispatch = false) {
  Require(!(require_draw && require_dispatch), "conflicting selected action requirements");
  const auto& textures = c.GetTextures(); const auto& resources = c.GetResources();
  Require(textures.size() <= kTextures && resources.size() <= kRows, "resource inventory budget exceeded");
  Text t; t.Add("id\twidth\theight\tdepth\tdimension\ttype\tmips\tarraysize\tsamples\tbytes\tcreation_flags\tformat_type\tcomponents\tcomponent_bytes\tcomponent_type\tbgra\tsrgb");
  Text u; u.Add("id\tevent\tusage");
  for (const auto& x : textures) {
    t.Add(Row(Id(x.resourceId), x.width, x.height, x.depth, x.dimension, unsigned(x.type), x.mips, x.arraysize,
        x.msSamp, x.byteSize, unsigned(x.creationFlags), unsigned(x.format.type), unsigned(x.format.compCount),
        unsigned(x.format.compByteWidth), unsigned(x.format.compType), x.format.BGRAOrder(), x.format.SRGBCorrected()));
    auto usage = c.GetUsage(x.resourceId); Require(usage.size() <= kRows, "resource usage budget exceeded");
    for (const auto& e : usage) u.Add(Row(Id(x.resourceId), e.eventId, unsigned(e.usage)));
  }
  Text r; r.Add("id\ttype\tname");
  for (const auto& x : resources) r.Add(Row(Id(x.resourceId), unsigned(x.type), Escape(std::string_view(x.name.c_str(), x.name.size()))));
  Text a; a.Add("event\taction\tparent_event\tflags\tname\tnum_indices\tnum_instances");
  struct Pending { const ActionDescription* action; uint32_t parent; size_t depth; };
  std::vector<Pending> todo; const auto& roots = c.GetRootActions();
  Require(roots.size() <= kRows, "root action budget exceeded");
  for (size_t i = roots.size(); i > 0; --i) todo.push_back({&roots[i - 1], 0, 0});
  bool found = false, draw_found = false, dispatch_found = false; const auto& structured = c.GetStructuredFile();
  while (!todo.empty()) {
    auto p = todo.back(); todo.pop_back(); const auto& x = *p.action;
    Require(p.depth <= 128 && todo.size() + x.children.size() <= kRows, "action tree budget exceeded");
    auto name = x.GetName(structured);
    a.Add(Row(x.eventId, x.actionId, p.parent, uint64_t(x.flags), Escape(std::string_view(name.c_str(), name.size())), x.numIndices, x.numInstances));
    found |= x.eventId == wanted_event;
    draw_found |= SelectedDraw(x.eventId, x.flags, wanted_event);
    dispatch_found |= SelectedDispatch(x.eventId, x.flags, wanted_event);
    for (size_t i = x.children.size(); i > 0; --i) todo.push_back({&x.children[i - 1], x.eventId, p.depth + 1});
  }
  Require(!selecting || found, "selected event is not an action in this capture");
  Require(!require_draw || draw_found, "bindings event must be an actual drawcall");
  Require(!require_dispatch || dispatch_found, "compute bindings event must be an actual dispatch");
  s.Write("textures.tsv", t.data); s.Write("resources.tsv", r.data); s.Write("actions.tsv", a.data); s.Write("usage.tsv", u.data);
}
void Bindings(State& s, IReplayController& c, ShaderStage stage = ShaderStage::Pixel) {
  Require(stage == ShaderStage::Pixel || stage == ShaderStage::Compute, "binding query stage unsupported");
  const bool compute = stage == ShaderStage::Compute;
  if (compute) {
    const auto* pipeline = c.GetD3D12PipelineState();
    Require(pipeline && ComputeBindingStage(Id(pipeline->computeShader.resourceId), pipeline->computeShader.stage), "bound Compute shader required");
  }
  const auto& access = c.GetDescriptorAccess(); Require(access.size() <= 8192, "binding access budget exceeded");
  Text t; t.Add(std::string("kind\treflection_index\tarray_element\tstore\toffset\tdescriptor_bytes\taccess_type\tstatically_unused\tresource_or_filter_min\tfirst_mip_or_filter_mag\tmips_or_filter_mip\tfirst_slice_or_filter_function\tslices_or_min_lod\tmin_lod_clamp_or_max_lod\tformat_type_or_mip_bias\tcomponents_or_anisotropy") +
      (compute ? "\tstage\tbuffer_byte_offset\tbuffer_byte_size" : ""));
  for (const auto& a : access) {
    if (a.stage != stage || (!BindingResourceType(stage, a.type) && !IsSamplerDescriptor(a.type))) continue;
    Require(a.byteSize > 0 && Id(a.descriptorStore) != 0, "invalid descriptor access");
    rdcarray<DescriptorRange> ranges; ranges.push_back(DescriptorRange(a));
    const auto prefix = Row(a.index, a.arrayElement, Id(a.descriptorStore), a.byteOffset, a.byteSize, unsigned(a.type), a.staticallyUnused);
    if (BindingResourceType(stage, a.type)) {
      auto descriptors = c.GetDescriptors(a.descriptorStore, ranges); Require(descriptors.size() == 1, "descriptor count mismatch");
      const auto& d = descriptors[0]; t.Add(Row(compute && IsReadWriteDescriptor(a.type) ? "resource_rw" : "resource", prefix, Id(d.resource), unsigned(d.firstMip), unsigned(d.numMips),
          d.firstSlice, d.numSlices, d.minLODClamp, unsigned(d.format.type), unsigned(d.format.compCount)) +
          (compute ? "\t" + Row(unsigned(stage), d.byteOffset, d.byteSize) : ""));
    }
    if (IsSamplerDescriptor(a.type) || a.type == DescriptorType::ImageSampler) {
      auto samplers = c.GetSamplerDescriptors(a.descriptorStore, ranges); Require(samplers.size() == 1, "sampler count mismatch");
      const auto& d = samplers[0]; t.Add(Row("sampler", prefix, unsigned(d.filter.minify), unsigned(d.filter.magnify),
          unsigned(d.filter.mip), unsigned(d.filter.filter), d.minLOD, d.maxLOD, d.mipBias, d.maxAnisotropy) +
          (compute ? "\t" + Row(unsigned(stage), "none", "none") : ""));
    }
  }
  s.Write(compute ? "cs-bindings.tsv" : "ps-bindings.tsv", t.data);
}
void BindingObservation(State& s, IReplayController& c, uint32_t event, ShaderStage stage = ShaderStage::Pixel) {
  Require(stage == ShaderStage::Pixel || stage == ShaderStage::Compute, "binding metadata stage unsupported");
  const auto* pipeline = c.GetD3D12PipelineState();
  Require(pipeline != nullptr, "D3D12 pipeline state unavailable");
  const auto& access = c.GetDescriptorAccess();
  Require(access.size() <= 8192, "all-stage access budget exceeded");
  const auto& root = pipeline->rootSignature;
  Require(root.parameters.size() <= 64, "root parameter budget exceeded");
  Text summary; summary.Add("event\tpipeline\troot_signature\tstage\tshader\treflection_present\treflection_readonly_count\treflection_sampler_count\ttotal_access_count");
  if (stage == ShaderStage::Compute)
    Require(ComputeBindingStage(Id(pipeline->computeShader.resourceId), pipeline->computeShader.stage), "compute pipeline metadata has no bound Compute shader");
  const std::vector<const D3D12Pipe::Shader*> shaders = stage == ShaderStage::Compute ?
      std::vector<const D3D12Pipe::Shader*>{&pipeline->computeShader} : std::vector<const D3D12Pipe::Shader*>{&pipeline->vertexShader, &pipeline->pixelShader};
  for (const auto* shader : shaders) {
    const auto* reflection = shader->reflection;
    Require(!reflection || (reflection->readOnlyResources.size() <= 8192 && reflection->samplers.size() <= 8192), "reflection count budget exceeded");
    summary.Add(Row(event, Id(pipeline->pipelineResourceId), Id(root.resourceId), unsigned(shader->stage),
        Id(shader->resourceId), reflection != nullptr, reflection ? reflection->readOnlyResources.size() : 0,
        reflection ? reflection->samplers.size() : 0, access.size()));
  }
  Text descriptors; descriptors.Add("stage\ttype\treflection_index\tarray_element\tstore\tbyte_offset\tbyte_size\tstatically_unused");
  for (const auto& a : access)
    descriptors.Add(Row(unsigned(a.stage), unsigned(a.type), a.index, a.arrayElement, Id(a.descriptorStore),
        a.byteOffset, a.byteSize, a.staticallyUnused));
  Text parameters;
  parameters.Add("kind\tparameter\trange\tvisibility\tregister\tspace\tresource\tdescriptor_type\tbyte_offset\tbyte_size\theap\theap_byte_offset\trange_category\trange_count\trange_table_byte_offset\trange_appended");
  for (size_t i = 0; i < root.parameters.size(); ++i) {
    const auto& p = root.parameters[i];
    Require(p.tableRanges.size() <= 32, "root table range budget exceeded");
    parameters.Add(Row("parameter", i, "none", uint32_t(p.visibility), p.reg, p.space, Id(p.descriptor.resource),
        unsigned(p.descriptor.type), p.descriptor.byteOffset, p.descriptor.byteSize, Id(p.heap), p.heapByteOffset,
        "none", "none", "none", "none"));
    for (size_t j = 0; j < p.tableRanges.size(); ++j) {
      const auto& range = p.tableRanges[j];
      parameters.Add(Row("table_range", i, j, uint32_t(p.visibility), range.baseRegister, range.space,
          "none", "none", "none", "none", Id(p.heap), p.heapByteOffset, unsigned(range.category),
          range.count, range.tableByteOffset, range.appended));
    }
  }
  // Metadata only: no root constants, buffer bytes, descriptor heap sweep, or
  // inference from engine-specific register/descriptor slots.
  s.Write("pipeline-summary.tsv", summary.data);
  s.Write("descriptor-access.tsv", descriptors.data);
  s.Write("root-parameters.tsv", parameters.data);
}
void EngineDescriptorCandidates(State& s, IReplayController& c, uint32_t event) {
  Text source, indices, descriptors;
  source.Add("event\tstatus\tbuffer\tbyte_offset\tbytes\tdwords\tscope");
  indices.Add("slot\tindex\tscope");
  descriptors.Add("slot\tindex\tkind\treflection_index\tspace\tregister\troot_parameter\theap\tbyte_offset\tstatus\tresource\ttype\tfirst_mip\tmips\tmin_lod\tmax_lod\tmip_bias\tanisotropy\tfilter_minify\tfilter_magnify\tfilter_mip\tfilter_function");
  const auto finish = [&] {
    s.Write("candidate-source.tsv", source.data); s.Write("candidate-indices.tsv", indices.data);
    s.Write("candidate-descriptors.tsv", descriptors.data);
  };
  const auto unavailable = [&](const char* status) {
    source.Add(Row(event, status, 0, 0, 0, 0, "mixed_candidates_not_observed_accesses_may_include_padding")); finish();
  };
  const auto* pipeline = c.GetD3D12PipelineState();
  Require(pipeline != nullptr && pipeline->rootSignature.parameters.size() <= 64, "candidate root state rejected");
  const auto* reflection = pipeline->pixelShader.reflection;
  if (!reflection) { unavailable("no_pixel_reflection"); return; }
  Require(reflection->constantBlocks.size() <= 64 && reflection->readOnlyResources.size() + reflection->samplers.size() <= 8,
      "candidate reflection budget exceeded");
  const ConstantBlock* block = nullptr;
  for (const auto& b : reflection->constantBlocks) if (b.name == "xe_descriptor_indices_cbuffer") {
    Require(!block, "duplicate engine candidate block"); block = &b;
  }
  if (!block) { unavailable("no_named_engine_block"); return; }
  Require(block->bufferBacked && !block->inlineDataBytes && !block->compileConstants &&
      block->fixedBindNumber == 4 && block->fixedBindSetOrSpace == 0 && block->bindArraySize == 1 &&
      block->variables.size() == 1, "engine candidate block identity rejected");
  const auto& variable = block->variables[0]; const auto& type = variable.type;
  Require(variable.name == "xe_descriptor_indices" && variable.byteOffset == 0 && !variable.bitFieldSize &&
      !variable.bitFieldOffset && type.baseType == VarType::UInt && type.rows == 1 && type.columns == 4 &&
      type.members.empty() && type.elements > 0 && type.elements <= 64 &&
      (type.arrayByteStride == 16 || (type.elements == 1 && type.arrayByteStride == 0)) &&
      block->byteSize == type.elements * 16, "engine uint4 candidate layout rejected");
  const D3D12Pipe::RootParam* cbv = nullptr;
  for (const auto& p : pipeline->rootSignature.parameters) {
    Require(p.tableRanges.size() <= 32, "candidate root range budget exceeded");
    if (bool(p.visibility & ShaderStageMask::Pixel) && p.reg == 4 && p.space == 0 &&
        p.descriptor.type == DescriptorType::ConstantBuffer && Id(p.descriptor.resource)) {
      Require(!cbv && p.tableRanges.empty() && !Id(p.heap) && p.constants.empty(), "candidate CBV ambiguous"); cbv = &p;
    }
  }
  Require(cbv != nullptr && cbv->descriptor.byteSize >= block->byteSize, "candidate Pixel b4 CBV absent or short");
  const auto& buffers = c.GetBuffers(); Require(buffers.size() <= kRows, "candidate buffer inventory budget exceeded");
  const BufferDescription* buffer = nullptr;
  for (const auto& b : buffers) if (b.resourceId == cbv->descriptor.resource) {
    Require(!buffer, "duplicate candidate buffer"); buffer = &b;
  }
  Require(buffer && CandidateReadBounds(cbv->descriptor.byteOffset, block->byteSize, buffer->length), "candidate buffer read bounds rejected");
  auto bytes = c.GetBufferData(buffer->resourceId, cbv->descriptor.byteOffset, block->byteSize);
  Require(bytes.size() == block->byteSize, "candidate buffer read incomplete");
  source.Add(Row(event, "decoded", Id(buffer->resourceId), cbv->descriptor.byteOffset, bytes.size(), bytes.size() / 4,
      "mixed_candidates_not_observed_accesses_may_include_padding"));
  const auto& stores = c.GetDescriptorStores(); Require(stores.size() <= 8192, "candidate descriptor-store budget exceeded");
  struct Binding { bool sampler; uint32_t reflection_index, space, reg, array; DescriptorType type; };
  std::vector<Binding> bindings;
  for (size_t i = 0; i < reflection->readOnlyResources.size(); ++i) {
    const auto& r = reflection->readOnlyResources[i];
    if (r.isTexture && r.isReadOnly && r.descriptorType == DescriptorType::Image)
      bindings.push_back({false, uint32_t(i), r.fixedBindSetOrSpace, r.fixedBindNumber, r.bindArraySize, r.descriptorType});
  }
  for (size_t i = 0; i < reflection->samplers.size(); ++i) {
    const auto& r = reflection->samplers[i];
    bindings.push_back({true, uint32_t(i), r.fixedBindSetOrSpace, r.fixedBindNumber, r.bindArraySize, DescriptorType::Sampler});
  }
  // SDK command_processor.cpp writes texture and sampler values to the SAME
  // uint4 array. Each lane is tried only against reflected arrays; no claimed
  // slot typing, actual access, vinyl identity, or removal of padded lanes.
  for (size_t slot = 0; slot < bytes.size() / 4; ++slot) {
    uint32_t value = 0; std::memcpy(&value, bytes.data() + slot * 4, 4);
    indices.Add(Row(slot, value, "mixed_candidate_or_padding"));
    for (const auto& binding : bindings) {
      const D3D12Pipe::RootParam* table = nullptr; const D3D12Pipe::RootTableRange* range = nullptr;
      size_t root_index = 0;
      for (size_t i = 0; i < pipeline->rootSignature.parameters.size(); ++i) {
        const auto& p = pipeline->rootSignature.parameters[i];
        if (!bool(p.visibility & ShaderStageMask::Pixel) || !Id(p.heap)) continue;
        for (const auto& r : p.tableRanges) {
          if (r.category != (binding.sampler ? DescriptorCategory::Sampler : DescriptorCategory::ReadOnlyResource) ||
              r.space != binding.space || binding.reg < r.baseRegister || uint64_t(binding.reg) - r.baseRegister >= r.count) continue;
          Require(!table, "candidate reflected root table ambiguous"); table = &p; range = &r; root_index = i;
        }
      }
      Require(table && range, "candidate reflected root table absent");
      const DescriptorStoreDescription* store = nullptr;
      for (const auto& x : stores) if (x.resourceId == table->heap) { Require(!store, "duplicate candidate descriptor store"); store = &x; }
      Require(store != nullptr, "candidate descriptor store absent"); uint32_t offset = 0;
      const auto prefix = Row(slot, value, binding.sampler ? "sampler_candidate" : "texture_candidate",
          binding.reflection_index, binding.space, binding.reg, root_index, Id(table->heap));
      if (!CandidateDescriptorOffset(table->heapByteOffset, range->tableByteOffset, range->baseRegister, range->count,
          binding.reg, binding.array, value, store->firstDescriptorOffset, store->descriptorByteSize, store->descriptorCount, offset)) {
        descriptors.Add(Row(prefix, "none", "out_of_bounds_not_queried", "none", "none", "none", "none", "none", "none", "none", "none", "none", "none", "none", "none")); continue;
      }
      DescriptorRange request; request.offset = offset; request.descriptorSize = store->descriptorByteSize;
      request.count = 1; request.type = binding.type; rdcarray<DescriptorRange> requests; requests.push_back(request);
      if (binding.sampler) {
        auto found = c.GetSamplerDescriptors(table->heap, requests); Require(found.size() == 1, "candidate sampler result count rejected");
        const auto& d = found[0];
        descriptors.Add(Row(prefix, offset, "candidate_not_observed_access", "none", "sampler", "none", "none", d.minLOD, d.maxLOD, d.mipBias, d.maxAnisotropy,
            unsigned(d.filter.minify), unsigned(d.filter.magnify), unsigned(d.filter.mip), unsigned(d.filter.filter)));
      } else {
        auto found = c.GetDescriptors(table->heap, requests); Require(found.size() == 1, "candidate descriptor result count rejected");
        const auto& d = found[0];
        descriptors.Add(Row(prefix, offset, "candidate_not_observed_access", Id(d.resource), unsigned(d.type), unsigned(d.firstMip), unsigned(d.numMips),
            d.minLODClamp, "none", "none", "none", "none", "none", "none", "none"));
      }
    }
  }
  finish();
}
void Failure(State* s, const char* message) noexcept {
  if (!s || s->output.empty()) return;
  try {
    const auto bounded = std::string_view(message ? message : "unknown exception").substr(0, 512);
    // CREATE_NEW through Write: never replace prior evidence; no COMPLETE on failure.
    s->Write("FAILURE.tsv", "MCLA-RENDERDOC-REPLAY-FAILURE-V1\nerror\t" + Escape(bounded) + "\n");
  } catch (...) { /* best effort only, including budget exhaustion */ }
}
void PixelDisassembly(State& s, IReplayController& c) {
  const auto* pipeline = c.GetD3D12PipelineState();
  const auto* reflection = pipeline ? pipeline->pixelShader.reflection : nullptr;
  if (!reflection || reflection->encoding != ShaderEncoding::DXBC) {
    s.Write("pixel-disassembly.txt", "MCLA-PIXEL-DISASSEMBLY-V1\nUNSUPPORTED_NO_DXBC_REFLECTION\n"); return;
  }
  auto targets = c.GetDisassemblyTargets(true);
  Require(targets.size() <= 32, "disassembly target budget exceeded");
  if (targets.empty()) { s.Write("pixel-disassembly.txt", "MCLA-PIXEL-DISASSEMBLY-V1\nUNSUPPORTED_NO_NATIVE_TARGET\n"); return; }
  // RenderDoc 1.45 API contract: first target is native disassembly. Never
  // request a hardware ISA backend, execute, replace, or dump other shaders.
  Require(targets[0].size() <= 128, "native disassembly target name rejected");
  auto text = c.DisassembleShader(pipeline->pipelineResourceId, reflection, targets[0]);
  Require(text.size() <= 2 * 1024 * 1024 - 512, "selected PS disassembly budget exceeded");
  s.Write("pixel-disassembly.txt", "MCLA-PIXEL-DISASSEMBLY-V1\n" + Row("shader", Id(pipeline->pixelShader.resourceId)) + "\n" +
      Row("target", Escape(std::string_view(targets[0].c_str(), targets[0].size()))) +
      "\nscope\tnative disassembler response, may contain an error; not observed accesses\nCONTENT\n" + std::string(text.c_str(), text.size()));
}
void BindingLogSnapshot(State& s) {
  constexpr uint64_t cap = 1024 * 1024;
  const auto path = s.output / L"renderdoc.log";
  // The active logger must retain write access; this is intentionally not PinFile.
  Handle log(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (log.h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND) {
    s.Write("renderdoc-snapshot.log", "MCLA-RENDERDOC-LOG-SNAPSHOT-V1\nUNAVAILABLE\n"); return;
  }
  BY_HANDLE_FILE_INFORMATION info{}; LARGE_INTEGER size{};
  Require(log.h != INVALID_HANDLE_VALUE && GetFileInformationByHandle(log.h, &info) &&
      !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
      info.nNumberOfLinks == 1 && EqualPath(FinalPath(log.h), path.wstring()) &&
      GetFileSizeEx(log.h, &size) && size.QuadPart >= 0, "active replay log identity rejected");
  const uint64_t original = uint64_t(size.QuadPart), offset = LogTailOffset(original, cap);
  LARGE_INTEGER position{}; position.QuadPart = LONGLONG(offset);
  Require(SetFilePointerEx(log.h, position, nullptr, FILE_BEGIN), "active replay log tail seek failed");
  // Last bounded bytes as of observed size. Later appends are not included.
  std::vector<unsigned char> bytes(static_cast<size_t>(original - offset)); DWORD got = 0;
  Require(bytes.empty() || (ReadFile(log.h, bytes.data(), DWORD(bytes.size()), &got, nullptr) && got == bytes.size()),
      "active replay log snapshot read failed");
  std::string snapshot = "MCLA-RENDERDOC-LOG-SNAPSHOT-V2\n" + Row("original_size", original) + "\n" +
      Row("byte_offset", offset) + "\n" + Row("bytes", bytes.size()) + "\n" + Row("truncated", offset != 0) + "\nCONTENT\n";
  Require(snapshot.size() <= 512, "log tail header budget exceeded");
  if (!bytes.empty()) snapshot.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  s.Write("renderdoc-snapshot.log", snapshot);
}
void Extract(State& s, IReplayController& c, uint32_t event, uint64_t wanted, uint32_t mip) {
  const TextureDescription* selected = nullptr;
  for (const auto& t : c.GetTextures()) if (Id(t.resourceId) == wanted) selected = &t;
  Require(selected != nullptr, "resource is not a captured texture"); const auto& t = *selected;
  Require(t.dimension == 2 && t.depth == 1 && t.arraysize == 1 && t.msSamp == 1 && mip < t.mips && mip < 32,
      "extract supports single-slice non-MSAA 2D texture only");
  const uint64_t width = std::max(1u, t.width >> mip), height = std::max(1u, t.height >> mip);
  Require(width <= 16384 && height <= 16384 && width * height <= kOutputBudget / 16 &&
      t.byteSize > 0 && t.byteSize <= kOutputBudget / 2, "texture extraction allocation budget exceeded");
  Require(t.byteSize + width * height * 8 + 65536 <= kOutputBudget - s.output_bytes,
      "raw plus conservative PNG output estimate exceeds budget");
  c.SetFrameEvent(event, true); Require(s.Ident() == 0, "listener before extraction"); Bindings(s, c);
  auto bytes = c.GetTextureData(t.resourceId, Subresource(mip, 0, 0));
  Require(bytes.size() > 0 && bytes.size() <= kOutputBudget / 2, "raw subresource budget exceeded");
  s.Write("texture.raw", bytes.data(), bytes.size());
  auto png = s.output / L"texture.png";
  Require(GetFileAttributesW(png.c_str()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND, "PNG collision");
  TextureSave save{}; save.resourceId = t.resourceId; save.mip = int32_t(mip); save.slice.sliceIndex = 0;
  save.alpha = AlphaMapping::Preserve; save.destType = FileType::PNG;
  Require(c.SaveTexture(save, Utf8(png.wstring()).c_str()).OK(), "PNG SaveTexture failed");
  auto pin = s.PinFile(png, kOutputBudget - s.output_bytes); LARGE_INTEGER size{};
  Require(GetFileSizeEx(pin.h, &size) && size.QuadPart > 0, "PNG empty");
  s.output_bytes += uint64_t(size.QuadPart); s.output_names.push_back("texture.png"); s.pins.push_back(std::move(pin));
  s.Write("selection.tsv", "event\tresource\tmip\tslice\tsample\twidth\theight\n" + Row(event, wanted, mip, 0, 0, width, height) + "\n");
}
void TargetPreview(State& s, IReplayController& c, uint32_t event, uint64_t wanted, uint32_t slice) {
  const TextureDescription* selected = nullptr;
  for (const auto& t : c.GetTextures()) if (Id(t.resourceId) == wanted) {
    Require(!selected, "duplicate target texture identity"); selected = &t;
  }
  Require(selected != nullptr, "target resource is not a captured texture");
  const auto& t = *selected; AdmitColorTarget(t, slice);
  Require(PreviewBudget(uint64_t(t.width) * t.height * 8 + 131072, s.output_bytes), "target PNG estimate exceeds budget");
  c.SetFrameEvent(event, true); Require(s.Ident() == 0, "listener before target preview");
  TextureSave save{}; save.resourceId = t.resourceId; save.mip = 0; save.slice.sliceIndex = int32_t(slice);
  save.slice.slicesAsGrid = false; save.slice.cubeCruciform = false;
  save.sample.mapToArray = false; save.sample.sampleIndex = TextureSampleMapping::ResolveSamples;
  save.typeCast = CompType::Typeless; save.destType = FileType::PNG; save.channelExtract = -1;
  save.comp.blackPoint = 0.0f; save.comp.whitePoint = 1.0f; save.alpha = AlphaMapping::Preserve;
  // Pinned API defines ResolveSamples as default average resolve; this is a
  // viewing projection with 0..1 clamping, not an individual sample oracle.
  const auto path = s.output / L"target.png";
  Require(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND, "target PNG collision");
  Require(c.SaveTexture(save, Utf8(path.wstring()).c_str()).OK(), "target SaveTexture failed");
  auto pin = s.PinFile(path, kOutputBudget - s.output_bytes); LARGE_INTEGER size{};
  Require(GetFileSizeEx(pin.h, &size) && size.QuadPart > 0, "target PNG empty");
  s.output_bytes += uint64_t(size.QuadPart); s.output_names.push_back("target.png"); s.pins.push_back(std::move(pin));
  s.Write("target-selection.tsv", TargetSelection(event, wanted, t, slice));
}
void CopyLinkObservation(State& s, IReplayController& c, uint32_t event, uint64_t destination) {
  // Action metadata only. In particular, buffer Subresource defaults are NOT
  // byte offsets/lengths and cannot prove which bytes a partial copy updates.
  const ActionDescription* selected = nullptr;
  std::unordered_map<uint32_t, ActionFlags> actions;
  struct Pending { const ActionDescription* action; size_t depth; };
  std::vector<Pending> pending; const auto& roots = c.GetRootActions();
  Require(roots.size() <= kRows, "copy-link root budget exceeded");
  for (const auto& root : roots) pending.push_back({&root, 0});
  size_t visited = 0;
  while (!pending.empty()) {
    const auto p = pending.back(); pending.pop_back(); const auto& a = *p.action;
    Require(++visited <= kRows && p.depth <= 128 && pending.size() + a.children.size() <= kRows, "copy-link action budget exceeded");
    if (a.eventId && a.eventId <= event) Require(actions.emplace(a.eventId, a.flags).second, "copy-link duplicate action event");
    if (a.eventId == event) { Require(!selected, "copy-link ambiguous selected event"); selected = &a; }
    for (const auto& child : a.children) pending.push_back({&child, p.depth + 1});
  }
  Require(selected && SelectedCopy(selected->eventId, selected->flags, event, Id(selected->copySource),
      Id(selected->copyDestination), destination), "copy-link requires exact Copy action and destination; no draw/dispatch");
  const auto source = selected->copySource;
  const auto& buffers = c.GetBuffers(); const auto& textures = c.GetTextures();
  Require(buffers.size() <= kRows && textures.size() <= kTextures, "copy-link resource metadata budget exceeded");
  size_t matches = 0; std::string kind, length, flags;
  for (const auto& b : buffers) if (b.resourceId == source) {
    ++matches; kind = "buffer"; length = std::to_string(b.length); flags = std::to_string(uint32_t(b.creationFlags));
  }
  for (const auto& t : textures) if (t.resourceId == source) {
    ++matches; kind = "texture"; length = "none"; flags = std::to_string(uint32_t(t.creationFlags));
  }
  Require(matches == 1, "copy-link source must uniquely classify as buffer or texture");
  auto usage = c.GetUsage(source);
  Require(CopyUsageBudget(usage.size(), 0), "copy-link source total usage budget exceeded");
  Text rows; rows.Add("source\tevent\tusage\taction_present\taction_flags");
  size_t eligible = 0;
  for (const auto& u : usage) {
    Require(u.eventId > 0, "copy-link zero source usage event");
    if (u.eventId > event) continue;
    Require(CopyUsageBudget(usage.size(), ++eligible), "copy-link eligible usage budget exceeded");
    const auto a = actions.find(u.eventId);
    rows.Add(Row(Id(source), u.eventId, uint32_t(u.usage), a != actions.end(), a == actions.end() ? 0 : uint64_t(a->second)));
  }
  const auto& src = selected->copySourceSubresource; const auto& dst = selected->copyDestinationSubresource;
  s.Write("copy-link.tsv", "event\taction_flags\tsource\tdestination\tsource_mip\tsource_slice\tsource_sample\tdestination_mip\tdestination_slice\tdestination_sample\ttotal_source_usage\texported_source_usage\tscope\n" +
      Row(event, uint64_t(selected->flags), Id(source), destination, src.mip, src.slice, src.sample, dst.mip, dst.slice, dst.sample,
          usage.size(), eligible, "action_metadata_only_buffer_subresource_not_byte_offset_not_full_copy_proof") + "\n");
  s.Write("copy-source.tsv", "resource\tkind\tbuffer_length\tcreation_flags\tscope\n" +
      Row(Id(source), kind, length, flags, "metadata_only_no_gpu_address_or_bytes") + "\n");
  s.Write("copy-source-usage.tsv", rows.data);
}
void PixelHistoryObservation(State& s, IReplayController& c, uint32_t event, uint64_t wanted, uint32_t x, uint32_t y) {
  Require(c.GetAPIProperties().pixelHistory, "pixel history API unsupported");
  const TextureDescription* selected = nullptr;
  for (const auto& t : c.GetTextures()) if (Id(t.resourceId) == wanted) {
    Require(!selected, "duplicate pixel history target"); selected = &t;
  }
  Require(selected != nullptr, "pixel history resource is not a captured texture");
  AdmitColorTarget(*selected, 0);
  Require(HistoryPoint(selected->width, selected->height, x, y, 0), "pixel history coordinate/sample rejected");
  // Conservative admission counts ALL usages through the selected action, not
  // merely RenderDoc's internal write-usage subset. No enumeration of pixels.
  auto usage = c.GetUsage(selected->resourceId); Require(usage.size() <= kRows, "pixel history usage metadata budget exceeded");
  size_t eligible = 0;
  for (const auto& u : usage) if (HistoryUsage(u.eventId, event)) Require(HistoryBudget(++eligible, 0), "pixel history eligible usage budget exceeded");
  std::unordered_map<uint32_t, ActionFlags> actions;
  struct Pending { const ActionDescription* action; size_t depth; };
  std::vector<Pending> pending; const auto& roots = c.GetRootActions();
  Require(roots.size() <= kRows, "pixel history action root budget exceeded");
  for (const auto& root : roots) pending.push_back({&root, 0});
  size_t visited = 0;
  while (!pending.empty()) {
    const auto p = pending.back(); pending.pop_back(); const auto& a = *p.action;
    Require(++visited <= kRows && p.depth <= 128 && pending.size() + a.children.size() <= kRows, "pixel history action tree budget exceeded");
    if (a.eventId && a.eventId <= event) Require(actions.emplace(a.eventId, a.flags).second, "pixel history duplicate action event");
    for (const auto& child : a.children) pending.push_back({&child, p.depth + 1});
  }
  Require(actions.find(event) != actions.end(), "pixel history requested action absent");
  s.Write("pixel-history-request.tsv",
      "event\tresource\tx\ty\tmip\tslice\tsample\tnative_samples\twidth\theight\ttype_cast\teligible_usage\tscope\n" +
      Row(event, wanted, x, y, 0, 0, 0, selected->msSamp, selected->width, selected->height, "native", eligible,
          "sample0_candidates_not_final_visible_contribution") + "\n");
  c.SetFrameEvent(event, true); Require(s.Ident() == 0, "listener before pixel history");
  // One explicit sample0 query. No ~0U resolve, shader debugging, another pixel,
  // history retry, or automatic follow-up. Returned colors/depth are NOT saved.
  auto history = c.PixelHistory(selected->resourceId, x, y, Subresource(0, 0, 0), CompType::Typeless);
  Require(HistoryBudget(eligible, history.size()), "pixel history row budget exceeded");
  Text rows;
  rows.Add("event\taction_flags\tfragment\tprimitive\tdirect_write\tunbound_ps\tpassed\tsample_masked\tbackface_culled\tdepth_clipped\tdepth_bounds_failed\tview_clipped\tscissor_clipped\tshader_discarded\tdepth_test_failed\tstencil_test_failed\tpredication_skipped\tcandidate_draw");
  for (const auto& p : history) {
    const auto found = actions.find(p.eventId);
    Require(p.eventId > 0 && p.eventId <= event && found != actions.end(), "pixel history returned event outside actual requested actions");
    rows.Add(Row(p.eventId, uint32_t(found->second), p.fragIndex, p.primitiveID, p.directShaderWrite, p.unboundPS, p.Passed(),
        p.sampleMasked, p.backfaceCulled, p.depthClipped, p.depthBoundsFailed, p.viewClipped, p.scissorClipped, p.shaderDiscarded,
        p.depthTestFailed, p.stencilTestFailed, p.predicationSkipped, HistoryCandidate(found->second, p)));
  }
  s.Write("pixel-history.tsv", rows.data);
  s.Write("pixel-history-selection.tsv",
      "event\tresource\tx\ty\tmip\tslice\tsample\tnative_samples\twidth\theight\ttype_cast\teligible_usage\trows\tscope\n" +
      Row(event, wanted, x, y, 0, 0, 0, selected->msSamp, selected->width, selected->height, "native", eligible, history.size(),
          "sample0_candidates_not_final_visible_contribution") + "\n");
}
void PixelDebugObservation(State& s, IReplayController& c, uint32_t event, uint64_t wanted,
    uint32_t x, uint32_t y, uint32_t primitive) {
  Require(DebugSelection(event, wanted, x, y, 0, primitive), "pixel debug explicit selection rejected");
  Require(c.GetAPIProperties().shaderDebugging, "shader debugging API unsupported");
  const TextureDescription* selected = nullptr;
  for (const auto& t : c.GetTextures()) if (Id(t.resourceId) == wanted) {
    Require(!selected, "duplicate pixel debug target"); selected = &t;
  }
  Require(selected != nullptr, "pixel debug target absent"); AdmitColorTarget(*selected, 0);
  Require(HistoryPoint(selected->width, selected->height, x, y, 0), "pixel debug point outside target");
  // Inventory has independently required an actual Drawcall before admission.
  c.SetFrameEvent(event, true); Require(s.Ident() == 0, "listener before pixel debug");
  const auto* pipeline = c.GetD3D12PipelineState();
  Require(pipeline && Id(pipeline->pixelShader.resourceId) && pipeline->pixelShader.stage == ShaderStage::Pixel &&
      pipeline->pixelShader.reflection && pipeline->pixelShader.reflection->encoding == ShaderEncoding::DXBC &&
      pipeline->pixelShader.reflection->debugInfo.debuggable, "bound debuggable DXBC Pixel shader required");
  Require(pipeline->outputMerger.renderTargets.size() <= 8, "pixel debug output target budget exceeded");
  size_t matching_targets = 0;
  for (const auto& target : pipeline->outputMerger.renderTargets)
    if (Id(target.resource) == wanted && target.firstMip == 0 && target.firstSlice == 0) ++matching_targets;
  Require(matching_targets == 1, "pixel debug requested target not uniquely bound at mip0/slice0");
  const auto shader = Id(pipeline->pixelShader.resourceId);
  const std::string request =
      "event\tresource\tx\ty\tmip\tslice\tsample\tprimitive\tview\tnative_samples\twidth\theight\tshader\tscope\n" +
      Row(event, wanted, x, y, 0, 0, 0, primitive, 0, 4, selected->width, selected->height, shader,
          "requested_fragment_not_verified") + "\n";
  s.Write("pixel-debug-request.tsv", request); // Before potentially expensive DebugPixel.
  PixelDisassembly(s, c);
  size_t calls = 0, total_states = 0, total_changes = 0, input_count = 0, resource_changes = 0;
  uint32_t last_validated_next = 0;
  DebugFailureCode failure_code = DebugFailureCode::Validation;
  Text states, deltas, inputs, resource_rows, reflected_bindings;
  states.Add("step\tnext_instruction\tflags\tchanges"); deltas.Add(kDebugVariableHeader); inputs.Add(kDebugVariableHeader);
  resource_rows.Add("step\tnext_instruction\tchange\tphase\tpresent\tname\ttype\trows\tcolumns\tflags\tcategory\treflection_index\tarray_element\tarray_size");
  reflected_bindings.Add("category\treflection_index\tarray_size");
  const auto& reflection = *pipeline->pixelShader.reflection;
  Require(reflection.readOnlyResources.size() <= 128 && reflection.readWriteResources.size() <= 128, "pixel debug reflected resource budget exceeded");
  for (size_t i = 0; i < reflection.readOnlyResources.size(); ++i)
    reflected_bindings.Add(Row(unsigned(DescriptorCategory::ReadOnlyResource), i, reflection.readOnlyResources[i].bindArraySize));
  for (size_t i = 0; i < reflection.readWriteResources.size(); ++i)
    reflected_bindings.Add(Row(unsigned(DescriptorCategory::ReadWriteResource), i, reflection.readWriteResources[i].bindArraySize));
  s.Write("pixel-debug-bindings.tsv", reflected_bindings.data);
  struct TraceOwner {
    IReplayController& controller; ShaderDebugTrace* trace;
    ~TraceOwner() { if (trace) controller.FreeTrace(trace); }
    TraceOwner(const TraceOwner&) = delete; TraceOwner& operator=(const TraceOwner&) = delete;
    TraceOwner(IReplayController& c, ShaderDebugTrace* t) : controller(c), trace(t) {}
  };
  try {
    {
      DebugPixelInputs selection; selection.sample = 0; selection.primitive = primitive; selection.view = 0;
      TraceOwner owned(c, c.DebugPixel(x, y, selection));
      Require(owned.trace && owned.trace->debugger && owned.trace->stage == ShaderStage::Pixel, "pixel debug invalid trace/debugger/stage");
      Require(owned.trace->inputs.size() <= 128, "pixel debug input budget exceeded"); input_count = owned.trace->inputs.size();
      for (size_t i = 0; i < input_count; ++i) DebugVariableRows(inputs, owned.trace->inputs[i], 0, 0, i, "input");
      s.Write("pixel-debug-inputs.tsv", inputs.data);
      bool finished = false;
      while (calls < 64) {
        ++calls;
        auto batch = c.ContinueDebug(owned.trace->debugger);
        if (batch.empty()) { finished = true; break; }
        Require(batch.size() <= 16384 - total_states, "pixel debug state budget exceeded");
        for (const auto& state : batch) {
          Require(DebugStep(state.stepIndex, total_states), "pixel debug step sequence rejected");
          Require(state.changes.size() <= 131072 - total_changes, "pixel debug change budget exceeded");
          if (total_states == 0) Require(state.nextInstruction == 0, "pixel debug initial instruction rejected");
          for (size_t i = 0; i < state.changes.size(); ++i) {
            const auto& change = state.changes[i];
            const auto resource_type = [](VarType type) { return type == VarType::ReadOnlyResource || type == VarType::ReadWriteResource; };
            if (resource_type(change.before.type) || resource_type(change.after.type)) {
              ++resource_changes;
              DebugResourceRow(resource_rows, change.before, reflection, state.stepIndex, state.nextInstruction, i, "before");
              DebugResourceRow(resource_rows, change.after, reflection, state.stepIndex, state.nextInstruction, i, "after");
            } else {
              DebugVariableRows(deltas, change.before, state.stepIndex, state.nextInstruction, i, "before");
              DebugVariableRows(deltas, change.after, state.stepIndex, state.nextInstruction, i, "after");
            }
          }
          Require(DebugBudget(calls, total_states+1, total_changes+state.changes.size()), "pixel debug aggregate budget exceeded");
          // Commit the header only after every before/after record in this state
          // validated. Partially validated delta/resource tails are never saved
          // by the failure path below.
          states.Add(Row(state.stepIndex, state.nextInstruction, uint32_t(state.flags), state.changes.size()));
          ++total_states; total_changes += state.changes.size(); last_validated_next = state.nextInstruction;
        }
      }
      if (!finished || total_states == 0) {
        failure_code = !finished ? DebugFailureCode::CallBudget : DebugFailureCode::ZeroStates;
        throw std::runtime_error("pixel debug " + DebugFailureProgress(failure_code, calls, total_states, total_changes, input_count, last_validated_next));
      }
    } // FreeTrace exactly once before final log snapshot/controller shutdown.
    s.Write("pixel-debug-states.tsv", states.data); s.Write("pixel-debug-deltas.tsv", deltas.data);
    s.Write("pixel-debug-resources.tsv", resource_rows.data);
    s.Write("pixel-debug-summary.tsv",
        "status\tcalls\tstates\tchanges\tinputs\tinput_rows\tdelta_rows\tresource_rows\tresource_changes\tstage\tshader\tscope\n" +
        Row("finished_empty_batch", calls, total_states, total_changes, input_count, inputs.rows-1, deltas.rows-1,
            resource_rows.rows-1, resource_changes, unsigned(ShaderStage::Pixel), shader,
            "requested_fragment_not_verified_interpreter_not_original_execution_not_hardware_lod") + "\n");
    BindingLogSnapshot(s);
  } catch (...) {
    // Diagnostic-only prefix, never success filenames or rejected record values.
    // Each state header was committed atomically after its complete validation.
    try { s.Write("failure-prefix-states.tsv", states.data); } catch (...) {}
    try {
      s.Write("failure-prefix-progress.tsv", "code\tcalls\tstates\tchanges\tinput_count\tlast_step\tlast_next_instruction\tscope\n" +
          DebugFailureProgress(failure_code, calls, total_states, total_changes, input_count, last_validated_next) + "\n");
    } catch (...) {}
    // Preserve bounded diagnostic log on failure as well, never publish COMPLETE.
    try { BindingLogSnapshot(s); } catch (...) {}
    throw;
  }
}
void Preview(State& s, IReplayController& c, uint32_t event, const std::vector<uint64_t>& ids) {
  struct Item { const TextureDescription* texture; CompType cast; };
  std::vector<Item> items;
  uint64_t estimated = s.output_bytes + 65536; // selection/receipt headroom
  for (const auto id : ids) {
    const TextureDescription* selected = nullptr;
    for (const auto& t : c.GetTextures()) if (Id(t.resourceId) == id) selected = &t;
    Require(selected != nullptr, "preview resource is not a captured texture");
    const auto cast = PreviewCast(*selected);
    const uint64_t estimate = (uint64_t(selected->width) * selected->height * 8 + 65536) *
        (PreviewAlpha(selected->format) ? 2 : 1);
    Require(PreviewBudget(estimate, estimated), "aggregate preview PNG estimate exceeds budget");
    estimated += estimate; items.push_back({selected, cast});
  }
  // Exactly one event selection, one replay session, no implicit candidate sweep.
  c.SetFrameEvent(event, true); Require(s.Ident() == 0, "listener before previews");
  Text manifest;
  manifest.Add("event\tresource\tmip\tslice\tsample\twidth\theight\tnative_mips\tformat_type\tcomponents\tcomponent_bytes\tcomponent_type\tbgra\tsrgb\ttype_cast\tblack_point\twhite_point\talpha\tcolor_channel\tcolor_file\talpha_channel\talpha_file");
  for (const auto& item : items) {
    const auto& t = *item.texture;
    const auto name = "texture-" + std::to_string(Id(t.resourceId)) + "-mip0.png";
    const bool alpha = PreviewAlpha(t.format);
    const auto alpha_name = alpha ? "texture-" + std::to_string(Id(t.resourceId)) + "-alpha.png" : "none";
    TextureSave save{}; save.resourceId = t.resourceId; save.mip = 0; save.slice.sliceIndex = 0;
    save.sample.sampleIndex = 0; save.destType = FileType::PNG; save.alpha = AlphaMapping::Preserve;
    save.typeCast = item.cast; save.comp.blackPoint = 0.0f; save.comp.whitePoint = 1.0f;
    const auto write_png = [&](const std::string& filename, int channel) {
      const auto path = s.output / filename;
      Require(GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND, "preview PNG collision");
      save.channelExtract = channel; // -1 preserves RGBA; 3 exposes alpha as grayscale.
      Require(c.SaveTexture(save, Utf8(path.wstring()).c_str()).OK(), "preview SaveTexture failed");
      auto pin = s.PinFile(path, kOutputBudget - s.output_bytes); LARGE_INTEGER size{};
      Require(GetFileSizeEx(pin.h, &size) && size.QuadPart > 0, "preview PNG empty");
      s.output_bytes += uint64_t(size.QuadPart); s.output_names.push_back(filename); s.pins.push_back(std::move(pin));
    };
    write_png(name, -1);
    if (alpha) write_png(alpha_name, 3);
    manifest.Add(Row(event, Id(t.resourceId), 0, 0, 0, t.width, t.height, t.mips,
        unsigned(t.format.type), unsigned(t.format.compCount), unsigned(t.format.compByteWidth), unsigned(t.format.compType),
        t.format.BGRAOrder(), t.format.SRGBCorrected(), item.cast == CompType::UNorm ? "UNorm" : "native",
        0, 1, "preserve", "rgba", name, alpha ? "alpha" : "none", alpha_name));
  }
  s.Write("previews.tsv", manifest.data);
}
void FetchBiasObservation(State& s, IReplayController& c, uint32_t event) {
  c.SetFrameEvent(event, true);
  const auto* pipeline = c.GetD3D12PipelineState();
  Require(pipeline && Id(pipeline->pixelShader.resourceId) &&
      pipeline->pixelShader.stage == ShaderStage::Pixel &&
      pipeline->rootSignature.parameters.size() <= 64, "fetch bias pixel pipeline rejected");
  const auto* reflection = pipeline->pixelShader.reflection;
  Require(reflection && reflection->constantBlocks.size() <= 64, "fetch bias reflection absent/budget");
  const ConstantBlock* block = nullptr;
  for (const auto& b : reflection->constantBlocks) if (b.name == "xe_fetch_cbuffer") {
    Require(!block, "duplicate fetch block"); block = &b;
  }
  Require(block && block->bufferBacked && !block->inlineDataBytes && !block->compileConstants &&
      block->fixedBindNumber == 3 && block->fixedBindSetOrSpace == 0 && block->bindArraySize == 1 &&
      block->byteSize == 768 && block->variables.size() == 1, "fetch block identity rejected");
  const auto& variable = block->variables[0]; const auto& type = variable.type;
  Require(variable.name == "xe_fetch_constants" && variable.byteOffset == 0 && !variable.bitFieldSize &&
      !variable.bitFieldOffset && type.baseType == VarType::UInt && type.rows == 1 && type.columns == 4 &&
      type.members.empty() && type.elements == 48 && type.arrayByteStride == 16, "fetch uint4[48] layout rejected");
  const D3D12Pipe::RootParam* cbv = nullptr;
  for (const auto& p : pipeline->rootSignature.parameters) {
    Require(p.tableRanges.size() <= 32, "fetch root range budget exceeded");
    if (bool(p.visibility & ShaderStageMask::Pixel)) for (const auto& range : p.tableRanges) {
      Require(!(range.category == DescriptorCategory::ConstantBlock && range.space == 0 &&
          range.baseRegister <= 3 && uint64_t(3 - range.baseRegister) < range.count),
          "fetch root CBV also covered by descriptor table");
    }
    if (bool(p.visibility & ShaderStageMask::Pixel) && p.reg == 3 && p.space == 0 &&
        p.descriptor.type == DescriptorType::ConstantBuffer) {
      Require(!cbv && Id(p.descriptor.resource) && p.tableRanges.empty() && !Id(p.heap) && p.constants.empty(), "fetch CBV ambiguous"); cbv = &p;
    }
  }
  Require(cbv, "fetch Pixel b3 root CBV absent");
  const auto& buffers = c.GetBuffers(); Require(buffers.size() <= kRows, "fetch buffer inventory budget");
  const BufferDescription* buffer = nullptr;
  for (const auto& b : buffers) if (b.resourceId == cbv->descriptor.resource) {
    Require(!buffer, "duplicate fetch buffer"); buffer = &b;
  }
  Require(buffer && FetchBiasBounds(cbv->descriptor.byteOffset, cbv->descriptor.byteSize, buffer->length),
      "fetch full reflected block bounds rejected");
  auto bytes = c.GetBufferData(buffer->resourceId, cbv->descriptor.byteOffset + 80, 16);
  Require(bytes.size() == 16, "fetch vector5 read incomplete");
  std::array<uint32_t, 4> words{}; std::memcpy(words.data(), bytes.data(), 16);
  s.Write("fetch-bias.tsv", "event\tstage\tshader\tblock\tvariable\tregister\tspace\tblock_bytes\tbuffer\tbuffer_bytes\tcb_offset\tcb_bytes\tvector\tread_offset\tread_bytes\tx\ty\tz\tw\tsigned_units\tdenominator\tscope\n" +
      Row(event, "Pixel", Id(pipeline->pixelShader.resourceId), "xe_fetch_cbuffer", "xe_fetch_constants",
          3, 0, 768, Id(buffer->resourceId), buffer->length, cbv->descriptor.byteOffset, cbv->descriptor.byteSize,
          5, cbv->descriptor.byteOffset + 80, 16, words[0], words[1], words[2], words[3], FetchBiasUnits(words[2]),
          32, "guest_fetch_bias_not_actual_hardware_lod") + "\n");
  s.Write("event-selection.tsv", "event\trequired_action\n" + Row(event, "drawcall") + "\n");
}
int SelfTest() {
  unsigned n = 0;
  auto check = [&](bool b) { ++n; Require(b, "self-test failed"); };
  check(FetchBiasMode(5, L"--fetch-bias")); check(!FetchBiasMode(4, L"--fetch-bias"));
  check(!FetchBiasMode(6, L"--fetch-bias")); check(!FetchBiasMode(5, L"--fetch-bias "));
  check(!FetchBiasMode(5, L"--bindings"));
  for (uint32_t bits = 0; bits < 1024; ++bits) {
    const int32_t expected = bits < 512 ? int32_t(bits) : int32_t(bits) - 1024;
    check(FetchBiasUnits(bits << 12) == expected);
    check(FetchBiasUnits((bits << 12) | 0xFFC00FFFu) == expected);
  }
  check(FetchBiasBounds(0, 768, 768)); check(FetchBiasBounds(256, 768, 1024));
  check(!FetchBiasBounds(0, 767, 768)); check(!FetchBiasBounds(0, 768, 767));
  check(!FetchBiasBounds(257, 768, 1024)); check(!FetchBiasBounds(UINT64_MAX, 768, UINT64_MAX));
  check(!FetchBiasBounds(256, UINT64_MAX, UINT64_MAX)); check(!FetchBiasBounds(10, 768, 9));
  check(PlainPath(L"C:\\safe\\one.rdc")); check(!PlainPath(L"C:\\safe\\..\\one.rdc"));
  check(!PlainPath(L"C:\\safe\\one.rdc:ads")); check(!PlainPath(L"\\\\server\\share\\one.rdc"));
  check(!PlainPath(L"relative.rdc")); check(!PlainPath(L"C:\\safe\\tail."));
  check(Number(L"42", 100) == 42); check(Number(L"0", 100) == 0);
  for (auto invalid : {L"01", L"-1", L"1 ", L"101", L"99999999999999999999"}) {
    bool rejected = false; try { (void)Number(invalid, 100); } catch (...) { rejected = true; } check(rejected);
  }
  check(Escape("a\tb\n\\") == "a\\x09b\\x0A\\\\");
  check(PreviewIds(L"1") == std::vector<uint64_t>{1});
  check(PreviewIds(L"5,7,18446744073709551615") == std::vector<uint64_t>({5,7,UINT64_MAX}));
  std::wstring sixteen;
  for (unsigned i = 1; i <= 16; ++i) { if (i != 1) sixteen += L','; sixteen += std::to_wstring(i); }
  check(PreviewIds(sixteen).size() == 16);
  for (const auto invalid : {L"", L"0", L"01", L"1,1", L"1,", L",1", L"1,,2", L"1, 2", L"1,2\n", L"-2", L"+2", L"0x2", L"18446744073709551616"}) {
    bool rejected = false; try { (void)PreviewIds(invalid); } catch (...) { rejected = true; } check(rejected);
  }
  { bool rejected = false; try { (void)PreviewIds(sixteen + L",17"); } catch (...) { rejected = true; } check(rejected); }
  check(PreviewBudget(kOutputBudget, 0)); check(PreviewBudget(1, kOutputBudget - 1));
  check(!PreviewBudget(1, kOutputBudget)); check(!PreviewBudget(0, kOutputBudget + 1));
  check(!PreviewBudget(UINT64_MAX, 1));
  check(SelectedDraw(42, ActionFlags::Drawcall, 42));
  check(!SelectedDraw(41, ActionFlags::Drawcall, 42));
  check(!SelectedDraw(0, ActionFlags::Drawcall, 0));
  check(!SelectedDraw(42, ActionFlags::Present, 42));
  check(!SelectedDraw(42, ActionFlags::Dispatch, 42));
  check(SelectedDispatch(42, ActionFlags::Dispatch, 42)); check(!SelectedDispatch(41, ActionFlags::Dispatch, 42));
  check(!SelectedDispatch(0, ActionFlags::Dispatch, 0)); check(!SelectedDispatch(42, ActionFlags::Drawcall, 42));
  check(!SelectedDispatch(42, ActionFlags::Present, 42)); check(!SelectedDispatch(42, ActionFlags::Dispatch | ActionFlags::Drawcall, 42));
  check(CopyLinkMode(6, L"--copy-link")); check(!CopyLinkMode(5, L"--copy-link")); check(!CopyLinkMode(7, L"--copy-link"));
  check(!CopyLinkMode(6, L"--copy-link ")); check(!CopyLinkMode(6, L"--bindings"));
  check(SelectedCopy(42, ActionFlags::Copy, 42, 515, 514, 514));
  check(!SelectedCopy(41, ActionFlags::Copy, 42, 515, 514, 514));
  check(!SelectedCopy(0, ActionFlags::Copy, 0, 515, 514, 514));
  check(!SelectedCopy(42, ActionFlags::Copy, 42, 0, 514, 514));
  check(!SelectedCopy(42, ActionFlags::Copy, 42, 515, 0, 0));
  check(!SelectedCopy(42, ActionFlags::Copy, 42, 515, 514, 513));
  check(!SelectedCopy(42, ActionFlags::Resolve, 42, 515, 514, 514));
  check(!SelectedCopy(42, ActionFlags::Copy | ActionFlags::Drawcall, 42, 515, 514, 514));
  check(!SelectedCopy(42, ActionFlags::Copy | ActionFlags::Dispatch, 42, 515, 514, 514));
  check(CopyUsageBudget(0, 0)); check(CopyUsageBudget(4096, 4096));
  check(!CopyUsageBudget(4097, 0)); check(!CopyUsageBudget(2, 3));
  check(ComputeBindingStage(1, ShaderStage::Compute)); check(!ComputeBindingStage(0, ShaderStage::Compute));
  check(!ComputeBindingStage(1, ShaderStage::Pixel));
  check(BindingResourceType(ShaderStage::Compute, DescriptorType::Buffer));
  check(BindingResourceType(ShaderStage::Compute, DescriptorType::ReadWriteBuffer));
  check(BindingResourceType(ShaderStage::Compute, DescriptorType::ReadWriteImage));
  check(!BindingResourceType(ShaderStage::Pixel, DescriptorType::ReadWriteBuffer));
  check(!BindingResourceType(ShaderStage::Compute, DescriptorType::ConstantBuffer));
  check(!BindingResourceType(ShaderStage::Compute, DescriptorType::Sampler));
  check(HistoryPoint(1280, 512, 335, 157, 0)); check(!HistoryPoint(1280, 512, 1280, 157, 0));
  check(!HistoryPoint(1280, 512, 335, 512, 0)); check(!HistoryPoint(1280, 512, 335, 157, UINT32_MAX));
  check(!HistoryPoint(0, 512, 0, 0, 0)); check(HistoryUsage(42, 42));
  check(!HistoryUsage(0, 42)); check(!HistoryUsage(43, 42));
  check(HistoryBudget(4096, 8192)); check(!HistoryBudget(4097, 0)); check(!HistoryBudget(0, 8193));
  check(DebugBudget(64, 16384, 131072)); check(!DebugBudget(65, 1, 1));
  check(!DebugBudget(1, 16385, 1)); check(!DebugBudget(1, 1, 131073));
  check(DebugSelection(21355, 7542, 335, 157, 0, 627));
  check(!DebugSelection(0, 7542, 335, 157, 0, 627)); check(!DebugSelection(1, 0, 0, 0, 0, 0));
  check(!DebugSelection(1, 1, 8192, 0, 0, 0)); check(!DebugSelection(1, 1, 0, 8192, 0, 0));
  check(!DebugSelection(1, 1, 0, 0, UINT32_MAX, 0)); check(!DebugSelection(1, 1, 0, 0, 0, UINT32_MAX));
  check(DebugStep(0, 0)); check(DebugStep(16383, 16383)); check(!DebugStep(2, 1)); check(!DebugStep(16384, 16384));
  check(DebugVariableShape("r24", VarType::UInt, 1, 4, 0)); check(DebugVariableShape("v0", VarType::Float, 1, 2, 0));
  check(DebugVariableShape("", VarType::Unknown, 0, 0, 0));
  // Native RED was step0/next0/change0/after Unknown1x4 memberless r0.
  // makeReg's typeless DXBC words are present, not an absent variable or Float.
  check(DebugVariableShape("r0", VarType::Unknown, 1, 4, 0));
  check(DebugVariableShape("r29", VarType::Unknown, 1, 4, 0)); check(DebugVariableShape("o0", VarType::Unknown, 1, 4, 0));
  check(DebugVariableShape("r4095", VarType::Unknown, 1, 4, 0)); check(DebugVariableShape("o31", VarType::Unknown, 1, 4, 0));
  check(!DebugAbsentVariable("r0", VarType::Unknown, 1, 4, 0));
  check(!DebugVariableShape("r0", VarType::Unknown, 0, 0, 1)); check(!DebugVariableShape("r0", VarType::Unknown, 1, 4, 1));
  check(!DebugVariableShape("", VarType::Unknown, 1, 4, 0)); check(!DebugVariableShape("v0", VarType::Unknown, 1, 4, 0));
  check(!DebugVariableShape("r0", VarType::Unknown, 1, 3, 0));
  for (const auto name : {"r", "o", "r00", "r01", "r-1", "r+1", "r4096", "r99999", "o32", "R0", "r0.x", "r0 "})
    check(!DebugVariableShape(name, VarType::Unknown, 1, 4, 0));
  check(!DebugVariableShape("r24", VarType::Unknown, 0, 0, 0)); check(!DebugVariableShape("", VarType::UInt, 1, 4, 0));
  check(!DebugVariableShape("r0", VarType::UInt, 2, 4, 0)); check(!DebugVariableShape("r0", VarType::UInt, 1, 5, 0));
  check(!DebugVariableShape("r0", VarType::UInt, 1, 4, 1)); check(!DebugVariableShape("r\t0", VarType::UInt, 1, 4, 0));
  check(!DebugVariableShape(std::string(65, 'r'), VarType::UInt, 1, 4, 0));
  for (auto type : {VarType::GPUPointer, VarType::Double, VarType::Half, VarType::ReadOnlyResource,
      VarType::ReadWriteResource, VarType::Sampler, VarType::ConstantBlock, VarType::Struct})
    check(!DebugVariableShape("unsupported", type, 1, 1, 0));
  check(DebugResourceRef(VarType::ReadOnlyResource, 0, DescriptorCategory::ReadOnlyResource, 0, 1, 4, 5));
  check(DebugResourceRef(VarType::ReadWriteResource, 0, DescriptorCategory::ReadWriteResource, 0, 1, 0, 1));
  check(!DebugResourceRef(VarType::ReadOnlyResource, 1, DescriptorCategory::ReadOnlyResource, 0, 1, 0, 1));
  check(!DebugResourceRef(VarType::ReadOnlyResource, 2, DescriptorCategory::ReadOnlyResource, 0, 1, 0, 1));
  check(!DebugResourceRef(VarType::ReadOnlyResource, 0, DescriptorCategory::ReadWriteResource, 0, 1, 0, 1));
  check(!DebugResourceRef(VarType::GPUPointer, 0, DescriptorCategory::ReadOnlyResource, 0, 1, 0, 1));
  check(!DebugResourceRef(VarType::ReadOnlyResource, 0, DescriptorCategory::ReadOnlyResource, 1, 1, 0, 1));
  check(!DebugResourceRef(VarType::ReadOnlyResource, 0, DescriptorCategory::ReadOnlyResource, 0, 1, 1, 1));
  check(!DebugResourceRef(VarType::ReadOnlyResource, 0, DescriptorCategory::ReadOnlyResource, 0, 1, 0, 0));
  check(DebugHex(0x3F800000) == "3F800000"); check(DebugHex(UINT32_MAX) == "FFFFFFFF");
  check(DebugFailureProgress(DebugFailureCode::ZeroStates, 1, 0, 0, 3, 999) ==
      "zero_states\t1\t0\t0\t3\tnone\tnone\tvalidated_state_headers_only_incomplete_not_success");
  check(DebugFailureProgress(DebugFailureCode::CallBudget, 64, 512, 1024, 3, 418) ==
      "call_budget\t64\t512\t1024\t3\t511\t418\tvalidated_state_headers_only_incomplete_not_success");
  check(DebugFailureProgress(DebugFailureCode::Validation, 2, 1, 31, 3, 0).find("validation_or_output_failure\t2\t1\t31\t3\t0\t0\t") == 0);
  check(DebugFailureProgress(DebugFailureCode::CallBudget, 64, 16384, 131072, 128, UINT32_MAX).size() < 256);
  {
    const auto metadata = DebugVariableFailure(7, 418, 3, "after", 255, 0, 0, 2, "r0\t\n\\");
    check(metadata.find("step=7 next=418 change=3 phase=after type=255 rows=0 columns=0 members=2 name_length=5") != std::string::npos);
    check(metadata.find("name_prefix=r0\\x09\\x0A\\\\ name_truncated=0") != std::string::npos);
    check(metadata.find_first_of("\t\r\n") == std::string::npos);
    const auto bounded = DebugVariableFailure(UINT32_MAX, UINT32_MAX, SIZE_MAX, "abcdefghijk", UINT32_MAX,
        UINT32_MAX, UINT32_MAX, SIZE_MAX, std::string(1024, char(1)));
    check(bounded.size() <= 480); check(bounded.find("phase=abcdefgh ") != std::string::npos);
    check(bounded.find("name_length=1024") != std::string::npos);
    check(bounded.find(char(1)) == std::string::npos);
    const auto long_name = DebugVariableFailure(0, 0, 0, "before", 0, 1, 4, 0, std::string(65, 'R'));
    check(long_name.find(std::string(65, 'R')) == std::string::npos);
    check(long_name.find("name_truncated=1") != std::string::npos);
  }
  check(DebugFloat(0x3F800000) == "1"); check(DebugFloat(0xBF800000) == "-1");
  check(DebugFloat(0x7FC00000) == "nan"); check(DebugFloat(0x7F800000) == "+inf"); check(DebugFloat(0xFF800000) == "-inf");
  PixelModification modification{};
  check(HistoryCandidate(ActionFlags::Drawcall, modification)); check(!HistoryCandidate(ActionFlags::Clear, modification));
  for (auto member : {&PixelModification::directShaderWrite, &PixelModification::unboundPS, &PixelModification::sampleMasked,
      &PixelModification::backfaceCulled, &PixelModification::depthClipped, &PixelModification::depthBoundsFailed,
      &PixelModification::viewClipped, &PixelModification::scissorClipped, &PixelModification::shaderDiscarded,
      &PixelModification::depthTestFailed, &PixelModification::stencilTestFailed, &PixelModification::predicationSkipped}) {
    auto rejected = modification; rejected.*member = true; check(!HistoryCandidate(ActionFlags::Drawcall, rejected));
  }
  check(CandidateReadBounds(16, 1024, 1040)); check(!CandidateReadBounds(16, 1024, 1039));
  check(!CandidateReadBounds(0, 0, 1024)); check(!CandidateReadBounds(0, 1028, 2048));
  check(!CandidateReadBounds(UINT64_MAX, 4, UINT64_MAX)); check(!CandidateReadBounds(0, 3, 10));
  uint32_t candidate_offset = 0;
  check(CandidateDescriptorOffset(0, 20, 0, UINT32_MAX, 0, UINT32_MAX, 5, 0, 1, 100, candidate_offset) && candidate_offset == 25);
  check(CandidateDescriptorOffset(32, 32, 1, 9, 2, 8, 2, 0, 32, 20, candidate_offset) && candidate_offset == 160);
  check(!CandidateDescriptorOffset(0, 20, 0, UINT32_MAX, 0, UINT32_MAX, 80, 0, 1, 100, candidate_offset));
  check(!CandidateDescriptorOffset(0, 20, 0, UINT32_MAX, 0, 1, 1, 0, 1, 100, candidate_offset));
  check(!CandidateDescriptorOffset(0, 20, 0, 5, 0, UINT32_MAX, 5, 0, 1, 100, candidate_offset));
  check(!CandidateDescriptorOffset(0, 20, 1, UINT32_MAX, 0, UINT32_MAX, 0, 0, 1, 100, candidate_offset));
  check(!CandidateDescriptorOffset(0, 1, 0, UINT32_MAX, 0, UINT32_MAX, 0, 0, 32, 100, candidate_offset));
  check(!CandidateDescriptorOffset(UINT32_MAX, 1, 0, UINT32_MAX, 0, UINT32_MAX, 0, 0, 1, 100, candidate_offset));
  check(!CandidateDescriptorOffset(0, 0, 0, UINT32_MAX, 0, UINT32_MAX, UINT32_MAX-1, 0, 32, UINT32_MAX, candidate_offset));
  check(!CandidateDescriptorOffset(0, 0, 0, UINT32_MAX, 0, UINT32_MAX, 0, 0, 0, 100, candidate_offset));
  check(LogTailOffset(0) == 0); check(LogTailOffset(1048576) == 0); check(LogTailOffset(1048577) == 1);
  check(LogTailOffset(UINT64_MAX) == UINT64_MAX - 1048576);
  TextureDescription fixture{}; fixture.width = fixture.height = 512; fixture.depth = fixture.dimension = 1;
  fixture.dimension = 2; fixture.arraysize = fixture.msSamp = fixture.mips = 1; fixture.byteSize = 1048576;
  fixture.format.type = ResourceFormatType::Regular; fixture.format.compCount = 4; fixture.format.compByteWidth = 1;
  fixture.format.compType = CompType::Typeless;
  {
    auto target = fixture; target.msSamp = 4; target.creationFlags = TextureCategory::ColorTarget;
    target.format.compByteWidth = 2; target.format.compType = CompType::Float;
    AdmitColorTarget(target, 0); check(true);
    check(TargetSelection(42, 7542, target, 0).find("42\t7542\t0\t0\t4\taverage\t512\t512\t1\t0\t4\t2\t1\tnative\t0\t1\trgba\tpreserve\ttarget.png\n") != std::string::npos);
    for (unsigned change = 0; change < 10; ++change) {
      auto bad = target;
      if (change == 0) bad.creationFlags = TextureCategory::DepthTarget;
      if (change == 1) bad.arraysize = 2;
      if (change == 2) bad.msSamp = 1;
      if (change == 3) bad.mips = 2;
      if (change == 4) bad.format.compType = CompType::Typeless;
      if (change == 5) bad.format.compByteWidth = 4;
      if (change == 6) bad.format.compCount = 3;
      if (change == 7) bad.width = 0;
      if (change == 8) bad.byteSize = kOutputBudget;
      bool rejected = false; try { AdmitColorTarget(bad, change == 9 ? 1 : 0); } catch (...) { rejected = true; } check(rejected);
    }
  }
  check(PreviewCast(fixture) == CompType::UNorm);
  fixture.format.compType = CompType::UNorm; check(PreviewCast(fixture) == CompType::Typeless);
  fixture.format.compType = CompType::UNormSRGB; check(PreviewCast(fixture) == CompType::Typeless);
  check(PreviewAlpha(fixture.format));
  fixture.format.compCount = 1; check(!PreviewAlpha(fixture.format)); fixture.format.compCount = 4;
  for (const auto format : {ResourceFormatType::BC2, ResourceFormatType::BC3, ResourceFormatType::BC7}) {
    auto compressed = fixture; compressed.format.type = format;
    check(PreviewCast(compressed) == CompType::Typeless); check(PreviewAlpha(compressed.format));
    for (unsigned components = 0; components < 4; ++components) {
      compressed.format.compCount = uint8_t(components);
      bool rejected = false; try { (void)PreviewCast(compressed); } catch (...) { rejected = true; }
      check(rejected); check(!PreviewAlpha(compressed.format));
    }
  }
  for (unsigned change = 0; change < 6; ++change) {
    auto bad = fixture;
    if (change == 0) bad.format.compType = CompType::Float;
    if (change == 1) bad.format.type = ResourceFormatType::D24S8;
    if (change == 2) bad.arraysize = 2;
    if (change == 3) bad.msSamp = 4;
    if (change == 4) bad.width = 0;
    if (change == 5) bad.creationFlags = TextureCategory::DepthTarget;
    bool rejected = false; try { (void)PreviewCast(bad); } catch (...) { rejected = true; } check(rejected);
  }
  check(GetProcAddress(GetModuleHandleW(nullptr), "renderdoc__replay__marker") != nullptr);
  std::cout << "SELF_TEST " << n << " PASS; no DLL loaded\n"; return 0;
}
} // namespace

int wmain(int argc, wchar_t** argv) {
  State* s = nullptr;
  try {
    if (argc == 2 && std::wstring_view(argv[1]) == L"--self-test") return SelfTest();
    const bool bootstrap = argc == 2 && std::wstring_view(argv[1]) == L"--bootstrap";
    const bool inventory = argc == 4 && std::wstring_view(argv[1]) == L"--inventory";
    const bool extract = argc == 7 && std::wstring_view(argv[1]) == L"--extract";
    const bool preview = argc == 6 && std::wstring_view(argv[1]) == L"--preview";
    const bool bindings = argc == 5 && std::wstring_view(argv[1]) == L"--bindings";
    const bool dispatch_bindings = argc == 5 && std::wstring_view(argv[1]) == L"--dispatch-bindings";
    const bool copy_link = argc > 1 && CopyLinkMode(argc, argv[1]);
    const bool fetch_bias = argc > 1 && FetchBiasMode(argc, argv[1]);
    const bool target_preview = argc == 7 && std::wstring_view(argv[1]) == L"--target-preview";
    const bool pixel_history = argc == 8 && std::wstring_view(argv[1]) == L"--pixel-history";
    const bool pixel_debug = argc == 9 && std::wstring_view(argv[1]) == L"--pixel-debug";
    Require(bootstrap || inventory || extract || preview || bindings || target_preview || pixel_history || pixel_debug || dispatch_bindings || copy_link || fetch_bias,
        "usage: --bootstrap | --inventory RDC NEWDIR | --extract RDC NEWDIR EVENT RESOURCE MIP | --preview RDC NEWDIR EVENT CSV_IDS | --bindings RDC NEWDIR EVENT | --dispatch-bindings RDC NEWDIR EVENT | --fetch-bias RDC NEWDIR EVENT | --copy-link RDC NEWDIR EVENT DESTINATION | --target-preview RDC NEWDIR EVENT RESOURCE SLICE | --pixel-history RDC NEWDIR EVENT RESOURCE X Y | --pixel-debug RDC NEWDIR EVENT RESOURCE X Y PRIMITIVE | --self-test");
    const uint32_t event = (extract || preview || bindings || target_preview || pixel_history || pixel_debug || dispatch_bindings || copy_link || fetch_bias) ? uint32_t(Number(argv[4], UINT32_MAX)) : 0;
    const uint64_t resource = (extract || target_preview || pixel_history || pixel_debug || copy_link) ? Number(argv[5], UINT64_MAX) : 0;
    const uint32_t mip = extract ? uint32_t(Number(argv[6], 31)) : 0;
    const uint32_t slice = target_preview ? uint32_t(Number(argv[6], 0)) : 0;
    const uint32_t pixel_x = (pixel_history || pixel_debug) ? uint32_t(Number(argv[6], 8191)) : 0;
    const uint32_t pixel_y = (pixel_history || pixel_debug) ? uint32_t(Number(argv[7], 8191)) : 0;
    const uint32_t primitive = pixel_debug ? uint32_t(Number(argv[8], UINT32_MAX - 1)) : 0;
    const auto preview_ids = preview ? PreviewIds(argv[5]) : std::vector<uint64_t>{};
    Require(!(extract || preview || bindings || target_preview || pixel_history || pixel_debug || dispatch_bindings || copy_link || fetch_bias) || event, "event must be positive");
    Require(!(extract || target_preview || pixel_history || pixel_debug || copy_link) || resource, "resource must be positive");
    s = new State; // Deliberate process-lifetime containment even on failure after DLL load.
    fs::path input, output;
    if (bootstrap) {
      wchar_t temp[32768]{}; DWORD n = GetTempPathW(DWORD(std::size(temp)), temp);
      Require(n && n < std::size(temp), "TEMP unavailable");
      std::array<unsigned char, 16> random{}; Require(BCryptGenRandom(nullptr, random.data(), ULONG(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0, "bootstrap nonce failed");
      output = fs::path(temp) / ("mcla-renderdoc-replay-" + Hex(random.data(), random.size()));
    } else {
      input = argv[2]; output = argv[3];
      Require(input.extension() == L".rdc", "input extension must be .rdc");
      auto pin = s->PinFile(input, 4ull * 1024 * 1024 * 1024);
      LARGE_INTEGER size{}; Require(GetFileSizeEx(pin.h, &size) && size.QuadPart > 0, "empty RDC rejected");
      s->input_sha = Hash(pin.h, 4ull * 1024 * 1024 * 1024); s->pins.push_back(std::move(pin));
    }
    s->Fresh(output); std::wcout << L"PRIVATE_OUTPUT " << output.wstring() << L"\n";
    s->Write("request.tsv", "schema\tnonce\tmode\tinput_sha256\tdll_sha256\n" +
        Row("mcla-renderdoc-replay-v1", s->nonce, bootstrap ? "bootstrap" : extract ? "extract" : preview ? "preview" : bindings ? "bindings" : dispatch_bindings ? "dispatch-bindings" : copy_link ? "copy-link" : fetch_bias ? "fetch-bias" : target_preview ? "target-preview" : pixel_history ? "pixel-history" : pixel_debug ? "pixel-debug" : "inventory", s->input_sha, kDllSha) + "\n");
    s->Load();
    {
      Replay replay(*s);
      if (!bootstrap) {
        replay.Open(input); Inventory(*s, *replay.controller, event, extract || preview || bindings || target_preview || pixel_history || pixel_debug || dispatch_bindings || copy_link || fetch_bias, bindings || pixel_debug || fetch_bias, dispatch_bindings);
        if (fetch_bias) FetchBiasObservation(*s, *replay.controller, event);
        if (copy_link) CopyLinkObservation(*s, *replay.controller, event, resource);
        if (extract) Extract(*s, *replay.controller, event, resource, mip);
        if (preview) Preview(*s, *replay.controller, event, preview_ids);
        if (target_preview) TargetPreview(*s, *replay.controller, event, resource, slice);
        if (pixel_history) PixelHistoryObservation(*s, *replay.controller, event, resource, pixel_x, pixel_y);
        if (pixel_debug) PixelDebugObservation(*s, *replay.controller, event, resource, pixel_x, pixel_y, primitive);
        if (dispatch_bindings) {
          replay.controller->SetFrameEvent(event, true);
          BindingObservation(*s, *replay.controller, event, ShaderStage::Compute);
          Bindings(*s, *replay.controller, ShaderStage::Compute);
          s->Write("event-selection.tsv", "event\trequired_action\tstage\n" + Row(event, "dispatch", "Compute") + "\n");
        }
        if (bindings) {
          replay.controller->SetFrameEvent(event, true);
          BindingObservation(*s, *replay.controller, event); Bindings(*s, *replay.controller);
          EngineDescriptorCandidates(*s, *replay.controller, event);
          PixelDisassembly(*s, *replay.controller);
          BindingLogSnapshot(*s);
          s->Write("event-selection.tsv", "event\trequired_action\n" + Row(event, "drawcall") + "\n");
        }
      }
      Require(s->Ident() == 0, "listener at end of operation"); replay.Close();
    }
    Require(s->Ident() == 0, "listener after shutdown");
    Text complete; complete.Add("MCLA-RENDERDOC-REPLAY-COMPLETE-V1");
    complete.Add(Row("nonce", s->nonce)); complete.Add(Row("input_sha256", s->input_sha)); complete.Add(Row("dll_sha256", kDllSha));
    complete.Add("scope\tlocal replay observation only; no graphics PASS or root cause claim");
    complete.Add("unsealed_local_log\trenderdoc.log; supervisor must bound log/total disk use");
    for (const auto& name : s->output_names) {
      auto p = s->PinFile(s->output / name, kOutputBudget); LARGE_INTEGER size{};
      Require(GetFileSizeEx(p.h, &size), "output size failed"); complete.Add(Row("artifact", name, size.QuadPart, Hash(p.h, kOutputBudget)));
      s->pins.push_back(std::move(p));
    }
    s->Write("COMPLETE.tsv", complete.data);
    std::cout << "LOCAL_REPLAY_COMPLETE_NO_GRAPHICS_CLAIM\n"; return 0;
  } catch (const std::exception& e) {
    Failure(s, e.what());
    std::cerr << "REPLAY_FAILED: " << e.what() << "\nPreserve partial output; no automatic retry.\n"; return 1;
  } catch (...) { Failure(s, "unknown exception"); std::cerr << "REPLAY_FAILED: unknown exception\n"; return 1; }
}
