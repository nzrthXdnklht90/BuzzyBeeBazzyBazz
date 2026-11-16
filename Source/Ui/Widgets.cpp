#include "Widgets.h"

namespace bfui
{
juce::String helpFor (const char* id)
{
    if (auto* d = bfparams::find (id)) return d->help;
    return {};
}

static juce::RangedAudioParameter& paramRef (juce::AudioProcessorValueTreeState& s, const char* id)
{
    auto* p = s.getParameter (id);
    jassert (p != nullptr);
    return *p;
}

// ============================================================================ ValueLabel
ValueLabel::ValueLabel (juce::AudioProcessorValueTreeState& s, const char* id)
    : param (paramRef (s, id)), def (*bfparams::find (id)),
      attachment (param, [this] (float) { refresh(); }, nullptr)
{
    setEditable (false, true, false);   // double-click to type (standing rule)
    setJustificationType (juce::Justification::centred);
    setFont (bfui::font (12.0f));
    setColour (juce::Label::textColourId, col::text);
    setTooltip (juce::String (def.help) + "  (Double-click the number to type a value.)");
    setWantsKeyboardFocus (false);
    attachment.sendInitialUpdate();
}

void ValueLabel::refresh()
{
    const float v = param.convertFrom0to1 (param.getValue());
    setText (formatter ? formatter (v) : bfparams::format (def, v), juce::dontSendNotification);
}

juce::TextEditor* ValueLabel::createEditorComponent()
{
    auto* e = juce::Label::createEditorComponent();
    e->setJustification (juce::Justification::centred);
    e->setFont (bfui::font (12.5f));
    return e;
}

void ValueLabel::editorShown (juce::TextEditor* e)
{
    e->setText (bfparams::format (def, param.convertFrom0to1 (param.getValue()), false), false);
    e->selectAll();
}

void ValueLabel::textWasEdited()
{
    const float v = bfparams::parseTyped (def, getText());   // clamps out-of-range entries
    attachment.setValueAsCompleteGesture (v);
    refresh();
}

// ============================================================================ Knob
Knob::Knob (juce::AudioProcessorValueTreeState& s, const char* id, juce::Colour accent, Size sz, const juce::String& cap)
    : value (s, id), size (sz), attach (s, id, slider)
{
    const auto* d = bfparams::find (id);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setDoubleClickReturnValue (true, d->defV);
    slider.setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier); // Shift = fine
    slider.setMouseDragSensitivity (sz == Size::Large ? 260 : 200);
    slider.setTooltip (d->help);
    slider.setWantsKeyboardFocus (true);
    slider.setTitle (d->name);
    caption.setText (cap.isNotEmpty() ? cap : juce::String (d->name).toUpperCase(), juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setFont (caps (sz == Size::Small ? 9.5f : 10.5f));
    caption.setColour (juce::Label::textColourId, col::textDim);
    caption.setTooltip (d->help);
    caption.setInterceptsMouseClicks (false, false);
    value.setFont (font (sz == Size::Small ? 11.0f : 12.5f));
    addAndMakeVisible (caption);
    addAndMakeVisible (slider);
    addAndMakeVisible (value);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (size == Size::Inline)   // caption | knob | value in one row (top bar)
    {
        caption.setJustificationType (juce::Justification::centredRight);
        value.setJustificationType (juce::Justification::centredLeft);
        caption.setBounds (r.removeFromLeft (r.getWidth() * 30 / 100));
        value.setBounds (r.removeFromRight (r.getWidth() * 45 / 100));
        slider.setBounds (r.withSizeKeepingCentre (std::min (r.getWidth(), r.getHeight()), std::min (r.getWidth(), r.getHeight())));
        return;
    }
    const int capH = size == Size::Small ? 13 : 15, valH = size == Size::Small ? 15 : 18;
    caption.setBounds (r.removeFromTop (capH));
    value.setBounds (r.removeFromBottom (valH));
    slider.setBounds (r);
}

// ============================================================================ PowerButton
PowerButton::PowerButton (juce::AudioProcessorValueTreeState& s, const char* id, juce::Colour a)
    : accent (a), attach (s, id, *this)
{
    setTooltip (helpFor (id));
    setTitle (bfparams::find (id)->name);
    setWantsKeyboardFocus (true);
}

void PowerButton::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float d = std::min (r.getWidth(), r.getHeight());
    const auto c = r.withSizeKeepingCentre (d, d);
    const bool on = getToggleState();
    g.setColour (on ? accent : col::track);
    g.fillEllipse (c);
    if (over) { g.setColour (juce::Colours::white.withAlpha (0.15f)); g.fillEllipse (c); }
    g.setColour (on ? juce::Colours::white : col::textDim);
    g.strokePath (powerIcon (c.reduced (d * 0.18f)), juce::PathStrokeType (std::max (1.4f, d * 0.09f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (hasKeyboardFocus (false)) { g.setColour (accent.withAlpha (0.6f)); g.drawEllipse (c.expanded (1.5f), 1.5f); }
}

// ============================================================================ TextToggle
TextToggle::TextToggle (juce::AudioProcessorValueTreeState& s, const char* id, const juce::String& text, juce::Colour on)
    : juce::ToggleButton (text), onColour (on)
{
    attach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, *this);
    setTooltip (helpFor (id));
    setWantsKeyboardFocus (true);
}

TextToggle::TextToggle (const juce::String& text, juce::Colour on, const juce::String& help)
    : juce::ToggleButton (text), onColour (on)
{
    setTooltip (help);
    setWantsKeyboardFocus (true);
}

void TextToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    g.setColour (on ? onColour : (over ? col::track.darker (0.03f) : col::track));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? juce::Colours::white : col::text);
    g.setFont (caps (std::min (11.0f, r.getHeight() * 0.6f)));
    g.drawText (getButtonText(), r, juce::Justification::centred);
    if (hasKeyboardFocus (false)) { g.setColour (onColour.withAlpha (0.6f)); g.drawRoundedRectangle (r, 4.0f, 1.5f); }
}

// ============================================================================ Segmented
Segmented::Segmented (juce::AudioProcessorValueTreeState& s, const char* id, juce::Colour a, juce::StringArray l)
    : param (paramRef (s, id)), labels (std::move (l)), accent (a),
      attachment (param, [this] (float v) { index = (int) std::lround (v); repaint(); }, nullptr)
{
    if (labels.isEmpty())
        labels = juce::StringArray::fromTokens (bfparams::find (id)->choices, "|", "");
    setTooltip (helpFor (id));
    setTitle (bfparams::find (id)->name);
    setWantsKeyboardFocus (true);
    attachment.sendInitialUpdate();
}

int Segmented::indexAt (juce::Point<int> p) const
{
    const int n = labels.size();
    return juce::jlimit (0, n - 1, p.x * n / std::max (1, getWidth()));
}

void Segmented::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (col::track);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    const int n = labels.size();
    const float w = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (2.0f);
        if (i == index)
        {
            juce::DropShadow (juce::Colours::black.withAlpha (0.12f), 4, { 0, 1 }).drawForRectangle (g, cell.toNearestInt());
            g.setColour (juce::Colours::white);
            g.fillRoundedRectangle (cell, cell.getHeight() * 0.5f);
        }
        else if (i == hover)
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.fillRoundedRectangle (cell, cell.getHeight() * 0.5f);
        }
        g.setColour (i == index ? accent.darker (0.25f) : col::textDim);
        g.setFont (caps (fontPx));
        g.drawFittedText (labels[i], cell.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
    if (hasKeyboardFocus (false)) { g.setColour (accent.withAlpha (0.6f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f); }
}

void Segmented::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked()) return;
    attachment.setValueAsCompleteGesture ((float) indexAt (e.getPosition()));
}

void Segmented::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt (e.getPosition());
    if (h != hover) { hover = h; repaint(); }
}

bool Segmented::keyPressed (const juce::KeyPress& k)
{
    const int n = labels.size();
    if (k == juce::KeyPress::leftKey) { attachment.setValueAsCompleteGesture ((float) juce::jmax (0, index - 1)); return true; }
    if (k == juce::KeyPress::rightKey) { attachment.setValueAsCompleteGesture ((float) juce::jmin (n - 1, index + 1)); return true; }
    return false;
}

// ============================================================================ ChoiceBox
ChoiceBox::ChoiceBox (juce::AudioProcessorValueTreeState& s, const char* id)
    : attach ((addItemList (juce::StringArray::fromTokens (bfparams::find (id)->choices, "|", ""), 1), s), id, *this)
{
    setTooltip (helpFor (id));
    setTitle (bfparams::find (id)->name);
}

// ============================================================================ Meter
void Meter::push (float a, float b)
{
    const float in[2] { a, b };
    for (int c = 0; c < nch; ++c)
    {
        level[c] = std::max (in[c], level[c] * 0.82f);
        if (in[c] >= hold[c]) { hold[c] = in[c]; holdTicks[c] = 45; }
        else if (--holdTicks[c] <= 0) hold[c] = std::max (0.0f, hold[c] * 0.9f);
        if (in[c] >= clipThreshold) clipped = true;
    }
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float gap = 2.0f;
    const float bw = (r.getWidth() - gap * (nch - 1)) / nch;
    auto dbToY = [&] (float lin) {
        const float db = juce::Decibels::gainToDecibels (lin, -60.0f);
        return r.getBottom() - (db + 60.0f) / 66.0f * r.getHeight();   // -60 .. +6 dB
    };
    for (int c = 0; c < nch; ++c)
    {
        auto bar = juce::Rectangle<float> (r.getX() + c * (bw + gap), r.getY(), bw, r.getHeight());
        g.setColour (col::track);
        g.fillRoundedRectangle (bar, 2.0f);
        const float y = dbToY (level[c]);
        const float y0 = dbToY (1.0f);
        auto fill = bar.withTop (y);
        g.setGradientFill (juce::ColourGradient (colour, 0, bar.getBottom(), col::meterHi, 0, dbToY (juce::Decibels::decibelsToGain (-6.0f)), false));
        g.fillRoundedRectangle (fill, 2.0f);
        if (level[c] > 1.0f) { g.setColour (col::clip); g.fillRect (fill.withBottom (y0)); }
        g.setColour (col::text.withAlpha (0.6f));
        if (hold[c] > 0.001f) g.fillRect (bar.getX(), dbToY (hold[c]) - 1.0f, bar.getWidth(), 1.5f);
    }
    // 0 dB tick
    g.setColour (col::textDim.withAlpha (0.5f));
    g.drawHorizontalLine ((int) dbToY (1.0f), r.getX(), r.getRight());
}

void GrMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::track);
    g.fillRoundedRectangle (r, 2.0f);
    const float w = r.getWidth() * std::min (1.0f, value / range);
    g.setColour (colour);
    g.fillRoundedRectangle (r.withLeft (r.getRight() - w), 2.0f);
    g.setColour (col::textDim);
    g.setFont (font (9.5f));
    g.drawText ("GR " + juce::String (-value, 1) + " dB", r.reduced (4, 0), juce::Justification::centredLeft);
}

// ============================================================================ ChannelStrip
ChannelStrip::ChannelStrip (juce::AudioProcessorValueTreeState& s, const juce::String& n, const char* faderId, const char* muteId,
                            juce::Colour a, std::atomic<bool>* soloAtomic, const juce::String& soloHelp)
    : meter (1, a.interpolatedWith (col::meterLo, 0.4f)), value (s, faderId), name (n), accent (a), soloFlag (soloAtomic), attach (s, faderId, fader)
{
    const auto* d = bfparams::find (faderId);
    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setColour (juce::Slider::trackColourId, a);
    fader.setDoubleClickReturnValue (true, d->defV);
    fader.setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    fader.setTooltip (d->help);
    fader.setTitle (d->name);
    fader.setWantsKeyboardFocus (true);
    fader.setSkewFactorFromMidPoint (-12.0);
    addAndMakeVisible (fader);
    addAndMakeVisible (meter);
    addAndMakeVisible (value);
    if (muteId != nullptr)
    {
        mute = std::make_unique<TextToggle> (s, muteId, "M", col::master);
        addAndMakeVisible (*mute);
    }
    if (soloFlag != nullptr)
    {
        solo = std::make_unique<TextToggle> ("S", col::meterHi.darker (0.15f), soloHelp);
        solo->setToggleState (soloFlag->load(), juce::dontSendNotification);
        solo->onClick = [this] { soloFlag->store (solo->getToggleState()); };
        addAndMakeVisible (*solo);
    }
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (4, 0);
    r.removeFromTop (22);   // name
    auto bottom = r.removeFromBottom (mute || solo ? 46 : 22);
    value.setBounds (bottom.removeFromTop (20));
    if (mute || solo)
    {
        auto row = bottom.reduced (2, 2);
        const int w = row.getWidth() / 2;
        if (mute) mute->setBounds (row.removeFromLeft (w).reduced (2, 0));
        if (solo) solo->setBounds (row.reduced (2, 0));
    }
    auto meterR = r.removeFromRight (8);
    meter.setBounds (meterR.reduced (0, 6));
    fader.setBounds (r);
}

void ChannelStrip::paint (juce::Graphics& g)
{
    auto top = getLocalBounds().removeFromTop (20).toFloat();
    g.setColour (accent);
    g.fillRoundedRectangle (top.withSizeKeepingCentre (top.getWidth() - 10, 3).withY (top.getBottom() - 3), 1.5f);
    g.setColour (col::text);
    g.setFont (caps (11.0f));
    g.drawText (name, top.withTrimmedBottom (4), juce::Justification::centred);
}

void ChannelStrip::tick (float peak)
{
    meter.push (peak);
    if (solo && soloFlag && solo->getToggleState() != soloFlag->load())
        solo->setToggleState (soloFlag->load(), juce::dontSendNotification);
}
} // namespace bfui
