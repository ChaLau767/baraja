// Prueba rápida del motor: procesa una señal con varias configuraciones extremas
// y verifica que la salida no tenga valores inválidos ni quede en silencio.

#include "BarajaEngine.h"
#include <cstdio>
#include <vector>

int main()
{
    struct Case { const char* name; baraja::Params p; };
    std::vector<Case> cases;

    { baraja::Params p; cases.push_back ({ "inicial", p }); }
    { baraja::Params p; p.pitch = 12; p.pitchVar = 7; p.reverse = 50; cases.push_back ({ "alturas e inversion", p }); }
    { baraja::Params p; p.drift = 24; p.driftRate = 8; p.vdir = 1; p.pitchVar = 24; cases.push_back ({ "deriva extrema", p }); }
    { baraja::Params p; p.size = 1000; p.density = 16; p.pitch = 24; p.delay = 8000; cases.push_back ({ "fragmentos largos y agudos", p }); }
    { baraja::Params p; p.size = 5; p.density = 16; p.jitter = 100; p.shape = 0; cases.push_back ({ "fragmentos minimos", p }); }

    int failures = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (auto& c : cases)
        {
            baraja::Engine e;
            e.prepare (sr);
            baraja::Telemetry t;
            const int block = 256;
            std::vector<float> in (block), l (block), r (block);
            double peak = 0.0;
            bool bad = false;
            long long k = 0;
            const int blocks = (int) (sr * 6.0 / block);
            for (int b = 0; b < blocks; ++b)
            {
                auto p = c.p;
                if (b == blocks / 2) p.freeze = true;   // la segunda mitad, congelada
                if (b > blocks / 2) p.freeze = true;
                for (int i = 0; i < block; ++i, ++k)
                    in[i] = (float) (0.5 * std::sin (k * 2.0 * 3.14159265 * 220.0 / sr));
                if (b > blocks * 3 / 4) std::fill (in.begin(), in.end(), 0.0f);   // fuente apagada
                e.process (in.data(), in.data(), l.data(), r.data(), block, p);
                if (b % 8 == 0) e.snapshot (t, p);
                for (int i = 0; i < block; ++i)
                {
                    if (! std::isfinite (l[i]) || ! std::isfinite (r[i])) bad = true;
                    peak = std::max (peak, (double) std::max (std::abs (l[i]), std::abs (r[i])));
                }
            }
            const bool ok = ! bad && peak > 0.01 && peak < 8.0;
            std::printf ("%-28s %6.0f Hz  pico %.3f  %s\n", c.name, sr, peak, ok ? "ok" : "FALLA");
            if (! ok) ++failures;
        }
    }
    return failures == 0 ? 0 : 1;
}
