// A ~50-line test harness. Deliberately not GoogleTest: on day 2 the point is
// that `cmake --build build && ctest` works with zero extra dependencies.
#ifndef NEXUS_TEST_HPP
#define NEXUS_TEST_HPP

#include <cstdio>
#include <string>

namespace nexus::test {
inline int g_failures = 0;
inline int g_checks   = 0;

inline void report(bool ok, const char* expr, const char* file, int line,
                   const std::string& detail) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::fprintf(stderr, "FAIL %s:%d\n  %s\n", file, line, expr);
    if (!detail.empty()) std::fprintf(stderr, "  %s\n", detail.c_str());
}

inline int finish(const char* suite) {
    std::printf("%-12s %3d checks, %d failures\n", suite, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
}  // namespace nexus::test

#define CHECK(expr) ::nexus::test::report((expr), #expr, __FILE__, __LINE__, {})

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        const auto _a = (a);                                                 \
        const auto _b = (b);                                                 \
        ::nexus::test::report(_a == _b, #a " == " #b, __FILE__, __LINE__,    \
                              "got " + std::to_string(_a) +                  \
                                  ", want " + std::to_string(_b));           \
    } while (0)

#define CHECK_HEX(a, b)                                                      \
    do {                                                                     \
        const unsigned long long _a = (unsigned long long)(a);               \
        const unsigned long long _b = (unsigned long long)(b);               \
        char _buf[80];                                                       \
        std::snprintf(_buf, sizeof(_buf), "got 0x%llX, want 0x%llX", _a, _b);\
        ::nexus::test::report(_a == _b, #a " == " #b, __FILE__, __LINE__,    \
                              _buf);                                         \
    } while (0)

#endif
