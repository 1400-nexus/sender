// Explicit little-endian access for the wire frame.
//
// The point of these is NOT byte-swapping - on x86 and ARM they compile down
// to a single load or store. The point is that the wire format is defined by
// the spec rather than by whatever byte order the compiling machine happens to
// have. memcpy'ing a native uint16_t into the buffer produces identical bytes
// today and silently encodes an assumption that nobody wrote down.
#ifndef NEXUS_COMMON_ENDIAN_HPP
#define NEXUS_COMMON_ENDIAN_HPP

#include <cstddef>
#include <cstdint>

namespace nexus::common {

inline void store_le16(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}

inline void store_le32(std::uint8_t* p, std::uint32_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
    p[2] = static_cast<std::uint8_t>(v >> 16);
    p[3] = static_cast<std::uint8_t>(v >> 24);
}

inline std::uint16_t load_le16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[0]) |
                                      static_cast<std::uint16_t>(p[1]) << 8);
}

inline std::uint32_t load_le32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 |
           static_cast<std::uint32_t>(p[3]) << 24;
}

}  // namespace nexus::common
#endif
