// BASSFORGE UI - palette (brief 16.1), fonts and the custom LookAndFeel. Original vector artwork only.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bfui
{
namespace col
{
const juce::Colour bg       { 0xffF3EEE4 };   // warm cream
const juce::Colour panel    { 0xffFBF8F2 };   // module surface
const juce::Colour panelEdge{ 0xffE3DCCF };
const juce::Colour text     { 0xff2B2A28 };   // graphite
const juce::Colour textDim  { 0xff7C766C };
const juce::Colour neutral  { 0xffB8B2A7 };   // soft grey controls
const juce::Colour track    { 0xffE6E0D5 };
const juce::Colour drive    { 0xffE07A3F };   // soft orange
const juce::Colour di       { 0xff5B7FA6 };   // muted blue
const juce::Colour room     { 0xff6E9A72 };   // muted green
const juce::Colour master   { 0xff4A4743 };
const juce::Colour clip     { 0xffD0473A };
const juce::Colour meterLo  { 0xff8DB596 };
const juce::Colour meterHi  { 0xffE0B04F };
} // namespace col

inline juce::Font font (float px, bool bold = false)
{
    return juce::Font (juce::FontOptions (px, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::Font caps (float px = 11.0f)
{
    return juce::Font (juce::FontOptions (px, juce::Font::bold)).withExtraKerningFactor (0.10f);
}

/** Rounded module surface with a subtle drop shadow. */
inline void drawModule (juce::Graphics& g, juce::Rectangle<float> r, float radius = 12.0f)
{
    juce::DropShadow (juce::Colours::black.withAlpha (0.07f), 10, { 0, 3 }).drawForRectangle (g, r.toNearestInt());
    g.setColour (col::panel);
    g.fillRoundedRectangle (r, radius);
    g.setColour (col::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

inline juce::Path powerIcon (juce::Rectangle<float> r)
{
    juce::Path p;
    const auto c = r.getCentre();
    const float rad = r.getWidth() * 0.34f;
    p.addCentredArc (c.x, c.y, rad, rad, 0.0f, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
    p.startNewSubPath (c.x, c.y - rad - 1.5f);
    p.lineTo (c.x, c.y - 0.5f);
    return p;
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::TooltipWindow::backgroundColourId, col::text);
        setColour (juce::TooltipWindow::textColourId, col::panel);
        setColour (juce::TooltipWindow::outlineColourId, col::text);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::Label::textWhenEditingColourId, col::text);
        setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::white);
        setColour (juce::Label::outlineWhenEditingColourId, col::neutral);
        setColour (juce::TextEditor::highlightColourId, col::drive.withAlpha (0.3f));
        setColour (juce::TextEditor::focusedOutlineColourId, col::neutral);
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::track);
        setColour (juce::PopupMenu::highlightedTextColourId, col::text);
        setColour (juce::ComboBox::backgroundColourId, col::panel);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::outlineColourId, col::panelEdge);
        setColour (juce::ComboBox::arrowColourId, col::textDim);
        setColour (juce::AlertWindow::backgroundColourId, col::panel);
        setColour (juce::AlertWindow::textColourId, col::text);
        setColour (juce::TextButton::buttonColourId, col::panel);
        setColour (juce::TextButton::textColourOffId, col::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float size = std::min (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();
        const float radius = size * 0.5f;
        const float arcR = radius - 2.0f, track = std::max (2.5f, size * 0.055f);
        juce::Path bgArc; bgArc.addCentredArc (c.x, c.y, arcR, arcR, 0, a0, a1, true);
        g.setColour (col::track);
        g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // bipolar parameters fill from the centre
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float aOrigin = bipolar ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
        const float aVal = a0 + pos * (a1 - a0);
        juce::Path valArc; valArc.addCentredArc (c.x, c.y, arcR, arcR, 0, std::min (aOrigin, aVal), std::max (aOrigin, aVal), true);
        g.setColour (s.isEnabled() ? accent : col::neutral);
        g.strokePath (valArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // body
        const float bodyR = arcR - track - 3.0f;
        juce::DropShadow (juce::Colours::black.withAlpha (0.16f), 6, { 0, 2 }).drawForPath (g, [&] { juce::Path p; p.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2); return p; }());
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x, c.y - bodyR, col::panel.darker (0.04f), c.x, c.y + bodyR, false));
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
        g.setColour (col::panelEdge);
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);
        // pointer
        juce::Path ptr;
        const float pl = bodyR * 0.62f;
        ptr.startNewSubPath (0, -bodyR * 0.25f);
        ptr.lineTo (0, -pl);
        g.setColour (col::text);
        g.strokePath (ptr, juce::PathStrokeType (std::max (2.0f, size * 0.04f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation (aVal).translated (c.x, c.y));
        if (s.hasKeyboardFocus (false))
        {
            g.setColour (accent.withAlpha (0.5f));
            g.drawEllipse (c.x - radius + 1, c.y - radius + 1, radius * 2 - 2, radius * 2 - 2, 1.5f);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        // vertical channel fader
        const auto accent = s.findColour (juce::Slider::trackColourId);
        const float cx = x + w * 0.5f;
        const auto trackR = juce::Rectangle<float> (cx - 2.5f, (float) y + 6, 5.0f, (float) h - 12);
        g.setColour (col::track);
        g.fillRoundedRectangle (trackR, 2.5f);
        // unity mark
        const float y0 = (float) s.getPositionOfValue (0.0);
        g.setColour (col::textDim.withAlpha (0.5f));
        g.drawHorizontalLine ((int) y0, cx - 10, cx + 10);
        g.setColour (accent.withAlpha (s.isEnabled() ? 0.9f : 0.3f));
        g.fillRoundedRectangle (trackR.withTop (pos), 2.5f);
        const auto cap = juce::Rectangle<float> (cx - 13, pos - 8, 26, 16);
        juce::DropShadow (juce::Colours::black.withAlpha (0.2f), 5, { 0, 2 }).drawForRectangle (g, cap.toNearestInt());
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (cap, 4);
        g.setColour (col::panelEdge.darker (0.1f));
        g.drawRoundedRectangle (cap, 4, 1);
        g.setColour (col::text);
        g.drawHorizontalLine ((int) pos, cap.getX() + 5, cap.getRight() - 5);
        if (s.hasKeyboardFocus (false)) { g.setColour (accent.withAlpha (0.6f)); g.drawRoundedRectangle (cap.expanded (2), 5, 1.5f); }
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.setColour (col::text);
        g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
        g.setColour (col::panel);
        g.setFont (font (13.0f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (10, 6), juce::Justification::centredLeft, 6);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = 280;
        juce::AttributedString a; a.append (text, font (13.0f));
        juce::TextLayout tl; tl.createLayout (a, (float) w - 20);
        const int h = (int) tl.getHeight() + 14;
        return juce::Rectangle<int> (pos.x + 12, pos.y + 18, w, h).constrainedWithin (parent);
    }

    juce::Font getLabelFont (juce::Label&) override { return font (12.0f); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f); }
};
} // namespace bfui
