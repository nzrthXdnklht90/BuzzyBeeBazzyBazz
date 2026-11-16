#include "Presets.h"

// Factory presets: starting points written by ear from the brief's targets (section 20). They are
// gain-staged against the nominal DI level (about -12 dBFS peaks). They still need the listening
// pass with real basses that the brief requires before release (tracked in docs/M0_architecture.md).
const std::vector<PresetManager::Factory>& PresetManager::factory()
{
    static const std::vector<Factory> f = {
        { "Default", {} },
        { "Classic Vintage", { { "drv_model", 0 }, { "drv_drive", 3.5f }, { "drv_char", 4 }, { "cab_model", 1 }, { "mic1_type", 0 },
                               { "mic1_pos", 0.33f }, { "mic1_dist", 1 }, { "fader_di", -6 }, { "drv_lowmode", 1 }, { "room_type", 0 }, { "fader_room", -18 } } },
        { "Clean Studio", { { "drv_model", 1 }, { "drv_drive", 1.5f }, { "drv_char", 4 }, { "fader_drive", -8 }, { "fader_di", 0 },
                            { "di_comp", 5 }, { "di_treble", 1.5f }, { "cab_model", 3 }, { "mic1_type", 1 }, { "drv_lowmode", 0 }, { "fader_room", -24 } } },
        { "Warm Drive", { { "drv_model", 1 }, { "drv_drive", 5 }, { "drv_char", 3.5f }, { "cab_model", 1 }, { "mic1_type", 2 }, { "mic1_pos", 0.5f },
                          { "mic1_dist", 2 }, { "fader_di", -6 }, { "drv_lowmode", 1 }, { "eq3_gain", -2 }, { "eq3_freq", 350 } } },
        { "Punk Pick", { { "drv_model", 0 }, { "drv_drive", 6.5f }, { "drv_char", 7.5f }, { "drv_highlvl", 0 }, { "cab_model", 0 }, { "mic1_type", 0 },
                         { "mic1_pos", 0.1f }, { "eq4_gain", 2.5f }, { "eq4_freq", 900 }, { "fader_di", -4 }, { "room_type", 0 }, { "fader_room", -20 } } },
        { "Modern Tight", { { "drv_model", 2 }, { "drv_drive", 5 }, { "drv_char", 6 }, { "drv_xlow", 160 }, { "cab_model", 3 }, { "mic1_type", 0 },
                            { "mic1_pos", 0.2f }, { "mic2_on", 1 }, { "mic2_type", 1 }, { "mic2_dist", 3 }, { "eq3_gain", -3 }, { "eq3_freq", 300 },
                            { "eq3_q", 1.4f }, { "fader_di", -2 }, { "drv_lowmode", 0 }, { "mixeq_on", 1 }, { "mx_lowboost", 2 }, { "mx_lowfreq", 2 } } },
        { "Heavy Grind", { { "drv_model", 2 }, { "drv_drive", 8 }, { "drv_char", 7 }, { "drv_xlow", 200 }, { "drv_highlvl", -6 }, { "cab_model", 3 },
                           { "mic1_type", 0 }, { "mic1_pos", 0.33f }, { "eq6_gain", -3 }, { "eq6_freq", 4000 }, { "eq3_gain", -4 }, { "eq3_freq", 280 },
                           { "fader_di", -1 }, { "drv_lowmode", 0 }, { "lvl_amount", 3 } } },
        { "Studio Blend", { { "drv_model", 0 }, { "drv_drive", 4.5f }, { "fader_drive", -3 }, { "fader_di", -3 }, { "drv_lowmode", 1 }, { "mic2_on", 1 },
                            { "mic2_type", 2 }, { "mic2_dist", 3 }, { "mic2_pos", 0.66f }, { "mic_bal", 0 } } },
        { "Roomy Vintage", { { "drv_model", 0 }, { "drv_drive", 4 }, { "drv_char", 4 }, { "cab_model", 1 }, { "mic1_type", 2 }, { "mic1_dist", 3 },
                             { "room_type", 1 }, { "room_size", 6 }, { "fader_room", -8 }, { "fader_di", -6 }, { "drv_lowmode", 1 } } },
        { "Deep Low B", { { "drv_model", 2 }, { "drv_drive", 4 }, { "drv_char", 5 }, { "drv_xlow", 180 }, { "cab_model", 2 }, { "drv_lowcomp", 7 },
                          { "di_comp", 5 }, { "fader_di", 0 }, { "drv_lowmode", 0 }, { "eq1_type", 2 }, { "eq1_freq", 28 }, { "eq1_q", 0.7f },
                          { "eq1_gain", 0 }, { "mixeq_on", 1 }, { "mx_lowboost", 1.5f }, { "mx_lowatten", 2 }, { "mx_lowfreq", 1 } } },
    };
    return f;
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("BASSFORGE").getChildFile ("Presets");
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray n;
    for (auto& f : factory()) n.add (f.name);
    return n;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray n;
    for (auto& f : userFolder().findChildFiles (juce::File::findFiles, false, "*.bfpreset"))
        n.add (f.getFileNameWithoutExtension());
    n.sortNatural();
    return n;
}

void PresetManager::setParam (const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

void PresetManager::resetToDefaults()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "quality") continue;
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
}

bool PresetManager::load (const juce::String& name)
{
    for (auto& f : factory())
        if (name == f.name)
        {
            resetToDefaults();
            for (auto& [id, v] : f.values) setParam (id, v);
            current = name;
            return true;
        }
    auto file = userFolder().getChildFile (name + ".bfpreset");
    if (auto xml = juce::XmlDocument::parse (file))
    {
        resetToDefaults();
        for (auto* e : xml->getChildWithTagNameIterator ("P"))
        {
            const auto id = e->getStringAttribute ("id");
            if (id == "quality") continue;
            setParam (id, (float) e->getDoubleAttribute ("v"));
        }
        current = name;
        return true;
    }
    return false;
}

bool PresetManager::saveUser (const juce::String& name)
{
    auto folder = userFolder();
    folder.createDirectory();
    juce::XmlElement root ("BASSFORGE_PRESET");
    root.setAttribute ("version", 1);
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            if (rp->getParameterID() == "quality") continue;
            auto* e = root.createNewChildElement ("P");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("v", rp->convertFrom0to1 (rp->getValue()));
        }
    const bool ok = root.writeTo (folder.getChildFile (juce::File::createLegalFileName (name) + ".bfpreset"));
    if (ok) current = name;
    return ok;
}

bool PresetManager::deleteUser (const juce::String& name)
{
    return userFolder().getChildFile (name + ".bfpreset").deleteFile();
}

void PresetManager::step (int delta)
{
    auto names = getFactoryNames();
    names.addArray (getUserNames());
    if (names.isEmpty()) return;
    int idx = names.indexOf (current);
    idx = ((idx < 0 ? 0 : idx + delta) % names.size() + names.size()) % names.size();
    load (names[idx]);
}
