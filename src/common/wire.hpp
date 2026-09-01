// The Uniflow datagram frame: a 12-byte hand-rolled prefix, then a protobuf
// body. The prefix exists because protobuf is unsafe on corrupted input, and
// this channel corrupts input by design.
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                       magic  ('U','N','I','F')                |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |      body_len (u16 LE)        |      hdr_crc (u16 LE)         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     body_crc (u32 LE)                         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                  protobuf body (body_len bytes)               |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// offset  size  field
//   0      4    magic     "UNIF", literal bytes, no byte order
//   4      2    body_len  u16 LITTLE-ENDIAN
//   6      2    hdr_crc   u16 LITTLE-ENDIAN, crc16 over bytes [0, 6)
//   8      4    body_crc  u32 LITTLE-ENDIAN, crc32c over the body only
//  12      n    body
//
// THREE CONTRACT DECISIONS - these must match the receiver exactly:
//   1. hdr_crc covers SIX bytes (magic + body_len), not eight. The design doc
//      says "the preceding 8 bytes", but only 6 bytes precede the field.
//   2. Every fixed-width integer in the prefix is LITTLE-ENDIAN.
//   3. body_crc covers the body only. The prefix has its own CRC.
//
// This header deliberately knows nothing about protobuf: it is pure bytes, so
// it unit-tests without a schema and can be handed to the receiver verbatim.
#ifndef NEXUS_COMMON_WIRE_HPP
#define NEXUS_COMMON_WIRE_HPP

#include <cstddef>
#include <cstdint>

namespace nexus::wire {

inline constexpr std::uint8_t kMagic[4]    = {'U', 'N', 'I', 'F'};
inline constexpr std::size_t  kPrefixBytes = 12;

// 1500 Ethernet MTU - 20 IP - 8 UDP. Anything larger fragments, and a
// fragmented datagram is lost if ANY fragment is lost - which silently
// multiplies the loss rate the FEC layer was sized against.
inline constexpr std::size_t kMaxFrameBytes = 1472;
inline constexpr std::size_t kMaxBodyBytes  = kMaxFrameBytes - kPrefixBytes;

// Stamp the prefix in front of a body that has ALREADY been serialised at
// slot + kPrefixBytes. Returns the total frame length, or 0 if body_len is out
// of range (in which case slot must be left untouched).
//
// Why "finalize" and not "build": protobuf serialises straight into the send
// buffer, so the body exists before the prefix does. You compute body_crc over
// bytes that are already sitting where they will be sent from - no extra copy.
//
//   uint8_t* body = slot + nexus::wire::kPrefixBytes;
//   size_t n = msg.ByteSizeLong();
//   msg.SerializeWithCachedSizesToArray(body);
//   size_t frame_len = nexus::wire::finalize(slot, n);
std::size_t finalize(std::uint8_t* slot, std::size_t body_len) noexcept;

enum class Verdict : std::uint8_t {
    kOk = 0,
    kTooShort,      // fewer than 12 bytes arrived
    kBadMagic,      // foreign or stale traffic
    kBadHeaderCrc,  // prefix corrupt - body_len is NOT trustworthy
    kBadLength,     // body_len disagrees with the datagram, or exceeds the cap
    kBadBodyCrc,    // body corrupt - drop it; the FEC layer already paid for this
};

const char* to_string(Verdict v) noexcept;

// Validate a received datagram. On kOk, *body points into `frame` at the
// protobuf body and *body_len is its length; on anything else the outputs are
// untouched and the caller drops the datagram.
//
// Dropping is not an error path. It is an erasure, and erasures are exactly
// what the 27.5% code rate was bought to absorb.
Verdict verify(const std::uint8_t* frame, std::size_t frame_len,
               const std::uint8_t** body, std::size_t* body_len) noexcept;

}  // namespace nexus::wire
#endif
