// BASSFORGE - preset manager: curated factory presets (in code) + user presets (XML files).
// Presets store parameters by stable string ID. Loading a preset never changes Quality (a machine
// setting) and never touches solo (a listening aid).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s);

    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;
    bool load (const juce::String& name);          // factory first, then user
    bool saveUser (const juce::String& name);
    bool deleteUser (const juce::String& name);
    void step (int delta);                          // previous / next across factory + user
    juce::String getCurrentName() const { return current; }
    void setCurrentName (const juce::String& n) { current = n; }
    static juce::File userFolder();

private:
    struct Factory { const char* name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Factory>& factory();
    void resetToDefaults();
    void setParam (const juce::String& id, float plain);

    juce::AudioProcessorValueTreeState& apvts;
    juce::String current { "Default" };
};
