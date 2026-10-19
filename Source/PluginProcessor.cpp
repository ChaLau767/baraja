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
    auto r = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    refs = { r ("size"), r ("sizeVar"), r ("density"), r ("jitter"), r ("shape"), r ("position"), r ("width"), r ("order"), r ("speed"), r ("pitch"),
             r ("pitchVar"), r ("drift"), r ("driftRate"), r ("reverse"), r ("spread"), r ("center"), r ("rotation"),
             r ("ampVar"), r ("mix"), r ("gain"), r ("vdir"), r ("quant"), r ("spaceMode"), r ("freeze"), r ("memory") };

    for (int l = 0; l < 2; ++l)
    {
        const juce::String n (l + 1);
        mod.shape[l] = r ("lfo" + n + "Shape"); mod.sync[l] = r ("lfo" + n + "Sync"); mod.div[l] = r ("lfo" + n + "Div");
        mod.rate[l] = r ("lfo" + n + "Rate");   mod.phase[l] = r ("lfo" + n + "Phase");
    }
    mod.folAttack = r ("folAttack"); mod.folRelease = r ("folRelease"); mod.folGain = r ("folGain");
    for (int i = 0; i < baraja::numRoutes; ++i)
    {
        mod.src[i] = r (baraja::routeId (i, "Src")); mod.dest[i] = r (baraja::routeId (i, "Dest")); mod.amt[i] = r (baraja::routeId (i, "Amt"));
    }
    for (const auto& d : baraja::modDestinations()) destRanges.push_back (baraja::modRange (baraja::spec (d.id)));
    for (auto& v : modNorm) v = -1.0f;
    lfos[0].reset (0x2545f491u);
    lfos[1].reset (0x9e3779b9u);

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
    maxFragMs = engine.getMaxFragmentMs();
    follower.reset();
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
        maxFragMs = engine.getMaxFragmentMs();
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

    // song position, for synced LFOs (without a host: 120 BPM from the start)
    double ppq = internalPpq, bpm = 120.0;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto q = pos->getPpqPosition()) ppq = *q;
            if (auto b = pos->getBpm()) bpm = *b;
        }
    internalPpq += bpm / 60.0 * n / getSampleRate();

    // modulation runs every 32 samples: the engine processes the block in short pieces
    const auto& dests = baraja::modDestinations();
    const int numDests = (int) dests.size();
    const int idxRate1 = baraja::destIndex ("lfo1Rate"), idxRate2 = baraja::destIndex ("lfo2Rate");
    float* outPtrs[baraja::Engine::maxOutputs];
    float* const* writes = buffer.getArrayOfWritePointers();
    constexpr int chunk = 32;
    for (int off = 0; off < n; off += chunk)
    {
        const int m = juce::jmin (chunk, n - off);
        const double sr = getSampleRate();

        // sources
        float src[5] { 0.0f, lfos[0].getValue(), lfos[1].getValue(), engine.getLastFrag(), 0.0f };
        src[4] = follower.process (input.getReadPointer (0) + off, input.getReadPointer (1) + off, m,
                                   mod.folAttack->load(), mod.folRelease->load(), mod.folGain->load(), sr);

        // offsets per destination, in knob turns
        float offset[32] {};
        bool touched[32] {};
        auto params = p;
        params.numFrag = 0;
        for (int r = 0; r < baraja::numRoutes; ++r)
        {
            const int s = juce::roundToInt (mod.src[r]->load());
            const int d = juce::roundToInt (mod.dest[r]->load());
            const float a = mod.amt[r]->load() / 100.0f;
            if (s <= 0 || d < 0 || d >= numDests || std::abs (a) < 1.0e-4f) continue;
            touched[d] = true;
            if (s == 3 && dests[(size_t) d].frag >= 0 && params.numFrag < 8)
            {
                params.frag[params.numFrag++] = { dests[(size_t) d].frag, a, destRanges[(size_t) d] };   // a value for every fragment
                continue;
            }
            offset[d] += a * src[s];
        }

        // LFOs (their rates can be modulated too)
        for (int l = 0; l < 2; ++l)
        {
            lfos[l].shape = juce::roundToInt (mod.shape[l]->load());
            lfos[l].phaseOffset = mod.phase[l]->load() / 360.0f;
            const int idx = l == 0 ? idxRate1 : idxRate2;
            if (mod.sync[l]->load() > 0.5f)
                lfos[l].syncTo (ppq + bpm / 60.0 * off / sr, modlib::divisionQuarters (juce::roundToInt (mod.div[l]->load())));
            else
                lfos[l].advanceFree (destRanges[(size_t) idx].apply (mod.rate[l]->load(), offset[idx]) * m / sr);
        }

        for (int d = 0; d < numDests; ++d)
        {
            if (! touched[d]) continue;
            if (float* field = fieldFor (params, d)) *field = destRanges[(size_t) d].apply (*field, offset[d]);
        }

        for (int c = 0; c < numOut; ++c) outPtrs[c] = writes[c] + off;
        engine.process (input.getReadPointer (0) + off, input.getReadPointer (1) + off, outPtrs, numOut, m, params);

        if (off + chunk >= n)
        {
            // what the interface shows: the sources and where each modulated knob is now
            for (int i = 0; i < 5; ++i) sourceValues[(size_t) i] = src[i];
            sourceValues[3] = engine.getLastFrag();
            for (int l = 0; l < 2; ++l) lfoPhases[(size_t) l] = (float) lfos[l].getPhase();
            for (int d = 0; d < numDests && d < 32; ++d)
            {
                float depth = 0.0f;
                for (int r = 0; r < baraja::numRoutes; ++r)
                    if (juce::roundToInt (mod.src[r]->load()) > 0 && juce::roundToInt (mod.dest[r]->load()) == d)
                        depth += std::abs (mod.amt[r]->load() / 100.0f);
                modDepth[(size_t) d] = depth;
                if (! touched[d]) { modNorm[(size_t) d] = -1.0f; continue; }
                float value = d == idxRate1 ? mod.rate[0]->load() : (d == idxRate2 ? mod.rate[1]->load() : 0.0f);
                if (float* field = fieldFor (params, d)) value = *field;
                else value = destRanges[(size_t) d].apply (value, offset[d]);
                float norm = destRanges[(size_t) d].toNorm (value);
                for (int i = 0; i < params.numFrag; ++i)   // FRAG: the newest fragment's value
                    if (params.frag[i].target == dests[(size_t) d].frag) norm += params.frag[i].amount * engine.getLastFrag();
                modNorm[(size_t) d] = juce::jlimit (0.0f, 1.0f, norm);
            }
        }
    }
    for (int ch = numOut; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);

    samplesSinceTelemetry += n;
    if (samplesSinceTelemetry >= (int) (getSampleRate() / 30.0))
    {
        const juce::SpinLock::ScopedTryLockType tl (telemetryLock);
        if (tl.isLocked())
        {
            engine.setPeakView (peakNew.load(), peakSpan.load());
            engine.snapshot (telemetry);
            samplesSinceTelemetry = 0;
        }
    }
}

float* BarajaProcessor::fieldFor (baraja::Params& p, int dest) const noexcept
{
    switch (dest)
    {
        case 0: return &p.size;     case 1: return &p.sizeVar;   case 2: return &p.density;  case 3: return &p.jitter;
        case 4: return &p.shape;    case 5: return &p.position;  case 6: return &p.width;    case 7: return &p.order;
        case 8: return &p.speed;    case 9: return &p.pitch;     case 10: return &p.pitchVar; case 11: return &p.drift;
        case 12: return &p.driftRate; case 13: return &p.reverse; case 14: return &p.spread; case 15: return &p.center;
        case 16: return &p.rotation; case 17: return &p.ampVar;  case 18: return &p.mix;     case 19: return &p.gain;
        default: return nullptr;   // the LFO rates are not engine settings
    }
}

float BarajaProcessor::getSourceValue (int source) const
{
    return juce::isPositiveAndBelow (source, 5) ? sourceValues[(size_t) source].load() : 0.0f;
}

float BarajaProcessor::getLfoPhase (int lfo) const { return lfoPhases[(size_t) juce::jlimit (0, 1, lfo)].load(); }

float BarajaProcessor::getModulatedNorm (int dest) const
{
    return juce::isPositiveAndBelow (dest, 32) ? modNorm[(size_t) dest].load() : -1.0f;
}

float BarajaProcessor::getModDepth (int dest) const
{
    return juce::isPositiveAndBelow (dest, 32) ? modDepth[(size_t) dest].load() : 0.0f;
}

int BarajaProcessor::sourceOf (const juce::String& id) const
{
    const int d = baraja::destIndex (id);
    for (int r = 0; r < baraja::numRoutes && d >= 0; ++r)
    {
        const int s = juce::roundToInt (mod.src[r]->load());
        if (s > 0 && juce::roundToInt (mod.dest[r]->load()) == d) return s;
    }
    return 0;
}

void BarajaProcessor::assignModulation (const juce::String& id, int source)
{
    const int d = baraja::destIndex (id);
    if (d < 0 || source <= 0) return;
    auto set = [this] (const juce::String& pid, float value) {
        if (auto* param = apvts.getParameter (pid))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
    };
    for (int r = 0; r < baraja::numRoutes; ++r)   // already there
        if (juce::roundToInt (mod.src[r]->load()) == source && juce::roundToInt (mod.dest[r]->load()) == d) return;
    for (int r = 0; r < baraja::numRoutes; ++r)
        if (juce::roundToInt (mod.src[r]->load()) == 0)
        {
            set (baraja::routeId (r, "Dest"), (float) d);
            set (baraja::routeId (r, "Src"), (float) source);
            if (std::abs (mod.amt[r]->load()) < 0.5f) set (baraja::routeId (r, "Amt"), 30.0f);
            return;
        }
}

void BarajaProcessor::removeModulation (const juce::String& id)
{
    const int d = baraja::destIndex (id);
    for (int r = 0; r < baraja::numRoutes && d >= 0; ++r)
        if (juce::roundToInt (mod.dest[r]->load()) == d && juce::roundToInt (mod.src[r]->load()) > 0)
            if (auto* param = apvts.getParameter (baraja::routeId (r, "Src")))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (0.0f);
                param->endChangeGesture();
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
