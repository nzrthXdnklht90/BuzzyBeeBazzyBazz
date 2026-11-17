#include "Pages.h"
#include "../PluginProcessor.h"
#include "../Dsp/Eq.h"
#include "../Dsp/Cabinet.h"

namespace bfui
{
// ============================================================================ Module
void Module::paint (juce::Graphics& g)
{
    drawModule (g, getLocalBounds().toFloat().reduced (1.0f));
    auto h = header();
    h.removeFromLeft (titleIndent);
    g.setColour (accent);
    g.fillRoundedRectangle ((float) h.getX(), (float) h.getCentreY() - 7.0f, 3.0f, 14.0f, 1.5f);
    g.setColour (col::text);
    g.setFont (caps (13.0f));
    g.drawText (title, h.withTrimmedLeft (9), juce::Justification::centredLeft);
    if (subtitle.isNotEmpty())
    {
        const int tw = juce::GlyphArrangement::getStringWidthInt (caps (13.0f), title) + 22;
        g.setColour (col::textDim);
        g.setFont (font (12.0f));
        g.drawText (subtitle, h.withTrimmedLeft (tw), juce::Justification::centredLeft);
    }
}

void AdvancedToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (getToggleState() ? col::text : (over ? col::track.darker (0.04f) : col::track));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (getToggleState() ? col::panel : col::textDim);
    g.setFont (caps (10.0f));
    g.drawText (getToggleState() ? "ADVANCED  -" : "ADVANCED  +", r, juce::Justification::centred);
}

static void placeRow (std::initializer_list<juce::Component*> comps, juce::Rectangle<int> r, int itemW)
{
    const int n = (int) comps.size();
    const int gap = n > 1 ? std::max (0, (r.getWidth() - n * itemW) / (n - 1)) : 0;
    int x = n > 1 ? r.getX() : r.getCentreX() - itemW / 2;
    for (auto* c : comps) { c->setBounds (x, r.getY(), itemW, r.getHeight()); x += itemW + gap; }
}

// ============================================================================ TONE
TonePage::TonePage (BassForgeProcessor& p, juce::ValueTree u)
    : proc (p), s (p.apvts), ui (std::move (u)),
      drvOn (s, "drv_on", col::drive), model (s, "drv_model", col::drive, { "DRIVE", "FUZZ", "MODERN" }),
      drvDrive (s, "drv_drive", col::drive, Knob::Size::Large), drvChar (s, "drv_char", col::drive, Knob::Size::Large),
      drvOut (s, "drv_out", col::drive, Knob::Size::Large, "OUTPUT"),
      xLow (s, "drv_xlow", col::drive, Knob::Size::Small), xHigh (s, "drv_xhigh", col::drive, Knob::Size::Small),
      midDrive (s, "drv_middrive", col::drive, Knob::Size::Small), highDrive (s, "drv_highdrive", col::drive, Knob::Size::Small),
      lowComp (s, "drv_lowcomp", col::drive, Knob::Size::Small), lowLvl (s, "drv_lowlvl", col::drive, Knob::Size::Small),
      midLvl (s, "drv_midlvl", col::drive, Knob::Size::Small), highLvl (s, "drv_highlvl", col::drive, Knob::Size::Small),
      lowMode (s, "drv_lowmode", col::drive),
      fzMids (s, "fz_mids", col::drive, Knob::Size::Small, "MIDS"), fzGrit (s, "fz_grit", col::drive, Knob::Size::Small, "GRIT"),
      fzClip (s, "fz_clip", col::drive, Knob::Size::Small, "CLIP"), compStyle (s, "di_comp_style", col::di, { "FET", "OPTO" }),
      compOn (s, "di_comp_on", col::di), eqOn (s, "di_eq_on", col::di),
      comp (s, "di_comp", col::di, Knob::Size::Large), compOut (s, "di_comp_out", col::di, Knob::Size::Medium, "OUTPUT"),
      bass (s, "di_bass", col::di), mid (s, "di_mid", col::di), treble (s, "di_treble", col::di),
      bassF (s, "di_bass_f", col::di, Knob::Size::Small), midF (s, "di_mid_f", col::di, Knob::Size::Small),
      midQ (s, "di_mid_q", col::di, Knob::Size::Small), trebF (s, "di_treble_f", col::di, Knob::Size::Small)
{
    drive.titleIndent = 30; di.titleIndent = 0;
    addAndMakeVisible (drive);
    addAndMakeVisible (di);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &drvOn, &model, &drvAdv, &drvDrive, &drvChar, &drvOut, &xLow, &xHigh, &midDrive, &highDrive,
                                &lowComp, &lowLvl, &midLvl, &highLvl, &lowMode, &lowModeCaption, &charHint, &lowGr, &fzMids, &fzGrit, &fzClip, &compStyle,
                                &compOn, &eqOn, &comp, &compOut, &bass, &mid, &treble, &bassF, &midF, &midQ, &trebF, &compGr, &compTitle, &eqTitle, &diAdv })
        addAndMakeVisible (c);
    lowModeCaption.setText ("LOW END", juce::dontSendNotification);
    lowModeCaption.setFont (caps (9.5f));
    lowModeCaption.setColour (juce::Label::textColourId, col::textDim);
    lowModeCaption.setJustificationType (juce::Justification::centredLeft);
    charHint.setFont (font (11.0f));
    charHint.setColour (juce::Label::textColourId, col::textDim);
    charHint.setJustificationType (juce::Justification::centred);
    for (auto* l : { &compTitle, &eqTitle })
    {
        l->setFont (caps (11.0f));
        l->setColour (juce::Label::textColourId, col::text);
    }
    compTitle.setText ("COMPRESSOR", juce::dontSendNotification);
    eqTitle.setText ("TONE", juce::dontSendNotification);
    lowGr.setTooltip ("How much the low comp is evening out your low end right now.");
    compGr.setTooltip ("How much the DI compressor is turning loud notes down right now.");

    drvAdv.setToggleState ((bool) ui.getProperty ("drvAdv", false), juce::dontSendNotification);
    diAdv.setToggleState ((bool) ui.getProperty ("diAdv", false), juce::dontSendNotification);
    drvAdv.onClick = [this] { ui.setProperty ("drvAdv", drvAdv.getToggleState(), nullptr); resized(); };
    diAdv.onClick = [this] { ui.setProperty ("diAdv", diAdv.getToggleState(), nullptr); resized(); };
}

void TonePage::resized()
{
    auto r = getLocalBounds().reduced (12);
    const int driveH = (int) (r.getHeight() * 0.53f);
    drive.setBounds (r.removeFromTop (driveH));
    r.removeFromTop (12);
    di.setBounds (r);

    // ---- DRIVE
    {
        auto h = drive.header() + drive.getPosition();
        drvOn.setBounds (h.removeFromLeft (24).withSizeKeepingCentre (24, 24));
        drvAdv.setBounds (h.removeFromRight (104).withSizeKeepingCentre (104, 24));
        h.removeFromRight (12);
        const int mw = std::min (300, h.getWidth() - 260);
        model.setBounds (h.removeFromRight (mw).withSizeKeepingCentre (mw, 28));
        auto c = drive.content() + drive.getPosition();
        const bool adv = drvAdv.getToggleState();
        for (auto* k : std::initializer_list<juce::Component*> { &xLow, &xHigh, &midDrive, &highDrive, &lowComp, &lowLvl, &midLvl, &highLvl, &lowMode, &lowModeCaption, &lowGr })
            k->setVisible (adv);
        auto main = adv ? c.removeFromLeft ((int) (c.getWidth() * 0.46f)) : c;
        auto hint = main.removeFromBottom (18);
        charHint.setBounds (hint);
        const bool fuzz = (int) std::lround (s.getRawParameterValue ("drv_model")->load()) == 1;
        for (auto* k : { &fzMids, &fzGrit, &fzClip }) k->setVisible (fuzz);
        if (fuzz)
        {
            // the fuzz pedal's own front-panel controls sit under the main knobs
            auto row = main.removeFromBottom (std::min (78, main.getHeight() / 3));
            placeRow ({ &fzMids, &fzGrit, &fzClip }, row.withSizeKeepingCentre (std::min (row.getWidth(), adv ? row.getWidth() : 420), row.getHeight()), 72);
        }
        const int kw = std::min (adv ? 118 : (fuzz ? 120 : 150), std::min (main.getWidth() / (adv ? 3 : 4), main.getHeight() - 40));
        placeRow ({ &drvDrive, &drvChar, &drvOut }, main.withSizeKeepingCentre (adv ? main.getWidth() : std::min (main.getWidth(), 620), std::min (main.getHeight(), kw + 40)), kw);
        if (adv)
        {
            c.removeFromLeft (10);
            auto bottom = c.removeFromBottom (28);
            lowModeCaption.setBounds (bottom.removeFromLeft (58));
            lowGr.setBounds (bottom.removeFromRight (110).reduced (0, 6));
            lowMode.setBounds (bottom.reduced (6, 1));
            c.removeFromBottom (6);
            auto row1 = c.removeFromTop (c.getHeight() / 2), row2 = c;
            const int sw = std::min (84, row1.getWidth() / 4);
            placeRow ({ &xLow, &xHigh, &midDrive, &highDrive }, row1, sw);
            placeRow ({ &lowComp, &lowLvl, &midLvl, &highLvl }, row2, sw);
        }
    }
    // ---- DI
    {
        auto c = di.content() + di.getPosition();
        diAdv.setBounds ((di.header() + di.getPosition()).removeFromRight (104).withSizeKeepingCentre (104, 24));
        auto left = c.removeFromLeft ((int) (c.getWidth() * 0.40f));
        c.removeFromLeft (16);
        auto lh = left.removeFromTop (24);
        compOn.setBounds (lh.removeFromLeft (24));
        compStyle.setBounds (lh.removeFromRight (110).reduced (0, 1));
        compTitle.setBounds (lh.withTrimmedLeft (6));
        compGr.setBounds (left.removeFromBottom (18).reduced (10, 0));
        left.removeFromBottom (6);
        placeRow ({ &comp, &compOut }, left.withSizeKeepingCentre (left.getWidth(), std::min (left.getHeight(), 150)), std::min (120, left.getWidth() / 2 - 6));
        auto eh = c.removeFromTop (24);
        eqOn.setBounds (eh.removeFromLeft (24));
        eqTitle.setBounds (eh.withTrimmedLeft (6));
        const bool adv = diAdv.getToggleState();
        for (auto* k : std::initializer_list<juce::Component*> { &bassF, &midF, &midQ, &trebF }) k->setVisible (adv);
        if (adv)
        {
            auto top = c.removeFromTop ((int) (c.getHeight() * 0.56f));
            placeRow ({ &bass, &mid, &treble }, top, std::min (100, top.getWidth() / 3));
            placeRow ({ &bassF, &midF, &midQ, &trebF }, c, std::min (80, c.getWidth() / 4));
        }
        else
            placeRow ({ &bass, &mid, &treble }, c.withSizeKeepingCentre (c.getWidth(), std::min (c.getHeight(), 150)), std::min (110, c.getWidth() / 3));
    }
}

void TonePage::tick()
{
    const int m = (int) std::lround (s.getRawParameterValue ("drv_model")->load());
    if (m != lastModel)
    {
        lastModel = m;
        static const char* hints[] = { "Character: smooth  <->  more presence", "Drive = fuzz amount   |   Character = tone: dark  <->  bright",
                                       "Character: more weight  <->  more bite" };
        static const char* tips[] = {
            "Character (Drive): adds presence. Low is smooth and round, high is crisp and cutting.",
            "Character (Fuzz): the fuzz tone control. Low is dark and woolly, high is bright and buzzy.",
            "Character (Modern): low adds weight going into the distortion, high adds bite and pick attack." };
        static const char* subs[] = { "flexible overdrive and saturation", "huge, thick fuzz (clean lows stay underneath)", "tight, aggressive distortion" };
        static const char* driveTips[] = {
            "Drive: how much dirt. Low = warm and round, high = gritty and aggressive. The volume stays about the same.",
            "Drive (Fuzz): how much fuzz and sustain. Low is a rough, splatty fuzz; high is a thick wall with long sustain.",
            "Drive: how much dirt. Low = warm and round, high = gritty and aggressive. The volume stays about the same." };
        drvDrive.slider.setTooltip (driveTips[juce::jlimit (0, 2, m)]);
        charHint.setText (hints[juce::jlimit (0, 2, m)], juce::dontSendNotification);
        drvChar.slider.setTooltip (tips[juce::jlimit (0, 2, m)]);
        drive.subtitle = subs[juce::jlimit (0, 2, m)];
        drive.repaint();
        resized();   // show / hide the fuzz controls
    }
    auto& mt = proc.meters();
    compGr.push (mt.diCompGr.load());
    lowGr.push (mt.lowCompGr.load());
}

// ============================================================================ CabView
static const double kDistRows[8] = { 0.0, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 };

class CabView : public juce::Component, public juce::SettableTooltipClient
{
public:
    CabView (APVTS& st, CabSpacePage& pg) : s (st), page (pg)
    {
        setWantsKeyboardFocus (true);
        setTitle ("Cabinet");
        setTooltip ("Your virtual cabinet. Hover or click it to place the microphones on the speaker.");
    }
    void paint (juce::Graphics& g) override
    {
        const int model = (int) std::lround (s.getRawParameterValue ("cab_model")->load());
        auto r = getLocalBounds().toFloat().reduced (6.0f);
        // cabinet box proportions per model
        const float aspect[4] = { 1.0f, 0.55f, 0.62f, 1.0f };   // width / height
        const float hgt = std::min (r.getHeight(), r.getWidth() / aspect[model]);
        auto box = r.withSizeKeepingCentre (hgt * aspect[model], hgt);
        juce::DropShadow (juce::Colours::black.withAlpha (0.22f), 12, { 0, 4 }).drawForRectangle (g, box.toNearestInt());
        g.setColour (juce::Colour (0xff3A3835));
        g.fillRoundedRectangle (box, 8.0f);
        auto grille = box.reduced (box.getWidth() * 0.06f);
        g.setColour (juce::Colour (0xff4B4844));
        g.fillRoundedRectangle (grille, 5.0f);
        // speaker layout
        int cols = 2, rows = 2;
        if (model == 1) rows = 4;
        if (model == 2) { cols = 1; rows = 2; }
        speakers.clear();
        const float cw = grille.getWidth() / cols, ch = grille.getHeight() / rows;
        const float rad = std::min (cw, ch) * 0.42f;
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < cols; ++x)
            {
                const auto c = juce::Point<float> (grille.getX() + (x + 0.5f) * cw, grille.getY() + (y + 0.5f) * ch);
                speakers.push_back ({ c, rad });
                g.setColour (juce::Colour (0xff2A2826));
                g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
                g.setColour (juce::Colour (0xff5A5650));
                g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.5f);
                g.drawEllipse (c.x - rad * 0.82f, c.y - rad * 0.82f, rad * 1.64f, rad * 1.64f, 1.0f);
                g.setColour (juce::Colour (0xff6A665F));
                g.fillEllipse (c.x - rad * 0.3f, c.y - rad * 0.3f, rad * 0.6f, rad * 0.6f);
            }
        if (model == 3)   // tweeter
        {
            const auto t = juce::Point<float> (box.getCentreX(), box.getBottom() - box.getHeight() * 0.035f);
            g.setColour (juce::Colour (0xff6A665F));
            g.fillEllipse (t.x - 5, t.y - 4, 10, 8);
        }
        // mic markers on the first speaker (mic 1 left of centre, mic 2 right)
        if (! speakers.empty())
        {
            const auto& sp = speakers.front();
            for (int m = 0; m < 2; ++m)
            {
                const juce::String pre = m == 0 ? "mic1_" : "mic2_";
                if (s.getRawParameterValue (pre + "on")->load() < 0.5f) continue;
                const float pos = s.getRawParameterValue (pre + "pos")->load();
                const float dist = s.getRawParameterValue (pre + "dist")->load();
                const float ang = m == 0 ? juce::MathConstants<float>::pi * 1.25f : juce::MathConstants<float>::pi * 0.25f;
                const auto mp = sp.c + juce::Point<float> (std::cos (ang), std::sin (ang)) * (pos * sp.r * 0.95f);
                const float mr = 9.0f - dist * 0.7f;
                g.setColour ((m == 0 ? col::drive : col::di).withAlpha (0.25f));
                g.fillEllipse (mp.x - mr - 6, mp.y - mr - 6, (mr + 6) * 2, (mr + 6) * 2);
                g.setColour (m == 0 ? col::drive : col::di);
                g.fillEllipse (mp.x - mr, mp.y - mr, mr * 2, mr * 2);
                g.setColour (juce::Colours::white);
                g.setFont (caps (9.0f));
                g.drawText (juce::String (m + 1), juce::Rectangle<float> (mp.x - mr, mp.y - mr, mr * 2, mr * 2), juce::Justification::centred);
            }
        }
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (font (10.5f));
        g.drawText (hasKeyboardFocus (false) ? "Enter: place mics" : "hover or click to place mics", box.removeFromBottom (18), juce::Justification::centred);
        if (hasKeyboardFocus (false)) { g.setColour (col::drive.withAlpha (0.6f)); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1), 8, 1.5f); }
    }
    void mouseEnter (const juce::MouseEvent&) override { page.showGrid (true, false); }
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked()) page.showGrid (true, ! page.isGridPinned()); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey) { page.showGrid (true, true); return true; }
        return false;
    }

private:
    struct Spk { juce::Point<float> c; float r; };
    std::vector<Spk> speakers;
    APVTS& s;
    CabSpacePage& page;
};

// ============================================================================ MicGrid (overlay)
class MicGrid : public juce::Component
{
public:
    MicGrid (APVTS& st, CabSpacePage& pg)
        : s (st), page (pg),
          type1 (st, "mic1_type", col::drive), type2 (st, "mic2_type", col::di),
          inv1 (st, "mic1_inv", "POLARITY", col::drive), inv2 (st, "mic2_inv", "POLARITY", col::di),
          pos1 (*st.getParameter ("mic1_pos"), [this] (float) { repaint(); }), dist1 (*st.getParameter ("mic1_dist"), [this] (float) { repaint(); }),
          pos2 (*st.getParameter ("mic2_pos"), [this] (float) { repaint(); }), dist2 (*st.getParameter ("mic2_dist"), [this] (float) { repaint(); })
    {
        for (auto* c : std::initializer_list<juce::Component*> { &tab1, &tab2, &type1, &type2, &inv1, &inv2, &close }) addAndMakeVisible (c);
        tab1.setButtonText ("MIC 1"); tab2.setButtonText ("MIC 2");
        for (auto* t : { &tab1, &tab2 }) { t->setClickingTogglesState (false); t->setRadioGroupId (77); }
        tab1.setToggleState (true, juce::dontSendNotification);
        tab1.onClick = [this] { select (0); };
        tab2.onClick = [this] { select (1); };
        tab1.setTooltip ("Place microphone 1."); tab2.setTooltip ("Place microphone 2.");
        close.setButtonText ("x");
        close.setTooltip ("Close the mic placement panel.");
        close.onClick = [this] { page.showGrid (false, false); };
        setWantsKeyboardFocus (true);
        setTitle ("Microphone placement");
        select (0);
    }
    void select (int m)
    {
        mic = m;
        tab1.setToggleState (m == 0, juce::dontSendNotification);
        tab2.setToggleState (m == 1, juce::dontSendNotification);
        type1.setVisible (m == 0); inv1.setVisible (m == 0);
        type2.setVisible (m == 1); inv2.setVisible (m == 1);
        repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (26);
        close.setBounds (top.removeFromRight (26));
        tab1.setBounds (top.removeFromLeft (64)); top.removeFromLeft (4);
        tab2.setBounds (top.removeFromLeft (64));
        r.removeFromTop (8);
        auto row = r.removeFromTop (26);
        inv1.setBounds (row.removeFromRight (84)); inv2.setBounds (inv1.getBounds());
        row.removeFromRight (8);
        type1.setBounds (row); type2.setBounds (row);
        r.removeFromTop (10);
        gridArea = r.withTrimmedLeft (40).withTrimmedTop (16).withTrimmedBottom (20);
    }
    void paint (juce::Graphics& g) override
    {
        juce::DropShadow (juce::Colours::black.withAlpha (0.25f), 18, { 0, 6 }).drawForRectangle (g, getLocalBounds().reduced (4));
        g.setColour (col::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4), 12.0f);
        g.setColour (col::panelEdge);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (4.5f), 12.0f, 1.0f);
        const auto accent = mic == 0 ? col::drive : col::di;
        static const char* colNames[] = { "CAP", "CAP EDGE", "CONE", "CONE EDGE" };
        const float cw = gridArea.getWidth() / 4.0f, rh = gridArea.getHeight() / 8.0f;
        g.setFont (caps (9.0f));
        g.setColour (col::textDim);
        for (int c = 0; c < 4; ++c) g.drawText (colNames[c], juce::Rectangle<float> (gridArea.getX() + c * cw, (float) gridArea.getY() - 16, cw, 14), juce::Justification::centred);
        for (int rr = 0; rr < 8; ++rr) g.drawText (juce::String (kDistRows[rr], 1) + " in", juce::Rectangle<float> ((float) gridArea.getX() - 44, gridArea.getY() + rr * rh, 40, rh), juce::Justification::centredRight);
        const float pos = (mic == 0 ? pos1 : pos2).value, dist = (mic == 0 ? dist1 : dist2).value;
        int selC = juce::jlimit (0, 3, (int) std::lround (pos * 3.0f)), selR = 0;
        double best = 1e9;
        for (int rr = 0; rr < 8; ++rr) if (std::abs (kDistRows[rr] - dist) < best) { best = std::abs (kDistRows[rr] - dist); selR = rr; }
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 8; ++rr)
            {
                auto cell = juce::Rectangle<float> (gridArea.getX() + c * cw, gridArea.getY() + rr * rh, cw, rh).reduced (2.0f);
                const bool sel = c == selC && rr == selR, hov = c == hoverC && rr == hoverR, foc = c == focusC && rr == focusR && hasKeyboardFocus (false);
                g.setColour (sel ? accent : (hov ? col::track.darker (0.06f) : col::track));
                g.fillRoundedRectangle (cell, 4.0f);
                if (foc) { g.setColour (accent); g.drawRoundedRectangle (cell, 4.0f, 1.5f); }
            }
        // exact position dot (continuous values between grid points)
        const float px = gridArea.getX() + (pos * 3.0f + 0.5f) * cw;
        float py = gridArea.getY() + 0.5f * rh;
        for (int rr = 0; rr < 7; ++rr)
            if (dist >= kDistRows[rr] && dist <= kDistRows[rr + 1])
                py = gridArea.getY() + (rr + 0.5f + (float) ((dist - kDistRows[rr]) / (kDistRows[rr + 1] - kDistRows[rr]))) * rh;
        if (dist >= kDistRows[7]) py = gridArea.getY() + 7.5f * rh;
        g.setColour (juce::Colours::white);
        g.fillEllipse (px - 3.5f, py - 3.5f, 7, 7);
        g.setColour (col::text);
        g.drawEllipse (px - 3.5f, py - 3.5f, 7, 7, 1.0f);
        g.setColour (col::textDim);
        g.setFont (font (10.5f));
        g.drawText (page.isGridPinned() ? "Pinned - click outside the grid area or press Esc to close" : "Click the cabinet to keep this open",
                    getLocalBounds().reduced (14, 8).removeFromBottom (14), juce::Justification::centredLeft);
    }
    bool cellAt (juce::Point<int> p, int& c, int& r) const
    {
        if (! gridArea.contains (p)) return false;
        c = juce::jlimit (0, 3, (p.x - gridArea.getX()) * 4 / gridArea.getWidth());
        r = juce::jlimit (0, 7, (p.y - gridArea.getY()) * 8 / gridArea.getHeight());
        return true;
    }
    void setCell (int c, int r)
    {
        auto& pa = mic == 0 ? pos1 : pos2;
        auto& da = mic == 0 ? dist1 : dist2;
        pa.att.setValueAsCompleteGesture ((float) c / 3.0f);
        da.att.setValueAsCompleteGesture ((float) kDistRows[r]);
        focusC = c; focusR = r;
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int c = -1, r = -1;
        if (! cellAt (e.getPosition(), c, r)) c = r = -1;
        if (c != hoverC || r != hoverR) { hoverC = c; hoverR = r; repaint(); }
    }
    void mouseExit (const juce::MouseEvent& e) override
    {
        hoverC = hoverR = -1; repaint();
        if (! getLocalBounds().contains (e.getEventRelativeTo (this).getPosition()) && ! page.isGridPinned())
            page.showGrid (false, false);
    }
    void mouseDown (const juce::MouseEvent& e) override { int c, r; if (cellAt (e.getPosition(), c, r)) setCell (c, r); }
    void mouseDrag (const juce::MouseEvent& e) override { int c, r; if (cellAt (e.getPosition(), c, r)) setCell (c, r); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { page.showGrid (false, false); return true; }
        if (k == juce::KeyPress::leftKey) { focusC = std::max (0, focusC - 1); setCell (focusC, focusR); return true; }
        if (k == juce::KeyPress::rightKey) { focusC = std::min (3, focusC + 1); setCell (focusC, focusR); return true; }
        if (k == juce::KeyPress::upKey) { focusR = std::max (0, focusR - 1); setCell (focusC, focusR); return true; }
        if (k == juce::KeyPress::downKey) { focusR = std::min (7, focusR + 1); setCell (focusC, focusR); return true; }
        if (k == juce::KeyPress::tabKey) { select (1 - mic); return true; }
        return false;
    }

private:
    struct Bound
    {
        Bound (juce::RangedAudioParameter& p, std::function<void (float)> cb)
            : att (p, [this, cb] (float v) { value = v; cb (v); }, nullptr) { att.sendInitialUpdate(); }
        float value = 0;
        juce::ParameterAttachment att;
    };
    APVTS& s;
    CabSpacePage& page;
    juce::TextButton tab1, tab2, close;
    Segmented type1, type2;
    TextToggle inv1, inv2;
    Bound pos1, dist1, pos2, dist2;
    juce::Rectangle<int> gridArea;
    int mic = 0, hoverC = -1, hoverR = -1, focusC = 1, focusR = 2;
};

// ============================================================================ EqGraph
class EqGraph : public juce::Component, public juce::SettableTooltipClient
{
public:
    EqGraph (APVTS& st, juce::ValueTree u) : s (st), ui (std::move (u))
    {
        static const char* ids[7][4] = { { "eq1_on", "eq1_freq", "eq1_gain", "eq1_q" }, { "eq2_on", "eq2_freq", "eq2_gain", "eq2_q" },
                                         { "eq3_on", "eq3_freq", "eq3_gain", "eq3_q" }, { "eq4_on", "eq4_freq", "eq4_gain", "eq4_q" },
                                         { "eq5_on", "eq5_freq", "eq5_gain", "eq5_q" }, { "eq6_on", "eq6_freq", "eq6_gain", "eq6_q" },
                                         { "eq7_on", "eq7_freq", "eq7_gain", "eq7_q" } };
        for (int b = 0; b < 7; ++b)
        {
            auto& band = bands[(size_t) b];
            band.on = std::make_unique<PowerButton> (s, ids[b][0], col::drive);
            band.freq = std::make_unique<Knob> (s, ids[b][1], col::drive, Knob::Size::Small, "FREQ");
            band.gain = std::make_unique<Knob> (s, ids[b][2], col::drive, Knob::Size::Small, "GAIN");
            band.q = std::make_unique<Knob> (s, ids[b][3], col::drive, Knob::Size::Small, "Q");
            if (b == 0 || b == 6) band.type = std::make_unique<Segmented> (s, b == 0 ? "eq1_type" : "eq7_type", col::drive,
                                                                            juce::StringArray { "BELL", "SHELF", "CUT" });
            band.fA = std::make_unique<juce::ParameterAttachment> (*s.getParameter (ids[b][1]), [this] (float) { repaint(); });
            band.gA = std::make_unique<juce::ParameterAttachment> (*s.getParameter (ids[b][2]), [this] (float) { repaint(); });
            band.qA = std::make_unique<juce::ParameterAttachment> (*s.getParameter (ids[b][3]), [this] (float) { repaint(); });
            for (juce::Component* c : std::initializer_list<juce::Component*> { band.on.get(), band.freq.get(), band.gain.get(), band.q.get() }) addChildComponent (c);
            if (band.type) addChildComponent (*band.type);
            band.ids = ids[b];
        }
        bandLabel.setFont (caps (11.0f));
        bandLabel.setColour (juce::Label::textColourId, col::text);
        addAndMakeVisible (bandLabel);
        setWantsKeyboardFocus (true);
        setTitle ("Drive EQ graph");
        setTooltip ("Drag a numbered dot to change that band: left/right = frequency, up/down = boost or cut. Scroll to change the width. Double-click a dot to reset its gain.");
        selected = juce::jlimit (0, 6, (int) ui.getProperty ("eqBand", 3));
        showSelected();
    }

    void showSelected()
    {
        for (int b = 0; b < 7; ++b)
        {
            auto& band = bands[(size_t) b];
            const bool v = b == selected;
            band.on->setVisible (v); band.freq->setVisible (v); band.gain->setVisible (v); band.q->setVisible (v);
            if (band.type) band.type->setVisible (v);
        }
        bandLabel.setText ("BAND " + juce::String (selected + 1), juce::dontSendNotification);
        ui.setProperty ("eqBand", selected, nullptr);
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto ctl = r.removeFromBottom (78);
        graph = r.reduced (2, 4).toFloat();
        auto& band = bands[(size_t) selected];
        auto lead = ctl.removeFromLeft (84);
        bandLabel.setBounds (lead.removeFromTop (30).withTrimmedLeft (30));
        band.on->setBounds (bandLabel.getBounds().withX (lead.getX()).withWidth (24).withSizeKeepingCentre (22, 22));
        if (band.type) band.type->setBounds (lead.removeFromTop (26).reduced (0, 1));
        placeRow ({ band.freq.get(), band.gain.get(), band.q.get() }, ctl, std::min (74, ctl.getWidth() / 3));
    }

    float xFor (double hz) const { return graph.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * graph.getWidth(); }
    double hzFor (float x) const { return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) (x - graph.getX()) / graph.getWidth())); }
    float yFor (double db) const { return graph.getCentreY() - (float) (db / 20.0) * graph.getHeight() * 0.5f; }
    double dbFor (float y) const { return juce::jlimit (-18.0, 18.0, (double) (graph.getCentreY() - y) / (graph.getHeight() * 0.5f) * 20.0); }

    bassforge::EqBand bandValue (int b) const
    {
        bassforge::EqBand e;
        const auto* id = bands[(size_t) b].ids;
        e.on = s.getRawParameterValue (id[0])->load() > 0.5f;
        e.freq = s.getRawParameterValue (id[1])->load();
        e.gainDb = s.getRawParameterValue (id[2])->load();
        e.q = s.getRawParameterValue (id[3])->load();
        if (b == 0) e.type = (bassforge::EqBandType) (int) s.getRawParameterValue ("eq1_type")->load();
        if (b == 6) e.type = (bassforge::EqBandType) (int) s.getRawParameterValue ("eq7_type")->load();
        return e;
    }

    juce::Point<float> pointFor (int b) const
    {
        const auto e = bandValue (b);
        const bool cut = (b == 0 || b == 6) && e.type == bassforge::EqBandType::Cut;
        return { xFor (e.freq), yFor (cut ? 0.0 : e.gainDb) };
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xffF6F2EA));
        g.fillRoundedRectangle (graph, 8.0f);
        g.setFont (font (9.5f));
        for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const float x = xFor (hz);
            g.setColour (col::panelEdge);
            g.drawVerticalLine ((int) x, graph.getY() + 4, graph.getBottom() - 4);
            g.setColour (col::textDim);
            g.drawText (hz >= 1000 ? juce::String (hz / 1000, 0) + "k" : juce::String (hz, 0), (int) x - 15, (int) graph.getBottom() - 14, 30, 12, juce::Justification::centred);
        }
        for (double db : { -12.0, -6.0, 0.0, 6.0, 12.0 })
        {
            g.setColour (db == 0 ? col::neutral : col::panelEdge);
            g.drawHorizontalLine ((int) yFor (db), graph.getX() + 4, graph.getRight() - 4);
        }
        bassforge::EqBand e[7];
        for (int b = 0; b < 7; ++b) e[b] = bandValue (b);
        const bool on = s.getRawParameterValue ("peq_on")->load() > 0.5f;
        juce::Path curve;
        for (int i = 0; i <= 240; ++i)
        {
            const float x = graph.getX() + graph.getWidth() * i / 240.0f;
            const double db = bassforge::ParametricEq7::responseDb (e, hzFor (x), 48000.0);
            const float y = juce::jlimit (graph.getY(), graph.getBottom(), yFor (db));
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (graph.getRight(), yFor (0)); fill.lineTo (graph.getX(), yFor (0)); fill.closeSubPath();
        g.setColour (col::drive.withAlpha (on ? 0.12f : 0.05f));
        g.fillPath (fill);
        g.setColour (on ? col::drive : col::neutral);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
        for (int b = 0; b < 7; ++b)
        {
            const auto p = pointFor (b);
            const float rr = b == selected ? 9.0f : 7.5f;
            g.setColour (e[b].on ? (b == selected ? col::drive : juce::Colours::white) : col::track);
            g.fillEllipse (p.x - rr, p.y - rr, rr * 2, rr * 2);
            g.setColour (b == selected ? col::drive.darker (0.3f) : col::neutral);
            g.drawEllipse (p.x - rr, p.y - rr, rr * 2, rr * 2, 1.2f);
            g.setColour (b == selected ? juce::Colours::white : col::text);
            g.setFont (caps (9.5f));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (p.x - rr, p.y - rr, rr * 2, rr * 2), juce::Justification::centred);
        }
        if (hasKeyboardFocus (false)) { g.setColour (col::drive.withAlpha (0.5f)); g.drawRoundedRectangle (graph, 8.0f, 1.5f); }
    }

    int hit (juce::Point<float> p) const
    {
        int best = -1; float bd = 14.0f;
        for (int b = 0; b < 7; ++b) { const float d = pointFor (b).getDistanceFrom (p); if (d < bd) { bd = d; best = b; } }
        return best;
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        if (h < 0) return;
        if (h != selected) { selected = h; showSelected(); }
        dragging = true;
        bands[(size_t) selected].fA->beginGesture();
        bands[(size_t) selected].gA->beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        auto& b = bands[(size_t) selected];
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const auto start = pointFor (selected);
        const auto target = start + (e.position - start) * fine;
        b.fA->setValueAsPartOfGesture ((float) juce::jlimit (20.0, 20000.0, hzFor (target.x)));
        b.gA->setValueAsPartOfGesture ((float) dbFor (target.y));
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        bands[(size_t) selected].fA->endGesture();
        bands[(size_t) selected].gA->endGesture();
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hit (e.position);
        if (h >= 0) bands[(size_t) h].gA->setValueAsCompleteGesture (0.0f);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int h = hit (e.position);
        const int b = h >= 0 ? h : selected;
        const float q = s.getRawParameterValue (bands[(size_t) b].ids[3])->load();
        bands[(size_t) b].qA->setValueAsCompleteGesture (juce::jlimit (0.1f, 18.0f, q * (w.deltaY > 0 ? 1.12f : 1.0f / 1.12f)));
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::leftKey) { selected = std::max (0, selected - 1); showSelected(); return true; }
        if (k == juce::KeyPress::rightKey) { selected = std::min (6, selected + 1); showSelected(); return true; }
        return false;
    }

private:
    struct Band
    {
        std::unique_ptr<PowerButton> on;
        std::unique_ptr<Knob> freq, gain, q;
        std::unique_ptr<Segmented> type;
        std::unique_ptr<juce::ParameterAttachment> fA, gA, qA;
        const char* const* ids = nullptr;
    };
    APVTS& s;
    juce::ValueTree ui;
    std::array<Band, 7> bands;
    juce::Label bandLabel;
    juce::Rectangle<float> graph;
    int selected = 3;
    bool dragging = false;
};

// ============================================================================ CAB & SPACE
CabSpacePage::CabSpacePage (BassForgeProcessor& p, juce::ValueTree u)
    : proc (p), s (p.apvts), ui (std::move (u)),
      cabOn (s, "cab_on", col::drive), eqOn (s, "peq_on", col::drive), roomOn (s, "room_on", col::room),
      cabModel (s, "cab_model", col::drive, { "4x10 BRIGHT", "8x10 CLASSIC", "2x15 DEEP", "4x10 MODERN" }),
      mic1On (s, "mic1_on", col::drive), mic2On (s, "mic2_on", col::di),
      mic1Type (s, "mic1_type", col::drive, { "DYN", "COND", "RIB" }), mic2Type (s, "mic2_type", col::di, { "DYN", "COND", "RIB" }),
      mic1Pos (s, "mic1_pos"), mic1Dist (s, "mic1_dist"), mic2Pos (s, "mic2_pos"), mic2Dist (s, "mic2_dist"),
      mic1Level (s, "mic1_level", col::drive, Knob::Size::Small, " "), mic2Level (s, "mic2_level", col::di, Knob::Size::Small, " "),
      mic1Inv (s, "mic1_inv", juce::CharPointer_UTF8 ("\xc3\x98"), col::drive), mic2Inv (s, "mic2_inv", juce::CharPointer_UTF8 ("\xc3\x98"), col::di),
      micBal (s, "mic_bal", col::drive, Knob::Size::Small, "BALANCE"), micSpread (s, "mic_spread", col::drive, Knob::Size::Small, "SPREAD"),
      micAlign (s, "mic_align", col::drive, { "AUTO ALIGN", "NATURAL" }),
      roomType (s, "room_type", col::room, { "TIGHT", "LIVE", "LARGE" }),
      roomSize (s, "room_size", col::room, Knob::Size::Medium, "SIZE"), roomChar (s, "room_char", col::room, Knob::Size::Medium, "CHARACTER")
{
    cab.titleIndent = 30; eq.titleIndent = 30; room.titleIndent = 30;
    cab.subtitle = "procedural speaker + microphone model";
    addAndMakeVisible (cab); addAndMakeVisible (eq); addAndMakeVisible (room);
    cabView = std::make_unique<CabView> (s, *this);
    eqGraph = std::make_unique<EqGraph> (s, ui);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &cabOn, &eqOn, &roomOn, &cabModel, cabView.get(), &mic1On, &mic2On, &mic1Type, &mic2Type,
                                &mic1Name, &mic2Name, &posCap, &distCap, &lvlCap, &mic1Pos, &mic1Dist, &mic2Pos, &mic2Dist, &mic1Level, &mic2Level,
                                &mic1Inv, &mic2Inv, &micBal, &micSpread, &micAlign, &alignCaption, eqGraph.get(), &roomType, &roomSize, &roomChar })
        addAndMakeVisible (c);
    grid = std::make_unique<MicGrid> (s, *this);
    addChildComponent (*grid);
    for (auto* l : { &mic1Name, &mic2Name, &posCap, &distCap, &lvlCap, &alignCaption })
    {
        l->setFont (caps (10.0f));
        l->setColour (juce::Label::textColourId, col::textDim);
        l->setJustificationType (juce::Justification::centred);
    }
    mic1Name.setText ("MIC 1", juce::dontSendNotification); mic1Name.setColour (juce::Label::textColourId, col::drive.darker (0.2f));
    mic2Name.setText ("MIC 2", juce::dontSendNotification); mic2Name.setColour (juce::Label::textColourId, col::di.darker (0.2f));
    posCap.setText ("POSITION", juce::dontSendNotification);
    distCap.setText ("DISTANCE", juce::dontSendNotification);
    lvlCap.setText ("LEVEL", juce::dontSendNotification);
    alignCaption.setText ("TIMING", juce::dontSendNotification);
}

CabSpacePage::~CabSpacePage() { stopTimer(); }

void CabSpacePage::resized()
{
    auto r = getLocalBounds().reduced (12);
    cab.setBounds (r.removeFromTop ((int) (r.getHeight() * 0.50f)));
    r.removeFromTop (12);
    eq.setBounds (r.removeFromLeft ((int) (r.getWidth() * 0.62f)));
    r.removeFromLeft (12);
    room.setBounds (r);

    // ---- cabinet
    {
        auto h = cab.header() + cab.getPosition();
        cabOn.setBounds (h.removeFromLeft (24).withSizeKeepingCentre (24, 24));
        auto c = cab.content() + cab.getPosition();
        auto left = c.removeFromLeft (std::min (300, c.getWidth() * 2 / 5));
        cabModel.setBounds (left.removeFromTop (28));
        cabModel.fontPx = 9.5f;
        left.removeFromTop (6);
        cabView->setBounds (left);
        c.removeFromLeft (16);
        auto colsHdr = c.removeFromTop (16);
        const int nameW = 44, valW = 62, lvlW = 58, invW = 40;
        const int typeW = std::max (120, c.getWidth() - (30 + nameW + 8 + 2 * valW + lvlW + invW + 8));
        auto hdr = colsHdr;
        hdr.removeFromLeft (nameW + 30 + typeW + 8);
        posCap.setBounds (hdr.removeFromLeft (valW));
        distCap.setBounds (hdr.removeFromLeft (valW));
        lvlCap.setBounds (hdr.removeFromLeft (lvlW));
        auto rowFor = [&] (juce::Rectangle<int> row, PowerButton& on, juce::Label& name, Segmented& type, ValueLabel& pos, ValueLabel& dist, Knob& lvl, TextToggle& inv) {
            on.setBounds (row.removeFromLeft (24).withSizeKeepingCentre (22, 22));
            row.removeFromLeft (6);
            name.setBounds (row.removeFromLeft (nameW));
            type.setBounds (row.removeFromLeft (typeW).withSizeKeepingCentre (typeW, 26));
            row.removeFromLeft (8);
            pos.setBounds (row.removeFromLeft (valW).withSizeKeepingCentre (valW, 22));
            dist.setBounds (row.removeFromLeft (valW).withSizeKeepingCentre (valW, 22));
            lvl.setBounds (row.removeFromLeft (lvlW).withSizeKeepingCentre (lvlW, std::min (row.getHeight(), 58)));
            row.removeFromLeft (8);
            inv.setBounds (row.removeFromLeft (invW).withSizeKeepingCentre (invW, 24));
        };
        const int rowH = std::min (60, c.getHeight() / 3);
        rowFor (c.removeFromTop (rowH), mic1On, mic1Name, mic1Type, mic1Pos, mic1Dist, mic1Level, mic1Inv);
        rowFor (c.removeFromTop (rowH), mic2On, mic2Name, mic2Type, mic2Pos, mic2Dist, mic2Level, mic2Inv);
        c.removeFromTop (6);
        auto last = c;
        micBal.setBounds (last.removeFromLeft (80));
        micSpread.setBounds (last.removeFromLeft (80));
        last.removeFromLeft (10);
        alignCaption.setBounds (last.removeFromTop (15).withWidth (200));
        micAlign.setBounds (last.withHeight (28).withWidth (std::min (220, last.getWidth())));
        // overlay sits over the cabinet illustration, larger than it, kept inside the page
        auto ov = juce::Rectangle<int> (0, 0, 380, std::max (330, cabView->getHeight() + 100)).withCentre (cabView->getBounds().getCentre());
        grid->setBounds (ov.constrainedWithin (getLocalBounds().reduced (4)));
    }
    // ---- EQ
    {
        auto h = eq.header() + eq.getPosition();
        eqOn.setBounds (h.removeFromLeft (24).withSizeKeepingCentre (24, 24));
        eqGraph->setBounds (eq.content() + eq.getPosition());
    }
    // ---- room
    {
        auto h = room.header() + room.getPosition();
        roomOn.setBounds (h.removeFromLeft (24).withSizeKeepingCentre (24, 24));
        auto c = room.content() + room.getPosition();
        roomType.setBounds (c.removeFromTop (28));
        c.removeFromTop (10);
        placeRow ({ &roomSize, &roomChar }, c.withSizeKeepingCentre (c.getWidth(), std::min (c.getHeight(), 130)), std::min (110, c.getWidth() / 2 - 8));
    }
}

void CabSpacePage::showGrid (bool show, bool pin)
{
    gridPinned = show && pin;
    if (show)
    {
        grid->setVisible (true);
        grid->toFront (gridPinned);
        if (! gridPinned) startTimerHz (10); else stopTimer();
    }
    else
    {
        grid->setVisible (false);
        stopTimer();
    }
    grid->repaint();
}

void CabSpacePage::timerCallback()
{
    // hover mode: close once the pointer has left both the cabinet and the overlay
    if (gridPinned || ! grid->isVisible()) { stopTimer(); return; }
    const auto mouse = getMouseXYRelative();
    if (! grid->getBounds().contains (mouse) && ! cabView->getBounds().contains (mouse))
        showGrid (false, false);
}

void CabSpacePage::tick()
{
    cabView->repaint();
    static const char* rooms[] = { "short, controlled reflections", "natural room presence", "bigger space, longer decay" };
    const int t = juce::jlimit (0, 2, (int) std::lround (s.getRawParameterValue ("room_type")->load()));
    if (room.subtitle != rooms[t]) { room.subtitle = rooms[t]; room.repaint(); }
}

// ============================================================================ MIXER
MixerPanel::MixerPanel (BassForgeProcessor& p, juce::ValueTree u)
    : proc (p), s (p.apvts), ui (std::move (u)),
      driveCh (s, "DRIVE", "fader_drive", "mute_drive", col::drive, &p.solo[0], "Solo: hear only the Drive channel. Solo is a listening aid and is never saved."),
      diCh (s, "DI", "fader_di", "mute_di", col::di, &p.solo[1], "Solo: hear only the clean DI. Solo is a listening aid and is never saved."),
      roomCh (s, "ROOM", "fader_room", "mute_room", col::room, &p.solo[2], "Solo: hear only the room sound. Solo is a listening aid and is never saved."),
      masterCh (s, "MASTER", "master", nullptr, col::master, nullptr, {}),
      mixEqOn (s, "mixeq_on", col::master), lvlOn (s, "lvl_on", col::master),
      lowBoost (s, "mx_lowboost", col::master, Knob::Size::Small, "LOW BOOST"), lowAtten (s, "mx_lowatten", col::master, Knob::Size::Small, "LOW CUT"),
      highBoost (s, "mx_highboost", col::master, Knob::Size::Small, "HI BOOST"), highAtten (s, "mx_highatten", col::master, Knob::Size::Small, "HI CUT"),
      highBw (s, "mx_highbw", col::master, Knob::Size::Small, "WIDTH"),
      lowFreq (s, "mx_lowfreq"), hbFreq (s, "mx_hbfreq"), haFreq (s, "mx_hafreq"),
      lvlAmount (s, "lvl_amount", col::master, Knob::Size::Small, "AMOUNT"), lvlGain (s, "lvl_gain", col::master, Knob::Size::Small, "GAIN"),
      ceiling (s, "tp_ceiling", col::master, Knob::Size::Small, "CEILING"),
      protect (s, "tp_on", "PROTECT", col::clip.darker (0.1f))
{
    mixEq.titleIndent = 30; limiter.titleIndent = 30;
    toggle.setTooltip ("Show or hide the mixer. The output meter stays visible.");
    toggle.onClick = [this] { ui.setProperty ("mixerOpen", ! isExpanded(), nullptr); if (onToggle) onToggle(); };
    masterMeter.clipThreshold = 0.999f;
    miniMeter.clipThreshold = 0.999f;
    masterMeter.setTooltip ("Output level, left and right. Red means the output touched full scale (click to clear).");
    miniMeter.setTooltip (masterMeter.getTooltip());
    outReadout.setFont (font (11.0f));
    outReadout.setJustificationType (juce::Justification::centred);
    outReadout.setColour (juce::Label::textColourId, col::textDim);
    lvlGr.setTooltip ("How much the leveller is smoothing the volume right now.");
    tpGr.setTooltip ("How much the peak protection is catching right now. Usually zero.");
    for (juce::Component* c : std::initializer_list<juce::Component*> { &toggle, &driveCh, &diCh, &roomCh, &masterCh, &masterMeter, &miniMeter, &outReadout, &mixEq, &limiter,
                                &mixEqOn, &lvlOn, &lowBoost, &lowAtten, &highBoost, &highAtten, &highBw, &lowFreq, &hbFreq, &haFreq,
                                &lvlAmount, &lvlGain, &ceiling, &protect, &lvlGr, &tpGr })
        addAndMakeVisible (c);
}

bool MixerPanel::isExpanded() const { return (bool) ui.getProperty ("mixerOpen", true); }

void MixerPanel::paint (juce::Graphics& g)
{
    g.setColour (col::bg.darker (0.025f));
    g.fillRect (getLocalBounds());
    g.setColour (col::panelEdge);
    g.drawVerticalLine (0, 0, (float) getHeight());
    if (isExpanded())
    {
        // strip background
        auto r = stripsArea.toFloat();
        drawModule (g, r);
        g.setColour (col::panelEdge);
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * 0.75f) - 1, r.getY() + 12, r.getBottom() - 12);
    }
}

void MixerPanel::resized()
{
    const bool open = isExpanded();
    toggle.setButtonText (open ? ">" : "<");
    for (juce::Component* c : std::initializer_list<juce::Component*> { &driveCh, &diCh, &roomCh, &masterCh, &masterMeter, &outReadout, &mixEq, &limiter,
                                &mixEqOn, &lvlOn, &lowBoost, &lowAtten, &highBoost, &highAtten, &highBw, &lowFreq, &hbFreq, &haFreq,
                                &lvlAmount, &lvlGain, &ceiling, &protect, &lvlGr, &tpGr })
        c->setVisible (open);
    miniMeter.setVisible (! open);
    auto r = getLocalBounds().reduced (10, 12);
    if (! open)
    {
        toggle.setBounds (r.removeFromTop (28));
        r.removeFromTop (12);
        miniMeter.setBounds (r.withSizeKeepingCentre (14, r.getHeight()));
        return;
    }
    auto top = r.removeFromTop (28);
    toggle.setBounds (top.removeFromLeft (28));
    r.removeFromTop (8);
    stripsArea = r.removeFromTop ((int) (r.getHeight() * 0.47f));
    auto strips = stripsArea.reduced (8, 10);
    const int w = strips.getWidth() / 4;
    driveCh.setBounds (strips.removeFromLeft (w));
    diCh.setBounds (strips.removeFromLeft (w));
    roomCh.setBounds (strips.removeFromLeft (w));
    auto m = strips;
    outReadout.setBounds (m.removeFromBottom (18));
    masterMeter.setBounds (m.removeFromRight (16).withTrimmedTop (28).withTrimmedBottom (26));
    masterCh.setBounds (m);
    masterCh.meter.setVisible (false);
    r.removeFromTop (10);
    const int blockH = (r.getHeight() - 10) / 2;
    mixEq.setBounds (r.removeFromTop (blockH));
    r.removeFromTop (10);
    limiter.setBounds (r);
    {
        auto h = mixEq.header() + mixEq.getPosition();
        mixEqOn.setBounds (h.removeFromLeft (24).withSizeKeepingCentre (22, 22));
        auto c = (mixEq.content() + mixEq.getPosition()).withTrimmedTop (-6);
        auto knobs = c.removeFromTop (c.getHeight() - 26);
        placeRow ({ &lowBoost, &lowAtten, &highBoost, &highBw, &highAtten }, knobs, std::min (66, knobs.getWidth() / 5));
        auto boxes = c.withTrimmedTop (2);
        const int bw = boxes.getWidth() / 3;
        lowFreq.setBounds (boxes.removeFromLeft (bw).reduced (3, 0));
        hbFreq.setBounds (boxes.removeFromLeft (bw).reduced (3, 0));
        haFreq.setBounds (boxes.reduced (3, 0));
    }
    {
        auto h = limiter.header() + limiter.getPosition();
        lvlOn.setBounds (h.removeFromLeft (24).withSizeKeepingCentre (22, 22));
        protect.setBounds (h.removeFromRight (84).withSizeKeepingCentre (84, 22));
        auto c = (limiter.content() + limiter.getPosition()).withTrimmedTop (-6);
        auto meters = c.removeFromBottom (18);
        lvlGr.setBounds (meters.removeFromLeft (meters.getWidth() / 2).reduced (2, 2));
        tpGr.setBounds (meters.reduced (2, 2));
        placeRow ({ &lvlAmount, &lvlGain, &ceiling }, c, std::min (76, c.getWidth() / 3));
    }
}

void MixerPanel::tick()
{
    auto& mt = proc.meters();
    driveCh.tick (mt.drive.load());
    diCh.tick (mt.di.load());
    roomCh.tick (mt.room.load());
    const float l = mt.outL.load(), r = mt.outR.load();
    masterCh.tick (std::max (l, r));
    masterMeter.push (l, r);
    miniMeter.push (l, r);
    const float pk = juce::Decibels::gainToDecibels (std::max (l, r), -100.0f);
    if (pk >= outPeakDb) { outPeakDb = pk; outHold = 60; }
    else if (--outHold <= 0) outPeakDb = std::max (pk, outPeakDb - 1.0f);
    outReadout.setText (outPeakDb <= -99.0f ? "-inf" : juce::String (outPeakDb, 1) + " dB", juce::dontSendNotification);
    outReadout.setColour (juce::Label::textColourId, outPeakDb > -0.1f ? col::clip : col::textDim);
    lvlGr.push (mt.levelGr.load());
    tpGr.push (mt.protectGr.load());
}
} // namespace bfui
