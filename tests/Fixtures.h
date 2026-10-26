#pragma once

// Synthetic audio and instrument fixtures used only by tests. They are generated
// on the fly, so the repository carries no audio files.

#include "realtone/AudioFile.h"
#include "realtone/Instrument.h"
#include "realtone/SamplerEngine.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace fixtures {

constexpr double kPi = 3.14159265358979323846;

inline double midiToHz(double note) { return 440.0 * std::pow(2.0, (note - 69.0) / 12.0); }

// Harmonic tone with a slow exponential decay.
inline std::vector<float> tone(double hz, double seconds, double rate, double decayPerSecond = 0.6, double amp = 0.5)
{
    const auto n = static_cast<std::size_t>(seconds * rate);
    std::vector<float> out(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double>(i) / rate;
        const double env = std::exp(-decayPerSecond * t);
        double s = std::sin(2 * kPi * hz * t) + 0.4 * std::sin(4 * kPi * hz * t) + 0.2 * std::sin(6 * kPi * hz * t);
        out[i] = static_cast<float>(amp * env * s / 1.6);
    }
    return out;
}

inline realtone::AudioClip toneClip(double hz, double seconds, double rate, int channels = 1, double decay = 0.6, double amp = 0.5)
{
    realtone::AudioClip c;
    c.sampleRate = rate;
    for (int ch = 0; ch < channels; ++ch)
        c.channels.push_back(tone(hz, seconds, rate, decay, amp));
    return c;
}

inline void writeText(const std::filesystem::path& p, const std::string& text)
{
    std::filesystem::create_directories(p.parent_path());
    std::ofstream(p) << text;
}

struct TestInstrument
{
    std::filesystem::path root;
    std::filesystem::path sfz;
};

// Samples every third semitone from C2 to C7 (pitch_keycenter), two velocity layers and a
// two-way round robin on the upper layer. `sustained` makes the samples long and looped.
inline TestInstrument makeInstrumentFiles(const std::filesystem::path& root, bool withReleaseRegions = false,
                                          double sampleRate = 44100.0, bool looped = false)
{
    using namespace realtone;
    std::filesystem::create_directories(root / "samples");
    std::string sfz = "<control>\ndefault_path=samples\\\n<global>\nampeg_release=0.3\n";
    for (int centre = 36; centre <= 96; centre += 3)
    {
        const int lo = centre - 1, hi = centre + 1;
        for (int layer = 0; layer < 2; ++layer)
        {
            for (int rr = 0; rr < (layer == 1 ? 2 : 1); ++rr)
            {
                const std::string name = "n" + std::to_string(centre) + "_v" + std::to_string(layer) + "_" + std::to_string(rr) + ".wav";
                AudioClip clip;
                clip.sampleRate = sampleRate;
                clip.channels.push_back(tone(midiToHz(centre), looped ? 1.0 : 3.0, sampleRate, looped ? 0.0 : 0.6,
                                             layer == 0 ? 0.3 : (rr == 0 ? 0.6 : 0.4)));
                WavWriteOptions o;
                o.format = WavSampleFormat::Float32;
                o.overwrite = true;
                writeWavFile(root / "samples" / name, clip, o);

                sfz += "<region> sample=" + name + " lokey=" + std::to_string(lo) + " hikey=" + std::to_string(hi) +
                       " pitch_keycenter=" + std::to_string(centre) + " lovel=" + (layer == 0 ? "1" : "64") +
                       " hivel=" + (layer == 0 ? "63" : "127");
                if (layer == 1)
                    sfz += " seq_length=2 seq_position=" + std::to_string(rr + 1);
                if (looped)
                {
                    // One exact period multiple keeps the loop seamless.
                    const double period = sampleRate / midiToHz(centre);
                    const int periods = static_cast<int>(std::floor(0.5 * sampleRate / period));
                    sfz += " loop_mode=loop_continuous loop_start=" + std::to_string(static_cast<long>(std::round(period))) +
                           " loop_end=" + std::to_string(static_cast<long>(std::round(period * periods)));
                }
                sfz += "\n";
            }
        }
        if (withReleaseRegions)
        {
            const std::string name = "rel" + std::to_string(centre) + ".wav";
            AudioClip clip;
            clip.sampleRate = sampleRate;
            clip.channels.push_back(tone(midiToHz(centre) * 2, 0.2, sampleRate, 12.0, 0.3));
            WavWriteOptions o;
            o.format = WavSampleFormat::Float32;
            o.overwrite = true;
            writeWavFile(root / "samples" / name, clip, o);
            sfz += "<region> trigger=release sample=" + name + " lokey=" + std::to_string(lo) + " hikey=" + std::to_string(hi) +
                   " pitch_keycenter=" + std::to_string(centre) + "\n";
        }
    }
    TestInstrument ti;
    ti.root = root;
    ti.sfz = root / "test.sfz";
    writeText(ti.sfz, sfz);
    return ti;
}

inline std::shared_ptr<realtone::Instrument> loadOrDie(const TestInstrument& ti)
{
    auto r = realtone::loadInstrument(ti.sfz, ti.root);
    return r.instrument;
}

// Autocorrelation pitch estimate of a steady section, independent of the production analyser.
inline double estimateHz(const float* x, int n, double rate, double minHz = 30.0, double maxHz = 4000.0)
{
    const int minLag = static_cast<int>(rate / maxHz), maxLag = static_cast<int>(rate / minHz);
    std::vector<double> ac(static_cast<std::size_t>(maxLag + 2), 0.0);
    for (int lag = minLag; lag <= maxLag && lag < n / 2; ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int i = 0; i + lag < n; ++i)
        {
            s += static_cast<double>(x[i]) * x[i + lag];
            e1 += static_cast<double>(x[i]) * x[i];
            e2 += static_cast<double>(x[i + lag]) * x[i + lag];
        }
        ac[static_cast<std::size_t>(lag)] = s / std::sqrt(e1 * e2 + 1e-30);
    }
    // First strong local maximum (avoids picking sub-octave multiples).
    double best = 0;
    for (int lag = minLag; lag < maxLag; ++lag)
        best = std::max(best, ac[static_cast<std::size_t>(lag)]);
    for (int lag = minLag + 1; lag < maxLag - 1; ++lag)
    {
        const double a = ac[static_cast<std::size_t>(lag)];
        if (a > 0.9 * best && a >= ac[static_cast<std::size_t>(lag - 1)] && a >= ac[static_cast<std::size_t>(lag + 1)])
        {
            const double l = ac[static_cast<std::size_t>(lag - 1)], r = ac[static_cast<std::size_t>(lag + 1)];
            const double d = l - 2 * a + r;
            const double shift = std::fabs(d) > 1e-12 ? 0.5 * (l - r) / d : 0;
            return rate / (lag + shift);
        }
    }
    return 0.0;
}

inline double rms(const float* x, int n)
{
    double s = 0;
    for (int i = 0; i < n; ++i)
        s += static_cast<double>(x[i]) * x[i];
    return std::sqrt(s / std::max(1, n));
}

// Renders `frames` through the engine in blocks and returns the left channel.
struct Rendered
{
    std::vector<float> l, r;
};

inline Rendered run(realtone::SamplerEngine& eng, int frames, const std::vector<realtone::EngineEvent>& events = {}, int block = 256)
{
    Rendered out;
    out.l.resize(static_cast<std::size_t>(frames));
    out.r.resize(static_cast<std::size_t>(frames));
    std::size_t ev = 0;
    std::vector<realtone::EngineEvent> blockEvents;
    for (int pos = 0; pos < frames; pos += block)
    {
        const int n = std::min(block, frames - pos);
        blockEvents.clear();
        while (ev < events.size() && events[ev].offset < pos + n)
        {
            auto e = events[ev++];
            e.offset = std::max(0, e.offset - pos);
            blockEvents.push_back(e);
        }
        eng.process(blockEvents.data(), static_cast<int>(blockEvents.size()), out.l.data() + pos, out.r.data() + pos, n);
    }
    return out;
}

inline realtone::EngineEvent noteOn(int note, int vel, int at = 0)
{
    return {at, realtone::EngineEvent::Type::NoteOn, note, vel};
}
inline realtone::EngineEvent noteOff(int note, int at = 0)
{
    return {at, realtone::EngineEvent::Type::NoteOff, note, 0};
}
inline realtone::EngineEvent bend(int value14, int at = 0)
{
    return {at, realtone::EngineEvent::Type::PitchBend, 0, value14};
}
inline realtone::EngineEvent cc(int num, int value, int at = 0)
{
    return {at, realtone::EngineEvent::Type::Controller, num, value};
}

}  // namespace fixtures
