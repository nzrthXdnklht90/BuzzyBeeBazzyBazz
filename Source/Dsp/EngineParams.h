// BASSFORGE - plain parameter snapshot passed from the wrapper to the DSP engine once per block.
// All values are in natural units (dB, Hz, 0..1, enum index). Copyright (c) 2026 Nazareth. Original code.
#pragma once

namespace bassforge
{
enum class Quality { Eco = 0, Standard = 1, High = 2 };          // 2x / 4x / 8x around the nonlinear stages
enum class DriveModel { Drive = 0, Fuzz = 1, Modern = 2 };   // v0.2: Fuzz replaced Vintage
enum class LowOwnership { DiPriority = 0, Hybrid = 1, FullParallel = 2 };
enum class CabModel { Alu410 = 0, Classic810 = 1, Deep215 = 2, Modern410 = 3 };
enum class MicType { Dynamic = 0, Condenser = 1, Ribbon = 2 };
enum class MicAlign { Auto = 0, Natural = 1 };
enum class EqBandType { Bell = 0, Shelf = 1, Cut = 2 };
enum class RoomType { Tight = 0, Live = 1, Large = 2 };

struct MicParams
{
    bool on = true;
    MicType type = MicType::Dynamic;
    double position = 0.25;   // 0 = cap (centre), 1/3 = cap edge, 2/3 = cone, 1 = cone edge (continuous)
    double distanceIn = 1.0;  // inches, 0..6
    double levelDb = 0.0;
    bool invert = false;
};

struct EqBand
{
    bool on = true;
    double freq = 1000.0, gainDb = 0.0, q = 0.9;
    EqBandType type = EqBandType::Bell;  // Shelf / Cut only honoured on bands 1 and 7
};

struct EngineParams
{
    double inputDb = 0.0;
    Quality quality = Quality::Standard;

    // DRIVE
    bool driveOn = true;
    DriveModel model = DriveModel::Drive;
    double drive = 0.4;          // 0..1 macro
    double character = 0.5;      // 0..1 model dependent
    double driveOutDb = 0.0;
    double xoverLow = 140.0, xoverHigh = 850.0;
    double midDriveDb = 0.0, highDriveDb = 0.0;   // offsets relative to the macro
    double lowComp = 0.5;        // 0..1
    double lowLevelDb = 0.0, midLevelDb = 0.0, highLevelDb = -3.0;
    LowOwnership ownership = LowOwnership::DiPriority;
    double fuzzMids = 0.5, fuzzGrit = 0.5, fuzzClip = 1.0;   // FUZZ voice: Scoop, Crunch, Clip (0..1, pedal pot rotation)

    // DI
    bool diCompOn = true, diEqOn = true;
    bool diOpto = false;          // DI compressor style: false = FET, true = optical
    double diComp = 0.35, diCompOutDb = 0.0;
    double diBassDb = 0.0, diMidDb = 0.0, diTrebleDb = 0.0;
    double diBassHz = 100.0, diMidHz = 700.0, diMidQ = 0.7, diTrebleHz = 3000.0;

    // CAB
    bool cabOn = true;
    CabModel cab = CabModel::Classic810;
    MicParams mic[2];
    double micBalance = 0.0;   // -1 = mic 1 only, +1 = mic 2 only
    MicAlign align = MicAlign::Auto;
    double micSpread = 0.0;    // 0..1 stereo spread of the two mics

    // 7-band EQ (drive only)
    bool peqOn = true;
    EqBand peq[7];

    // ROOM
    bool roomOn = true;
    RoomType room = RoomType::Live;
    double roomSize = 0.5, roomCharacter = 0.5;

    // MIXER (fader values in dB; <= -60 dB treated as -inf)
    double faderDrive = 0.0, faderDi = -3.0, faderRoom = -12.0, masterDb = 0.0;
    bool muteDrive = false, muteDi = false, muteRoom = false;
    bool soloDrive = false, soloDi = false, soloRoom = false;   // transient, not stored in presets

    // MIX EQ (program EQ, 0..10 scales like the classic hardware)
    bool mixEqOn = true;
    double lowBoost = 0.0, lowAtten = 0.0; int lowFreqIdx = 1;        // 20/30/60/100 Hz
    double highBoost = 0.0, highBw = 0.5; int highBoostFreqIdx = 3;   // 3k..16k
    double highAtten = 0.0; int highAttenFreqIdx = 1;                // 5k/10k/20k

    // LIMITER (optical leveller + true-peak protection)
    bool levelOn = true;
    double levelAmount = 0.2, levelGainDb = 0.0;
    bool protectOn = true;
    double ceilingDb = -1.0;

    EngineParams()
    {
        const double f[7] = { 50, 120, 300, 650, 1500, 3500, 8000 };
        for (int i = 0; i < 7; ++i) { peq[i].freq = f[i]; peq[i].q = 0.9; }
        peq[0].type = EqBandType::Shelf; peq[6].type = EqBandType::Shelf;
        mic[1].on = false; mic[1].type = MicType::Condenser; mic[1].position = 0.6; mic[1].distanceIn = 4.0;
    }
};
} // namespace bassforge
