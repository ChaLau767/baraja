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

    juce::AudioProcessorValueTreeState apvts;

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void handleAsyncUpdate() override;
    void reallocateMemory();
    baraja::Params readParams() const noexcept;

    baraja::Engine engine;
    juce::SpinLock engineLock;          // held while the memory is reallocated
    double allocatedSeconds = 0.0;
    juce::AudioBuffer<float> inputCopy;

    baraja::Telemetry telemetry;
    juce::SpinLock telemetryLock;
    int samplesSinceTelemetry = 0;
    int currentProgram = 0;

    struct Refs
    {
        std::atomic<float> *size, *sizeVar, *density, *jitter, *shape, *position, *width, *pitch, *pitchVar,
                           *drift, *driftRate, *reverse, *spread, *center, *rotation, *ampVar, *mix, *gain,
                           *vdir, *quant, *spaceMode, *freeze, *memory;
    } refs {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarajaProcessor)
};
