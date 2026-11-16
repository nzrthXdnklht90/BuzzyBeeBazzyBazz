// Component-level DSP tests (filters, crossover, oversampling, voices, dynamics, cabinet, room, EQs).
#include "TestHelpers.h"

using namespace bftest;

namespace
{
std::vector<double> impulseThrough (Svf& f, const SvfCoeffs& c, int len)
{
    std::vector<double> y ((size_t) len);
    for (int i = 0; i < len; ++i) y[(size_t) i] = f.process (i == 0 ? 1.0 : 0.0, c);
    return y;
}
}

BF_TEST ("SVF-01", "SVF magnitude() matches the impulse response (bell, shelves, LP, HP, AP)")
{
    const double fs = 48000;
    std::vector<SvfCoeffs> cs (6);
    const double t = SvfCoeffs::prewarp (800, fs);
    cs[0].makeBell (t, 9, 1.5); cs[1].makeLowShelf (t, -7); cs[2].makeHighShelf (t, 5, 1.0 / 0.6);
    cs[3].makeLowpass (t, 1.0 / 0.9); cs[4].makeHighpass (t); cs[5].makeAllpass (t);
    double worst = 0;
    for (auto& c : cs)
    {
        Svf f; auto h = impulseThrough (f, c, 1 << 14);
        for (double hz : { 30.0, 120.0, 500.0, 800.0, 2000.0, 9000.0, 18000.0 })
        {
            const double meas = gainToDb (dftMag (h, hz, fs)), pred = gainToDb (c.magnitude (hz, fs));
            worst = std::max (worst, std::abs (meas - pred));
        }
    }
    note (fmt ("worst |measured - predicted| = %.4f dB", worst));
    BF_CHECK (worst < 0.02);
}

BF_TEST ("XO-01", "3-way LR4 crossover with low-branch allpass reconstructs flat (= AP_lo * AP_hi)")
{
    const double fs = 48000;
    for (auto fr : { std::pair<double, double> { 140, 850 }, { 70, 300 }, { 400, 2000 } })
    {
        LR4 a, b; a.setFreq (fr.first, fs); b.setFreq (fr.second, fs);
        SvfCoeffs apc; apc.makeAllpass (SvfCoeffs::prewarp (fr.second, fs));
        SvfCoeffs apl; apl.makeAllpass (SvfCoeffs::prewarp (fr.first, fs));
        Svf ap, r1, r2;
        const int N = 1 << 15;
        std::vector<double> sum ((size_t) N), ref ((size_t) N);
        double maxErr = 0;
        for (int i = 0; i < N; ++i)
        {
            const double x = i == 0 ? 1.0 : 0.0;
            double lo, rest, mid, hi;
            a.process (x, lo, rest); b.process (rest, mid, hi);
            sum[(size_t) i] = ap.process (lo, apc) + mid + hi;
            ref[(size_t) i] = r2.process (r1.process (x, apl), apc);
            maxErr = std::max (maxErr, std::abs (sum[(size_t) i] - ref[(size_t) i]));
        }
        double worst = 0;
        for (double hz = 20; hz < 20000; hz *= 1.12)
            worst = std::max (worst, std::abs (gainToDb (dftMag (sum, hz, fs))));
        note (fmt ("xover %.0f/%.0f Hz:", fr.first, fr.second) + fmt (" max |mag| deviation %.5f dB, max |sum - AP*AP| %.2e", worst, maxErr));
        BF_CHECK (worst < 0.01);
        BF_CHECK (maxErr < 1e-9);
    }
}

BF_TEST ("OS-01", "oversampler round trip: integer latency after padding, flat passband (all qualities)")
{
    // exercised through the engine's DI-alignment test as well; here the raw resampler
    const double fs = 48000;
    struct Q { int stages; std::vector<HalfbandSpec> hb; DecimatorSpec dec; double flatTo; };
    std::vector<Q> qs = { { 1, { { 0.205, 70 } }, { 0.41, 0.5, 70 }, 18000 },
                          { 2, { { 0.21, 90 }, { 0.115, 90 } }, { 0.42, 0.5, 90 }, 19000 },
                          { 3, { { 0.215, 100 }, { 0.115, 100 }, { 0.06, 100 } }, { 0.43, 0.5, 100 }, 19500 } };
    for (auto& q : qs)
    {
        OversamplerChannel os; os.prepare (q.stages, q.hb, q.dec, 32);
        const int f = os.getFactor(), rt = os.getRoundTripLatencyTop(), pad = (f - rt % f) % f, lat = (rt + pad) / f;
        DelayLine padL; padL.prepare (16);
        const int N = 8192;
        std::vector<double> in ((size_t) N, 0.0), out ((size_t) N, 0.0), up (32 * (size_t) f);
        in[0] = 1.0;
        for (int p = 0; p < N; p += 32)
        {
            os.upsample (in.data() + p, up.data(), 32);
            for (int i = 0; i < 32 * f; ++i) up[(size_t) i] = padL.process (up[(size_t) i], (size_t) pad);
            os.downsample (up.data(), out.data() + p, 32);
        }
        size_t pk = 0; for (size_t i = 0; i < out.size(); ++i) if (std::abs (out[i]) > std::abs (out[pk])) pk = i;
        double worst = 0;
        for (double hz = 20; hz < q.flatTo; hz *= 1.1) worst = std::max (worst, std::abs (gainToDb (dftMag (out, hz, fs))));
        note (fmt ("%.0fx: latency %.0f samples (impulse peak at %.0f)", f, lat, (double) pk) + fmt (", passband ripple %.3f dB", worst));
        BF_CHECK ((int) pk == lat);
        BF_CHECK (worst < 0.1);
    }
}

BF_TEST ("VOICE-01", "drive voices stay finite and bounded under extreme input, all models")
{
    DriveVoiceBank b; b.prepare (192000, 256);
    for (int m = 0; m < 3; ++m)
        for (double d : { 0.0, 0.5, 1.0 })
        {
            VoiceSetup s; s.drive = d; s.offsetDb = 12; s.character = 1.0;
            b.reset(); b.setup ((DriveModel) m, s);
            std::vector<double> x (192000 / 10);
            double pk = 0; bool finite = true;
            for (size_t i = 0; i < x.size(); ++i) x[i] = 10.0 * std::sin (2 * kPi * 400 * i / 192000.0);
            for (size_t p = 0; p < x.size(); p += 256) b.process (x.data() + p, (int) std::min<size_t> (256, x.size() - p));
            for (auto v : x) { finite &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
            BF_CHECK (finite);
            BF_CHECK_MSG (pk < 8.0, fmt ("model %.0f drive %.1f peak %.2f", m, d, pk));
        }
}

BF_TEST ("VOICE-02", "voices are harmonically distinct (even/odd balance) and calibration is loudness-neutral")
{
    const double fs = 192000;
    DriveVoiceBank b; b.prepare (fs, 256); b.calibrate (BandRole::Mid);
    double ratio[3] {};
    for (int m = 0; m < 3; ++m)
    {
        VoiceSetup s; s.drive = 0.5; s.character = 0.5;
        b.reset(); b.setup ((DriveModel) m, s);
        const int N = (int) fs / 2;
        std::vector<double> x ((size_t) N);
        for (int i = 0; i < N; ++i) x[(size_t) i] = 0.1 * std::sin (2 * kPi * 250 * i / fs);
        const double inRms = rms (x, (size_t) N / 2);
        for (int p = 0; p < N; p += 256) b.process (x.data() + p, 256);
        const size_t st = (size_t) N / 2, len = (size_t) fs / 250 * 50;
        const double h1 = dftMag (x, 250, fs, st, len), h2 = dftMag (x, 500, fs, st, len), h3 = dftMag (x, 750, fs, st, len);
        ratio[m] = h2 / std::max (h3, 1e-12);
        const double outRms = rms (x, st);
        note (fmt ("model %.0f: H2/H1 %.1f dB, H3/H1 %.1f dB", m, gainToDb (h2 / h1), gainToDb (h3 / h1)) + fmt (", out/in RMS %.2f dB", gainToDb (outRms / inRms)));
        BF_CHECK (std::abs (gainToDb (outRms / inRms)) < 3.0);
    }
    BF_CHECK (ratio[0] > ratio[2]);   // VINTAGE is more even-order than MODERN
}

BF_TEST ("DYN-01", "low-band compressor: ~4:1 above threshold; FET + opto programme-dependent release")
{
    const double fs = 48000;
    {
        LowBandCompressor c; c.prepare (fs); c.setAmount (0.8);
        auto steady = [&] (double amp) {
            c.reset(); double last = 0;
            for (int i = 0; i < (int) fs; ++i) { const double y = c.process (amp * std::sin (2 * kPi * 60 * i / fs)); last = std::max (last * 0.9999, std::abs (y)); }
            return gainToDb (last);
        };
        const double o1 = steady (dbToGain (-6)), o2 = steady (dbToGain (6));
        const double slope = 12.0 / std::max (o2 - o1, 1e-6);
        note (fmt ("low comp: 12 dB input change -> %.2f dB output change (ratio ~%.1f:1)", o2 - o1, slope));
        BF_CHECK (slope > 3.0 && slope < 5.5);
    }
    auto recoveryMs = [&] (auto& comp, double burstS, auto getGr) {
        comp.reset();
        for (int i = 0; i < (int) (burstS * fs); ++i) comp.process (0.8 * std::sin (2 * kPi * 80 * i / fs));
        for (int i = 0; i < (int) (8 * fs); ++i) { comp.process (0.0); if (getGr() > -1.0) return i * 1000.0 / fs; }
        return 8000.0;
    };
    {
        FetCompressor f; f.prepare (fs); f.setAmount (0.8);
        const double shortR = recoveryMs (f, 0.05, [&] { return f.getGrDb(); }), longR = recoveryMs (f, 2.0, [&] { return f.getGrDb(); });
        note (fmt ("FET recovery to -1 dB: after 50 ms burst %.0f ms, after 2 s %.0f ms", shortR, longR));
        BF_CHECK (longR > shortR * 1.5);
    }
    {
        OptoLeveller o; o.prepare (fs); o.setAmount (0.7);
        auto rec = [&] (double burstS) {
            o.reset(); double l, r;
            for (int i = 0; i < (int) (burstS * fs); ++i) { l = r = 0.7 * std::sin (2 * kPi * 80 * i / fs); o.processFrame (l, r); }
            for (int i = 0; i < (int) (10 * fs); ++i) { l = r = 0.001; o.processFrame (l, r); if (o.getGrDb() > -1.0) return i * 1000.0 / fs; }
            return 10000.0;
        };
        const double a = rec (0.2), b = rec (4.0);
        note (fmt ("opto recovery to -1 dB: after 0.2 s %.0f ms, after 4 s %.0f ms", a, b));
        BF_CHECK (b > a * 1.5);
    }
}

BF_TEST ("CAB-01", "four cabinets: level-matched (band noise), audibly distinct responses, finite")
{
    const double fs = 48000;
    std::vector<std::vector<double>> spectra;
    std::vector<double> levels;
    for (int c = 0; c < 4; ++c)
    {
        Cabinet cab; cab.prepare (fs);
        EngineParams p; p.cab = (CabModel) c; p.mic[0].position = 1.0 / 3.0; p.mic[0].distanceIn = 1.0; p.mic[1].on = false;
        for (int k = 0; k < 40; ++k) cab.setParams (p);
        Prng r; r.seed (99);
        OnePole h1, h2, l1, l2; h1.setCutoff (150, fs); h2.setCutoff (150, fs); l1.setCutoff (5000, fs); l2.setCutoff (5000, fs);
        const int N = (int) fs;
        std::vector<double> in ((size_t) N), L ((size_t) N), R ((size_t) N), imp ((size_t) 8192, 0.0), iL (8192), iR (8192);
        for (auto& v : in) v = l2.lp (l1.lp (h2.hp (h1.hp (r.uniform() * 2 - 1))));
        for (int pos = 0; pos < N; pos += 32) cab.process (in.data() + pos, L.data() + pos, R.data() + pos, 32);
        levels.push_back (gainToDb (rms (L, 4800) / rms (in, 4800)));
        cab.reset(); imp[0] = 1.0;
        for (int pos = 0; pos < 8192; pos += 32) cab.process (imp.data() + pos, iL.data() + pos, iR.data() + pos, 32);
        std::vector<double> sp;
        for (double hz = 100; hz < 10000; hz *= 1.25) sp.push_back (gainToDb (dftMag (iL, hz, fs)));
        spectra.push_back (sp);
        note (std::string (cabProfile ((CabModel) c).name) + fmt (": band-noise gain %.2f dB", levels.back()));
    }
    for (auto l : levels) BF_CHECK (std::abs (l - levels[0]) < 1.5);
    for (int a = 0; a < 4; ++a)
        for (int b = a + 1; b < 4; ++b)
        {
            double maxDiff = 0;
            for (size_t i = 0; i < spectra[0].size(); ++i) maxDiff = std::max (maxDiff, std::abs (spectra[(size_t) a][i] - spectra[(size_t) b][i]));
            BF_CHECK_MSG (maxDiff > 3.0, fmt ("cab %.0f vs %.0f max spectral difference %.1f dB", a, b, maxDiff));
        }
}

BF_TEST ("MIC-01", "mic position/distance/type move the response predictably")
{
    const double fs = 48000;
    auto response = [&] (MicType t, double pos, double dist) {
        Cabinet cab; cab.prepare (fs);
        EngineParams p; p.cab = CabModel::Classic810; p.mic[0].type = t; p.mic[0].position = pos; p.mic[0].distanceIn = dist; p.mic[1].on = false;
        for (int k = 0; k < 60; ++k) cab.setParams (p);
        std::vector<double> imp (8192, 0.0), L (8192), R (8192); imp[0] = 1.0;
        for (int i = 0; i < 8192; i += 32) cab.process (imp.data() + i, L.data() + i, R.data() + i, 32);
        return L;
    };
    const auto cap = response (MicType::Dynamic, 0.0, 1.0), edge = response (MicType::Dynamic, 1.0, 1.0);
    const double hfCap = dftMag (cap, 3000, fs), hfEdge = dftMag (edge, 3000, fs);
    note (fmt ("3 kHz: cap %.1f dB vs cone edge %.1f dB", gainToDb (hfCap), gainToDb (hfEdge)));
    BF_CHECK (hfCap > hfEdge * 1.4);
    const auto near = response (MicType::Ribbon, 0.33, 0.0), far = response (MicType::Ribbon, 0.33, 6.0);
    const double prox = gainToDb (dftMag (near, 90, fs) / dftMag (near, 1000, fs)) - gainToDb (dftMag (far, 90, fs) / dftMag (far, 1000, fs));
    note (fmt ("ribbon proximity: 90 Hz vs 1 kHz tilt is %.1f dB stronger at 0 in than at 6 in", prox));
    BF_CHECK (prox > 4.0);
}

BF_TEST ("ROOM-01", "room: decays, keeps lows out of the return, decorrelated stereo, longer for larger rooms")
{
    const double fs = 48000;
    double decay[3] {};
    for (int t = 0; t < 3; ++t)
    {
        Room room; room.prepare (fs);
        EngineParams p; p.room = (RoomType) t;
        room.setParams (p);
        const int N = (int) (fs * 3);
        std::vector<double> in ((size_t) N, 0.0), L ((size_t) N), R ((size_t) N);
        for (int i = 0; i < (int) (0.05 * fs); ++i) in[(size_t) i] = std::sin (2 * kPi * 50 * i / fs) + 0.5 * std::sin (2 * kPi * 900 * i / fs);
        for (int i = 0; i < N; i += 32) { room.setParams (p); room.process (in.data() + i, L.data() + i, R.data() + i, 32); }
        // energy envelope: time to fall 30 dB from the max 50 ms window
        auto winE = [&] (int s) { double e = 0; for (int i = s; i < s + 2400 && i < N; ++i) e += L[(size_t) i] * L[(size_t) i] + R[(size_t) i] * R[(size_t) i]; return e; };
        double emax = 0; for (int s = 0; s < N - 2400; s += 1200) emax = std::max (emax, winE (s));
        int t30 = N;
        for (int s = 0; s < N - 2400; s += 1200) if (winE (s) < emax * 1e-3) { t30 = s; break; }
        decay[t] = t30 / fs;
        const double lf = dftMag (L, 50, fs, 0, (size_t) (0.3 * fs)), mf = dftMag (L, 900, fs, 0, (size_t) (0.3 * fs));
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = (int) (0.05 * fs); i < (int) (0.5 * fs); ++i) { sxy += L[(size_t) i] * R[(size_t) i]; sxx += L[(size_t) i] * L[(size_t) i]; syy += R[(size_t) i] * R[(size_t) i]; }
        const double corr = sxy / std::sqrt (sxx * syy + 1e-30);
        bool finite = true; for (int i = 0; i < N; ++i) finite &= std::isfinite (L[(size_t) i]) && std::isfinite (R[(size_t) i]);
        note (fmt ("room %.0f: -30 dB after %.2f s, 50 Hz vs 900 Hz %.1f dB", t, decay[t], gainToDb (lf / mf)) + fmt (", L/R correlation %.2f", corr));
        BF_CHECK (finite);
        BF_CHECK (gainToDb (lf / mf) < -20.0);
        BF_CHECK (std::abs (corr) < 0.6);
        BF_CHECK (decay[t] < 2.9);
    }
    BF_CHECK (decay[0] < decay[1] && decay[1] < decay[2]);
}

BF_TEST ("EQ-01", "7-band EQ and Mix EQ are exactly flat at zero; programme EQ boost+atten gives bump-and-dip")
{
    const double fs = 48000;
    EqBand bands[7];
    for (int i = 0; i < 7; ++i) bands[i].gainDb = 0;
    double worst = 0;
    for (double hz = 20; hz < 20000; hz *= 1.1) worst = std::max (worst, std::abs (ParametricEq7::responseDb (bands, hz, fs)));
    ProgramEq::Settings z { 0, 0, 0, 0.5, 0, 1, 3, 1 };
    for (double hz = 20; hz < 20000; hz *= 1.1) worst = std::max (worst, std::abs (ProgramEq::responseDb (z, hz, fs)));
    note (fmt ("flat-at-zero deviation %.2e dB", worst));
    BF_CHECK (worst < 1e-9);
    ProgramEq::Settings s { 5, 5, 0, 0.5, 0, 2, 3, 1 }; // 60 Hz boost 5 + atten 5
    const double at40 = ProgramEq::responseDb (s, 40, fs), at300 = ProgramEq::responseDb (s, 300, fs);
    note (fmt ("60 Hz boost5+atten5: %+.1f dB at 40 Hz, %+.1f dB at 300 Hz", at40, at300));
    BF_CHECK (at40 > 1.0 && at300 < -1.0);
}
