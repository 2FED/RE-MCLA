// Standalone C++23 stdlib-only numerical protocol/ledger checks. No game, SDK,
// packet/ring IO, clock, process, file, thread or resource operations.
#include "../src/mcla_swap_emission_core.h"

#include <iostream>

namespace {
namespace swap = rex::diagnostics::swap_emission;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
constexpr std::array<uint32_t, 4> stock = {0x01020304, 0x11223344, 0xFFFFFFFF, 0};
constexpr std::array<uint32_t, 6> fetch = {1, 2, 3, 4, 5, 6};
constexpr uint64_t epoch = 0xFEDCBA9876543210ull;
swap::Snapshot Available() { return {110, 111, 7, 0x82130004, 0x1000, 0, 0x7FC00042, true}; }
bool Clear(const swap::Payload& value) {
  if (value.length) return false;
  for (auto word : value.words) if (word) return false;
  return true;
}
bool Clear(const swap::Decoded& value) {
  return value.stock == std::array<uint32_t, 4>{} && value.token == swap::Token{} && !value.extended;
}
bool Clear(const swap::Match& value) {
  return value.token == swap::Token{} && value.snapshot == swap::Snapshot{} &&
         value.snapshot_result == swap::SnapshotResult::kUnavailable;
}
bool Clear(const swap::Emission& value) {
  return Clear(value.payload) && value.token == swap::Token{} && value.snapshot == swap::Snapshot{} &&
         value.snapshot_result == swap::SnapshotResult::kUnavailable;
}

void ProtocolTests() {
  swap::Payload payload; payload.words.fill(42); payload.length = 56;
  Check(swap::Encode(stock, {epoch, 1}, payload) == swap::Result::kLegacyDisabled,
        "default-off encoding legacy");
  Check(payload.length == 4, "legacy exact count");
  for (size_t i = 0; i < 4; ++i) Check(payload.words[i] == stock[i], "legacy original word unchanged");
  for (size_t i = 4; i < payload.words.size(); ++i) Check(payload.words[i] == 0, "legacy trailing words cleared");
  swap::Decoded decoded;
  Check(swap::Decode(payload.view(), decoded) == swap::Result::kLegacyPacket && !decoded.extended,
        "legacy distinct from diagnostic token");
  Check(decoded.stock == stock && decoded.token == swap::Token{}, "legacy words decode without invented token");
  const swap::Token token{epoch, 0x123456789ABCDEF0ull};
  Check(swap::Encode(stock, token, payload, true) == swap::Result::kTokenIssued, "extended encode");
  Check(payload.length == 11 && payload.words[4] == swap::kMagic && payload.words[5] == 1 &&
        payload.words[6] == 11, "exact extension schema");
  Check(payload.words[7] == 0xFEDCBA98 && payload.words[8] == 0x76543210 &&
        payload.words[9] == 0x12345678 && payload.words[10] == 0x9ABCDEF0,
        "epoch and serial high/low numerical words");
  Check(swap::Decode(payload.view(), decoded) == swap::Result::kExtendedPacket &&
        decoded.stock == stock && decoded.token == token && decoded.extended, "extended roundtrip");
  for (size_t i = 0; i < 4; ++i) Check(payload.words[i] == stock[i], "extension preserves original stock word");
  Check(swap::Encode(std::span(stock).first(3), token, payload, true) == swap::Result::kInvalidStockWordCount &&
        Clear(payload), "invalid stock count clears output");
  for (const swap::Token bad : {swap::Token{0, 1}, swap::Token{epoch, 0}})
    Check(swap::Encode(stock, bad, payload, true) == swap::Result::kZeroToken && Clear(payload),
          "zero token rejected and cleared");
}

void DecodeRejectionTests() {
  swap::Payload payload;
  swap::Encode(stock, {epoch, 1}, payload, true);
  swap::Decoded decoded;
  for (size_t length = 0; length <= 56; ++length) {
    if (length == 4 || length == 11) continue;
    decoded.stock.fill(42); decoded.token = {1, 2}; decoded.extended = true;
    Check(swap::Decode({payload.words.data(), length}, decoded) == swap::Result::kUnsupportedPayloadLength &&
          Clear(decoded), "missing truncated or unsupported payload length clears decode");
  }
  std::array<uint32_t, 57> oversized{};
  Check(swap::Decode(oversized, decoded) == swap::Result::kPayloadTooLong && Clear(decoded), "oversized payload rejected before access");
  for (const auto index : {4u, 5u, 6u}) {
    auto altered = payload; altered.words[index] ^= 1;
    const auto expected = index == 4 ? swap::Result::kUnknownMagic :
                          index == 5 ? swap::Result::kUnknownVersion : swap::Result::kDeclaredLengthMismatch;
    Check(swap::Decode(altered.view(), decoded) == expected && Clear(decoded), "unknown/corrupt extension rejected");
  }
  auto altered = payload; altered.words[7] = altered.words[8] = 0;
  Check(swap::Decode(altered.view(), decoded) == swap::Result::kZeroToken && Clear(decoded), "zero epoch on wire");
  altered = payload; altered.words[9] = altered.words[10] = 0;
  Check(swap::Decode(altered.view(), decoded) == swap::Result::kZeroToken && Clear(decoded), "zero serial on wire");
}

void LedgerTests() {
  swap::Ledger<2> ledger(epoch, true);
  swap::Emission first, second, failed;
  const auto supplied = Available();
  Check(ledger.issue(stock, fetch, supplied, first) == swap::Result::kTokenIssued &&
        first.token == swap::Token{epoch, 1} && first.snapshot == supplied, "first exact issuance");
  Check(ledger.issue(stock, fetch, supplied, second) == swap::Result::kTokenIssued &&
        second.token.serial == 2, "serial monotonic without recycling");
  swap::Match match;
  Check(ledger.consume(first.payload.view(), fetch, match) == swap::Result::kTokenMatched &&
        match.token == first.token && match.snapshot == supplied, "stock/fetch/snapshot bound once");
  Check(ledger.consume(first.payload.view(), fetch, match) == swap::Result::kReplay && Clear(match), "replay clears output");
  Check(ledger.issue(stock, fetch, supplied, failed) == swap::Result::kCapacityExhausted && Clear(failed),
        "consumed capacity never recycled, normal-swap fallback required");
  auto changed = fetch; changed[5] ^= 1;
  Check(ledger.consume(second.payload.view(), changed, match) == swap::Result::kFetchMismatch && Clear(match),
        "changed sixth fetch word rejected");
  Check(ledger.consume(second.payload.view(), fetch, match) == swap::Result::kReplay && Clear(match),
        "failed binding consumes recognizable token, no repaired retry");
  swap::Ledger<1> off;
  Check(off.issue(stock, {}, supplied, first) == swap::Result::kLegacyDisabled &&
        first.payload.length == 4 && first.token == swap::Token{}, "disabled ledger does not require unused fetch or invent token");
  Check(off.consume(first.payload.view(), fetch, match) == swap::Result::kLegacyPacket && Clear(match),
        "legacy consumption carries no provenance");
  swap::Ledger<1> zero_epoch(0, true);
  Check(zero_epoch.issue(stock, fetch, supplied, first) == swap::Result::kInvalidEpoch && Clear(first), "invalid ledger epoch rejects");
}

void BindingRejectionTests() {
  const auto supplied = Available();
  for (size_t i = 0; i < 4; ++i) {
    swap::Ledger<1> ledger(epoch, true); swap::Emission issued; swap::Match match;
    ledger.issue(stock, fetch, supplied, issued); issued.payload.words[i] ^= 1;
    Check(ledger.consume(issued.payload.view(), fetch, match) == swap::Result::kStockMismatch && Clear(match),
          "each stock word must bind");
  }
  for (size_t i = 0; i < 6; ++i) {
    swap::Ledger<1> ledger(epoch, true); swap::Emission issued; swap::Match match;
    ledger.issue(stock, fetch, supplied, issued); auto changed = fetch; changed[i] ^= 1;
    Check(ledger.consume(issued.payload.view(), changed, match) == swap::Result::kFetchMismatch && Clear(match),
          "each fetch word must bind");
  }
  swap::Ledger<4> ledger(epoch, true); swap::Emission issued, rejected; swap::Match match;
  Check(ledger.issue(std::span(stock).first(3), fetch, supplied, rejected) == swap::Result::kInvalidStockWordCount && Clear(rejected),
        "issuer invalid stock clears");
  Check(ledger.issue(stock, std::span(fetch).first(5), supplied, rejected) == swap::Result::kInvalidFetchWordCount && Clear(rejected),
        "issuer invalid fetch clears");
  ledger.issue(stock, fetch, supplied, issued);
  auto changed = issued.payload; changed.words[8] ^= 1;
  Check(ledger.consume(changed.view(), fetch, match) == swap::Result::kTokenEpochMismatch && Clear(match), "wrong epoch never valid");
  changed = issued.payload; changed.words[9] = changed.words[10] = 0xFFFFFFFF;
  Check(ledger.consume(changed.view(), fetch, match) == swap::Result::kUnissuedToken && Clear(match), "max serial rejects before lookup");
  Check(ledger.consume(issued.payload.view(), std::span(fetch).first(5), match) == swap::Result::kInvalidFetchWordCount && Clear(match),
        "consumer missing fetch clears and burns recognizable token");
  Check(ledger.consume(issued.payload.view(), fetch, match) == swap::Result::kReplay && Clear(match), "missing-fetch retry not valid");
  swap::Payload invented; swap::Encode(stock, {epoch, 2}, invented, true);
  Check(ledger.consume(invented.view(), fetch, match) == swap::Result::kUnissuedToken && Clear(match), "encoded syntax not issuance proof");
  swap::Ledger<1> maximum_epoch(std::numeric_limits<uint64_t>::max(), true);
  Check(maximum_epoch.issue(stock, fetch, supplied, issued) == swap::Result::kTokenIssued && issued.token.serial == 1,
        "maximum epoch stays opaque, serial starts at one");
  Check(maximum_epoch.issue(stock, fetch, supplied, rejected) == swap::Result::kCapacityExhausted && Clear(rejected),
        "bounded issuance no serial wrap");
}

void SnapshotTests() {
  const auto available = Available();
  swap::Snapshot clean;
  Check(swap::NormalizeSnapshot(available, clean) == swap::SnapshotResult::kAvailable && clean == available,
        "opaque NaN bits and zero counter preserved, no numeric interpretation");
  auto absent = available; absent.snapshot_available = false;
  Check(swap::NormalizeSnapshot(absent, clean) == swap::SnapshotResult::kUnavailable && clean == swap::Snapshot{},
        "absent snapshot clears partial/stale fields");
  const swap::Snapshot invalids[] = {
      {0, 111, 7, 1, 1, 0, 0, true}, {112, 111, 7, 1, 1, 0, 0, true},
      {swap::kMaxQpc, swap::kMaxQpc + 1, 7, 1, 1, 0, 0, true},
      {110, 111, 0, 1, 1, 0, 0, true}, {110, 111, 7, 0, 1, 0, 0, true},
      {110, 111, 7, 1, 0, 0, 0, true}, absent};
  for (size_t i = 0; i < std::size(invalids); ++i) {
    swap::Ledger<1> ledger(epoch, true); swap::Emission issued; swap::Match match;
    auto rejected_snapshot = invalids[i];
    rejected_snapshot.callback_id = std::numeric_limits<uint64_t>::max();
    const auto expected = i < 3 ? swap::SnapshotResult::kInvalidQpc :
                          i < 6 ? swap::SnapshotResult::kMissingIdentity : swap::SnapshotResult::kUnavailable;
    Check(ledger.issue(stock, fetch, rejected_snapshot, issued) == swap::Result::kTokenIssued &&
          issued.snapshot_result == expected && issued.snapshot == swap::Snapshot{}, "transport issuance separate from invalid snapshot");
    Check(ledger.consume(issued.payload.view(), fetch, match) == swap::Result::kTokenMatched &&
          match.snapshot_result == expected && match.snapshot == swap::Snapshot{}, "unavailable snapshot is not timing provenance");
  }
  auto equal = available; equal.qpc_after = equal.qpc_before;
  Check(swap::NormalizeSnapshot(equal, clean) == swap::SnapshotResult::kAvailable, "equal QPC witness allowed");
  Check(!swap::kSimulationProven && !swap::kSnapshotCoherenceProven && !swap::kNativeAdmitted,
        "token matching never grants simulation/coherence/native admission");
}

void CallbackIdentityTests() {
  Check(swap::kSnapshotAbiVersion == 2 && swap::kVersion == 1 && swap::kExtendedWords == 11 &&
        swap::kMaximumPayloadWords == 56, "snapshot ABI v2 leaves token packet v1/counts unchanged");
  Check(offsetof(swap::Snapshot, callback_id) > offsetof(swap::Snapshot, snapshot_available),
        "explicit callback identity appended after existing snapshot fields");
  for (const uint64_t id : {uint64_t{0}, uint64_t{1}, std::numeric_limits<uint64_t>::max()}) {
    auto supplied = Available(); supplied.callback_id = id;
    swap::Snapshot clean;
    Check(swap::NormalizeSnapshot(supplied, clean) == swap::SnapshotResult::kAvailable && clean == supplied,
          "zero/positive/max callback IDs opaque in an available snapshot");
    auto in_place = supplied;
    Check(swap::NormalizeSnapshot(in_place, in_place) == swap::SnapshotResult::kAvailable && in_place == supplied,
          "callback ID normalization safe in place");
    swap::Ledger<1> ledger(epoch, true); swap::Emission issued; swap::Match match;
    Check(ledger.issue(stock, fetch, supplied, issued) == swap::Result::kTokenIssued &&
          issued.snapshot.callback_id == id && issued.snapshot == supplied,
          "callback ID preserved at issuance without replacing timer bits");
    swap::Payload expected; swap::Encode(stock, issued.token, expected, true);
    Check(issued.payload.length == expected.length && issued.payload.words == expected.words,
          "callback ID never encoded into token packet or original stock words");
    Check(ledger.consume(issued.payload.view(), fetch, match) == swap::Result::kTokenMatched &&
          match.snapshot == supplied && match.snapshot.callback_id == id,
          "exact ID available through one-use ledger match");
    Check(ledger.consume(issued.payload.view(), fetch, match) == swap::Result::kReplay && Clear(match),
          "replay clears callback ID and whole match");
    auto absent = supplied; absent.snapshot_available = false;
    Check(swap::NormalizeSnapshot(absent, clean) == swap::SnapshotResult::kUnavailable &&
          clean == swap::Snapshot{}, "unavailable normalization clears callback ID including stale maxima");
    swap::Ledger<1> disabled; swap::Emission legacy;
    Check(disabled.issue(stock, fetch, supplied, legacy) == swap::Result::kLegacyDisabled &&
          legacy.payload.length == 4 && legacy.snapshot == swap::Snapshot{} && legacy.token == swap::Token{},
          "disabled emission retains no callback ID and preserves legacy swap");
  }
  // Callback order can differ from issuance order: concurrent observers finish
  // before taking the SDK ledger lock. No ID==serial/monotonic-ID assumption.
  swap::Ledger<2> ledger(epoch, true); swap::Emission first, second; swap::Match match;
  auto observed_later = Available(); observed_later.callback_id = 900;
  auto observed_earlier = Available(); observed_earlier.callback_id = 7;
  Check(ledger.issue(stock, fetch, observed_later, first) == swap::Result::kTokenIssued &&
        first.token.serial == 1 && first.snapshot.callback_id == 900, "later callback can issue first");
  Check(ledger.issue(stock, fetch, observed_earlier, second) == swap::Result::kTokenIssued &&
        second.token.serial == 2 && second.snapshot.callback_id == 7, "callback IDs need not increase with serials");
  Check(ledger.consume(second.payload.view(), fetch, match) == swap::Result::kTokenMatched &&
        match.token.serial == 2 && match.snapshot.callback_id == 7, "reversed consume keeps second exact ID");
  Check(ledger.consume(first.payload.view(), fetch, match) == swap::Result::kTokenMatched &&
        match.token.serial == 1 && match.snapshot.callback_id == 900, "reversed consume keeps first exact ID");
  Check(ledger.consume(first.payload.view(), fetch, match) == swap::Result::kReplay && Clear(match),
        "reversed identity record cannot be replayed");
  first.snapshot.callback_id = std::numeric_limits<uint64_t>::max();
  Check(ledger.issue(stock, fetch, observed_earlier, first) == swap::Result::kCapacityExhausted && Clear(first),
        "rejected issuance clears preexisting callback identity with the whole emission");
}

void AliasAndFallbackTests() {
  swap::Payload payload; swap::Encode(stock, {}, payload);
  Check(swap::Encode(std::span(payload.words).first(4), {epoch, 1}, payload, true) == swap::Result::kTokenIssued,
        "encoding safe when stock aliases destination");
  swap::Decoded decoded; swap::Decode(payload.view(), decoded);
  Check(decoded.stock == stock, "aliased encoder stock unchanged");
  Check(swap::Decode(decoded.stock, decoded) == swap::Result::kLegacyPacket && decoded.stock == stock,
        "legacy decode safe in place");
  auto snapshot = Available();
  Check(swap::NormalizeSnapshot(snapshot, snapshot) == swap::SnapshotResult::kAvailable && snapshot == Available(),
        "snapshot normalization safe in place");
  swap::Ledger<1> ledger(epoch, true); swap::Emission issued;
  swap::Encode(stock, {}, issued.payload); issued.snapshot = Available();
  Check(ledger.issue(std::span(issued.payload.words).first(4), fetch, issued.snapshot, issued) == swap::Result::kTokenIssued &&
        issued.snapshot == Available(), "issuance snapshots aliasing arguments before output clearing");
  swap::Match match;
  Check(ledger.consume(issued.payload.view(), fetch, match) == swap::Result::kTokenMatched,
        "aliased issuance still binds stock/fetch");
  swap::Emission rejected;
  Check(ledger.issue(stock, fetch, Available(), rejected) == swap::Result::kCapacityExhausted && Clear(rejected),
        "failed diagnostic issuance cleared");
  Check(swap::Encode(stock, {}, payload) == swap::Result::kLegacyDisabled && payload.length == 4,
        "failure caller can still execute original legacy swap");
  for (size_t i = 0; i < 4; ++i) Check(payload.words[i] == stock[i], "fallback original stock untouched");
  payload.length = 57;
  Check(payload.view().empty(), "invalid internal count never constructs out-of-range span");
}
}  // namespace

int main() {
  ProtocolTests(); DecodeRejectionTests(); LedgerTests(); BindingRejectionTests(); SnapshotTests(); CallbackIdentityTests(); AliasAndFallbackTests();
  std::cout << "checks=" << checks << " failures=" << failures << '\n';
  return failures ? 1 : 0;
}
