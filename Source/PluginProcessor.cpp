// Baraja — processor
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // The standalone app offers up to 64 outputs; in a DAW the track decides.
    juce::AudioChannelSet defaultOutputs()
    {
        return juce::JUCEApplicationBase::isStandaloneApp() ? juce::AudioChannelSet::discreteChannels (baraja::Engine::maxOutputs)
                                                            : juce::AudioChannelSet::stereo();
    }
}

BarajaProcessor::BarajaProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", defaultOutputs(), true)),
      apvts (*this, nullptr, "BARAJA", baraja::createLayout())
{
    auto r = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    refs = { r ("size"), r ("sizeVar"), r ("density"), r ("jitter"), r ("shape"), r ("position"), r ("width"), r ("order"), r ("speed"), r ("pitch"),
             r ("pitchVar"), r ("drift"), r ("driftRate"), r ("reverse"), r ("spread"), r ("center"), r ("rotation"),
             r ("ampVar"), r ("mix"), r ("gain"), r ("vdir"), r ("quant"), r ("spaceMode"), r ("freeze"), r ("memory") };

    apvts.addParameterListener ("memory", this);
    if (! apvts.state.hasProperty ("lang")) apvts.state.setProperty ("lang", 0, nullptr);
}

BarajaProcessor::~BarajaProcessor()
{
    apvts.removeParameterListener ("memory", this);
    cancelPendingUpdate();
}

double BarajaProcessor::memorySeconds() const
{
    const auto& choices = baraja::memoryChoices();
    const int index = juce::jlimit (0, (int) choices.size() - 1, (int) refs.memory->load());
    return (double) choices[(size_t) index];
}

bool BarajaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const int outs = layouts.getMainOutputChannels();
    const int ins  = layouts.getMainInputChannels();
    // any output count from 1 to 64; extra inputs beyond the first two are ignored
    return outs >= 1 && outs <= baraja::Engine::maxOutputs && ins >= 0 && ins <= baraja::Engine::maxOutputs;
}

void BarajaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const juce::SpinLock::ScopedLockType lock (engineLock);
    allocatedSeconds = memorySeconds();
    engine.prepare (sampleRate, allocatedSeconds);
    inputCopy.setSize (2, juce::jmax (samplesPerBlock, 4096));
    samplesSinceTelemetry = 0;
}

void BarajaProcessor::parameterChanged (const juce::String& id, float)
{
    // may arrive on any thread: the reallocation always happens on the message thread
    if (id == "memory") triggerAsyncUpdate();
}

void BarajaProcessor::handleAsyncUpdate() { reallocateMemory(); }

void BarajaProcessor::reallocateMemory()
{
    const double seconds = memorySeconds();
    if (std::abs (seconds - allocatedSeconds) < 0.5 || getSampleRate() <= 0.0) return;

    baraja::Engine fresh;
    fresh.prepare (getSampleRate(), seconds);          // allocate outside the lock
    {
        const juce::SpinLock::ScopedLockType lock (engineLock);
        std::swap (engine, fresh);                     // O(1): swaps the vectors
        allocatedSeconds = seconds;
    }
    // "fresh" now holds the old memory and is freed here, on the message thread
}

baraja::Params BarajaProcessor::readParams() const noexcept
{
    baraja::Params p;
    p.size = refs.size->load();           p.sizeVar = refs.sizeVar->load();     p.density = refs.density->load();
    p.jitter = refs.jitter->load();       p.shape = refs.shape->load();         p.position = refs.position->load();
    p.width = refs.width->load();         p.order = refs.order->load();         p.speed = refs.speed->load();
            p.pitch = refs.pitch->load();         p.pitchVar = refs.pitchVar->load();
    p.drift = refs.drift->load();         p.driftRate = refs.driftRate->load(); p.reverse = refs.reverse->load();
    p.spread = refs.spread->load();       p.center = refs.center->load();       p.rotation = refs.rotation->load();
    p.ampVar = refs.ampVar->load();       p.mix = refs.mix->load();             p.gain = refs.gain->load();
    p.vdir = (int) refs.vdir->load();     p.quant = (int) refs.quant->load();   p.spaceMode = (int) refs.spaceMode->load();
    p.freeze = refs.freeze->load() > 0.5f;
    return p;
}

void BarajaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = juce::jmin (getTotalNumOutputChannels(), buffer.getNumChannels(), baraja::Engine::maxOutputs);

    const juce::SpinLock::ScopedTryLockType lock (engineLock);
    if (! lock.isLocked() || numOut < 1)
    {
        buffer.clear();   // memory being resized: one silent block
        return;
    }

    const auto p = readParams();

    // copy the input first: inputs and outputs usually share the same buffer
    auto& input = inputCopy;
    input.setSize (2, n, false, false, true);
    if (numIn >= 1) input.copyFrom (0, 0, buffer, 0, 0, n); else input.clear (0, 0, n);
    if (numIn >= 2) input.copyFrom (1, 0, buffer, 1, 0, n); else input.copyFrom (1, 0, input, 0, 0, n);

    engine.process (input.getReadPointer (0), input.getReadPointer (1), buffer.getArrayOfWritePointers(), numOut, n, p);
    for (int ch = numOut; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    samplesSinceTelemetry += n;
    if (samplesSinceTelemetry >= (int) (getSampleRate() / 30.0))
    {
        const juce::SpinLock::ScopedTryLockType tl (telemetryLock);
        if (tl.isLocked())
        {
            engine.snapshot (telemetry);
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
    dest.ringPhase = telemetry.ringPhase;
    dest.headMs = telemetry.headMs;
    dest.numOutputs = telemetry.numOutputs;
    telemetry.outPeak = 0.0f;
    if (fresh)
    {
        std::copy (telemetry.peaks, telemetry.peaks + baraja::Telemetry::numPeaks, dest.peaks);
        dest.memoryMs = telemetry.memoryMs;
        telemetry.peaksFresh = false;
    }
    return fresh;
}

baraja::Lang BarajaProcessor::getLanguage() const
{
    return (int) apvts.state.getProperty ("lang", 0) == 1 ? baraja::Lang::es : baraja::Lang::en;
}

void BarajaProcessor::setLanguage (baraja::Lang lang)
{
    apvts.state.setProperty ("lang", lang == baraja::Lang::es ? 1 : 0, nullptr);
}

int BarajaProcessor::getNumPrograms() { return (int) baraja::presets().size(); }

const juce::String BarajaProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? baraja::presets()[(size_t) index].en : "";
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

    // everything returns to its default except the session decisions:
    // memory length, spatial center, mix, output level and freeze
    static const juce::StringArray kept { "mix", "gain", "center" };
    for (const auto& s : baraja::floatSpecs())
        if (! kept.contains (s.id)) set (s.id, s.def);
    set ("vdir", 0.0f);
    set ("quant", 0.0f);

    for (const auto& [id, value] : baraja::presets()[(size_t) index].values) set (id, value);
}

void BarajaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void BarajaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    // Sessions from 0.1: "delay" (depth) becomes the window width; the window starts at "now".
    if (xml->getChildByAttribute ("id", "width") == nullptr)
        if (auto* old = xml->getChildByAttribute ("id", "delay"))
            old->setAttribute ("id", "width");

    auto state = juce::ValueTree::fromXml (*xml);
    currentProgram = state.getProperty ("program", 0);
    apvts.replaceState (state);
    if (! apvts.state.hasProperty ("lang")) apvts.state.setProperty ("lang", 0, nullptr);
}

juce::AudioProcessorEditor* BarajaProcessor::createEditor() { return new BarajaEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BarajaProcessor(); }
