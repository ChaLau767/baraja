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
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return label (14.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

// A slider that leaves the right click to the modulation menu.
class ModSlider : public juce::Slider
{
public:
    std::function<void()> onPopup;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onPopup) onPopup(); return; } Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! e.mods.isPopupMenu()) Slider::mouseUp (e); }
};

class Knob : public juce::Component
{
public:
    Knob (BarajaProcessor&, const juce::String& paramId, float knobSize = 58.0f);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void setLanguage (Lang);
    std::function<void()> onUserChange, onShowModulation;
    ModSlider slider;
    bool isModulated() const;

private:
    void showModMenu();
    BarajaProcessor& processor;
    juce::String paramId;
    int dest = -1;
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
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scale) override;

private:
    juce::Rectangle<float> stripArea() const;
    juce::Rectangle<float> barArea() const;
    float viewNew() const;      // ms: the newest edge of what the tape shows
    float viewSpan() const;     // ms shown
public:
    void zoomAround (float ms, float factor);
private:
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
    float zoomNew = 0.0f, zoomSpan = 0.0f;   // zoomSpan 0: the whole memory
    bool scrolling = false;
    float scrollGrab = 0.0f;
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

// The shape buttons of an LFO.
class ShapePicker : public juce::Component
{
public:
    ShapePicker (BarajaProcessor&, const juce::String& paramId);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter* param;
};

// One route: source → setting, and how much.
class RouteRow : public juce::Component
{
public:
    RouteRow (BarajaProcessor&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setLanguage (Lang);
    juce::ComboBox source, target;
    juce::Slider amount;
private:
    BarajaProcessor& processor;
    int index;
    Lang lang = Lang::en;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment, targetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
};

// The modulation page: sources on the left, routes on the right.
class ModPanel : public juce::Component
{
public:
    explicit ModPanel (BarajaProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void setLanguage (Lang);
    void update();
private:
    juce::Rectangle<float> screenArea() const;
    juce::Rectangle<int> tabArea (int i) const;
    void showSource (int s);
    BarajaProcessor& processor;
    Lang lang = Lang::en;
    int selected = 0;                         // 0 LFO 1, 1 LFO 2, 2 FRAG, 3 FOLLOW
    std::unique_ptr<ShapePicker> shapes[2];
    std::unique_ptr<Switch> syncs[2];
    juce::ComboBox divisions[2];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttachments[2];
    juce::OwnedArray<Knob> lfoKnobs, followKnobs;   // rate, phase × 2; attack, release, gain
    juce::OwnedArray<RouteRow> rows;
    std::vector<float> trail;                   // recent values of the shown source
};

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (BarajaProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseWidth = 1180, baseHeight = 800;
    void showPage (int page);     // 0 controls, 1 modulation
    Transport& getTransport() { return transport; }

private:
    void timerCallback() override;
    void setLanguage (Lang);
    void highlightPreset();
    juce::Rectangle<int> sectionArea (int column) const;

    BarajaProcessor& processor;
    LookAndFeel lnf;
    Lang lang = Lang::en;

    juce::TextButton langEn { "EN" }, langEs { "ES" };
    juce::TextButton pageControls, pageModulation;
    int page = 0;
    ModPanel modPanel;
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
