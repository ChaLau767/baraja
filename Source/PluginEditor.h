// Baraja — "Tape" interface
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace baraja::tape
{
namespace colours
{
    const juce::Colour panelTop    { 0xffcfcbc2 };
    const juce::Colour panelBottom { 0xffc1bdb3 };
    const juce::Colour ink         { 0xff1d1b18 };
    const juce::Colour inkSoft     { 0xff4a463f };
    const juce::Colour smoked      { 0xff1e1c1a };
    const juce::Colour smokedText  { 0xff8d877c };
    const juce::Colour oxideTop    { 0xff6b3e22 };
    const juce::Colour oxide       { 0xff5a3420 };
    const juce::Colour oxideDark   { 0xff4c2b1a };
    const juce::Colour printed     { 0xffb9875c };
    const juce::Colour amber       { 0xfff2b65a };
    const juce::Colour glint       { 0xffffe2a8 };
    const juce::Colour reverse     { 0xffa9c8ff };
    const juce::Colour metalDark   { 0xff8d8a83 };
    const juce::Colour metalLight  { 0xffcfccc4 };
    const juce::Colour cream       { 0xfff1eadb };
    const juce::Colour lamp        { 0xffe2493a };
    const juce::Colour face        { 0xffefe6cf };
    const juce::Colour redZone     { 0xffc8372d };
}

juce::Font label (float height);       // screen-printed condensed caps
juce::Font heavy (float height);       // logo and section titles
juce::Font mono (float height);        // counter and values

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getPopupMenuFont() override { return label (15.0f); }
};

class Knob : public juce::Component
{
public:
    Knob (BarajaProcessor&, const juce::String& paramId, float knobSize = 58.0f);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setLanguage (Lang);
    std::function<void()> onUserChange;
    juce::Slider slider;

private:
    juce::RangedAudioParameter* param;
    const FloatSpec& spec;
    float knobSize;
    Lang lang = Lang::en;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Switch : public juce::Component
{
public:
    Switch (BarajaProcessor&, const juce::String& paramId, std::vector<const char*> keys, const char* titleKey);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setLanguage (Lang l) { lang = l; repaint(); }
    void setPlainLabels (juce::StringArray labels) { plain = std::move (labels); }
    std::function<void()> onUserChange;

private:
    juce::Rectangle<float> trackArea() const;
    juce::RangedAudioParameter* param;
    std::vector<const char*> keys;
    juce::StringArray plain;
    const char* titleKey;
    Lang lang = Lang::en;
};

class Transport : public juce::Component
{
public:
    explicit Transport (BarajaProcessor&);
    void paint (juce::Graphics&) override;
    void update (const Telemetry&, bool peaksFresh, bool running);
    void setLanguage (Lang l) { lang = l; repaint(); }

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> stripArea() const;
    float xForMs (float ms) const;
    float msForX (float x) const;
    float memoryMs() const;
    enum class Hit { none, newer, older, body };
    Hit hitTest (juce::Point<float>) const;
    void paintReel (juce::Graphics&, juce::Point<float> centre, float radius, float angle, float pack);

    BarajaProcessor& processor;
    juce::RangedAudioParameter *position, *width;
    Telemetry tele;
    bool hasPeaks = false;
    float waveScale = 1.0f, reelAngle = 0.0f;
    Lang lang = Lang::en;

    Hit dragging = Hit::none;
    float dragStartPos = 0.0f, dragStartWidth = 0.0f, dragStartMs = 0.0f;
};

class Meter : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void push (float peak);
    void setLanguage (Lang l) { lang = l; repaint(); }

private:
    float levelDb = -60.0f, needleDb = -60.0f, needleVel = 0.0f;
    int overHold = 0;
    Lang lang = Lang::en;
};

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (BarajaProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseWidth = 1180, baseHeight = 800;

private:
    void timerCallback() override;
    void setLanguage (Lang);
    void highlightPreset();
    juce::Rectangle<int> sectionArea (int column) const;

    BarajaProcessor& processor;
    LookAndFeel lnf;
    Lang lang = Lang::en;

    juce::TextButton langEn { "EN" }, langEs { "ES" };
    juce::OwnedArray<juce::TextButton> presetButtons;
    Transport transport;
    Meter meter;
    juce::OwnedArray<Knob> knobs;
    std::unique_ptr<Switch> memorySwitch, vdirSwitch, quantSwitch, spaceSwitch;
    juce::TextButton freezeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    Knob* knob (const juce::String& id) const;
    Telemetry telemetry;
    int shownProgram = -2;
    int lastOutputs = -1;
};

} // namespace baraja::tape

class BarajaEditor : public juce::AudioProcessorEditor
{
public:
    explicit BarajaEditor (BarajaProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (baraja::tape::colours::panelBottom); }

private:
    baraja::tape::Content content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BarajaEditor)
};
