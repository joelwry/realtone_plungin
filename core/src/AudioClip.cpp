#include "realtone/AudioClip.h"

#include <algorithm>
#include <cmath>

namespace realtone {

std::vector<float> AudioClip::mixToMono() const
{
    std::vector<float> mono(static_cast<std::size_t>(numFrames()), 0.0f);
    if (channels.empty())
        return mono;

    const float scale = 1.0f / static_cast<float>(channels.size());
    for (const auto& ch : channels)
        for (std::size_t i = 0; i < mono.size(); ++i)
            mono[i] += ch[i] * scale;
    return mono;
}

float AudioClip::peak() const
{
    float p = 0.0f;
    for (const auto& ch : channels)
        for (float s : ch)
            p = std::max(p, std::fabs(s));
    return p;
}

}  // namespace realtone
