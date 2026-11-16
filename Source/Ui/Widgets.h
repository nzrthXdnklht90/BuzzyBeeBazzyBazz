// BASSFORGE UI - reusable controls. Every control:
//  - is bound to a host parameter (automation is shown live, edits are host gestures),
//  - shows plain-language help after the pointer rests on it for 3 s (standing rule),
//  - has a value readout you can double-click to type an exact value; out-of-range entries clamp,
//    units are optional ("75", "-3 dB", "1.2k") (standing rule),
//  - resets to default on double-click of the control itself, supports Shift for fine adjustment,
//    and works with the keyboard (focus ring, arrow keys).
#pragma once

#include "Theme.h"
#include "../Params.h"

namespace bfui
{
juce::String helpFor (const char* id);

/** Value readout with typed entry. */
class ValueLabel : public juce::Label
{
public:
    ValueLabel (juce::AudioProcessorValueTreeState& s, const char* paramId);
    std::function<juce::String (float)> formatter;   // optional override of the registry format
    void refresh();
    void textWasEdited() override;
    juce::TextEditor* createEditorComponent() override;
    void editorShown (juce::TextEditor*) override;

private:
    juce::RangedAudioParameter& param;
    const bfparams::Def& def;
    juce::ParameterAttachment attachment;
};

/** Rotary knob + caption + editable value. */
class Knob : public juce::Component
{
public:
    enum class Size { Large, Medium, Small, Inline };
    Knob (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::Colour accent, Size size = Size::Medium, const juce::String& caption = {});
    void resized() override;
    void setAccent (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); repaint(); }
    juce::Slider slider;
    juce::Label caption;
    ValueLabel value;
    Size size;

private:
    juce::AudioProcessorValueTreeState::SliderAttachment attach;
};

/** Circular power switch for a processing block. */
class PowerButton : public juce::ToggleButton
{
public:
    PowerButton (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::Colour accent);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Colour accent;
    juce::AudioProcessorValueTreeState::ButtonAttachment attach;
};

/** Small rounded text toggle (Mute, Polarity...) bound to a bool parameter. */
class TextToggle : public juce::ToggleButton
{
public:
    TextToggle (juce::AudioProcessorValueTreeState& s, const char* paramId, const juce::String& text, juce::Colour onColour);
    /** Unbound variant (e.g. Solo, which is never stored). */
    TextToggle (const juce::String& text, juce::Colour onColour, const juce::String& help);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Colour onColour;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach;
};

/** Segmented selector bound to a choice parameter (e.g. VINTAGE | DRIVE | MODERN). */
class Segmented : public juce::Component, public juce::SettableTooltipClient
{
public:
    Segmented (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::Colour accent, juce::StringArray labels = {});
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    bool keyPressed (const juce::KeyPress&) override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }
    int getIndex() const noexcept { return index; }
    float fontPx = 12.0f;

private:
    int indexAt (juce::Point<int>) const;
    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    juce::Colour accent;
    int index = 0, hover = -1;
    juce::ParameterAttachment attachment;
};

/** Small drop-down for long choice lists (Mix EQ frequencies). */
class ChoiceBox : public juce::ComboBox
{
public:
    ChoiceBox (juce::AudioProcessorValueTreeState& s, const char* paramId);

private:
    juce::AudioProcessorValueTreeState::ComboBoxAttachment attach;
};

/** Peak meter with decay and 1.5 s peak hold. Vertical; one or two bars. */
class Meter : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Meter (int channels = 1, juce::Colour c = col::meterLo) : nch (channels), colour (c) {}
    void push (float a, float b = 0.0f);   // linear peaks, call from the UI timer
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { clipped = false; hold[0] = hold[1] = 0; repaint(); }
    bool clipped = false;
    float clipThreshold = 1.0f;

private:
    int nch;
    juce::Colour colour;
    float level[2] {}, hold[2] {};
    int holdTicks[2] {};
};

/** Horizontal gain-reduction meter (dB, grows from the right). */
class GrMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit GrMeter (juce::Colour c, float rangeDb = 20.0f) : colour (c), range (rangeDb) {}
    void push (float grDb) { const float v = std::max (0.0f, -grDb); value = std::max (v, value * 0.85f); repaint(); }
    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
    float range, value = 0;
};

/** Mixer channel: name, meter + fader, editable value, mute + solo. */
class ChannelStrip : public juce::Component
{
public:
    ChannelStrip (juce::AudioProcessorValueTreeState& s, const juce::String& name, const char* faderId, const char* muteId,
                  juce::Colour accent, std::atomic<bool>* solo, const juce::String& soloHelp);
    void resized() override;
    void paint (juce::Graphics&) override;
    void tick (float peak);
    juce::Slider fader;
    Meter meter;
    ValueLabel value;
    std::unique_ptr<TextToggle> mute, solo;

private:
    juce::String name;
    juce::Colour accent;
    std::atomic<bool>* soloFlag;
    juce::AudioProcessorValueTreeState::SliderAttachment attach;
};
} // namespace bfui
