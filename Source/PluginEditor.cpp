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

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.75f);
    g.setColour (colours::cream.withAlpha (box.isEnabled() ? 0.55f : 0.25f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (colours::ink.withAlpha (box.isMouseOver (true) ? 1.0f : 0.75f));
    g.drawRoundedRectangle (r, 3.0f, 1.3f);
    juce::Path p;
    const float ax = (float) w - 12.0f, ay = h * 0.5f;
    p.startNewSubPath (ax - 4.0f, ay - 2.0f);
    p.lineTo (ax, ay + 2.0f);
    p.lineTo (ax + 4.0f, ay - 2.0f);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& l)
{
    l.setBounds (6, 1, box.getWidth() - 24, box.getHeight() - 2);
    l.setFont (getComboBoxFont (box));
    l.setColour (juce::Label::textColourId, colours::ink);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = y + h * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, cy - 3.0f, (float) w, 6.0f);
    g.setColour (colours::smoked);
    g.fillRoundedRectangle (track, 3.0f);
    const float mid = x + w * 0.5f;
    g.setColour (colours::inkSoft);
    g.fillRect (mid - 0.5f, cy - 7.0f, 1.0f, 14.0f);
    g.setColour (s.isEnabled() ? colours::amber : colours::smokedText);
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mid, pos), cy - 2.0f, std::abs (pos - mid), 4.0f), 2.0f);
    g.setColour (colours::ink);
    g.fillRoundedRectangle (pos - 4.0f, cy - 9.0f, 8.0f, 18.0f, 2.0f);
    g.setColour (colours::cream);
    g.fillRect (pos - 0.5f, cy - 6.0f, 1.0f, 12.0f);
}

//==============================================================================
Knob::Knob (BarajaProcessor& p, const juce::String& id, float size)
    : processor (p), paramId (id), dest (baraja::destIndex (id)), param (p.apvts.getParameter (id)), spec (baraja::spec (id)), knobSize (size)
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
    if (dest >= 0) slider.onPopup = [this] { showModMenu(); };
    addAndMakeVisible (slider);
    setLanguage (Lang::en);
}

void Knob::setLanguage (Lang l)
{
    lang = l;
    juce::String note = u8 (lang == Lang::es ? spec.noteEs : spec.noteEn);
    slider.setTooltip ((note.isEmpty() ? juce::String() : note + ". ") + tr (lang, "dblclick") + "." + (dest >= 0 ? " " + tr (lang, "rightClickMod") : juce::String()));
    repaint();
}

bool Knob::isModulated() const { return dest >= 0 && processor.getModDepth (dest) > 0.0f; }

void Knob::showModMenu()
{
    const int current = processor.sourceOf (paramId);
    juce::PopupMenu m;
    m.addSectionHeader (caps (tr (lang, "modWith")));
    const char* keys[] { "lfo1", "lfo2", "frag", "follow" };
    for (int s = 1; s <= 4; ++s) m.addItem (s, tr (lang, keys[s - 1]), true, current == s);
    m.addSeparator();
    m.addItem (10, tr (lang, "removeMod"), current > 0);
    m.addItem (11, tr (lang, "showModPage"));
    juce::Component::SafePointer<Knob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider), [safe] (int r) {
        if (safe == nullptr) return;
        if (r >= 1 && r <= 4) safe->processor.assignModulation (safe->paramId, r);
        if (r == 10) safe->processor.removeModulation (safe->paramId);
        if (r == 11 && safe->onShowModulation) safe->onShowModulation();
        safe->repaint();
    });
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    if (! isModulated()) return;
    const auto b = slider.getBounds().toFloat();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 4.5f;
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float start = pi * 1.25f, end = pi * 2.75f;
    const float base = param->getValue(), depth = processor.getModDepth (dest);
    const float lo = juce::jlimit (0.0f, 1.0f, base - depth), hi = juce::jlimit (0.0f, 1.0f, base + depth);
    juce::Path arc;
    arc.addCentredArc (cx, cy, r, r, 0.0f, start + lo * (end - start), start + hi * (end - start), true);
    g.setColour (colours::amber.withAlpha (isEnabled() ? 0.95f : 0.4f));
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float now = processor.getModulatedNorm (dest);
    if (now >= 0.0f)
    {
        const float a = start + now * (end - start);
        const float dx = cx + std::sin (a) * r, dy = cy - std::cos (a) * r;
        g.setColour (colours::ink);
        g.fillEllipse (dx - 4.0f, dy - 4.0f, 8.0f, 8.0f);
        g.setColour (colours::glint);
        g.fillEllipse (dx - 2.5f, dy - 2.5f, 5.0f, 5.0f);
    }
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
    juce::String text = param->getCurrentValueAsText();
    if (paramId == "size")
    {
        const float maxMs = (float) processor.maxFragmentMs();
        if (maxMs > 1.0f && param->convertFrom0to1 (param->getValue()) > maxMs)
            text = formatValue (spec, maxMs) + " " + tr (lang, "maxShort");
    }
    g.drawText (localNumber (text, lang), r.removeFromTop (14.0f), juce::Justification::centred);
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

juce::Rectangle<float> Transport::barArea() const
{
    const auto s = stripArea();
    return { s.getX(), 164.0f, s.getWidth(), 6.0f };
}

float Transport::viewNew() const  { return zoomSpan > 0.0f ? zoomNew : 0.0f; }
float Transport::viewSpan() const { return zoomSpan > 0.0f ? zoomSpan : memoryMs(); }

// The tape shows "now" at the right; zoomed, it shows viewSpan() ms ending viewNew() ms ago.
float Transport::xForMs (float ms) const
{
    const auto s = stripArea();
    return s.getX() + s.getWidth() * (1.0f - (ms - viewNew()) / viewSpan());
}

float Transport::msForX (float x) const
{
    const auto s = stripArea();
    return juce::jlimit (0.0f, memoryMs(), viewNew() + (1.0f - (x - s.getX()) / s.getWidth()) * viewSpan());
}

void Transport::zoomAround (float ms, float factor)
{
    const float mem = memoryMs();
    const float span = juce::jlimit (juce::jmin (50.0f, mem), mem, viewSpan() * factor);
    if (span >= mem - 0.5f) { zoomSpan = 0.0f; zoomNew = 0.0f; repaint(); return; }
    // keep the point under the mouse where it is
    const float newer = ms - (ms - viewNew()) * span / viewSpan();
    zoomSpan = span;
    zoomNew = juce::jlimit (0.0f, mem - span, newer);
    repaint();
}

void Transport::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto s = stripArea();
    if (e.position.y < barArea().getY() - 6.0f || e.position.y > s.getBottom() + 26.0f) return;
    const bool horizontal = std::abs (w.deltaX) > std::abs (w.deltaY) || e.mods.isShiftDown();
    if (horizontal)
    {
        if (zoomSpan <= 0.0f) return;
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        zoomNew = juce::jlimit (0.0f, memoryMs() - zoomSpan, zoomNew + d * zoomSpan * 0.6f);
        repaint();
    }
    else zoomAround (msForX (e.position.x), std::pow (2.0f, -w.deltaY * 3.0f));
}

void Transport::mouseMagnify (const juce::MouseEvent& e, float scale)
{
    if (scale > 0.0f) zoomAround (msForX (e.position.x), 1.0f / scale);
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
    scrolling = false;
    if (zoomSpan > 0.0f && barArea().expanded (0.0f, 6.0f).contains (e.position))
    {
        // the overview bar: grab the view, or jump to where it was clicked
        const auto bar = barArea();
        const float ms = (1.0f - (e.position.x - bar.getX()) / bar.getWidth()) * memoryMs();
        if (ms < zoomNew || ms > zoomNew + zoomSpan) zoomNew = juce::jlimit (0.0f, memoryMs() - zoomSpan, ms - zoomSpan * 0.5f);
        scrolling = true;
        scrollGrab = ms - zoomNew;
        repaint();
        return;
    }
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
    if (scrolling)
    {
        const auto bar = barArea();
        const float ms = (1.0f - (e.position.x - bar.getX()) / bar.getWidth()) * memoryMs();
        zoomNew = juce::jlimit (0.0f, memoryMs() - zoomSpan, ms - scrollGrab);
        repaint();
        return;
    }
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
    if (zoomSpan > 0.0f && barArea().expanded (0.0f, 6.0f).contains (e.position)) { zoomSpan = 0.0f; zoomNew = 0.0f; repaint(); return; }
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
    tele.headMs = t.headMs;
    if (peaksFresh)
    {
        std::copy (t.peaks, t.peaks + Telemetry::numPeaks, tele.peaks);
        hasPeaks = true;
    }
    if (running) reelAngle += 0.035f;   // the reels turn while the memory records
    processor.setPeakView (viewNew(), viewSpan());
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

    if (zoomSpan > 0.0f)
    {
        // overview of the whole memory: the bright part is what the tape shows
        const auto bar = barArea();
        g.setColour (juce::Colour (0xff141210));
        g.fillRoundedRectangle (bar, 3.0f);
        const float x0 = bar.getX() + bar.getWidth() * (1.0f - (zoomNew + zoomSpan) / mem);
        const float x1 = bar.getX() + bar.getWidth() * (1.0f - zoomNew / mem);
        g.setColour (colours::printed);
        g.fillRoundedRectangle (juce::Rectangle<float> (x0, bar.getY(), juce::jmax (6.0f, x1 - x0), bar.getHeight()), 3.0f);
    }
    else
    {
        g.setColour (colours::smokedText);
        g.setFont (label (12.0f));
        g.drawText (tr (lang, "dragHint"), 0.0f, 162.0f, W, 14.0f, juce::Justification::centred);
    }

    // the tape: the memory, oldest at the left, "now" at the right
    const auto s = stripArea();
    juce::ColourGradient oxide (colours::oxideTop, 0.0f, s.getY(), colours::oxideDark, 0.0f, s.getBottom(), false);
    oxide.addColour (0.5, colours::oxide);
    g.setGradientFill (oxide);
    g.fillRect (s);
    g.saveState();
    g.reduceClipRegion (s.expanded (10.0f, 12.0f).toNearestInt());

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

    // the read head: fragments gather around it as Order rises
    const float order = processor.apvts.getRawParameterValue ("order")->load() / 100.0f;
    if (order > 0.005f)
    {
        const float hx = xForMs (tele.headMs);
        g.setColour (colours::cream.withAlpha (0.25f + 0.7f * order));
        g.fillRect (hx - 1.0f, s.getY(), 2.0f, s.getHeight());
        juce::Path tri;
        tri.addTriangle (hx - 6.0f, s.getY() - 1.0f, hx + 6.0f, s.getY() - 1.0f, hx, s.getY() + 8.0f);
        tri.addTriangle (hx - 6.0f, s.getBottom() + 1.0f, hx + 6.0f, s.getBottom() + 1.0f, hx, s.getBottom() - 8.0f);
        g.fillPath (tri);
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

    g.restoreState();

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

    // ruler, for what the tape shows
    const float steps[] = { 1, 2, 5, 10, 20, 50, 100, 250, 500, 1000, 2000, 5000, 10000 };
    const float span = viewSpan(), first = viewNew();
    float st = 10000.0f;
    for (auto v : steps) if (span / v <= 12.0f) { st = v; break; }
    g.setFont (mono (11.5f));
    bool unitShown = false;
    for (int k = (int) std::ceil (first / st - 1.0e-4f); k * st <= first + span + 0.01f; ++k)
    {
        const float ms = k * st, x = xForMs (ms);
        g.setColour (colours::smokedText);
        g.fillRect (x - 0.5f, s.getBottom() + 3.0f, 1.0f, 4.0f);
        const bool inMs = st < 100.0f;
        const juce::String num = inMs ? juce::String (juce::roundToInt (ms)) : juce::String (ms / 1000.0f, std::fmod (ms, 1000.0f) > 0 ? (st < 1000.0f ? 2 : 1) : 0);
        juce::String t = k == 0 ? tr (lang, "now") : juce::String::fromUTF8 ("−") + localNumber (num, lang);
        if (k != 0 && ! unitShown && x < s.getRight() - 40.0f) { t += inMs ? " ms" : " s"; unitShown = true; }
        if (x > s.getRight() - 30.0f) g.drawText (t, (int) x - 60, (int) s.getBottom() + 9, 60, 13, juce::Justification::right);
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
namespace
{
    // Small drawings of the LFO shapes, in a 0…1 box.
    juce::Path shapeIcon (int shape, juce::Rectangle<float> r)
    {
        juce::Path p;
        auto pt = [&] (float x, float y) { return juce::Point<float> (r.getX() + x * r.getWidth(), r.getY() + (0.5f - 0.5f * y) * r.getHeight()); };
        if (shape == modlib::steps)
        {
            const float v[] { 0.2f, 0.9f, -0.6f, 0.4f, -0.9f };
            p.startNewSubPath (pt (0.0f, v[0]));
            for (int i = 0; i < 5; ++i) { p.lineTo (pt (i / 5.0f, v[i])); p.lineTo (pt ((i + 1) / 5.0f, v[i])); }
            return p;
        }
        const float a[] { 0.2f, 0.9f, -0.6f, 0.4f, -0.9f, 0.1f };
        for (int i = 0; i <= 60; ++i)
        {
            const float t = i / 60.0f;
            float y = 0.0f;
            if (shape == modlib::smooth)
            {
                const int k = juce::jmin (4, (int) (t * 5.0f));
                const float f = t * 5.0f - k, sm = 0.5f - 0.5f * std::cos (f * pi);
                y = a[k] + (a[k + 1] - a[k]) * sm;
            }
            else y = modlib::shapeValue (shape, t * 2.0, 0.0f, 0.0f);
            if (i == 0) p.startNewSubPath (pt (t, y)); else p.lineTo (pt (t, y));
        }
        return p;
    }
}

ShapePicker::ShapePicker (BarajaProcessor& p, const juce::String& id) : param (p.apvts.getParameter (id))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ShapePicker::paint (juce::Graphics& g)
{
    const int n = modlib::numShapes;
    const int current = juce::roundToInt (param->getValue() * (n - 1));
    const float w = getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<float> (i * w, 0.0f, w, (float) getHeight()).reduced (2.0f, 0.0f);
        const bool on = i == current;
        if (on) { g.setColour (colours::ink); g.fillRoundedRectangle (cell, 3.0f); }
        g.setColour (colours::ink);
        g.drawRoundedRectangle (cell.reduced (0.6f), 3.0f, 1.3f);
        g.setColour (on ? colours::cream : colours::ink);
        g.strokePath (shapeIcon (i, cell.reduced (8.0f, 9.0f)), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void ShapePicker::mouseDown (const juce::MouseEvent& e)
{
    const int n = modlib::numShapes;
    const int i = juce::jlimit (0, n - 1, (int) (e.position.x / getWidth() * n));
    param->beginChangeGesture();
    param->setValueNotifyingHost ((float) i / (float) (n - 1));
    param->endChangeGesture();
    repaint();
}

//==============================================================================
RouteRow::RouteRow (BarajaProcessor& p, int i) : processor (p), index (i)
{
    source.addItemList (modSourceChoices(), 1);
    juce::StringArray names;
    for (const auto& d : modDestinations()) names.add (u8 (spec (d.id).en));
    target.addItemList (names, 1);
    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    amount.setDoubleClickReturnValue (true, 0.0);
    amount.onValueChange = [this] { repaint(); };
    sourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, routeId (i, "Src"), source);
    targetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, routeId (i, "Dest"), target);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, routeId (i, "Amt"), amount);
    source.onChange = [this] { repaint(); };
    for (auto* c : { (juce::Component*) &source, (juce::Component*) &target, (juce::Component*) &amount }) addAndMakeVisible (c);
}

void RouteRow::setLanguage (Lang l)
{
    lang = l;
    // read the selection before renaming: a combo box forgets it when its text changes
    const int srcIndex = juce::roundToInt (processor.apvts.getRawParameterValue (routeId (index, "Src"))->load());
    const int dstIndex = juce::roundToInt (processor.apvts.getRawParameterValue (routeId (index, "Dest"))->load());
    const char* keys[] { "none", "lfo1", "lfo2", "frag", "follow" };
    for (int k = 0; k < 5; ++k) source.changeItemText (k + 1, tr (l, keys[k]));
    const auto& d = modDestinations();
    for (int k = 0; k < (int) d.size(); ++k) target.changeItemText (k + 1, u8 (l == Lang::es ? spec (d[(size_t) k].id).es : spec (d[(size_t) k].id).en));
    source.setText (source.getItemText (srcIndex), juce::dontSendNotification);
    target.setText (target.getItemText (dstIndex), juce::dontSendNotification);
    amount.setTooltip (u8 (l == Lang::es ? routeAmountSpec().noteEs : routeAmountSpec().noteEn));
    repaint();
}

void RouteRow::resized()
{
    source.setBounds (30, 3, 110, 28);
    target.setBounds (164, 3, 170, 28);
    amount.setBounds (346, 3, 150, 28);
}

void RouteRow::paint (juce::Graphics& g)
{
    const bool active = source.getSelectedItemIndex() > 0;
    g.setColour (colours::ink.withAlpha (active ? 0.07f : 0.03f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    g.setColour (colours::inkSoft);
    g.setFont (mono (13.0f));
    g.drawText (juce::String (index + 1), 0, 0, 26, getHeight(), juce::Justification::centred);
    g.setColour (colours::ink);
    g.drawText (u8 ("→"), 140, 0, 24, getHeight(), juce::Justification::centred);
    g.drawText (localNumber (formatValue (routeAmountSpec(), (float) amount.getValue()), lang), getWidth() - 64, 0, 60, getHeight(), juce::Justification::centredRight);
}

//==============================================================================
ModPanel::ModPanel (BarajaProcessor& p) : processor (p)
{
    for (int l = 0; l < 2; ++l)
    {
        const juce::String n (l + 1);
        shapes[l] = std::make_unique<ShapePicker> (p, "lfo" + n + "Shape");
        syncs[l] = std::make_unique<Switch> (p, "lfo" + n + "Sync", std::vector<const char*> { "freeLfo", "host" }, "sync");
        divisions[l].addItemList (divisionChoices(), 1);
        divAttachments[l] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "lfo" + n + "Div", divisions[l]);
        lfoKnobs.add (new Knob (p, "lfo" + n + "Rate", 52));
        lfoKnobs.add (new Knob (p, "lfo" + n + "Phase", 52));
        for (auto* c : { (juce::Component*) shapes[l].get(), (juce::Component*) syncs[l].get(), (juce::Component*) &divisions[l] }) addChildComponent (c);
    }
    for (auto* k : lfoKnobs) addChildComponent (k);
    for (const char* id : { "folAttack", "folRelease", "folGain" }) addChildComponent (followKnobs.add (new Knob (p, id, 52)));
    for (int i = 0; i < numRoutes; ++i) addAndMakeVisible (rows.add (new RouteRow (p, i)));
    showSource (0);
}

juce::Rectangle<int> ModPanel::tabArea (int i) const { return { i * 128, 34, 120, 32 }; }
juce::Rectangle<float> ModPanel::screenArea() const { return { 0.0f, 102.0f, 504.0f, 104.0f }; }

void ModPanel::showSource (int s)
{
    selected = s;
    trail.clear();
    for (int l = 0; l < 2; ++l)
    {
        const bool on = s == l;
        shapes[l]->setVisible (on);
        syncs[l]->setVisible (on);
        lfoKnobs[l * 2 + 1]->setVisible (on);
        const bool synced = processor.apvts.getRawParameterValue ("lfo" + juce::String (l + 1) + "Sync")->load() > 0.5f;
        lfoKnobs[l * 2]->setVisible (on && ! synced);
        divisions[l].setVisible (on && synced);
    }
    for (auto* k : followKnobs) k->setVisible (s == 3);
    repaint();
}

void ModPanel::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 4; ++i)
        if (tabArea (i).contains (e.getPosition())) showSource (i);
}

void ModPanel::setLanguage (Lang l)
{
    lang = l;
    for (auto* k : lfoKnobs) k->setLanguage (l);
    for (auto* k : followKnobs) k->setLanguage (l);
    for (auto& sw : syncs) sw->setLanguage (l);
    for (auto* r : rows) r->setLanguage (l);
    for (int b = 0; b < 2; ++b)
    {
        auto& box = divisions[b];
        const int current = juce::roundToInt (processor.apvts.getRawParameterValue ("lfo" + juce::String (b + 1) + "Div")->load());
        for (int i = 0; i < modlib::numDivisions; ++i)
        {
            juce::String name = modlib::divisionName (i);
            if (l == Lang::es) name = name.replace ("bars", u8 ("compases")).replace ("bar", u8 ("compás"));
            box.changeItemText (i + 1, name);
        }
        box.setText (box.getItemText (current), juce::dontSendNotification);
    }
    repaint();
}

void ModPanel::update()
{
    // the sync switch decides between the rate knob and the division menu
    for (int l = 0; l < 2; ++l)
        if (selected == l)
        {
            const bool synced = processor.apvts.getRawParameterValue ("lfo" + juce::String (l + 1) + "Sync")->load() > 0.5f;
            if (divisions[l].isVisible() != synced) { lfoKnobs[l * 2]->setVisible (! synced); divisions[l].setVisible (synced); }
            syncs[l]->repaint();
            shapes[l]->repaint();
        }
    trail.push_back (processor.getSourceValue (selected + 1));
    if (trail.size() > 160) trail.erase (trail.begin());
    for (auto* k : lfoKnobs) if (k->isModulated()) k->repaint();
    repaint (screenArea().toNearestInt().expanded (2));
}

void ModPanel::resized()
{
    for (int l = 0; l < 2; ++l)
    {
        shapes[l]->setBounds (0, 222, 300, 40);
        syncs[l]->setBounds (322, 214, 182, 44);
        lfoKnobs[l * 2]->setBounds (0, 272, 82, 84);
        divisions[l].setBounds (6, 298, 120, 30);
        lfoKnobs[l * 2 + 1]->setBounds (140, 272, 82, 84);
    }
    for (int i = 0; i < followKnobs.size(); ++i) followKnobs[i]->setBounds (i * 96, 226, 82, 84);
    for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (548, 60 + i * 37, 580, 34);
}

void ModPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::ink);
    g.setFont (heavy (16.0f));
    g.drawText (caps (tr (lang, "sources")), 0, 0, 504, 20, juce::Justification::centredLeft);
    g.fillRect (0, 22, 504, 2);
    g.drawText (caps (tr (lang, "routes")), 548, 0, 580, 20, juce::Justification::centredLeft);
    g.fillRect (548, 22, 580, 2);
    g.setColour (colours::inkSoft);
    g.setFont (label (12.0f));
    g.drawText (caps (tr (lang, "source")), 578, 34, 110, 20, juce::Justification::centredLeft);
    g.drawText (caps (tr (lang, "setting")), 712, 34, 170, 20, juce::Justification::centredLeft);
    g.drawText (caps (tr (lang, "amount")), 894, 34, 150, 20, juce::Justification::centredLeft);

    const char* keys[] { "lfo1", "lfo2", "frag", "follow" };
    const char* help[] { "lfoHelp", "lfoHelp", "fragHelp", "followHelp" };
    for (int i = 0; i < 4; ++i)
    {
        const auto t = tabArea (i).toFloat();
        const bool on = i == selected;
        if (on) { g.setColour (colours::ink); g.fillRoundedRectangle (t, 4.0f); }
        g.setColour (colours::ink);
        g.drawRoundedRectangle (t.reduced (0.7f), 4.0f, 1.4f);
        g.setColour (on ? colours::cream : colours::ink);
        g.setFont (label (14.0f));
        g.drawText (caps (tr (lang, keys[i])), t, juce::Justification::centred);
    }
    g.setColour (colours::inkSoft);
    g.setFont (label (13.5f));
    g.drawText (tr (lang, help[selected]), 0, 74, 504, 22, juce::Justification::centredLeft);

    // the source over the last few seconds
    const auto sc = screenArea();
    g.setColour (colours::smoked);
    g.fillRoundedRectangle (sc, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    const bool unipolar = selected == 3;
    const float zeroY = unipolar ? sc.getBottom() - 10.0f : sc.getCentreY();
    g.fillRect (sc.getX() + 8.0f, zeroY, sc.getWidth() - 16.0f, 1.0f);
    if (trail.size() > 1)
    {
        juce::Path p;
        const float h = unipolar ? sc.getHeight() - 20.0f : sc.getHeight() * 0.5f - 10.0f;
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const float x = sc.getX() + 8.0f + (sc.getWidth() - 16.0f) * i / 159.0f;
            const float y = zeroY - trail[i] * h;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (colours::amber);
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse (sc.getX() + 8.0f + (sc.getWidth() - 16.0f) * (trail.size() - 1) / 159.0f - 3.5f, zeroY - trail.back() * h - 3.5f, 7.0f, 7.0f);
    }

    if (selected == 2)
    {
        g.setColour (colours::inkSoft);
        g.setFont (label (13.5f));
        g.drawFittedText (tr (lang, "fragMore"), 0, 222, 504, 90, juce::Justification::topLeft, 5);
    }
    for (int l = 0; l < 2; ++l)
        if (selected == l && divisions[l].isVisible())
        {
            g.setColour (colours::ink);
            g.setFont (label (13.0f));
            g.drawText (caps (tr (lang, "division")), 6, 276, 120, 18, juce::Justification::centredLeft);
        }
}

//==============================================================================
Content::Content (BarajaProcessor& p)
    : processor (p), modPanel (p), transport (p)
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

    for (auto* b : { &pageControls, &pageModulation })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }
    pageControls.onClick = [this] { showPage (0); };
    pageModulation.onClick = [this] { showPage (1); };
    addChildComponent (modPanel);

    addAndMakeVisible (transport);
    addAndMakeVisible (meter);

    auto onUser = [this] { shownProgram = -1; highlightPreset(); };
    const std::vector<std::pair<const char*, float>> knobIds {
        { "position", 58 }, { "width", 58 }, { "order", 58 }, { "speed", 58 },
        { "size", 58 }, { "sizeVar", 58 }, { "density", 58 }, { "jitter", 58 }, { "shape", 58 },
        { "pitch", 58 }, { "pitchVar", 58 }, { "drift", 58 }, { "driftRate", 58 }, { "reverse", 58 },
        { "spread", 50 }, { "center", 50 }, { "rotation", 50 },
        { "ampVar", 44 }, { "mix", 44 }, { "gain", 44 },
    };
    for (const auto& [id, size] : knobIds)
    {
        auto* k = knobs.add (new Knob (processor, id, size));
        k->onUserChange = onUser;
        k->onShowModulation = [this] { showPage (1); };
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
    showPage (0);
    startTimerHz (30);
}

void Content::showPage (int p)
{
    page = p;
    pageControls.setToggleState (p == 0, juce::dontSendNotification);
    pageModulation.setToggleState (p == 1, juce::dontSendNotification);
    for (auto* k : knobs) k->setVisible (p == 0);
    for (auto* s : { memorySwitch.get(), vdirSwitch.get(), quantSwitch.get(), spaceSwitch.get() }) s->setVisible (p == 0);
    freezeButton.setVisible (p == 0);
    meter.setVisible (p == 0);
    modPanel.setVisible (p == 1);
    repaint();
}

Content::~Content()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

Knob* Content::knob (const juce::String& id) const
{
    const auto& ids = std::vector<const char*> { "position", "width", "order", "speed", "size", "sizeVar", "density", "jitter", "shape",
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
    modPanel.setLanguage (l);
    pageControls.setButtonText (tr (l, "controls"));
    pageModulation.setButtonText (tr (l, "modulation"));
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
    if (page == 1) modPanel.update();
    else for (auto* k : knobs) if (k->isModulated()) k->repaint();
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
    for (int i = 0; i < 5 && page == 0; ++i)
    {
        const auto a = sectionArea (i);
        g.setColour (colours::ink);
        g.setFont (heavy (16.0f));
        g.drawText (caps (tr (lang, titles[i])), a.getX(), a.getY(), a.getWidth(), 20, juce::Justification::centredLeft);
        g.fillRect (a.getX(), a.getY() + 22, a.getWidth(), 2);
    }

    if (outs < 3 && page == 0)
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
    {
        const int w1 = (int) juce::GlyphArrangement::getStringWidth (label (13.5f), caps (pageModulation.getButtonText())) + 26;
        const int w0 = (int) juce::GlyphArrangement::getStringWidth (label (13.5f), caps (pageControls.getButtonText())) + 26;
        pageControls.setBounds (560, 28, w0, 26);
        pageModulation.setBounds (pageControls.getRight() + 6, 28, w1, 26);
    }
    modPanel.setBounds (26, 424, getWidth() - 52, 356);

    auto place = [this] (const char* id, int cx, int y) { knob (id)->setBounds (cx - 41, y, 82, 100); };

    // memory
    {
        const auto a = sectionArea (0);
        memorySwitch->setBounds (a.getX(), a.getY() + 34, a.getWidth(), 44);
        place ("position", a.getX() + 52, a.getY() + 84);
        place ("width", a.getX() + 154, a.getY() + 84);
        place ("order", a.getX() + 52, a.getY() + 186);
        place ("speed", a.getX() + 154, a.getY() + 186);
        freezeButton.setBounds (a.getX(), a.getY() + 294, a.getWidth(), 46);
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
