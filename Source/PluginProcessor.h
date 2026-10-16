// Baraja — procesador
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BarajaEngine.h"
#include "Parameters.h"

class BarajaProcessor : public juce::AudioProcessor
{
public:
    BarajaProcessor();
    ~BarajaProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void applyPreset (int index);

    // La interfaz copia la telemetría bajo este lock; el hilo de audio solo usa try-lock.
    bool readTelemetry (baraja::Telemetry& dest);

    juce::AudioProcessorValueTreeState apvts;

private:
    baraja::Params readParams() const noexcept;

    baraja::Engine engine;
    juce::AudioBuffer<float> inputCopy;
    baraja::Telemetry telemetry;
    juce::SpinLock telemetryLock;
    int samplesSinceTelemetry = 0;
    int currentProgram = 0;

    struct FloatRef { const char* id; std::atomic<float>* value; };
    std::vector<FloatRef> floatRefs;
    std::atomic<float>* vdirParam = nullptr;
    std::atomic<float>* quantParam = nullptr;
    std::atomic<float>* freezeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarajaProcessor)
};
