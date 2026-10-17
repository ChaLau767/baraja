// Herramienta de desarrollo: abre la interfaz, procesa unos segundos de señal
// y guarda una captura en PNG. Solo se compila con -DBARAJA_SNAPSHOT=ON.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc > 1 && juce::String (argv[1]) == "--compat")
    {
        // a session saved by Baraja 0.1: "delay" (depth) must become the window width
        BarajaProcessor proc;
        juce::XmlElement state ("BARAJA");
        auto* d = state.createNewChildElement ("PARAM"); d->setAttribute ("id", "delay"); d->setAttribute ("value", 3000.0);
        auto* s = state.createNewChildElement ("PARAM"); s->setAttribute ("id", "size");  s->setAttribute ("value", 333.0);
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (state, block);
        proc.setStateInformation (block.getData(), (int) block.getSize());
        const auto width = proc.apvts.getParameter ("width")->getCurrentValueAsText();
        const auto size  = proc.apvts.getParameter ("size")->getCurrentValueAsText();
        const bool ok = width == "3.00 s" && size == "333 ms";
        std::printf ("%s  0.1 session restored: width %s, size %s\n", ok ? "ok  " : "FAIL", width.toRawUTF8(), size.toRawUTF8());
        return ok ? 0 : 1;
    }
    const juce::String out = argc > 1 ? argv[1] : "baraja.png";
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const int width  = argc > 3 ? juce::String (argv[3]).getIntValue() : 1060;
    const int height = argc > 4 ? juce::String (argv[4]).getIntValue() : 760;
    const int outs   = argc > 5 ? juce::String (argv[5]).getIntValue() : 2;
    const bool spanish = argc > 6 && juce::String (argv[6]) == "es";

    auto proc = std::make_unique<BarajaProcessor>();
    proc->setPlayConfigDetails (2, outs, 48000.0, 512);
    if (spanish) proc->setLanguage (baraja::Lang::es);
    proc->prepareToPlay (48000.0, 512);
    proc->applyPreset (preset);

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
    editor->setSize (width, height);

    juce::AudioBuffer<float> buf (juce::jmax (2, outs), 512);
    juce::MidiBuffer midi;
    juce::Random rng (3);
    long long k = 0;
    for (int b = 0; b < 48000 * 4 / 512; ++b)
    {
        for (int i = 0; i < 512; ++i, ++k)
        {
            const double t = k / 48000.0;
            const double env = std::exp (-std::fmod (t, 0.6) * 5.0);
            const float s = (float) (0.4 * env * std::sin (2 * juce::MathConstants<double>::pi * (220.0 + 110.0 * std::floor (std::fmod (t / 0.6, 4.0))) * t));
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
            for (int ch = 2; ch < buf.getNumChannels(); ++ch) buf.setSample (ch, i, 0.0f);
        }
        proc->processBlock (buf, midi);
        if (b % 6 == 0)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }

    // dejar que el temporizador de la interfaz lea la telemetría
    for (int i = 0; i < 12; ++i)
    {
        proc->processBlock (buf, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }

    auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (out);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    editor.reset();
    return 0;
}
