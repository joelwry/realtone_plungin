#pragma once

#include "realtone/AudioClip.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace realtone {

struct DecodeResult
{
    bool ok = false;
    std::string error;
    std::string format;  // "wav", "flac" or "mp3"
    AudioClip clip;
};

struct DecodeOptions
{
    // Hard limit so a corrupt header cannot make us allocate gigabytes.
    std::int64_t maxFrames = 48000LL * 60 * 30;  // 30 minutes at 48 kHz
    std::uintmax_t maxFileBytes = 1024ULL * 1024 * 1024;
};

DecodeResult decodeAudioFile(const std::filesystem::path& path, const DecodeOptions& options = {});
DecodeResult decodeAudioMemory(const std::uint8_t* data, std::size_t size, const DecodeOptions& options = {});

enum class WavSampleFormat
{
    Pcm16,
    Pcm24,
    Float32
};

struct WavWriteOptions
{
    WavSampleFormat format = WavSampleFormat::Pcm24;
    bool ditherPcm16 = true;     // TPDF dither, seeded so output is deterministic
    bool overwrite = false;      // never replace an existing file unless the caller says so
};

// Serialises to a RIFF/WAVE image. Samples outside [-1, 1] are clamped for the
// integer formats; callers are expected to handle headroom beforehand.
std::vector<std::uint8_t> encodeWav(const AudioClip& clip, const WavWriteOptions& options = {});

// Writes via a temporary file and renames, so a failure never leaves a
// half-written file at the destination.
bool writeWavFile(const std::filesystem::path& path, const AudioClip& clip, const WavWriteOptions& options,
                  std::string* error = nullptr);

}  // namespace realtone
