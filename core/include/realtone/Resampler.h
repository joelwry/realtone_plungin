#pragma once

#include "realtone/AudioClip.h"

#include <vector>

namespace realtone {

// Offline band-limited resampler (windowed sinc, Kaiser window). Intended for
// export and analysis paths, never for the audio callback.
std::vector<float> resample(const std::vector<float>& input, double sourceRate, double targetRate);
AudioClip resampleClip(const AudioClip& input, double targetRate);

}  // namespace realtone
