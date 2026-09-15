#pragma once
#include <cstdio>
#include <cmath>
#include <functional>
#include <vector>

struct TestCase { const char* name; std::function<void()> fn; };
std::vector<TestCase>& allTests();
extern int g_failures;

struct TestReg {
    TestReg(const char* n, std::function<void()> f) { allTests().push_back({n, f}); }
};
#define TEST(name) \
    static void name(); \
    static TestReg reg_##name(#name, name); \
    static void name()

#define CHECK(cond) do { \
    if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_failures++; } \
} while (0)
#define CHECK_NEAR(a, b, eps) do { \
    float va_ = (a), vb_ = (b); \
    if (std::fabs(va_ - vb_) > (eps)) { \
        std::printf("FAIL %s:%d: %s (%f) != %s (%f)\n", __FILE__, __LINE__, #a, va_, #b, vb_); \
        g_failures++; } \
} while (0)
