// Baraja — interfaz
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace baraja::ui
{
namespace colours
{
    const juce::Colour bg    { 0xff101419 };
    const juce::Colour panel { 0xff171c22 };
    const juce::Colour ink   { 0xffe3e7ec };
    const juce::Colour muted { 0xff8c97a3 };
    const juce::Colour line  { 0xff29313a };
    const juce::Colour strip { 0xff0b0e12 };
    const juce::Colour wave  { 0xff9aa7b5 };
    const juce::Colour grain { 0xfff0a04b };
    const juce::Colour rev   { 0xff7fa3ff };
    const juce::Colour hot   { 0xffff6b72 };
}

juce::Font font (float size, bool bold = false);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

class ParamRow : public juce::Component
{
public:
    ParamRow (juce::AudioProcessorValueTreeState&, const baraja::FloatSpec&);
    void resized() override;
    std::function<void()> onUserChange;

private:
    void refreshValue();
    juce::RangedAudioParameter* param;
    juce::Label name, value;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ChoiceRow : public juce::Component
{
public:
    ChoiceRow (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& label, const juce::StringArray& items);
    void resized() override;
    juce::ComboBox box;

private:
    juce::Label name;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

class Group : public juce::Component
{
public:
    explicit Group (const juce::String& groupTitle) : title (groupTitle) {}
    void paint (juce::Graphics&) override;
    void resized() override;
    void add (juce::Component& c) { rows.push_back (&c); addAndMakeVisible (c); }
    int preferredHeight() const;

private:
    juce::String title;
    std::vector<juce::Component*> rows;
};

class MemoryView : public juce::Component
{
public:
    explicit MemoryView (juce::AudioProcessorValueTreeState& s) : state (s) {}
    void paint (juce::Graphics&) override;
    void update (const baraja::Telemetry& t, bool peaksFresh);

private:
    juce::AudioProcessorValueTreeState& state;
    baraja::Telemetry tele;
    float waveScale = 1.0f;
    bool hasPeaks = false;
};

class Meter : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void push (float peak);

private:
    float hold = 0.0f;
};

} // namespace baraja::ui

class BarajaEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BarajaEditor (BarajaProcessor&);
    ~BarajaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void highlightPreset();

    BarajaProcessor& processor;
    baraja::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::OwnedArray<juce::TextButton> presetButtons;
    juce::Label presetsLabel;
    baraja::ui::Meter meter;
    baraja::ui::MemoryView memory;

    baraja::ui::Group fragmentos { "Fragmentos" }, memoria { "Memoria" }, altura { "Altura y sentido" }, salida { "Espacio y salida" };
    juce::OwnedArray<baraja::ui::ParamRow> rows;
    std::unique_ptr<baraja::ui::ChoiceRow> vdirRow, quantRow;

    juce::TextButton freezeButton { "Congelar memoria" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    juce::Label freezeHint;

    baraja::Telemetry telemetry;
    int shownProgram = -1;
    int legendY = 120;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarajaEditor)
};
