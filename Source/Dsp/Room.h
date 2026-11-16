// BASSFORGE - studio room return: early reflections + 8-line feedback delay network.
// Copyright (c) 2026 Nazareth. Original code. Classic FDN structure (Jot / Stautner-Puckette family,
// textbook material): Hadamard feedback matrix, per-line absorption gains computed from the target
// RT60, one-pole damping in the loop, slow delay modulation to avoid metallic ringing.
// 100 % wet return with a low-frequency high-pass so the room never thickens the fundamental.
#pragma once

#include "Filters.h"
#include "EngineParams.h"
#include <array>

namespace bassforge
{
class Room
{
public:
    static constexpr int kLines = 8, kTaps = 8;
    static constexpr double kTailSeconds = 3.0;

    void prepare (double fs);
    void reset() noexcept;
    void setParams (const EngineParams& p) noexcept;          // control rate
    void process (const double* in, double* outL, double* outR, int n) noexcept;

private:
    struct TypeDef { double lenScale, rt60, preMs, erScale, hpHz, erLevel, outTrim; };
    static const TypeDef& def (RoomType t) noexcept;
    inline double readFrac (const std::vector<double>& buf, int pos, double d) const noexcept
    {
        const int di = (int) d;
        const double fr = d - di;
        const int m = (int) buf.size() - 1;
        const double a = buf[(size_t) ((pos - di) & m)], b = buf[(size_t) ((pos - di - 1) & m)];
        return a + (b - a) * fr;
    }

    double fs = 48000.0;
    std::array<std::vector<double>, kLines> line;
    std::array<double, kLines> fb {}, len {}, lenTarget {}, damp {};
    std::array<OnePole, kLines> dampF;
    std::vector<double> pre;   // predelay + ER source
    int pos = 0, prePos = 0;
    SvfCoeffs hpC; Svf hp1, hp2;
    std::array<double, kTaps> erT {}, erG {};
    double preDelay = 0.0, erLevel = 0.5, outTrim = 1.0;
    double lfoPhase = 0.0, lfoInc = 0.0;
    Smoother sSize, sChar, sType;
    RoomType curType = RoomType::Live;
    bool first = true;
};
} // namespace bassforge
