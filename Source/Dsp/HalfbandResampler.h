// BASSFORGE (shared from CrestForge) - linear-phase halfband FIR cascade for 2^S oversampling.
// Original implementation. Filters are designed in prepare() (Kaiser-windowed sinc),
// never in the audio callback.
#pragma once

#include "DspCommon.h"
#include <vector>

namespace bassforge
{
/** Halfband design parameters for one stage. */
struct HalfbandSpec
{
    double passbandEdge = 0.2268; // normalized to the stage's HIGH rate (must be < 0.25)
    double attenuationDb = 100.0;
};

/** Designs the odd-offset taps of a halfband (center tap is exactly 0.5). */
std::vector<double> designHalfbandOddTaps (const HalfbandSpec& spec, int& outM);

/** One 2x stage. Up: 1 in -> 2 out. Down: 2 in -> 1 out. Latency M samples at the high rate. */
class HalfbandStage
{
public:
    void prepare (const HalfbandSpec& spec);
    void reset() noexcept;
    int getM() const noexcept { return M; }

    // Upsampling: produces out[0], out[1] (high-rate order) from one input sample.
    inline void up (double x, double& out0, double& out1) noexcept
    {
        // history of inputs, newest at index pos
        upHist[(size_t) upPos] = x;
        upHist[(size_t) (upPos + histLen)] = x; // mirrored copy for contiguous backwards reads
        const double* newest = &upHist[(size_t) (upPos + histLen)];
        // even output (high index 2n): 2 * sum_i h[2i-M] * x[n-i]
        double acc = 0.0;
        for (int i = 0; i < numTaps; ++i)
            acc += taps[(size_t) i] * newest[-i];
        out0 = 2.0 * acc;
        // odd output (high index 2n+1): centre tap only -> pure delayed input
        out1 = newest[-(M - 1) / 2];
        if (++upPos == histLen)
            upPos = 0;
    }

    // Downsampling: consumes z0 (earlier) then z1 (later); returns one output.
    inline double down (double z0, double z1) noexcept
    {
        // z0 = z[2n] (even stream), z1 = z[2n+1] (odd stream). Output is taken at high index
        // m = 2n so the stage latency is exactly M high-rate samples (round trip 2M = M base samples).
        evenHist[(size_t) dnPos] = z0; evenHist[(size_t) (dnPos + histLen)] = z0;
        oddHist[(size_t) dnPos]  = z1; oddHist[(size_t) (dnPos + histLen)]  = z1;
        const double* e = &evenHist[(size_t) (dnPos + histLen)];
        const double* o = &oddHist[(size_t) (dnPos + histLen)];
        double acc = 0.0;
        for (int i = 0; i < numTaps; ++i)       // j = 2i (even offsets) -> even stream
            acc += taps[(size_t) i] * e[-i];
        acc += 0.5 * o[-(M + 1) / 2];           // centre tap j = M -> odd stream
        if (++dnPos == histLen)
            dnPos = 0;
        return acc;
    }

private:
    std::vector<double> taps; // taps[i] = h[2i - M], i=0..M  (numTaps = M+1)
    int M = 0, numTaps = 0, histLen = 0;
    std::vector<double> upHist, evenHist, oddHist;
    int upPos = 0, dnPos = 0;
};

/** Final 2:1 decimator with a full (non-halfband) linear-phase low-pass whose stopband begins at
    the OUTPUT Nyquist. A halfband is only -6 dB at Nyquist, which lets near-Nyquist energy reach the
    host rate where no short reconstruction kernel can estimate its true peak (found by DSP-12). */
struct DecimatorSpec
{
    double passEdge = 0.4535; // fraction of OUTPUT rate
    double stopEdge = 0.5;    // fraction of OUTPUT rate
    double attenuationDb = 100.0;
};

class FirDecimator2
{
public:
    void prepare (const DecimatorSpec& spec);
    void reset() noexcept { std::fill (hist.begin(), hist.end(), 0.0); pos = 0; }
    int getLatencyHigh() const noexcept { return (numTaps - 1) / 2; }
    inline double down (double z0, double z1) noexcept
    {
        push (z0);
        const double* x = &hist[(size_t) (pos + numTaps - 1)]; // newest = z0
        const int half = numTaps / 2;
        double acc = taps[(size_t) half] * x[-half];
        for (int j = 0; j < half; ++j)
            acc += taps[(size_t) j] * (x[-j] + x[-(numTaps - 1 - j)]); // symmetric taps
        push (z1);
        return acc;
    }

private:
    inline void push (double v) noexcept
    {
        hist[(size_t) pos] = v;
        hist[(size_t) (pos + numTaps)] = v;
        if (++pos == numTaps) pos = 0;
    }
    std::vector<double> taps, hist;
    int numTaps = 1, pos = 0;
};

/** Cascade of S stages for factor 2^S, operating on blocks. One instance per channel. */
class OversamplerChannel
{
public:
    void prepare (int numStages, const std::vector<HalfbandSpec>& specs, const DecimatorSpec& finalSpec, int maxBaseBlock);
    void reset() noexcept;
    int getFactor() const noexcept { return factor; }

    /** Round-trip latency (up+down) expressed in top-rate samples. */
    int getRoundTripLatencyTop() const noexcept { return roundTripTop; }

    /** Upsample n base samples into out (n*factor samples). */
    void upsample (const double* in, double* out, int n) noexcept;
    /** Downsample n*factor samples into out (n samples). */
    void downsample (const double* in, double* out, int n) noexcept;

private:
    std::vector<HalfbandStage> upStages, downStages;
    FirDecimator2 finalDown;
    std::vector<double> scratchA, scratchB;
    int factor = 1, stages = 0, roundTripTop = 0;
};
} // namespace bassforge
