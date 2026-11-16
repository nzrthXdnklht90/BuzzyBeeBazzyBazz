#include "Params.h"

namespace bfparams
{
namespace
{
using K = Kind;
std::vector<Def> makeDefs()
{
    std::vector<Def> d = {
        // ---------------------------------------------------------------- global
        { "in_gain", "Input", K::Float, -24, 24, 0, 0, "dB", nullptr, "Input: sets how hard your bass hits BASSFORGE. Aim for the input meter to peak around the middle." },
        { "quality", "Quality", K::Choice, 0, 2, 1, 0, "", "Eco|Standard|High", "Quality: how carefully the distortion is calculated. Eco is lightest on your computer, High is cleanest for final mixes.", false },

        // ---------------------------------------------------------------- DRIVE
        { "drv_on", "Drive On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns the amp/distortion on or off. When off, your clean bass still goes through the cabinet." },
        { "drv_model", "Amp Voice", K::Choice, 0, 2, 0, 0, "", "Vintage|Drive|Modern", "Amp voice: Vintage is warm valve-amp growl, Drive is a flexible overdrive, Modern is tight and aggressive." },
        { "drv_drive", "Drive", K::Float, 0, 10, 4, 0, "", nullptr, "Drive: how much dirt. Low = warm and round, high = gritty and aggressive. The volume stays about the same." },
        { "drv_char", "Character", K::Float, 0, 10, 5, 0, "", nullptr, "Character: shapes the tone of the selected voice. Lower is darker and fuller, higher is brighter with more bite." },
        { "drv_out", "Drive Output", K::Float, -24, 12, 0, 0, "dB", nullptr, "Output: a volume trim for the amp, after the distortion." },
        { "drv_xlow", "Low Split", K::Float, 70, 400, 140, 160, "Hz", nullptr, "Low split: everything below this note range stays clean and solid. Higher = more clean low end." },
        { "drv_xhigh", "High Split", K::Float, 300, 2000, 850, 800, "Hz", nullptr, "High split: where the growl (mid band) hands over to the bite (high band)." },
        { "drv_middrive", "Mid Drive", K::Float, -12, 12, 0, 0, "dB", nullptr, "Mid drive: extra or less distortion for the growl band only." },
        { "drv_highdrive", "High Drive", K::Float, -12, 12, 0, 0, "dB", nullptr, "High drive: extra or less distortion for the bite band only." },
        { "drv_lowcomp", "Low Comp", K::Float, 0, 10, 5, 0, "", nullptr, "Low comp: keeps your clean low end even, so every note has the same weight." },
        { "drv_lowlvl", "Low Level", K::Float, -24, 12, 0, 0, "dB", nullptr, "Low level: volume of the clean low band inside the Drive channel." },
        { "drv_midlvl", "Mid Level", K::Float, -24, 12, 0, 0, "dB", nullptr, "Mid level: volume of the growl band." },
        { "drv_highlvl", "High Level", K::Float, -24, 12, -3, 0, "dB", nullptr, "High level: volume of the bite band. Turn down if the sound gets fizzy." },
        { "drv_lowmode", "Low End", K::Choice, 0, 2, 0, 0, "", "DI Priority|Hybrid|Full Parallel", "Low end: who supplies the deep bass. DI Priority lets the clean DI carry it (tightest). Full Parallel lets both carry it (biggest)." },

        // ---------------------------------------------------------------- DI
        { "di_comp_on", "DI Comp On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns the DI compressor on or off." },
        { "di_comp", "Compression", K::Float, 0, 10, 3.5f, 0, "", nullptr, "Compression: evens out your playing so soft and hard notes sit at a similar level." },
        { "di_comp_out", "DI Comp Output", K::Float, -12, 12, 0, 0, "dB", nullptr, "Output: volume after the compressor." },
        { "di_eq_on", "DI EQ On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns the DI tone controls on or off." },
        { "di_bass", "Bass", K::Float, -12, 12, 0, 0, "dB", nullptr, "Bass: more or less low end on the clean DI." },
        { "di_mid", "Mid", K::Float, -12, 12, 0, 0, "dB", nullptr, "Mid: more or less body on the clean DI." },
        { "di_treble", "Treble", K::Float, -12, 12, 0, 0, "dB", nullptr, "Treble: more or less brightness and finger/pick noise on the clean DI." },
        { "di_bass_f", "Bass Freq", K::Float, 40, 250, 100, 100, "Hz", nullptr, "Bass frequency: where the Bass knob works." },
        { "di_mid_f", "Mid Freq", K::Float, 200, 3000, 700, 700, "Hz", nullptr, "Mid frequency: where the Mid knob works." },
        { "di_mid_q", "Mid Width", K::Float, 0.3f, 3, 0.7f, 0, "", nullptr, "Mid width: low numbers affect a wide range, high numbers a narrow one." },
        { "di_treble_f", "Treble Freq", K::Float, 1000, 8000, 3000, 2800, "Hz", nullptr, "Treble frequency: where the Treble knob starts working." },

        // ---------------------------------------------------------------- CAB
        { "cab_on", "Cab On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns the speaker cabinet on or off. Off sounds like plugging the amp straight into the desk." },
        { "cab_model", "Cabinet", K::Choice, 0, 3, 1, 0, "", "4x10 Bright|8x10 Classic|2x15 Deep|4x10 Modern", "Cabinet: the speaker box. 4x10s are punchy, the 8x10 is big and classic, the 2x15 is deep and soft." },
        { "mic1_on", "Mic 1 On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns microphone 1 on or off." },
        { "mic1_type", "Mic 1 Type", K::Choice, 0, 2, 0, 0, "", "Dynamic|Condenser|Ribbon", "Mic type: Dynamic is punchy with a bright edge, Condenser is clear and detailed, Ribbon is warm and smooth." },
        { "mic1_pos", "Mic 1 Position", K::Float, 0, 1, 0.25f, 0, "pos", nullptr, "Position: centre of the speaker is brightest, the edge is darker and warmer." },
        { "mic1_dist", "Mic 1 Distance", K::Float, 0, 6, 1, 0, "in", nullptr, "Distance: closer gives more bass boost and punch, further is more natural." },
        { "mic1_level", "Mic 1 Level", K::Float, -24, 12, 0, 0, "dB", nullptr, "Level of microphone 1." },
        { "mic1_inv", "Mic 1 Polarity", K::Bool, 0, 1, 0, 0, "", nullptr, "Flips microphone 1 upside down. Try it if two mics together sound thin." },
        { "mic2_on", "Mic 2 On", K::Bool, 0, 1, 0, 0, "", nullptr, "Turns microphone 2 on or off." },
        { "mic2_type", "Mic 2 Type", K::Choice, 0, 2, 1, 0, "", "Dynamic|Condenser|Ribbon", "Mic type: Dynamic is punchy with a bright edge, Condenser is clear and detailed, Ribbon is warm and smooth." },
        { "mic2_pos", "Mic 2 Position", K::Float, 0, 1, 0.6f, 0, "pos", nullptr, "Position: centre of the speaker is brightest, the edge is darker and warmer." },
        { "mic2_dist", "Mic 2 Distance", K::Float, 0, 6, 4, 0, "in", nullptr, "Distance: closer gives more bass boost and punch, further is more natural." },
        { "mic2_level", "Mic 2 Level", K::Float, -24, 12, 0, 0, "dB", nullptr, "Level of microphone 2." },
        { "mic2_inv", "Mic 2 Polarity", K::Bool, 0, 1, 0, 0, "", nullptr, "Flips microphone 2 upside down. Try it if two mics together sound thin." },
        { "mic_bal", "Mic Balance", K::Float, -100, 100, 0, 0, "%", nullptr, "Balance between mic 1 (left) and mic 2 (right)." },
        { "mic_align", "Mic Timing", K::Choice, 0, 1, 0, 0, "", "Auto Align|Natural", "Mic timing: Auto Align lines the two mics up so they never cancel. Natural keeps the real distance delay for a more 3D sound." },
        { "mic_spread", "Mic Spread", K::Float, 0, 100, 0, 0, "%", nullptr, "Spread: pans the two mics apart for a wider cabinet. The deep bass always stays in the centre." },

        // ---------------------------------------------------------------- 7-band EQ (Drive channel)
        { "peq_on", "EQ On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns the Drive EQ on or off. It only affects the Drive channel, never the clean DI." },
    };
    const float f[7] = { 50, 120, 300, 650, 1500, 3500, 8000 };
    static const char* ids[7][5] = {
        { "eq1_on", "eq1_freq", "eq1_gain", "eq1_q", "eq1_type" }, { "eq2_on", "eq2_freq", "eq2_gain", "eq2_q", "" },
        { "eq3_on", "eq3_freq", "eq3_gain", "eq3_q", "" }, { "eq4_on", "eq4_freq", "eq4_gain", "eq4_q", "" },
        { "eq5_on", "eq5_freq", "eq5_gain", "eq5_q", "" }, { "eq6_on", "eq6_freq", "eq6_gain", "eq6_q", "" },
        { "eq7_on", "eq7_freq", "eq7_gain", "eq7_q", "eq7_type" } };
    static const char* names[7][5] = {
        { "EQ 1 On", "EQ 1 Freq", "EQ 1 Gain", "EQ 1 Q", "EQ 1 Type" }, { "EQ 2 On", "EQ 2 Freq", "EQ 2 Gain", "EQ 2 Q", "" },
        { "EQ 3 On", "EQ 3 Freq", "EQ 3 Gain", "EQ 3 Q", "" }, { "EQ 4 On", "EQ 4 Freq", "EQ 4 Gain", "EQ 4 Q", "" },
        { "EQ 5 On", "EQ 5 Freq", "EQ 5 Gain", "EQ 5 Q", "" }, { "EQ 6 On", "EQ 6 Freq", "EQ 6 Gain", "EQ 6 Q", "" },
        { "EQ 7 On", "EQ 7 Freq", "EQ 7 Gain", "EQ 7 Q", "EQ 7 Type" } };
    for (int b = 0; b < 7; ++b)
    {
        d.push_back ({ ids[b][0], names[b][0], K::Bool, 0, 1, 1, 0, "", nullptr, "Turns this EQ band on or off." });
        d.push_back ({ ids[b][1], names[b][1], K::Float, 20, 20000, f[b], 640, "Hz", nullptr, "Frequency: which part of the sound this band changes. Drag the dot left/right on the graph." });
        d.push_back ({ ids[b][2], names[b][2], K::Float, -18, 18, 0, 0, "dB", nullptr, "Gain: boost (up) or cut (down) this part of the sound. Drag the dot up/down on the graph." });
        d.push_back ({ ids[b][3], names[b][3], K::Float, 0.1f, 18, b == 0 || b == 6 ? 0.7f : 0.9f, 1.5f, "", nullptr, "Q: how narrow the band is. Small numbers are broad and gentle, big numbers are narrow and surgical." });
    }
    d.push_back ({ "eq1_type", "EQ 1 Type", K::Choice, 0, 2, 1, 0, "", "Bell|Low Shelf|Low Cut", "Band shape: Bell boosts/cuts around a point, Shelf lifts or lowers everything below, Cut removes everything below." });
    d.push_back ({ "eq7_type", "EQ 7 Type", K::Choice, 0, 2, 1, 0, "", "Bell|High Shelf|High Cut", "Band shape: Bell boosts/cuts around a point, Shelf lifts or lowers everything above, Cut removes everything above." });

    const std::vector<Def> rest = {
        // ---------------------------------------------------------------- ROOM
        { "room_on", "Room On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns the studio room sound on or off." },
        { "room_type", "Room", K::Choice, 0, 2, 1, 0, "", "Tight Studio|Live Room|Large Studio", "Room: the kind of space your cabinet is recorded in." },
        { "room_size", "Size", K::Float, 0, 10, 5, 0, "", nullptr, "Size: how big the room feels and how long the sound hangs in the air." },
        { "room_char", "Room Character", K::Float, 0, 10, 5, 0, "", nullptr, "Character: soft, dark walls (low) or hard, bright walls (high)." },

        // ---------------------------------------------------------------- MIXER
        { "fader_drive", "Drive Fader", K::Float, -60, 6, 0, 0, "dB", nullptr, "Drive fader: how loud the amp + cabinet sound is in the final mix." },
        { "fader_di", "DI Fader", K::Float, -60, 6, -3, 0, "dB", nullptr, "DI fader: how loud the clean bass is in the final mix." },
        { "fader_room", "Room Fader", K::Float, -60, 6, -12, 0, "dB", nullptr, "Room fader: how much room sound you hear. It follows the Drive fader automatically." },
        { "mute_drive", "Drive Mute", K::Bool, 0, 1, 0, 0, "", nullptr, "Mute: silences the Drive channel (the Room keeps its own button)." },
        { "mute_di", "DI Mute", K::Bool, 0, 1, 0, 0, "", nullptr, "Mute: silences the clean DI channel." },
        { "mute_room", "Room Mute", K::Bool, 0, 1, 0, 0, "", nullptr, "Mute: silences the room sound." },
        { "master", "Master", K::Float, -60, 0, 0, 0, "dB", nullptr, "Master: the final volume. It can only turn down, so it can never cause clipping." },

        // ---------------------------------------------------------------- MIX EQ
        { "mixeq_on", "Mix EQ On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns the final tone shaper on or off." },
        { "mx_lowboost", "Low Boost", K::Float, 0, 10, 0, 0, "", nullptr, "Low boost: adds a big, smooth low-end lift at the chosen frequency." },
        { "mx_lowatten", "Low Cut", K::Float, 0, 10, 0, 0, "", nullptr, "Low cut: trims the low end. Use it together with Low Boost to tighten muddy bass while keeping weight." },
        { "mx_lowfreq", "Low Freq", K::Choice, 0, 3, 1, 0, "", "20 Hz|30 Hz|60 Hz|100 Hz", "Which low frequency Low Boost and Low Cut work around." },
        { "mx_highboost", "High Boost", K::Float, 0, 10, 0, 0, "", nullptr, "High boost: adds presence and air at the chosen frequency." },
        { "mx_highbw", "High Width", K::Float, 0, 10, 5, 0, "", nullptr, "Width of the high boost: low = focused, high = broad and smooth." },
        { "mx_hbfreq", "Boost Freq", K::Choice, 0, 6, 3, 0, "", "3k|4k|5k|8k|10k|12k|16k", "Which high frequency the High Boost lifts." },
        { "mx_highatten", "High Cut", K::Float, 0, 10, 0, 0, "", nullptr, "High cut: gently softens the top end." },
        { "mx_hafreq", "Cut Freq", K::Choice, 0, 2, 1, 0, "", "5k|10k|20k", "Where the gentle High Cut starts." },

        // ---------------------------------------------------------------- LIMITER
        { "lvl_on", "Leveller On", K::Bool, 0, 1, 1, 0, "", nullptr, "Turns the smooth levelling compressor on or off." },
        { "lvl_amount", "Amount", K::Float, 0, 10, 2, 0, "", nullptr, "Amount: how much the leveller smooths out volume changes. Smooth and slow, great for bass." },
        { "lvl_gain", "Leveller Gain", K::Float, -12, 12, 0, 0, "dB", nullptr, "Gain: volume after the leveller." },
        { "tp_on", "Peak Protect", K::Bool, 0, 1, 1, 0, "", nullptr, "Peak protection: stops the output ever going over the ceiling, even between samples." },
        { "tp_ceiling", "Ceiling", K::Float, -6, 0, -1, 0, "dB", nullptr, "Ceiling: the loudest the output is allowed to be. -1 dB is a safe choice for streaming." },
    };
    d.insert (d.end(), rest.begin(), rest.end());
    return d;
}
} // namespace

const std::vector<Def>& all()
{
    static const std::vector<Def> defs = makeDefs();
    return defs;
}

const Def* find (const juce::String& id)
{
    for (auto& d : all()) if (id == d.id) return &d;
    return nullptr;
}

juce::String format (const Def& d, float v, bool withUnit)
{
    const juce::String unit (d.unit);
    if (d.kind == Kind::Bool) return v > 0.5f ? "On" : "Off";
    if (d.kind == Kind::Choice) return juce::StringArray::fromTokens (d.choices, "|", "")[(int) std::round (v)];
    if (unit == "pos")
    {
        static const char* names[] = { "Cap", "Cap Edge", "Cone", "Cone Edge" };
        const float s = v * 3.0f;
        const int k = (int) std::round (s);
        if (std::abs (s - k) < 0.04f) return names[k];
        return juce::String (v * 100.0f, 0) + "%";
    }
    juce::String s;
    if (unit == "Hz")
        s = v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + (withUnit ? " kHz" : "k") : juce::String (v, 0) + (withUnit ? " Hz" : "");
    else if (unit == "dB")
        s = (v <= d.minV && d.minV <= -60.0f) ? juce::String ("-inf") + (withUnit ? " dB" : "")
                                               : (v > 0.05f ? "+" : "") + juce::String (v, 1) + (withUnit ? " dB" : "");
    else if (unit == "in")
        s = juce::String (v, 1) + (withUnit ? " in" : "");
    else if (unit == "%")
        s = juce::String (v, 0) + (withUnit ? "%" : "");
    else
        s = juce::String (v, (d.maxV - d.minV) <= 4.0f ? 2 : 1);
    return s;
}

float parseTyped (const Def& d, const juce::String& textIn)
{
    auto text = textIn.trim().toLowerCase();
    if (d.kind == Kind::Bool)
        return (text == "on" || text == "1" || text == "true" || text == "yes") ? 1.0f : 0.0f;
    if (d.kind == Kind::Choice)
    {
        const auto items = juce::StringArray::fromTokens (d.choices, "|", "");
        for (int i = 0; i < items.size(); ++i) if (items[i].toLowerCase().startsWith (text)) return (float) i;
        return juce::jlimit (d.minV, d.maxV, (float) text.getIntValue());
    }
    if (juce::String (d.unit) == "pos")
    {
        if (text.contains ("cone edge")) return 1.0f;
        if (text.contains ("cap edge")) return 1.0f / 3.0f;
        if (text.contains ("cone")) return 2.0f / 3.0f;
        if (text.contains ("cap")) return 0.0f;
    }
    if (text.startsWith ("-inf")) return d.minV;
    double mult = 1.0;
    if (text.contains ("khz") || text.endsWithChar ('k')) mult = 1000.0;
    const juce::String num = text.retainCharacters ("0123456789.-+");
    double v = num.getDoubleValue() * mult;
    if (juce::String (d.unit) == "pos" && text.containsChar ('%')) v /= 100.0;
    return juce::jlimit (d.minV, d.maxV, (float) v);  // out-of-range entries clamp (standing rule)
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& d : all())
    {
        const juce::ParameterID pid { d.id, 1 };
        auto attrs = juce::AudioParameterFloatAttributes()
                         .withStringFromValueFunction ([&d] (float v, int) { return format (d, v); })
                         .withValueFromStringFunction ([&d] (const juce::String& t) { return parseTyped (d, t); })
                         .withAutomatable (d.automatable);
        switch (d.kind)
        {
            case Kind::Float:
            {
                juce::NormalisableRange<float> r (d.minV, d.maxV);
                if (d.centre > 0.0f) r.setSkewForCentre (d.centre);
                layout.add (std::make_unique<juce::AudioParameterFloat> (pid, d.name, r, d.defV, attrs));
                break;
            }
            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, d.name, d.defV > 0.5f,
                    juce::AudioParameterBoolAttributes().withAutomatable (d.automatable)));
                break;
            case Kind::Choice:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, d.name, juce::StringArray::fromTokens (d.choices, "|", ""),
                    (int) d.defV, juce::AudioParameterChoiceAttributes().withAutomatable (d.automatable)));
                break;
        }
    }
    return layout;
}

Bridge::Bridge (juce::AudioProcessorValueTreeState& s)
{
    for (auto& d : all())
    {
        auto* raw = s.getRawParameterValue (d.id);
        jassert (raw != nullptr);
        ptr[d.id] = raw;
    }
}

float Bridge::get (const char* id) const noexcept { return p (id)->load (std::memory_order_relaxed); }

void Bridge::fill (bassforge::EngineParams& e) const noexcept
{
    using namespace bassforge;
    auto f = [this] (const char* id) { return (double) get (id); };
    auto b = [this] (const char* id) { return get (id) > 0.5f; };
    auto i = [this] (const char* id) { return (int) std::lround (get (id)); };
    e.inputDb = f ("in_gain");
    e.quality = (Quality) i ("quality");
    e.driveOn = b ("drv_on"); e.model = (DriveModel) i ("drv_model");
    e.drive = f ("drv_drive") / 10.0; e.character = f ("drv_char") / 10.0; e.driveOutDb = f ("drv_out");
    e.xoverLow = f ("drv_xlow"); e.xoverHigh = f ("drv_xhigh"); e.midDriveDb = f ("drv_middrive"); e.highDriveDb = f ("drv_highdrive");
    e.lowComp = f ("drv_lowcomp") / 10.0; e.lowLevelDb = f ("drv_lowlvl"); e.midLevelDb = f ("drv_midlvl"); e.highLevelDb = f ("drv_highlvl");
    e.ownership = (LowOwnership) i ("drv_lowmode");
    e.diCompOn = b ("di_comp_on"); e.diComp = f ("di_comp") / 10.0; e.diCompOutDb = f ("di_comp_out");
    e.diEqOn = b ("di_eq_on"); e.diBassDb = f ("di_bass"); e.diMidDb = f ("di_mid"); e.diTrebleDb = f ("di_treble");
    e.diBassHz = f ("di_bass_f"); e.diMidHz = f ("di_mid_f"); e.diMidQ = f ("di_mid_q"); e.diTrebleHz = f ("di_treble_f");
    e.cabOn = b ("cab_on"); e.cab = (CabModel) i ("cab_model");
    const char* micIds[2][6] = { { "mic1_on", "mic1_type", "mic1_pos", "mic1_dist", "mic1_level", "mic1_inv" },
                                 { "mic2_on", "mic2_type", "mic2_pos", "mic2_dist", "mic2_level", "mic2_inv" } };
    for (int m = 0; m < 2; ++m)
    {
        e.mic[m].on = b (micIds[m][0]); e.mic[m].type = (MicType) i (micIds[m][1]); e.mic[m].position = f (micIds[m][2]);
        e.mic[m].distanceIn = f (micIds[m][3]); e.mic[m].levelDb = f (micIds[m][4]); e.mic[m].invert = b (micIds[m][5]);
    }
    e.micBalance = f ("mic_bal") / 100.0; e.align = (MicAlign) i ("mic_align"); e.micSpread = f ("mic_spread") / 100.0;
    e.peqOn = b ("peq_on");
    static const char* eqIds[7][4] = { { "eq1_on", "eq1_freq", "eq1_gain", "eq1_q" }, { "eq2_on", "eq2_freq", "eq2_gain", "eq2_q" },
                                       { "eq3_on", "eq3_freq", "eq3_gain", "eq3_q" }, { "eq4_on", "eq4_freq", "eq4_gain", "eq4_q" },
                                       { "eq5_on", "eq5_freq", "eq5_gain", "eq5_q" }, { "eq6_on", "eq6_freq", "eq6_gain", "eq6_q" },
                                       { "eq7_on", "eq7_freq", "eq7_gain", "eq7_q" } };
    for (int k = 0; k < 7; ++k)
    {
        e.peq[k].on = b (eqIds[k][0]); e.peq[k].freq = f (eqIds[k][1]); e.peq[k].gainDb = f (eqIds[k][2]); e.peq[k].q = f (eqIds[k][3]);
        e.peq[k].type = EqBandType::Bell;
    }
    e.peq[0].type = (EqBandType) i ("eq1_type");
    e.peq[6].type = (EqBandType) i ("eq7_type");
    e.roomOn = b ("room_on"); e.room = (RoomType) i ("room_type"); e.roomSize = f ("room_size") / 10.0; e.roomCharacter = f ("room_char") / 10.0;
    e.faderDrive = f ("fader_drive"); e.faderDi = f ("fader_di"); e.faderRoom = f ("fader_room"); e.masterDb = f ("master");
    e.muteDrive = b ("mute_drive"); e.muteDi = b ("mute_di"); e.muteRoom = b ("mute_room");
    e.mixEqOn = b ("mixeq_on"); e.lowBoost = f ("mx_lowboost"); e.lowAtten = f ("mx_lowatten"); e.lowFreqIdx = i ("mx_lowfreq");
    e.highBoost = f ("mx_highboost"); e.highBw = f ("mx_highbw") / 10.0; e.highBoostFreqIdx = i ("mx_hbfreq");
    e.highAtten = f ("mx_highatten"); e.highAttenFreqIdx = i ("mx_hafreq");
    e.levelOn = b ("lvl_on"); e.levelAmount = f ("lvl_amount") / 10.0; e.levelGainDb = f ("lvl_gain");
    e.protectOn = b ("tp_on"); e.ceilingDb = f ("tp_ceiling");
}
} // namespace bfparams
