// Baraja — motor de brassage en tiempo real
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Este archivo no depende de JUCE: es el mismo algoritmo del prototipo web,
// pensado para poder reutilizarse en otros proyectos (por ejemplo, un instrumento MIDI).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace baraja
{

struct Params
{
    float size      = 120.0f;   // ms
    float sizeVar   = 30.0f;    // %
    float density   = 4.0f;     // fragmentos superpuestos en promedio
    float jitter    = 60.0f;    // % irregularidad rítmica
    float shape     = 60.0f;    // % suavidad de ventana
    float delay     = 1500.0f;  // ms, profundidad en la memoria
    float pitch     = 0.0f;     // semitonos, transposición fija
    float pitchVar  = 0.0f;     // semitonos, variación aleatoria
    int   vdir      = 0;        // 0 ambos, 1 arriba, 2 abajo
    int   quant     = 0;        // 0 libres, 1 semitonos, 2 octavas
    float drift     = 0.0f;     // semitonos, deriva aleatoria de la transposición
    float driftRate = 0.3f;     // Hz
    float reverse   = 0.0f;     // % fragmentos invertidos
    float spread    = 60.0f;    // % dispersión estéreo
    float ampVar    = 0.0f;     // % amplitud aleatoria
    float mix       = 100.0f;   // % procesado
    float gain      = 0.0f;     // dB
    bool  freeze    = false;
};

struct GrainView
{
    float delayMs, spanMs, amp, pan, progress;
    bool reversed;
};

struct Telemetry
{
    static constexpr int numPeaks = 360;
    GrainView grains[128];
    int numGrains = 0;
    float peaks[numPeaks] {};
    bool peaksFresh = false;
    float displayMs = 2000.0f;
    float drift = 0.0f;
    float outPeak = 0.0f;
};

class Engine
{
public:
    static constexpr int maxGrains = 128;

    void prepare (double newSampleRate)
    {
        sr = newSampleRate;
        int len = 1;
        while (len < (int) (sr * 10.0)) len <<= 1;   // al menos 10 s de memoria
        buffer.assign ((size_t) len, 0.0f);
        mask = len - 1;
        smoothCoef = 1.0 - std::exp (-1.0 / (0.02 * sr));
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        nextGrain = 0.0;
        for (auto& g : grains) g.active = false;
        driftPhase = driftFrom = driftTo = driftNow = 0.0;
        teleCount = 0;
        outPeakAcc = 0.0f;
    }

    double getMemorySeconds() const noexcept { return (double) buffer.size() / sr; }

    static float displayMsFor (const Params& p) noexcept
    {
        return std::min (10000.0f, p.delay * 1.08f + p.size * 1.5f + 60.0f);
    }

    // inR puede ser igual a inL (entrada mono). outR puede ser nullptr (salida mono: se suma).
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const Params& p) noexcept
    {
        if (buffer.empty()) return;

        mixTarget  = p.mix / 100.0;
        gainTarget = std::pow (10.0, p.gain / 20.0);
        const double wetScale = 0.85 / std::sqrt (std::max (1.0f, p.density));
        const double interval = (std::max (4.0f, p.size) * sr / 1000.0) / std::max (0.1f, p.density);

        // deriva aleatoria de la transposición central (curva coseno entre destinos al azar)
        driftPhase += n * p.driftRate / sr;
        if (driftPhase >= 1.0)
        {
            driftPhase -= std::floor (driftPhase);
            driftFrom = driftTo;
            driftTo = rnd() * 2.0 - 1.0;
        }
        {
            const double s = 0.5 - 0.5 * std::cos (pi * driftPhase);
            driftNow = (driftFrom + (driftTo - driftFrom) * s) * p.drift;
        }

        const int len = (int) buffer.size();
        float* buf = buffer.data();

        for (int i = 0; i < n; ++i)
        {
            const float dl = inL ? inL[i] : 0.0f;
            const float dr = inR ? inR[i] : dl;

            if (! p.freeze)
            {
                buf[writePos] = 0.5f * (dl + dr);
                writePos = (writePos + 1) & mask;
            }

            nextGrain -= 1.0;
            if (nextGrain <= 0.0)
            {
                spawn (p);
                nextGrain = std::max (16.0, interval * (1.0 + (rnd() * 2.0 - 1.0) * p.jitter / 100.0));
            }

            double wl = 0.0, wr = 0.0;
            for (auto& g : grains)
            {
                if (! g.active) continue;

                const double x = (double) g.n / g.length;
                double win = 1.0;
                if (x < g.edge)            win = 0.5 - 0.5 * std::cos (pi * x / g.edge);
                else if (x > 1.0 - g.edge) win = 0.5 - 0.5 * std::cos (pi * (1.0 - x) / g.edge);

                const int i0 = (int) std::floor (g.pos);
                const double f = g.pos - i0;
                const float xm1 = buf[(i0 - 1) & mask], x0 = buf[i0 & mask],
                            x1  = buf[(i0 + 1) & mask], x2 = buf[(i0 + 2) & mask];
                const double c1 = 0.5 * (x1 - xm1);
                const double c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
                const double c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
                const double s = (((c3 * f + c2) * f + c1) * f + x0) * win * g.amp;

                wl += s * g.panL;
                wr += s * g.panR;

                g.pos += g.inc;
                if (g.pos >= len) g.pos -= len;
                else if (g.pos < 0.0) g.pos += len;

                if (++g.n >= g.length) g.active = false;
            }

            mixNow  += (mixTarget  - mixNow)  * smoothCoef;
            gainNow += (gainTarget - gainNow) * smoothCoef;

            const double yl = (dl * (1.0 - mixNow) + wl * wetScale * mixNow) * gainNow;
            const double yr = (dr * (1.0 - mixNow) + wr * wetScale * mixNow) * gainNow;

            if (outR != nullptr) { outL[i] = (float) yl; outR[i] = (float) yr; }
            else                 { outL[i] = (float) (0.5 * (yl + yr)); }

            const float a = (float) std::max (std::abs (yl), std::abs (yr));
            if (a > outPeakAcc) outPeakAcc = a;
        }
    }

    // Llamar desde el hilo de audio, con el lock de telemetría tomado.
    void snapshot (Telemetry& t, const Params& p) noexcept
    {
        const int len = (int) buffer.size();
        int c = 0;
        for (auto& g : grains)
        {
            if (! g.active || c >= maxGrains) continue;
            double d = std::fmod ((double) writePos - g.pos, (double) len);
            if (d < 0) d += len;
            auto& v = t.grains[c++];
            v.delayMs  = (float) (d * 1000.0 / sr);
            v.spanMs   = (float) ((g.length - g.n) * std::abs (g.inc) * 1000.0 / sr);
            v.amp      = (float) g.amp;
            v.pan      = (float) g.pan;
            v.progress = (float) g.n / (float) g.length;
            v.reversed = g.inc < 0.0;
        }
        t.numGrains = c;
        t.drift = (float) driftNow;
        t.outPeak = std::max (t.outPeak, outPeakAcc);
        outPeakAcc = 0.0f;

        if (++teleCount % 3 == 0)
        {
            const float dispMs = std::min (displayMsFor (p), (float) ((len - 2048) * 1000.0 / sr));
            const double D = dispMs * sr / 1000.0;
            const double perBin = D / Telemetry::numPeaks;
            const int step = std::max (1, (int) (perBin / 48.0));
            for (int b = 0; b < Telemetry::numPeaks; ++b)
            {
                const double d0 = D * (1.0 - (double) b / Telemetry::numPeaks);
                const double d1 = D * (1.0 - (double) (b + 1) / Telemetry::numPeaks);
                float mx = 0.0f;
                for (double d = d0; d > d1; d -= step)
                    mx = std::max (mx, std::abs (buffer[(size_t) ((writePos - (int) d) & mask)]));
                t.peaks[b] = mx;
            }
            t.displayMs = dispMs;
            t.peaksFresh = true;
        }
    }

private:
    struct Grain
    {
        bool active = false;
        double pos = 0, inc = 1, amp = 1, pan = 0.5, panL = 0.7, panR = 0.7, edge = 0.3;
        int n = 0, length = 1;
    };

    static constexpr double pi = 3.14159265358979323846;

    double rnd() noexcept
    {
        uint32_t x = seed;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        seed = x;
        return x / 4294967296.0;
    }

    void spawn (const Params& p) noexcept
    {
        Grain* g = nullptr;
        for (auto& gr : grains) if (! gr.active) { g = &gr; break; }
        if (g == nullptr) return;

        const int len = (int) buffer.size();

        double dur = p.size * (1.0 + (rnd() * 2.0 - 1.0) * p.sizeVar / 100.0);
        if (dur < 4.0) dur = 4.0;
        int length = (int) std::lround (dur * sr / 1000.0);

        const double r = rnd();
        double v = p.vdir == 1 ? r * p.pitchVar
                 : p.vdir == 2 ? -r * p.pitchVar
                               : (r * 2.0 - 1.0) * p.pitchVar;
        if (p.quant == 1)      v = std::round (v);
        else if (p.quant == 2) v = std::round (v / 12.0) * 12.0;

        const double rate = std::pow (2.0, (p.pitch + driftNow + v) / 12.0);
        const bool rev = rnd() < p.reverse / 100.0;

        const double maxOld = len - 1024.0;
        if (length * rate > maxOld * 0.5) length = (int) (maxOld * 0.5 / rate);
        length = std::max (length, 16);
        const double span = length * rate;

        // la lectura nunca debe alcanzar a la cabeza de escritura
        const double minD = rev ? 64.0
                          : (p.freeze ? span + 64.0 : std::max (0.0, length * (rate - 1.0)) + 64.0);
        double d = minD + rnd() * p.delay * sr / 1000.0;
        const double oldest = rev ? d + span : d;
        if (oldest > maxOld) d -= oldest - maxOld;
        if (d < minD) d = minD;

        double pos = writePos - d;
        while (pos < 0.0) pos += len;

        const double pan = 0.5 + (rnd() * 2.0 - 1.0) * p.spread / 200.0;

        g->active = true;
        g->pos = pos;
        g->inc = rev ? -rate : rate;
        g->n = 0;
        g->length = length;
        g->amp = 1.0 - rnd() * p.ampVar / 100.0;
        g->pan = pan;
        g->panL = std::cos (pan * pi / 2.0);
        g->panR = std::sin (pan * pi / 2.0);
        g->edge = (0.02 + 0.98 * p.shape / 100.0) / 2.0;
    }

    double sr = 48000.0;
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
    Grain grains[maxGrains];
    double nextGrain = 0.0;
    uint32_t seed = 2463534242u;
    double mixNow = 1.0, gainNow = 1.0, mixTarget = 1.0, gainTarget = 1.0, smoothCoef = 0.001;
    double driftPhase = 0, driftFrom = 0, driftTo = 0, driftNow = 0;
    int teleCount = 0;
    float outPeakAcc = 0.0f;
};

} // namespace baraja
