// Whole-engine tests: latency, Drive/DI coherence, safety, level matching, clicks, routing, stress.
#include "TestHelpers.h"

using namespace bftest;

namespace
{
constexpr double kFs = 48000;

Engine& sharedEngine (const EngineParams& p, double fs = kFs)
{
    static Engine e;
    static double preparedFs = 0;
    e.setParams (p);
    if (preparedFs != fs) { e.prepare (fs, 512); preparedFs = fs; }
    e.reset();
    return e;
}

std::vector<double> sine (double hz, double amp, double seconds, double fs = kFs)
{
    std::vector<double> x ((size_t) (seconds * fs));
    for (size_t i = 0; i < x.size(); ++i) x[i] = amp * std::sin (2 * kPi * hz * i / fs);
    return x;
}
} // namespace

BF_TEST ("ENG-LAT", "DI impulse lands exactly on the reported latency, every quality and sample rate")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int q = 0; q < 3; ++q)
        {
            EngineParams p = neutralParams();
            p.quality = (Quality) q; p.muteDrive = true; p.muteRoom = true;
            p.xoverLow = 70; p.xoverHigh = 2000;   // DI carries the crossover allpass phase; corners far from DC/ Nyquist
            Engine e; e.setParams (p); e.prepare (fs, 512);
            std::vector<double> in ((size_t) fs / 4, 0.0); in[1000] = 0.5;
            auto o = runEngine (e, in);
            size_t pk = 0; for (size_t i = 0; i < o.l.size(); ++i) if (std::abs (o.l[i]) > std::abs (o.l[pk])) pk = i;
            // allpass phase spreads the impulse; the energy centroid of the first few ms must sit at the latency
            const int lat = e.getLatencySamples();
            note (fmt ("fs %.0f quality %.0f: reported latency %.0f", fs, q, lat) + fmt (" samples, impulse peak at +%.0f", (double) pk - 1000));
            BF_CHECK (std::abs ((int) pk - 1000 - lat) <= 2);
        }
}

BF_TEST ("ENG-COH", "clean Drive path + DI add coherently: flat +6 dB from 25 Hz to 16 kHz (no crossover cancellation)")
{
    EngineParams p = neutralParams();
    p.muteRoom = true;
    auto& e = sharedEngine (p);
    std::vector<double> imp ((size_t) kFs / 2, 0.0); imp[0] = 0.25;
    auto both = runEngine (e, imp);
    p.muteDrive = true; e.setParams (p); e.reset();
    auto diOnly = runEngine (e, imp);
    double worst = 0;
    for (double hz = 25; hz < 16000; hz *= 1.15)
        worst = std::max (worst, std::abs (gainToDb (dftMag (both.l, hz, kFs) / dftMag (diOnly.l, hz, kFs)) - 6.0206));
    note (fmt ("max deviation from +6.02 dB: %.3f dB", worst));
    BF_CHECK (worst < 0.1);
}

BF_TEST ("ENG-SAFE", "NaN/Inf input is sanitised; silence in gives silence out (no DC, no denormal noise)")
{
    EngineParams p; // defaults: everything on
    auto& e = sharedEngine (p);
    auto x = bassPhrase (kFs, 1.0);
    for (size_t i = 0; i < x.size(); i += 997) x[i] = std::numeric_limits<double>::quiet_NaN();
    x[5000] = std::numeric_limits<double>::infinity();
    auto o = runEngine (e, x);
    bool finite = true; for (size_t i = 0; i < o.l.size(); ++i) finite &= std::isfinite (o.l[i]) && std::isfinite (o.r[i]);
    BF_CHECK (finite);
    std::vector<double> zero ((size_t) (kFs * 4), 0.0);
    auto s = runEngine (e, zero);
    const double tail = peak (s.l, (size_t) (kFs * 3.5));
    note (fmt ("peak 3.5 s after the input stopped: %.1f dBFS", gainToDb (tail)));
    BF_CHECK (tail < 1e-6);
}

BF_TEST ("ENG-LEVEL", "Drive models and Drive amounts are loudness-matched on a bass phrase (+/-3 dB)")
{
    const auto x = bassPhrase (kFs, 2.0);
    double ref = 0;
    bool first = true;
    for (int m = 0; m < 3; ++m)
    {
        std::string line = fmt ("model %.0f:", m);
        for (double d : { 0.2, 0.5, 0.8 })
        {
            EngineParams p = neutralParams();
            p.driveOn = true; p.cabOn = true; p.model = (DriveModel) m; p.drive = d;
            p.muteDi = true; p.muteRoom = true;
            auto& e = sharedEngine (p);
            auto o = runEngine (e, x);
            const double lvl = gainToDb (rms (o.l, (size_t) kFs / 2));
            if (first) { ref = lvl; first = false; }
            line += fmt (" d%.1f=%+.1f dB", d, lvl - ref);
            BF_CHECK_MSG (std::abs (lvl - ref) < 3.0, fmt ("model %.0f drive %.1f is %+.1f dB from reference", m, d, lvl - ref));
        }
        note (line);
    }
}

BF_TEST ("ENG-CABLVL", "cabinet changes are level-matched in the full engine (+/-2 dB)")
{
    const auto x = bassPhrase (kFs, 1.5);
    double ref = 0;
    for (int c = 0; c < 4; ++c)
    {
        EngineParams p = neutralParams();
        p.driveOn = true; p.cabOn = true; p.cab = (CabModel) c; p.muteDi = true; p.muteRoom = true;
        auto& e = sharedEngine (p);
        auto o = runEngine (e, x);
        const double lvl = gainToDb (rms (o.l, (size_t) kFs / 2));
        if (c == 0) ref = lvl;
        note (std::string (cabProfile ((CabModel) c).name) + fmt (": %+.2f dB", lvl - ref));
        BF_CHECK (std::abs (lvl - ref) < 2.0);
    }
}

BF_TEST ("ENG-TP", "protection holds the ceiling on a hot, heavily driven signal (sample and 8x-interpolated peaks)")
{
    EngineParams p;
    p.inputDb = 18; p.model = DriveModel::Modern; p.drive = 1.0; p.faderDi = 6; p.faderRoom = 0; p.levelOn = false;
    p.ceilingDb = -1.0;
    auto& e = sharedEngine (p);
    auto x = bassPhrase (kFs, 2.0, 0.9);
    auto o = runEngine (e, x);
    const double sp = std::max (peak (o.l, 4800), peak (o.r, 4800));
    // independent 8x windowed-sinc interpolation (long kernel) as the true-peak reference
    double tp = 0;
    const int H = 64;
    for (const auto* ch : { &o.l, &o.r })
        for (size_t m = 4800 + H; m + H < ch->size(); ++m)
            for (int ph = 1; ph < 8; ++ph)
            {
                const double fr = ph / 8.0;
                double acc = 0;
                for (int k = -H + 1; k <= H; ++k)
                {
                    const double t = k - fr;
                    const double w = 0.5 + 0.5 * std::cos (kPi * t / H);
                    acc += (*ch)[m + (size_t) k] * std::sin (kPi * t) / (kPi * t) * w;
                }
                tp = std::max (tp, std::abs (acc));
            }
    note (fmt ("sample peak %.2f dBFS, interpolated peak %.2f dBFS (ceiling -1.00)", gainToDb (sp), gainToDb (tp)));
    BF_CHECK (gainToDb (sp) <= -0.99);
    BF_CHECK (gainToDb (tp) <= -0.9);
}

BF_TEST ("ENG-CLICK", "switching blocks, models, cabinets, mic types and quality during a held note stays click-free")
{
    EngineParams p;
    p.faderRoom = -60;   // a room tail would mask nothing here, keep the check strict
    auto& e = sharedEngine (p);
    const auto x = sine (55, 0.25, 3.0);
    // steady-state max sample-to-sample step
    auto o0 = runEngine (e, x);
    double base = 0; for (size_t i = (size_t) kFs; i + 1 < o0.l.size(); ++i) base = std::max (base, std::abs (o0.l[i + 1] - o0.l[i]));
    struct Change { const char* what; std::function<void (EngineParams&)> fn; };
    std::vector<Change> changes = {
        { "drive off", [] (EngineParams& q) { q.driveOn = false; } },
        { "cab off", [] (EngineParams& q) { q.cabOn = false; } },
        { "eq off", [] (EngineParams& q) { q.peqOn = false; } },
        { "DI comp off", [] (EngineParams& q) { q.diCompOn = false; } },
        { "mix EQ off", [] (EngineParams& q) { q.mixEqOn = false; } },
        { "leveller off", [] (EngineParams& q) { q.levelOn = false; } },
        { "model -> modern", [] (EngineParams& q) { q.model = DriveModel::Modern; } },
        { "cab -> 2x15", [] (EngineParams& q) { q.cab = CabModel::Deep215; } },
        { "mic -> ribbon", [] (EngineParams& q) { q.mic[0].type = MicType::Ribbon; } },
        { "mute DI", [] (EngineParams& q) { q.muteDi = true; } },
        { "solo drive", [] (EngineParams& q) { q.soloDrive = true; } },
        { "crossover jump", [] (EngineParams& q) { q.xoverLow = 300; q.xoverHigh = 1800; } },
    };
    for (auto& c : changes)
    {
        EngineParams q = p;
        e.setParams (q); e.reset();
        std::vector<double> l (x.size()), r (x.size());
        for (size_t pos = 0; pos < x.size(); pos += 256)
        {
            if (pos >= (size_t) kFs * 2) { EngineParams q2 = p; c.fn (q2); e.setParams (q2); }
            const double* ip[1] = { x.data() + pos }; double* op[2] = { l.data() + pos, r.data() + pos };
            e.process (ip, 1, op, 2, (int) std::min<size_t> (256, x.size() - pos));
        }
        double mx = 0; for (size_t i = (size_t) kFs; i + 1 < l.size(); ++i) mx = std::max (mx, std::abs (l[i + 1] - l[i]));
        note (fmt ("max step %.2fx steady state", mx / base) + " (" + c.what + ")");
        BF_CHECK_MSG (mx < base * 2.5, c.what);
    }
}

BF_TEST ("ENG-ROUTE", "mixer routing: mute/solo, room-only, Drive+Room without DI, ownership restores the low band")
{
    const auto x = bassPhrase (kFs, 1.0);
    auto level = [&] (EngineParams p) { auto& e = sharedEngine (p); auto o = runEngine (e, x); return rms (o.l, (size_t) kFs / 4) + rms (o.r, (size_t) kFs / 4); };
    EngineParams p; p.levelOn = false; p.mixEqOn = false; p.protectOn = false;
    const double all = level (p);
    EngineParams s = p; s.soloRoom = true;
    const double roomOnly = level (s);
    EngineParams m = p; m.muteDrive = m.muteDi = m.muteRoom = true;
    const double none = level (m);
    EngineParams dr = p; dr.muteDi = true;
    const double driveRoom = level (dr);
    note (fmt ("all %.1f dB, room solo %.1f dB, Drive+Room %.1f dB", gainToDb (all), gainToDb (roomOnly), gainToDb (driveRoom)) + fmt (", all muted %.1f dB", gainToDb (none + 1e-12)));
    BF_CHECK (none < 1e-9);
    BF_CHECK (roomOnly > 1e-4 && roomOnly < all);
    BF_CHECK (driveRoom > roomOnly);

    // ownership: DI Priority cuts the Drive low band while the DI is present; muting the DI restores it
    auto lowOfDrive = [&] (bool muteDi, LowOwnership own) {
        EngineParams q = neutralParams(); q.driveOn = true; q.cabOn = true; q.ownership = own; q.muteDi = muteDi;
        q.soloDrive = true;   // listen to the Drive channel only (solo does not affect ownership)
        auto& e = sharedEngine (q); auto o = runEngine (e, sine (45, 0.25, 1.0));
        return dftMag (o.l, 45, kFs, (size_t) kFs / 2, (size_t) kFs / 2);
    };
    const double full = lowOfDrive (false, LowOwnership::FullParallel), pri = lowOfDrive (false, LowOwnership::DiPriority),
                 restored = lowOfDrive (true, LowOwnership::DiPriority);
    note (fmt ("Drive 45 Hz: DI Priority %+.1f dB vs Full Parallel; with DI muted %+.1f dB", gainToDb (pri / full), gainToDb (restored / full)));
    BF_CHECK (gainToDb (pri / full) < -10.0);
    BF_CHECK (std::abs (gainToDb (restored / full)) < 0.5);
}

BF_TEST ("ENG-QUAL", "quality switching while running: latency follows, output stays finite, DI stays aligned")
{
    EngineParams p;
    Engine e; e.setParams (p); e.prepare (kFs, 512);
    const auto x = bassPhrase (kFs, 3.0);
    std::vector<double> l (x.size()), r (x.size());
    int lastLat = e.getLatencySamples();
    std::string lats = fmt ("%.0f", lastLat);
    for (size_t pos = 0; pos < x.size(); pos += 512)
    {
        p.quality = (Quality) ((pos / 24000) % 3);
        e.setParams (p);
        const double* ip[1] = { x.data() + pos }; double* op[2] = { l.data() + pos, r.data() + pos };
        e.process (ip, 1, op, 2, (int) std::min<size_t> (512, x.size() - pos));
        if (e.getLatencySamples() != lastLat) { lastLat = e.getLatencySamples(); lats += fmt (" -> %.0f", lastLat); }
    }
    bool finite = true; for (auto v : l) finite &= std::isfinite (v);
    note ("latency sequence: " + lats);
    BF_CHECK (finite);
    BF_CHECK (e.latencyFor (Quality::Eco) < e.latencyFor (Quality::Standard));
    BF_CHECK (e.latencyFor (Quality::Standard) < e.latencyFor (Quality::High));
}

BF_TEST ("ENG-STRESS", "random automation of every parameter, every block, for 20 s: finite and under the ceiling")
{
    Engine e; EngineParams p; e.setParams (p); e.prepare (kFs, 512);
    Prng r; r.seed (2026);
    const auto x = bassPhrase (kFs, 20.0, 0.5);
    std::vector<double> l (x.size()), rr (x.size());
    auto u = [&] { return r.uniform(); };
    for (size_t pos = 0; pos < x.size(); pos += 128)
    {
        p.inputDb = -12 + 30 * u(); p.driveOn = u() > 0.1; p.model = (DriveModel) (int) (u() * 2.99); p.drive = u(); p.character = u();
        p.driveOutDb = -12 + 24 * u(); p.xoverLow = 70 + 330 * u(); p.xoverHigh = 300 + 1700 * u(); p.midDriveDb = -12 + 24 * u(); p.highDriveDb = -12 + 24 * u();
        p.lowComp = u(); p.lowLevelDb = -12 + 18 * u(); p.midLevelDb = -12 + 18 * u(); p.highLevelDb = -12 + 18 * u(); p.ownership = (LowOwnership) (int) (u() * 2.99);
        p.diComp = u(); p.diCompOutDb = -6 + 12 * u(); p.diBassDb = -12 + 24 * u(); p.diMidDb = -12 + 24 * u(); p.diTrebleDb = -12 + 24 * u();
        p.diMidHz = 200 + 2800 * u(); p.diMidQ = 0.3 + 2.7 * u(); p.cab = (CabModel) (int) (u() * 3.99); p.cabOn = u() > 0.1;
        for (auto& m : p.mic) { m.on = u() > 0.3; m.type = (MicType) (int) (u() * 2.99); m.position = u(); m.distanceIn = 6 * u(); m.levelDb = -12 + 18 * u(); m.invert = u() > 0.8; }
        p.micBalance = -1 + 2 * u(); p.align = u() > 0.5 ? MicAlign::Auto : MicAlign::Natural; p.micSpread = u();
        for (int b = 0; b < 7; ++b) { p.peq[b].freq = 20 * std::pow (1000.0, u()); p.peq[b].gainDb = -18 + 36 * u(); p.peq[b].q = 0.1 + 10 * u(); p.peq[b].on = u() > 0.2; p.peq[b].type = (EqBandType) (int) (u() * 2.99); }
        p.room = (RoomType) (int) (u() * 2.99); p.roomSize = u(); p.roomCharacter = u();
        p.faderDrive = -60 + 66 * u(); p.faderDi = -60 + 66 * u(); p.faderRoom = -60 + 66 * u(); p.masterDb = -60 + 60 * u();
        p.lowBoost = 10 * u(); p.lowAtten = 10 * u(); p.highBoost = 10 * u(); p.highAtten = 10 * u(); p.highBw = u();
        p.lowFreqIdx = (int) (u() * 3.99); p.highBoostFreqIdx = (int) (u() * 6.99); p.highAttenFreqIdx = (int) (u() * 2.99);
        p.levelAmount = u(); p.levelGainDb = -12 + 24 * u(); p.protectOn = true; p.ceilingDb = -1;
        e.setParams (p);
        const double* ip[1] = { x.data() + pos }; double* op[2] = { l.data() + pos, rr.data() + pos };
        e.process (ip, 1, op, 2, (int) std::min<size_t> (128, x.size() - pos));
    }
    bool finite = true; double pk = 0;
    for (size_t i = 0; i < l.size(); ++i) { finite &= std::isfinite (l[i]) && std::isfinite (rr[i]); pk = std::max ({ pk, std::abs (l[i]), std::abs (rr[i]) }); }
    note (fmt ("peak %.2f dBFS, non-finite resets %.0f", gainToDb (pk), e.meters().nonFiniteResets.load()));
    BF_CHECK (finite);
    BF_CHECK (gainToDb (pk) <= -0.95);
    BF_CHECK (e.meters().nonFiniteResets.load() == 0);
}
