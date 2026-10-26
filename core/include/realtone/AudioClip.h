#pragma once

#include <cstdint>
#include <vector>

namespace realtone {

struct LoopPoints
{
    bool valid = false;
    std::int64_t start = 0;  // first frame of the loop
    std::int64_t end = 0;    // last frame of the loop (inclusive)
};

// Planar float audio held in memory. Used for decoded sources, loaded samples
// and rendered output.
struct AudioClip
{
    double sampleRate = 0.0;
    std::vector<std::vector<float>> channels;
    LoopPoints loop;

    int numChannels() const { return static_cast<int>(channels.size()); }
    std::int64_t numFrames() const { return channels.empty() ? 0 : static_cast<std::int64_t>(channels[0].size()); }
    double durationSeconds() const { return sampleRate > 0.0 ? static_cast<double>(numFrames()) / sampleRate : 0.0; }

    // Average of all channels. Returns an empty vector for an empty clip.
    std::vector<float> mixToMono() const;

    // Largest absolute sample value across all channels.
    float peak() const;
};

}  // namespace realtone
