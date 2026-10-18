// Baraja — parameters, presets and interface texts
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cstring>
#include "BarajaEngine.h"

namespace baraja
{

// Source texts are UTF-8: always convert them with this.
inline juce::String u8 (const char* text) { return juce::String::fromUTF8 (text); }

enum class Lang { en = 0, es = 1 };

// toUpperCase() does not change accented letters: finish the job for Spanish.
inline juce::String caps (const juce::String& t)
{
    auto r = t.toUpperCase();
    const char* pairs[][2] { { "á", "Á" }, { "é", "É" }, { "í", "Í" }, { "ó", "Ó" }, { "ú", "Ú" }, { "ñ", "Ñ" }, { "ü", "Ü" } };
    for (auto& pr : pairs) r = r.replace (juce::String::fromUTF8 (pr[0]), juce::String::fromUTF8 (pr[1]));
    return r;
}

struct FloatSpec
{
    const char* id;
    const char* en;
    const char* es;
    float min, max, def;
    const char* unit;
    int decimals;
    enum Scale { linear, logarithmic, skewed } scale;
    bool signedDisplay;
    const char* noteEn;
    const char* noteEs;
};

inline const std::vector<FloatSpec>& floatSpecs()
{
    using S = FloatSpec;
    static const std::vector<FloatSpec> specs {
        { "size",      "Size",          "Tamaño",         5.0f,   1000.0f,  120.0f,  "ms",    0, S::logarithmic, false, "Average fragment length", "Duración media de cada fragmento" },
        { "sizeVar",   "Variation",     "Variación",      0.0f,   100.0f,   30.0f,   "%",     0, S::linear,      false, "How much the length changes between fragments", "Cuánto cambia la duración entre fragmentos" },
        { "density",   "Density",       "Densidad",       0.5f,   16.0f,    4.0f,    "x",     1, S::logarithmic, false, "Average number of overlapping fragments", "Fragmentos superpuestos en promedio" },
        { "jitter",    "Irregular",     "Irregular",      0.0f,   100.0f,   60.0f,   "%",     0, S::linear,      false, "0 = regular pulse, 100 = fully irregular", "0 = pulso regular, 100 = totalmente irregular" },
        { "shape",     "Window",        "Ventana",        0.0f,   100.0f,   60.0f,   "%",     0, S::linear,      false, "Fragment envelope: 0 = hard edges, 100 = bell", "Envolvente: 0 = bordes abruptos, 100 = campana" },
        { "position",  "Position",      "Posición",       0.0f,   60000.0f, 0.0f,    "ms",    0, S::skewed,      false, "How far back the reading window starts", "Desde qué punto del pasado se lee" },
        { "width",     "Width",         "Ancho",          20.0f,  60000.0f, 1500.0f, "ms",    0, S::logarithmic, false, "Length of the reading window", "Cuánto abarca la zona de lectura" },
        { "order",     "Order",         "Orden",          0.0f,   100.0f,   0.0f,    "%",     0, S::linear,      false, "0 = fragments taken at random (shuffling), 100 = they follow the read head (freeze)", "0 = fragmentos al azar (shuffling), 100 = siguen al cabezal de lectura (freeze)" },
        { "speed",     "Speed",         "Velocidad",      -4.0f,  4.0f,     1.0f,    "x",     2, S::linear,      true,  "Read head speed and direction, independent of pitch. 0 = stopped", "Velocidad y sentido del cabezal, independiente de la altura. 0 = detenido" },
        { "pitch",     "Transpose",     "Transposición",  -24.0f, 24.0f,    0.0f,    "st",    2, S::linear,      true,  "Every fragment is transposed by the same amount", "Todos los fragmentos se transponen igual" },
        { "pitchVar",  "Random",        "Aleatoria",      0.0f,   24.0f,    0.0f,    "st",    2, S::linear,      false, "Each fragment also gets a random pitch in this range", "Cada fragmento recibe además una altura al azar" },
        { "drift",     "Drift",         "Deriva",         0.0f,   24.0f,    0.0f,    "st",    2, S::linear,      false, "The transposition wanders on its own", "La transposición vaga sola en el tiempo" },
        { "driftRate", "Drift rate",    "Vel. deriva",    0.02f,  8.0f,     0.3f,    "Hz",    2, S::logarithmic, false, "", "" },
        { "reverse",   "Reverse",       "Invertidos",     0.0f,   100.0f,   0.0f,    "%",     0, S::linear,      false, "Chance that a fragment plays backwards", "Probabilidad de que un fragmento suene al revés" },
        { "spread",    "Spread",        "Dispersión",     0.0f,   100.0f,   60.0f,   "%",     0, S::linear,      false, "How widely fragments are spread across the outputs", "Cuánto se reparten los fragmentos entre las salidas" },
        { "center",    "Center",        "Centro",         0.0f,   360.0f,   0.0f,    "deg",   0, S::linear,      false, "Multichannel: center of the spread on the speaker ring", "Multicanal: centro del reparto en el anillo de altavoces" },
        { "rotation",  "Rotation",      "Rotación",       -2.0f,  2.0f,     0.0f,    "Hz",    2, S::linear,      true,  "Multichannel: turns per second of the center around the ring", "Multicanal: vueltas por segundo del centro alrededor del anillo" },
        { "ampVar",    "Rnd amp",       "Amp. azar", 0.0f,   100.0f,   0.0f,    "%",     0, S::linear,      false, "Random level change between fragments", "Variación de nivel al azar entre fragmentos" },
        { "mix",       "Mix",           "Mezcla",         0.0f,   100.0f,   100.0f,  "%",     0, S::linear,      false, "Share of processed signal", "Proporción de señal procesada" },
        { "gain",      "Output",        "Salida",         -24.0f, 12.0f,    0.0f,    "dB",    1, S::linear,      true,  "", "" },
    };
    return specs;
}

inline const FloatSpec& spec (const juce::String& id)
{
    for (const auto& s : floatSpecs()) if (id == s.id) return s;
    jassertfalse;
    return floatSpecs().front();
}

// Memory lengths offered (seconds). The buffer is reallocated when this changes.
inline const std::vector<int>& memoryChoices() { static const std::vector<int> c { 5, 10, 20, 30, 60 }; return c; }

inline const juce::StringArray& vdirChoices()  { static const juce::StringArray c { "Both ways", "Up", "Down" }; return c; }
inline const juce::StringArray& quantChoices() { static const juce::StringArray c { "Free", "Semitones", "Octaves" }; return c; }
inline const juce::StringArray& spaceChoices() { static const juce::StringArray c { "Points", "Ring" }; return c; }

inline juce::String formatValue (const FloatSpec& s, float v)
{
    const juce::String unit (s.unit);
    juce::String t;
    if (unit == "ms" && v >= 1000.0f)  t = juce::String (v / 1000.0f, 2) + " s";
    else if (unit == "deg")            t = juce::String (juce::roundToInt (v)) + u8 ("°");
    else t = (s.decimals == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, s.decimals)) + " " + unit;
    if (s.signedDisplay && v > 0.0f) t = "+" + t;
    return t;
}

inline float parseValue (const FloatSpec& s, juce::String t)
{
    t = t.trim().replaceCharacter (',', '.');
    const float v = t.getFloatValue();
    if (juce::String (s.unit) == "ms" && t.endsWithIgnoreCase ("s") && ! t.endsWithIgnoreCase ("ms")) return v * 1000.0f;
    return v;
}

inline juce::NormalisableRange<float> rangeFor (const FloatSpec& s)
{
    if (s.scale == FloatSpec::logarithmic)
    {
        const float lmin = std::log (s.min), lmax = std::log (s.max);
        return juce::NormalisableRange<float> (s.min, s.max,
            [lmin, lmax] (float, float, float x) { return std::exp (lmin + x * (lmax - lmin)); },
            [lmin, lmax] (float, float, float v) { return (std::log (v) - lmin) / (lmax - lmin); });
    }
    juce::NormalisableRange<float> r (s.min, s.max);
    if (s.scale == FloatSpec::skewed) r.setSkewForCentre (s.min + (s.max - s.min) * 0.08f);   // finer near "now"
    return r;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const int v1 = 1, v2 = 2, v3 = 3;   // parameter versions: 2 = added in Baraja 0.2, 3 = added in 0.3

    for (const auto& s : floatSpecs())
    {
        const juce::String id (s.id);
        const bool isNew = id == "position" || id == "width" || id == "center" || id == "rotation";
        const bool isNewer = id == "order" || id == "speed";
        const auto sp = s;
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { s.id, isNewer ? v3 : (isNew ? v2 : v1) }, u8 (s.en), rangeFor (s), s.def,
            juce::AudioParameterFloatAttributes()
                .withLabel (s.unit)
                .withStringFromValueFunction ([sp] (float v, int) { return formatValue (sp, v); })
                .withValueFromStringFunction ([sp] (const juce::String& t) { return parseValue (sp, t); })));
    }

    juce::StringArray memNames;
    for (int sec : memoryChoices()) memNames.add (juce::String (sec) + " s");
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "memory", v2 }, "Memory", memNames, 1,
                    juce::AudioParameterChoiceAttributes().withAutomatable (false)));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "vdir", v1 },  "Direction", vdirChoices(), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "quant", v1 }, "Steps", quantChoices(), 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "spaceMode", v2 }, "Space mode", spaceChoices(), 1));
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { "freeze", v1 }, "Freeze", false));
    return layout;
}

//==============================================================================
// Factory presets: only values that differ from the defaults are listed.
struct Preset
{
    const char* en;
    const char* es;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list {
        { "Init",       "Inicial",    {} },
        { "Hail",       "Granizo",    { {"size",40}, {"sizeVar",50}, {"density",8}, {"jitter",90}, {"shape",30}, {"width",800}, {"pitchVar",2}, {"reverse",20}, {"spread",90}, {"ampVar",60} } },
        { "Slow cloud", "Nube lenta", { {"size",450}, {"density",6}, {"jitter",70}, {"shape",100}, {"width",3000}, {"pitch",-12}, {"pitchVar",0.3f}, {"reverse",30}, {"spread",70}, {"ampVar",20} } },
        { "Stutter",    "Tartamudeo", { {"size",90}, {"sizeVar",0}, {"density",1.2f}, {"jitter",8}, {"shape",12}, {"width",300}, {"spread",30} } },
        { "Choir",      "Coral",      { {"size",220}, {"sizeVar",20}, {"density",10}, {"shape",85}, {"pitchVar",7}, {"quant",1}, {"spread",100}, {"ampVar",30} } },
        { "Mirror",     "Espejo",     { {"size",160}, {"sizeVar",25}, {"density",3}, {"jitter",50}, {"shape",70}, {"width",1200}, {"reverse",100}, {"spread",80}, {"ampVar",10} } },
        { "Octaves",    "Octavas",    { {"size",300}, {"sizeVar",40}, {"density",5}, {"jitter",65}, {"shape",90}, {"width",2500}, {"pitchVar",13}, {"quant",2}, {"reverse",15}, {"spread",100}, {"ampVar",25} } },
        { "Drift",      "Deriva",     { {"size",260}, {"sizeVar",35}, {"density",6}, {"jitter",55}, {"shape",90}, {"width",2200}, {"pitchVar",0.4f}, {"drift",7}, {"driftRate",0.15f}, {"reverse",20}, {"spread",90}, {"ampVar",20} } },
        { "Swarm",      "Enjambre",   { {"size",60}, {"sizeVar",60}, {"density",12}, {"jitter",95}, {"shape",50}, {"width",1000}, {"pitch",5}, {"pitchVar",12}, {"vdir",1}, {"drift",3}, {"driftRate",2.5f}, {"reverse",10}, {"spread",100}, {"ampVar",50} } },
        { "Stretch",    "Estirado",   { {"order",100}, {"speed",0.25f}, {"size",200}, {"sizeVar",10}, {"density",8}, {"jitter",30}, {"shape",95}, {"width",4000}, {"spread",60} } },
        { "Suspended",  "Suspendido", { {"order",95}, {"speed",0}, {"size",300}, {"sizeVar",20}, {"density",10}, {"jitter",50}, {"shape",100}, {"position",200}, {"width",600}, {"spread",80}, {"ampVar",10} } },
        { "Rewind",     "Retroceso",  { {"order",80}, {"speed",-1}, {"size",150}, {"density",5}, {"jitter",40}, {"shape",80}, {"width",6000}, {"spread",70} } },
        { "Echoes",     "Ecos",       { {"size",350}, {"sizeVar",20}, {"density",2.5f}, {"jitter",40}, {"shape",80}, {"position",4000}, {"width",1500}, {"spread",100}, {"rotation",0.05f} } },
    };
    return list;
}

//==============================================================================
// Interface texts that are not parameter names.
inline juce::String tr (Lang lang, const char* key)
{
    struct Entry { const char* key; const char* en; const char* es; };
    static const Entry table[] {
        { "memory",      "Memory",                 "Memoria" },
        { "fragments",   "Fragments",              "Fragmentos" },
        { "pitch",       "Pitch",                  "Altura" },
        { "space",       "Space",                  "Espacio" },
        { "output",      "Output",                 "Salida" },
        { "length",      "Length",                 "Duración" },
        { "freeze",      "Freeze",                 "Congelar" },
        { "frozen",      "Frozen",                 "Congelada" },
        { "direction",   "Direction",              "Sentido" },
        { "steps",       "Steps",                  "Alturas" },
        { "mode",        "Mode",                   "Modo" },
        { "presets",     "Presets",                "Ajustes" },
        { "now",         "now",                    "ahora" },
        { "playhead",    "Reading window",         "Ventana de lectura" },
        { "subtitle",    "Model B-2 · tape brassage", "Modelo B-2 · brassage en cinta" },
        { "outputs",     "outputs",                "salidas" },
        { "both",        "Both",                   "Ambos" },
        { "up",          "Up",                     "Arriba" },
        { "down",        "Down",                   "Abajo" },
        { "free",        "Free",                   "Libres" },
        { "semi",        "Semi",                   "Semi" },
        { "oct",         "Oct",                    "Oct" },
        { "points",      "Points",                 "Puntos" },
        { "ring",        "Ring",                   "Anillo" },
        { "stereoOnly",  "Center and rotation act with 3 or more outputs", "Centro y rotación actúan con 3 salidas o más" },
        { "dragHint",    "Drag the heads to set the window; drag between them to move it", "Arrastrá los cabezales para ajustar la ventana; entre ellos, para moverla" },
        { "peak",        "PEAK  dBFS",             "PICO  dBFS" },
        { "dblclick",    "Double-click: default value", "Doble clic: valor inicial" },
    };
    for (const auto& e : table)
        if (std::strcmp (e.key, key) == 0) return u8 (lang == Lang::es ? e.es : e.en);
    return key;
}

} // namespace baraja
