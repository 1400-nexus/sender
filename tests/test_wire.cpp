// Frame round-trip, byte-level layout, and - the half people skip - the
// rejection cases. A framing layer that only proves it accepts good frames has
// proved nothing at all.
#include "nexus_test.hpp"

#include <cstring>
#include <random>
#include <vector>

#include "common/crc.hpp"
#include "common/wire.hpp"

using namespace nexus;

namespace {

// A body with recognisable, deterministic content: body[i] = (i * 31 + 7) % 256
std::vector<std::uint8_t> make_frame(std::size_t body_len) {
    std::vector<std::uint8_t> slot(wire::kMaxFrameBytes, 0);
    for (std::size_t i = 0; i < body_len; ++i)
        slot[wire::kPrefixBytes + i] = static_cast<std::uint8_t>(i * 31 + 7);
    slot.resize(wire::finalize(slot.data(), body_len));
    return slot;
}

wire::Verdict verdict_of(const std::vector<std::uint8_t>& f) {
    const std::uint8_t* body = nullptr;
    std::size_t body_len = 0;
    return wire::verify(f.data(), f.size(), &body, &body_len);
}

}  // namespace

int main() {
    // Fail fast and readably while the stubs are still stubs, instead of
    // segfaulting on an empty frame further down.
    {
        std::vector<std::uint8_t> probe(wire::kMaxFrameBytes, 0);
        if (wire::finalize(probe.data(), 16) == 0) {
            std::fprintf(stderr,
                "wire::finalize() returned 0 for a valid 16-byte body, so it "
                "is not implemented yet.\n"
                "Order to work in: crc16 -> crc32c -> finalize -> verify -> "
                "to_string.\n"
                "Run test_crc first; test_wire cannot pass until it does.\n");
            return 1;
        }
    }

    // --- constants are part of the contract -------------------------------
    CHECK_EQ(wire::kPrefixBytes, std::size_t{12});
    CHECK_EQ(wire::kMaxFrameBytes, std::size_t{1472});
    CHECK_EQ(wire::kMaxBodyBytes, std::size_t{1460});

    // --- round trip at the size that actually ships -----------------------
    // A DataPacket carrying a 1400-byte symbol serialises to ~1427 bytes.
    {
        auto f = make_frame(1430);
        CHECK_EQ(f.size(), std::size_t{1442});
        CHECK(f.size() + 28 <= 1500);  // +IP +UDP: must never fragment

        const std::uint8_t* body = nullptr;
        std::size_t body_len = 0;
        CHECK(wire::verify(f.data(), f.size(), &body, &body_len) ==
              wire::Verdict::kOk);
        CHECK_EQ(body_len, std::size_t{1430});
        CHECK(body == f.data() + 12);
        CHECK_EQ(body[0], std::uint8_t{7});      // (0 * 31 + 7)
        CHECK_EQ(body[1], std::uint8_t{38});     // (1 * 31 + 7)
    }

    // --- EXACT BYTES. These pin the two decisions that were ambiguous. ----
    //     0x334B is crc16 over SIX bytes ("UNIF" + 1430 little-endian).
    //     If you covered eight bytes, or wrote big-endian, this fails.
    {
        auto f = make_frame(1430);
        CHECK_EQ(f[0], std::uint8_t{'U'});
        CHECK_EQ(f[1], std::uint8_t{'N'});
        CHECK_EQ(f[2], std::uint8_t{'I'});
        CHECK_EQ(f[3], std::uint8_t{'F'});

        CHECK_EQ(f[4], std::uint8_t{0x96});      // 1430 = 0x0596, low byte first
        CHECK_EQ(f[5], std::uint8_t{0x05});

        CHECK_EQ(f[6], std::uint8_t{0x4B});      // hdr_crc  = 0x334B, LE
        CHECK_EQ(f[7], std::uint8_t{0x33});

        CHECK_EQ(f[8],  std::uint8_t{0x5E});     // body_crc = 0x8EB9525E, LE
        CHECK_EQ(f[9],  std::uint8_t{0x52});
        CHECK_EQ(f[10], std::uint8_t{0xB9});
        CHECK_EQ(f[11], std::uint8_t{0x8E});
    }

    // --- degenerate sizes -------------------------------------------------
    {
        auto f = make_frame(0);
        CHECK_EQ(f.size(), std::size_t{12});
        CHECK(verdict_of(f) == wire::Verdict::kOk);
    }
    {
        std::vector<std::uint8_t> slot(wire::kMaxFrameBytes + 64, 0);
        CHECK_EQ(wire::finalize(slot.data(), wire::kMaxBodyBytes),
                 wire::kMaxFrameBytes);
        // Refusing to build an over-MTU frame is a caught bug rather than
        // silent IP fragmentation.
        CHECK_EQ(wire::finalize(slot.data(), wire::kMaxBodyBytes + 1),
                 std::size_t{0});
    }

    // --- rejection cases --------------------------------------------------
    {
        auto f = make_frame(100);
        f.resize(11);
        CHECK(verdict_of(f) == wire::Verdict::kTooShort);
    }
    {
        auto f = make_frame(100);
        f[1] = 'X';
        CHECK(verdict_of(f) == wire::Verdict::kBadMagic);
    }
    {
        // A flipped bit inside body_len. Without hdr_crc this would be read as
        // a length and acted on. It must be caught BEFORE that happens.
        auto f = make_frame(100);
        f[4] ^= 0x40;
        CHECK(verdict_of(f) == wire::Verdict::kBadHeaderCrc);
    }
    {
        // Length is CRC-clean but disagrees with the datagram: truncated.
        auto f = make_frame(100);
        f.resize(f.size() - 1);
        CHECK(verdict_of(f) == wire::Verdict::kBadLength);
    }
    {
        auto f = make_frame(1430);
        f[12 + 999] ^= 0x01;
        CHECK(verdict_of(f) == wire::Verdict::kBadBodyCrc);
    }

    // --- every bit of the body must be covered by body_crc ----------------
    {
        auto f = make_frame(1430);
        bool all_caught = true;
        for (std::size_t bit = 0; bit < 1430 * 8; bit += 7) {
            const std::size_t idx = 12 + bit / 8;
            const std::uint8_t msk = static_cast<std::uint8_t>(1u << (bit % 8));
            f[idx] ^= msk;
            const bool caught = verdict_of(f) == wire::Verdict::kBadBodyCrc;
            f[idx] ^= msk;
            if (!caught) {
                std::fprintf(stderr, "  body bit %zu slipped through\n", bit);
                all_caught = false;
                break;
            }
        }
        CHECK(all_caught);
    }

    // --- 20,000 random single-bit corruptions. NONE may be accepted. ------
    {
        std::mt19937_64 rng(7);
        auto base = make_frame(1430);
        int accepted = 0;
        for (int i = 0; i < 20000; ++i) {
            auto f = base;
            f[rng() % f.size()] ^= static_cast<std::uint8_t>(1u << (rng() % 8));
            if (verdict_of(f) == wire::Verdict::kOk) ++accepted;
        }
        CHECK_EQ(accepted, 0);
    }

    // --- to_string must name every verdict --------------------------------
    {
        using V = wire::Verdict;
        for (auto v : {V::kOk, V::kTooShort, V::kBadMagic, V::kBadHeaderCrc,
                       V::kBadLength, V::kBadBodyCrc}) {
            const char* s = wire::to_string(v);
            CHECK(s != nullptr && s[0] != '\0' &&
                  std::strcmp(s, "unimplemented") != 0);
        }
    }

    return nexus::test::finish("test_wire");
}
