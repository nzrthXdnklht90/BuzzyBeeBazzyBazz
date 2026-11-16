#include "TruePeakGuard.h"

namespace bassforge
{
namespace
{
double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    const double q = x * x / 4.0;
    for (int k = 1; k < 64; ++k) { term *= q / ((double) k * k); sum += term; if (term < sum * 1e-17) break; }
    return sum;
}
} // namespace

void TruePeakGuard::prepare (double baseRate, int numChannels)
{
    channels = std::clamp (numChannels, 1, 2);
    // kernel: phase p estimates x(m + p/P) from samples m-kHalf+1 .. m+kHalf
    const int taps = 2 * kHalf;
    kernel.assign ((size_t) kPhases * (size_t) taps, 0.0);
    const double beta = 7.0;
    const double i0b = besselI0 (beta);
    for (int p = 0; p < kPhases; ++p)
    {
        const double frac = (double) p / kPhases;
        double sum = 0.0;
        for (int k = 0; k < taps; ++k)
        {
            const double t = (double) (k - (kHalf - 1)) - frac; // sample offset relative to m, minus frac
            const double sinc = std::abs (t) < 1e-12 ? 1.0 : std::sin (kPi * t) / (kPi * t);
            const double r = t / (double) kHalf;
            const double w = std::abs (r) >= 1.0 ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
            kernel[(size_t) (p * taps + k)] = sinc * w;
            sum += sinc * w;
        }
        for (int k = 0; k < taps; ++k)
            kernel[(size_t) (p * taps + k)] /= sum; // unity DC gain per phase
    }
    // meter-style kernel: c(u) = sinc(u/4) * hann over 49 taps, u = 4*(tau - k) (libebur128 design, MIT)
    for (int f = 1; f <= 3; ++f)
        for (int i = 0; i < 2 * kMeterHalf; ++i)
        {
            const int k = i - (kMeterHalf - 1);              // sample offset relative to m
            const double uu = 4.0 * (0.0 - k) + f;           // tau = m + f/4
            double c = 0.0;
            if (std::abs (uu) <= 24.0)
            {
                const double j = uu + 24.0;                  // tap index 0..48
                c = std::abs (uu) < 1e-12 ? 1.0 : std::sin (uu * kPi / 4.0) / (uu * kPi / 4.0);
                c *= 0.5 * (1.0 - std::cos (2.0 * kPi * j / 48.0));
            }
            meterKernel[f - 1][i] = c;
        }
    // bound used by the quiet fast path: grid phases (incl. parabolic refinement, <= 1.125 x grid max)
    // and the meter-style kernel
    double phaseMax = 1.0;
    for (int p = 1; p < kPhases; ++p)
    {
        double s = 0.0;
        for (int k = 0; k < taps; ++k) s += std::abs (kernel[(size_t) (p * taps + k)]);
        phaseMax = std::max (phaseMax, s);
    }
    double meterMax = 0.0;
    for (int f = 0; f < 3; ++f)
    {
        double s = 0.0;
        for (int i = 0; i < 2 * kMeterHalf; ++i) s += std::abs (meterKernel[f][i]);
        meterMax = std::max (meterMax, s);
    }
    absBound = std::max (1.125 * phaseMax, meterMax) * 1.001;
    quietThresh = ceilingLin / absBound;
    histLen = taps;
    for (auto& h : hist) h.assign ((size_t) histLen * 2, 0.0);
    std::size_t cap = 1; while (cap < (size_t) (2 * kSpread + 4)) cap <<= 1;
    spMask = cap - 1;
    for (int c = 0; c < 2; ++c)
    {
        spreadVal[(size_t) c].assign (cap, 1.0);
        spreadIdx[(size_t) c].assign (cap, 0);
    }
    const int attack = std::max (1, (int) std::lround (kAttackMs * 1e-3 * baseRate));
    envDelay = attack;
    for (auto& e : env)
    {
        e.prepare (attack, envDelay);
        e.setAttackTarget (attack);
        e.setReleaseSamples ((int) std::lround (kReleaseMs * 1e-3 * baseRate));
    }
    for (auto& d : audioDelay) d.prepare ((size_t) getLatency() + 1);
    reset();
}

void TruePeakGuard::reset() noexcept
{
    for (auto& h : hist) std::fill (h.begin(), h.end(), 0.0);
    histPos = 0;
    prevR = { 1.0, 1.0 };
    loudCountdown = { 0, 0 };
    spHead = { 0, 0 }; spTail = { 0, 0 };
    counter = 0;
    for (auto& e : env) e.reset();
    for (auto& d : audioDelay) d.reset();
    minGain = 1.0;
}

double TruePeakGuard::estimateIntervalPeak (const double* oldest) const noexcept
{
    // oldest points at sample m-kHalf+1; samples m and m+1 are oldest[kHalf-1], oldest[kHalf]
    const int taps = 2 * kHalf;
    double v[kPhases + 1];
    v[0] = oldest[kHalf - 1];
    for (int p = 1; p < kPhases; ++p)
    {
        const double* k = &kernel[(size_t) (p * taps)];
        double acc = 0.0;
        for (int i = 0; i < taps; ++i)
            acc += k[i] * oldest[i];
        v[p] = acc;
    }
    v[kPhases] = oldest[kHalf];
    // meter-style 4x estimate on the same interval
    double meterPk = 0.0;
    const double* mBase = oldest + (kHalf - 1) - (kMeterHalf - 1);
    for (int f = 0; f < 3; ++f)
    {
        double acc = 0.0;
        for (int i = 0; i < 2 * kMeterHalf; ++i) acc += meterKernel[f][i] * mBase[i];
        meterPk = std::max (meterPk, std::abs (acc));
    }
    int best = 0;
    double bestAbs = std::abs (v[0]);
    for (int p = 1; p <= kPhases; ++p)
        if (std::abs (v[p]) > bestAbs) { bestAbs = std::abs (v[p]); best = p; }
    // parabolic refinement on |v| around the best grid point (conservative: never below grid max)
    if (best > 0 && best < kPhases)
    {
        const double a = std::abs (v[best - 1]), b = bestAbs, c = std::abs (v[best + 1]);
        const double den = a - 2.0 * b + c;
        if (den < 0.0)
        {
            const double d = 0.5 * (a - c) / den;
            const double peak = b - 0.25 * (a - c) * d;
            if (peak > bestAbs) bestAbs = peak;
        }
    }
    return std::max (bestAbs, meterPk);
}

void TruePeakGuard::processFrame (double* frame, double* alignedPre) noexcept
{
    const int64_t n = counter++;
    double r[2] { 1.0, 1.0 };
    for (int c = 0; c < channels; ++c)
    {
        auto& h = hist[(size_t) c];
        h[(size_t) histPos] = frame[c];
        h[(size_t) (histPos + histLen)] = frame[c];
        const double* oldest = &h[(size_t) (histPos + 1)]; // window of last histLen samples
        if (std::abs (frame[c]) > quietThresh) loudCountdown[(size_t) c] = histLen + 2;
        double pk = 0.0;
        if (loudCountdown[(size_t) c] > 0)
        {
            --loudCountdown[(size_t) c];
            pk = estimateIntervalPeak (oldest);   // interval [m, m+1], m = n - kHalf
        }
        const double rInt = pk > ceilingLin ? ceilingLin / pk : 1.0;
        // sample m is touched by intervals [m-1,m] and [m,m+1]
        double rm = std::min (rInt, prevR[(size_t) c]);
        prevR[(size_t) c] = rInt;
        // spread: moving minimum over 2*kSpread+1 samples (centre = m - kSpread)
        auto& sv = spreadVal[(size_t) c]; auto& si = spreadIdx[(size_t) c];
        auto& hd = spHead[(size_t) c]; auto& tl = spTail[(size_t) c];
        while (tl != hd && sv[(tl - 1) & spMask] >= rm) --tl;
        sv[tl & spMask] = rm; si[tl & spMask] = n; ++tl;
        while (si[hd & spMask] <= n - (2 * kSpread + 1)) ++hd;
        // also include rInt (interval [m, m+1] touches m+1, which lies inside the spread window)
        r[c] = std::min (sv[hd & spMask], rInt);
    }
    if (++histPos == histLen) histPos = 0;

    if (channels == 2 && link > 0.0)
    {
        const double shared = std::min (r[0], r[1]);
        r[0] += link * (shared - r[0]);
        r[1] += link * (shared - r[1]);
    }
    double frameMin = 1.0;
    for (int c = 0; c < channels; ++c)
    {
        const double g = env[(size_t) c].push (r[c]);
        if (g < frameMin) frameMin = g;
        const double xd = audioDelay[(size_t) c].process (frame[c], (size_t) getLatency());
        alignedPre[c] = xd;
        frame[c] = xd * g;
        if (g < minGain) minGain = g;
    }
    lastFrameGain = frameMin;
}
} // namespace bassforge
