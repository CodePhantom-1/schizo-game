#pragma once
// Test.hpp — the zero-dependency test harness (coordinator-owned).
// Each tests/test_*.cpp defines static bool test_xxx() functions and a main()
// built from SIM_MAIN(tests...). A failing check prints and returns 1.
#include <cstdio>
#include <string>

#define SIM_CHECK(cond)                                                        \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            return false;                                                      \
        }                                                                      \
    } while (0)

#define SIM_CHECK_EQ(a, b)                                                     \
    do {                                                                       \
        if (!((a) == (b))) {                                                   \
            std::fprintf(stderr, "FAIL %s:%d  %s == %s\n", __FILE__, __LINE__, \
                         #a, #b);                                              \
            return false;                                                      \
        }                                                                      \
    } while (0)

#define SIM_MAIN(...)                                                          \
    int main() {                                                               \
        bool (*tests[])() = {__VA_ARGS__};                                     \
        for (bool (*t)() : tests) {                                            \
            if (!t()) return 1;                                                \
        }                                                                      \
        std::printf("ok\n");                                                   \
        return 0;                                                              \
    }
