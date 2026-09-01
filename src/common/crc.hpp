// Checksums for the Uniflow wire frame.
//
// Two checksums, two different jobs:
//
//   crc16  - guards the frame prefix, so that `body_len` is trustworthy BEFORE
//            anything uses it as a length. A flipped bit inside a length field
//            that gets acted on becomes an allocation request.
//
//   crc32c - guards the protobuf body, so that a corrupted packet becomes a
//            DROPPED packet. That matters because a drop is an *erasure*, and
//            an erasure is the only failure mode Reed-Solomon can solve. UDP's
//            own checksum is 16 bits, so ~1 in 65,536 corrupt datagrams passes
//            it while still being wrong: about 14 poisoned frames per 1 GB.
//
// INVARIANT: verify the CRC, then parse. Never the reverse.
#ifndef NEXUS_COMMON_CRC_HPP
#define NEXUS_COMMON_CRC_HPP

#include <cstddef>
#include <cstdint>

namespace nexus::common {

// CRC-16/CCITT-FALSE.
//   polynomial  0x1021
//   init        0xFFFF
//   reflected   no (neither input nor output)
//   final xor   0x0000
//   check       crc16("123456789") == 0x29B1
std::uint16_t crc16(const void* data, std::size_t len) noexcept;

// CRC-32C (Castagnoli).
//   polynomial  0x1EDC6F41  (reflected form: 0x82F63B78)
//   init        0xFFFFFFFF
//   reflected   yes (both input and output)
//   final xor   0xFFFFFFFF
//   check       crc32c("123456789") == 0xE3069283
//
// "Reflected" changes the shape of the loop: you shift RIGHT and index the
// table with the LOW byte, where the non-reflected crc16 shifts LEFT and
// indexes with the HIGH byte. Getting this backwards is the single most common
// way to produce a plausible-looking checksum that nobody else agrees with.
//
// Later, once the vector below passes, this is where hardware acceleration
// goes: _mm_crc32_u64 on x86 (SSE4.2) or __crc32cd on ARMv8. Both compute
// exactly this function. Correctness first - the fast path is worthless if you
// cannot prove it agrees with a reference.
std::uint32_t crc32c(const void* data, std::size_t len) noexcept;

}  // namespace nexus::common
#endif
