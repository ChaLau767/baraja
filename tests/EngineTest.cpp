// Engine test: runs a signal through extreme settings, channel counts and memory sizes,
// and checks the output has no invalid values and never goes silent.

#include "BarajaEngine.h"
#include <cstdio>
#include <vector>

int main()
{
    struct Case { const char* name; baraja::Params p; };
    std::vector<Case> cases;

    { baraja::Params p; cases.push_back ({ "default", p }); }
    { baraja::Params p; p.pitch = 12; p.pitchVar = 7; p.reverse = 50; cases.push_back ({ "pitch and reverse", p }); }
    { baraja::Params p; p.drift = 24; p.driftRate = 8; p.vdir = 1; p.pitchVar = 24; cases.push_back ({ "extreme drift", p }); }
    { baraja::Params p; p.size = 1000; p.density = 16; p.pitch = 24; p.width = 60000; cases.push_back ({ "long, high, wide", p }); }
    { baraja::Params p; p.size = 5; p.density = 16; p.jitter = 100; p.shape = 0; cases.push_back ({ "tiny fragments", p }); }
    { baraja::Params p; p.size = 2; p.sizeVar = 0; p.density = 16; p.shape = 50; cases.push_back ({ "2 ms fragments", p }); }
    { baraja::Params p; p.size = 10000; p.density = 4; p.width = 60000; p.reverse = 50; cases.push_back ({ "10 s fragments", p }); }
    {
        // FRAG: every fragment gets its own size, pitch, place and window position
        baraja::Params p; p.density = 8; p.width = 2000;
        modlib::Range sizeR { modlib::Range::logarithmic, 2.0f, 10000.0f, 1.0f };
        modlib::Range pitchR { modlib::Range::linear, -24.0f, 24.0f, 1.0f };
        modlib::Range posR { modlib::Range::linear, 0.0f, 60000.0f, 1.0f };
        modlib::Range spreadR { modlib::Range::linear, 0.0f, 100.0f, 1.0f };
        p.frag[0] = { baraja::fragSize, 0.3f, sizeR };
        p.frag[1] = { baraja::fragPitch, 0.5f, pitchR };
        p.frag[2] = { baraja::fragPosition, 1.0f, posR };
        p.frag[3] = { baraja::fragSpread, -1.0f, spreadR };
        p.numFrag = 4;
        cases.push_back ({ "FRAG routes", p });
    }
    { baraja::Params p; p.position = 3000; p.width = 500; p.spaceMode = 0; p.spread = 100; p.density = 12; cases.push_back ({ "window in the past, points", p }); }
    { baraja::Params p; p.position = 1000; p.width = 2000; p.spaceMode = 1; p.spread = 40; p.rotation = 0.5f; p.center = 90; cases.push_back ({ "ring, rotating", p }); }
    { baraja::Params p; p.order = 100; p.speed = 0.25f; p.size = 200; p.density = 8; p.shape = 95; p.width = 4000; cases.push_back ({ "freeze: slow head", p }); }
    { baraja::Params p; p.order = 95; p.speed = 0; p.width = 600; p.position = 200; cases.push_back ({ "freeze: head stopped", p }); }
    { baraja::Params p; p.order = 100; p.speed = -4; p.pitch = 12; p.width = 3000; cases.push_back ({ "freeze: fast backwards", p }); }
    { baraja::Params p; p.order = 50; p.speed = 4; p.pitch = -12; p.reverse = 50; p.width = 3000; cases.push_back ({ "half order, fast forward", p }); }

    int failures = 0;
    for (double memory : { 5.0, 60.0 })
    for (int outs : { 1, 2, 8, 64 })
    for (double sr : { 44100.0, 96000.0 })
    {
        for (auto& c : cases)
        {
            baraja::Engine e;
            e.prepare (sr, memory);
            baraja::Telemetry t;
            const int block = 256;
            std::vector<float> in (block);
            std::vector<std::vector<float>> out ((size_t) outs, std::vector<float> (block));
            std::vector<float*> ptrs;
            for (auto& o : out) ptrs.push_back (o.data());
            std::vector<double> chanPeak ((size_t) outs, 0.0);

            double peak = 0.0;
            bool bad = false;
            long long k = 0;
            const int blocks = (int) (sr * 7.0 / block);
            for (int b = 0; b < blocks; ++b)
            {
                auto p = c.p;
                if (b >= blocks / 2) p.freeze = true;   // second half frozen
                for (int i = 0; i < block; ++i, ++k)
                    in[(size_t) i] = (float) (0.5 * std::sin (k * 2.0 * 3.14159265 * 220.0 / sr));
                if (b > blocks * 3 / 4) std::fill (in.begin(), in.end(), 0.0f);   // source stops
                e.process (in.data(), in.data(), ptrs.data(), outs, block, p);
                if (b % 8 == 0) e.snapshot (t);
                for (int ch = 0; ch < outs; ++ch)
                    for (int i = 0; i < block; ++i)
                    {
                        const float v = out[(size_t) ch][(size_t) i];
                        if (! std::isfinite (v)) bad = true;
                        peak = std::max (peak, (double) std::abs (v));
                        chanPeak[(size_t) ch] = std::max (chanPeak[(size_t) ch], (double) std::abs (v));
                    }
            }
            int silent = 0;
            for (auto cp : chanPeak) if (cp < 1.0e-4) ++silent;
            // with a wide spread every output should receive something
            const bool coverageOk = c.p.spread < 100 || silent == 0;
            const bool ok = ! bad && peak > 0.01 && peak < 8.0 && coverageOk;
            if (! ok || outs == 64)
                std::printf ("%-28s mem %2.0f s  %2d out  %6.0f Hz  peak %.3f  silent outs %d  %s\n",
                             c.name, memory, outs, sr, peak, silent, ok ? "ok" : "FAIL");
            if (! ok) ++failures;
        }
    }
    // modulation library: LFO shapes stay in range, synced LFOs follow the position, the follower rises and falls
    {
        bool ok = true;
        for (int shape = 0; shape < modlib::numShapes; ++shape)
        {
            modlib::Lfo l; l.reset (99); l.shape = shape;
            for (int i = 0; i < 20000; ++i)
            {
                const float v = l.advanceFree (0.0037);
                if (! (v >= -1.0001f && v <= 1.0001f)) ok = false;
            }
        }
        modlib::Lfo s; s.reset (5); s.shape = modlib::saw;
        const float a = s.syncTo (2.0, 4.0), b = s.syncTo (6.0, 4.0);
        if (std::abs (a - b) > 1.0e-5f || std::abs (a - 0.0f) > 1.0e-4f) ok = false;   // one bar later: the same point
        modlib::Range lg { modlib::Range::logarithmic, 2.0f, 10000.0f, 1.0f };
        if (std::abs (lg.fromNorm (lg.toNorm (120.0f)) - 120.0f) > 0.01f) ok = false;
        if (std::abs (lg.apply (120.0f, 2.0f) - 10000.0f) > 0.5f) ok = false;   // clamps to the top
        modlib::Follower f;
        std::vector<float> loud (4800, 0.5f), quiet (48000, 0.0f);
        const float up = f.process (loud.data(), loud.data(), 4800, 10.0f, 250.0f, 6.0f, 48000.0);
        const float down = f.process (quiet.data(), quiet.data(), 48000, 10.0f, 250.0f, 6.0f, 48000.0);
        if (! (up > 0.9f && down < 0.05f)) ok = false;
        std::printf ("%-28s %s\n", "modulation library", ok ? "ok" : "FAIL");
        if (! ok) ++failures;
    }
    std::printf (failures == 0 ? "all cases passed\n" : "%d cases failed\n", failures);
    return failures == 0 ? 0 : 1;
}
