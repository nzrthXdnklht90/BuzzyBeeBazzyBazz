// fuzz_fit: fits the reduced "Fast Pickle" FUZZ voice to reference renders of Big Dill's validated
// full-circuit engine (tools/fuzz_fit/refgen.cpp, run against the Big Dill repository).
// usage: fuzz_fit REF_DIR cases.txt [--fit] [--report]
// Error per case: level error (dB, RMS over the steady part) and spectral-shape error (dB RMS over
// 1/3-octave bands 40 Hz - 10 kHz, level removed). Fit: Nelder-Mead in log-parameter space.
#include "FuzzVoice.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <complex>
#include <algorithm>

using namespace bassforge;

namespace
{
constexpr double kFs = 192000.0;
struct Case { FuzzControls c; std::vector<double> in, ref; std::string name; bool linear; };

std::vector<double> load (const std::string& p)
{
    std::ifstream f (p, std::ios::binary);
    std::vector<double> v;
    if (! f) return v;
    f.seekg (0, std::ios::end); const auto sz = (size_t) f.tellg(); f.seekg (0);
    v.resize (sz / sizeof (double));
    f.read ((char*) v.data(), (std::streamsize) (v.size() * sizeof (double)));
    return v;
}

void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2 * 3.14159265358979 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
    }
}

std::vector<double> bands (const std::vector<double>& x)
{
    const size_t N = 32768, st = (size_t) (0.1 * kFs);
    std::vector<std::complex<double>> a (N);
    for (size_t i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * 3.14159265 * i / N); a[i] = (st + i < x.size() ? x[st + i] : 0.0) * w; }
    fft (a);
    std::vector<double> b;
    for (double fc = 40.0; fc < 10000.0; fc *= 1.2599)
    {
        const size_t lo = (size_t) (fc / 1.12 / kFs * N), hi = std::max (lo + 1, (size_t) (fc * 1.12 / kFs * N));
        double e = 1e-30; for (size_t k = lo; k < hi; ++k) e += std::norm (a[k]);
        b.push_back (10 * std::log10 (e));
    }
    return b;
}

double rmsOf (const std::vector<double>& x) { double e = 0; size_t st = (size_t) (0.1 * kFs); for (size_t i = st; i < x.size(); ++i) e += x[i] * x[i]; return std::sqrt (e / (x.size() - st)); }

struct Err { double level, shape; };
Err evalCase (FuzzVoice& v, const Case& c)
{
    v.reset(); v.setControls (c.c);
    std::vector<double> y (c.in.size());
    for (size_t i = 0; i < y.size(); ++i) y[i] = v.processVolts (c.in[i]);
    const double lv = 20 * std::log10 ((rmsOf (y) + 1e-12) / (rmsOf (c.ref) + 1e-12));
    const auto bm = bands (y), br = bands (c.ref);
    // shape: compare only bands within 50 dB of the reference's loudest band
    const double top = *std::max_element (br.begin(), br.end());
    double mean = 0; int cnt = 0;
    for (size_t k = 0; k < bm.size(); ++k) if (br[k] > top - 50) { mean += bm[k] - br[k]; ++cnt; }
    mean /= std::max (1, cnt);
    double se = 0;
    for (size_t k = 0; k < bm.size(); ++k) if (br[k] > top - 50) { const double d = bm[k] - br[k] - mean; se += d * d; }
    return { lv, std::sqrt (se / std::max (1, cnt)) };
}

// parameter vector <-> FuzzFit (shared stage constants: the three stages are the same circuit)
constexpr int kP = 8;
FuzzFit toFit (const double* p)
{
    FuzzFit f;
    for (int s = 0; s < 3; ++s) { f.aol[s] = std::exp (p[0]); f.vPos[s] = std::exp (p[1]); f.vNeg[s] = std::exp (p[2]); }
    f.q4Gain = std::exp (p[3]); f.q4Pos = std::exp (p[4]); f.q4Neg = std::exp (p[5]);
    f.toneSrcOhms = std::exp (p[6]); f.rpi = std::exp (p[7]);
    return f;
}

double objective (const std::vector<Case>& cs, const double* p, bool print = false)
{
    FuzzVoice v; v.prepare (kFs); v.setFit (toFit (p));
    double tot = 0, sumL = 0, sumS = 0, maxL = 0, maxS = 0;
    for (auto& c : cs)
    {
        const auto e = evalCase (v, c);
        const double w = c.linear ? 2.0 : 1.0;
        tot += w * (e.level * e.level + 0.5 * e.shape * e.shape);
        sumL += std::abs (e.level); sumS += e.shape; maxL = std::max (maxL, std::abs (e.level)); maxS = std::max (maxS, e.shape);
        if (print) std::printf ("  %-34s level %+6.2f dB   shape %5.2f dB\n", c.name.c_str(), e.level, e.shape);
    }
    if (print) std::printf ("  mean |level| %.2f dB (max %.2f), mean shape %.2f dB (max %.2f)\n", sumL / cs.size(), maxL, sumS / cs.size(), maxS);
    return tot / cs.size();
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: fuzz_fit REF_DIR cases.txt [--fit]\n"); return 2; }
    const std::string dir = argv[1];
    const bool doFit = argc > 3 && std::strcmp (argv[3], "--fit") == 0;
    std::vector<Case> cs;
    std::ifstream cf (argv[2]);
    std::string line; int idx = 0;
    const char* amps[] = { "0.05", "0.3", "1.0" };
    while (std::getline (cf, line))
    {
        std::istringstream ss (line); FuzzControls c;
        if (! (ss >> c.sustain >> c.filter >> c.scoop >> c.crunch >> c.clip)) continue;
        for (auto a : amps) for (int k : { 1, 2 })
        {
            const std::string base = dir + "/c" + std::to_string (idx) + "_a" + a + "_k" + std::to_string (k) + ".raw";
            Case x { c, load (base + ".in"), load (base), "c" + std::to_string (idx) + " amp " + a + (k == 1 ? " sine220" : " bass"), false };
            if (! x.ref.empty()) cs.push_back (x);
        }
        const std::string lb = dir + "/c" + std::to_string (idx) + "_lin.raw";
        Case l { c, load (lb + ".in"), load (lb), "c" + std::to_string (idx) + " small-signal noise", true };
        if (! l.ref.empty()) cs.push_back (l);
        ++idx;
    }
    std::printf ("%zu reference cases\n", cs.size());
    FuzzFit d;
    double p[kP] = { std::log (d.aol[0]), std::log (d.vPos[0]), std::log (d.vNeg[0]), std::log (d.q4Gain), std::log (d.q4Pos),
                     std::log (d.q4Neg), std::log (d.toneSrcOhms), std::log (d.rpi) };
    std::printf ("start objective %.3f\n", objective (cs, p));
    if (doFit)
    {
        // Nelder-Mead
        std::vector<std::vector<double>> s (kP + 1, std::vector<double> (p, p + kP));
        std::vector<double> fv (kP + 1);
        for (int i = 0; i < kP; ++i) s[(size_t) i + 1][(size_t) i] += 0.3;
        for (int i = 0; i <= kP; ++i) fv[(size_t) i] = objective (cs, s[(size_t) i].data());
        for (int it = 0; it < 400; ++it)
        {
            std::vector<int> o (kP + 1); for (int i = 0; i <= kP; ++i) o[(size_t) i] = i;
            std::sort (o.begin(), o.end(), [&] (int a, int b) { return fv[(size_t) a] < fv[(size_t) b]; });
            std::vector<std::vector<double>> s2; std::vector<double> f2;
            for (int i : o) { s2.push_back (s[(size_t) i]); f2.push_back (fv[(size_t) i]); }
            s = s2; fv = f2;
            if (it % 20 == 0) std::printf ("it %3d best %.4f\n", it, fv[0]), std::fflush (stdout);
            if (fv[(size_t) kP] - fv[0] < 1e-4) break;
            std::vector<double> c (kP, 0.0);
            for (int i = 0; i < kP; ++i) for (int j = 0; j < kP; ++j) c[(size_t) j] += s[(size_t) i][(size_t) j] / kP;
            auto at = [&] (double t) { std::vector<double> x (kP); for (int j = 0; j < kP; ++j) x[(size_t) j] = c[(size_t) j] + t * (s[(size_t) kP][(size_t) j] - c[(size_t) j]); return x; };
            auto xr = at (-1.0); const double fr = objective (cs, xr.data());
            if (fr < fv[0]) { auto xe = at (-2.0); const double fe = objective (cs, xe.data()); if (fe < fr) { s[(size_t) kP] = xe; fv[(size_t) kP] = fe; } else { s[(size_t) kP] = xr; fv[(size_t) kP] = fr; } }
            else if (fr < fv[(size_t) kP - 1]) { s[(size_t) kP] = xr; fv[(size_t) kP] = fr; }
            else
            {
                auto xc = at (0.5); const double fc = objective (cs, xc.data());
                if (fc < fv[(size_t) kP]) { s[(size_t) kP] = xc; fv[(size_t) kP] = fc; }
                else for (int i = 1; i <= kP; ++i) { for (int j = 0; j < kP; ++j) s[(size_t) i][(size_t) j] = s[0][(size_t) j] + 0.5 * (s[(size_t) i][(size_t) j] - s[0][(size_t) j]); fv[(size_t) i] = objective (cs, s[(size_t) i].data()); }
            }
        }
        for (int j = 0; j < kP; ++j) p[j] = s[0][(size_t) j];
    }
    std::printf ("final objective %.4f\n", objective (cs, p, true));
    const auto f = toFit (p);
    std::printf ("fit: aol %.3f vPos %.3f vNeg %.3f q4Gain %.3f q4Pos %.3f q4Neg %.3f toneSrc %.1f rpi %.0f\n",
                 f.aol[0], f.vPos[0], f.vNeg[0], f.q4Gain, f.q4Pos, f.q4Neg, f.toneSrcOhms, f.rpi);
    return 0;
}
