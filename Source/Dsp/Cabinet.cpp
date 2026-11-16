#include "Cabinet.h"

namespace bassforge
{
namespace
{
// PROVISIONAL voicings (see header). Names are descriptive, not manufacturer marks.
const CabProfile kProfiles[4] = {
    // name           ported resHz resQ  bump          warm          dip            brk            brk2           lp         singleLp
    { "4x10 Bright",  true,  55,  0.75, 100, 2.0, 1.2, 250, 0.5, 1.0, 420, -2.5, 1.0, 2500, 4.0, 2.0, 4500, 3.0, 2.5, 5500, 0.75, false },
    { "8x10 Classic", false, 62,  0.95, 120, 2.5, 1.1, 260, 1.5, 0.9, 700, -2.0, 1.2, 1800, 2.0, 1.6, 3000, 1.0, 2.0, 4000, 0.80, false },
    { "2x15 Deep",    true,  40,  0.80, 80,  3.0, 1.0, 200, 1.0, 0.8, 500, -3.0, 0.9, 1500, 2.0, 1.5, 2400, 1.0, 2.0, 3000, 0.75, false },
    { "4x10 Modern",  true,  45,  0.75, 90,  1.5, 1.0, 230, 0.5, 0.9, 450, -1.5, 0.7, 2200, 1.5, 1.4, 4200, 2.0, 1.0, 9000, 0.70, true  },
};

inline double tw (double hz, double fs) noexcept { return SvfCoeffs::prewarp (hz, fs); }

struct CapsuleVoice { double hpHz, hpQ, presHz, presDb, presQ, lpHz, lpQ, proxMaxDb; };
const CapsuleVoice kCaps[3] = {
    { 70.0, 0.7, 5200.0, 4.5, 1.4, 13000.0, 0.7, 6.0 },   // dynamic (cardioid, presence peak)
    { 25.0, 0.7, 10000.0, 2.0, 0.8, 18000.0, 0.7, 4.0 },  // condenser (large diaphragm, flat, air)
    { 20.0, 0.7, 4000.0, -3.0, 0.5, 7000.0, 0.6, 9.0 },   // ribbon (figure-8: dark, strong proximity)
};
constexpr double kSpeedOfSoundInPerSec = 13504.0;
} // namespace

const CabProfile& cabProfile (CabModel m) noexcept { return kProfiles[(int) m]; }

// ============================================================================ chain
void CabChain::prepare (double rate)
{
    fs = rate;
    for (auto& d : delay) d.prepare ((int) Cabinet::maxMicDelaySamples (fs) + 2);
    setupCommon();
    reset();
}

void CabChain::reset() noexcept
{
    for (auto& f : common) f.reset();
    for (auto& m : micF) for (auto& f : m) f.reset();
    for (auto& d : delay) d.reset();
}

double CabChain::trimDb (int mic) const noexcept
{
    return trimTable ? (*trimTable)[(size_t) cab][(size_t) types[(size_t) mic]] : 0.0;
}

void CabChain::configure (CabModel c, MicType t1, MicType t2, MicAlign a) noexcept
{
    cab = c; types = { t1, t2 }; align = a;
    setupCommon();
}

void CabChain::setupCommon() noexcept
{
    const auto& p = kProfiles[(int) cab];
    commonC[0].makeHighpass (tw (p.resHz, fs), 1.0 / p.resQ);
    if (p.ported) commonC[1].makeHighpass (tw (p.resHz * 0.8, fs), 1.0 / 0.6); // vented box: steeper below tuning
    else commonC[1].makeIdentity();
    commonC[2].makeBell (tw (p.bumpHz, fs), p.bumpDb, p.bumpQ);
    commonC[3].makeBell (tw (p.warmHz, fs), p.warmDb, p.warmQ);
    commonC[4].makeBell (tw (p.dipHz, fs), p.dipDb, p.dipQ);
    commonC[5].makeBell (tw (p.brkHz, fs), p.brkDb, p.brkQ);
    commonC[6].makeBell (tw (p.brk2Hz, fs), p.brk2Db, p.brk2Q);
    commonC[7].makeLowpass (tw (p.lpHz * 1.25, fs), 1.0 / p.lpQ);
}

void CabChain::setupMic (int m, const MicState& st) noexcept
{
    const auto& p = kProfiles[(int) cab];
    const auto& cv = kCaps[(int) st.type];
    const double pos = std::clamp (st.position, 0.0, 1.0);
    const double d = std::clamp (st.distanceIn, 0.0, 6.0);
    auto& c = micC[(size_t) m];
    // radial position: cap is brightest, cone edge darkest and warmest
    const double posLp = p.lpHz * std::pow (2.0, 0.6 - 1.7 * pos);
    if (p.singleLp) c[0].makeLowpass (tw (std::max (posLp, 2500.0) * 1.3, fs), 1.0 / 0.6);
    else c[0].makeLowpass (tw (posLp, fs), 1.0 / p.lpQ);
    c[1].makeBell (tw (p.brkHz, fs), -5.0 * std::pow (pos, 1.2), 1.0);
    c[2].makeBell (tw (300.0, fs), 2.0 * pos, 0.8);
    // proximity effect (directional capsules), decaying with distance
    c[3].makeLowShelf (tw (150.0, fs), cv.proxMaxDb * std::exp (-d / 1.8));
    // capsule voicing
    c[4].makeHighpass (tw (cv.hpHz, fs), 1.0 / cv.hpQ);
    c[5].makeBell (tw (cv.presHz, fs), cv.presDb, cv.presQ);
    c[6].makeLowpass (tw (cv.lpHz / (1.0 + 0.06 * d), fs), 1.0 / cv.lpQ);
    // distance: about -3 dB per doubling of (d + 2 in) (large near-field source), acoustic delay
    micGain[(size_t) m] = std::sqrt (2.0 / (d + 2.0)) * dbToGain (trimDb (m));
    distIn[(size_t) m] = d;
}

void CabChain::update (const MicState& m1, const MicState& m2) noexcept
{
    setupMic (0, m1);
    setupMic (1, m2);
    const double base = 1.0;
    if (align == MicAlign::Natural)
    {
        // preserve the path-length difference; the nearer mic is the reference (no added bulk delay)
        const double dmin = std::min (distIn[0], distIn[1]);
        micDelay[0] = base + (distIn[0] - dmin) / kSpeedOfSoundInPerSec * fs;
        micDelay[1] = base + (distIn[1] - dmin) / kSpeedOfSoundInPerSec * fs;
    }
    else
        micDelay = { base, base };
}

// ============================================================================ block
void Cabinet::prepare (double rate)
{
    fs = rate;
    calibrate();
    for (auto& c : chains) { c.trimTable = &trims; c.prepare (fs); }
    fadeStep = 1.0 / (0.025 * fs);
    for (auto* r : { &g1L, &g1R, &g2L, &g2R }) r->setRampLength ((int) (0.015 * fs));
    for (auto& s : pos) s.setTime (40.0, fs / 32.0);
    for (auto& s : dist) s.setTime (40.0, fs / 32.0);
    firstUpdate = true;
    reset();
}

void Cabinet::reset() noexcept
{
    for (auto& c : chains) c.reset();
    fade = 1.0;
}

void Cabinet::calibrate()
{
    // Level-match cabinets and capsules: measure each (cab, type) on band-limited noise
    // (150 Hz - 5 kHz, the region the cabinet sees above the low band) and store the inverse gain.
    CabChain c;
    c.prepare (fs);
    Prng rng; rng.seed (1234);
    const int warm = (int) (0.05 * fs), len = (int) (0.4 * fs);
    OnePole hp1, hp2, lp1, lp2;
    for (int cab = 0; cab < 4; ++cab)
        for (int t = 0; t < 3; ++t)
        {
            c.trimTable = nullptr;
            c.configure ((CabModel) cab, (MicType) t, (MicType) t, MicAlign::Auto);
            MicState ms; ms.type = (MicType) t; ms.position = 1.0 / 3.0; ms.distanceIn = 1.0;
            c.update (ms, ms);
            c.reset();
            hp1.setCutoff (150, fs); hp2.setCutoff (150, fs); lp1.setCutoff (5000, fs); lp2.setCutoff (5000, fs);
            hp1.reset(); hp2.reset(); lp1.reset(); lp2.reset();
            double eIn = 0, eOut = 0;
            for (int i = 0; i < warm + len; ++i)
            {
                const double x = lp2.lp (lp1.lp (hp2.hp (hp1.hp (rng.uniform() * 2.0 - 1.0))));
                double a, b;
                c.process (x, a, b);
                if (i >= warm) { eIn += x * x; eOut += a * a; }
            }
            // the distance gain at 1 in is part of the musical behaviour; trim relative to it
            trims[(size_t) cab][(size_t) t] = 10.0 * std::log10 (eIn / std::max (eOut, 1e-20)) + 20.0 * std::log10 (std::sqrt (2.0 / 3.0));
        }
}

void Cabinet::setParams (const EngineParams& p) noexcept
{
    const auto& cur = chains[active];
    const bool needSwap = cur.getCab() != p.cab || cur.getType (0) != p.mic[0].type
                          || cur.getType (1) != p.mic[1].type || cur.getAlign() != p.align;
    if (needSwap)
    {
        if (fade < 1.0) fade = 1.0; // a change during a fade: finish instantly and start a new one
        active ^= 1;
        chains[active].reset();
        chains[active].configure (p.cab, p.mic[0].type, p.mic[1].type, p.align);
        fade = 0.0;
    }
    MicState st[2];
    for (int m = 0; m < 2; ++m)
    {
        if (firstUpdate) { pos[m].snap (p.mic[m].position); dist[m].snap (p.mic[m].distanceIn); }
        st[m].type = p.mic[m].type;
        st[m].position = pos[m].next (p.mic[m].position);
        st[m].distanceIn = dist[m].next (p.mic[m].distanceIn);
    }
    chains[active].update (st[0], st[1]);
    if (fade < 1.0) chains[active ^ 1].update (st[0], st[1]);

    // mic mixing gains: level, balance (equal power), polarity, enable, stereo spread (equal power pan)
    const double bal = std::clamp (p.micBalance, -1.0, 1.0);
    const double b1 = std::cos ((bal + 1.0) * 0.25 * kPi) * std::sqrt (2.0);
    const double b2 = std::sin ((bal + 1.0) * 0.25 * kPi) * std::sqrt (2.0);
    double g[2];
    g[0] = (p.mic[0].on ? 1.0 : 0.0) * dbToGain (p.mic[0].levelDb) * std::min (1.0, b1) * (p.mic[0].invert ? -1.0 : 1.0);
    g[1] = (p.mic[1].on ? 1.0 : 0.0) * dbToGain (p.mic[1].levelDb) * std::min (1.0, b2) * (p.mic[1].invert ? -1.0 : 1.0);
    // two mics summed: keep the combined level close to one mic (-3 dB when both are on)
    if (p.mic[0].on && p.mic[1].on) { g[0] *= 0.7071; g[1] *= 0.7071; }
    const double s = std::clamp (p.micSpread, 0.0, 1.0);
    const double pan1 = 0.5 - 0.5 * s, pan2 = 0.5 + 0.5 * s; // 0 = left, 1 = right
    const auto L = [] (double pan) { return std::cos (pan * 0.5 * kPi) * std::sqrt (2.0); };
    const auto R = [] (double pan) { return std::sin (pan * 0.5 * kPi) * std::sqrt (2.0); };
    g1L.setTarget (g[0] * L (pan1)); g1R.setTarget (g[0] * R (pan1));
    g2L.setTarget (g[1] * L (pan2)); g2R.setTarget (g[1] * R (pan2));
    if (firstUpdate)
    {
        g1L.snapTo (g1L.getTarget()); g1R.snapTo (g1R.getTarget());
        g2L.snapTo (g2L.getTarget()); g2R.snapTo (g2R.getTarget());
        firstUpdate = false;
    }
}

void Cabinet::process (const double* in, double* outL, double* outR, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        double m1, m2;
        chains[active].process (in[i], m1, m2);
        if (fade < 1.0)
        {
            double o1, o2;
            chains[active ^ 1].process (in[i], o1, o2);
            fade = std::min (1.0, fade + fadeStep);
            const double a = fade, b = 1.0 - fade; // linear: the two renders are strongly correlated
            m1 = a * m1 + b * o1;
            m2 = a * m2 + b * o2;
        }
        const double a1L = g1L.next(), a1R = g1R.next(), a2L = g2L.next(), a2R = g2R.next();
        outL[i] = a1L * m1 + a2L * m2;
        outR[i] = a1R * m1 + a2R * m2;
    }
}
} // namespace bassforge
