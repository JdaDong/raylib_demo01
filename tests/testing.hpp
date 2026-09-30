// testing.hpp - zero-dependency test framework (auto registration, suites,
// filters, CHECK/REQUIRE macros, near-compare). ~100 lines, no gtest needed.
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace rt {

struct TestCase {
    const char* suite;
    const char* name;
    std::function<void()> fn;
    const char* file;
    int line;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void()> fn, const char* file, int line) {
        Registry().push_back({suite, name, std::move(fn), file, line});
    }
};

struct Context {
    int checks = 0;
    int failures = 0;
};
inline Context& Ctx() {
    static Context c;
    return c;
}

struct RequireFailed {};

inline void ReportFailure(const char* file, int line, const std::string& msg) {
    ++Ctx().failures;
    std::fprintf(stderr, "    FAIL %s:%d: %s\n", file, line, msg.c_str());
}

template <typename A, typename B>
std::string DescribeEq(const char* ea, const char* eb, const A& a, const B& b) {
    std::ostringstream os;
    os << ea << " == " << eb << "  (" << a << " vs " << b << ")";
    return os.str();
}

}  // namespace rt

#define RT_CAT2(a, b) a##b
#define RT_CAT(a, b) RT_CAT2(a, b)

#define TEST(suite, name)                                                                         \
    static void RT_CAT(rt_test_, __LINE__)();                                                     \
    static ::rt::Registrar RT_CAT(rt_reg_, __LINE__)(#suite, #name, &RT_CAT(rt_test_, __LINE__),   \
                                                     __FILE__, __LINE__);                         \
    static void RT_CAT(rt_test_, __LINE__)()

#define CHECK(cond)                                                                               \
    do {                                                                                          \
        ++::rt::Ctx().checks;                                                                     \
        if (!(cond)) ::rt::ReportFailure(__FILE__, __LINE__, "CHECK(" #cond ")");                 \
    } while (0)

#define REQUIRE(cond)                                                                             \
    do {                                                                                          \
        ++::rt::Ctx().checks;                                                                     \
        if (!(cond)) {                                                                            \
            ::rt::ReportFailure(__FILE__, __LINE__, "REQUIRE(" #cond ")");                        \
            throw ::rt::RequireFailed{};                                                          \
        }                                                                                         \
    } while (0)

#define CHECK_EQ(a, b)                                                                            \
    do {                                                                                          \
        ++::rt::Ctx().checks;                                                                     \
        const auto& rt_a = (a);                                                                   \
        const auto& rt_b = (b);                                                                   \
        if (!(rt_a == rt_b)) ::rt::ReportFailure(__FILE__, __LINE__, ::rt::DescribeEq(#a, #b, rt_a, rt_b)); \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                     \
    do {                                                                                          \
        ++::rt::Ctx().checks;                                                                     \
        const double rt_a = static_cast<double>(a), rt_b = static_cast<double>(b);                \
        if (!(std::fabs(rt_a - rt_b) <= (eps)))                                                   \
            ::rt::ReportFailure(__FILE__, __LINE__, ::rt::DescribeEq(#a, #b, rt_a, rt_b) + " eps=" #eps); \
    } while (0)

#define CHECK_STREQ(a, b) CHECK_EQ(std::string(a), std::string(b))
