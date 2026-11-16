// bf_host_smoke: loads the BUILT BASSFORGE.vst3 through JUCE's VST3 hosting layer (the same SDK
// path a DAW uses) and runs scripted host checks. Exit code 0 = all checks passed.
// (Adapted from the CrestForge host smoke test.)
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <random>

static int failures = 0;
#define EXPECT(c, msg) do { if (! (c)) { ++failures; std::printf ("  FAIL: %s\n", msg); } else std::printf ("  ok:   %s\n", msg); } while (0)

static std::unique_ptr<juce::AudioPluginInstance> load (juce::VST3PluginFormat& fmt, const juce::String& path, double sr, int block)
{
    juce::OwnedArray<juce::PluginDescription> types;
    fmt.findAllTypesForFile (types, path);
    if (types.isEmpty()) return nullptr;
    juce::String err;
    auto inst = fmt.createInstanceFromDescription (*types[0], sr, block, err);
    if (! inst) std::printf ("  load error: %s\n", err.toRawUTF8());
    return inst;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: bf_host_smoke path/to/BASSFORGE.vst3 [--editor]\n"); return 2; }
    const juce::String path (argv[1]);
    const bool testEditor = argc > 2 && juce::String (argv[2]) == "--editor";
    juce::VST3PluginFormat fmt;

    std::printf ("[HOST-01] scan + load as VST3 effect\n");
    auto p = load (fmt, path, 48000.0, 512);
    EXPECT (p != nullptr, "plug-in instance created");
    if (! p) return 1;
    auto desc = p->getPluginDescription();
    std::printf ("  name=%s manufacturer=%s category=%s version=%s\n", desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(), desc.category.toRawUTF8(), desc.version.toRawUTF8());

    std::printf ("[HOST-02] bus layouts\n");
    {
        auto mk = [] (juce::AudioChannelSet i, juce::AudioChannelSet o) { juce::AudioProcessor::BusesLayout l; l.inputBuses.add (i); l.outputBuses.add (o); return l; };
        EXPECT (p->checkBusesLayoutSupported (mk (juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())), "stereo -> stereo");
        EXPECT (p->checkBusesLayoutSupported (mk (juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo())), "mono -> stereo (typical bass track)");
        EXPECT (p->checkBusesLayoutSupported (mk (juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())), "mono -> mono");
        EXPECT (! p->checkBusesLayoutSupported (mk (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())), "5.1 not advertised");
    }

    auto& params = p->getParameters();
    std::printf ("[HOST-03] %d host parameters exposed\n", params.size());
    EXPECT (params.size() >= 90, "full parameter set exposed");
    auto findParam = [&] (const char* name) -> juce::AudioProcessorParameter* {
        for (auto* q : params) if (q->getName (64) == name) return q;
        std::printf ("  (parameter '%s' not found)\n", name);
        return nullptr; };
    auto setPlain = [&] (const char* name, float norm) { if (auto* q = findParam (name)) q->setValueNotifyingHost (norm); };

    std::printf ("[HOST-04] sample-rate / block-size lifecycle + latency alignment (DI impulse lands on reported latency)\n");
    // DI only, every block neutral; crossover corners far apart so the DI's phase match is a near-pure delay at the peak
    setPlain ("Drive Mute", 1); setPlain ("Room Mute", 1); setPlain ("DI Comp On", 0); setPlain ("DI EQ On", 0);
    setPlain ("Mix EQ On", 0); setPlain ("Leveller On", 0); setPlain ("Peak Protect", 0);
    setPlain ("DI Fader", 60.0f / 66.0f); setPlain ("Low Split", 0.0f); setPlain ("High Split", 1.0f);
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int block : { 32, 64, 128, 256, 512 })
        {
            p->releaseResources();
            p->setRateAndBufferSizeDetails (sr, block);
            p->prepareToPlay (sr, block);
            const int lat = p->getLatencySamples();
            const int N = lat + 8192;
            juce::AudioBuffer<float> io (2, N); io.clear();
            io.setSample (0, 2000, 0.5f); io.setSample (1, 2000, 0.5f);
            juce::MidiBuffer midi;
            for (int pos = 0; pos < N; pos += block)
            {
                const int n = std::min (block, N - pos);
                juce::AudioBuffer<float> sub (io.getArrayOfWritePointers(), 2, pos, n);
                p->processBlock (sub, midi);
            }
            int pk = 0; for (int i = 0; i < N; ++i) if (std::abs (io.getSample (0, i)) > std::abs (io.getSample (0, pk))) pk = i;
            const bool ok = std::abs (pk - 2000 - lat) <= 2;
            if (! ok || block == 512)
                std::printf ("  sr=%6.0f block=%3d latency=%4d (%.2f ms)  impulse at +%d %s\n", sr, block, lat, 1000.0 * lat / sr, pk - 2000, ok ? "" : "<-- FAIL");
            if (! ok) ++failures;
        }
    for (auto* q : params) q->setValueNotifyingHost (q->getDefaultValue());

    std::printf ("[HOST-05] hot input, random block sizes, defaults (protection on)\n");
    {
        p->releaseResources(); p->prepareToPlay (48000.0, 1024);
        if (auto* d = findParam ("Input")) d->setValueNotifyingHost ((18.0f + 24.0f) / 48.0f);
        std::mt19937 rng (3); std::normal_distribution<float> nd (0.f, 0.5f); std::uniform_int_distribution<int> bl (1, 1024);
        float worst = 0; bool finite = true; int done = 0;
        juce::MidiBuffer midi;
        while (done < 48000 * 6)
        {
            const int n = bl (rng);
            juce::AudioBuffer<float> b (2, n);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) b.setSample (c, i, nd (rng));
            p->processBlock (b, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) { finite = finite && std::isfinite (b.getSample (c, i)); if (done > 48000) worst = std::max (worst, std::abs (b.getSample (c, i))); }
            done += n;
        }
        std::printf ("  hottest output sample %.3f dBFS (ceiling -1.0)\n", juce::Decibels::gainToDecibels (worst));
        EXPECT (finite, "finite output under random block sizes");
        EXPECT (juce::Decibels::gainToDecibels (worst) <= -0.9f, "sample peak contained");
        for (auto* q : params) q->setValueNotifyingHost (q->getDefaultValue());
    }

    std::printf ("[HOST-06] automation storm (every parameter, every block)\n");
    {
        std::mt19937 rng (9); std::uniform_real_distribution<float> u (0, 1);
        juce::MidiBuffer midi; bool finite = true;
        for (int k = 0; k < 2000; ++k)
        {
            for (auto* q : params) if (q != p->getBypassParameter() && u (rng) < 0.3f) q->setValue (u (rng));
            juce::AudioBuffer<float> b (2, 128);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) b.setSample (c, i, 0.5f * (u (rng) * 2 - 1));
            p->processBlock (b, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 128; ++i) finite = finite && std::isfinite (b.getSample (c, i));
        }
        EXPECT (finite, "finite output during random automation of all parameters");
    }

    std::printf ("[HOST-07] state: save -> new instance -> restore\n");
    {
        std::mt19937 rng (17); std::uniform_real_distribution<float> u (0, 1);
        for (auto* q : params) if (q != p->getBypassParameter()) q->setValueNotifyingHost (u (rng));
        juce::MemoryBlock state; p->getStateInformation (state);
        auto p2 = load (fmt, path, 48000.0, 512);
        p2->setStateInformation (state.getData(), (int) state.getSize());
        int mismatches = 0;
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i] == p->getBypassParameter()) continue;
            float a = params[i]->getValue(), b = p2->getParameters()[i]->getValue();
            const int steps = params[i]->getNumSteps();
            if (params[i]->isDiscrete() && steps > 1 && steps < 1000)
            { a = std::round (a * (steps - 1)) / (steps - 1); b = std::round (b * (steps - 1)) / (steps - 1); }
            if (std::abs (a - b) > 1e-5f) { ++mismatches; std::printf ("  mismatch %s: %f vs %f\n", params[i]->getName (64).toRawUTF8(), a, b); }
        }
        std::printf ("  state size %d bytes\n", (int) state.getSize());
        EXPECT (mismatches == 0, "every host parameter restored exactly");
        juce::MemoryBlock junk (state); for (size_t i = 8; i < junk.getSize(); i += 7) static_cast<char*> (junk.getData())[i] ^= 0x5a;
        p2->setStateInformation (junk.getData(), (int) junk.getSize());
        p2->setStateInformation (nullptr, 0);
        EXPECT (true, "corrupted / empty state accepted without crash");
        for (auto* q : params) q->setValueNotifyingHost (q->getDefaultValue());
    }

    std::printf ("[HOST-08] host bypass is latency-aligned dry\n");
    if (auto* bp = p->getBypassParameter())
    {
        p->releaseResources(); p->prepareToPlay (48000.0, 512);
        bp->setValueNotifyingHost (1.0f);
        const int lat = p->getLatencySamples();
        juce::AudioBuffer<float> io (2, 48000); std::mt19937 rng (4); std::normal_distribution<float> nd (0.f, 0.5f);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 48000; ++i) io.setSample (c, i, nd (rng));
        juce::AudioBuffer<float> ref (io); juce::MidiBuffer midi;
        for (int pos = 0; pos < 48000; pos += 512) { juce::AudioBuffer<float> sub (io.getArrayOfWritePointers(), 2, pos, std::min (512, 48000 - pos)); p->processBlock (sub, midi); }
        int diff = 0; for (int i = 4800; i < 48000; ++i) if (io.getSample (0, i) != ref.getSample (0, i - lat)) ++diff;
        EXPECT (diff == 0, "settled host bypass = input delayed by the reported latency");
        bp->setValueNotifyingHost (0.0f);
    }
    else std::printf ("  (host has no bypass parameter for this plug-in; skipped)\n");

    if (testEditor)
    {
        std::printf ("[HOST-09] editor open/close/resize x30 during processing\n");
        for (int k = 0; k < 30; ++k)
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditorIfNeeded());
            if (! ed) { EXPECT (false, "editor created"); break; }
            ed->setSize (900 + k * 10, 540 + k * 6);
            juce::MidiBuffer midi; juce::AudioBuffer<float> b (2, 256); b.clear(); p->processBlock (b, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
            p->editorBeingDeleted (ed.get());
        }
        EXPECT (true, "editor opened/resized/closed 30 times");
    }

    p->releaseResources();
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "HOST SMOKE PASSED" : "HOST SMOKE FAILED", failures);
    return failures == 0 ? 0 : 1;
}
