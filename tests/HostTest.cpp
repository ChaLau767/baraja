// Development test: loads the built VST3 the way a DAW does and checks
// channel layouts up to 64 outputs, memory reallocation while audio runs,
// and loading a session saved by Baraja 0.1. Only built with -DBARAJA_SNAPSHOT=ON.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <cstdio>

static int failures = 0;
static const bool unbuffered = [] { std::setvbuf (stdout, nullptr, _IONBF, 0); return true; }();
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: baraja_hosttest <path to Baraja.vst3>\n"); return 2; }

    juce::AudioPluginFormatManager formats;
    formats.addFormat (new juce::VST3PluginFormat());
    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, argv[1]);
    check (found.size() == 1, "plugin found in the VST3 bundle");
    if (found.isEmpty()) return 1;

    juce::String error;
    auto plugin = formats.createPluginInstance (*found[0], 48000.0, 512, error);
    check (plugin != nullptr, "plugin instance created " + error);
    if (plugin == nullptr) return 1;

    // 0. which multichannel layouts survive the VST3 round trip?
    {
        juce::Array<juce::AudioChannelSet::ChannelType> named;
        for (int t = juce::AudioChannelSet::left; t <= juce::AudioChannelSet::discreteChannel0; ++t)
        {
            const auto type = (juce::AudioChannelSet::ChannelType) t;
            if (type >= juce::AudioChannelSet::ambisonicACN0 && type <= juce::AudioChannelSet::ambisonicACN35) continue;
            if (juce::AudioChannelSet::getAbbreviatedChannelTypeName (type).isNotEmpty() && type != juce::AudioChannelSet::discreteChannel0)
                named.add (type);
        }
        int maxNamed = 0;
        for (int n = 3; n <= named.size(); ++n)
        {
            juce::AudioChannelSet set;
            for (int i = 0; i < n; ++i) set.addChannel (named[i]);
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo());
            layout.outputBuses.add (set);
            if (plugin->setBusesLayout (layout) && plugin->getTotalNumOutputChannels() == n) maxNamed = n;
        }
        std::printf ("info  named speaker layouts accepted up to %d outputs (of %d named types)\n", maxNamed, named.size());
        for (int order = 1; order <= 7; ++order)
        {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add (juce::AudioChannelSet::stereo());
            layout.outputBuses.add (juce::AudioChannelSet::ambisonic (order));
            const bool ok = plugin->setBusesLayout (layout);
            std::printf ("info  ambisonic order %d (%d ch): %s\n", order, (order + 1) * (order + 1), ok ? "accepted" : "rejected");
        }
    }

    // 1. layouts the VST3 format can carry: mono, stereo, named speaker sets and 64 channels (7th-order ambisonic arrangement)
    juce::Array<juce::AudioChannelSet::ChannelType> namedTypes;
    for (int t = juce::AudioChannelSet::left; t < juce::AudioChannelSet::discreteChannel0; ++t)
    {
        const auto type = (juce::AudioChannelSet::ChannelType) t;
        if (type >= juce::AudioChannelSet::ambisonicACN0 && type <= juce::AudioChannelSet::ambisonicACN35) continue;
        if (juce::AudioChannelSet::getAbbreviatedChannelTypeName (type).isNotEmpty()) namedTypes.add (type);
    }
    for (int outs : { 1, 2, 4, 8, 16, 24, 33, 64 })
    {
        juce::AudioChannelSet set;
        if (outs == 1) set = juce::AudioChannelSet::mono();
        else if (outs == 2) set = juce::AudioChannelSet::stereo();
        else if (outs == 64) set = juce::AudioChannelSet::ambisonic (7);
        else for (int i = 0; i < outs; ++i) set.addChannel (namedTypes[i]);
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (set);
        const bool accepted = plugin->setBusesLayout (layout);
        check (accepted && plugin->getTotalNumOutputChannels() == outs,
               "layout with " + juce::String (outs) + " outputs accepted (" + juce::String (plugin->getTotalNumOutputChannels()) + ")");
        if (! accepted) continue;

        plugin->prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (juce::jmax (2, outs), 512);
        juce::MidiBuffer midi;
        std::vector<float> peak ((size_t) outs, 0.0f);
        bool finite = true;
        long long k = 0;
        for (int b = 0; b < 48000 * 3 / 512; ++b)
        {
            buf.clear();
            for (int i = 0; i < 512; ++i, ++k)
            {
                const float s = (float) (0.4 * std::sin (k * 2.0 * juce::MathConstants<double>::pi * 330.0 / 48000.0));
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
            }
            plugin->processBlock (buf, midi);
            for (int ch = 0; ch < outs; ++ch)
                for (int i = 0; i < 512; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) finite = false;
                    peak[(size_t) ch] = juce::jmax (peak[(size_t) ch], std::abs (v));
                }
        }
        int active = 0;
        for (auto p : peak) if (p > 1.0e-4f) ++active;
        check (finite && active > 0, "  audio on " + juce::String (active) + " of " + juce::String (outs) + " outputs");
        plugin->releaseResources();
    }

    // 2. change the memory length while audio is running on another thread
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::create7point1());
        check (plugin->setBusesLayout (layout), "8-channel layout for the memory test");
        plugin->prepareToPlay (48000.0, 256);

        juce::AudioProcessorParameter* memory = nullptr;
        for (auto* p : plugin->getParameters())
            if (p->getName (64) == "Memory") memory = p;
        check (memory != nullptr, "memory parameter visible to the host");

        std::atomic<bool> running { true }, bad { false };
        std::thread audio ([&] {
            juce::AudioBuffer<float> buf (juce::jmax (2, plugin->getTotalNumOutputChannels()), 256);
            juce::MidiBuffer midi;
            juce::Random r;
            while (running)
            {
                for (int i = 0; i < 256; ++i) { const float s = r.nextFloat() * 0.2f - 0.1f; buf.setSample (0, i, s); buf.setSample (1, i, s); }
                plugin->processBlock (buf, midi);
                for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                    for (int i = 0; i < 256; ++i)
                        if (! std::isfinite (buf.getSample (ch, i))) bad = true;
                std::this_thread::sleep_for (std::chrono::microseconds (500));
            }
        });

        if (memory != nullptr)
            for (int round = 0; round < 12; ++round)
            {
                memory->setValueNotifyingHost ((round % 5) / 4.0f);   // 5, 10, 20, 30, 60 s
                juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
            }
        running = false;
        audio.join();
        check (! bad, "memory resized 12 times while processing, no invalid samples");
        plugin->releaseResources();
    }

    // 3. the session state survives a save and reload in a fresh instance
    {
        for (auto* p : plugin->getParameters())
            if (p->getName (64) == "Size") p->setValueNotifyingHost (0.8f);
        juce::MemoryBlock block;
        plugin->getStateInformation (block);
        auto second = formats.createPluginInstance (*found[0], 48000.0, 512, error);
        second->setStateInformation (block.getData(), (int) block.getSize());
        juce::String a, b;
        for (auto* p : plugin->getParameters()) if (p->getName (64) == "Size") a = p->getCurrentValueAsText();
        for (auto* p : second->getParameters()) if (p->getName (64) == "Size") b = p->getCurrentValueAsText();
        check (a.isNotEmpty() && a == b, "state restored in a new instance (size " + b + ")");
    }

    // 4. the editor opens and closes
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
        check (editor != nullptr && editor->getWidth() > 0, "editor opens (" + juce::String (editor ? editor->getWidth() : 0) + " px)");
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        editor.reset();
    }

    plugin.reset();
    std::printf (failures == 0 ? "\nall checks passed\n" : "\n%d checks failed\n", failures);
    return failures == 0 ? 0 : 1;
}
