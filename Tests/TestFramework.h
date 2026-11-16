// Minimal dependency-free test framework for BASSFORGE DSP tests.
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace bftest
{
struct Case { std::string id, name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline std::string& current() { static std::string s; return s; }
struct Reg { Reg (const char* id, const char* name, std::function<void()> f) { registry().push_back ({ id, name, std::move (f) }); } };
inline void check (bool ok, const char* expr, const char* file, int line, const std::string& msg = {})
{
    if (! ok)
    {
        ++failures();
        std::printf ("    FAIL [%s] %s:%d  %s  %s\n", current().c_str(), file, line, expr, msg.c_str());
    }
}
inline void note (const std::string& s) { std::printf ("    %s\n", s.c_str()); }
} // namespace bftest

#define BF_CAT2(a, b) a##b
#define BF_CAT(a, b) BF_CAT2(a, b)
#define BF_TEST(id, name) \
    static void BF_CAT(bf_test_, __LINE__)(); \
    static bftest::Reg BF_CAT(bf_reg_, __LINE__) (id, name, &BF_CAT(bf_test_, __LINE__)); \
    static void BF_CAT(bf_test_, __LINE__)()
#define BF_CHECK(e) bftest::check ((e), #e, __FILE__, __LINE__)
#define BF_CHECK_MSG(e, m) bftest::check ((e), #e, __FILE__, __LINE__, (m))
