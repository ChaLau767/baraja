// Baraja — real-time brassage engine
// Copyright (C) 2026 Lautaro Martín Vieyra
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// No JUCE dependency: the same algorithm as the web prototype, written so it
// can be reused elsewhere (for instance a MIDI-playable instrument).

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace baraja
{

enum class SpaceMode { points = 0, ring = 1 };

struct Params
{
    float size      = 120.0f;   // ms
    float sizeVar   = 30.0f;    // %
    float density   = 4.0f;     // average number of overlapping fragments
    float jitter    = 60.0f;    // % rhythmic irregularity
    float shape     = 60.0f;    // % window smoothness
    float position  = 0.0f;     // ms: how far back the newest edge of the reading window is
    float width     = 1500.0f;  // ms: length of the reading window
    float pitch     = 0.0f;     // semitones, fixed transposition
    float pitchVar  = 0.0f;     // semitones, random variation
    int   vdir      = 0;        // 0 both ways, 1 up, 2 down
    int   quant     = 0;        // 0 free, 1 semitones, 2 octaves
    float drift     = 0.0f;     // semitones, random drift of the fixed transposition
    float driftRate = 0.3f;     // Hz
    float reverse   = 0.0f;     // % reversed fragments
    float spread    = 60.0f;    // % spatial spread
    int   spaceMode = 0;        // multichannel only: SpaceMode
    float center    = 0.0f;     // degrees, multichannel only
    float rotation  = 0.0f;     // turns per second, multichannel only
    float ampVar    = 0.0f;     // % random amplitude
    float mix       = 100.0f;   // % processed
    float gain      = 0.0f;     // dB
    bool  freeze    = false;
};

struct GrainView
{
    float delayMs, spanMs, amp, place, progress;   // place: 0..1 (stereo pan or position on the ring)
    bool reversed;
};

struct Telemetry
{
    static constexpr int numPeaks = 480;
    GrainView grains[128];
    int numGrains = 0;
    float peaks[numPeaks] {};
    bool peaksFresh = false;
    float memoryMs = 10000.0f;
    float drift = 0.0f;
    float outPeak = 0.0f;
    float ringPhase = 0.0f;
    int numOutputs = 2;
};

class Engine
{
public:
    static constexpr int maxGrains = 128;
    static constexpr int maxOutputs = 64;

    // Allocates the memory: call off the audio thread.
    void prepare (double newSampleRate, double memorySeconds)
    {
        sr = newSampleRate;
        memorySamples = std::max (1.0, memorySeconds) * sr;
        int len = 1;
        while (len < (int) (memorySamples + 2.0 * sr)) len <<= 1;   // headroom for fragments that read past the window
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
        ringPhase = 0.0;
        teleCount = 0;
        outPeakAcc = 0.0f;
    }

    double getMemoryMs() const noexcept { return memorySamples * 1000.0 / sr; }

    // inR may equal inL (mono input). outs holds numOuts channel pointers (1..64).
    void process (const float* inL, const float* inR, float* const* outs, int numOuts, int n, const Params& p) noexcept
    {
        if (buffer.empty() || numOuts <= 0) return;
        numOuts = std::min (numOuts, maxOutputs);
        currentOutputs = numOuts;

        mixTarget  = p.mix / 100.0;
        gainTarget = std::pow (10.0, p.gain / 20.0);
        const double wetScale = 0.85 / std::sqrt (std::max (1.0f, p.density));
        const double interval = (std::max (4.0f, p.size) * sr / 1000.0) / std::max (0.1f, p.density);

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

        ringPhase += n * p.rotation / sr;
        ringPhase -= std::floor (ringPhase);

        const int len = (int) buffer.size();
        float* buf = buffer.data();
        double wet[maxOutputs];

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
                spawn (p, numOuts);
                nextGrain = std::max (16.0, interval * (1.0 + (rnd() * 2.0 - 1.0) * p.jitter / 100.0));
            }

            std::fill (wet, wet + numOuts, 0.0);

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

                wet[g.outA] += s * g.gainA;
                if (g.gainB > 0.0) wet[g.outB] += s * g.gainB;

                g.pos += g.inc;
                if (g.pos >= len) g.pos -= len;
                else if (g.pos < 0.0) g.pos += len;

                if (++g.n >= g.length) g.active = false;
            }

            mixNow  += (mixTarget  - mixNow)  * smoothCoef;
            gainNow += (gainTarget - gainNow) * smoothCoef;
            const double dryAmt = 1.0 - mixNow, wetAmt = wetScale * mixNow;

            float pk = 0.0f;
            for (int c = 0; c < numOuts; ++c)
            {
                double dry = 0.0;
                if (numOuts == 1)  dry = 0.5 * (dl + dr);
                else if (c == 0)   dry = dl;
                else if (c == 1)   dry = dr;
                const float y = (float) ((dry * dryAmt + wet[c] * wetAmt) * gainNow);
                outs[c][i] = y;
                pk = std::max (pk, std::abs (y));
            }
            if (pk > outPeakAcc) outPeakAcc = pk;
        }
    }

    // Call from the audio thread while holding the telemetry lock.
    void snapshot (Telemetry& t) noexcept
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
            v.place    = (float) g.place;
            v.progress = (float) g.n / (float) g.length;
            v.reversed = g.inc < 0.0;
        }
        t.numGrains = c;
        t.drift = (float) driftNow;
        t.outPeak = std::max (t.outPeak, outPeakAcc);
        t.ringPhase = (float) ringPhase;
        t.numOutputs = currentOutputs;
        outPeakAcc = 0.0f;

        if (++teleCount % 3 == 0)
        {
            const double D = memorySamples;
            const int step = std::max (1, (int) (D / Telemetry::numPeaks / 48.0));
            for (int b = 0; b < Telemetry::numPeaks; ++b)
            {
                const double d0 = D * (1.0 - (double) b / Telemetry::numPeaks);
                const double d1 = D * (1.0 - (double) (b + 1) / Telemetry::numPeaks);
                float mx = 0.0f;
                for (double d = d0; d > d1; d -= step)
                    mx = std::max (mx, std::abs (buffer[(size_t) ((writePos - (int) d) & mask)]));
                t.peaks[b] = mx;
            }
            t.memoryMs = (float) getMemoryMs();
            t.peaksFresh = true;
        }
    }

private:
    struct Grain
    {
        bool active = false;
        double pos = 0, inc = 1, amp = 1, place = 0.5, gainA = 0.7, gainB = 0.7, edge = 0.3;
        int n = 0, length = 1, outA = 0, outB = 1;
    };

    static constexpr double pi = 3.14159265358979323846;

    double rnd() noexcept
    {
        uint32_t x = seed;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        seed = x;
        return x / 4294967296.0;
    }

    void place (Grain& g, const Params& p, int numOuts) noexcept
    {
        const double r = rnd() * 2.0 - 1.0;

        if (numOuts == 1)
        {
            g.place = 0.5; g.outA = 0; g.outB = 0; g.gainA = 1.0; g.gainB = 0.0;
            return;
        }

        if (numOuts == 2)   // stereo: equal-power pan, as in 0.1
        {
            const double pan = 0.5 + r * p.spread / 200.0;
            g.place = pan; g.outA = 0; g.outB = 1;
            g.gainA = std::cos (pan * pi / 2.0);
            g.gainB = std::sin (pan * pi / 2.0);
            return;
        }

        // ring of numOuts speakers; spread 100 % covers the whole circle
        double pos = p.center / 360.0 + ringPhase + r * p.spread / 200.0;
        pos -= std::floor (pos);
        g.place = pos;
        const double x = pos * numOuts;

        if (p.spaceMode == (int) SpaceMode::points)
        {
            g.outA = ((int) std::lround (x)) % numOuts;
            g.outB = g.outA; g.gainA = 1.0; g.gainB = 0.0;
        }
        else
        {
            const int a = ((int) std::floor (x)) % numOuts;
            const double f = x - std::floor (x);
            g.outA = a; g.outB = (a + 1) % numOuts;
            g.gainA = std::cos (f * pi / 2.0);
            g.gainB = std::sin (f * pi / 2.0);
        }
    }

    void spawn (const Params& p, int numOuts) noexcept
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
        if (length * rate > maxOld * 0.25) length = (int) (maxOld * 0.25 / rate);
        length = std::max (length, 16);
        const double span = length * rate;

        // the read head must never overtake the write head
        const double minD = rev ? 64.0
                          : (p.freeze ? span + 64.0 : std::max (0.0, length * (rate - 1.0)) + 64.0);

        const double memMs = getMemoryMs();
        const double posMs = std::clamp ((double) p.position, 0.0, memMs - 20.0);
        const double widMs = std::clamp ((double) p.width, 1.0, memMs - posMs);
        double d = minD + (posMs + rnd() * widMs) * sr / 1000.0;
        const double oldest = rev ? d + span : d;
        if (oldest > maxOld) d -= oldest - maxOld;
        if (d < minD) d = minD;

        double pos = writePos - d;
        while (pos < 0.0) pos += len;

        g->active = true;
        g->pos = pos;
        g->inc = rev ? -rate : rate;
        g->n = 0;
        g->length = length;
        g->amp = 1.0 - rnd() * p.ampVar / 100.0;
        g->edge = (0.02 + 0.98 * p.shape / 100.0) / 2.0;
        place (*g, p, numOuts);
    }

    double sr = 48000.0, memorySamples = 480000.0;
    std::vector<float> buffer;
    int mask = 0, writePos = 0, currentOutputs = 2;
    Grain grains[maxGrains];
    double nextGrain = 0.0;
    uint32_t seed = 2463534242u;
    double mixNow = 1.0, gainNow = 1.0, mixTarget = 1.0, gainTarget = 1.0, smoothCoef = 0.001;
    double driftPhase = 0, driftFrom = 0, driftTo = 0, driftNow = 0, ringPhase = 0;
    int teleCount = 0;
    float outPeakAcc = 0.0f;
};

} // namespace baraja
