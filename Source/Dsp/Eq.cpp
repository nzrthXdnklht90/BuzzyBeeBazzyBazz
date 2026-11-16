#include "Eq.h"

namespace bassforge
{
namespace
{
constexpr double kCtl = 32.0; // control update interval (samples)
inline double tw (double hz, double fs) noexcept { return SvfCoeffs::prewarp (hz, fs); }
}

// ============================================================================ 7-band
SvfCoeffs ParametricEq7::coeffs (const EqBand& b, int index, double fs, double amount) noexcept
{
    SvfCoeffs c;
    const double t = tw (std::clamp (b.freq, 20.0, 20000.0), fs);
    const double q = std::clamp (b.q, 0.1, 18.0);
    const bool outer = index == 0 || index == kBands - 1;
    const EqBandType type = outer ? b.type : EqBandType::Bell;
    switch (type)
    {
        case EqBandType::Bell:
            c.makeBell (t, b.gainDb * amount, q);
            break;
        case EqBandType::Shelf:
            if (index == 0) c.makeLowShelf (t, b.gainDb * amount, 1.0 / std::max (0.3, std::min (q, 2.0)));
            else            c.makeHighShelf (t, b.gainDb * amount, 1.0 / std::max (0.3, std::min (q, 2.0)));
            break;
        case EqBandType::Cut:
            if (index == 0) c.makeHighpass (t, 1.0 / std::max (0.3, std::min (q, 4.0)));
            else            c.makeLowpass (t, 1.0 / std::max (0.3, std::min (q, 4.0)));
            break;
    }
    return c;
}

double ParametricEq7::responseDb (const EqBand* bands, double hz, double fs) noexcept
{
    double mag = 1.0;
    for (int i = 0; i < kBands; ++i)
        if (bands[i].on)
            mag *= coeffs (bands[i], i, fs).magnitude (hz, fs);
    return gainToDb (mag);
}

void ParametricEq7::prepare (double rate)
{
    fs = rate;
    for (int i = 0; i < kBands; ++i)
    {
        sLogF[(size_t) i].setTime (30.0, fs / kCtl);
        sGain[(size_t) i].setTime (30.0, fs / kCtl);
        sQ[(size_t) i].setTime (30.0, fs / kCtl);
        sAmt[(size_t) i].setTime (15.0, fs / kCtl);
    }
    first = true;
    reset();
}

void ParametricEq7::reset() noexcept { for (auto& ch : f) for (auto& x : ch) x.reset(); }

void ParametricEq7::setParams (const EqBand* bands) noexcept
{
    for (int i = 0; i < kBands; ++i)
    {
        const auto& b = bands[i];
        const double lf = std::log (std::clamp (b.freq, 20.0, 20000.0));
        const double amt = b.on ? 1.0 : 0.0;
        if (first) { sLogF[(size_t) i].snap (lf); sGain[(size_t) i].snap (b.gainDb); sQ[(size_t) i].snap (b.q); sAmt[(size_t) i].snap (amt); }
        EqBand s = b;
        s.freq = std::exp (sLogF[(size_t) i].next (lf));
        s.gainDb = sGain[(size_t) i].next (b.gainDb);
        s.q = sQ[(size_t) i].next (b.q);
        const double a = sAmt[(size_t) i].next (amt);
        const bool outer = i == 0 || i == kBands - 1;
        isCut[(size_t) i] = outer && b.type == EqBandType::Cut;
        cutMix[(size_t) i] = a;
        c[(size_t) i] = coeffs (s, i, fs, isCut[(size_t) i] ? 1.0 : a);
    }
    first = false;
}

void ParametricEq7::process (double* l, double* r, int n) noexcept
{
    double* ch[2] = { l, r };
    for (int k = 0; k < 2; ++k)
    {
        auto& fk = f[(size_t) k];
        for (int i = 0; i < n; ++i)
        {
            double x = ch[k][i];
            for (int b = 0; b < kBands; ++b)
            {
                const double y = fk[(size_t) b].process (x, c[(size_t) b]);
                x = isCut[(size_t) b] ? x + (y - x) * cutMix[(size_t) b] : y;
            }
            ch[k][i] = x;
        }
    }
}

// ============================================================================ DI EQ
void DiEq::prepare (double rate)
{
    fs = rate;
    for (auto& x : s) x.setTime (30.0, fs / kCtl);
    first = true;
    reset();
}

void DiEq::setParams (const EngineParams& p) noexcept
{
    const double tgt[7] = { p.diBassDb, p.diMidDb, p.diTrebleDb, std::log (p.diBassHz), std::log (p.diMidHz), p.diMidQ, std::log (p.diTrebleHz) };
    double v[7];
    for (int i = 0; i < 7; ++i) { if (first) s[i].snap (tgt[i]); v[i] = s[i].next (tgt[i]); }
    first = false;
    c[0].makeLowShelf (tw (std::exp (v[3]), fs), v[0], 1.0 / 0.6);
    c[1].makeBell (tw (std::exp (v[4]), fs), v[1], std::clamp (v[5], 0.2, 6.0));
    c[2].makeHighShelf (tw (std::exp (v[6]), fs), v[2], 1.0 / 0.6);
}

// ============================================================================ programme EQ
void ProgramEq::coeffs (const Settings& s, double fs, std::array<SvfCoeffs, 4>& out) noexcept
{
    const double lf = kLowFreqs[std::clamp (s.lowIdx, 0, 3)];
    // low boost: resonant shelf (k < sqrt2 gives the gentle bump at the corner), up to ~ +13.5 dB at 10
    out[0].makeLowShelf (tw (lf * 1.4, fs), 1.35 * std::clamp (s.lowBoost, 0.0, 10.0), 1.0 / 0.9);
    // low attenuation: very broad (Q 0.35) shelf just above the boost corner, up to -13 dB at 10.
    // Corner/Q/scale were fitted (tools/analysis/fit_program_eq.py) to the documented behaviour:
    // boost 5 + atten 5 at 60 Hz -> about +2.5 dB at 40 Hz with a -2.5 dB dip near 150 Hz.
    out[1].makeLowShelf (tw (lf * 1.5, fs), -1.3 * std::clamp (s.lowAtten, 0.0, 10.0), 1.0 / 0.35);
    // high boost: bell, bandwidth 0 = sharp (Q 2.5) .. 1 = broad (Q 0.45), up to ~ +16 dB at 10
    const double q = 2.5 * std::pow (0.45 / 2.5, std::clamp (s.highBw, 0.0, 1.0));
    out[2].makeBell (tw (kHighBoostFreqs[std::clamp (s.hbIdx, 0, 6)], fs), 1.6 * std::clamp (s.highBoost, 0.0, 10.0), q);
    // high attenuation: very gentle shelf (low Q emulates the first-order passive slope), up to -16 dB
    out[3].makeHighShelf (tw (kHighAttenFreqs[std::clamp (s.haIdx, 0, 2)] * 0.5, fs), -1.6 * std::clamp (s.highAtten, 0.0, 10.0), 1.0 / 0.4);
}

double ProgramEq::responseDb (const Settings& s, double hz, double fs) noexcept
{
    std::array<SvfCoeffs, 4> c;
    coeffs (s, fs, c);
    double m = 1.0;
    for (auto& x : c) m *= x.magnitude (hz, fs);
    return gainToDb (m);
}

void ProgramEq::prepare (double rate)
{
    fs = rate;
    for (auto* x : { &sLb, &sLa, &sHb, &sHa, &sBw, &sLf, &sHbf, &sHaf }) x->setTime (40.0, fs / kCtl);
    first = true;
    reset();
}

void ProgramEq::setParams (const EngineParams& p) noexcept
{
    if (first)
    {
        sLb.snap (p.lowBoost); sLa.snap (p.lowAtten); sHb.snap (p.highBoost); sHa.snap (p.highAtten); sBw.snap (p.highBw);
        first = false;
    }
    Settings s { sLb.next (p.lowBoost), sLa.next (p.lowAtten), sHb.next (p.highBoost), sBw.next (p.highBw), sHa.next (p.highAtten),
                 p.lowFreqIdx, p.highBoostFreqIdx, p.highAttenFreqIdx };
    coeffs (s, fs, c);
}

void ProgramEq::process (double* l, double* r, int n) noexcept
{
    double* ch[2] = { l, r };
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < n; ++i)
        {
            double x = ch[k][i];
            for (int b = 0; b < 4; ++b) x = f[(size_t) k][(size_t) b].process (x, c[(size_t) b]);
            ch[k][i] = x;
        }
}
} // namespace bassforge
