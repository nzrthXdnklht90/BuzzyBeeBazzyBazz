#include "DriveVoices.h"
#include <mutex>

namespace bassforge
{
namespace
{
/** Fast tanh: Lambert continued-fraction rational (7th order), |error| < 1e-4 for |x| < 4.97, where it
    reaches 1 and is clamped (value and slope are continuous to within that error, so no added aliasing). */
inline double ftanh (double x) noexcept
{
    if (x > 4.97) return 1.0;
    if (x < -4.97) return -1.0;
    const double x2 = x * x;
    const double y = x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2))) / (135135.0 + x2 * (62370.0 + x2 * (3150.0 + 28.0 * x2)));
    return std::clamp (y, -1.0, 1.0);
}
inline double asymTanhK (double x, double bias, double tanhBias) noexcept { return ftanh (x + bias) - tanhBias; }
const double kTanhP01 = std::tanh (0.1);
/** Smooth-but-hard clipper: x / (1 + |x|^p)^(1/p), p = 3. */
inline double hardSoft (double x) noexcept
{
    const double a = std::abs (x);
    return x / std::cbrt (1.0 + a * a * a);
}
inline double lerp (double a, double b, double t) noexcept { return a + (b - a) * t; }
} // namespace

// ============================================================================ DRIVE (SansAmp-inspired)
void SansVoice::prepare (double rate)
{
    fs = rate;
    hpIn.setCutoff (40.0, fs);
    dcOut.setCutoff (8.0, fs);
    reset();
}

void SansVoice::reset() noexcept { hpIn.reset(); postLp.reset(); dcOut.reset(); pre.reset(); de.reset(); bump.reset(); }

void SansVoice::setup (const VoiceSetup& s) noexcept
{
    preGain = dbToGain (40.0 * s.drive + s.offsetDb);
    stage2 = std::clamp ((s.drive - 0.55) / 0.45, 0.0, 1.0);
    const double c = s.character; // presence
    preC.makeHighShelf (SvfCoeffs::prewarp (1200.0, fs), lerp (1.0, 9.0, c));
    deC.makeHighShelf (SvfCoeffs::prewarp (3000.0, fs), lerp (-6.0, -1.0, c));
    bumpC.makeBell (SvfCoeffs::prewarp (260.0, fs), 2.0, 0.8);
    postLp.setCutoff (s.role == BandRole::High ? 6500.0 : 11000.0, fs);
}

void SansVoice::process (double* buf, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        double x = pre.process (hpIn.hp (buf[i]), preC);
        const double u = preGain * x;
        double y = 0.6 * asymTanhK (u, 0.1, kTanhP01) + 0.4 * (u / (1.0 + std::abs (u)));
        if (stage2 > 0.0)
            y = lerp (y, ftanh (1.6 * y) * 0.85, stage2);
        y = bump.process (de.process (y, deC), bumpC);
        buf[i] = dcOut.hp (postLp.lp (y));
    }
}

// ============================================================================ MODERN (B7K-inspired)
void ModernVoice::prepare (double rate)
{
    fs = rate;
    hpIn.setCutoff (30.0, fs);
    dcOut.setCutoff (8.0, fs);
    reset();
}

void ModernVoice::reset() noexcept { hpIn.reset(); dcOut.reset(); low.reset(); high.reset(); lp1.reset(); lp2.reset(); scoop.reset(); }

void ModernVoice::setup (const VoiceSetup& s) noexcept
{
    preGain = dbToGain (10.0 + 40.0 * s.drive + s.offsetDb);
    const double c = s.character; // 0 = weight (low emphasis), 1 = bite (high emphasis)
    lowC.makeLowShelf (SvfCoeffs::prewarp (250.0, fs), lerp (9.0, 0.0, c));
    highC.makeHighShelf (SvfCoeffs::prewarp (2200.0, fs), lerp (2.0, 11.0, c));
    const double lpHz = s.role == BandRole::High ? 7000.0 : 8500.0;
    lpC.makeLowpass (SvfCoeffs::prewarp (lpHz, fs), 1.0 / 0.6);
    scoopC.makeBell (SvfCoeffs::prewarp (550.0, fs), -2.5, 0.9);
}

void ModernVoice::process (double* buf, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        double x = high.process (low.process (hpIn.hp (buf[i]), lowC), highC);
        const double u = preGain * x;
        double y = u >= 0.0 ? hardSoft (u) : 1.08 * hardSoft (u / 1.08); // tiny asymmetry
        y = lp2.process (lp1.process (y, lpC), lpC);
        y = scoop.process (y, scoopC);
        buf[i] = dcOut.hp (y);
    }
}

// ============================================================================ BANK
void DriveVoiceBank::prepare (double rate, int maxBlockOs)
{
    osRate = rate;
    sans.prepare (rate); fuzz.prepare (rate); modern.prepare (rate);
    scratch.assign ((size_t) std::max (1, maxBlockOs), 0.0);
    fadeStep = 1.0 / (0.020 * rate); // 20 ms model crossfade
    compGainCur.setRampLength ((int) (0.010 * rate));
    compGainPrev.setRampLength ((int) (0.010 * rate));
    compGainCur.snapTo (1.0);
    compGainPrev.snapTo (1.0);
    fadePos = 1.0;
}

void DriveVoiceBank::setRate (double rate) noexcept
{
    osRate = rate;
    sans.prepare (rate); fuzz.prepare (rate); modern.prepare (rate);
    fadeStep = 1.0 / (0.020 * rate);
    compGainCur.setRampLength ((int) (0.010 * rate));
    compGainPrev.setRampLength ((int) (0.010 * rate));
    fadePos = 1.0;
}

void DriveVoiceBank::reset() noexcept { sans.reset(); fuzz.reset(); modern.reset(); fadePos = 1.0; }

void DriveVoiceBank::setupFuzz (const VoiceSetup& s) noexcept
{
    // Drive macro -> Sustain (with the Mid Drive offset as extra input gain), Character -> Filter (Tone)
    FuzzControls c;
    c.sustain = std::clamp (0.04 + 0.96 * s.drive, 0.0, 1.0);
    c.filter = std::clamp (s.character, 0.0, 1.0);
    c.scoop = std::clamp (s.fuzzMids, 0.0, 1.0);
    c.crunch = std::clamp (s.fuzzGrit, 0.0, 1.0);
    c.clip = std::clamp (s.fuzzClip, 0.0, 1.0);
    fuzz.setControls (c);
    fuzzInGain = dbToGain (s.offsetDb);
}

void DriveVoiceBank::setupModel (DriveModel m, const VoiceSetup& s) noexcept
{
    switch (m)
    {
        case DriveModel::Drive:   sans.setup (s); break;
        case DriveModel::Fuzz:    setupFuzz (s); break;
        case DriveModel::Modern:  modern.setup (s); break;
    }
}

void DriveVoiceBank::runModel (DriveModel m, double* buf, int n) noexcept
{
    switch (m)
    {
        case DriveModel::Drive:   sans.process (buf, n); break;
        case DriveModel::Fuzz:    if (fuzzInGain != 1.0) for (int i = 0; i < n; ++i) buf[i] *= fuzzInGain;
                                  fuzz.process (buf, n); break;
        case DriveModel::Modern:  modern.process (buf, n); break;
    }
}

double DriveVoiceBank::compensationDb (DriveModel m, double drive, double character) const noexcept
{
    if (! calibrated) return 0.0;
    const auto& t = comp[(size_t) m];
    const double fd = std::clamp (drive, 0.0, 1.0) * (kDriveSteps - 1);
    const double fc = std::clamp (character, 0.0, 1.0) * (kCharSteps - 1);
    const int d0 = std::min ((int) fd, kDriveSteps - 2), c0 = std::min ((int) fc, kCharSteps - 2);
    const double td = fd - d0, tc = fc - c0;
    const double a = lerp (t[(size_t) d0][(size_t) c0], t[(size_t) d0 + 1][(size_t) c0], td);
    const double b = lerp (t[(size_t) d0][(size_t) c0 + 1], t[(size_t) d0 + 1][(size_t) c0 + 1], td);
    return lerp (a, b, tc);
}

void DriveVoiceBank::setup (DriveModel m, const VoiceSetup& s) noexcept
{
    if (m != current)
    {
        previous = current;
        current = m;
        fadePos = 0.0;
        // fresh state for the incoming voice so the crossfade starts from silence-consistent filters
        switch (m)
        {
            case DriveModel::Drive:   sans.reset(); break;
            case DriveModel::Fuzz:    fuzz.reset(); break;
            case DriveModel::Modern:  modern.reset(); break;
        }
        compGainPrev.snapTo (compGainCur.getCurrent());
        compGainCur.snapTo (dbToGain (compensationDb (m, s.drive, s.character)));
    }
    lastSetup = s;
    setupModel (current, s);
    compGainCur.setTarget (dbToGain (compensationDb (current, s.drive, s.character)));
    if (fadePos < 1.0)
        setupModel (previous, s);
}

void DriveVoiceBank::process (double* buf, int n) noexcept
{
    if (fadePos >= 1.0)
    {
        runModel (current, buf, n);
        for (int i = 0; i < n; ++i)
            buf[i] *= compGainCur.next();
        return;
    }
    std::copy (buf, buf + n, scratch.data());
    runModel (current, buf, n);
    runModel (previous, scratch.data(), n);
    for (int i = 0; i < n; ++i)
    {
        fadePos = std::min (1.0, fadePos + fadeStep);
        // equal-power crossfade (the two voices are uncorrelated in their distortion products)
        const double a = std::sin (0.5 * kPi * fadePos), b = std::cos (0.5 * kPi * fadePos);
        buf[i] = a * buf[i] * compGainCur.next() + b * scratch[(size_t) i] * compGainPrev.getCurrent();
    }
}

void DriveVoiceBank::calibrate (BandRole role)
{
    // The tables are rate-independent (they describe loudness, not filtering), so they are measured
    // once per process at a fixed 192 kHz and shared by every instance (deterministic, thread-safe).
    using Table = decltype (comp);
    static std::once_flag once;
    static Table tables[2];
    std::call_once (once, [] {
        for (int r = 0; r < 2; ++r)
        {
            DriveVoiceBank tmp;
            tmp.prepare (192000.0, 256);
            tmp.measure ((BandRole) r);
            tables[r] = tmp.comp;
        }
    });
    comp = tables[(int) role];
    calibrated = true;
}

void DriveVoiceBank::measure (BandRole role)
{
    // Target: output RMS == input RMS, so Drive / Character /
    // Model changes are loudness-neutral at the nominal playing level (brief section 19).
    // Signal: a bass-like note (band-limited saw, A1 = 55 Hz, -12 dBFS peak) split by the default
    // crossover, so each band is calibrated on the kind of content it really receives.
    const int warm = (int) (0.01 * osRate), len = (int) (0.24 * osRate);
    std::vector<double> src ((size_t) (warm + len)), sig ((size_t) (warm + len));
    {
        LR4 a, b; a.setFreq (140.0, osRate); b.setFreq (850.0, osRate);
        double peakV = 0.0;
        const double dt = 55.0 / osRate;
        double ph = 0.0;
        for (int i = 0; i < warm + len; ++i)
        {
            // polyBLEP sawtooth (cheap, alias-suppressed at the oversampled rate)
            double s = 2.0 * ph - 1.0;
            if (ph < dt) { const double t = ph / dt; s -= t + t - t * t - 1.0; }
            else if (ph > 1.0 - dt) { const double t = (ph - 1.0) / dt; s -= t * t + t + t + 1.0; }
            ph += dt; if (ph >= 1.0) ph -= 1.0;
            peakV = std::max (peakV, std::abs (s));
            src[(size_t) i] = s;
        }
        // decaying pluck (like a real note): heavier distortion holds the level up for longer, and the
        // compensation has to account for that, not just the steady-state level
        for (size_t i = 0; i < src.size(); ++i) src[i] *= 0.25 / peakV * std::exp (-4.0 * (double) i / osRate);
        for (auto& v : src)
        {
            double lo, rest, mid, hi;
            a.process (v, lo, rest); b.process (rest, mid, hi);
            v = role == BandRole::Mid ? mid : hi;
        }
    }
    for (int m = 0; m < 3; ++m)
        for (int d = 0; d < kDriveSteps; ++d)
            for (int c = 0; c < kCharSteps; ++c)
            {
                VoiceSetup s;
                s.drive = (double) d / (kDriveSteps - 1);
                s.character = (double) c / (kCharSteps - 1);
                s.role = role;
                const auto model = (DriveModel) m;
                switch (model)
                {
                    case DriveModel::Drive:   sans.reset(); break;
                    case DriveModel::Fuzz:    fuzz.reset(); break;
                    case DriveModel::Modern:  modern.reset(); break;
                }
                setupModel (model, s);
                double inE = 0.0;
                for (int i = 0; i < warm + len; ++i)
                {
                    sig[(size_t) i] = src[(size_t) i];
                    if (i >= warm) inE += sig[(size_t) i] * sig[(size_t) i];
                }
                runModel (model, sig.data(), warm + len);
                double outE = 0.0;
                for (int i = warm; i < warm + len; ++i) outE += sig[(size_t) i] * sig[(size_t) i];
                comp[(size_t) m][(size_t) d][(size_t) c] = 10.0 * std::log10 ((inE + 1e-20) / (outE + 1e-20));
            }
    calibrated = true;
    reset();
}
} // namespace bassforge
