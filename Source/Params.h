// BASSFORGE - parameter registry (single source of truth for IDs, ranges, defaults, units and the
// plain-language hover help). Stable string IDs are the state contract: never rename or reuse one.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Dsp/EngineParams.h"

namespace bfparams
{
enum class Kind { Float, Bool, Choice };

struct Def
{
    const char* id;
    const char* name;
    Kind kind;
    float minV, maxV, defV;
    float centre;          // skew centre for frequency-like ranges (0 = linear)
    const char* unit;      // "dB", "Hz", "in", "%", "" ...
    const char* choices;   // "A|B|C" for Choice
    const char* help;      // hover help (3 s popup), plain language for beginners
    bool automatable = true;
};

const std::vector<Def>& all();
const Def* find (const juce::String& id);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Parse typed text ("75", "75%", "-3 dB", "24k", "1.2 kHz") into a plain value, clamped to range. */
float parseTyped (const Def& d, const juce::String& text);
juce::String format (const Def& d, float v, bool withUnit = true);

/** Caches raw parameter pointers and builds the engine snapshot once per block. */
class Bridge
{
public:
    explicit Bridge (juce::AudioProcessorValueTreeState& s);
    void fill (bassforge::EngineParams& p) const noexcept;
    float get (const char* id) const noexcept;

private:
    std::unordered_map<std::string, std::atomic<float>*> ptr;
    std::atomic<float>* p (const char* id) const noexcept { return ptr.at (id); }
};
} // namespace bfparams
