#include "PluginEditor.h"

using namespace bfui;

namespace
{
class TabLnF : public bfui::LookAndFeel
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::text : (over ? col::track.darker (0.04f) : col::track));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setColour (b.getToggleState() ? col::panel : col::text);
        g.setFont (caps (11.5f));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
};
TabLnF& tabLnf() { static TabLnF l; return l; }
} // namespace

BassForgeEditor::BassForgeEditor (BassForgeProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      input (p.apvts, "in_gain", col::master, Knob::Size::Inline, "INPUT"),
      quality (p.apvts, "quality")
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (root);

    tone = std::make_unique<TonePage> (p, p.uiState);
    cabPage = std::make_unique<CabSpacePage> (p, p.uiState);
    mixer = std::make_unique<MixerPanel> (p, p.uiState);
    mixer->onToggle = [this] { layoutRoot(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &tabTone, &tabCab, &presetPrev, &presetNext, &presetName, &presetSave, &input,
                                &inputMeter, &quality, &qualityCaption, tone.get(), cabPage.get(), mixer.get() })
        root.addAndMakeVisible (c);
    for (auto* b : { &tabTone, &tabCab, &presetPrev, &presetNext, &presetName, &presetSave })
        b->setLookAndFeel (&tabLnf());
    tabTone.setTooltip ("TONE page: the amp (Drive) and the clean DI channel.");
    tabCab.setTooltip ("CAB & SPACE page: speaker cabinet, microphones, Drive EQ and room sound.");
    tabTone.onClick = [this] { setPage (0); };
    tabCab.onClick = [this] { setPage (1); };
    presetPrev.setTooltip ("Previous preset.");
    presetNext.setTooltip ("Next preset.");
    presetName.setTooltip ("Click to choose a preset. Presets never change your Quality setting.");
    presetSave.setTooltip ("Save your current sound as a user preset.");
    presetPrev.onClick = [this] { proc.presets.step (-1); };
    presetNext.onClick = [this] { proc.presets.step (1); };
    presetName.onClick = [this] { showPresetMenu(); };
    presetSave.onClick = [this] { savePresetDialog(); };
    inputMeter.setTooltip ("Input level. Aim for peaks around the middle of the meter; adjust with the Input knob.");
    qualityCaption.setText ("QUALITY", juce::dontSendNotification);
    qualityCaption.setFont (caps (9.5f));
    qualityCaption.setColour (juce::Label::textColourId, col::textDim);
    qualityCaption.setJustificationType (juce::Justification::centred);

    setPage ((int) p.uiState.getProperty ("page", 0));
    const double scale = juce::jlimit (0.7, 2.0, (double) p.uiState.getProperty ("scale", 1.0));   // read before limits trigger resized()
    setResizable (true, true);
    setResizeLimits (kW * 7 / 10, kH * 7 / 10, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kW / kH);
    setSize ((int) (kW * scale), (int) (kH * scale));
    startTimerHz (30);
}

BassForgeEditor::~BassForgeEditor()
{
    stopTimer();
    for (auto* b : { &tabTone, &tabCab, &presetPrev, &presetNext, &presetName, &presetSave }) b->setLookAndFeel (nullptr);
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void BassForgeEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void BassForgeEditor::resized()
{
    const float scale = (float) getWidth() / (float) kW;
    root.setTransform (juce::AffineTransform::scale (scale));
    root.setBounds (0, 0, kW, kH);
    proc.uiState.setProperty ("scale", scale, nullptr);
}

void BassForgeEditor::Root::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // wordmark (original, plain typography)
    g.setColour (col::text);
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)).withExtraKerningFactor (0.18f));
    g.drawText ("BASSFORGE", 18, 0, 200, 56, juce::Justification::centredLeft);
    g.setColour (col::drive);
    g.fillRoundedRectangle (18.0f, 40.0f, 28.0f, 3.0f, 1.5f);
    g.setColour (col::panelEdge);
    g.drawHorizontalLine (56, 0, (float) getWidth());
}

void BassForgeEditor::Root::resized() { ed.layoutRoot(); }

void BassForgeEditor::layoutRoot()
{
    auto r = root.getLocalBounds();
    auto top = r.removeFromTop (56).reduced (14, 12);
    top.removeFromLeft (196);
    tabTone.setBounds (top.removeFromLeft (92));
    top.removeFromLeft (6);
    tabCab.setBounds (top.removeFromLeft (124));
    auto right = top.removeFromRight (330);
    quality.setBounds (right.removeFromRight (110).withTrimmedTop (12));
    qualityCaption.setBounds (quality.getBounds().withY (quality.getY() - 14).withHeight (13));
    right.removeFromRight (16);
    inputMeter.setBounds (right.removeFromRight (10).reduced (0, 2));
    right.removeFromRight (4);
    input.setBounds (right.removeFromRight (150).withY (8).withHeight (40));
    auto preset = top.withSizeKeepingCentre (std::min (340, top.getWidth()), top.getHeight());
    presetPrev.setBounds (preset.removeFromLeft (32));
    presetSave.setBounds (preset.removeFromRight (60));
    preset.removeFromRight (4);
    presetNext.setBounds (preset.removeFromRight (32));
    presetName.setBounds (preset.reduced (4, 0));

    const int mw = mixer->isExpanded() ? MixerPanel::kExpandedW : MixerPanel::kCollapsedW;
    mixer->setBounds (r.removeFromRight (mw));
    mixer->resized();
    tone->setBounds (r);
    cabPage->setBounds (r);
    mixer->repaint();
}

void BassForgeEditor::setPage (int p)
{
    proc.uiState.setProperty ("page", p, nullptr);
    tone->setVisible (p == 0);          // pages only hide: audio is never interrupted
    cabPage->setVisible (p == 1);
    tabTone.setToggleState (p == 0, juce::dontSendNotification);
    tabCab.setToggleState (p == 1, juce::dontSendNotification);
    if (p != 1) cabPage->showGrid (false, false);
}

void BassForgeEditor::showPresetMenu()
{
    juce::PopupMenu m, user;
    const auto fac = proc.presets.getFactoryNames();
    const auto usr = proc.presets.getUserNames();
    int id = 1;
    m.addSectionHeader ("Factory");
    for (auto& n : fac) m.addItem (id++, n, true, n == proc.presets.getCurrentName());
    for (auto& n : usr) user.addItem (id++, n, true, n == proc.presets.getCurrentName());
    if (usr.size() > 0) m.addSubMenu ("User", user);
    m.addSeparator();
    m.addItem (1000, "Open user preset folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetName), [this, fac, usr] (int r) {
        if (r == 1000) { auto f = PresetManager::userFolder(); f.createDirectory(); f.revealToUser(); return; }
        if (r <= 0) return;
        const int i = r - 1;
        if (i < fac.size()) proc.presets.load (fac[i]);
        else if (i - fac.size() < usr.size()) proc.presets.load (usr[i - fac.size()]);
    });
}

void BassForgeEditor::savePresetDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name your sound:", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", proc.presets.getCurrentName());
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r) {
        if (r == 1 && saveWindow)
        {
            const auto name = saveWindow->getTextEditorContents ("name").trim();
            if (name.isNotEmpty()) proc.presets.saveUser (name);
        }
        saveWindow.reset();
    }), false);
}

void BassForgeEditor::timerCallback()
{
    ++tickCount;
    inputMeter.push (proc.meters().input.load());
    mixer->tick();
    if (tone->isVisible()) tone->tick();
    if (cabPage->isVisible() && (tickCount & 1)) cabPage->tick();
    const auto n = proc.presets.getCurrentName();
    if (presetName.getButtonText() != n) presetName.setButtonText (n);
}
