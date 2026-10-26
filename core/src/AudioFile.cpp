#include "realtone/AudioFile.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <system_error>

#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#include "dr_flac.h"
#include "dr_mp3.h"
#include "dr_wav.h"

namespace realtone {

namespace {

bool readWholeFile(const std::filesystem::path& path, std::uintmax_t maxBytes, std::vector<std::uint8_t>& out,
                   std::string& error)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
    {
        error = "File not found: " + path.string();
        return false;
    }
    if (!std::filesystem::is_regular_file(path, ec))
    {
        error = "Not a regular file: " + path.string();
        return false;
    }
    const auto size = std::filesystem::file_size(path, ec);
    if (ec)
    {
        error = "Cannot read file size: " + path.string();
        return false;
    }
    if (size == 0)
    {
        error = "File is empty: " + path.string();
        return false;
    }
    if (size > maxBytes)
    {
        error = "File is too large to import (" + std::to_string(size / (1024 * 1024)) + " MB): " + path.string();
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        error = "Cannot open file: " + path.string();
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(size));
    if (static_cast<std::uintmax_t>(in.gcount()) != size)
    {
        error = "Short read: " + path.string();
        return false;
    }
    return true;
}

void deinterleave(const float* src, std::uint64_t frames, unsigned channels, AudioClip& clip)
{
    clip.channels.assign(channels, std::vector<float>(static_cast<std::size_t>(frames)));
    for (std::uint64_t i = 0; i < frames; ++i)
        for (unsigned c = 0; c < channels; ++c)
            clip.channels[c][static_cast<std::size_t>(i)] = src[i * channels + c];
}

bool isRiffWave(const std::uint8_t* d, std::size_t n)
{
    return n >= 12 && std::memcmp(d, "RIFF", 4) == 0 && std::memcmp(d + 8, "WAVE", 4) == 0;
}

bool isFlac(const std::uint8_t* d, std::size_t n) { return n >= 4 && std::memcmp(d, "fLaC", 4) == 0; }

bool looksLikeMp3(const std::uint8_t* d, std::size_t n)
{
    if (n >= 3 && std::memcmp(d, "ID3", 3) == 0)
        return true;
    // Raw MPEG audio frame sync.
    return n >= 2 && d[0] == 0xFF && (d[1] & 0xE0) == 0xE0;
}

DecodeResult fail(std::string message)
{
    DecodeResult r;
    r.error = std::move(message);
    return r;
}

DecodeResult decodeWav(const std::uint8_t* data, std::size_t size, const DecodeOptions& options)
{
    drwav wav;
    if (!drwav_init_memory_with_metadata(&wav, data, size, 0, nullptr))
        return fail("The WAV file is damaged or uses an unsupported encoding.");

    DecodeResult result;
    const auto frames = wav.totalPCMFrameCount;
    if (wav.channels == 0 || wav.channels > 16 || wav.sampleRate == 0)
    {
        drwav_uninit(&wav);
        return fail("The WAV file reports an invalid channel count or sample rate.");
    }
    if (frames == 0 || static_cast<std::int64_t>(frames) > options.maxFrames)
    {
        drwav_uninit(&wav);
        return fail(frames == 0 ? "The WAV file contains no audio." : "The WAV file is longer than the supported limit.");
    }

    std::vector<float> interleaved(static_cast<std::size_t>(frames) * wav.channels);
    const auto read = drwav_read_pcm_frames_f32(&wav, frames, interleaved.data());
    if (read == 0)
    {
        drwav_uninit(&wav);
        return fail("The WAV file could not be decoded.");
    }

    deinterleave(interleaved.data(), read, wav.channels, result.clip);
    result.clip.sampleRate = wav.sampleRate;

    for (drwav_uint32 i = 0; i < wav.metadataCount; ++i)
    {
        const auto& md = wav.pMetadata[i];
        if (md.type == drwav_metadata_type_smpl && md.data.smpl.sampleLoopCount > 0 && md.data.smpl.pLoops != nullptr)
        {
            const auto& loop = md.data.smpl.pLoops[0];
            if (loop.lastSampleOffset > loop.firstSampleOffset && loop.lastSampleOffset < read)
            {
                result.clip.loop.valid = true;
                result.clip.loop.start = loop.firstSampleOffset;
                result.clip.loop.end = loop.lastSampleOffset;
            }
            break;
        }
    }

    drwav_uninit(&wav);
    result.ok = true;
    result.format = "wav";
    return result;
}

DecodeResult decodeFlac(const std::uint8_t* data, std::size_t size, const DecodeOptions& options)
{
    unsigned channels = 0, rate = 0;
    drflac_uint64 frames = 0;
    float* pcm = drflac_open_memory_and_read_pcm_frames_f32(data, size, &channels, &rate, &frames, nullptr);
    if (pcm == nullptr)
        return fail("The FLAC file is damaged or unsupported.");

    DecodeResult result;
    if (channels == 0 || channels > 16 || rate == 0 || frames == 0 || static_cast<std::int64_t>(frames) > options.maxFrames)
    {
        drflac_free(pcm, nullptr);
        return fail("The FLAC file has invalid or unsupported properties.");
    }
    deinterleave(pcm, frames, channels, result.clip);
    drflac_free(pcm, nullptr);
    result.clip.sampleRate = rate;
    result.ok = true;
    result.format = "flac";
    return result;
}

DecodeResult decodeMp3(const std::uint8_t* data, std::size_t size, const DecodeOptions& options)
{
    drmp3_config cfg{};
    drmp3_uint64 frames = 0;
    float* pcm = drmp3_open_memory_and_read_pcm_frames_f32(data, size, &cfg, &frames, nullptr);
    if (pcm == nullptr || frames == 0)
    {
        if (pcm != nullptr)
            drmp3_free(pcm, nullptr);
        return fail("The MP3 file is damaged or contains no decodable audio.");
    }

    DecodeResult result;
    if (cfg.channels == 0 || cfg.channels > 2 || cfg.sampleRate == 0 || static_cast<std::int64_t>(frames) > options.maxFrames)
    {
        drmp3_free(pcm, nullptr);
        return fail("The MP3 file has invalid or unsupported properties.");
    }
    deinterleave(pcm, frames, cfg.channels, result.clip);
    drmp3_free(pcm, nullptr);
    result.clip.sampleRate = cfg.sampleRate;
    result.ok = true;
    result.format = "mp3";
    return result;
}

void put16(std::vector<std::uint8_t>& v, std::uint16_t x)
{
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
}

void put32(std::vector<std::uint8_t>& v, std::uint32_t x)
{
    for (int i = 0; i < 4; ++i)
        v.push_back(static_cast<std::uint8_t>((x >> (8 * i)) & 0xFF));
}

void putTag(std::vector<std::uint8_t>& v, const char* tag) { v.insert(v.end(), tag, tag + 4); }

}  // namespace

DecodeResult decodeAudioMemory(const std::uint8_t* data, std::size_t size, const DecodeOptions& options)
{
    if (data == nullptr || size < 4)
        return fail("The file is too small to be an audio file.");
    if (isRiffWave(data, size))
        return decodeWav(data, size, options);
    if (isFlac(data, size))
        return decodeFlac(data, size, options);
    if (looksLikeMp3(data, size))
        return decodeMp3(data, size, options);
    return fail("Unrecognised audio format. Supported formats: WAV, FLAC, MP3.");
}

DecodeResult decodeAudioFile(const std::filesystem::path& path, const DecodeOptions& options)
{
    std::vector<std::uint8_t> bytes;
    std::string error;
    if (!readWholeFile(path, options.maxFileBytes, bytes, error))
        return fail(error);
    return decodeAudioMemory(bytes.data(), bytes.size(), options);
}

std::vector<std::uint8_t> encodeWav(const AudioClip& clip, const WavWriteOptions& options)
{
    const unsigned channels = static_cast<unsigned>(clip.numChannels());
    const std::uint64_t frames = static_cast<std::uint64_t>(clip.numFrames());
    const unsigned bytesPerSample = options.format == WavSampleFormat::Pcm16 ? 2 : options.format == WavSampleFormat::Pcm24 ? 3 : 4;
    const std::uint64_t dataBytes = frames * channels * bytesPerSample;

    std::vector<std::uint8_t> out;
    out.reserve(static_cast<std::size_t>(dataBytes) + 44);

    putTag(out, "RIFF");
    put32(out, static_cast<std::uint32_t>(36 + dataBytes));
    putTag(out, "WAVE");
    putTag(out, "fmt ");
    put32(out, 16);
    put16(out, options.format == WavSampleFormat::Float32 ? 3 : 1);
    put16(out, static_cast<std::uint16_t>(channels));
    put32(out, static_cast<std::uint32_t>(clip.sampleRate));
    put32(out, static_cast<std::uint32_t>(clip.sampleRate * channels * bytesPerSample));
    put16(out, static_cast<std::uint16_t>(channels * bytesPerSample));
    put16(out, static_cast<std::uint16_t>(bytesPerSample * 8));
    putTag(out, "data");
    put32(out, static_cast<std::uint32_t>(dataBytes));

    std::uint32_t rng = 0x12345678u;  // fixed seed: identical input gives identical bytes
    auto nextRand = [&rng]() {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>(rng >> 8) / 16777216.0f;
    };

    for (std::uint64_t i = 0; i < frames; ++i)
    {
        for (unsigned c = 0; c < channels; ++c)
        {
            float s = clip.channels[c][static_cast<std::size_t>(i)];
            if (!std::isfinite(s))
                s = 0.0f;

            if (options.format == WavSampleFormat::Float32)
            {
                std::uint32_t bits;
                std::memcpy(&bits, &s, 4);
                put32(out, bits);
            }
            else if (options.format == WavSampleFormat::Pcm24)
            {
                const float clamped = std::clamp(s, -1.0f, 1.0f);
                const auto v = static_cast<std::int32_t>(std::lrint(clamped * 8388607.0f));
                out.push_back(static_cast<std::uint8_t>(v & 0xFF));
                out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
                out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
            }
            else
            {
                float scaled = std::clamp(s, -1.0f, 1.0f) * 32767.0f;
                if (options.ditherPcm16)
                    scaled += nextRand() - nextRand();
                const auto v = static_cast<std::int32_t>(std::clamp(std::lrint(scaled), -32768L, 32767L));
                put16(out, static_cast<std::uint16_t>(v & 0xFFFF));
            }
        }
    }
    if (dataBytes % 2 != 0)
        out.push_back(0);
    return out;
}

bool writeWavFile(const std::filesystem::path& path, const AudioClip& clip, const WavWriteOptions& options,
                  std::string* error)
{
    auto setError = [&](const std::string& m) {
        if (error != nullptr)
            *error = m;
        return false;
    };

    if (clip.numChannels() < 1 || clip.numFrames() < 1 || clip.sampleRate <= 0.0)
        return setError("There is no audio to write.");

    std::error_code ec;
    if (!options.overwrite && std::filesystem::exists(path, ec))
        return setError("The destination file already exists and will not be replaced: " + path.string());

    const auto bytes = encodeWav(clip, options);
    auto tmp = path;
    tmp += ".part";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            return setError("Cannot create output file: " + tmp.string());
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out)
        {
            out.close();
            std::filesystem::remove(tmp, ec);
            return setError("Writing the output file failed (disk full?).");
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec)
    {
        // rename() onto an existing file fails on Windows, so replace explicitly when allowed.
        if (options.overwrite)
        {
            std::filesystem::remove(path, ec);
            ec.clear();
            std::filesystem::rename(tmp, path, ec);
        }
        if (ec)
        {
            std::filesystem::remove(tmp, ec);
            return setError("Could not move the finished file into place: " + path.string());
        }
    }
    return true;
}

}  // namespace realtone
