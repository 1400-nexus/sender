#include "common/crc.hpp"

#include <array>

namespace nexus::common {
namespace {

consteval std::array<std::uint32_t, 256> make_crc32c_table() {
    std::array<std::uint32_t, 256> t{};
    for (int i = 0; i < 256; ++i) {
        std::uint32_t c = static_cast<std::uint32_t>(i);
        for (int k = 0; k < 8; ++k)
            c = (c & 1u) ? (c >> 1) ^ 0x82F63B78u : (c >> 1);
        t[static_cast<std::size_t>(i)] = c;
    }
    return t;
}
constexpr auto kCrc32cTable = make_crc32c_table();

}

std::uint16_t crc16(const void* data, std::size_t len) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint16_t crc = 0xFFFFu;

    while (len--) {
        crc ^= static_cast<std::uint16_t>(*p++) << 8;   // byte into the TOP

        for (int bit = 0; bit < 8; ++bit) {
            // The cast is load-bearing: crc << 1 promotes to int, so without
            // it bits escape above position 15 and never come back.
            crc = (crc & 0x8000u) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021u)
                                  : static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

// CRC-32C (Castagnoli): poly 0x1EDC6F41, init 0xFFFFFFFF, reflected,
// final xor 0xFFFFFFFF.
//
// Reflected: the byte enters at the BOTTOM, the register shifts RIGHT, and the
// table is indexed by the LOW byte. Table-driven because this runs over 1400
// bytes per packet at ~17,000 packets/second.
//
// Fast enough as-is (~1 GB/s against the ~24 MB/s you actually need). If you
// ever want the hardware path, _mm_crc32_u64 on x86 and __crc32cd on ARMv8
// compute exactly this function - keep this version as the reference to check
// the accelerated one against, over many lengths and alignments.
std::uint32_t crc32c(const void* data, std::size_t len) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint32_t crc = 0xFFFFFFFFu;

    while (len--)
        crc = (crc >> 8) ^ kCrc32cTable[static_cast<std::uint8_t>(crc ^ *p++)];

    return crc ^ 0xFFFFFFFFu;
}

}  // namespace nexus::common
