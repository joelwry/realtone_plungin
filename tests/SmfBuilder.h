#pragma once

#include <cstdint>
#include <vector>

namespace smf {

// Minimal Standard MIDI File writer for tests.
struct SmfBuilder
{
    std::vector<std::vector<std::uint8_t>> tracks;
    int format = 1;
    int division = 480;

    static void vlq(std::vector<std::uint8_t>& v, std::uint32_t x)
    {
        std::uint8_t buf[5];
        int n = 0;
        buf[n++] = x & 0x7F;
        while ((x >>= 7) > 0)
            buf[n++] = static_cast<std::uint8_t>((x & 0x7F) | 0x80);
        while (n > 0)
            v.push_back(buf[--n]);
    }
    static void be32(std::vector<std::uint8_t>& v, std::uint32_t x)
    {
        for (int s = 24; s >= 0; s -= 8)
            v.push_back(static_cast<std::uint8_t>(x >> s));
    }

    std::vector<std::uint8_t> bytes() const
    {
        std::vector<std::uint8_t> out = {'M', 'T', 'h', 'd'};
        be32(out, 6);
        out.push_back(0); out.push_back(static_cast<std::uint8_t>(format));
        out.push_back(0); out.push_back(static_cast<std::uint8_t>(tracks.size()));
        out.push_back(static_cast<std::uint8_t>(division >> 8)); out.push_back(static_cast<std::uint8_t>(division & 0xFF));
        for (const auto& t : tracks)
        {
            out.insert(out.end(), {'M', 'T', 'r', 'k'});
            be32(out, static_cast<std::uint32_t>(t.size() + 4));
            out.insert(out.end(), t.begin(), t.end());
            out.insert(out.end(), {0x00, 0xFF, 0x2F, 0x00});
        }
        return out;
    }
};

inline void tempo(std::vector<std::uint8_t>& t, std::uint32_t delta, std::uint32_t micros)
{
    SmfBuilder::vlq(t, delta);
    t.insert(t.end(), {0xFF, 0x51, 0x03, static_cast<std::uint8_t>(micros >> 16), static_cast<std::uint8_t>(micros >> 8), static_cast<std::uint8_t>(micros)});
}
inline void note(std::vector<std::uint8_t>& t, std::uint32_t delta, int ch, int n, int vel, bool on)
{
    SmfBuilder::vlq(t, delta);
    t.push_back(static_cast<std::uint8_t>((on ? 0x90 : 0x80) | ch));
    t.push_back(static_cast<std::uint8_t>(n));
    t.push_back(static_cast<std::uint8_t>(vel));
}


}  // namespace smf
