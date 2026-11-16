#include "Room.h"

namespace bassforge
{
namespace
{
// mutually prime-ish base lengths (ms) for the Tight room; other rooms scale them
constexpr double kBaseMs[Room::kLines] = { 7.13, 8.39, 9.71, 11.27, 12.91, 14.63, 16.19, 18.07 };
constexpr double kErMs[Room::kTaps] = { 2.9, 4.7, 6.1, 8.3, 10.9, 13.1, 16.7, 19.9 };
constexpr double kErGain[Room::kTaps] = { 0.82, -0.66, 0.58, -0.47, 0.41, -0.33, 0.27, -0.21 };
// output sign patterns (decorrelated left / right)
constexpr double kOutL[Room::kLines] = { 1, -1, 1, 1, -1, 1, -1, -1 };
constexpr double kOutR[Room::kLines] = { 1, 1, -1, 1, 1, -1, -1, 1 };
constexpr double kMaxScale = 3.0 * 1.35 * 1.05;
} // namespace

const Room::TypeDef& Room::def (RoomType t) noexcept
{
    // lenScale, rt60 (s), predelay (ms), ER time scale, return HP (Hz), ER level, output trim  (all TUNABLE)
    static const TypeDef d[3] = {
        { 1.0, 0.28, 1.5, 0.8, 200.0, 0.70, 0.95 },   // Tight Studio
        { 1.8, 0.65, 5.0, 1.6, 170.0, 0.55, 0.75 },   // Live Room
        { 3.0, 1.35, 12.0, 2.8, 150.0, 0.45, 0.62 },  // Large Studio
    };
    return d[(int) t];
}

void Room::prepare (double rate)
{
    fs = rate;
    for (int i = 0; i < kLines; ++i)
    {
        int cap = 1;
        while (cap < (int) (kBaseMs[i] * kMaxScale * 0.001 * fs) + 64) cap <<= 1;
        line[(size_t) i].assign ((size_t) cap, 0.0);
    }
    int pcap = 1;
    while (pcap < (int) ((20.0 + 19.9 * 2.8 * 1.35) * 0.001 * fs) + 64) pcap <<= 1;
    pre.assign ((size_t) pcap, 0.0);
    lfoInc = 2.0 * kPi * 0.31 / fs;
    sSize.setTime (120.0, fs / 32.0);
    sChar.setTime (60.0, fs / 32.0);
    first = true;
    reset();
}

void Room::reset() noexcept
{
    for (auto& l : line) std::fill (l.begin(), l.end(), 0.0);
    std::fill (pre.begin(), pre.end(), 0.0);
    for (auto& d : dampF) d.reset();
    hp1.reset(); hp2.reset();
    pos = prePos = 0;
}

void Room::setParams (const EngineParams& p) noexcept
{
    const auto& d = def (p.room);
    if (first) { sSize.snap (p.roomSize); sChar.snap (p.roomCharacter); }
    const double size = sSize.next (p.roomSize), ch = sChar.next (p.roomCharacter);
    const double lenScale = d.lenScale * (0.7 + 0.65 * size);
    const double rt = d.rt60 * (0.6 + 1.0 * size);
    for (int i = 0; i < kLines; ++i)
    {
        lenTarget[(size_t) i] = kBaseMs[i] * lenScale * 0.001 * fs;
        if (first || p.room != curType) len[(size_t) i] = lenTarget[(size_t) i]; // room type change: jump (masked by the fade below)
        // absorption per pass for the target RT60: g = 10^(-3 L / (RT60 fs))
        fb[(size_t) i] = std::pow (10.0, -3.0 * lenTarget[(size_t) i] / (rt * fs));
        dampF[(size_t) i].setCutoff (2500.0 * std::pow (9000.0 / 2500.0, ch), fs);
    }
    for (int t = 0; t < kTaps; ++t)
    {
        erT[(size_t) t] = kErMs[t] * d.erScale * (0.75 + 0.5 * size) * 0.001 * fs;
        erG[(size_t) t] = kErGain[t];
    }
    preDelay = d.preMs * 0.001 * fs;
    erLevel = d.erLevel;
    outTrim = d.outTrim;
    hpC.makeHighpass (SvfCoeffs::prewarp (d.hpHz, fs), SvfCoeffs::kButterworth);
    if (p.room != curType && ! first) reset();   // the engine fades the return around type changes
    curType = p.room;
    first = false;
}

void Room::process (const double* in, double* outL, double* outR, int n) noexcept
{
    const int pm = (int) pre.size() - 1;
    for (int i = 0; i < n; ++i)
    {
        // send conditioning: 4th-order high-pass keeps bass fundamentals out of the room
        const double x = hp2.process (hp1.process (in[i], hpC), hpC);
        pre[(size_t) prePos] = x;
        const double feed = readFrac (pre, prePos, preDelay);

        // early reflections (stereo alternation)
        double erL = 0.0, erR = 0.0;
        for (int t = 0; t < kTaps; ++t)
        {
            const double v = erG[(size_t) t] * readFrac (pre, prePos, preDelay + erT[(size_t) t]);
            if (t & 1) erR += v; else erL += v;
        }
        prePos = (prePos + 1) & pm;

        // late field: FDN
        lfoPhase += lfoInc;
        if (lfoPhase > 2.0 * kPi) lfoPhase -= 2.0 * kPi;
        const double mod = std::sin (lfoPhase) * 0.0004 * fs; // +/- 0.4 ms on two lines
        double o[kLines];
        for (int k = 0; k < kLines; ++k)
        {
            len[(size_t) k] += (lenTarget[(size_t) k] - len[(size_t) k]) * 0.0005; // glide on Size changes
            double dl = len[(size_t) k];
            if (k == 2) dl += mod;
            if (k == 5) dl -= mod;
            o[k] = dampF[(size_t) k].lp (readFrac (line[(size_t) k], pos, dl)) * fb[(size_t) k];
        }
        // fast Walsh-Hadamard transform (orthogonal, energy preserving), scaled 1/sqrt(8)
        for (int h = 1; h < kLines; h <<= 1)
            for (int a = 0; a < kLines; a += h << 1)
                for (int b = a; b < a + h; ++b)
                {
                    const double u = o[b], v = o[b + h];
                    o[b] = u + v; o[b + h] = u - v;
                }
        double lateL = 0.0, lateR = 0.0;
        for (int k = 0; k < kLines; ++k)
        {
            const double v = o[k] * 0.35355339059327373;
            auto& buf = line[(size_t) k];
            buf[(size_t) (pos & ((int) buf.size() - 1))] = flushDenormal (v + feed * 0.5 * ((k & 1) ? -1.0 : 1.0) + (k < 4 ? erL : erR) * 0.15);
            lateL += kOutL[k] * v;
            lateR += kOutR[k] * v;
        }
        pos = (pos + 1) & 0x3fffffff;
        outL[i] = outTrim * (erLevel * erL + 0.5 * lateL);
        outR[i] = outTrim * (erLevel * erR + 0.5 * lateR);
    }
}
} // namespace bassforge
