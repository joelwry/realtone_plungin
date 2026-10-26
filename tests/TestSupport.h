#pragma once

// Small self-contained test harness so the core library can be tested without
// any third-party framework on every platform we build for.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace rt_test {

struct Case
{
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> r;
    return r;
}

struct Registrar
{
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

struct Failure
{
    std::string message;
};

inline int& failureCount()
{
    static int n = 0;
    return n;
}

inline void report(const char* file, int line, const std::string& msg)
{
    std::fprintf(stderr, "    FAIL %s:%d  %s\n", file, line, msg.c_str());
    ++failureCount();
}

// Fresh scratch directory per call, removed at the end of the process.
inline std::filesystem::path scratchDir(const std::string& name)
{
    auto p = std::filesystem::temp_directory_path() / ("realtone_tests_" + name);
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
    std::filesystem::create_directories(p, ec);
    return p;
}

}  // namespace rt_test

#define RT_TEST(name)                                                                  \
    static void test_##name();                                                         \
    static rt_test::Registrar registrar_##name(#name, &test_##name);                   \
    static void test_##name()

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) rt_test::report(__FILE__, __LINE__, "CHECK(" #cond ")");          \
    } while (0)

#define CHECK_MSG(cond, msg)                                                           \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::ostringstream os_;                                                    \
            os_ << "CHECK(" #cond ") " << msg;                                         \
            rt_test::report(__FILE__, __LINE__, os_.str());                            \
        }                                                                              \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                          \
    do {                                                                               \
        const double a_ = (a), b_ = (b);                                               \
        if (!(std::fabs(a_ - b_) <= (tol))) {                                          \
            std::ostringstream os_;                                                    \
            os_ << #a " = " << a_ << ", " #b " = " << b_ << ", tolerance " << (tol);   \
            rt_test::report(__FILE__, __LINE__, os_.str());                            \
        }                                                                              \
    } while (0)
