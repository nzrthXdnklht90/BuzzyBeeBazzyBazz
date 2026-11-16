// BASSFORGE (shared from CrestForge) - topology-preserving (trapezoidal) state-variable filter.
// Based on the publicly documented linear trapezoidal SVF derivation (A. Simper, Cytomic,
// "Solving the continuous SVF equations using trapezoidal integration", 2013). Independent code.
//
// Why this structure: the integrator states are scaled like physical capacitor charges, so
// per-sample modulation of cutoff/gain cannot produce the coefficient-interpolation blow-ups
// that direct-form biquads suffer. Coefficients are always generated from (g, k, m*) closed forms,
// never interpolated.
#pragma once

#include "DspCommon.h"

namespace bassforge
{
struct SvfCoeffs
{
    double a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;

    static constexpr double kButterworth = 1.4142135623730951; // k = 1/Q, Q = 1/sqrt(2)

    /** Safe prewarp: cutoff clamped below Nyquist. */
    static double prewarp (double hz, double sampleRate) noexcept
    {
        const double f = std::min (hz, 0.49 * sampleRate);
        return std::tan (kPi * std::max (f, 1.0) / sampleRate);
    }

    double g = 0.0, k = 1.0;   // kept for magnitude evaluation (UI curves, tests)

    /** Exact magnitude of this digital filter at frequency hz (bilinear-domain evaluation). */
    double magnitude (double hz, double sampleRate) const noexcept
    {
        if (g <= 0.0) return std::abs (m0);
        const double w = std::tan (kPi * std::min (hz, 0.4999 * sampleRate) / sampleRate) / g; // s = j w
        // H(s) = m0 + m1 * (k? ) : with LP = 1/(s^2 + k s + 1), BP = s/(...) (state v1 ~ BP/ k-less form)
        // SVF outputs: v1 = g*s'/(..) ; in normalised form v1 -> s/(s^2+ks+1), v2 -> 1/(s^2+ks+1)
        const double reD = 1.0 - w * w, imD = k * w;
        const double den = reD * reD + imD * imD;
        // N = m0*(1 - w^2 + j k w) + m1*(j w) + m2
        const double reN = m0 * reD + m2, imN = m0 * imD + m1 * w;
        return std::sqrt ((reN * reN + imN * imN) / std::max (den, 1e-300));
    }

    void setCore (double gIn, double kIn) noexcept
    {
        g = gIn; k = kIn;
        a1 = 1.0 / (1.0 + gIn * (gIn + kIn));
        a2 = gIn * a1;
        a3 = gIn * a2;
    }

    void makeLowpass (double tanW, double k = kButterworth) noexcept
    {
        setCore (tanW, k);
        m0 = 0.0; m1 = 0.0; m2 = 1.0;
    }
    void makeHighpass (double tanW, double k = kButterworth) noexcept
    {
        setCore (tanW, k);
        m0 = 1.0; m1 = -k; m2 = -1.0;
    }
    /** Low shelf: gain gainDb below the corner, gainDb/2 at the corner. */
    void makeLowShelf (double tanW, double gainDb, double k = kButterworth) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        setCore (tanW / std::sqrt (A), k);
        m0 = 1.0; m1 = k * (A - 1.0); m2 = A * A - 1.0;
    }
    /** Peaking bell (constant-Q style): gainDb at the centre. */
    void makeBell (double tanW, double gainDb, double q) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double k = 1.0 / (std::max (q, 0.05) * A);
        setCore (tanW, k);
        m0 = 1.0; m1 = k * (A * A - 1.0); m2 = 0.0;
    }
    /** Second-order allpass (k = 1/Q). With k = sqrt(2) this equals LP4+HP4 of a Linkwitz-Riley pair. */
    void makeAllpass (double tanW, double k = kButterworth) noexcept
    {
        setCore (tanW, k);
        m0 = 1.0; m1 = -2.0 * k; m2 = 0.0;
    }
    void makeBandpass (double tanW, double k) noexcept
    {
        setCore (tanW, k);
        m0 = 0.0; m1 = k; m2 = 0.0; // unity peak gain
    }
    void makeIdentity() noexcept { setCore (0.0, 1.0); m0 = 1.0; m1 = 0.0; m2 = 0.0; }
    /** High shelf: gain gainDb above the corner, gainDb/2 at the corner. */
    void makeHighShelf (double tanW, double gainDb, double k = kButterworth) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        setCore (tanW * std::sqrt (A), k);
        m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - A * A;
    }
};

class Svf
{
public:
    void reset() noexcept { ic1 = ic2 = 0.0; }

    inline double process (double v0, const SvfCoeffs& c) noexcept
    {
        const double v3 = v0 - ic2;
        const double v1 = c.a1 * ic1 + c.a2 * v3;
        const double v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = flushDenormal (2.0 * v1 - ic1);
        ic2 = flushDenormal (2.0 * v2 - ic2);
        return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
    }

    bool isFinite() const noexcept { return std::isfinite (ic1) && std::isfinite (ic2); }

private:
    double ic1 = 0.0, ic2 = 0.0;
};
} // namespace bassforge
