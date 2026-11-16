// Minimal WAV reader/writer for CrestForge tools (PCM 16/24/32, float32/64). Not used by the plug-in.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct WavData { int channels = 0; double sampleRate = 0; std::vector<std::vector<double>> ch; };

inline bool readWav (const std::string& path, WavData& w)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (! f) return false;
    std::vector<uint8_t> d;
    uint8_t buf[65536]; size_t n;
    while ((n = std::fread (buf, 1, sizeof buf, f)) > 0) d.insert (d.end(), buf, buf + n);
    std::fclose (f);
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) || std::memcmp (d.data() + 8, "WAVE", 4)) return false;
    size_t p = 12; int fmtTag = 0, bits = 0; const uint8_t* data = nullptr; size_t dataLen = 0;
    auto u16 = [&] (size_t o) { return (int) (d[o] | (d[o + 1] << 8)); };
    auto u32 = [&] (size_t o) { return (uint32_t) (d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | ((uint32_t) d[o + 3] << 24)); };
    while (p + 8 <= d.size())
    {
        const uint32_t len = u32 (p + 4);
        if (! std::memcmp (&d[p], "fmt ", 4)) { fmtTag = u16 (p + 8); w.channels = u16 (p + 10); w.sampleRate = u32 (p + 12); bits = u16 (p + 22);
            if (fmtTag == 0xFFFE && len >= 40) fmtTag = u16 (p + 32); }
        else if (! std::memcmp (&d[p], "data", 4)) { data = &d[p + 8]; dataLen = std::min<size_t> (len, d.size() - p - 8); }
        p += 8 + len + (len & 1);
    }
    if (! data || w.channels <= 0) return false;
    const int bps = bits / 8; const size_t frames = dataLen / (size_t) (bps * w.channels);
    w.ch.assign ((size_t) w.channels, std::vector<double> (frames));
    for (size_t i = 0; i < frames; ++i)
        for (int c = 0; c < w.channels; ++c)
        {
            const uint8_t* s = data + (i * (size_t) w.channels + (size_t) c) * (size_t) bps;
            double v = 0;
            if (fmtTag == 3 && bits == 32) { float x; std::memcpy (&x, s, 4); v = x; }
            else if (fmtTag == 3 && bits == 64) { double x; std::memcpy (&x, s, 8); v = x; }
            else if (bits == 16) v = (int16_t) (s[0] | (s[1] << 8)) / 32768.0;
            else if (bits == 24) { int32_t x = (s[0] << 8) | (s[1] << 16) | (s[2] << 24); v = (x >> 8) / 8388608.0; }
            else if (bits == 32) { int32_t x; std::memcpy (&x, s, 4); v = x / 2147483648.0; }
            w.ch[(size_t) c][i] = v;
        }
    return true;
}

/** Writes float32 (bits=32) or float64 (bits=64) WAV. */
inline bool writeWav (const std::string& path, const WavData& w, int bits = 32)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return false;
    const uint32_t frames = (uint32_t) w.ch[0].size(), bps = (uint32_t) bits / 8, nc = (uint32_t) w.channels;
    const uint32_t dataLen = frames * nc * bps;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataLen); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (3); w16 ((uint16_t) nc); w32 ((uint32_t) w.sampleRate); w32 ((uint32_t) w.sampleRate * nc * bps); w16 ((uint16_t) (nc * bps)); w16 ((uint16_t) bits);
    std::fwrite ("data", 1, 4, f); w32 (dataLen);
    for (uint32_t i = 0; i < frames; ++i)
        for (uint32_t c = 0; c < nc; ++c)
        {
            if (bits == 64) { double x = w.ch[c][i]; std::fwrite (&x, 8, 1, f); }
            else { float x = (float) w.ch[c][i]; std::fwrite (&x, 4, 1, f); }
        }
    std::fclose (f);
    return true;
}
