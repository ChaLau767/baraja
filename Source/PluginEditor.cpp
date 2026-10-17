// Baraja — "Tape" interface
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"
#include "BinaryData.h"

namespace baraja::tape
{

//==============================================================================
namespace
{
    struct Typefaces
    {
        juce::Typeface::Ptr semi, bold, xbold, mono;
        static Typefaces& get()
        {
            static Typefaces t {
                juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize),
                juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize),
                juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedExtraBold_ttf, BinaryData::BarlowCondensedExtraBold_ttfSize),
                juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize)
            };
            return t;
        }
    };

    constexpr float pi = juce::MathConstants<float>::pi;

    juce::String localNumber (juce::String t, Lang lang)
    {
        return lang == Lang::es ? t.replaceCharacter ('.', ',') : t;
    }

    void drawScrew (juce::Graphics& g, float cx, float cy, float r, float angle)
    {
        juce::ColourGradient grad (colours::metalLight, cx - r * 0.4f, cy - r * 0.5f, colours::metalDark, cx + r, cy + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
        g.setColour (colours::ink.withAlpha (0.25f));
        g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 0.8f);
        g.setColour (colours::ink.withAlpha (0.55f));
        const float dx = std::cos (angle) * r * 0.7f, dy = std::sin (angle) * r * 0.7f;
        g.drawLine (cx - dx, cy - dy, cx + dx, cy + dy, 1.6f);
    }

    juce::String formatSeconds (float ms, Lang lang)
    {
        return localNumber (juce::String (ms / 1000.0f, 2), lang) + " s";
    }
}

juce::Font label (float h) { return juce::Font (juce::FontOptions (Typefaces::get().semi).withHeight (h).withKerningFactor (0.10f)); }
juce::Font heavy (float h) { return juce::Font (juce::FontOptions (Typefaces::get().xbold).withHeight (h).withKerningFactor (0.06f)); }
juce::Font mono (float h)  { return juce::Font (juce::FontOptions (Typefaces::get().mono).withHeight (h)); }

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::TooltipWindow::backgroundColourId, colours::smoked);
    setColour (juce::TooltipWindow::textColourId, colours::cream);
    setColour (juce::TooltipWindow::outlineColourId, colours::smoked);
    setColour (juce::PopupMenu::backgroundColourId, colours::face);
    setColour (juce::PopupMenu::textColourId, colours::ink);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    const float r = juce::jmin (w, h) * 0.5f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;

    // scale ticks, screen-printed on the panel
    g.setColour (colours::ink.withAlpha (s.isEnabled() ? 1.0f : 0.4f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (end - start) * i / 10.0f;
        const float len = (i == 0 || i == 5 || i == 10) ? 6.0f : 4.0f;
        const float sx = std::sin (a), sy = -std::cos (a);
        g.drawLine (cx + sx * (r - len), cy + sy * (r - len), cx + sx * r, cy + sy * r, i % 5 == 0 ? 1.6f : 1.1f);
    }

    // bakelite cap
    const float cr = r - 9.0f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - cr, cy - cr + 3.0f, cr * 2, cr * 2);
    juce::ColourGradient cap (juce::Colour (0xff4f4843), cx - cr * 0.35f, cy - cr * 0.45f,
                              juce::Colour (0xff0d0b0a), cx + cr, cy + cr, true);
    cap.addColour (0.55, juce::Colour (0xff1a1614));
    g.setGradientFill (cap);
    g.fillEllipse (cx - cr, cy - cr, cr * 2, cr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (cx - cr + 1.5f, cy - cr + 1.5f, cr * 2 - 3, cr * 2 - 3, 1.0f);

    // pointer
    const float a = start + pos * (end - start);
    const float sx = std::sin (a), sy = -std::cos (a);
    g.setColour (s.isEnabled() ? colours::cream : colours::cream.withAlpha (0.4f));
    g.drawLine (cx + sx * cr * 0.25f, cy + sy * cr * 0.25f, cx + sx * cr * 0.86f, cy + sy * cr * 0.86f, 3.0f);

    if (s.hasKeyboardFocus (true))
    {
        g.setColour (colours::amber);
        g.drawEllipse (cx - r + 1, cy - r + 1, r * 2 - 2, r * 2 - 2, 1.5f);
    }
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.75f);
    const bool on = b.getToggleState();

    if (b.getProperties().getWithDefault ("freeze", false))
    {
        g.setColour (down ? colours::ink.brighter (0.15f) : colours::ink);
        g.fillRoundedRectangle (r, 6.0f);
        const float lr = 6.0f, lx = r.getX() + 22.0f, ly = r.getCentreY();
        if (on)
        {
            g.setColour (colours::lamp.withAlpha (0.35f));
            g.fillEllipse (lx - lr * 2.2f, ly - lr * 2.2f, lr * 4.4f, lr * 4.4f);
            g.setColour (colours::lamp);
        }
        else g.setColour (juce::Colour (0xff4a2420));
        g.fillEllipse (lx - lr, ly - lr, lr * 2, lr * 2);
        return;
    }

    // chips: presets and language
    if (on) { g.setColour (colours::ink); g.fillRoundedRectangle (r, 4.0f); }
    g.setColour (on || over ? colours::ink : colours::inkSoft.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 4.0f, 1.4f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool freeze = b.getProperties().getWithDefault ("freeze", false);
    g.setColour (freeze || b.getToggleState() ? colours::cream : colours::ink);
    g.setFont (freeze ? heavy (17.0f) : label (13.5f));
    auto area = b.getLocalBounds();
    if (freeze) area.removeFromLeft (30);
    g.drawText (caps (b.getButtonText()), area, juce::Justification::centred);
}

//==============================================================================
Knob::Knob (BarajaProcessor& p, const juce::String& id, float size)
    : param (p.apvts.getParameter (id)), spec (baraja::spec (id)), knobSize (size)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (pi * 1.25f, pi * 2.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setScrollWheelEnabled (true);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    slider.setDoubleClickReturnValue (true, spec.def);
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [this] { if (onUserChange) onUserChange(); };
    addAndMakeVisible (slider);
    setLanguage (Lang::en);
}

void Knob::setLanguage (Lang l)
{
    lang = l;
    juce::String note = u8 (lang == Lang::es ? spec.noteEs : spec.noteEn);
    slider.setTooltip ((note.isEmpty() ? juce::String() : note + ". ") + tr (lang, "dblclick") + ".");
    repaint();
}

void Knob::resized()
{
    slider.setBounds (juce::Rectangle<int> ((int) knobSize, (int) knobSize).withCentre ({ getWidth() / 2, (int) (knobSize / 2) }));
}

void Knob::paint (juce::Graphics& g)
{
    const float alpha = isEnabled() ? 1.0f : 0.4f;
    auto r = getLocalBounds().toFloat();
    r.removeFromTop (knobSize + 3.0f);
    g.setColour (colours::ink.withAlpha (alpha));
    g.setFont (label (13.0f));
    g.drawText (caps (u8 (lang == Lang::es ? spec.es : spec.en)), r.removeFromTop (15.0f), juce::Justification::centred);
    g.setColour (colours::inkSoft.withAlpha (alpha));
    g.setFont (mono (12.0f));
    g.drawText (localNumber (param->getCurrentValueAsText(), lang), r.removeFromTop (14.0f), juce::Justification::centred);
}

//==============================================================================
Switch::Switch (BarajaProcessor& p, const juce::String& id, std::vector<const char*> k, const char* title)
    : param (p.apvts.getParameter (id)), keys (std::move (k)), titleKey (title) {}

juce::Rectangle<float> Switch::trackArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (17.0f).withHeight (26.0f).reduced (0.75f, 0.0f);
}

void Switch::paint (juce::Graphics& g)
{
    g.setColour (colours::inkSoft);
    g.setFont (label (12.5f));
    g.drawText (caps (tr (lang, titleKey)), getLocalBounds().removeFromTop (15), juce::Justification::centredLeft);

    const auto track = trackArea();
    const int n = plain.isEmpty() ? (int) keys.size() : plain.size();
    const int current = juce::roundToInt (param->getValue() * (n - 1));
    const float w = track.getWidth() / n;

    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (track.getX() + i * w, track.getY(), w, track.getHeight());
        if (i == current)
        {
            g.setColour (colours::ink);
            g.fillRect (seg);
        }
        g.setColour (i == current ? colours::cream : colours::ink);
        g.setFont (label (13.0f));
        const juce::String text = plain.isEmpty() ? tr (lang, keys[(size_t) i]) : plain[i];
        g.drawText (caps (text), seg, juce::Justification::centred);
    }
    g.setColour (colours::ink);
    g.drawRoundedRectangle (track, 3.0f, 1.5f);
}

void Switch::mouseDown (const juce::MouseEvent& e)
{
    const auto track = trackArea();
    if (! track.contains (e.position)) return;
    const int n = plain.isEmpty() ? (int) keys.size() : plain.size();
    const int index = juce::jlimit (0, n - 1, (int) ((e.position.x - track.getX()) / track.getWidth() * n));
    param->beginChangeGesture();
    param->setValueNotifyingHost ((float) index / (float) (n - 1));
    param->endChangeGesture();
    if (onUserChange) onUserChange();
    repaint();
}

//==============================================================================
Transport::Transport (BarajaProcessor& p)
    : processor (p), position (p.apvts.getParameter ("position")), width (p.apvts.getParameter ("width")) {}

juce::Rectangle<float> Transport::stripArea() const
{
    return { 18.0f, 182.0f, getWidth() - 36.0f, 90.0f };
}

float Transport::memoryMs() const { return (float) processor.memorySeconds() * 1000.0f; }

float Transport::xForMs (float ms) const
{
    const auto s = stripArea();
    return s.getX() + s.getWidth() * (1.0f - juce::jlimit (0.0f, 1.0f, ms / memoryMs()));
}

float Transport::msForX (float x) const
{
    const auto s = stripArea();
    return (1.0f - juce::jlimit (0.0f, 1.0f, (x - s.getX()) / s.getWidth())) * memoryMs();
}

Transport::Hit Transport::hitTest (juce::Point<float> p) const
{
    const auto s = stripArea();
    if (p.y < s.getY() - 12.0f || p.y > s.getBottom() + 12.0f) return Hit::none;
    const float pos = position->convertFrom0to1 (position->getValue());
    const float wid = width->convertFrom0to1 (width->getValue());
    const float xNew = xForMs (pos), xOld = xForMs (pos + wid);
    if (std::abs (p.x - xNew) <= 11.0f) return Hit::newer;
    if (std::abs (p.x - xOld) <= 11.0f) return Hit::older;
    if (p.x > xOld && p.x < xNew) return Hit::body;
    return Hit::none;
}

void Transport::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hitTest (e.position);
    setMouseCursor (h == Hit::body ? juce::MouseCursor::DraggingHandCursor
                  : h == Hit::none ? juce::MouseCursor::NormalCursor
                                   : juce::MouseCursor::LeftRightResizeCursor);
}

void Transport::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTest (e.position);
    if (dragging == Hit::none) return;
    dragStartPos = position->convertFrom0to1 (position->getValue());
    dragStartWidth = width->convertFrom0to1 (width->getValue());
    dragStartMs = msForX (e.position.x);
    position->beginChangeGesture();
    width->beginChangeGesture();
}

void Transport::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == Hit::none) return;
    const float mem = memoryMs(), ms = msForX (e.position.x);
    const float olderEdge = dragStartPos + dragStartWidth;
    float pos = dragStartPos, wid = dragStartWidth;

    if (dragging == Hit::newer)
    {
        pos = juce::jlimit (0.0f, olderEdge - 20.0f, ms);
        wid = olderEdge - pos;
    }
    else if (dragging == Hit::older)
    {
        wid = juce::jlimit (20.0f, mem - dragStartPos, ms - dragStartPos);
    }
    else
    {
        pos = juce::jlimit (0.0f, juce::jmax (0.0f, mem - dragStartWidth), dragStartPos + (ms - dragStartMs));
    }

    position->setValueNotifyingHost (position->convertTo0to1 (pos));
    width->setValueNotifyingHost (width->convertTo0to1 (juce::jlimit (20.0f, 60000.0f, wid)));
    repaint();
}

void Transport::mouseUp (const juce::MouseEvent&)
{
    if (dragging == Hit::none) return;
    position->endChangeGesture();
    width->endChangeGesture();
    dragging = Hit::none;
}

void Transport::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (hitTest (e.position) == Hit::none) return;
    position->beginChangeGesture(); width->beginChangeGesture();
    position->setValueNotifyingHost (position->convertTo0to1 (0.0f));
    width->setValueNotifyingHost (width->convertTo0to1 (1500.0f));
    position->endChangeGesture(); width->endChangeGesture();
}

void Transport::update (const Telemetry& t, bool peaksFresh, bool running)
{
    tele.numGrains = t.numGrains;
    std::copy (t.grains, t.grains + t.numGrains, tele.grains);
    tele.drift = t.drift;
    tele.numOutputs = t.numOutputs;
    if (peaksFresh)
    {
        std::copy (t.peaks, t.peaks + Telemetry::numPeaks, tele.peaks);
        hasPeaks = true;
    }
    if (running) reelAngle += 0.035f;   // the reels turn while the memory records
    repaint();
}

void Transport::paintReel (juce::Graphics& g, juce::Point<float> c, float r, float angle, float pack)
{
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (c.x - r, c.y - r + 4.0f, r * 2, r * 2);

    // tape pack, seen through the flange windows
    g.setColour (colours::oxide);
    g.fillEllipse (c.x - r * pack, c.y - r * pack, r * pack * 2, r * pack * 2);
    g.setColour (colours::oxideDark);
    for (float rr = r * 0.3f; rr < r * pack; rr += 5.0f)
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.5f);

    juce::Path flange;
    flange.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
    for (int i = 0; i < 3; ++i)
    {
        const float a0 = angle + i * 2.0f * pi / 3.0f;
        juce::Path window;
        window.addPieSegment (c.x - r * 0.9f, c.y - r * 0.9f, r * 1.8f, r * 1.8f, a0, a0 + 1.05f, 0.36f);
        flange.addPath (window);
    }
    flange.setUsingNonZeroWinding (false);
    g.setColour (juce::Colour (0xff2b2926));
    g.fillPath (flange);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawEllipse (c.x - r + 1, c.y - r + 1, r * 2 - 2, r * 2 - 2, 1.0f);

    // hub
    const float hr = r * 0.24f;
    juce::ColourGradient hub (colours::metalLight, c.x - hr * 0.4f, c.y - hr * 0.5f, colours::metalDark, c.x + hr, c.y + hr, true);
    g.setGradientFill (hub);
    g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
    g.setColour (juce::Colour (0xff2b2926));
    for (int i = 0; i < 3; ++i)
    {
        const float a = angle + i * 2.0f * pi / 3.0f + 0.5f;
        g.fillEllipse (c.x + std::sin (a) * hr * 0.62f - 2.5f, c.y - std::cos (a) * hr * 0.62f - 2.5f, 5.0f, 5.0f);
    }
    g.fillEllipse (c.x - 3.5f, c.y - 3.5f, 7.0f, 7.0f);
}

void Transport::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    g.setColour (colours::smoked);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);

    const bool frozen = processor.apvts.getRawParameterValue ("freeze")->load() > 0.5f;
    const float pos = position->convertFrom0to1 (position->getValue());
    const float wid = width->convertFrom0to1 (width->getValue());
    const float mem = memoryMs();

    // reels and tape path
    const float rr = 74.0f;
    const juce::Point<float> left (26.0f + rr, 92.0f), right (W - 26.0f - rr, 92.0f);
    const float packL = 0.86f, packR = 0.58f;
    g.setColour (colours::oxide);
    g.drawLine (left.x, left.y + rr * packL, W * 0.5f, 158.0f, 4.0f);
    g.drawLine (W * 0.5f, 158.0f, right.x, right.y + rr * packR, 4.0f);
    paintReel (g, left, rr, reelAngle, packL);
    paintReel (g, right, rr, reelAngle * 1.48f, packR);

    // head block with the position counter
    auto head = juce::Rectangle<float> (W * 0.5f - 92.0f, 26.0f, 184.0f, 112.0f);
    juce::ColourGradient metal (colours::metalLight, head.getX(), head.getY(), colours::metalDark, head.getX(), head.getBottom(), false);
    g.setGradientFill (metal);
    g.fillRoundedRectangle (head, 7.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (head, 7.0f, 1.0f);
    drawScrew (g, head.getX() + 10, head.getY() + 10, 3.5f, 0.6f);
    drawScrew (g, head.getRight() - 10, head.getY() + 10, 3.5f, 2.1f);

    auto counter = juce::Rectangle<float> (head.getX() + 22.0f, head.getY() + 22.0f, head.getWidth() - 44.0f, 40.0f);
    g.setColour (juce::Colour (0xff141210));
    g.fillRoundedRectangle (counter, 4.0f);
    g.setColour (frozen ? colours::amber : colours::cream);
    g.setFont (mono (30.0f));
    juce::String digits = juce::String::fromUTF8 ("−") + juce::String (pos / 1000.0f, 2).paddedLeft ('0', 5);
    g.drawText (localNumber (digits, lang), counter, juce::Justification::centred);
    g.setColour (colours::ink);
    g.setFont (label (12.5f));
    g.drawText (caps (tr (lang, "playhead")), head.getX(), counter.getBottom() + 6.0f, head.getWidth(), 14.0f, juce::Justification::centred);
    g.setColour (colours::inkSoft);
    g.setFont (mono (13.0f));
    g.drawText (formatSeconds (wid, lang), head.getX(), counter.getBottom() + 22.0f, head.getWidth(), 14.0f, juce::Justification::centred);

    g.setColour (colours::smokedText);
    g.setFont (label (12.0f));
    g.drawText (tr (lang, "dragHint"), 0.0f, 162.0f, W, 14.0f, juce::Justification::centred);

    // the tape: the memory, oldest at the left, "now" at the right
    const auto s = stripArea();
    juce::ColourGradient oxide (colours::oxideTop, 0.0f, s.getY(), colours::oxideDark, 0.0f, s.getBottom(), false);
    oxide.addColour (0.5, colours::oxide);
    g.setGradientFill (oxide);
    g.fillRect (s);

    if (hasPeaks)
    {
        float mx = 0.0f;
        for (auto p : tele.peaks) mx = juce::jmax (mx, p);
        const float target = juce::jmin (6.0f, 1.0f / juce::jmax (0.08f, mx));
        waveScale += (target - waveScale) * 0.08f;
        g.setColour (colours::printed.withAlpha (0.55f));
        const float bw = s.getWidth() / Telemetry::numPeaks;
        for (int b = 0; b < Telemetry::numPeaks; ++b)
        {
            const float a = juce::jmin (1.0f, tele.peaks[b] * waveScale) * s.getHeight() * 0.44f + 0.5f;
            g.fillRect (s.getX() + b * bw, s.getCentreY() - a, juce::jmax (1.0f, bw * 0.7f), a * 2.0f);
        }
    }

    const float xNew = xForMs (pos), xOld = xForMs (pos + wid);
    g.setColour (colours::amber.withAlpha (0.16f));
    g.fillRect (juce::Rectangle<float> (xOld, s.getY(), xNew - xOld, s.getHeight()));
    g.setColour (colours::amber);
    g.fillRect (xOld - 1.0f, s.getY(), 2.0f, s.getHeight());
    g.fillRect (xNew - 1.0f, s.getY(), 2.0f, s.getHeight());

    // fragments: where they read (x) and where they sound (y: stereo pan or place on the ring)
    for (int i = 0; i < tele.numGrains; ++i)
    {
        const auto& v = tele.grains[i];
        const float x0 = xForMs (v.delayMs);
        const float x1 = xForMs (v.reversed ? v.delayMs + v.spanMs : juce::jmax (0.0f, v.delayMs - v.spanMs));
        const float y = s.getY() + 8.0f + v.place * (s.getHeight() - 16.0f);
        const float env = std::sin (pi * juce::jlimit (0.0f, 1.0f, v.progress));
        const auto c = v.reversed ? colours::reverse : colours::glint;
        const float alpha = 0.3f + 0.7f * v.amp * (0.35f + 0.65f * env);
        const auto rect = juce::Rectangle<float> (juce::jmin (x0, x1), y - 2.0f, juce::jmax (2.0f, std::abs (x1 - x0)), 4.0f);
        g.setColour (c.withAlpha (alpha * 0.25f));
        g.fillRoundedRectangle (rect.expanded (2.0f, 3.0f), 3.0f);
        g.setColour (c.withAlpha (alpha));
        g.fillRoundedRectangle (rect, 2.0f);
    }

    // the two heads that hold the window: drag them
    for (float hx : { xOld, xNew })
    {
        auto h = juce::Rectangle<float> (hx - 9.0f, s.getY() - 9.0f, 18.0f, s.getHeight() + 18.0f);
        juce::ColourGradient hg (colours::metalDark, h.getX(), 0.0f, colours::metalDark, h.getRight(), 0.0f, false);
        hg.addColour (0.5, colours::metalLight);
        g.setGradientFill (hg);
        g.fillRoundedRectangle (h, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (h, 3.0f, 1.0f);
        g.fillRect (hx - 0.5f, h.getY() + 6.0f, 1.0f, h.getHeight() - 12.0f);
    }

    g.setFont (label (12.0f));
    if (frozen)
    {
        g.setColour (colours::amber);
        g.drawText (caps (tr (lang, "frozen")), s.getRight() - 160.0f, s.getY() - 22.0f, 160.0f, 14.0f, juce::Justification::right);
    }
    const float drift = processor.apvts.getRawParameterValue ("drift")->load();
    if (drift > 0.0f)
    {
        const float c = processor.apvts.getRawParameterValue ("pitch")->load() + tele.drift;
        g.setColour (colours::glint);
        g.drawText (localNumber ((c > 0 ? "+" : "") + juce::String (c, 1), lang) + " st", s.getX(), s.getY() - 22.0f, 160.0f, 14.0f, juce::Justification::left);
    }

    // ruler
    const float steps[] = { 250, 500, 1000, 2000, 5000, 10000 };
    float st = 10000.0f;
    for (auto v : steps) if (mem / v <= 12.0f) { st = v; break; }
    g.setFont (mono (11.5f));
    for (int k = 0; k * st <= mem + 1.0f; ++k)
    {
        const float ms = k * st, x = xForMs (ms);
        g.setColour (colours::smokedText);
        g.fillRect (x - 0.5f, s.getBottom() + 3.0f, 1.0f, 4.0f);
        const juce::String t = k == 0 ? tr (lang, "now")
                             : juce::String::fromUTF8 ("−") + localNumber (juce::String (ms / 1000.0f, std::fmod (ms, 1000.0f) > 0 ? 1 : 0), lang) + (k == 1 ? " s" : "");
        if (k == 0) g.drawText (t, (int) x - 60, (int) s.getBottom() + 9, 60, 13, juce::Justification::right);
        else if (x > s.getX() + 16.0f) g.drawText (t, (int) x - 30, (int) s.getBottom() + 9, 60, 13, juce::Justification::centred);
    }
    juce::ignoreUnused (H);
}

//==============================================================================
void Meter::push (float peak)
{
    const float target = peak > 1.0e-6f ? juce::jlimit (-60.0f, 3.0f, 20.0f * std::log10 (peak)) : -60.0f;
    // peak programme ballistics: instant rise, ~1.7 s to fall 20 dB (30 frames per second)
    levelDb = target > levelDb ? target : juce::jmax (target, levelDb - 11.8f / 30.0f);
    // the needle has a little mass
    needleVel += (levelDb - needleDb) * 0.32f - needleVel * 0.62f;
    needleDb += needleVel;
    if (peak >= 0.977f) overHold = 45; else if (overHold > 0) --overHold;
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 7.0f);
    g.setColour (colours::ink);
    g.fillRoundedRectangle (bounds, 7.0f);
    auto face = bounds.reduced (5.0f);
    g.setColour (colours::face);
    g.fillRoundedRectangle (face, 4.0f);

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());

    const float W = face.getWidth();
    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + 34.0f);
    const float R = face.getHeight() + 6.0f;
    const float minDb = -40.0f, maxDb = 3.0f, spanDeg = 90.0f;
    // like a real programme meter: compressed at the bottom, open near 0 dB
    auto angleFor = [&] (float db) {
        static const float pts[][2] { { -40, 0.0f }, { -20, 0.26f }, { -10, 0.50f }, { -6, 0.64f }, { -3, 0.77f }, { 0, 0.90f }, { 3, 1.0f } };
        db = juce::jlimit (minDb, maxDb, db);
        float f = 1.0f;
        for (int i = 0; i < 6; ++i)
            if (db <= pts[i + 1][0])
            {
                const float t = (db - pts[i][0]) / (pts[i + 1][0] - pts[i][0]);
                f = pts[i][1] + t * (pts[i + 1][1] - pts[i][1]);
                break;
            }
        return juce::degreesToRadians (-spanDeg * 0.5f + spanDeg * f);
    };
    auto pt = [&] (float a, float r) { return juce::Point<float> (pivot.x + std::sin (a) * r, pivot.y - std::cos (a) * r); };

    {
        juce::Path red;
        red.addCentredArc (pivot.x, pivot.y, R - 14.0f, R - 14.0f, 0.0f, angleFor (0.0f), angleFor (maxDb), true);
        g.setColour (colours::redZone);
        g.strokePath (red, juce::PathStrokeType (5.0f));
        juce::Path warn;
        warn.addCentredArc (pivot.x, pivot.y, R - 14.0f, R - 14.0f, 0.0f, angleFor (-6.0f), angleFor (0.0f), true);
        g.setColour (colours::redZone.withAlpha (0.45f));
        g.strokePath (warn, juce::PathStrokeType (2.0f));
    }
    {
        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, R - 18.0f, R - 18.0f, 0.0f, angleFor (minDb), angleFor (maxDb), true);
        g.setColour (colours::ink);
        g.strokePath (arc, juce::PathStrokeType (1.0f));
    }

    for (int db = -40; db <= 3; ++db)
    {
        const bool major = db == -40 || db == -20 || db == -10 || db == -6 || db == -3 || db == 0 || db == 3;
        const bool minor = db >= -10 || db % 10 == 0 || db == -15;
        if (! major && ! minor) continue;
        const float a = angleFor ((float) db);
        g.setColour (db > 0 ? colours::redZone : colours::ink);
        g.drawLine (juce::Line<float> (pt (a, R - 18.0f), pt (a, R - (major ? 28.0f : 23.0f))), major ? 1.5f : 1.0f);
        if (major)
        {
            const auto p = pt (a, R - 37.0f);
            g.setFont (label (11.0f));
            g.drawText (db > 0 ? "+" + juce::String (db) : juce::String (db), juce::Rectangle<float> (24.0f, 12.0f).withCentre (p), juce::Justification::centred);
        }
    }

    g.setColour (colours::inkSoft);
    g.setFont (label (11.0f));
    g.drawText (tr (lang, "peak"), face.getX(), face.getBottom() - 30.0f, W, 12.0f, juce::Justification::centred);

    // needle
    const float a = angleFor (needleDb);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine (juce::Line<float> (pivot.translated (2.0f, 2.0f), pt (a, R - 8.0f).translated (2.0f, 2.0f)), 1.6f);
    g.setColour (colours::ink);
    g.drawLine (juce::Line<float> (pivot, pt (a, R - 8.0f)), 1.4f);

    // needle housing
    g.setColour (colours::ink);
    g.fillEllipse (pivot.x - 46.0f, pivot.y - 46.0f, 92.0f, 92.0f);
    g.restoreState();

    // over lamp
    const float lx = face.getRight() - 13.0f, ly = face.getY() + 11.0f;
    if (overHold > 0)
    {
        g.setColour (colours::lamp.withAlpha (0.35f));
        g.fillEllipse (lx - 9.0f, ly - 9.0f, 18.0f, 18.0f);
    }
    g.setColour (overHold > 0 ? colours::lamp : juce::Colour (0xff6e3a33));
    g.fillEllipse (lx - 4.5f, ly - 4.5f, 9.0f, 9.0f);
}

//==============================================================================
Content::Content (BarajaProcessor& p)
    : processor (p), transport (p)
{
    setLookAndFeel (&lnf);
    lang = processor.getLanguage();

    for (auto* b : { &langEn, &langEs })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }
    langEn.onClick = [this] { setLanguage (Lang::en); };
    langEs.onClick = [this] { setLanguage (Lang::es); };

    for (int i = 0; i < processor.getNumPrograms(); ++i)
    {
        auto* b = presetButtons.add (new juce::TextButton());
        b->onClick = [this, i] { processor.applyPreset (i); shownProgram = -2; highlightPreset(); };
        addAndMakeVisible (b);
    }

    addAndMakeVisible (transport);
    addAndMakeVisible (meter);

    auto onUser = [this] { shownProgram = -1; highlightPreset(); };
    const std::vector<std::pair<const char*, float>> knobIds {
        { "position", 58 }, { "width", 58 },
        { "size", 58 }, { "sizeVar", 58 }, { "density", 58 }, { "jitter", 58 }, { "shape", 58 },
        { "pitch", 58 }, { "pitchVar", 58 }, { "drift", 58 }, { "driftRate", 58 }, { "reverse", 58 },
        { "spread", 50 }, { "center", 50 }, { "rotation", 50 },
        { "ampVar", 44 }, { "mix", 44 }, { "gain", 44 },
    };
    for (const auto& [id, size] : knobIds)
    {
        auto* k = knobs.add (new Knob (processor, id, size));
        k->onUserChange = onUser;
        addAndMakeVisible (k);
    }

    memorySwitch = std::make_unique<Switch> (processor, "memory", std::vector<const char*> {}, "length");
    juce::StringArray memLabels;
    for (int sec : memoryChoices()) memLabels.add (juce::String (sec));
    memLabels.set (memLabels.size() - 1, memLabels[memLabels.size() - 1] + " s");
    memorySwitch->setPlainLabels (memLabels);
    vdirSwitch  = std::make_unique<Switch> (processor, "vdir", std::vector<const char*> { "both", "up", "down" }, "direction");
    quantSwitch = std::make_unique<Switch> (processor, "quant", std::vector<const char*> { "free", "semi", "oct" }, "steps");
    spaceSwitch = std::make_unique<Switch> (processor, "spaceMode", std::vector<const char*> { "points", "ring" }, "mode");
    for (auto* s : { memorySwitch.get(), vdirSwitch.get(), quantSwitch.get(), spaceSwitch.get() })
    {
        s->onUserChange = onUser;
        addAndMakeVisible (s);
    }
    memorySwitch->onUserChange = {};

    freezeButton.setClickingTogglesState (true);
    freezeButton.getProperties().set ("freeze", true);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "freeze", freezeButton);
    addAndMakeVisible (freezeButton);

    setLanguage (lang);
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

Content::~Content()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

Knob* Content::knob (const juce::String& id) const
{
    const auto& ids = std::vector<const char*> { "position", "width", "size", "sizeVar", "density", "jitter", "shape",
                                                 "pitch", "pitchVar", "drift", "driftRate", "reverse",
                                                 "spread", "center", "rotation", "ampVar", "mix", "gain" };
    for (size_t i = 0; i < ids.size(); ++i) if (id == ids[i]) return knobs[(int) i];
    return nullptr;
}

void Content::setLanguage (Lang l)
{
    lang = l;
    processor.setLanguage (l);
    langEn.setToggleState (l == Lang::en, juce::dontSendNotification);
    langEs.setToggleState (l == Lang::es, juce::dontSendNotification);
    for (int i = 0; i < presetButtons.size(); ++i)
    {
        const auto& pr = presets()[(size_t) i];
        presetButtons[i]->setButtonText (u8 (l == Lang::es ? pr.es : pr.en));
    }
    for (auto* k : knobs) k->setLanguage (l);
    for (auto* s : { memorySwitch.get(), vdirSwitch.get(), quantSwitch.get(), spaceSwitch.get() }) s->setLanguage (l);
    transport.setLanguage (l);
    meter.setLanguage (l);
    freezeButton.setButtonText (tr (l, "freeze"));
    freezeButton.setTooltip (l == Lang::es ? u8 ("Deja de grabar y sigue barajando lo que quedó en la cinta")
                                           : u8 ("Stops recording and keeps shuffling what is on the tape"));
    resized();
    repaint();
}

void Content::highlightPreset()
{
    const int current = shownProgram == -1 ? -1 : processor.getCurrentProgram();
    for (int i = 0; i < presetButtons.size(); ++i)
        presetButtons[i]->setToggleState (i == current, juce::dontSendNotification);
}

juce::Rectangle<int> Content::sectionArea (int column) const
{
    static const int widths[] { 206, 246, 246, 186, 172 };
    int x = 26;
    for (int i = 0; i < column; ++i) x += widths[i] + 18;
    return { x, 424, widths[column], 356 };
}

void Content::timerCallback()
{
    const bool fresh = processor.readTelemetry (telemetry);
    const bool frozen = processor.apvts.getRawParameterValue ("freeze")->load() > 0.5f;
    transport.update (telemetry, fresh, ! frozen);
    meter.push (telemetry.outPeak);
    for (auto* s : { memorySwitch.get(), vdirSwitch.get(), quantSwitch.get(), spaceSwitch.get() }) s->repaint();

    const int outs = processor.getTotalNumOutputChannels();
    if (outs != lastOutputs)
    {
        lastOutputs = outs;
        const bool multi = outs >= 3;
        knob ("center")->setEnabled (multi);
        knob ("rotation")->setEnabled (multi);
        spaceSwitch->setEnabled (multi);
        spaceSwitch->setAlpha (multi ? 1.0f : 0.4f);
        repaint();
    }
}

void Content::paint (juce::Graphics& g)
{
    juce::ColourGradient panel (colours::panelTop, 0.0f, 0.0f, colours::panelBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill (panel);
    g.fillAll();
    // faint brushed grain
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int y = 0; y < getHeight(); y += 3) g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    drawScrew (g, 12, 12, 5, 0.4f);
    drawScrew (g, getWidth() - 12.0f, 12, 5, 1.9f);
    drawScrew (g, 12, getHeight() - 12.0f, 5, 2.6f);
    drawScrew (g, getWidth() - 12.0f, getHeight() - 12.0f, 5, 1.1f);

    g.setColour (colours::ink);
    g.setFont (heavy (46.0f));
    g.drawText ("BARAJA", 26, 16, 220, 48, juce::Justification::centredLeft);
    g.setColour (colours::inkSoft);
    g.setFont (label (14.0f));
    g.drawText (caps (tr (lang, "subtitle")) + "  ·  v" + JucePlugin_VersionString, 196, 36, 420, 20, juce::Justification::centredLeft);

    const int outs = processor.getTotalNumOutputChannels();
    const juce::String info = juce::String (outs) + " " + caps (tr (lang, "outputs"))
                            + "  ·  " + localNumber (juce::String (processor.getSampleRate() / 1000.0, 1), lang) + " KHZ";
    g.drawText (info, getWidth() - 460, 30, 300, 20, juce::Justification::centredRight);
    g.setColour (colours::lamp.withAlpha (0.35f));
    g.fillEllipse (getWidth() - 46.0f, 31.0f, 18.0f, 18.0f);
    g.setColour (colours::lamp);
    g.fillEllipse (getWidth() - 41.0f, 36.0f, 8.0f, 8.0f);

    g.setColour (colours::inkSoft);
    g.setFont (label (13.0f));
    g.drawText (caps (tr (lang, "presets")), 26, 74, 80, 26, juce::Justification::centredLeft);

    const char* titles[] { "memory", "fragments", "pitch", "space", "output" };
    for (int i = 0; i < 5; ++i)
    {
        const auto a = sectionArea (i);
        g.setColour (colours::ink);
        g.setFont (heavy (16.0f));
        g.drawText (caps (tr (lang, titles[i])), a.getX(), a.getY(), a.getWidth(), 20, juce::Justification::centredLeft);
        g.fillRect (a.getX(), a.getY() + 22, a.getWidth(), 2);
    }

    if (outs < 3)
    {
        const auto a = sectionArea (3);
        g.setColour (colours::inkSoft);
        g.setFont (label (12.0f));
        g.drawFittedText (tr (lang, "stereoOnly"), a.getX(), a.getY() + 214, a.getWidth(), 34, juce::Justification::topLeft, 3);
    }
}

void Content::resized()
{
    langEs.setBounds (getWidth() - 100, 30, 34, 22);
    langEn.setBounds (getWidth() - 140, 30, 34, 22);

    int x = 104;
    for (auto* b : presetButtons)
    {
        const int w = (int) juce::GlyphArrangement::getStringWidth (label (13.5f), caps (b->getButtonText())) + 22;
        b->setBounds (x, 74, w, 26);
        x += w + 6;
    }

    transport.setBounds (26, 110, getWidth() - 52, 300);

    auto place = [this] (const char* id, int cx, int y) { knob (id)->setBounds (cx - 41, y, 82, 100); };

    // memory
    {
        const auto a = sectionArea (0);
        memorySwitch->setBounds (a.getX(), a.getY() + 34, a.getWidth(), 44);
        place ("position", a.getX() + 52, a.getY() + 90);
        place ("width", a.getX() + 154, a.getY() + 90);
        freezeButton.setBounds (a.getX(), a.getY() + 204, a.getWidth(), 50);
    }
    // fragments and pitch: 3 × 2 grids
    for (int col : { 1, 2 })
    {
        const auto a = sectionArea (col);
        const char* ids1[] { "size", "sizeVar", "density", "jitter", "shape" };
        const char* ids2[] { "pitch", "pitchVar", "drift", "driftRate", "reverse" };
        for (int i = 0; i < 5; ++i)
            place (col == 1 ? ids1[i] : ids2[i], a.getX() + 41 + (i % 3) * 82, a.getY() + 34 + (i / 3) * 104);
    }
    {
        const auto a = sectionArea (2);
        vdirSwitch->setBounds (a.getX(), a.getY() + 250, 120, 44);
        quantSwitch->setBounds (a.getX() + 126, a.getY() + 250, 120, 44);
    }
    // space
    {
        const auto a = sectionArea (3);
        spaceSwitch->setBounds (a.getX(), a.getY() + 34, a.getWidth(), 44);
        const char* ids[] { "spread", "center", "rotation" };
        for (int i = 0; i < 3; ++i) knob (ids[i])->setBounds (a.getX() + i * 62, a.getY() + 96, 62, 92);
    }
    // output
    {
        const auto a = sectionArea (4);
        meter.setBounds (a.getX(), a.getY() + 34, a.getWidth(), 126);
        const char* ids[] { "ampVar", "mix", "gain" };
        for (int i = 0; i < 3; ++i) knob (ids[i])->setBounds (a.getX() + i * 57, a.getY() + 174, 57, 84);
    }
}

} // namespace baraja::tape

//==============================================================================
BarajaEditor::BarajaEditor (BarajaProcessor& p)
    : AudioProcessorEditor (p), content (p)
{
    using C = baraja::tape::Content;
    addAndMakeVisible (content);
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) C::baseWidth / C::baseHeight);
        c->setSizeLimits (C::baseWidth * 6 / 10, C::baseHeight * 6 / 10, C::baseWidth * 3 / 2, C::baseHeight * 3 / 2);
    }
    setSize (C::baseWidth * 9 / 10, C::baseHeight * 9 / 10);
}

void BarajaEditor::resized()
{
    using C = baraja::tape::Content;
    const float scale = (float) getWidth() / (float) C::baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, C::baseWidth, C::baseHeight);
}
