// BASSFORGE (shared from CrestForge) - bound-preserving lookahead gain envelope.
// Original implementation of the "moving minimum -> bounded release -> moving average" family
// (see Signalsmith Audio, "Designing a straightforward limiter", 2022, for the reasoning).
//
// Contract: push(r[n]) receives the REQUIRED gain for input sample n (0 < r <= 1).
// The returned gain is aligned to input sample n - D (D = fixed audio delay) and is guaranteed
// g <= r[n - D] (up to floating-point rounding of the running sum, ~1e-15 relative).
//
// Variable attack: the ramp length A may change at runtime, by at most one sample per pushed
// sample (rate-limited internally). Proof sketch (docs/dsp_design.md section "Envelope"):
//   h[k] = min r[k-A(k) .. k]     (hold window A+1)
//   e[k] <= h[k]                   (release can only lower gain)
//   g[n] = mean e[n-A+1 .. n]      (box of length A)
//   => g[n] <= r[j] for j in {n-A, n-A+1}; gains are written to a ring indexed by target j and
//      combined with min(). With |dA| <= 1 per sample no target is ever skipped. The audio delay
//      D >= Amax guarantees each target is final before it is read.
#pragma once

#include "DspCommon.h"
#include <array>

namespace bassforge
{
class LookaheadEnvelope
{
public:
    /** maxAttack: largest ramp A (samples). delay: fixed audio delay D (>= maxAttack). */
    void prepare (int maxAttack, int delay);
    void reset() noexcept;

    /** Desired ramp length in samples (>=1). Applied with |dA| <= 1 per sample. */
    void setAttackTarget (int samples) noexcept { attackTarget = std::clamp (samples, 1, maxA); }
    /** Release segment length in samples (0 = no release tail). Latched at segment start. */
    void setReleaseSamples (int samples) noexcept { releaseSamples = std::max (0, samples); }
    /** Shape constant of the finite release segment (larger = faster initial recovery). */
    void setReleaseShape (double k);

    int getDelay() const noexcept { return D; }
    int getCurrentAttack() const noexcept { return A; }

    /** Push required gain for newest sample; returns gain aligned to sample (newest - D). */
    double push (double requiredGain) noexcept;

private:
    double releaseCurve (double u) const noexcept; // s(u): 1 -> 0 over u in [0,1]

    int maxA = 1, D = 1, A = 1, attackTarget = 1, releaseSamples = 0;
    int64_t n = -1, written = -1;

    // monotonic deque for moving minimum
    std::vector<int64_t> dqIdx;
    std::vector<double> dqVal;
    std::size_t dqMask = 0, dqHead = 0, dqTail = 0; // [head, tail)

    // release state
    bool relActive = false;
    double relStart = 1.0;
    int relPos = 0, relLen = 0;
    std::array<double, 1025> relTable {};

    // box smoothing
    std::vector<double> eRing;
    std::size_t eMask = 0;
    double sum = 1.0, sumComp = 0.0;

    // gain ring indexed by target sample
    std::vector<double> gRing;
    std::size_t gMask = 0;
};
} // namespace bassforge
