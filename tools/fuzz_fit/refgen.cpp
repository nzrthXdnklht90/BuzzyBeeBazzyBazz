// Generates reference outputs from Big Dill's validated RtEngine for a set of control settings and stimuli.
#include "sp/RtEngine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
int main (int argc, char** argv)
{
    const double fs = 192000;
    // args: sustain filter scoop crunch clip amp(V) kind(0 sine82,1 sine220,2 bassphrase,3 noise-lowlevel) out.raw
    sp::RtControls c; c.sustain = atof (argv[1]); c.filter = atof (argv[2]); c.scoop = atof (argv[3]); c.crunch = atof (argv[4]); c.clip = atof (argv[5]);
    c.loudness = 1.0; c.voice = 1.0;
    const double amp = atof (argv[6]); const int kind = atoi (argv[7]);
    sp::RtEngine e; e.prepare (fs, c, true);
    const int N = (int) (0.35 * fs);
    std::vector<double> x (N), y (N);
    unsigned s = 1;
    for (int i = 0; i < N; ++i)
    {
        const double t = i / fs; double v = 0;
        if (kind == 0) v = std::sin (2 * M_PI * 82.4 * t);
        else if (kind == 1) v = std::sin (2 * M_PI * 220 * t);
        else if (kind == 2) { const double f0 = (t < 0.175 ? 41.2 : 61.7), tn = std::fmod (t, 0.175); for (int h = 1; h < 30; ++h) v += std::sin (2 * M_PI * h * f0 * tn) / h * std::exp (-tn * h); v *= 0.6 * std::exp (-tn * 3); }
        else { s = s * 1664525u + 1013904223u; v = ((s >> 8) / 16777216.0 - 0.5) * 2; }
        x[i] = amp * v;
    }
    e.process (x.data(), y.data(), N);
    FILE* f = fopen (argv[8], "wb"); fwrite (y.data(), sizeof (double), N, f); fclose (f);
    FILE* g = fopen ((std::string (argv[8]) + ".in").c_str(), "wb"); fwrite (x.data(), sizeof (double), N, g); fclose (g);
    std::printf ("ok capped=%llu\n", (unsigned long long) e.stats().capped);
}
