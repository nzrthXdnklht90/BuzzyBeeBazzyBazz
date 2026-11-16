// BASSFORGE UI - the two sound-design pages and the persistent mixer.
#pragma once

#include "Widgets.h"

class BassForgeProcessor;

namespace bfui
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Rounded section with a coloured title, optional power switch and an "ADVANCED" toggle. */
class Module : public juce::Component
{
public:
    Module (const juce::String& title, juce::Colour accent) : title (title), accent (accent) {}
    void paint (juce::Graphics&) override;
    /** Content area inside the header and padding. */
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (14).withTrimmedTop (34); }
    juce::Rectangle<int> header() const { return getLocalBounds().reduced (14, 10).removeFromTop (26); }
    juce::String title, subtitle;
    juce::Colour accent;
    int titleIndent = 0;   // space reserved at the left of the title for a power button
};

class AdvancedToggle : public juce::TextButton
{
public:
    AdvancedToggle() : juce::TextButton ("ADVANCED") { setClickingTogglesState (true); setTooltip ("Show or hide the detailed controls for this section."); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// ============================================================================ TONE page
class TonePage : public juce::Component
{
public:
    TonePage (BassForgeProcessor&, juce::ValueTree ui);
    void resized() override;
    void tick();

private:
    BassForgeProcessor& proc;
    APVTS& s;
    juce::ValueTree ui;
    Module drive { "DRIVE", col::drive }, di { "DI", col::di };
    PowerButton drvOn;
    Segmented model;
    AdvancedToggle drvAdv, diAdv;
    Knob drvDrive, drvChar, drvOut;
    Knob xLow, xHigh, midDrive, highDrive, lowComp, lowLvl, midLvl, highLvl;
    Segmented lowMode;
    juce::Label lowModeCaption, charHint;
    PowerButton compOn, eqOn;
    Knob comp, compOut, bass, mid, treble, bassF, midF, midQ, trebF;
    GrMeter compGr { col::di, 20.0f }, lowGr { col::drive, 12.0f };
    juce::Label compTitle, eqTitle;
    int lastModel = -1;
};

// ============================================================================ CAB & SPACE page
class CabView;
class MicGrid;
class EqGraph;

class CabSpacePage : public juce::Component, private juce::Timer
{
public:
    CabSpacePage (BassForgeProcessor&, juce::ValueTree ui);
    ~CabSpacePage() override;
    void resized() override;
    void tick();
    void showGrid (bool show, bool pin);
    bool isGridPinned() const noexcept { return gridPinned; }

private:
    void timerCallback() override;
    BassForgeProcessor& proc;
    APVTS& s;
    juce::ValueTree ui;
    Module cab { "CABINET", col::drive }, eq { "DRIVE EQ", col::drive }, room { "ROOM", col::room };
    PowerButton cabOn, eqOn, roomOn;
    Segmented cabModel;
    std::unique_ptr<CabView> cabView;
    std::unique_ptr<MicGrid> grid;
    bool gridPinned = false;
    // mic rows
    PowerButton mic1On, mic2On;
    Segmented mic1Type, mic2Type;
    juce::Label mic1Name, mic2Name, posCap, distCap, lvlCap;
    ValueLabel mic1Pos, mic1Dist, mic2Pos, mic2Dist;
    Knob mic1Level, mic2Level;
    TextToggle mic1Inv, mic2Inv;
    Knob micBal, micSpread;
    Segmented micAlign;
    juce::Label alignCaption;
    // EQ
    std::unique_ptr<EqGraph> eqGraph;
    // room
    Segmented roomType;
    Knob roomSize, roomChar;
};

// ============================================================================ mixer
class MixerPanel : public juce::Component
{
public:
    MixerPanel (BassForgeProcessor&, juce::ValueTree ui);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick();
    bool isExpanded() const;
    std::function<void()> onToggle;
    static constexpr int kExpandedW = 404, kCollapsedW = 52;

private:
    BassForgeProcessor& proc;
    APVTS& s;
    juce::ValueTree ui;
    juce::TextButton toggle;
    ChannelStrip driveCh, diCh, roomCh, masterCh;
    Meter masterMeter { 2, col::meterLo }, miniMeter { 2, col::meterLo };
    juce::Label outReadout;
    Module mixEq { "MIX EQ", col::master }, limiter { "LIMITER", col::master };
    PowerButton mixEqOn, lvlOn;
    Knob lowBoost, lowAtten, highBoost, highAtten, highBw;
    ChoiceBox lowFreq, hbFreq, haFreq;
    Knob lvlAmount, lvlGain, ceiling;
    TextToggle protect;
    GrMeter lvlGr { col::master, 12.0f }, tpGr { col::clip, 6.0f };
    float outPeakDb = -100.0f;
    int outHold = 0;
    juce::Rectangle<int> stripsArea;
};
} // namespace bfui
