// BASSFORGE - the three Drive characters (VINTAGE / DRIVE / MODERN), run at the oversampled rate.
// Copyright (c) 2026 Nazareth. Original grey-box DSP: the structures follow the published signal flow
// of the reference circuit families (cascaded valve stages with bias shift and supply sag; a
// pre/de-emphasis soft-clipping preamp; pre-distortion low/high emphasis into a hard op-amp-style
// clipper). No component values, code or captures from any manufacturer are used. Every constant
// marked TUNABLE is a listening-test starting point, not a measurement.
#pragma once

#include "Filters.h"
#include "EngineParams.h"
#include <array>

namespace bassforge
{
enum class BandRole { Mid = 0, High = 1 };

struct VoiceSetup
{
    double drive = 0.4;       // 0..1 macro
    double offsetDb = 0.0;    // band drive offset (Mid Drive / High Drive)
    double character = 0.5;   // 0..1
    BandRole role = BandRole::Mid;
};

/** Envelope follower (|x|) with separate attack/release time constants. */
class EnvFollower
{
public:
    void set (double attackMs, double releaseMs, double fs) noexcept
    {
        aA = std::exp (-1.0 / (attackMs * 0.001 * fs));
        aR = std::exp (-1.0 / (releaseMs * 0.001 * fs));
    }
    void reset() noexcept { e = 0.0; }
    inline double process (double x) noexcept
    {
        const double a = std::abs (x);
        e = a > e ? a + (e - a) * aA : a + (e - a) * aR;
        e = flushDenormal (e);
        return e;
    }
    double value() const noexcept { return e; }

private:
    double aA = 0.0, aR = 0.0, e = 0.0;
};

/** VINTAGE: two asymmetric valve-like stages, level-dependent bias shift (grid "blocking"), interstage
    coupling/Miller filtering, a character-controlled tone stack, a soft push-pull power stage and
    slow supply sag. */
class VintageVoice
{
public:
    void prepare (double fs);
    void reset() noexcept;
    void setup (const VoiceSetup& s) noexcept;
    void process (double* buf, int n) noexcept;

private:
    double fs = 192000.0, preGain = 1.0, sagAmt = 0.0, postLpHz = 6000.0;
    OnePole couple1, couple2, miller, postLp, dcOut;
    EnvFollower env1, supply;
    SvfCoeffs bassC, midC, trebC; Svf bass, mid, treb;
};

/** DRIVE: presence pre-emphasis into a soft, slightly asymmetric clipping preamp, a second soft stage
    that only engages at high drive, then de-emphasis. Deliberately NO speaker emulation: the separate
    cabinet stage provides it (brief section 6.2, avoiding double cabinet filtering). */
class SansVoice
{
public:
    void prepare (double fs);
    void reset() noexcept;
    void setup (const VoiceSetup& s) noexcept;
    void process (double* buf, int n) noexcept;

private:
    double fs = 192000.0, preGain = 1.0, stage2 = 0.0;
    OnePole hpIn, postLp, dcOut;
    SvfCoeffs preC, deC, bumpC; Svf pre, de, bump;
};

/** MODERN: frequency-dependent drive (low "weight" and high "bite" emphasis BEFORE the clipper),
    a hard-knee, slightly asymmetric op-amp-style clipper, then a steep anti-fizz low-pass. */
class ModernVoice
{
public:
    void prepare (double fs);
    void reset() noexcept;
    void setup (const VoiceSetup& s) noexcept;
    void process (double* buf, int n) noexcept;

private:
    double fs = 192000.0, preGain = 1.0;
    OnePole hpIn, dcOut;
    SvfCoeffs lowC, highC, lpC, scoopC; Svf low, high, lp1, lp2, scoop;
};

/** Holds the three voices for one band, applies level compensation and crossfades model changes. */
class DriveVoiceBank
{
public:
    static constexpr int kDriveSteps = 11, kCharSteps = 3;

    void prepare (double osRate, int maxBlockOs);
    /** Change the oversampled rate without allocating (quality switch on the audio thread). */
    void setRate (double osRate) noexcept;
    void reset() noexcept;
    /** Measures loudness compensation tables offline (call from prepare, never the audio thread). */
    void calibrate (BandRole role);
    void setup (DriveModel m, const VoiceSetup& s) noexcept;
    void process (double* buf, int n) noexcept;

    double compensationDb (DriveModel m, double drive, double character) const noexcept;

private:
    void measure (BandRole role);
    void runModel (DriveModel m, double* buf, int n) noexcept;
    void setupModel (DriveModel m, const VoiceSetup& s) noexcept;

    VintageVoice vintage; SansVoice sans; ModernVoice modern;
    DriveModel current = DriveModel::Vintage, previous = DriveModel::Vintage;
    VoiceSetup lastSetup;
    double fadePos = 1.0, fadeStep = 0.001;
    double osRate = 192000.0;
    std::vector<double> scratch;
    // compensation[model][drive][char] in dB
    std::array<std::array<std::array<double, kCharSteps>, kDriveSteps>, 3> comp {};
    LinearRamp compGainCur, compGainPrev;
    bool calibrated = false;
};
} // namespace bassforge
