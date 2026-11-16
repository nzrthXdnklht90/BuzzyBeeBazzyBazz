// bf_ui_capture: hosts the real BASSFORGE processor + editor headlessly, feeds a bass phrase so the
// meters move, and writes screenshots of every page/state. Usage: bf_ui_capture OUT_DIR (xvfb-run on Linux)
#include "PluginEditor.h"

namespace
{
template <typename T> T* findChild (juce::Component* c)
{
    if (auto* t = dynamic_cast<T*> (c)) return t;
    for (auto* ch : c->getChildren()) if (auto* t = findChild<T> (ch)) return t;
    return nullptr;
}

void feed (BassForgeProcessor& p, int blocks)
{
    static double ph = 0, t = 0;
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        juce::AudioBuffer<float> buf (2, 512);
        for (int i = 0; i < 512; ++i)
        {
            const double env = std::exp (-std::fmod (t, 0.5) * 3.0);
            float v = 0;
            for (int h = 1; h < 12; ++h) v += (float) (std::sin (ph * h) / h);
            v *= (float) (0.12 * env);
            buf.setSample (0, i, v); buf.setSample (1, i, v);
            ph += 2 * juce::MathConstants<double>::pi * 41.2 / 48000.0; t += 1.0 / 48000.0;
        }
        p.processBlock (buf, midi);
    }
}

void shot (BassForgeEditor& ed, BassForgeProcessor& p, const juce::File& dir, const juce::String& name)
{
    for (int k = 0; k < 20; ++k) { feed (p, 3); juce::MessageManager::getInstance()->runDispatchLoopUntil (20); }
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    auto f = dir.getChildFile (name + ".png");
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File dir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("ui_captures"));
    dir.createDirectory();
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& p = *dynamic_cast<BassForgeProcessor*> (base.get());
    p.setRateAndBufferSizeDetails (48000, 512);
    p.prepareToPlay (48000, 512);

    auto make = [&] { return std::unique_ptr<BassForgeEditor> (dynamic_cast<BassForgeEditor*> (p.createEditor())); };
    {
        auto ed = make(); ed->setVisible (true);
        shot (*ed, p, dir, "01_tone_default");
        p.presets.load ("Modern Tight");
        p.uiState.setProperty ("drvAdv", true, nullptr); p.uiState.setProperty ("diAdv", true, nullptr);
    }
    {
        auto ed = make(); ed->setVisible (true);
        shot (*ed, p, dir, "02_tone_advanced_modern");
        p.uiState.setProperty ("page", 1, nullptr);
    }
    {
        auto ed = make(); ed->setVisible (true);
        shot (*ed, p, dir, "03_cab_space");
        if (auto* cp = findChild<bfui::CabSpacePage> (ed.get())) cp->showGrid (true, true);
        shot (*ed, p, dir, "04_cab_mic_grid");
        p.uiState.setProperty ("mixerOpen", false, nullptr);
    }
    {
        auto ed = make(); ed->setVisible (true);
        shot (*ed, p, dir, "05_cab_mixer_collapsed");
        p.uiState.setProperty ("mixerOpen", true, nullptr);
        p.uiState.setProperty ("page", 0, nullptr);
        p.uiState.setProperty ("scale", 0.75, nullptr);
    }
    {
        auto ed = make(); ed->setVisible (true);
        shot (*ed, p, dir, "06_tone_scale75");
    }
    p.releaseResources();
    return 0;
}
