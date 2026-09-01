// Known-answer vectors. Every one of these was computed independently of your
// implementation, so passing them means your polynomial, initial value,
// reflection direction and final XOR all agree with the rest of the world.
//
// Hand this same list to Person B. Two implementations that both reproduce
// these will interoperate; two that were "checked by eye" will not.
#include "nexus_test.hpp"

#include <cstring>
#include <vector>

#include "common/crc.hpp"

using nexus::common::crc16;
using nexus::common::crc32c;

int main() {
    // --- the canonical vector -------------------------------------------
    CHECK_HEX(crc16("123456789", 9), 0x29B1u);
    CHECK_HEX(crc32c("123456789", 9), 0xE3069283u);

    // --- empty input. Note these are NOT zero for crc16: the init value
    //     survives when there is nothing to fold into it. ------------------
    CHECK_HEX(crc16("", 0), 0xFFFFu);
    CHECK_HEX(crc32c("", 0), 0x00000000u);

    // --- single byte ------------------------------------------------------
    CHECK_HEX(crc16("a", 1), 0x9D77u);
    CHECK_HEX(crc32c("a", 1), 0xC1D04330u);

    // --- every byte value, so no table entry goes untested ----------------
    {
        std::vector<std::uint8_t> all(256);
        for (int i = 0; i < 256; ++i) all[i] = static_cast<std::uint8_t>(i);
        CHECK_HEX(crc16(all.data(), all.size()), 0x3FBDu);
        CHECK_HEX(crc32c(all.data(), all.size()), 0x9C44184Bu);
    }

    // --- at the real symbol size (1400 bytes), which is where a broken tail
    //     loop or a bad alignment assumption actually shows up -------------
    {
        const std::vector<std::uint8_t> a5(1400, 0xA5);
        CHECK_HEX(crc16(a5.data(), a5.size()), 0x60DEu);
        CHECK_HEX(crc32c(a5.data(), a5.size()), 0xCBCA4E4Cu);

        // All-zero input must NOT produce zero. If it does, your init value or
        // final XOR is missing, and every silently-truncated packet in the
        // system would checksum as valid.
        const std::vector<std::uint8_t> zeros(1400, 0x00);
        CHECK_HEX(crc32c(zeros.data(), zeros.size()), 0xE4574042u);
        CHECK(crc32c(zeros.data(), zeros.size()) != 0u);
    }

    // --- a single flipped bit anywhere must change the checksum ----------
    {
        std::vector<std::uint8_t> a(1400, 0xA5);
        bool all_detected = true;
        const std::uint32_t base = crc32c(a.data(), a.size());
        for (std::size_t byte : {std::size_t{0}, std::size_t{699},
                                 std::size_t{1399}}) {
            for (int bit = 0; bit < 8; ++bit) {
                a[byte] ^= static_cast<std::uint8_t>(1u << bit);
                if (crc32c(a.data(), a.size()) == base) all_detected = false;
                a[byte] ^= static_cast<std::uint8_t>(1u << bit);
            }
        }
        CHECK(all_detected);
    }

    // --- length must matter: a longer run of the same byte is a different
    //     message, not the same one. -------------------------------------
    {
        const std::vector<std::uint8_t> n1(10, 0x00), n2(11, 0x00);
        CHECK(crc32c(n1.data(), n1.size()) != crc32c(n2.data(), n2.size()));
    }

    return nexus::test::finish("test_crc");
}
