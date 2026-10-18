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
    std::printf (failures == 0 ? "all cases passed\n" : "%d cases failed\n", failures);
    return failures == 0 ? 0 : 1;
}
