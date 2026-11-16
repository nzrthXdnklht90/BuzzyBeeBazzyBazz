// BASSFORGE - procedural cabinet + dual virtual microphone model (no recorded IRs).
// Copyright (c) 2026 Nazareth. Original code.
//
// Model per microphone:  speaker/enclosure (shared)  ->  radial position  ->  proximity (type, distance)
//                         ->  capsule voicing (type)  ->  distance level/HF  ->  acoustic delay
//
// STATUS OF THE FOUR PROFILES: PROVISIONAL. They are hand-voiced from the general, publicly described
// character of each cabinet family (ported vs sealed, driver size, cone material, tweeter or not).
// They are NOT fitted to measurements yet. Each value below is TUNABLE and will be replaced by
// fitted values once reference measurements exist (docs/M0_architecture.md, section 7).
#pragma once

#include "Filters.h"
#include "EngineParams.h"
#include <array>

namespace bassforge
{
struct CabProfile
{
    const char* name;
    bool ported;
    double resHz, resQ;           // enclosure low-frequency corner / resonance
    double bumpHz, bumpDb, bumpQ; // box / port bump
    double warmHz, warmDb, warmQ; // low-mid body
    double dipHz, dipDb, dipQ;    // cone/cabinet dip
    double brkHz, brkDb, brkQ;    // first cone break-up region
    double brk2Hz, brk2Db, brk2Q; // second break-up / tweeter presence
    double lpHz, lpQ;             // top roll-off (applied twice = 4th order)
    bool singleLp;                // true: 2nd-order roll-off only (tweeter-equipped cabinets)
};

const CabProfile& cabProfile (CabModel m) noexcept;

/** Mic positions are continuous; the UI grid snaps to 4 radial x 8 distance points. */
struct MicState
{
    MicType type = MicType::Dynamic;
    double position = 0.25, distanceIn = 1.0;
};

/** One complete cabinet render (shared speaker filters + two microphone chains). Two of these are
    kept so discrete changes (cabinet, mic type, align mode) crossfade without clicks. */
class CabChain
{
public:
    void prepare (double fs);
    void reset() noexcept;
    void configure (CabModel cab, MicType t1, MicType t2, MicAlign align) noexcept;
    /** Continuous updates (control rate). */
    void update (const MicState& m1, const MicState& m2) noexcept;
    inline void process (double x, double& mic1, double& mic2) noexcept
    {
        double s = x;
        for (int i = 0; i < kCommon; ++i)
            s = common[i].process (s, commonC[i]);
        mic1 = runMic (0, s);
        mic2 = runMic (1, s);
    }
    CabModel getCab() const noexcept { return cab; }
    MicType getType (int i) const noexcept { return types[(size_t) i]; }
    MicAlign getAlign() const noexcept { return align; }

    /** Static level trim (dB) measured at prepare() for this cabinet + capsule type. */
    double trimDb (int mic) const noexcept;

private:
    static constexpr int kCommon = 8, kMic = 7;
    inline double runMic (int m, double s) noexcept
    {
        auto& f = micF[(size_t) m];
        auto& c = micC[(size_t) m];
        for (int i = 0; i < kMic; ++i)
            s = f[(size_t) i].process (s, c[(size_t) i]);
        return delay[(size_t) m].process (s * micGain[(size_t) m], micDelay[(size_t) m]);
    }
    void setupCommon() noexcept;
    void setupMic (int m, const MicState& st) noexcept;

    double fs = 48000.0;
    CabModel cab = CabModel::Classic810;
    std::array<MicType, 2> types { MicType::Dynamic, MicType::Condenser };
    MicAlign align = MicAlign::Auto;
    SvfCoeffs commonC[kCommon]; Svf common[kCommon];
    std::array<std::array<SvfCoeffs, kMic>, 2> micC; std::array<std::array<Svf, kMic>, 2> micF;
    std::array<FracDelay, 2> delay;
    std::array<double, 2> micDelay { 1.0, 1.0 }, micGain { 1.0, 1.0 }, distIn { 1.0, 1.0 };

public:
    using TrimTable = std::array<std::array<double, 3>, 4>;   // [cab][capsule type] in dB
    const TrimTable* trimTable = nullptr;
};

/** Cabinet block: crossfades between two CabChains, mixes the two mics (level, balance, polarity,
    enable, stereo spread) with ramped gains. Mono in, stereo out. */
class Cabinet
{
public:
    void prepare (double fs);
    void reset() noexcept;
    void setParams (const EngineParams& p) noexcept;   // control rate
    void process (const double* in, double* outL, double* outR, int n) noexcept;
    static double maxMicDelaySamples (double fs) noexcept { return 6.5 / 13504.0 * fs + 4.0; }

private:
    void calibrate();   // per-cabinet/per-capsule level trims, measured in prepare()
    double fs = 48000.0;
    CabChain::TrimTable trims {};
    CabChain chains[2];
    int active = 0;
    double fade = 1.0, fadeStep = 0.001;
    LinearRamp g1L, g1R, g2L, g2R;
    Smoother pos[2], dist[2];
    bool firstUpdate = true;
};
} // namespace bassforge
