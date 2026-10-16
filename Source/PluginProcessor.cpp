// Baraja — procesador
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstring>

BarajaProcessor::BarajaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Entrada", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Salida",  juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BARAJA", baraja::createLayout())
{
    for (const auto& s : baraja::floatSpecs())
        floatRefs.push_back ({ s.id, apvts.getRawParameterValue (s.id) });

    vdirParam   = apvts.getRawParameterValue ("vdir");
    quantParam  = apvts.getRawParameterValue ("quant");
    freezeParam = apvts.getRawParameterValue ("freeze");
}

bool BarajaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    if (in != juce::AudioChannelSet::stereo() && in != juce::AudioChannelSet::mono() && ! in.isDisabled()) return false;
    return true;
}

void BarajaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    inputCopy.setSize (2, juce::jmax (samplesPerBlock, 4096));
    samplesSinceTelemetry = 0;
}

baraja::Params BarajaProcessor::readParams() const noexcept
{
    baraja::Params p;
    auto get = [this] (const char* id) {
        for (const auto& r : floatRefs) if (std::strcmp (r.id, id) == 0) return r.value->load();
        return 0.0f;
    };
    p.size = get ("size");        p.sizeVar = get ("sizeVar");     p.density = get ("density");
    p.jitter = get ("jitter");    p.shape = get ("shape");         p.delay = get ("delay");
    p.pitch = get ("pitch");      p.pitchVar = get ("pitchVar");   p.drift = get ("drift");
    p.driftRate = get ("driftRate"); p.reverse = get ("reverse");  p.spread = get ("spread");
    p.ampVar = get ("ampVar");    p.mix = get ("mix");             p.gain = get ("gain");
    p.vdir   = (int) vdirParam->load();
    p.quant  = (int) quantParam->load();
    p.freeze = freezeParam->load() > 0.5f;
    return p;
}

void BarajaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const auto p = readParams();

    // Copia de la entrada: en muchos hosts entrada y salida comparten el mismo búfer.
    // (el tamaño se reserva en prepareToPlay; solo crece si el host manda un bloque más grande de lo anunciado)
    auto& input = inputCopy;
    input.setSize (2, n, false, false, true);
    if (numIn >= 1) input.copyFrom (0, 0, buffer, 0, 0, n); else input.clear (0, 0, n);
    if (numIn >= 2) input.copyFrom (1, 0, buffer, 1, 0, n); else input.copyFrom (1, 0, input, 0, 0, n);

    float* outL = buffer.getWritePointer (0);
    float* outR = numOut >= 2 ? buffer.getWritePointer (1) : nullptr;
    engine.process (input.getReadPointer (0), input.getReadPointer (1), outL, outR, n, p);

    for (int ch = 2; ch < numOut; ++ch) buffer.clear (ch, 0, n);

    samplesSinceTelemetry += n;
    if (samplesSinceTelemetry >= (int) (getSampleRate() / 30.0))
    {
        const juce::SpinLock::ScopedTryLockType lock (telemetryLock);
        if (lock.isLocked())
        {
            engine.snapshot (telemetry, p);
            samplesSinceTelemetry = 0;
        }
    }
}

bool BarajaProcessor::readTelemetry (baraja::Telemetry& dest)
{
    const juce::SpinLock::ScopedLockType lock (telemetryLock);
    const bool fresh = telemetry.peaksFresh;
    dest.numGrains = telemetry.numGrains;
    std::copy (telemetry.grains, telemetry.grains + telemetry.numGrains, dest.grains);
    dest.drift = telemetry.drift;
    dest.outPeak = telemetry.outPeak;
    telemetry.outPeak = 0.0f;
    if (fresh)
    {
        std::copy (telemetry.peaks, telemetry.peaks + baraja::Telemetry::numPeaks, dest.peaks);
        dest.displayMs = telemetry.displayMs;
        telemetry.peaksFresh = false;
    }
    return fresh;
}

int BarajaProcessor::getNumPrograms() { return (int) baraja::presets().size(); }

const juce::String BarajaProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? baraja::presets()[(size_t) index].name : "";
}

void BarajaProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms())) applyPreset (index);
}

void BarajaProcessor::applyPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;

    auto set = [this] (const juce::String& id, float value) {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    // todo vuelve al valor inicial salvo mezcla, salida y congelado, que son decisiones de la sesión
    for (const auto& s : baraja::floatSpecs())
        if (juce::String (s.id) != "mix" && juce::String (s.id) != "gain") set (s.id, s.def);
    set ("vdir", 0.0f);
    set ("quant", 0.0f);

    for (const auto& [id, value] : baraja::presets()[(size_t) index].values) set (id, value);
}

void BarajaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void BarajaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            currentProgram = state.getProperty ("program", 0);
            apvts.replaceState (state);
        }
}

juce::AudioProcessorEditor* BarajaProcessor::createEditor() { return new BarajaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BarajaProcessor(); }
