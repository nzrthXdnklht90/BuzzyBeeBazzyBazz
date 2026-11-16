// BASSFORGE (shared from CrestForge) - shared DSP utilities (framework independent).
// Copyright (c) 2026 Nazareth. Original code.
#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace bassforge
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1.0e-30;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) noexcept { return 20.0 * std::log10 (std::max (g, kEps)); }
inline double clamp01 (double x) noexcept { return x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x); }

/** Flush tiny values (denormal protection for recursive state). */
inline double flushDenormal (double x) noexcept { return std::abs (x) < 1.0e-25 ? 0.0 : x; }

inline bool isFiniteSample (double x) noexcept { return std::isfinite (x); }

/** Fixed-capacity ring buffer delay line. Capacity allocated in prepare(), never in process. */
class DelayLine
{
public:
    void prepare (std::size_t maxDelay)
    {
        std::size_t cap = 1;
        while (cap < maxDelay + 1)
            cap <<= 1;
        buffer.assign (cap, 0.0);
        mask = cap - 1;
        writePos = 0;
    }

    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0); writePos = 0; }
    void resetTo (double v) noexcept { std::fill (buffer.begin(), buffer.end(), v); writePos = 0; }

    /** Push x and return the sample delayed by `delay` (delay <= maxDelay). delay==0 returns x. */
    inline double process (double x, std::size_t delay) noexcept
    {
        buffer[writePos] = x;
        const double y = buffer[(writePos - delay) & mask];
        writePos = (writePos + 1) & mask;
        return y;
    }

    inline void push (double x) noexcept
    {
        buffer[writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Read sample written `delay` pushes ago (0 = most recent). */
    inline double read (std::size_t delay) const noexcept { return buffer[(writePos - 1 - delay) & mask]; }

    std::size_t capacity() const noexcept { return buffer.size(); }

private:
    std::vector<double> buffer { 0.0 };
    std::size_t mask = 0;
    std::size_t writePos = 0;
};

/** Linear ramp toward a target over a fixed number of samples (used in dB, Hz-log or plain domains). */
class LinearRamp
{
public:
    void setRampLength (int samples) noexcept { rampLength = std::max (1, samples); }
    void snapTo (double v) noexcept { current = target = v; remaining = 0; step = 0.0; }
    void setTarget (double t) noexcept
    {
        if (t == target)
            return;
        target = t;
        remaining = rampLength;
        step = (target - current) / (double) remaining;
    }
    inline double next() noexcept
    {
        if (remaining > 0)
        {
            current += step;
            if (--remaining == 0)
                current = target;
        }
        return current;
    }
    /** Advance n samples at once (control-rate usage). */
    inline double advance (int n) noexcept
    {
        if (remaining > 0)
        {
            const int k = std::min (n, remaining);
            current += step * k;
            remaining -= k;
            if (remaining == 0)
                current = target;
        }
        return current;
    }
    double getCurrent() const noexcept { return current; }
    double getTarget() const noexcept { return target; }
    bool isSmoothing() const noexcept { return remaining > 0; }

private:
    double current = 0.0, target = 0.0, step = 0.0;
    int remaining = 0, rampLength = 1;
};

/** Tiny deterministic PRNG (xoshiro128+ style, 32-bit outputs). Independent per channel. */
class Prng
{
public:
    void seed (uint64_t s) noexcept
    {
        // splitmix64 seeding
        for (auto& w : state)
        {
            s += 0x9E3779B97F4A7C15ull;
            uint64_t z = s;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            w = (uint32_t) ((z ^ (z >> 31)) & 0xffffffffu);
            if (w == 0) w = 0x12345678u;
        }
    }
    inline uint32_t nextU32() noexcept
    {
        const uint32_t result = state[0] + state[3];
        const uint32_t t = state[1] << 9;
        state[2] ^= state[0]; state[3] ^= state[1];
        state[1] ^= state[2]; state[0] ^= state[3];
        state[2] ^= t;
        state[3] = (state[3] << 11) | (state[3] >> 21);
        return result;
    }
    /** Uniform in [0, 1). */
    inline double uniform() noexcept { return (nextU32() >> 8) * (1.0 / 16777216.0); }

private:
    uint32_t state[4] { 1, 2, 3, 4 };
};
} // namespace bassforge
