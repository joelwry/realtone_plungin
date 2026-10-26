#include "realtone/Instrument.h"

#include "realtone/AudioFile.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace realtone {

namespace {
constexpr double kHalfPi = 1.57079632679489661923;
}

bool Instrument::hasReleaseRegions() const
{
    return std::any_of(regions.begin(), regions.end(),
                       [](const Region& r) { return r.def.trigger == SfzRegion::Trigger::Release; });
}

void Instrument::buildIndex()
{
    for (auto& v : attackByKey)
        v.clear();
    for (auto& v : releaseByKey)
        v.clear();

    for (std::uint32_t i = 0; i < regions.size(); ++i)
    {
        const auto& d = regions[i].def;
        auto& target = d.trigger == SfzRegion::Trigger::Release ? releaseByKey : attackByKey;
        for (int k = d.lokey; k <= d.hikey; ++k)
            target[k].push_back(i);
    }
}

std::size_t Instrument::memoryBytes() const
{
    std::set<const SampleData*> seen;
    std::size_t total = 0;
    for (const auto& r : regions)
        if (r.sample && seen.insert(r.sample.get()).second)
            total += r.sample->bytes();
    return total;
}

LoadResult loadInstrument(const std::filesystem::path& sfzPath, const std::filesystem::path& rootDir,
                          const LoadOptions& options)
{
    LoadResult result;
    auto report = [&](float f, const std::string& s) {
        if (options.progress)
            options.progress(f, s);
    };
    auto cancelled = [&]() { return options.cancel != nullptr && options.cancel->load(); };

    report(0.0f, "Reading instrument definition");
    auto parsed = parseSfzFile(sfzPath, rootDir);
    if (!parsed.ok)
    {
        result.error = parsed.error;
        return result;
    }

    auto inst = std::make_shared<Instrument>();
    inst->name = sfzPath.stem().string();
    inst->report.warnings = parsed.warnings;
    inst->report.unsupportedOpcodes.assign(parsed.unsupportedOpcodes.begin(), parsed.unsupportedOpcodes.end());
    inst->report.regionsDeclared = parsed.regions.size();

    // Unique sample files, in first-use order.
    std::vector<std::string> files;
    std::map<std::string, std::size_t> fileIndex;
    for (const auto& r : parsed.regions)
        if (fileIndex.emplace(r.sample, files.size()).second)
            files.push_back(r.sample);

    struct Slot
    {
        std::shared_ptr<SampleData> data;
        std::string error;
        bool missing = false;
    };
    std::vector<Slot> slots(files.size());

    std::atomic<std::size_t> next{0};
    std::atomic<std::size_t> done{0};
    std::atomic<std::uint64_t> loadedBytes{0};
    std::atomic<bool> overBudget{false};
    std::mutex progressMutex;

    const auto baseDir = sfzPath.parent_path();
    auto worker = [&]() {
        for (;;)
        {
            const auto i = next.fetch_add(1);
            if (i >= files.size() || cancelled() || overBudget.load())
                return;

            Slot& slot = slots[i];
            const auto path = baseDir / files[i];
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
            {
                slot.missing = true;
            }
            else
            {
                auto decoded = decodeAudioFile(path);
                if (!decoded.ok)
                    slot.error = decoded.error;
                else
                {
                    auto data = std::make_shared<SampleData>();
                    data->sampleRate = decoded.clip.sampleRate;
                    data->frames = decoded.clip.numFrames();
                    data->loop = decoded.clip.loop;
                    data->left = std::move(decoded.clip.channels[0]);
                    if (decoded.clip.numChannels() > 1)
                        data->right = std::move(decoded.clip.channels[1]);
                    // Zero padding lets the interpolator read past the end without bounds checks.
                    data->left.resize(data->left.size() + 16, 0.0f);
                    if (!data->right.empty())
                        data->right.resize(data->right.size() + 16, 0.0f);
                    const auto total = loadedBytes.fetch_add(data->bytes()) + data->bytes();
                    if (total > options.maxBytes)
                        overBudget = true;
                    slot.data = std::move(data);
                }
            }
            const auto finished = done.fetch_add(1) + 1;
            std::lock_guard<std::mutex> lock(progressMutex);
            report(0.05f + 0.9f * static_cast<float>(finished) / static_cast<float>(files.size()),
                   "Loading samples (" + std::to_string(finished) + " of " + std::to_string(files.size()) + ")");
        }
    };

    int threads = options.threads > 0 ? options.threads : static_cast<int>(std::thread::hardware_concurrency());
    threads = std::clamp(threads, 1, 8);
    threads = std::min<int>(threads, static_cast<int>(std::max<std::size_t>(1, files.size())));
    std::vector<std::thread> pool;
    for (int t = 1; t < threads; ++t)
        pool.emplace_back(worker);
    worker();
    for (auto& t : pool)
        t.join();

    if (cancelled())
    {
        result.cancelled = true;
        result.error = "Loading was cancelled.";
        return result;
    }
    if (overBudget.load())
    {
        result.error = "This instrument needs more memory than the configured sample memory limit (" +
                       std::to_string(options.maxBytes / (1024 * 1024)) + " MB). Raise the limit in Settings.";
        return result;
    }

    for (std::size_t i = 0; i < files.size(); ++i)
    {
        if (slots[i].missing)
            inst->report.missingSamples.push_back(files[i]);
        else if (!slots[i].error.empty())
            inst->report.failedSamples.push_back(files[i] + ": " + slots[i].error);
    }

    for (const auto& def : parsed.regions)
    {
        const auto& slot = slots[fileIndex[def.sample]];
        if (!slot.data)
            continue;

        Region r;
        r.def = def;
        r.sample = slot.data;
        r.gain = std::pow(10.0f, def.volumeDb / 20.0f);
        const double theta = (std::clamp(def.pan, -100.0f, 100.0f) + 100.0) / 200.0 * kHalfPi;
        r.panLeft = static_cast<float>(std::cos(theta) * std::sqrt(2.0));
        r.panRight = static_cast<float>(std::sin(theta) * std::sqrt(2.0));
        if (slot.data->stereo())
        {
            // Stereo files keep their image; pan acts as balance.
            const float p = std::clamp(def.pan / 100.0f, -1.0f, 1.0f);
            r.panLeft = p > 0 ? 1.0f - p : 1.0f;
            r.panRight = p < 0 ? 1.0f + p : 1.0f;
        }

        const double frames = static_cast<double>(slot.data->frames);
        r.startFrame = std::clamp(def.offset, 0.0, std::max(0.0, frames - 1.0));
        r.endFrame = def.end >= 0.0 ? std::clamp(def.end + 1.0, r.startFrame + 1.0, frames) : frames;

        using LM = SfzRegion::LoopMode;
        bool wantLoop = false;
        switch (def.loopMode)
        {
            case LM::LoopContinuous: wantLoop = true; break;
            case LM::LoopSustain: wantLoop = true; r.loopWhileHeld = true; break;
            case LM::Default: wantLoop = slot.data->loop.valid; break;
            default: break;
        }
        if (wantLoop)
        {
            double ls = def.loopStart, le = def.loopEnd;
            if (ls < 0.0 && slot.data->loop.valid) ls = static_cast<double>(slot.data->loop.start);
            if (le < 0.0 && slot.data->loop.valid) le = static_cast<double>(slot.data->loop.end);
            if (ls >= 0.0 && le > ls && le < frames)
            {
                r.looping = true;
                r.loopStart = ls;
                r.loopEnd = le;
            }
        }
        inst->regions.push_back(std::move(r));
    }

    inst->report.regionsPlayable = inst->regions.size();
    inst->report.bytesLoaded = loadedBytes.load();

    if (inst->regions.empty())
    {
        result.error = "None of the instrument's samples could be loaded (" +
                       std::to_string(inst->report.missingSamples.size()) + " missing, " +
                       std::to_string(inst->report.failedSamples.size()) + " unreadable).";
        result.instrument = inst;  // keep the report so the UI can show details
        return result;
    }
    if (!options.allowPartial && !inst->report.complete())
    {
        result.error = "The instrument is incomplete: " + std::to_string(inst->report.missingSamples.size()) +
                       " sample files are missing.";
        result.instrument = inst;
        return result;
    }

    inst->buildIndex();
    report(1.0f, "Ready");
    result.ok = true;
    result.instrument = std::move(inst);
    return result;
}

}  // namespace realtone
