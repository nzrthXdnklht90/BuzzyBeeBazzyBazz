#include "FuzzVoice.h"

namespace bassforge
{
namespace
{
constexpr double kVt = 0.025852;   // thermal voltage at 27 C (as in the Big Dill device models)
// Big Dill calibrated device set (NAM-capture fit of a real MkIIS): Si pair Is/N, LED pair Is (N = 3.73)
constexpr double kSiIs = 2.308e-8, kSiN = 1.878, kLedIs = 9.951e-12, kLedN = 3.73;
inline double potA (double rot) { rot = std::clamp (rot, 0.0, 1.0); return (std::pow (81.0, rot) - 1.0) / 80.0; } // audio taper
}

// ============================================================================ LinearNet
void LinearNet::begin (int nodes, double rate)
{
    n = nodes; fs = rate; nc = nr = 0; gSrc = 0.0;
}
void LinearNet::addR (int a, int b, double ohms) { ra[nr] = a; rb[nr] = b; rg[nr] = 1.0 / std::max (ohms, 1e-3); ++nr; }
void LinearNet::addC (int a, int b, double f) { ca[nc] = a; cb[nc] = b; gc[nc] = 2.0 * f * fs; ++nc; }
void LinearNet::setSource (int node, double ohms) { src = node; gSrc = 1.0 / ohms; }
void LinearNet::reset() { for (auto& h : hist) h = 0.0; }

void LinearNet::finalize()
{
    double G[kMaxN][kMaxN] {};
    auto stamp = [&] (int a, int b, double g) {
        if (a >= 0) G[a][a] += g;
        if (b >= 0) G[b][b] += g;
        if (a >= 0 && b >= 0) { G[a][b] -= g; G[b][a] -= g; }
    };
    for (int k = 0; k < nr; ++k) stamp (ra[k], rb[k], rg[k]);
    for (int k = 0; k < nc; ++k) stamp (ca[k], cb[k], gc[k]);
    G[src][src] += gSrc;
    // Gauss-Jordan inverse (n <= 10, control rate only)
    double M[kMaxN][2 * kMaxN] {};
    for (int i = 0; i < n; ++i) { for (int j = 0; j < n; ++j) M[i][j] = G[i][j]; M[i][n + i] = 1.0; }
    for (int c = 0; c < n; ++c)
    {
        int p = c;
        for (int r = c + 1; r < n; ++r) if (std::abs (M[r][c]) > std::abs (M[p][c])) p = r;
        if (p != c) for (int j = 0; j < 2 * n; ++j) std::swap (M[p][j], M[c][j]);
        const double d = M[c][c];
        for (int j = 0; j < 2 * n; ++j) M[c][j] /= d;
        for (int r = 0; r < n; ++r)
            if (r != c && M[r][c] != 0.0)
            {
                const double f = M[r][c];
                for (int j = 0; j < 2 * n; ++j) M[r][j] -= f * M[c][j];
            }
    }
    for (int i = 0; i < n; ++i) for (int j = 0; j < n; ++j) inv[i * kMaxN + j] = M[i][n + j];
}

// ============================================================================ FuzzStage
void FuzzStage::prepare (double rate, double rinOhms, double cinF, double rfOhms, double cfF, double rbOhms, double rpi)
{
    fs = rate; T = 1.0 / fs;
    rf = rfOhms; gcf = 2.0 * cfF * fs; gb = 1.0 / rbOhms + 1.0 / rpi;
    setInput (rinOhms, cinF);
    reset();
}

void FuzzStage::setInput (double rinOhms, double cinF) noexcept
{
    cin = cinF;
    rinEff = rinOhms + T / (2.0 * cin);
}

void FuzzStage::reset() noexcept
{
    vcin = iin = jf = vb = 0.0;
    for (auto& b : br) { b.vcap = 0.0; b.iPrev = 0.0; }
}

inline double FuzzStage::swing (double z, double& dz) const noexcept
{
    // knee exponent 2: z / sqrt(1 + (z/L)^2), derivative (1 + (z/L)^2)^(-3/2)
    const double a = z / (z >= 0.0 ? vp : vn);
    const double base = 1.0 + a * a;
    const double s = 1.0 / std::sqrt (base);
    dz = s / base;
    return z * s;
}

void FuzzStage::Branch::build (double rsEff)
{
    // exact solve of v + R i(v) = w at each grid point (control rate only)
    double v = 0.0;
    for (int k = 0; k <= kN; ++k)
    {
        const double w = -kW + 2.0 * kW * k / kN;
        double i = 0.0, g = 0.0;
        for (int it = 0; it < 60; ++it)
        {
            const double e = std::clamp (v / nvt, -80.0, 80.0);
            const double ep = std::exp (e), en = 1.0 / ep;
            i = is * (ep - en); g = is * (ep + en) / nvt;
            const double f = v + rsEff * i - w;
            double dv = -f / (1.0 + rsEff * g);
            dv = std::clamp (dv, -0.25, 0.25);
            v += dv;
            if (std::abs (dv) < 1e-12) break;
        }
        ti[k] = (float) i;
        tg[k] = (float) (g / (1.0 + rsEff * g) * (2.0 * kW / kN));   // slope x grid step
    }
    builtRs = rsEff;
}

void FuzzStage::updateBranches()
{
    for (auto& b : br)
        if (b.on)
        {
            const double rsEff = b.rs + T / (2.0 * b.c);
            if (std::abs (rsEff - b.builtRs) > 1e-6 * std::max (1.0, rsEff)) b.build (rsEff);
        }
}

double FuzzStage::process (double u) noexcept
{
    const double vcin0 = vcin + (T / (2.0 * cin)) * iin;
    double vcap0[2], rsEff[2];
    for (int k = 0; k < 2; ++k)
    {
        auto& b = br[(size_t) k];
        if (! b.on) continue;
        vcap0[k] = b.vcap + (T / (2.0 * b.c)) * b.iPrev;
        rsEff[k] = b.rs + T / (2.0 * b.c);
    }
    const double gf = 1.0 / rf + gcf;
    double x = vb, y = 0.0, iIn = 0.0, ib[2] = { 0.0, 0.0 };
    for (int it = 0; it < 2; ++it)   // warm-started; 2 iterations match 6 to 0.01 dB on the reference set
    {
        double dS;
        y = swing (-A * x, dS);
        const double dy = -A * dS;
        iIn = (u - x - vcin0) / rinEff;
        const double vfb = y - x;
        double F = iIn + vfb * gf - jf - x * gb;
        double dF = -1.0 / rinEff + (dy - 1.0) * gf - gb;
        for (int k = 0; k < 2; ++k)
        {
            auto& b = br[(size_t) k];
            if (! b.on) continue;
            double didw;
            const double i = b.eval (vfb - vcap0[k], didw);
            ib[k] = i;
            F += i;
            dF += didw * (dy - 1.0);
        }
        x += std::clamp (-F / dF, -0.5, 0.5);
    }
    double dS;
    y = swing (-A * x, dS);
    // commit states (trapezoidal companions)
    iIn = (u - x - vcin0) / rinEff;
    vcin = vcin0 + (T / (2.0 * cin)) * iIn;
    iin = iIn;
    jf = flushDenormal (2.0 * gcf * (y - x) - jf);
    for (int k = 0; k < 2; ++k)
    {
        auto& b = br[(size_t) k];
        if (! b.on) continue;
        b.vcap = flushDenormal (vcap0[k] + (T / (2.0 * b.c)) * ib[k]);
        b.iPrev = ib[k];
    }
    vb = x;
    if (! std::isfinite (y)) { reset(); return 0.0; }
    return y;
}

// ============================================================================ FuzzVoice
void FuzzVoice::prepare (double rate)
{
    fs = rate;
    // Q1: 33k + 600R source, 100n coupling; Q2/Q3: 10k (+ sustain source), 1u coupling. 470k/100k voicing, 470p.
    s1.prepare (fs, 33e3 + 600.0, 100e-9, 470e3, 470e-12, 100e3, fit.rpi);
    s2.prepare (fs, 10e3, 1e-6, 470e3, 470e-12, 100e3, fit.rpi);
    s3.prepare (fs, 10e3, 1e-6, 470e3, 470e-12, 100e3, fit.rpi);
    for (auto* s : { &s1, &s2, &s3 }) (void) s;
    s1.setGain (fit.aol[0], fit.vPos[0], fit.vNeg[0], 2.0);
    s2.setGain (fit.aol[1], fit.vPos[1], fit.vNeg[1], 2.0);
    s3.setGain (fit.aol[2], fit.vPos[2], fit.vNeg[2], 2.0);
    // Q2 diode branches: Si pair via 100n, LED pair via 47n, blended by the 500k Clip pot
    s2.br[0] = { true, kSiIs, kSiN * kVt, 0.0, 100e-9 };
    s2.br[1] = { true, kLedIs, kLedN * kVt, 0.0, 47e-9 };
    // Q3: Si pair via 100n behind the 470k (audio) Crunch rheostat
    s3.br[0] = { true, kSiIs, kSiN * kVt, 0.0, 100e-9 };
    s3.br[1].on = false;
    for (auto& b : s2.br) b.builtRs = -1.0;
    for (auto& b : s3.br) b.builtRs = -1.0;
    dcOut.setCutoff (5.0, fs);
    toneDirty = true;
    setControls (ctl);
    reset();
}

void FuzzVoice::setFit (const FuzzFit& f)
{
    fit = f;
    prepare (fs);
}

void FuzzVoice::reset() noexcept
{
    s1.reset(); s2.reset(); s3.reset(); tone.reset(); dcOut.reset();
}

void FuzzVoice::setControls (const FuzzControls& c) noexcept
{
    const bool toneChange = c.filter != ctl.filter || c.scoop != ctl.scoop;
    ctl = c;
    // Sustain 100kB: pin3 <- Q1, pin1 -> 1k -> ground, wiper -> Q2 input. Thevenin equivalent:
    const double rot = std::clamp (c.sustain, 0.0, 1.0);
    const double upper = std::max (1.0, (1.0 - rot) * 100e3), lower = rot * 100e3 + 1e3;
    sustainFrac = lower / (upper + lower);
    s2.setInput (10e3 + upper * lower / (upper + lower), 1e-6);
    // Clip 500kB: Si branch gets the pin1-wiper part, LED branch the wiper-pin3 part
    const double cl = std::clamp (c.clip, 0.0, 1.0);
    s2.br[0].rs = std::max (1.0, 500e3 * cl);
    s2.br[1].rs = std::max (1.0, 500e3 * (1.0 - cl));
    // Crunch 470kA rheostat in series with the Q3 Si pair
    s3.br[0].rs = std::max (1.0, 470e3 * potA (c.crunch));
    s2.updateBranches();
    s3.updateBranches();
    if (toneChange || toneDirty) rebuildTone();
}

void FuzzVoice::rebuildTone()
{
    // nodes: 0 c3(src) 1 t 2 lp 3 hp 4 v1 5 v3 6 fw 7 sco 8 b4
    const double f = std::clamp (ctl.filter, 0.0, 1.0), sc = std::clamp (ctl.scoop, 0.0, 1.0);
    tone.begin (9, fs);
    tone.setSource (0, fit.toneSrcOhms);
    tone.addC (0, 1, 1e-6);                 // C11
    tone.addR (1, 2, 33e3);                 // R3
    tone.addC (2, -1, 100e-9);              // C4
    tone.addC (1, 5, 33e-9);                // C9
    tone.addC (1, 4, 3.3e-9);               // C10
    tone.addR (4, 3, 500e3);                // Voice 500kB fully CW (factory): pin1-wiper = 500k
    tone.addR (3, 5, 1.0);                  //                                  wiper-pin3 ~ 0
    tone.addR (2, 6, std::max (1.0, 100e3 * f));          // Filter 100kB pin1(lp)-wiper
    tone.addR (6, 3, std::max (1.0, 100e3 * (1.0 - f)));  //               wiper-pin3(hp)
    tone.addR (3, 7, std::max (1.0, 50e3 * sc));          // Scoop 50kB rheostat
    tone.addR (7, -1, 1.5e3);               // R17
    tone.addC (6, 8, 1e-6);                 // C1
    tone.addR (8, -1, 1.0 / (1.0 / 470e3 + 1.0 / 100e3 + 1.0 / (400.0 * 1.5e3))); // Q4 input: RF || RB || beta*Re
    tone.setOutput (8);
    tone.finalize();
    toneDirty = false;
}

void FuzzVoice::process (double* buf, int n) noexcept
{
    for (int i = 0; i < n; ++i)
        buf[i] = processVolts (buf[i] * 2.0) * outScale;
}
} // namespace bassforge
