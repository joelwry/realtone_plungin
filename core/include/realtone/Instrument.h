#pragma once

#include "realtone/AudioClip.h"
#include "realtone/Sfz.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace realtone {

struct SampleData
{
    std::vector<float> left;
    std::vector<float> right;  // empty for mono samples
    double sampleRate = 44100.0;
    std::int64_t frames = 0;
    LoopPoints loop;

    bool stereo() const { return !right.empty(); }
    std::size_t bytes() const { return (left.size() + right.size()) * sizeof(float); }
};

// A region with its sample resolved and its static gains precomputed, so the
// audio thread does no parsing or maths beyond a few multiplies per note.
struct Region
{
    SfzRegion def;
    std::shared_ptr<const SampleData> sample;
    float gain = 1.0f;       // volume opcode converted to linear
    float panLeft = 1.0f;    // constant-power pan, unity at centre
    float panRight = 1.0f;
    double startFrame = 0.0;
    double endFrame = 0.0;   // exclusive
    bool looping = false;
    bool loopWhileHeld = false;  // loop_sustain: leave the loop on key release
    double loopStart = 0.0;
    double loopEnd = 0.0;        // last frame of the loop
};

struct LoadReport
{
    std::vector<std::string> missingSamples;
    std::vector<std::string> failedSamples;  // "file: reason"
    std::vector<std::string> warnings;
    std::vector<std::string> unsupportedOpcodes;
    std::size_t regionsDeclared = 0;
    std::size_t regionsPlayable = 0;
    std::uint64_t bytesLoaded = 0;

    bool complete() const { return missingSamples.empty() && failedSamples.empty(); }
};

class Instrument
{
public:
    std::string name;
    std::vector<Region> regions;
    LoadReport report;

    // Region indices per MIDI key for note-on and release-trigger lookups.
    std::vector<std::uint32_t> attackByKey[128];
    std::vector<std::uint32_t> releaseByKey[128];

    bool hasReleaseRegions() const;
    void buildIndex();
    std::size_t memoryBytes() const;
};

struct LoadOptions
{
    std::uint64_t maxBytes = 3ULL * 1024 * 1024 * 1024;
    // 0..1 plus a short description of the current step.
    std::function<void(float, const std::string&)> progress;
    const std::atomic<bool>* cancel = nullptr;
    bool allowPartial = true;  // load what exists and report the rest
    int threads = 0;           // 0 = choose automatically
};

struct LoadResult
{
    bool ok = false;
    bool cancelled = false;
    std::string error;
    std::shared_ptr<Instrument> instrument;
};

// Parses the SFZ file and decodes every referenced sample into memory.
// Runs on a background thread; never call from the audio callback.
LoadResult loadInstrument(const std::filesystem::path& sfzPath, const std::filesystem::path& rootDir,
                          const LoadOptions& options = {});

}  // namespace realtone
