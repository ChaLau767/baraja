// Baraja — interfaz
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"

namespace baraja::ui
{

juce::Font font (float size, bool bold)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

static juce::String es (float v, int decimals)
{
    return juce::String (v, decimals).replaceCharacter ('.', ',');
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg);
    setColour (juce::Label::textColourId, colours::ink);
    setColour (juce::TextButton::buttonColourId, colours::panel);
    setColour (juce::TextButton::buttonOnColourId, colours::ink);
    setColour (juce::TextButton::textColourOffId, colours::ink);
    setColour (juce::TextButton::textColourOnId, colours::bg);
    setColour (juce::ComboBox::backgroundColourId, colours::panel);
    setColour (juce::ComboBox::textColourId, colours::ink);
    setColour (juce::ComboBox::outlineColourId, colours::line);
    setColour (juce::ComboBox::arrowColourId, colours::muted);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::line);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::ink);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::ink);
    setColour (juce::TooltipWindow::outlineColourId, colours::line);
    setColour (juce::Slider::textBoxTextColourId, colours::ink);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = y + h * 0.5f;
    const float x0 = x + 6.0f, x1 = x + w - 6.0f;
    g.setColour (colours::line);
    g.fillRoundedRectangle (x0, cy - 2.0f, x1 - x0, 4.0f, 2.0f);
    g.setColour (s.isEnabled() ? colours::ink : colours::muted);
    g.fillRoundedRectangle (x0, cy - 2.0f, juce::jmax (0.0f, pos - x0), 4.0f, 2.0f);

    const float r = s.isMouseOverOrDragging() ? 8.0f : 7.0f;
    g.setColour (colours::panel);
    g.fillEllipse (pos - r, cy - r, r * 2, r * 2);
    g.setColour (s.hasKeyboardFocus (false) ? colours::grain : colours::ink);
    g.drawEllipse (pos - r, cy - r, r * 2, r * 2, 2.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool freeze = b.getProperties().getWithDefault ("freeze", false);
    const auto fill = on ? (freeze ? colours::rev : colours::ink) : colours::panel;
    g.setColour (down ? fill.darker (0.2f) : fill);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? fill : (over ? colours::ink : colours::line));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int height)
{
    return font (juce::jmin (14.0f, height * 0.5f), true);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.isMouseOver() ? colours::ink : colours::line);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = w - 16.0f, ay = h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (colours::muted);
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (13.0f); }
juce::Font LookAndFeel::getPopupMenuFont() { return font (14.0f); }

//==============================================================================
ParamRow::ParamRow (juce::AudioProcessorValueTreeState& state, const baraja::FloatSpec& spec)
    : param (state.getParameter (spec.id))
{
    name.setText (baraja::u8 (spec.name), juce::dontSendNotification);
    name.setMinimumHorizontalScale (1.0f);
    name.setFont (font (13.5f));
    name.setColour (juce::Label::textColourId, colours::ink);
    name.setInterceptsMouseClicks (false, false);

    value.setFont (font (12.5f));
    value.setColour (juce::Label::textColourId, colours::muted);
    value.setJustificationType (juce::Justification::centredRight);
    value.setInterceptsMouseClicks (false, false);

    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setScrollWheelEnabled (true);
    juce::String tip = baraja::u8 (spec.note);
    slider.setTooltip ((tip.isEmpty() ? juce::String() : tip + ". ") + baraja::u8 ("Doble clic: valor inicial."));

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, spec.id, slider);
    slider.setDoubleClickReturnValue (true, spec.def);
    slider.onValueChange = [this] { refreshValue(); };
    slider.onDragStart = [this] { if (onUserChange) onUserChange(); };

    addAndMakeVisible (name);
    addAndMakeVisible (value);
    addAndMakeVisible (slider);
    refreshValue();
}

void ParamRow::refreshValue() { value.setText (param->getCurrentValueAsText(), juce::dontSendNotification); }

void ParamRow::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (20);
    value.setBounds (top.removeFromRight (78));
    name.setBounds (top);
    slider.setBounds (r.removeFromTop (24));
}

//==============================================================================
ChoiceRow::ChoiceRow (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                      const juce::String& label, const juce::StringArray& items)
{
    name.setText (label, juce::dontSendNotification);
    name.setFont (font (13.5f));
    name.setMinimumHorizontalScale (1.0f);
    box.addItemList (items, 1);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id, box);
    addAndMakeVisible (name);
    addAndMakeVisible (box);
}

void ChoiceRow::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (20));
    box.setBounds (r.removeFromTop (26).reduced (2, 0));
}

//==============================================================================
void Group::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
    g.setColour (colours::ink);
    g.setFont (font (15.0f, true));
    g.drawText (title, 16, 10, getWidth() - 32, 20, juce::Justification::centredLeft);
}

void Group::resized()
{
    auto r = getLocalBounds().reduced (14, 0);
    r.removeFromTop (40);
    for (auto* c : rows)
    {
        const int h = c->getProperties().getWithDefault ("rowHeight", 50);
        c->setBounds (r.removeFromTop (h));
        r.removeFromTop (6);
    }
}

int Group::preferredHeight() const
{
    int h = 40 + 10;
    for (auto* c : rows) h += (int) c->getProperties().getWithDefault ("rowHeight", 50) + 6;
    return h;
}

//==============================================================================
void MemoryView::update (const baraja::Telemetry& t, bool peaksFresh)
{
    tele.numGrains = t.numGrains;
    std::copy (t.grains, t.grains + t.numGrains, tele.grains);
    tele.drift = t.drift;
    if (peaksFresh)
    {
        std::copy (t.peaks, t.peaks + baraja::Telemetry::numPeaks, tele.peaks);
        tele.displayMs = t.displayMs;
        hasPeaks = true;
    }
    repaint();
}

void MemoryView::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    g.setColour (colours::strip);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    const float delay = state.getRawParameterValue ("delay")->load();
    const float pitch = state.getRawParameterValue ("pitch")->load();
    const float drift = state.getRawParameterValue ("drift")->load();
    const bool freeze = state.getRawParameterValue ("freeze")->load() > 0.5f;

    const float padX = 12.0f, top = 16.0f, bottom = H - 26.0f, plotH = bottom - top, mid = top + plotH * 0.5f;
    const float D = juce::jmax (100.0f, tele.displayMs);
    auto xFor = [&] (float ms) { return padX + (W - padX * 2.0f) * (1.0f - ms / D); };

    // zona fuera del alcance
    const float xLim = juce::jlimit (padX, W - padX, xFor (delay));
    g.setColour (colours::line.withAlpha (0.35f));
    g.fillRect (padX, top, xLim - padX, plotH);

    // forma de onda de la memoria
    if (hasPeaks)
    {
        float mx = 0.0f;
        for (auto p : tele.peaks) mx = juce::jmax (mx, p);
        const float target = juce::jmin (6.0f, 1.0f / juce::jmax (0.08f, mx));
        waveScale += (target - waveScale) * 0.08f;
        g.setColour (colours::wave.withAlpha (0.55f));
        const int B = baraja::Telemetry::numPeaks;
        const float bw = (W - padX * 2.0f) / B;
        for (int b = 0; b < B; ++b)
        {
            const float a = juce::jmin (1.0f, tele.peaks[b] * waveScale) * plotH * 0.46f + 0.5f;
            g.fillRect (padX + b * bw, mid - a, juce::jmax (1.0f, bw * 0.75f), a * 2.0f);
        }
    }
    g.setColour (colours::line);
    g.drawHorizontalLine ((int) mid, padX, W - padX);

    // límite de profundidad
    {
        const float dashes[] = { 3.0f, 4.0f };
        g.setColour (colours::muted);
        g.drawDashedLine ({ xLim, top, xLim, bottom }, dashes, 2, 1.0f);
    }

    // fragmentos activos: x = de dónde leen, y = lugar en el estéreo
    for (int i = 0; i < tele.numGrains; ++i)
    {
        const auto& v = tele.grains[i];
        const float x0 = xFor (v.delayMs);
        const float x1 = xFor (v.reversed ? v.delayMs + v.spanMs : juce::jmax (0.0f, v.delayMs - v.spanMs));
        const float y = top + 8.0f + v.pan * (plotH - 16.0f);
        const float env = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, v.progress));
        g.setColour ((v.reversed ? colours::rev : colours::grain).withAlpha (0.25f + 0.75f * v.amp * (0.35f + 0.65f * env)));
        g.fillRect (juce::jmin (x0, x1), y - 2.5f, juce::jmax (2.0f, std::abs (x1 - x0)), 5.0f);
        g.fillEllipse (x0 - 3.5f, y - 3.5f, 7.0f, 7.0f);
    }

    // eje de tiempo
    g.setFont (font (11.0f));
    const float steps[] = { 10, 20, 50, 100, 200, 250, 500, 1000, 2000, 5000 };
    float st = 10000.0f;
    for (auto s : steps) if (D / s <= 7.0f) { st = s; break; }
    for (int k = 0; k * st <= D; ++k)
    {
        const int ms = juce::roundToInt (k * st);
        const float x = xFor ((float) ms);
        g.setColour (colours::muted);
        g.fillRect (x, bottom + 2.0f, 1.0f, 4.0f);
        juce::String label = ms == 0 ? "ahora"
                           : ms >= 1000 ? juce::String::fromUTF8 ("−") + es (ms / 1000.0f, ms % 1000 != 0 ? 2 : 0) + " s"
                                        : juce::String::fromUTF8 ("−") + juce::String (ms) + " ms";
        if (ms == 0) g.drawText (label, (int) x - 60, (int) bottom + 8, 62, 14, juce::Justification::right);
        else if (x > 34.0f) g.drawText (label, (int) x - 40, (int) bottom + 8, 80, 14, juce::Justification::centred);
    }

    if (xLim - padX > 100.0f)
    {
        g.setColour (colours::muted);
        g.drawText ("fuera de alcance", (int) padX + 8, (int) top + 4, 140, 14, juce::Justification::left);
    }

    int labelY = (int) top + 4;
    if (freeze)
    {
        g.setColour (colours::rev);
        g.setFont (font (11.0f, true));
        g.drawText ("MEMORIA CONGELADA", (int) (W - padX - 204), labelY, 200, 14, juce::Justification::right);
        labelY += 16;
    }
    if (drift > 0.0f)
    {
        const float c = pitch + tele.drift;
        g.setColour (colours::grain);
        g.setFont (font (11.0f));
        g.drawText ("altura central " + juce::String (c > 0 ? "+" : "") + es (c, 1) + " st",
                    (int) (W - padX - 204), labelY, 200, 14, juce::Justification::right);
    }
}

//==============================================================================
void Meter::push (float peak) { hold = juce::jmax (peak, hold * 0.9f); repaint(); }

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto text = r.removeFromRight (64.0f);
    auto bar = r.withSizeKeepingCentre (r.getWidth(), 8.0f);
    g.setColour (colours::strip);
    g.fillRoundedRectangle (bar, 4.0f);
    const float db = hold > 1.0e-5f ? 20.0f * std::log10 (hold) : -100.0f;
    const float f = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
    g.setColour (db > -0.5f ? colours::hot : colours::grain);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * f), 4.0f);
    g.setColour (colours::muted);
    g.setFont (font (12.0f));
    g.drawText (db > -99.0f ? es (db, 1) + " dB" : juce::String::fromUTF8 ("−∞ dB"), text, juce::Justification::centredRight);
}

} // namespace baraja::ui

//==============================================================================
using namespace baraja::ui;

BarajaEditor::BarajaEditor (BarajaProcessor& p)
    : AudioProcessorEditor (p), processor (p), memory (p.apvts)
{
    setLookAndFeel (&lnf);

    presetsLabel.setText ("AJUSTES", juce::dontSendNotification);
    presetsLabel.setFont (font (11.0f, true));
    presetsLabel.setColour (juce::Label::textColourId, colours::muted);
    addAndMakeVisible (presetsLabel);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
    {
        auto* b = presetButtons.add (new juce::TextButton (processor.getProgramName (i)));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { processor.applyPreset (i); shownProgram = -2; highlightPreset(); };
        addAndMakeVisible (b);
    }

    addAndMakeVisible (meter);
    addAndMakeVisible (memory);

    auto onUser = [this] { shownProgram = -1; highlightPreset(); };

    for (const auto& spec : baraja::floatSpecs())
    {
        auto* row = rows.add (new ParamRow (processor.apvts, spec));
        row->onUserChange = onUser;
        row->getProperties().set ("rowHeight", 46);
        switch (spec.group)
        {
            case baraja::Group::fragmentos: fragmentos.add (*row); break;
            case baraja::Group::memoria:    memoria.add (*row); break;
            case baraja::Group::altura:     altura.add (*row); break;
            case baraja::Group::salida:     salida.add (*row); break;
        }
    }

    vdirRow  = std::make_unique<ChoiceRow> (processor.apvts, "vdir",  baraja::u8 ("Sentido de la variación"), baraja::vdirChoices());
    quantRow = std::make_unique<ChoiceRow> (processor.apvts, "quant", "Alturas posibles", baraja::quantChoices());
    vdirRow->getProperties().set ("rowHeight", 48);
    quantRow->getProperties().set ("rowHeight", 48);
    altura.add (*vdirRow);
    altura.add (*quantRow);

    freezeButton.setClickingTogglesState (true);
    freezeButton.getProperties().set ("freeze", true);
    freezeButton.setTooltip (baraja::u8 ("Deja de grabar en la memoria y sigue barajando lo que quedó guardado."));
    freezeButton.getProperties().set ("rowHeight", 40);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "freeze", freezeButton);
    memoria.add (freezeButton);

    freezeHint.setText (juce::String::fromUTF8 ("Con la memoria congelada, Baraja sigue sonando aunque la pista quede en silencio."),
                        juce::dontSendNotification);
    freezeHint.setFont (font (12.0f));
    freezeHint.setColour (juce::Label::textColourId, colours::muted);
    freezeHint.setJustificationType (juce::Justification::topLeft);
    freezeHint.getProperties().set ("rowHeight", 54);
    memoria.add (freezeHint);

    for (auto* g : { &fragmentos, &memoria, &altura, &salida }) addAndMakeVisible (g);

    setResizable (true, true);
    setResizeLimits (1000, 740, 1800, 1200);
    setSize (1060, 760);
    shownProgram = -2;
    highlightPreset();
    startTimerHz (30);
}

BarajaEditor::~BarajaEditor() { stopTimer(); setLookAndFeel (nullptr); }

void BarajaEditor::highlightPreset()
{
    const int current = shownProgram == -1 ? -1 : processor.getCurrentProgram();
    for (int i = 0; i < presetButtons.size(); ++i)
        presetButtons[i]->setToggleState (i == current, juce::dontSendNotification);
}

void BarajaEditor::timerCallback()
{
    const bool fresh = processor.readTelemetry (telemetry);
    memory.update (telemetry, fresh);
    meter.push (telemetry.outPeak);

    const juce::String freezeText = freezeButton.getToggleState() ? "Memoria congelada" : "Congelar memoria";
    if (freezeButton.getButtonText() != freezeText) freezeButton.setButtonText (freezeText);
}

void BarajaEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    g.setColour (colours::ink);
    g.setFont (font (38.0f, true));
    g.drawText ("Baraja", 24, 16, 260, 44, juce::Justification::centredLeft);
    g.setColour (colours::muted);
    g.setFont (font (11.5f));
    g.drawText (juce::String::fromUTF8 ("BRASSAGE EN TIEMPO REAL · MEMORIA DE 8 S · VERSIÓN ") + JucePlugin_VersionString,
                26, 60, 420, 16, juce::Justification::centredLeft);
    g.drawText ("Memoria", 24, legendY, 200, 18, juce::Justification::centredLeft);

    g.setFont (font (11.5f));
    const int lx = getWidth() - 24 - 560, ly = legendY + 1;
    g.setColour (colours::wave);    g.fillRect (lx, ly + 6, 14, 5);
    g.setColour (colours::muted);   g.drawText ("entrada reciente (derecha = ahora)", lx + 20, ly, 200, 16, juce::Justification::centredLeft);
    g.setColour (colours::grain);   g.fillRect (lx + 230, ly + 6, 14, 5);
    g.setColour (colours::muted);   g.drawText ("fragmento hacia adelante", lx + 250, ly, 160, 16, juce::Justification::centredLeft);
    g.setColour (colours::rev);     g.fillRect (lx + 420, ly + 6, 14, 5);
    g.setColour (colours::muted);   g.drawText ("fragmento invertido", lx + 440, ly, 130, 16, juce::Justification::centredLeft);
}

void BarajaEditor::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    auto header = r.removeFromTop (64);
    meter.setBounds (header.removeFromRight (220).withSizeKeepingCentre (220, 20));

    auto presetsRow = r.removeFromTop (34);
    presetsLabel.setBounds (presetsRow.removeFromLeft (70));
    const int bw = juce::jmin (96, (presetsRow.getWidth() - (presetButtons.size() - 1) * 6) / juce::jmax (1, presetButtons.size()));
    for (auto* b : presetButtons)
    {
        b->setBounds (presetsRow.removeFromLeft (bw).reduced (0, 3));
        presetsRow.removeFromLeft (6);
    }

    r.removeFromTop (8);
    legendY = r.getY();
    r.removeFromTop (26);   // título "Memoria" y leyenda (dibujados en paint)
    const int groupsH = juce::jmax (altura.preferredHeight(), fragmentos.preferredHeight());
    const int memH = juce::jmax (160, r.getHeight() - groupsH - 14);
    memory.setBounds (r.removeFromTop (memH));
    r.removeFromTop (14);

    const int gap = 12, gw = (r.getWidth() - gap * 3) / 4;
    for (auto* g : { &fragmentos, &memoria, &altura, &salida })
    {
        g->setBounds (r.removeFromLeft (gw).withHeight (groupsH));
        r.removeFromLeft (gap);
    }
}
