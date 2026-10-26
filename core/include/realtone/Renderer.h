#pragma once

#include "realtone/AudioClip.h"
#include "realtone/Sequence.h"
#include "realtone/SamplerEngine.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace realtone {

enum class NormalizeMode
{
    None,          // leave the level alone; the result reports if it exceeds full scale
    LimitPeak,     // only turn down if the peak would exceed the target
    NormalizePeak  // scale to the target peak
};

struct RenderSettings
{
    double sampleRate = 48000.0;
    int blockSize = 512;
    EngineParams engine;
    int maxVoices = 128;

    // Performance edits applied to the sequence before rendering.
    int transposeSemitones = 0;
    double tempoScale = 1.0;            // 1 = as written, 2 = twice as fast
    float velocityScale = 1.0f;
    int velocityOffset = 0;
    double timingHumanizeMs = 0.0;      // random shift of whole notes, +/- this many milliseconds
    std::vector<int> enabledTracks;     // empty = every track
    bool includeControllers = true;
    bool includePitchBend = true;

    double maxTailSeconds = 10.0;
    double silenceThresholdDb = -80.0;
    double silenceHoldSeconds = 0.25;

    NormalizeMode normalize = NormalizeMode::LimitPeak;
    float targetPeakDb = -1.0f;

    std::uint32_t seed = 1;  // same seed + same input = same output
};

struct RenderResult
{
    bool ok = false;
    bool cancelled = false;
    std::string error;
    std::vector<std::string> warnings;
    AudioClip clip;                    // stereo
    double peakBeforeGainDb = -200.0;  // as produced by the engine
    double appliedGainDb = 0.0;
    int notesRendered = 0;
    int notesSkipped = 0;              // out of range after transposing, or filtered out
    int peakVoices = 0;
};

using ProgressFn = std::function<void(float fraction, const std::string& stage)>;

// Offline, faster-than-realtime render. Call from a worker thread.
RenderResult renderSequence(std::shared_ptr<const Instrument> instrument, const Sequence& sequence,
                            const RenderSettings& settings, const ProgressFn& progress = {},
                            const std::atomic<bool>* cancel = nullptr);

}  // namespace realtone
