#include "common/wire.hpp"

#include <cstring>

#include "common/crc.hpp"
#include "common/endian.hpp"

namespace nexus::wire {

using common::crc16;
using common::crc32c;
using common::load_le16;
using common::load_le32;
using common::store_le16;
using common::store_le32;

namespace {
constexpr std::size_t kOffMagic   = 0;  // 4 bytes
constexpr std::size_t kOffBodyLen = 4;  // 2 bytes
constexpr std::size_t kOffHdrCrc  = 6;  // 2 bytes
constexpr std::size_t kOffBodyCrc = 8;  // 4 bytes
constexpr std::size_t kHdrCrcSpan = 6;  // <- the SIX, not eight
}  // namespace

std::size_t finalize(std::uint8_t* slot, std::size_t body_len) noexcept {
    // Refuse rather than fragment. A datagram split across IP fragments is
    // lost if ANY fragment is lost, which would multiply the real loss rate
    // out from under the code rate the FEC layer was sized against.
    if (body_len > kMaxBodyBytes) return 0;

    std::memcpy(slot + kOffMagic, kMagic, sizeof(kMagic));
    store_le16(slot + kOffBodyLen, static_cast<std::uint16_t>(body_len));

    // Order matters: the length is part of what hdr_crc protects, so this must
    // come after the length is written.
    store_le16(slot + kOffHdrCrc, crc16(slot, kHdrCrcSpan));

    store_le32(slot + kOffBodyCrc, crc32c(slot + kPrefixBytes, body_len));

    return kPrefixBytes + body_len;
}

Verdict verify(const std::uint8_t* frame, std::size_t frame_len,
               const std::uint8_t** body, std::size_t* body_len) noexcept {
    // 1. Enough bytes to have a prefix at all.
    if (frame_len < kPrefixBytes) return Verdict::kTooShort;

    // 2. Magic. Rejects foreign and stale traffic for the price of a memcmp,
    //    before anything else is touched.
    if (std::memcmp(frame + kOffMagic, kMagic, sizeof(kMagic)) != 0)
        return Verdict::kBadMagic;

    // 3. Header CRC. Until this passes, body_len is just four bits of cosmic
    //    ray away from being anything at all - so it is not read yet.
    if (load_le16(frame + kOffHdrCrc) != crc16(frame, kHdrCrcSpan))
        return Verdict::kBadHeaderCrc;

    // 4. Now the length may be read, and it still gets bounds-checked. For a
    //    datagram the frame length is exact, so a short read or trailing bytes
    //    both mean something went wrong upstream.
    const std::size_t n = load_le16(frame + kOffBodyLen);
    if (n > kMaxBodyBytes || kPrefixBytes + n != frame_len)
        return Verdict::kBadLength;

    // 5. Body CRC. This is the step that turns a bit flip into a clean
    //    erasure, which is the only damage Reed-Solomon knows how to repair.
    if (load_le32(frame + kOffBodyCrc) != crc32c(frame + kPrefixBytes, n))
        return Verdict::kBadBodyCrc;

    *body = frame + kPrefixBytes;
    *body_len = n;
    return Verdict::kOk;
}

const char* to_string(Verdict v) noexcept {
    switch (v) {
        case Verdict::kOk:           return "ok";
        case Verdict::kTooShort:     return "too_short";
        case Verdict::kBadMagic:     return "bad_magic";
        case Verdict::kBadHeaderCrc: return "bad_header_crc";
        case Verdict::kBadLength:    return "bad_length";
        case Verdict::kBadBodyCrc:   return "bad_body_crc";
    }
    return "unknown";
}

}  // namespace nexus::wire
