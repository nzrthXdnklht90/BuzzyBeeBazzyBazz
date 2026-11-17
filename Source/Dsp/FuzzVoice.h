// BASSFORGE - FUZZ voice ("Fast Pickle"): a reduced-order model of the Swollen Pickle MkII-topology
// circuit from the Big Dill project, built for BASSFORGE's multiband engine.
// Copyright (c) 2026 Nazareth. Original code.
//
// Where it comes from: Big Dill's netlist (spice/mkII_circuit.inc, 470k/100k voicing, NAM-calibrated
// diodes) and its validated full-circuit engine, which is used here as the REFERENCE the reduced
// model is fitted to (tools/fuzz_fit). Same component values, same stage order, same pot laws.
//
// What is reduced (so it costs ~1/20 of the full solver):
//  * each transistor stage = one node equation at the base: input R-C branch, Rf || Cf feedback,
//    the base resistors, the diode branches with their series caps and pot resistances (exact
//    trapezoidal companions), and the transistor as a linear transconductance stage with a smooth,
//    asymmetric output-swing limit (cutoff / saturation). One 1-D Newton solve per stage per sample.
//  * the passive Filter / Scoop / Voice tone network is solved EXACTLY (linear nodal, trapezoidal).
//  * Q4 is a degenerated CE gain stage with its own asymmetric swing limit.
// Fitted constants (FuzzFit) are the per-stage loop gains and swing limits; they are listed with
// their fit error in docs/fuzz_voice_report.md.
#pragma once

#include "Filters.h"
#include <array>

namespace bassforge
{
/** Constants the fit adjusts. Defaults = values fitted on 17 Nov 2026 against Big Dill's validated
    full-circuit engine (84 cases; tools/fuzz_fit, docs/fuzz_voice_report.md). Starting values were the
    hand estimates from the circuit (aol 40, swing 4.5/3.2 V, Q4 6.2 with 6.0/1.7 V, 3k, 68k). */
struct FuzzFit
{
    double aol[3] = { 21.92, 21.92, 21.92 };  // open-loop gain of Q1..Q3 (gm_eff * Rc)
    double vPos[3] = { 7.148, 7.148, 7.148 }; // output swing, positive side (V)
    double vNeg[3] = { 1.864, 1.864, 1.864 }; // output swing, negative side (V)
    double q4Gain = 6.704, q4Pos = 3.227, q4Neg = 5.931;
    double toneSrcOhms = 3618.7;              // Q3 collector output impedance seen by the tone network
    double rpi = 1461993.0;                   // transistor base input resistance (fit drives it high: negligible)
};

/** User-facing controls, 0..1 like the pedal's pots (0 = fully CCW). */
struct FuzzControls
{
    double sustain = 0.5, filter = 0.5, scoop = 0.5, crunch = 0.5, clip = 1.0;
};

/** Small exact linear nodal network, trapezoidal companions, used for the tone stack. */
class LinearNet
{
public:
    static constexpr int kMaxN = 10, kMaxC = 6, kMaxR = 12;
    void begin (int nodes, double fs);
    void addR (int a, int b, double ohms);          // node -1 = ground
    void addC (int a, int b, double farads);
    void setSource (int node, double seriesOhms);   // Thevenin input into 'node'
    void setOutput (int node) { out = node; }
    void finalize();                                // builds and inverts the conductance matrix
    void reset();
    inline double process (double u) noexcept
    {
        double rhs[kMaxN] {};
        rhs[src] += u * gSrc;
        for (int k = 0; k < nc; ++k)
        {
            if (ca[k] >= 0) rhs[ca[k]] += hist[k];
            if (cb[k] >= 0) rhs[cb[k]] -= hist[k];
        }
        double v[kMaxN];
        for (int i = 0; i < n; ++i)
        {
            double s = 0.0;
            const double* row = inv + i * kMaxN;
            for (int j = 0; j < n; ++j) s += row[j] * rhs[j];
            v[i] = s;
        }
        for (int k = 0; k < nc; ++k)
        {
            const double vk = (ca[k] >= 0 ? v[ca[k]] : 0.0) - (cb[k] >= 0 ? v[cb[k]] : 0.0);
            // trapezoidal companion: i = G v - J ; J_next = G v + i = 2 G v - J
            hist[k] = flushDenormal (2.0 * gc[k] * vk - hist[k]);
        }
        return v[out];
    }

private:
    int n = 0, nc = 0, nr = 0, src = 0, out = 0;
    double fs = 48000.0, gSrc = 0.0;
    int ra[kMaxR] {}, rb[kMaxR] {}; double rg[kMaxR] {};
    int ca[kMaxC] {}, cb[kMaxC] {}; double gc[kMaxC] {}, hist[kMaxC] {};
    double inv[kMaxN * kMaxN] {};
};

/** One shunt-feedback common-emitter stage with optional diode-pair feedback branches. */
class FuzzStage
{
public:
    /** Diode pair + series R (+ series-cap companion). The branch current i(w) for the voltage w across
        (R + diodes) is pre-solved into a table (value + slope, cubic Hermite), rebuilt only when R changes. */
    struct Branch
    {
        bool on = false; double is = 1e-9, nvt = 0.045, rs = 0.0, c = 100e-9;
        double vcap = 0.0, iPrev = 0.0;
        static constexpr int kN = 512; static constexpr double kW = 8.0;   // table spans +/- 8 V (float: stays in L1 cache)
        float ti[kN + 1] {}, tg[kN + 1] {}; double builtRs = -1.0;
        void build (double rsEff);
        inline double eval (double w, double& didw) const noexcept
        {
            constexpr double invH = kN / (2.0 * kW);
            const double x = (std::clamp (w, -kW, kW) + kW) * invH;
            const int k = std::min ((int) x, kN - 1);
            const double t = x - k;
            // tg holds the slope pre-multiplied by the grid step (Hermite tangents)
            const double y0 = ti[k], y1 = ti[k + 1], m0 = tg[k], m1 = tg[k + 1];
            const double d = y1 - y0;
            const double a = m0 + m1 - 2.0 * d, b = 3.0 * d - 2.0 * m0 - m1;   // y = y0 + m0 t + b t^2 + a t^3
            didw = (m0 + t * (2.0 * b + 3.0 * a * t)) * invH;
            return y0 + t * (m0 + t * (b + a * t));
        }
    };
    void prepare (double fs, double rin, double cin, double rf, double cf, double rb, double rpi);
    void setInput (double rin, double cin) noexcept;   // sustain pot changes the source resistance
    void setGain (double aol, double vpos, double vneg, double knee) noexcept { A = aol; vp = vpos; vn = vneg; kn = knee; }
    void reset() noexcept;
    double process (double u) noexcept;
    void updateBranches();   // control rate: rebuild tables whose resistance changed
    std::array<Branch, 2> br;

private:
    inline double swing (double z, double& dz) const noexcept;
    double fs = 192000.0, T = 1.0 / 192000.0;
    double rinEff = 10e3, cin = 1e-6, rf = 470e3, gcf = 0.0, gb = 0.0, A = 40.0, vp = 4.5, vn = 3.2, kn = 2.0;
    double vcin = 0.0, iin = 0.0, jf = 0.0, vb = 0.0;
};

class FuzzVoice
{
public:
    void prepare (double fs);
    void reset() noexcept;
    void setControls (const FuzzControls& c) noexcept;   // control rate
    void setFit (const FuzzFit& f);                     // tests / fitting tool
    /** Volts in -> volts out at the Loudness-max output. */
    inline double processVolts (double x) noexcept
    {
        double y = s1.process (x);
        y *= sustainFrac;
        y = s2.process (y);
        y = s3.process (y);
        y = tone.process (y);
        double d;
        y = q4Swing (-fit.q4Gain * y, d);
        return dcOut.hp (y);
    }
    /** BASSFORGE units: 1.0 full scale = 2 V at the pedal input. */
    void process (double* buf, int n) noexcept;
    double outScale = 0.25;   // volts -> digital (pre loudness compensation)

private:
    inline double q4Swing (double z, double& dz) const noexcept
    {
        const double a = z / (z >= 0.0 ? fit.q4Pos : fit.q4Neg);
        const double base = 1.0 + a * a;
        const double s = 1.0 / std::sqrt (base);
        dz = s / base;
        return z * s;
    }
    void rebuildTone();
    double fs = 192000.0, sustainFrac = 0.5;
    FuzzStage s1, s2, s3;
    LinearNet tone;
    OnePole dcOut;
    FuzzFit fit;
    FuzzControls ctl;
    bool toneDirty = true;
};
} // namespace bassforge
