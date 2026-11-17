// bf_render: offline render through the BASSFORGE engine (for listening tests and A/B work).
// usage: bf_render in.wav out.wav [key=value ...]
//   keys: model=drive|fuzz|modern drive=0..1 character=0..1 cab=0..3 quality=0..2 input=dB
//         di=dB drivefader=dB room=dB roomtype=0..2 own=0..2 mic1=0..2 mic1pos=0..1 mic1dist=0..6
//         synth=1 (ignore in.wav, render the built-in test phrase at 48 kHz)
#include "Engine.h"
#include "WavIO.h"
#include <cstdlib>
#include <cstring>
#include <string>

using namespace bassforge;

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: bf_render in.wav out.wav [key=value ...]\n"); return 2; }
    EngineParams p;
    bool synth = false;
    for (int i = 3; i < argc; ++i)
    {
        std::string a = argv[i]; const auto eq = a.find ('=');
        if (eq == std::string::npos) continue;
        const std::string k = a.substr (0, eq), v = a.substr (eq + 1);
        const double d = std::atof (v.c_str());
        if (k == "model") p.model = v == "modern" ? DriveModel::Modern : v == "fuzz" ? DriveModel::Fuzz : DriveModel::Drive;
        else if (k == "drive") p.drive = d;
        else if (k == "character") p.character = d;
        else if (k == "cab") p.cab = (CabModel) (int) d;
        else if (k == "quality") p.quality = (Quality) (int) d;
        else if (k == "input") p.inputDb = d;
        else if (k == "di") p.faderDi = d;
        else if (k == "drivefader") p.faderDrive = d;
        else if (k == "room") p.faderRoom = d;
        else if (k == "roomtype") p.room = (RoomType) (int) d;
        else if (k == "own") p.ownership = (LowOwnership) (int) d;
        else if (k == "mic1") p.mic[0].type = (MicType) (int) d;
        else if (k == "mic1pos") p.mic[0].position = d;
        else if (k == "mic1dist") p.mic[0].distanceIn = d;
        else if (k == "synth") synth = d > 0;
    }
    WavData in;
    if (synth)
    {
        in.channels = 1; in.sampleRate = 48000; in.ch.assign (1, std::vector<double> (48000 * 4));
        const double notes[] = { 30.87, 41.2, 55.0, 73.42, 41.2, 82.41, 30.87, 61.74 };
        for (size_t i = 0; i < in.ch[0].size(); ++i)
        {
            const double t = i / 48000.0; const int k = (int) (t / 0.25); const double tn = t - k * 0.25, f0 = notes[k % 8];
            double s = 0; for (int h = 1; h <= 24; ++h) s += std::sin (2 * kPi * h * f0 * tn + 0.3 * h) / h * std::exp (-tn * h * 0.8);
            in.ch[0][i] = 0.12 * s * std::exp (-tn * 4.0) * (1.0 - std::exp (-tn * 800.0));
        }
    }
    else if (! readWav (argv[1], in)) { std::printf ("cannot read %s\n", argv[1]); return 1; }
    Engine e; e.setParams (p); e.prepare (in.sampleRate, 512);
    const int lat = e.getLatencySamples();
    const size_t n = in.ch[0].size();
    WavData out; out.channels = 2; out.sampleRate = in.sampleRate; out.ch.assign (2, std::vector<double> (n));
    const size_t extra = (size_t) lat + (size_t) (in.sampleRate * 0.5);
    std::vector<double> l (512), r (512), mono (512);
    size_t written = 0;
    for (size_t pos = 0; pos < n + extra; pos += 512)
    {
        const int len = (int) std::min<size_t> (512, n + extra - pos);
        for (int i = 0; i < len; ++i)
        {
            const size_t idx = pos + (size_t) i;
            double v = 0; if (idx < n) { for (int c = 0; c < in.channels; ++c) v += in.ch[(size_t) c][idx]; v /= in.channels; }
            mono[(size_t) i] = v;
        }
        const double* ip[1] = { mono.data() }; double* op[2] = { l.data(), r.data() };
        e.process (ip, 1, op, 2, len);
        for (int i = 0; i < len; ++i)
        {
            const long long dst = (long long) (pos + (size_t) i) - lat;   // remove the plug-in latency
            if (dst >= 0 && (size_t) dst < n) { out.ch[0][(size_t) dst] = l[(size_t) i]; out.ch[1][(size_t) dst] = r[(size_t) i]; ++written; }
        }
    }
    writeWav (argv[2], out);
    std::printf ("rendered %zu frames at %.0f Hz, latency %d samples compensated\n", written, in.sampleRate, lat);
    return 0;
}
