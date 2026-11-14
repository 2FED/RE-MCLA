// This test links the SDK's real RingBuffer implementation: no GPU or game.
#include "../src/mcla_swap_packet_words.h"
#include "../src/mcla_swap_emission_core.h"
#include <rex/memory/ring_buffer.h>
#include <array>
#include <bit>
#include <cstring>
#include <iostream>
namespace sw = rex::diagnostics::swap_emission;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* why) { ++checks; if (!value) { ++failures; std::cerr << why << '\n'; } }
int main() {
  const std::array<uint32_t, 4> stock{0x53574150, 0x1234000, 1280, 720};
  sw::Payload payload; sw::Encode(stock, {0x1122334455667788, 0x99aabbccddeeff00}, payload, true);
  for (size_t start : {size_t(0), size_t(4), size_t(112), size_t(124)}) {
    alignas(4) std::array<uint8_t, 128> bytes{};
    rex::memory::RingBuffer ring(bytes.data(), bytes.size());
    ring.set_read_offset(start); ring.set_write_offset(start);
    for (auto word : payload.view()) {
      const uint32_t be = std::byteswap(word);
      Check(ring.Write(reinterpret_cast<const uint8_t*>(&be), 4) == 4, "write exact ring word");
    }
    sw::ReadPayload read;
    Check(sw::ReadPayloadWords(ring, 11, read), "real ring extended read");
    Check(ring.empty() && read.captured().size() == 11, "exact cursor movement");
    sw::Decoded decoded;
    Check(sw::Decode(read.captured(), decoded) == sw::Result::kExtendedPacket &&
          decoded.token == sw::Token{0x1122334455667788, 0x99aabbccddeeff00} && decoded.stock == stock,
          "wrapped endian token unchanged");
  }
  for (uint32_t count : {0u, 1u, 2u, 3u, 11u, 16385u, UINT32_MAX}) {
    alignas(4) std::array<uint8_t, 128> bytes{};
    rex::memory::RingBuffer ring(bytes.data(), bytes.size());
    ring.set_write_offset(40);  // ten words, one short of diagnostic payload.
    sw::ReadPayload read; read.words.fill(42); read.declared_count = 11; read.diagnostic_length_supported = true;
    Check(!sw::ReadPayloadWords(ring, count, read) && ring.read_offset() == 0, "bad count rejected before reads");
    Check(read.words == std::array<uint32_t, 56>{} && !read.declared_count && read.captured().empty(),
          "bad packet clears output");
  }
  for (uint32_t count : {4u, 5u, 56u, 57u, 16384u}) {
    alignas(4) std::array<uint8_t, 65540> bytes{};
    rex::memory::RingBuffer ring(bytes.data(), bytes.size());
    for (uint32_t i = 0; i < count; ++i) {
      const auto word = std::byteswap(i < 4 ? stock[i] : i);
      ring.Write(reinterpret_cast<const uint8_t*>(&word), 4);
    }
    sw::ReadPayload read;
    Check(sw::ReadPayloadWords(ring, count, read) && ring.empty(), "normal swap args remain readable");
    for (size_t i = 0; i < 4; ++i) Check(read.words[i] == stock[i], "original four words preserved");
    Check(read.captured().empty() == (count > 56), "large extension not captured");
  }
  std::cout << "real ring payload: " << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
