// BASSFORGE - equalisers: 7-band parametric (Drive channel), 3-band DI EQ, and the programme
// "Mix EQ" on the master bus. Copyright (c) 2026 Nazareth. Original code.
#pragma once

#include "Filters.h"
#include "EngineParams.h"
#include <array>

namespace bassforge
{
// ============================================================================ 7-band parametric
class ParametricEq7
{
public:
    static constexpr int kBands = 7;
    /** Coefficients for band i (0..6) with enable amount folded into the gain (bell/shelf). */
    static SvfCoeffs coeffs (const EqBand& b, int index, double fs, double amount = 1.0) noexcept;
    /** Total response in dB at hz (for the UI curve / tests). */
    static double responseDb (const EqBand* bands, double hz, double fs) noexcept;

    void prepare (double fs);
    void reset() noexcept;
    void setParams (const EqBand* bands) noexcept;   // control rate (smoothing inside)
    void process (double* l, double* r, int n) noexcept;

private:
    double fs = 48000.0;
    std::array<SvfCoeffs, kBands> c;
    std::array<std::array<Svf, kBands>, 2> f;
    std::array<double, kBands> cutMix {};  // dry/wet for Cut bands (enable fade)
    std::array<bool, kBands> isCut {};
    std::array<Smoother, kBands> sLogF, sGain, sQ, sAmt;
    bool first = true;
};

// ============================================================================ DI 3-band
class DiEq
{
public:
    void prepare (double fs);
    void reset() noexcept { for (auto& x : f) x.reset(); }
    void setParams (const EngineParams& p) noexcept;
    inline double process (double x) noexcept
    {
        for (int i = 0; i < 3; ++i) x = f[(size_t) i].process (x, c[(size_t) i]);
        return x;
    }

private:
    double fs = 48000.0;
    std::array<SvfCoeffs, 3> c; std::array<Svf, 3> f;
    Smoother s[7];
    bool first = true;
};

// ============================================================================ programme Mix EQ
/** Inspired by the curves of passive programme equalisers: a resonant low boost and a low shelf cut
    whose corner sits slightly higher, so using both at once gives the characteristic "boost the
    fundamental, dip the mud just above it" shape instead of cancelling; a bandwidth-variable high
    boost; and a gentle first-order-like high attenuation shelf. Same DSP drives the macro and the
    expanded views. */
class ProgramEq
{
public:
    static constexpr double kLowFreqs[4] = { 20.0, 30.0, 60.0, 100.0 };
    static constexpr double kHighBoostFreqs[7] = { 3000.0, 4000.0, 5000.0, 8000.0, 10000.0, 12000.0, 16000.0 };
    static constexpr double kHighAttenFreqs[3] = { 5000.0, 10000.0, 20000.0 };
    struct Settings { double lowBoost, lowAtten, highBoost, highBw, highAtten; int lowIdx, hbIdx, haIdx; };
    static void coeffs (const Settings& s, double fs, std::array<SvfCoeffs, 4>& out) noexcept;
    static double responseDb (const Settings& s, double hz, double fs) noexcept;

    void prepare (double fs);
    void reset() noexcept { for (auto& ch : f) for (auto& x : ch) x.reset(); }
    void setParams (const EngineParams& p) noexcept;
    void process (double* l, double* r, int n) noexcept;

private:
    double fs = 48000.0;
    std::array<SvfCoeffs, 4> c;
    std::array<std::array<Svf, 4>, 2> f;
    Smoother sLb, sLa, sHb, sHa, sBw, sLf, sHbf, sHaf;
    bool first = true;
};
} // namespace bassforge
