// Baraja — processor
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BarajaEngine.h"
#include "Parameters.h"

class BarajaProcessor : public juce::AudioProcessor,
                        private juce::AudioProcessorValueTreeState::Listener,
                        private juce::AsyncUpdater
{
public:
    BarajaProcessor();
    ~BarajaProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Baraja"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return memorySeconds(); }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void applyPreset (int index);
    double memorySeconds() const;

    // Interface language, stored with the session.
    baraja::Lang getLanguage() const;
    void setLanguage (baraja::Lang);

    // The interface copies telemetry under this lock; the audio thread only try-locks it.
    bool readTelemetry (baraja::Telemetry& dest);

    // Modulation, for the interface.
    float getSourceValue (int source) const;           // 1 LFO 1, 2 LFO 2, 3 FRAG, 4 FOLLOW
    float getLfoPhase (int lfo) const;                  // 0…1
    float getModulatedNorm (int dest) const;            // where the modulated knob is now (0…1), or -1 if it is not modulated
    float getModDepth (int dest) const;                 // sum of the route amounts (0…1+) on that setting
    int  sourceOf (const juce::String& paramId) const;  // the first source modulating a setting, or 0
    void assignModulation (const juce::String& paramId, int source);
    void removeModulation (const juce::String& paramId);
    double maxFragmentMs() const { return maxFragMs.load(); }
    void setPeakView (float newestMs, float spanMs) { peakNew = newestMs; peakSpan = spanMs; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;
    void reallocateMemory();
    baraja::Params readParams() const noexcept;
    float* fieldFor (baraja::Params&, int dest) const noexcept;

    baraja::Engine engine;
    juce::SpinLock engineLock;          // held while the memory is reallocated
    double allocatedSeconds = 0.0;
    juce::AudioBuffer<float> inputCopy;

    // modulation
    struct ModRefs
    {
        std::atomic<float> *shape[2] {}, *sync[2] {}, *div[2] {}, *rate[2] {}, *phase[2] {};
        std::atomic<float> *folAttack = nullptr, *folRelease = nullptr, *folGain = nullptr;
        std::atomic<float> *src[baraja::numRoutes] {}, *dest[baraja::numRoutes] {}, *amt[baraja::numRoutes] {};
    } mod;
    std::vector<modlib::Range> destRanges;
    modlib::Lfo lfos[2];
    modlib::Follower follower;
    double internalPpq = 0.0;
    std::array<std::atomic<float>, 5> sourceValues {};
    std::array<std::atomic<float>, 2> lfoPhases {};
    std::array<std::atomic<float>, 32> modNorm {};
    std::array<std::atomic<float>, 32> modDepth {};
    std::atomic<double> maxFragMs { 0.0 };
    std::atomic<float> peakNew { 0.0f }, peakSpan { 0.0f };

    baraja::Telemetry telemetry;
    juce::SpinLock telemetryLock;
    int samplesSinceTelemetry = 0;
    int currentProgram = 0;

    struct Refs
    {
        std::atomic<float> *size, *sizeVar, *density, *jitter, *shape, *position, *width, *order, *speed, *pitch, *pitchVar,
                           *drift, *driftRate, *reverse, *spread, *center, *rotation, *ampVar, *mix, *gain,
                           *vdir, *quant, *spaceMode, *freeze, *memory;
    } refs {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarajaProcessor)
};
