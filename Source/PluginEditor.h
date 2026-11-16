// BASSFORGE - editor: top bar, two pages (TONE / CAB & SPACE) and the persistent, collapsible mixer.
// Fixed 1200 x 720 logical layout, scaled as a whole (smooth resize, high-DPI safe).
#pragma once

#include "PluginProcessor.h"
#include "Ui/Pages.h"

class BassForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BassForgeEditor (BassForgeProcessor&);
    ~BassForgeEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kW = 1200, kH = 720;

private:
    class Root : public juce::Component
    {
    public:
        explicit Root (BassForgeEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        BassForgeEditor& ed;
    };

    void timerCallback() override;
    void setPage (int p);
    void layoutRoot();
    void showPresetMenu();
    void savePresetDialog();

    BassForgeProcessor& proc;
    bfui::LookAndFeel lnf;
    Root root { *this };
    juce::TextButton tabTone { "TONE" }, tabCab { "CAB & SPACE" };
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, presetName, presetSave { "SAVE" };
    bfui::Knob input;
    bfui::Meter inputMeter { 1, bfui::col::meterLo };
    bfui::ChoiceBox quality;
    juce::Label qualityCaption;
    std::unique_ptr<bfui::TonePage> tone;
    std::unique_ptr<bfui::CabSpacePage> cabPage;
    std::unique_ptr<bfui::MixerPanel> mixer;
    juce::TooltipWindow tooltips { this, 3000 };   // plain-language help after a 3 s rest (standing rule)
    std::unique_ptr<juce::AlertWindow> saveWindow;
    int tickCount = 0;
};
