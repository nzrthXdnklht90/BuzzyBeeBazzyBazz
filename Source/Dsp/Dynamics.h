// BASSFORGE - dynamics processors: Drive low-band compressor, DI FET-style compressor and the
// optical-style programme leveller on the master bus. Copyright (c) 2026 Nazareth. Original code.
//
// All three are log-domain gain computers with smoothed gain-reduction branches (attack/release in
// the dB domain, "decoupled" style) so release behaviour is independent of the knee. The character
// of each comes from its detector, timing law and coloration, documented per class. Constants marked
// TUNABLE are listening-test starting points.
#pragma once

#include "Filters.h"

namespace bassforge
{
/** Static soft-knee gain computer: returns gain reduction in dB (<= 0). */
inline double softKneeGr (double levelDb, double thrDb, double ratio, double kneeDb) noexcept
{
    const double over = levelDb - thrDb;
    const double slope = 1.0 - 1.0 / ratio;
    if (2.0 * over <= -kneeDb) return 0.0;
    if (2.0 * over >= kneeDb) return -slope * over;
    const double t = over + 0.5 * kneeDb;
    return -slope * t * t / (2.0 * kneeDb);
}

/** Drive low band: 4:1 compressor that keeps the fundamental steady under the distortion.
    3 ms attack (TUNABLE) and a two-speed release (fast 120 ms recovering quickly from single notes,
    slow 600 ms when compression is sustained) so held low notes do not pump. */
class LowBandCompressor
{
public:
    void prepare (double fs);
    void reset() noexcept { grDb = 0.0; slow = 0.0; rms = 0.0; }
    void setAmount (double a) noexcept { amount = std::clamp (a, 0.0, 1.0); }
    double process (double x) noexcept;
    double getGrDb() const noexcept { return grDb; }

private:
    double fs = 48000.0, amount = 0.5, grDb = 0.0, slow = 0.0, rms = 0.0;
    double aAtt = 0.0, aRelFast = 0.0, aRelSlow = 0.0, aSlow = 0.0, aRms = 0.0;
};

/** DI compressor inspired by FET limiting amplifiers, simplified for bass:
    - "Compression" drives the signal into a fixed 4:1 soft-knee threshold (like pushing the input
      knob), with automatic partial make-up.
    - Fast fixed attack (0.8 ms, TUNABLE) lets the very first pick/finger transient through; a
      programme-dependent release (60 ms -> 400 ms as gain reduction persists) avoids pumping on
      sustained low notes.
    - Side-chain high-pass at 30 Hz ignores sub rumble only (a higher corner would let the
      fundamental vary, which is what the DI is meant to prevent).
    - Optional subtle even-harmonic coloration that rises with gain reduction. */
class FetCompressor
{
public:
    void prepare (double fs);
    void reset() noexcept { grDb = 0.0; persist = 0.0; scHp.reset(); dc.reset(); }
    void setAmount (double c) noexcept { amount = std::clamp (c, 0.0, 1.0); }
    double process (double x) noexcept;
    double getGrDb() const noexcept { return grDb; }

private:
    double fs = 48000.0, amount = 0.35, grDb = 0.0, persist = 0.0;
    double aAtt = 0.0, aPersist = 0.0, aPersistDecay = 0.0;
    OnePole scHp, dc;
};

/** Optical programme leveller (stereo linked), modelled on the documented behaviour of
    electro-optical levelling amplifiers rather than a plain compressor with "opto" on the label:
    - feedback topology (the detector listens to the leveller's own output),
    - the light source rises in ~10 ms (relatively slow attack),
    - the photo-cell has a fast component (~60 ms) and a slow "memory" component (0.5 s .. 5 s) that
      charges with how long and how hard it has been lit, giving the classic two-stage,
      programme-dependent release,
    - gentle ~3:1 soft-knee ratio. */
class OptoLeveller
{
public:
    void prepare (double fs);
    void reset() noexcept { light = cellFast = cellSlow = 0.0; lastOut = 0.0; grDb = 0.0; }
    void setAmount (double a) noexcept { amount = std::clamp (a, 0.0, 1.0); }
    /** Returns the gain to apply this sample (stereo-linked: pass the frame's peak magnitude of the
        previous output via processFrame). */
    void processFrame (double& l, double& r) noexcept;
    double getGrDb() const noexcept { return grDb; }

private:
    double fs = 48000.0, amount = 0.2, light = 0.0, cellFast = 0.0, cellSlow = 0.0, lastOut = 0.0, grDb = 0.0;
    double aLightUp = 0.0, aLightDown = 0.0, aFastUp = 0.0, aFastDown = 0.0, aSlowUp = 0.0;
    double aSlowDownFast = 0.0, aSlowDownSlow = 0.0;
};
} // namespace bassforge
