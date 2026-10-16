// Baraja — parámetros y ajustes de fábrica
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BarajaEngine.h"

namespace baraja
{

enum class Group { fragmentos, memoria, altura, salida };

struct FloatSpec
{
    const char* id;
    const char* name;
    float min, max, def;
    const char* unit;
    int decimals;
    bool logarithmic;
    bool signedDisplay;
    Group group;
    const char* note;
};

inline const std::vector<FloatSpec>& floatSpecs()
{
    static const std::vector<FloatSpec> specs {
        { "size",      "Tamaño",                5.0f,    1000.0f, 120.0f,  "ms", 0, true,  false, Group::fragmentos, "" },
        { "sizeVar",   "Variación del tamaño",  0.0f,    100.0f,  30.0f,   "%",  0, false, false, Group::fragmentos, "" },
        { "density",   "Densidad",              0.5f,    16.0f,   4.0f,    "x",  1, true,  false, Group::fragmentos, "fragmentos superpuestos en promedio" },
        { "jitter",    "Irregularidad rítmica", 0.0f,    100.0f,  60.0f,   "%",  0, false, false, Group::fragmentos, "" },
        { "shape",     "Suavidad de ventana",   0.0f,    100.0f,  60.0f,   "%",  0, false, false, Group::fragmentos, "0 = bordes abruptos, 100 = campana" },
        { "delay",     "Profundidad",           20.0f,   8000.0f, 1500.0f, "ms", 0, true,  false, Group::memoria,    "hasta qué punto del pasado se toman fragmentos" },
        { "pitch",     "Transposición fija",    -24.0f,  24.0f,   0.0f,    "st", 2, false, true,  Group::altura,     "todos los fragmentos se transponen igual" },
        { "pitchVar",  "Variación aleatoria",   0.0f,    24.0f,   0.0f,    "st", 2, false, false, Group::altura,     "cada fragmento recibe una altura al azar" },
        { "drift",     "Deriva aleatoria",      0.0f,    24.0f,   0.0f,    "st", 2, false, false, Group::altura,     "la transposición fija vaga sola en el tiempo" },
        { "driftRate", "Velocidad de deriva",   0.02f,   8.0f,    0.3f,    "Hz", 2, true,  false, Group::altura,     "" },
        { "reverse",   "Fragmentos invertidos", 0.0f,    100.0f,  0.0f,    "%",  0, false, false, Group::altura,     "" },
        { "spread",    "Dispersión estéreo",    0.0f,    100.0f,  60.0f,   "%",  0, false, false, Group::salida,     "" },
        { "ampVar",    "Amplitud aleatoria",    0.0f,    100.0f,  0.0f,    "%",  0, false, false, Group::salida,     "" },
        { "mix",       "Mezcla",                0.0f,    100.0f,  100.0f,  "%",  0, false, false, Group::salida,     "porcentaje de señal procesada" },
        { "gain",      "Salida",                -24.0f,  12.0f,   0.0f,    "dB", 1, false, true,  Group::salida,     "" },
    };
    return specs;
}

// Los textos del código fuente están en UTF-8: siempre se convierten con esta función.
inline juce::String u8 (const char* text) { return juce::String::fromUTF8 (text); }

inline const juce::StringArray& vdirChoices()  { static const juce::StringArray c { u8 ("Hacia arriba y abajo"), u8 ("Solo hacia arriba"), u8 ("Solo hacia abajo") }; return c; }
inline const juce::StringArray& quantChoices() { static const juce::StringArray c { u8 ("Alturas libres"), u8 ("Semitonos"), u8 ("Octavas") }; return c; }

inline juce::String formatValue (const FloatSpec& s, float v)
{
    juce::String t;
    if (juce::String (s.unit) == "ms" && v >= 1000.0f)
        t = juce::String (v / 1000.0f, 2) + " s";
    else
        t = (s.decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, s.decimals)) + " " + s.unit;

    if (s.signedDisplay && v > 0.0f) t = "+" + t;
    return t.replaceCharacter ('.', ',');   // coma decimal
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    int version = 1;

    for (const auto& s : floatSpecs())
    {
        juce::NormalisableRange<float> range (s.min, s.max);
        if (s.logarithmic)
        {
            const float lmin = std::log (s.min), lmax = std::log (s.max);
            range = juce::NormalisableRange<float> (s.min, s.max,
                [lmin, lmax] (float, float, float x) { return std::exp (lmin + x * (lmax - lmin)); },
                [lmin, lmax] (float, float, float v) { return (std::log (v) - lmin) / (lmax - lmin); });
        }

        const auto spec = s;
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, version }, u8 (s.name), range, s.def,
            juce::AudioParameterFloatAttributes()
                .withLabel (s.unit)
                .withStringFromValueFunction ([spec] (float v, int) { return formatValue (spec, v); })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.replaceCharacter (',', '.').getFloatValue(); })));
    }

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "vdir", version },  u8 ("Sentido de la variación"), vdirChoices(), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "quant", version }, "Alturas posibles", quantChoices(), 0));
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { "freeze", version }, "Congelar memoria", false));
    return layout;
}

// Ajustes de fábrica: solo se listan los valores que cambian respecto del inicial.
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Inicial", {} },
        { "Granizo",    { {"size",40}, {"sizeVar",50}, {"density",8}, {"jitter",90}, {"shape",30}, {"delay",800}, {"pitchVar",2}, {"reverse",20}, {"spread",90}, {"ampVar",60} } },
        { "Nube lenta", { {"size",450}, {"density",6}, {"jitter",70}, {"shape",100}, {"delay",3000}, {"pitch",-12}, {"pitchVar",0.3f}, {"reverse",30}, {"spread",70}, {"ampVar",20} } },
        { "Tartamudeo", { {"size",90}, {"sizeVar",0}, {"density",1.2f}, {"jitter",8}, {"shape",12}, {"delay",300}, {"spread",30} } },
        { "Coral",      { {"size",220}, {"sizeVar",20}, {"density",10}, {"shape",85}, {"pitchVar",7}, {"quant",1}, {"spread",100}, {"ampVar",30} } },
        { "Espejo",     { {"size",160}, {"sizeVar",25}, {"density",3}, {"jitter",50}, {"shape",70}, {"delay",1200}, {"reverse",100}, {"spread",80}, {"ampVar",10} } },
        { "Octavas",    { {"size",300}, {"sizeVar",40}, {"density",5}, {"jitter",65}, {"shape",90}, {"delay",2500}, {"pitchVar",13}, {"quant",2}, {"reverse",15}, {"spread",100}, {"ampVar",25} } },
        { "Deriva",     { {"size",260}, {"sizeVar",35}, {"density",6}, {"jitter",55}, {"shape",90}, {"delay",2200}, {"pitchVar",0.4f}, {"drift",7}, {"driftRate",0.15f}, {"reverse",20}, {"spread",90}, {"ampVar",20} } },
        { "Enjambre",   { {"size",60}, {"sizeVar",60}, {"density",12}, {"jitter",95}, {"shape",50}, {"delay",1000}, {"pitch",5}, {"pitchVar",12}, {"vdir",1}, {"drift",3}, {"driftRate",2.5f}, {"reverse",10}, {"spread",100}, {"ampVar",50} } },
    };
    return list;
}

} // namespace baraja
