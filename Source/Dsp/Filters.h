// BASSFORGE - small filter helpers built on the trapezoidal SVF (framework independent).
// Copyright (c) 2026 Nazareth. Original code.
#pragma once

#include "SvfFilters.h"

namespace bassforge
{
/** Topology-preserving one-pole (trapezoidal). Low-pass and high-pass outputs from one state. */
class OnePole
{
public:
    void setCutoff (double hz, double sampleRate) noexcept
    {
        const double g = std::tan (kPi * std::clamp (hz, 1.0, 0.49 * sampleRate) / sampleRate);
        G = g / (1.0 + g);
    }
    void reset (double v = 0.0) noexcept { s = v; }
    inline double lp (double x) noexcept
    {
        const double v = (x - s) * G;
        const double y = v + s;
        s = flushDenormal (y + v);
        return y;
    }
    inline double hp (double x) noexcept { return x - lp (x); }

private:
    double G = 0.0, s = 0.0;
};

/** Linkwitz-Riley 4th-order low/high pair (two cascaded Butterworth SVFs per output). */
class LR4
{
public:
    void reset() noexcept { for (auto& f : lp) f.reset(); for (auto& f : hp) f.reset(); }
    void setFreq (double hz, double sampleRate) noexcept
    {
        const double t = SvfCoeffs::prewarp (hz, sampleRate);
        cl.makeLowpass (t);
        ch.makeHighpass (t);
    }
    inline void process (double x, double& low, double& high) noexcept
    {
        low = lp[1].process (lp[0].process (x, cl), cl);
        high = hp[1].process (hp[0].process (x, ch), ch);
    }

private:
    SvfCoeffs cl, ch;
    Svf lp[2], hp[2];
};

/** Fractional delay line with 3rd-order Lagrange interpolation (used for microphone distance). */
class FracDelay
{
public:
    void prepare (int maxDelaySamples) { line.prepare ((std::size_t) maxDelaySamples + 4); }
    void reset() noexcept { line.reset(); }
    inline double process (double x, double delay) noexcept
    {
        line.push (x);
        delay = std::max (delay, 1.0); // 1 sample minimum keeps the 4-point kernel causal
        const int d = (int) delay;
        const double f = delay - d;
        const double y0 = line.read ((std::size_t) d - 1), y1 = line.read ((std::size_t) d),
                     y2 = line.read ((std::size_t) d + 1), y3 = line.read ((std::size_t) d + 2);
        // Lagrange around points at -1,0,1,2 relative to d, evaluated at f
        const double c0 = -f * (f - 1.0) * (f - 2.0) / 6.0;
        const double c1 = (f + 1.0) * (f - 1.0) * (f - 2.0) / 2.0;
        const double c2 = -(f + 1.0) * f * (f - 2.0) / 2.0;
        const double c3 = (f + 1.0) * f * (f - 1.0) / 6.0;
        return c0 * y0 + c1 * y1 + c2 * y2 + c3 * y3;
    }

private:
    DelayLine line;
};

/** One-pole smoother for control values (control-rate use). */
class Smoother
{
public:
    void setTime (double ms, double updateRate) noexcept { a = std::exp (-1.0 / std::max (1.0e-6, ms * 0.001 * updateRate)); }
    void snap (double v) noexcept { y = v; }
    inline double next (double target) noexcept { y = target + (y - target) * a; return y; }
    double get() const noexcept { return y; }

private:
    double a = 0.0, y = 0.0;
};

/** Linear crossfade state for click-free bypasses and model switches. */
class Fade
{
public:
    void setLength (int samples) noexcept { step = 1.0 / std::max (1, samples); }
    void snap (bool on) noexcept { v = on ? 1.0 : 0.0; target = v; }
    void setOn (bool on) noexcept { target = on ? 1.0 : 0.0; }
    inline double next() noexcept
    {
        if (v < target) v = std::min (target, v + step);
        else if (v > target) v = std::max (target, v - step);
        return v;
    }
    bool isFullyOff() const noexcept { return v == 0.0 && target == 0.0; }
    bool isFullyOn() const noexcept { return v == 1.0 && target == 1.0; }
    bool isMoving() const noexcept { return v != target; }
    double value() const noexcept { return v; }

private:
    double v = 1.0, target = 1.0, step = 0.01;
};

/** Peak meter accumulator (audio thread writes, UI reads a published copy). */
struct PeakAcc
{
    double peak = 0.0;
    inline void push (double x) noexcept { const double a = std::abs (x); if (a > peak) peak = a; }
    double take() noexcept { const double p = peak; peak = 0.0; return p; }
};
} // namespace bassforge
