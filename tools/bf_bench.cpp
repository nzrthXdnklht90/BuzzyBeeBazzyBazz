// bf_bench: repeatable CPU benchmark of the engine (share of one core per configuration).
#include "Engine.h"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace bassforge;

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int q = 0; q < 3; ++q)
            for (int block : { 64, 256 })
            {
                EngineParams p; p.quality = (Quality) q; p.mic[1].on = true;
                Engine e; e.setParams (p); e.prepare (fs, block);
                const int seconds = 10;
                const size_t n = (size_t) (fs * seconds);
                std::vector<float> in ((size_t) block), l ((size_t) block), r ((size_t) block);
                double phase = 0;
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos < n; pos += (size_t) block)
                {
                    for (int i = 0; i < block; ++i) { in[(size_t) i] = (float) (0.25 * std::sin (phase)); phase += 2 * kPi * 55 / fs; }
                    const float* ip[1] = { in.data() }; float* op[2] = { l.data(), r.data() };
                    e.process (ip, 1, op, 2, block);
                }
                const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                std::printf ("fs %6.0f  quality %-8s  block %4d : %5.2f%% of one core (realtime x%.0f), latency %d samples\n", fs,
                             q == 0 ? "Eco" : q == 1 ? "Standard" : "High", block, 100.0 * sec / seconds, seconds / sec, e.getLatencySamples());
            }
    // per-voice cost (48 kHz, Standard, block 256)
    for (int m = 0; m < 3; ++m)
    {
        EngineParams p; p.model = (DriveModel) m; p.mic[1].on = true;
        Engine e; e.setParams (p); e.prepare (48000.0, 256);
        std::vector<float> in (256), l (256), r (256); double phase = 0;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < 48000 * 10 / 256; ++b)
        {
            for (auto& v : in) { v = (float) (0.25 * std::sin (phase)); phase += 2 * kPi * 55 / 48000.0; }
            const float* ip[1] = { in.data() }; float* op[2] = { l.data(), r.data() };
            e.process (ip, 1, op, 2, 256);
        }
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("voice %-6s (48 kHz, Standard, 256): %5.2f%% of one core\n", m == 0 ? "Drive" : m == 1 ? "Fuzz" : "Modern", 100.0 * sec / 10.0);
    }
    return 0;
}
