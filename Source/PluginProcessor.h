// BASSFORGE - JUCE audio processor (host integration, parameters, state, presets).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/Engine.h"
#include "Params.h"
#include "Presets.h"

class BassForgeProcessor : public juce::AudioProcessor
{
public:
    BassForgeProcessor();
    ~BassForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    bool supportsDoublePrecisionProcessing() const override { return true; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BASSFORGE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return bassforge::Room::kTailSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    bassforge::Meters& meters() noexcept { return engine.meters(); }
    double getEngineSampleRate() const noexcept { return engine.getSampleRate(); }

    /** Solo is a listening aid: never stored in presets or the session (brief 13.4). */
    std::atomic<bool> solo[3] { false, false, false };   // drive, di, room

    PresetManager presets;
    /** UI-only state (page, mixer expanded, selected EQ band, advanced panels) saved with the session. */
    juce::ValueTree uiState { "UI" };

private:
    template <typename T> void processT (juce::AudioBuffer<T>&);
    template <typename T> void bypassT (juce::AudioBuffer<T>&);

    bassforge::Engine engine;
    bfparams::Bridge bridge;
    bassforge::EngineParams snapshot;
    // host bypass: input delayed by the reported latency (keeps the session time-aligned)
    std::vector<double> bypassLine[2];
    int bypassPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassForgeProcessor)
};
