#include "TestFramework.h"
#include <chrono>
int main (int argc, char** argv)
{
    const std::string filter = argc > 1 ? argv[1] : "";
    int run = 0;
    for (auto& c : bftest::registry())
    {
        if (! filter.empty() && c.id.find (filter) == std::string::npos) continue;
        bftest::current() = c.id;
        const int before = bftest::failures();
        const auto t0 = std::chrono::steady_clock::now();
        std::printf ("[%s] %s\n", c.id.c_str(), c.name.c_str());
        c.fn();
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("  -> %s (%.0f ms)\n", bftest::failures() == before ? "PASS" : "FAIL", ms);
        ++run;
    }
    std::printf ("\n%d tests, %d failed checks\n", run, bftest::failures());
    return bftest::failures() == 0 ? 0 : 1;
}
