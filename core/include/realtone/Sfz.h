#pragma once

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace realtone {

// One <region> after the global/master/group inheritance has been flattened.
// Only the opcode subset the engine honours is represented; everything else is
// reported through SfzParseResult::unsupportedOpcodes so nothing is dropped silently.
struct SfzRegion
{
    enum class Trigger { Attack, Release, First, Legato };
    enum class LoopMode { Default, NoLoop, OneShot, LoopContinuous, LoopSustain };

    std::string sample;  // path relative to the .sfz directory, '/' separators
    int lokey = 0, hikey = 127;
    int lovel = 0, hivel = 127;
    int pitchKeycenter = 60;
    float pitchKeytrack = 100.0f;  // cents per key, 0 disables pitch tracking
    float lorand = 0.0f, hirand = 1.0f;
    int seqLength = 1, seqPosition = 1;
    Trigger trigger = Trigger::Attack;
    LoopMode loopMode = LoopMode::Default;
    double offset = 0.0;       // frames
    double end = -1.0;         // frames, <0 means "to the end of the file"
    double loopStart = -1.0;   // frames, <0 means "from the file's loop info"
    double loopEnd = -1.0;
    float volumeDb = 0.0f;
    float pan = 0.0f;          // -100..100
    float tuneCents = 0.0f;    // tune + transpose * 100
    float ampVeltrack = 100.0f;
    float ampegAttack = 0.0f;  // seconds
    float ampegHold = 0.0f;
    float ampegDecay = 0.0f;
    float ampegSustain = 100.0f;  // percent
    float ampegRelease = 0.0f;
    int group = 0;
    int offBy = 0;
};

struct SfzParseResult
{
    bool ok = false;
    std::string error;
    std::vector<SfzRegion> regions;
    std::set<std::string> unsupportedOpcodes;
    std::vector<std::string> warnings;
};

// `text` is the file content, `baseDir` the directory #include paths are resolved against.
// `rootDir` bounds both #include and sample paths: anything resolving outside is rejected.
SfzParseResult parseSfz(const std::string& text, const std::filesystem::path& baseDir,
                        const std::filesystem::path& rootDir);

SfzParseResult parseSfzFile(const std::filesystem::path& path, const std::filesystem::path& rootDir);

// "c4" -> 60, "f#3" -> 54, "eb2" -> 39, plain integers pass through. Returns -1 when invalid.
int parseNoteToken(const std::string& token);

}  // namespace realtone
