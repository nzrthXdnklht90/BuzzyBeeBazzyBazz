#include "Dynamics.h"

namespace bassforge
{
namespace
{
inline double coef (double ms, double fs) noexcept { return std::exp (-1.0 / (std::max (ms, 0.01) * 0.001 * fs)); }
}

// ============================================================================ low band
void LowBandCompressor::prepare (double rate)
{
    fs = rate;
    aAtt = coef (3.0, fs);
    aRelFast = coef (120.0, fs);
    aRelSlow = coef (600.0, fs);
    aSlow = coef (300.0, fs);
    aRms = coef (5.0, fs);   // short RMS window: below one cycle of 31 Hz would ripple, so 5 ms + peak blend
    reset();
}

double LowBandCompressor::process (double x) noexcept
{
    if (amount <= 0.0)
    {
        grDb = 0.0;
        return x;
    }
    rms = x * x + (rms - x * x) * aRms;
    // level: blend of short RMS (+3 dB to read like peak on sines) and instantaneous peak
    const double lvl = std::max (std::sqrt (2.0 * rms), 0.5 * std::abs (x));
    const double levelDb = gainToDb (lvl);
    const double thr = -8.0 - 26.0 * amount;                // TUNABLE
    const double target = softKneeGr (levelDb, thr, 4.0, 6.0);
    if (target < grDb)
        grDb = target + (grDb - target) * aAtt;
    else
    {
        const double a = slow > 3.0 ? aRelSlow : aRelFast + (aRelSlow - aRelFast) * (slow / 3.0);
        grDb = target + (grDb - target) * a;
    }
    slow = -grDb + (slow + grDb) * aSlow;                    // how much sustained reduction we are in
    const double makeup = 0.5 * -softKneeGr (-14.0, thr, 4.0, 6.0); // partial auto make-up at nominal level
    return x * dbToGain (grDb + makeup);
}

// ============================================================================ FET
void FetCompressor::prepare (double rate)
{
    fs = rate;
    aAtt = coef (0.8, fs);
    aPersist = coef (400.0, fs);
    aPersistDecay = coef (300.0, fs);
    scHp.setCutoff (30.0, fs);
    dc.setCutoff (10.0, fs);
    reset();
}

double FetCompressor::process (double x) noexcept
{
    if (amount <= 0.0)
    {
        grDb = 0.0;
        return x;
    }
    const double driveDb = 30.0 * amount;                    // input drive into fixed threshold
    const double sc = scHp.hp (x);
    const double levelDb = gainToDb (std::abs (sc)) + driveDb;
    const double target = softKneeGr (levelDb, -10.0, 4.0, 8.0);
    if (target < grDb)
        grDb = target + (grDb - target) * aAtt;
    else
    {
        // programme-dependent release: 60 ms when reduction is brief, up to 400 ms when sustained
        const double p = std::clamp (persist / 8.0, 0.0, 1.0);
        const double relMs = 60.0 + 340.0 * p;
        grDb = target + (grDb - target) * coef (relMs, fs);
    }
    // "how long have we been compressing": charges only while the signal is actually over the knee,
    // so a release that is still recovering does not lengthen itself
    if (target < -0.5) persist = -target + (persist + target) * aPersist;
    else persist *= aPersistDecay;
    // automatic partial make-up so the Compression knob mostly changes density, not loudness
    const double nominalGr = softKneeGr (-15.0 + driveDb, -10.0, 4.0, 8.0);
    const double makeupDb = -0.75 * nominalGr;
    double y = x * dbToGain (grDb + makeupDb);
    // subtle FET-style even harmonics proportional to gain reduction (TUNABLE, about -45 dB H2 at 10 dB GR)
    const double colour = 0.015 * std::clamp (-grDb / 10.0, 0.0, 1.0);
    if (colour > 0.0)
        y = y + colour * y * y;
    return dc.hp (y);
}

// ============================================================================ optical leveller
void OptoLeveller::prepare (double rate)
{
    fs = rate;
    aLightUp = coef (10.0, fs);       // light source rise (TUNABLE)
    aLightDown = coef (5.0, fs);
    aFastUp = coef (8.0, fs);
    aFastDown = coef (60.0, fs);      // first release stage (~50% recovery in about 60 ms)
    aSlowUp = coef (1200.0, fs);      // memory charges over about a second of sustained reduction
    aSlowDownFast = coef (500.0, fs);
    aSlowDownSlow = coef (5000.0, fs);
    reset();
}

void OptoLeveller::processFrame (double& l, double& r) noexcept
{
    if (amount <= 0.0)
    {
        grDb = 0.0;
        lastOut = std::max (std::abs (l), std::abs (r));
        return;
    }
    // feedback detector: previous output, driven by the Amount (peak reduction) knob
    const double drive = lastOut * dbToGain (36.0 * amount + sideGainDb);
    light = drive > light ? drive + (light - drive) * aLightUp : drive + (light - drive) * aLightDown;
    cellFast = light > cellFast ? light + (cellFast - light) * aFastUp : light + (cellFast - light) * aFastDown;
    if (light > cellSlow)
        cellSlow = light + (cellSlow - light) * aSlowUp;
    else
    {
        // the more charge the memory holds, the slower it lets go (programme-dependent second stage)
        const double charge = std::clamp (cellSlow / std::max (cellFast, 1e-6) - 0.5, 0.0, 1.0);
        cellSlow = light + (cellSlow - light) * (aSlowDownFast + (aSlowDownSlow - aSlowDownFast) * charge);
    }
    cellSlow = flushDenormal (cellSlow);
    const double cell = 0.65 * cellFast + 0.35 * cellSlow;
    const double cellDb = gainToDb (cell);
    grDb = softKneeGr (cellDb, -6.0, 3.0, 10.0);
    // feedback designs need extra loop gain: scale reduction so heavy Amount settings still level
    grDb *= 1.6;
    const double makeupDb = 0.4 * 12.0 * amount + extraMakeupDb;   // partial make-up (TUNABLE)
    const double g = dbToGain (grDb + makeupDb);
    l *= g;
    r *= g;
    lastOut = std::max (std::abs (l), std::abs (r)) / dbToGain (makeupDb);
}
} // namespace bassforge
