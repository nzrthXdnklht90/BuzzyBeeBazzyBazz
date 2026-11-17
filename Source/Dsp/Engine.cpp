#include "Engine.h"

namespace bassforge
{
namespace
{
inline double faderGain (double db) noexcept { return db <= -60.0 ? 0.0 : dbToGain (db); }

void specsFor (Quality q, std::vector<HalfbandSpec>& hb, DecimatorSpec& dec, int& stages)
{
    switch (q)
    {
        case Quality::Eco:
            stages = 1; hb = { { 0.205, 70.0 } }; dec = { 0.41, 0.5, 70.0 }; break;
        case Quality::Standard:
            stages = 2; hb = { { 0.21, 90.0 }, { 0.115, 90.0 } }; dec = { 0.42, 0.5, 90.0 }; break;
        case Quality::High:
        default:
            stages = 3; hb = { { 0.215, 100.0 }, { 0.115, 100.0 }, { 0.06, 100.0 } }; dec = { 0.43, 0.5, 100.0 }; break;
    }
}
} // namespace

void Engine::prepare (double sampleRate, int maxBlockIn)
{
    fs = sampleRate;
    maxBlock = std::max (1, maxBlockIn);

    // oversamplers for all three qualities are prepared now so switching never allocates
    int maxLat = 0;
    for (int q = 0; q < 3; ++q)
    {
        std::vector<HalfbandSpec> hb; DecimatorSpec dec; int stages = 0;
        specsFor ((Quality) q, hb, dec, stages);
        osMid[q].prepare (stages, hb, dec, kChunk);
        osHigh[q].prepare (stages, hb, dec, kChunk);
        const int f = osMid[q].getFactor();
        const int rt = osMid[q].getRoundTripLatencyTop();
        osPadTop[q] = (f - rt % f) % f;
        osLatency[q] = (rt + osPadTop[q]) / f;
        maxLat = std::max (maxLat, osLatency[q]);
    }
    padMid.prepare (16); padHigh.prepare (16);
    lowDelay.prepare ((size_t) maxLat + 1);
    cleanDelay.prepare ((size_t) maxLat + 1);
    diDelay.prepare ((size_t) maxLat + 1);

    const double osRateStd = fs * 4.0;
    midBank.prepare (osRateStd, kChunk * 8);
    highBank.prepare (osRateStd, kChunk * 8);
    midBank.calibrate (BandRole::Mid);      // offline loudness tables (prepare-time only)
    highBank.calibrate (BandRole::High);

    lowComp.prepare (fs);
    fet.prepare (fs);
    diOptoComp.prepare (fs);
    diEq.prepare (fs);
    cab.prepare (fs);
    peq.prepare (fs);
    room.prepare (fs);
    mixEq.prepare (fs);
    opto.prepare (fs);
    guard.prepare (fs, 2);
    guard.setLink (1.0);

    for (auto* r : { &gIn, &gDriveOut, &gLow, &gMid, &gHigh, &gOwn, &gFDrive, &gFDi, &gFRoom, &gAudDrive, &gAudDi, &gAudRoom, &gMaster, &gLevelOut, &gDiOut })
        r->setRampLength ((int) (0.02 * fs));
    const int fadeLen = (int) (0.012 * fs);
    for (auto* f : { &fDrive, &fCab, &fPeq, &fFet, &fDiEq, &fRoom, &fMixEq, &fLevel, &fProtect, &fQuality, &fRoomType, &fFuzz, &fDiStyle })
        f->setLength (fadeLen);
    sXLow.setTime (25.0, fs / kChunk);
    sXHigh.setTime (25.0, fs / kChunk);

    const auto sz = (size_t) kChunk;
    for (auto* b : { &inMono, &outL, &outR, &bLow, &bMid, &bHigh, &bUpper, &bCabL, &bCabR, &bDi, &bSend, &bRoomL, &bRoomR, &bTmp })
        b->assign (sz, 0.0);
    bOsMid.assign (sz * 8, 0.0);
    bOsHigh.assign (sz * 8, 0.0);

    activeQuality = params.quality;
    qualityTarget = activeQuality;
    const double osRate = fs * osMid[(int) activeQuality].getFactor();
    midBank.setRate (osRate);
    highBank.setRate (osRate);
    firstControl = true;
    reset();
}

void Engine::reset() noexcept
{
    xLow.reset(); xHigh.reset(); ap.reset(); diAp1.reset(); diAp2.reset();
    lowComp.reset();
    for (int q = 0; q < 3; ++q) { osMid[q].reset(); osHigh[q].reset(); }
    padMid.reset(); padHigh.reset();
    midBank.reset(); highBank.reset();
    lowDelay.reset(); cleanDelay.reset(); diDelay.reset();
    fet.reset(); diOptoComp.reset(); fuzzShelf.reset(); diEq.reset(); cab.reset(); peq.reset(); room.reset(); mixEq.reset(); opto.reset(); guard.reset();
    firstControl = true;
}

void Engine::switchQuality (Quality q) noexcept
{
    activeQuality = q;
    const double osRate = fs * osMid[(int) q].getFactor();
    // voice filters are recomputed for the new rate (no allocation: scratch was sized for 8x)
    midBank.setRate (osRate);
    highBank.setRate (osRate);
    osMid[(int) q].reset(); osHigh[(int) q].reset();
    padMid.reset(); padHigh.reset();
    lowDelay.reset(); cleanDelay.reset(); diDelay.reset();
}

void Engine::updateControls() noexcept
{
    const auto& p = params;
    const bool snap = firstControl;

    // ---- quality change: fade the whole output out, switch, fade back in (latency changes here)
    if (snap && p.quality != activeQuality)
    {
        switchQuality (p.quality);   // nothing playing yet: switch immediately
        qualityPending = false;
    }
    if (p.quality != activeQuality && ! qualityPending)
    {
        qualityPending = true;
        qualityTarget = p.quality;
        fQuality.setOn (false);
    }
    if (qualityPending && fQuality.isFullyOff())
    {
        switchQuality (qualityTarget);
        qualityPending = false;
        fQuality.setOn (true);
    }

    // ---- crossover (smoothed in log-frequency, upper corner kept >= 1.5x lower)
    const double lfLo = std::log (std::clamp (p.xoverLow, 70.0, 400.0));
    const double lfHi = std::log (std::clamp (p.xoverHigh, 300.0, 2000.0));
    if (snap) { sXLow.snap (lfLo); sXHigh.snap (lfHi); }
    const double fLo = std::exp (sXLow.next (lfLo));
    const double fHi = std::max (std::exp (sXHigh.next (lfHi)), fLo * 1.5);
    xLow.setFreq (fLo, fs);
    xHigh.setFreq (fHi, fs);
    apC.makeAllpass (SvfCoeffs::prewarp (fHi, fs));
    apLoC.makeAllpass (SvfCoeffs::prewarp (fLo, fs));

    // ---- drive voices
    VoiceSetup vs;
    vs.drive = std::clamp (p.drive, 0.0, 1.0);
    vs.character = std::clamp (p.character, 0.0, 1.0);
    vs.fuzzMids = p.fuzzMids; vs.fuzzGrit = p.fuzzGrit; vs.fuzzClip = p.fuzzClip;
    vs.role = BandRole::Mid; vs.offsetDb = p.midDriveDb;
    midBank.setup (p.model, vs);
    vs.role = BandRole::High; vs.offsetDb = p.highDriveDb;
    highBank.setup (p.model, vs);
    lowComp.setAmount (p.lowComp);
    fFuzz.setOn (p.model == DriveModel::Fuzz);
    fuzzHighPre = dbToGain (p.highDriveDb);   // Fuzz mode: High Drive = how much of the high band feeds the fuzz
    fuzzShelfC.makeHighShelf (SvfCoeffs::prewarp (fHi, fs), fFuzz.value() * p.highLevelDb, SvfCoeffs::kButterworth);

    // ---- low-frequency ownership: how much of the fundamental the Drive low band keeps.
    // DI Priority -12 dB, Hybrid -6 dB, Full Parallel 0 dB, scaled by how present the DI really is
    // (muted DI or a DI fader below about -36 dB gives the low band back automatically).
    const double maxCut = p.ownership == LowOwnership::DiPriority ? -12.0 : (p.ownership == LowOwnership::Hybrid ? -6.0 : 0.0);
    const double diPresence = p.muteDi ? 0.0 : std::clamp ((p.faderDi + 36.0) / 30.0, 0.0, 1.0);
    gOwn.setTarget (dbToGain (maxCut * diPresence));

    gIn.setTarget (dbToGain (p.inputDb));
    gDriveOut.setTarget (dbToGain (p.driveOutDb));
    gLow.setTarget (dbToGain (p.lowLevelDb));
    gMid.setTarget (dbToGain (p.midLevelDb));
    gHigh.setTarget (dbToGain (p.highLevelDb));
    gDiOut.setTarget (dbToGain (p.diCompOutDb));

    // ---- mixer: faders, mute and solo (solo never changes stored settings)
    const bool soloAny = p.soloDrive || p.soloDi || p.soloRoom;
    const auto audible = [soloAny] (bool mute, bool solo) { return ! mute && (! soloAny || solo); };
    gFDrive.setTarget (faderGain (p.faderDrive));
    gFDi.setTarget (faderGain (p.faderDi));
    gFRoom.setTarget (faderGain (p.faderRoom));
    gAudDrive.setTarget (audible (p.muteDrive, p.soloDrive) ? 1.0 : 0.0);
    gAudDi.setTarget (audible (p.muteDi, p.soloDi) ? 1.0 : 0.0);
    gAudRoom.setTarget (audible (p.muteRoom, p.soloRoom) ? 1.0 : 0.0);
    gMaster.setTarget (dbToGain (std::min (p.masterDb, 0.0)));   // attenuation only (brief 4)
    gLevelOut.setTarget (dbToGain (p.levelGainDb));

    // ---- block on/off
    fDrive.setOn (p.driveOn); fCab.setOn (p.cabOn); fPeq.setOn (p.peqOn);
    fFet.setOn (p.diCompOn); fDiEq.setOn (p.diEqOn); fRoom.setOn (p.roomOn);
    fMixEq.setOn (p.mixEqOn); fLevel.setOn (p.levelOn); fProtect.setOn (p.protectOn);

    // ---- room type: fade the return out, apply, fade back in
    if (p.room != roomTypeApplied)
    {
        fRoomType.setOn (false);
        if (fRoomType.isFullyOff()) { roomTypeApplied = p.room; fRoomType.setOn (true); }
    }
    EngineParams rp = p;
    rp.room = roomTypeApplied;

    fet.setAmount (p.diComp);
    diOptoComp.setAmount (p.diComp);
    // matched to the FET style on a bass phrase: same average gain reduction (+11 dB detector gain)
    // and the same output level (extra make-up) at every Compression setting (docs/study_electric_toad.md)
    diOptoComp.sideGainDb = 11.0;
    diOptoComp.extraMakeupDb = 5.5 * std::pow (std::clamp (p.diComp, 0.0, 1.0), 1.5);
    fDiStyle.setOn (p.diOpto);
    diEq.setParams (p);
    cab.setParams (p);
    peq.setParams (p.peq);
    room.setParams (rp);
    mixEq.setParams (p);
    opto.setAmount (p.levelAmount);
    guard.setCeilingDb (std::clamp (p.ceilingDb, -12.0, 0.0));

    if (snap)
    {
        for (auto* r : { &gIn, &gDriveOut, &gLow, &gMid, &gHigh, &gOwn, &gFDrive, &gFDi, &gFRoom, &gAudDrive, &gAudDi, &gAudRoom, &gMaster, &gLevelOut, &gDiOut })
            r->snapTo (r->getTarget());
        fFuzz.snap (p.model == DriveModel::Fuzz); fDiStyle.snap (p.diOpto);
        fDrive.snap (p.driveOn); fCab.snap (p.cabOn); fPeq.snap (p.peqOn); fFet.snap (p.diCompOn); fDiEq.snap (p.diEqOn);
        fRoom.snap (p.roomOn); fMixEq.snap (p.mixEqOn); fLevel.snap (p.levelOn); fProtect.snap (p.protectOn);
        if (! qualityPending) fQuality.snap (true);
        fRoomType.snap (true);
        roomTypeApplied = p.room;
        firstControl = false;
    }
}

void Engine::processChunk (int n) noexcept
{
    updateControls();
    const int q = (int) activeQuality;
    const int factor = osMid[q].getFactor();
    const int lat = osLatency[q];
    const bool driveActive = ! fDrive.isFullyOff();

    // ================================================================ input + crossover
    for (int i = 0; i < n; ++i)
    {
        const double x = inMono[(size_t) i] * gIn.next();
        inMono[(size_t) i] = x;
        pIn.push (x);
        double lo, rest, mid, hi;
        xLow.process (x, lo, rest);
        xHigh.process (rest, mid, hi);
        bLow[(size_t) i] = ap.process (lo, apC);   // phase-match the low band to the second split
        bMid[(size_t) i] = mid;
        bHigh[(size_t) i] = hi;
        // clean, latency-aligned reference for the Drive bypass (crossover sum = allpass of input)
        bTmp[(size_t) i] = cleanDelay.process (mid + hi, (size_t) lat);
    }

    // ================================================================ mid / high nonlinear bands
    const bool fuzzAny = ! fFuzz.isFullyOff();
    const bool highNeeded = ! fFuzz.isFullyOn();
    if (fuzzAny)
        for (int i = 0; i < n; ++i)
        {
            const double w = fFuzz.next();
            bMid[(size_t) i] += w * fuzzHighPre * bHigh[(size_t) i];
            bHigh[(size_t) i] *= 1.0 - w;
        }
    if (driveActive)
    {
        const int nOs = n * factor;
        osMid[q].upsample (bMid.data(), bOsMid.data(), n);
        midBank.process (bOsMid.data(), nOs);
        if (osPadTop[q] > 0) for (int i = 0; i < nOs; ++i) bOsMid[(size_t) i] = padMid.process (bOsMid[(size_t) i], (size_t) osPadTop[q]);
        osMid[q].downsample (bOsMid.data(), bMid.data(), n);

        if (highNeeded)
        {
            osHigh[q].upsample (bHigh.data(), bOsHigh.data(), n);
            highBank.process (bOsHigh.data(), nOs);
            if (osPadTop[q] > 0) for (int i = 0; i < nOs; ++i) bOsHigh[(size_t) i] = padHigh.process (bOsHigh[(size_t) i], (size_t) osPadTop[q]);
            osHigh[q].downsample (bOsHigh.data(), bHigh.data(), n);
        }
        else
        {
            std::fill (bHigh.begin(), bHigh.begin() + n, 0.0);   // Fuzz mode: the high band is inside the fuzz
            highBank.reset();
            osHigh[q].reset(); padHigh.reset();
        }
        if (fuzzAny)
            for (int i = 0; i < n; ++i) bMid[(size_t) i] = fuzzShelf.process (bMid[(size_t) i], fuzzShelfC);
        else
            fuzzShelf.reset();
    }

    for (int i = 0; i < n; ++i)
    {
        const double w = fDrive.next();
        // low band: compressor + level when the Drive block is on, clean when off; ownership always
        const double loIn = bLow[(size_t) i];
        const double loProc = driveActive ? lowComp.process (loIn) * gLow.next() : loIn;
        double lo = w * loProc + (1.0 - w) * loIn;
        lo = lowDelay.process (lo, (size_t) lat) * gOwn.next();
        double up = bTmp[(size_t) i];
        if (driveActive)
        {
            const double proc = bMid[(size_t) i] * gMid.next() + bHigh[(size_t) i] * gHigh.next();
            up = w * proc + (1.0 - w) * up;
        }
        const double trim = gDriveOut.next();
        bUpper[(size_t) i] = up * trim;
        bLow[(size_t) i] = lo * trim;
    }
    if (fDrive.isFullyOff()) { lowComp.reset(); midBank.reset(); highBank.reset(); }
    grLow = lowComp.getGrDb();

    // ================================================================ cabinet (upper bands only) + low band
    if (! fCab.isFullyOff())
        cab.process (bUpper.data(), bCabL.data(), bCabR.data(), n);
    else
        cab.reset();
    for (int i = 0; i < n; ++i)
    {
        const double w = fCab.next();
        const double u = bUpper[(size_t) i];
        const double l = w * bCabL[(size_t) i] + (1.0 - w) * u + bLow[(size_t) i];
        const double r = w * bCabR[(size_t) i] + (1.0 - w) * u + bLow[(size_t) i];
        bCabL[(size_t) i] = l;
        bCabR[(size_t) i] = r;
    }

    // ================================================================ 7-band EQ (Drive only)
    if (! fPeq.isFullyOff())
    {
        std::copy (bCabL.begin(), bCabL.begin() + n, bRoomL.begin());   // dry copies for the bypass fade
        std::copy (bCabR.begin(), bCabR.begin() + n, bRoomR.begin());
        peq.process (bCabL.data(), bCabR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const double w = fPeq.next();
            bCabL[(size_t) i] = w * bCabL[(size_t) i] + (1.0 - w) * bRoomL[(size_t) i];
            bCabR[(size_t) i] = w * bCabR[(size_t) i] + (1.0 - w) * bRoomR[(size_t) i];
        }
    }
    else
        peq.reset();

    // ================================================================ DRIVE fader, room send, DI path
    for (int i = 0; i < n; ++i)
    {
        const double f = gFDrive.next();
        const double l = bCabL[(size_t) i] * f, r = bCabR[(size_t) i] * f;
        bSend[(size_t) i] = 0.5 * (l + r);               // post-fader, pre-mute (so Room solo works)
        const double a = gAudDrive.next();
        bCabL[(size_t) i] = l * a;
        bCabR[(size_t) i] = r * a;
        pDrive.push (std::max (std::abs (bCabL[(size_t) i]), std::abs (bCabR[(size_t) i])));

        // DI: aligned to the Drive path latency, then compressor + EQ (each with its own bypass)
        // the Drive low band carries the crossover's allpass phase; the DI gets the same phase so the
        // two fundamentals add coherently (no partial cancellation around the crossover points)
        double d = diAp2.process (diAp1.process (diDelay.process (inMono[(size_t) i], (size_t) lat), apLoC), apC);
        const double wf = fFet.next();
        const double ws = fDiStyle.next();   // 0 = FET, 1 = Opto
        if (wf > 0.0)
        {
            double c = 0.0;
            if (ws < 1.0) c += (1.0 - ws) * fet.process (d);
            if (ws > 0.0) { double l = d, r = d; diOptoComp.processFrame (l, r); c += ws * l; }
            d = wf * c * gDiOut.next() + (1.0 - wf) * d;
        }
        else gDiOut.next();
        const double we = fDiEq.next();
        if (we > 0.0) d = we * diEq.process (d) + (1.0 - we) * d;
        d *= gFDi.next() * gAudDi.next();
        bDi[(size_t) i] = d;
        pDi.push (d);
    }
    if (fFet.isFullyOff()) { fet.reset(); diOptoComp.reset(); }
    else if (fDiStyle.isFullyOn()) fet.reset();
    else if (fDiStyle.isFullyOff()) diOptoComp.reset();
    if (fDiEq.isFullyOff()) diEq.reset();
    grDi = fDiStyle.value() > 0.5 ? diOptoComp.getGrDb() : fet.getGrDb();

    // ================================================================ ROOM return
    const bool roomActive = ! fRoom.isFullyOff();
    if (roomActive)
        room.process (bSend.data(), bRoomL.data(), bRoomR.data(), n);
    else
        room.reset();

    // ================================================================ bus
    for (int i = 0; i < n; ++i)
    {
        double rl = 0.0, rr = 0.0;
        const double wr = fRoom.next() * fRoomType.next();
        const double rg = gFRoom.next() * gAudRoom.next() * wr;
        if (roomActive) { rl = bRoomL[(size_t) i] * rg; rr = bRoomR[(size_t) i] * rg; }
        pRoom.push (std::max (std::abs (rl), std::abs (rr)));
        outL[(size_t) i] = bCabL[(size_t) i] + bDi[(size_t) i] + rl;
        outR[(size_t) i] = bCabR[(size_t) i] + bDi[(size_t) i] + rr;
    }

    // Mix EQ
    if (! fMixEq.isFullyOff())
    {
        std::copy (outL.begin(), outL.begin() + n, bRoomL.begin());
        std::copy (outR.begin(), outR.begin() + n, bRoomR.begin());
        mixEq.process (outL.data(), outR.data(), n);
        for (int i = 0; i < n; ++i)
        {
            const double w = fMixEq.next();
            outL[(size_t) i] = w * outL[(size_t) i] + (1.0 - w) * bRoomL[(size_t) i];
            outR[(size_t) i] = w * outR[(size_t) i] + (1.0 - w) * bRoomR[(size_t) i];
        }
    }
    else
        mixEq.reset();

    // optical leveller -> master fader -> true-peak protection
    double frame[2], aligned[2];
    for (int i = 0; i < n; ++i)
    {
        double l = outL[(size_t) i], r = outR[(size_t) i];
        const double wl = fLevel.next();
        if (wl > 0.0)
        {
            double pl = l, pr = r;
            opto.processFrame (pl, pr);
            const double og = gLevelOut.next();
            l = wl * pl * og + (1.0 - wl) * l;
            r = wl * pr * og + (1.0 - wl) * r;
        }
        else
            gLevelOut.next();
        const double m = gMaster.next() * fQuality.next();
        frame[0] = l * m; frame[1] = r * m;
        guard.processFrame (frame, aligned);   // always runs: latency stays fixed when protection is off
        const double wp = fProtect.next();
        l = wp * frame[0] + (1.0 - wp) * aligned[0];
        r = wp * frame[1] + (1.0 - wp) * aligned[1];
        if (! std::isfinite (l) || ! std::isfinite (r))
        {
            l = r = 0.0;
            meterValues.nonFiniteResets.fetch_add (1);
            reset();
        }
        outL[(size_t) i] = l;
        outR[(size_t) i] = r;
        pOutL.push (l); pOutR.push (r);
        if (std::abs (l) >= 1.0 || std::abs (r) >= 1.0) ++clipCount;
    }
    if (fLevel.isFullyOff()) opto.reset();
    grLevel = opto.getGrDb();
}

template void Engine::process<float> (const float* const*, int, float* const*, int, int) noexcept;
template void Engine::process<double> (const double* const*, int, double* const*, int, int) noexcept;
} // namespace bassforge
