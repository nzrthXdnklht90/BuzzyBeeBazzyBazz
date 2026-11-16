// Shared helpers for BASSFORGE tests.
#pragma once
#include "TestFramework.h"
#include "Engine.h"
#include <cstdio>
#include <string>
#include <vector>

namespace bftest
{
using namespace bassforge;
inline std::string fmt (const char* f, double a = 0, double b = 0, double c = 0)
{
    char buf[256]; std::snprintf (buf, sizeof buf, f, a, b, c); return buf;
}
/** Magnitude of the DFT of x at hz (Goertzel). */
inline double dftMag (const std::vector<double>& x, double hz, double fs, size_t start = 0, size_t len = 0)
{
    if (len == 0) len = x.size() - start;
    const double w = 2.0 * kPi * hz / fs;
    double re = 0, im = 0;
    for (size_t i = 0; i < len; ++i) { re += x[start + i] * std::cos (w * i); im -= x[start + i] * std::sin (w * i); }
    return std::sqrt (re * re + im * im);
}
inline double rms (const std::vector<double>& x, size_t start = 0, size_t len = 0)
{
    if (len == 0) len = x.size() - start;
    double e = 0; for (size_t i = 0; i < len; ++i) e += x[start + i] * x[start + i];
    return std::sqrt (e / std::max<size_t> (1, len));
}
inline double peak (const std::vector<double>& x, size_t start = 0)
{
    double p = 0; for (size_t i = start; i < x.size(); ++i) p = std::max (p, std::abs (x[i])); return p;
}
/** Synthetic picked-bass phrase: band-limited saw-like notes with pluck envelopes (B0, E1, A1, D2, octave jumps).
    peakTarget: approximate sample peak (nominal DI ~ 0.25 = -12 dBFS). */
inline std::vector<double> bassPhrase (double fs, double seconds, double peakTarget = 0.25, uint64_t seed = 7)
{
    const double notes[] = { 30.87, 41.20, 55.0, 73.42, 41.20, 82.41, 30.87, 61.74 };
    std::vector<double> y ((size_t) (fs * seconds), 0.0);
    const double noteLen = 0.25;
    Prng r; r.seed (seed);
    for (size_t i = 0; i < y.size(); ++i)
    {
        const double t = i / fs;
        const int k = (int) (t / noteLen);
        const double tn = t - k * noteLen;
        const double f0 = notes[k % 8];
        const double env = std::exp (-tn * 4.0) * (1.0 - std::exp (-tn * 800.0));
        double s = 0;
        for (int h = 1; h <= 24 && h * f0 < 0.45 * fs; ++h)
            s += std::sin (2 * kPi * h * f0 * tn + 0.3 * h) / h * std::exp (-tn * h * 0.8);
        y[i] = s * env;
    }
    const double p = peak (y);
    for (auto& v : y) v *= peakTarget / p;
    (void) r;
    return y;
}
/** Run a mono signal through the engine; returns stereo output (L, R). */
struct Stereo { std::vector<double> l, r; };
inline Stereo runEngine (Engine& e, const std::vector<double>& in, int block = 256)
{
    Stereo o; o.l.assign (in.size(), 0.0); o.r.assign (in.size(), 0.0);
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in.size() - pos);
        const double* ip[1] = { in.data() + pos };
        double* op[2] = { o.l.data() + pos, o.r.data() + pos };
        e.process (ip, 1, op, 2, n);
    }
    return o;
}
/** Parameter set where every block is neutral/off except what the test switches on. */
inline EngineParams neutralParams()
{
    EngineParams p;
    p.driveOn = false; p.cabOn = false; p.peqOn = false; p.roomOn = false;
    p.diCompOn = false; p.diEqOn = false; p.mixEqOn = false; p.levelOn = false; p.protectOn = false;
    p.faderDrive = 0; p.faderDi = 0; p.faderRoom = 0; p.masterDb = 0;
    p.ownership = LowOwnership::FullParallel;
    return p;
}
} // namespace bftest
