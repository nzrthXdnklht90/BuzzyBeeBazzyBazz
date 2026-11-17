// BASSFORGE - complete processing graph (framework independent).
// Copyright (c) 2026 Nazareth. Original code.
//
//  in (mono/stereo -> mono) -> input gain ─┬─► DRIVE: LR4 3-way ─► low: comp, level, ownership ───────────────┐
//                                          │          (allpass-   ─► mid:  OS up ─► voice ─► OS down ─► level │
//                                          │           compensated)─► high: OS up ─► voice ─► OS down ─► level│
//                                          │                         upper = mid + high ─► CAB (2 mics) ─► + low
//                                          │                         ─► 7-band EQ ─► DRIVE fader ─┬─► mute/solo ─► bus
//                                          │                                                     └─► ROOM send (post-fader, pre-mute)
//                                          │                                                          ─► HP ─► room ─► ROOM fader ─► bus
//                                          └─► delay (= Drive latency) ─► FET comp ─► 3-band EQ ─► DI fader ─► bus
//  bus (stereo) ─► Mix EQ ─► optical leveller ─► MASTER fader ─► true-peak protection ─► out
//
// Latency is fixed per (quality, sample rate): oversampler round trip (padded to whole base samples)
// + the protection stage's look-ahead, which is reserved even when protection is off.
#pragma once

#include "EngineParams.h"
#include "Filters.h"
#include "DriveVoices.h"
#include "Dynamics.h"
#include "Cabinet.h"
#include "Eq.h"
#include "Room.h"
#include "HalfbandResampler.h"
#include "TruePeakGuard.h"
#include <atomic>

namespace bassforge
{
/** Meter values published once per block (UI thread reads). Peaks are linear magnitude. */
struct Meters
{
    std::atomic<float> input { 0 }, drive { 0 }, di { 0 }, room { 0 }, outL { 0 }, outR { 0 };
    std::atomic<float> lowCompGr { 0 }, diCompGr { 0 }, levelGr { 0 }, protectGr { 0 };
    std::atomic<int> clips { 0 };           // count of output samples at/over full scale since start
    std::atomic<int> nonFiniteResets { 0 };
};

class Engine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate, int maxBlock);
    void reset() noexcept;
    void setParams (const EngineParams& p) noexcept { params = p; }

    /** Process one host block. numIn/numOut: 1 or 2. T = float or double. */
    template <typename T>
    void process (const T* const* in, int numIn, T* const* out, int numOut, int n) noexcept;

    int getLatencySamples() const noexcept { return latencyFor (activeQuality); }
    int latencyFor (Quality q) const noexcept { return osLatency[(int) q] + guard.getLatency(); }
    Quality getActiveQuality() const noexcept { return activeQuality; }
    double getSampleRate() const noexcept { return fs; }
    double tailSeconds() const noexcept { return Room::kTailSeconds; }
    Meters& meters() noexcept { return meterValues; }

    /** For tests: compensation (dB) the voice bank applies. */
    double voiceCompDb (BandRole role, DriveModel m, double drive, double ch) const noexcept
    { return (role == BandRole::Mid ? midBank : highBank).compensationDb (m, drive, ch); }

private:
    void processChunk (int n) noexcept;
    void updateControls() noexcept;
    void switchQuality (Quality q) noexcept;

    double fs = 48000.0;
    int maxBlock = 512;
    EngineParams params;

    // drive
    LR4 xLow, xHigh;
    SvfCoeffs apC, apLoC; Svf ap, diAp1, diAp2;   // diAp*: DI phase-matched to the crossover (LR4 bands are in phase with AP_lo * AP_hi)
    LowBandCompressor lowComp;
    Fade fFuzz;                // FUZZ routing: one fuzz across mid + high (like a real pedal), low band stays clean
    SvfCoeffs fuzzShelfC; Svf fuzzShelf; double fuzzHighPre = 1.0;   // in Fuzz mode High Level = post-fuzz shelf above the High Split
    OversamplerChannel osMid[3], osHigh[3];
    int osLatency[3] {}, osPadTop[3] {};
    DelayLine padMid, padHigh;
    DriveVoiceBank midBank, highBank;
    Quality activeQuality = Quality::Standard;
    DelayLine lowDelay, cleanDelay, diDelay;
    Smoother sXLow, sXHigh;

    // other blocks
    FetCompressor fet;
    OptoLeveller diOptoComp;   // DI compressor 'Opto' style (feedback optical cell)
    Fade fDiStyle;
    DiEq diEq;
    Cabinet cab;
    ParametricEq7 peq;
    Room room;
    ProgramEq mixEq;
    OptoLeveller opto;
    TruePeakGuard guard;

    // ramps / fades
    LinearRamp gIn, gDriveOut, gLow, gMid, gHigh, gOwn, gFDrive, gFDi, gFRoom, gAudDrive, gAudDi, gAudRoom, gMaster, gLevelOut, gDiOut;
    Fade fDrive, fCab, fPeq, fFet, fDiEq, fRoom, fMixEq, fLevel, fProtect, fQuality, fRoomType;
    RoomType roomTypeApplied = RoomType::Live;
    bool qualityPending = false;
    Quality qualityTarget = Quality::Standard;

    // buffers (allocated in prepare)
    std::vector<double> inMono, outL, outR;
    std::vector<double> bLow, bMid, bHigh, bUpper, bOsMid, bOsHigh, bCabL, bCabR, bDi, bSend, bRoomL, bRoomR, bTmp;

    PeakAcc pIn, pDrive, pDi, pRoom, pOutL, pOutR;
    double grLow = 0, grDi = 0, grLevel = 0;
    int clipCount = 0;
    Meters meterValues;
    bool firstControl = true;
};

// ---------------------------------------------------------------- template implementation
template <typename T>
void Engine::process (const T* const* in, int numIn, T* const* out, int numOut, int n) noexcept
{
    int done = 0;
    while (done < n)
    {
        const int len = std::min (kChunk, n - done);
        for (int i = 0; i < len; ++i)
        {
            double x = numIn > 1 ? 0.5 * ((double) in[0][done + i] + (double) in[1][done + i]) : (double) in[0][done + i];
            inMono[(size_t) i] = std::isfinite (x) ? x : 0.0;
        }
        processChunk (len);
        for (int i = 0; i < len; ++i)
        {
            if (numOut > 1)
            {
                out[0][done + i] = (T) outL[(size_t) i];
                out[1][done + i] = (T) outR[(size_t) i];
            }
            else
                out[0][done + i] = (T) (0.5 * (outL[(size_t) i] + outR[(size_t) i]));
        }
        done += len;
    }
    meterValues.input.store ((float) pIn.take());
    meterValues.drive.store ((float) pDrive.take());
    meterValues.di.store ((float) pDi.take());
    meterValues.room.store ((float) pRoom.take());
    meterValues.outL.store ((float) pOutL.take());
    meterValues.outR.store ((float) pOutR.take());
    meterValues.lowCompGr.store ((float) grLow);
    meterValues.diCompGr.store ((float) grDi);
    meterValues.levelGr.store ((float) grLevel);
    meterValues.protectGr.store ((float) guard.takeGainReductionDb());
    meterValues.clips.store (clipCount);
}
} // namespace bassforge
