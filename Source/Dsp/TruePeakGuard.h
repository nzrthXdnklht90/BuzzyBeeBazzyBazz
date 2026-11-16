// BASSFORGE (shared from CrestForge) - reconstruction-aware final peak guard (host/base rate, after down-sampling).
//
// For every host sample m the guard estimates the reconstructed (inter-sample) peak on the interval
// [m, m+1] with an 8-phase windowed-sinc interpolator (64 taps/phase) plus parabolic refinement of the
// largest phase. The required gain for m is the min over the intervals touching m, spread by a short
// moving minimum so the gain is nearly constant across the interpolation kernel's main support, then
// fed to the bound-preserving lookahead envelope. A small safety margin (TUNABLE) absorbs the residual
// error of applying a time-varying gain before reconstruction. Containment is verified by an
// INDEPENDENT measurement (exact FFT band-limited reconstruction in the test suite), not by this
// detector.
//
// Meter-safe policy: the required gain uses max(accurate estimate, BS.1770-style 4x estimate).
// Short 4x interpolators (libebur128: 49-tap Hann sinc; typical DAW TP meters) over-read dense
// near-Nyquist material by up to ~0.6 dB relative to exact reconstruction (measured, docs/dsp_report).
// Limiting against both keeps the output at/below the ceiling on exact reconstruction AND on such
// meters, at the cost of slightly more reduction on that material.
#pragma once

#include "LookaheadEnvelope.h"
#include <array>

namespace bassforge
{
class TruePeakGuard
{
public:
    static constexpr int kPhases = 8;
    static constexpr int kHalf = 32;         // taps each side
    static constexpr int kSpread = 8;        // +/- spread of the required gain (samples)
    static constexpr double kMarginDb = 0.03;
    static constexpr double kAttackMs = 0.5, kReleaseMs = 10.0;

    void prepare (double baseRate, int numChannels);
    void reset() noexcept;
    int getLatency() const noexcept { return kHalf + kSpread + envDelay; }

    void setCeilingDb (double c) noexcept { ceilingLin = dbToGain (c - kMarginDb); quietThresh = ceilingLin / std::max (absBound, 1.0); }
    void setLink (double l) noexcept { link = clamp01 (l); }

    /** Process one frame in place; alignedPre receives the input delayed by getLatency(). */
    void processFrame (double* frame, double* alignedPre) noexcept;

    double getLastFrameGain() const noexcept { return lastFrameGain; }
    double takeGainReductionDb() noexcept { const double v = gainToDb (minGain); minGain = 1.0; return v; }

    /** Offline helper (also used by tests): reconstructed peak estimate around hist centre. */
    double estimateIntervalPeak (const double* centre) const noexcept;

private:
    std::vector<double> kernel; // [phase][2*kHalf]
    // BS.1770-style 4x estimator: phases 1..3, taps over offsets -6..+6 (relative to m)
    static constexpr int kMeterHalf = 7;
    double meterKernel[3][2 * kMeterHalf] {};
    // Fast path (BASSFORGE addition): if every sample in the estimator window is below quietThresh,
    // no estimate can reach the ceiling (|estimate| <= absBound * max|x|), so the estimate is skipped.
    double absBound = 3.0, quietThresh = 0.0;
    std::array<int, 2> loudCountdown { 0, 0 };
    int channels = 2, envDelay = 1;
    double ceilingLin = 1.0, link = 0.0, minGain = 1.0, lastFrameGain = 1.0;
    std::array<std::vector<double>, 2> hist;
    int histPos = 0, histLen = 0;
    std::array<double, 2> prevR { 1.0, 1.0 };
    std::array<std::vector<double>, 2> spreadVal;
    std::array<std::vector<int64_t>, 2> spreadIdx;
    std::array<std::size_t, 2> spHead {}, spTail {};
    std::size_t spMask = 0;
    int64_t counter = 0;
    std::array<LookaheadEnvelope, 2> env;
    std::array<DelayLine, 2> audioDelay;
};
} // namespace bassforge
