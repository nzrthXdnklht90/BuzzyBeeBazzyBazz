#include "PluginProcessor.h"
#include "PluginEditor.h"

BassForgeProcessor::BassForgeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BASSFORGE", bfparams::createLayout()),
      presets (apvts),
      bridge (apvts)
{
    uiState.setProperty ("page", 0, nullptr);
    uiState.setProperty ("mixerOpen", true, nullptr);
    uiState.setProperty ("eqBand", 3, nullptr);
    uiState.setProperty ("scale", 1.0, nullptr);
}

bool BassForgeProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void BassForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    bridge.fill (snapshot);
    engine.setParams (snapshot);
    engine.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (engine.getLatencySamples());
    const int maxLat = engine.latencyFor (bassforge::Quality::High) + 1;
    for (auto& l : bypassLine) l.assign ((size_t) maxLat + 1, 0.0);
    bypassPos = 0;
}

template <typename T>
void BassForgeProcessor::bypassT (juce::AudioBuffer<T>& buffer)
{
    const int lat = getLatencySamples();
    const int len = (int) bypassLine[0].size();
    if (len <= lat) return;
    const int nIn = std::max (1, getTotalNumInputChannels());
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const int rd = (bypassPos - lat + len) % len;
        for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
        {
            const int src = std::min (c, nIn - 1);
            bypassLine[c][(size_t) bypassPos] = (double) buffer.getSample (src, i);
        }
        for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
            buffer.setSample (c, i, (T) bypassLine[c][(size_t) rd]);
        bypassPos = (bypassPos + 1) % len;
    }
}

void BassForgeProcessor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { bypassT (b); }
void BassForgeProcessor::processBlockBypassed (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { bypassT (b); }

template <typename T>
void BassForgeProcessor::processT (juce::AudioBuffer<T>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    bridge.fill (snapshot);
    snapshot.soloDrive = solo[0].load (std::memory_order_relaxed);
    snapshot.soloDi = solo[1].load (std::memory_order_relaxed);
    snapshot.soloRoom = solo[2].load (std::memory_order_relaxed);
    engine.setParams (snapshot);

    const int numIn = std::max (1, getTotalNumInputChannels());
    const int numOut = std::max (1, getTotalNumOutputChannels());
    const int n = buffer.getNumSamples();
    const T* in[2] = { buffer.getReadPointer (0), buffer.getReadPointer (std::min (1, buffer.getNumChannels() - 1)) };
    T* out[2] = { buffer.getWritePointer (0), buffer.getWritePointer (std::min (1, buffer.getNumChannels() - 1)) };
    // the engine reads a whole chunk before writing it, so in-place processing is safe
    engine.process (in, std::min (numIn, 2), out, std::min (numOut, 2), n);

    // quality (oversampling) changes alter the latency; tell the host (JUCE notifies asynchronously)
    if (engine.getLatencySamples() != getLatencySamples())
        setLatencySamples (engine.getLatencySamples());
}

void BassForgeProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer&) { processT (b); }
void BassForgeProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer&) { processT (b); }

juce::AudioProcessorEditor* BassForgeProcessor::createEditor() { return new BassForgeEditor (*this); }

void BassForgeProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    state.setProperty ("preset", presets.getCurrentName(), nullptr);
    auto ui = state.getChildWithName ("UI");
    if (ui.isValid()) state.removeChild (ui, nullptr);
    state.appendChild (uiState.createCopy(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void BassForgeProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType())) return;
        auto ui = tree.getChildWithName ("UI");
        if (ui.isValid())
        {
            uiState.copyPropertiesFrom (ui, nullptr);
            tree.removeChild (ui, nullptr);
        }
        presets.setCurrentName (tree.getProperty ("preset", "Default").toString());
        apvts.replaceState (tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BassForgeProcessor(); }
