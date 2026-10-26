#include <cstdint>
#include "realtone/Renderer.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <set>

namespace realtone {

namespace {

double linToDb(double x) { return x <= 1e-10 ? -200.0 : 20.0 * std::log10(x); }

}  // namespace

RenderResult renderSequence(std::shared_ptr<const Instrument> instrument, const Sequence& sequence,
                            const RenderSettings& settings, const ProgressFn& progress,
                            const std::atomic<bool>* cancel)
{
    RenderResult result;
    auto report = [&](float f, const char* s) {
        if (progress)
            progress(f, s);
    };
    auto cancelled = [&]() { return cancel != nullptr && cancel->load(); };

    if (!instrument || instrument->regions.empty())
    {
        result.error = "No instrument is loaded, so there is nothing to render with.";
        return result;
    }
    if (settings.sampleRate < 8000.0 || settings.sampleRate > 384000.0)
    {
        result.error = "The output sample rate is outside the supported range.";
        return result;
    }
    if (settings.tempoScale <= 0.0 || settings.tempoScale > 16.0)
    {
        result.error = "The tempo scale must be between 0 and 16.";
        return result;
    }

    // 1. Select, transpose and humanise.
    const std::set<int> enabled(settings.enabledTracks.begin(), settings.enabledTracks.end());
    std::mt19937 rng(settings.seed);
    std::uniform_real_distribution<double> jitter(-1.0, 1.0);

    struct Pending
    {
        double time;
        SequenceEvent::Kind kind;
        int d1, d2;
    };
    std::vector<Pending> events;
    events.reserve(sequence.events.size());
    std::map<int, std::vector<std::pair<double, int>>> activeNotes;  // key (ch,src note) -> (shift, rendered note)

    for (const auto& e : sequence.events)
    {
        if (!enabled.empty() && enabled.count(e.track) == 0)
        {
            if (e.kind == SequenceEvent::Kind::NoteOn)
                ++result.notesSkipped;
            continue;
        }
        const double t = e.time / settings.tempoScale;
        switch (e.kind)
        {
            case SequenceEvent::Kind::NoteOn:
            {
                const int note = e.data1 + settings.transposeSemitones;
                const int key = e.track * 4096 + e.channel * 128 + e.data1;
                if (note < 0 || note > 127)
                {
                    activeNotes[key].push_back({0.0, -1});
                    ++result.notesSkipped;
                    break;
                }
                const double shift = settings.timingHumanizeMs > 0.0 ? jitter(rng) * settings.timingHumanizeMs * 0.001 : 0.0;
                activeNotes[key].push_back({shift, note});
                const int vel = std::clamp(static_cast<int>(std::lround(e.data2 * settings.velocityScale)) + settings.velocityOffset, 1, 127);
                events.push_back({std::max(0.0, t + shift), SequenceEvent::Kind::NoteOn, note, vel});
                ++result.notesRendered;
                break;
            }
            case SequenceEvent::Kind::NoteOff:
            {
                const int key = e.track * 4096 + e.channel * 128 + e.data1;
                auto it = activeNotes.find(key);
                if (it == activeNotes.end() || it->second.empty())
                    break;
                const auto [shift, note] = it->second.front();
                it->second.erase(it->second.begin());
                if (note >= 0)
                    events.push_back({std::max(0.0, t + shift), SequenceEvent::Kind::NoteOff, note, 0});
                break;
            }
            case SequenceEvent::Kind::Controller:
                if (settings.includeControllers)
                    events.push_back({t, e.kind, e.data1, e.data2});
                break;
            case SequenceEvent::Kind::PitchBend:
                if (settings.includePitchBend)
                    events.push_back({t, e.kind, 0, e.data2});
                break;
        }
    }
    if (result.notesRendered == 0)
    {
        result.error = "No notes are left to render. Check the selected tracks and the transpose setting.";
        return result;
    }
    std::stable_sort(events.begin(), events.end(), [](const Pending& a, const Pending& b) {
        if (a.time != b.time) return a.time < b.time;
        return static_cast<int>(a.kind) < static_cast<int>(b.kind);
    });

    // Anything still sounding at the end gets a note-off so tails decay naturally.
    const double lastTime = events.back().time;
    const double closeTime = lastTime;
    std::set<int> sounding;
    for (const auto& e : events)
    {
        if (e.kind == SequenceEvent::Kind::NoteOn) sounding.insert(e.d1);
        if (e.kind == SequenceEvent::Kind::NoteOff) sounding.erase(e.d1);
    }
    for (int n : sounding)
        events.push_back({closeTime, SequenceEvent::Kind::NoteOff, n, 0});
    events.push_back({closeTime, SequenceEvent::Kind::Controller, 64, 0});

    // 2. Render.
    SamplerEngine engine;
    engine.prepare(settings.sampleRate, settings.maxVoices);
    EngineParams params = settings.engine;
    params.polyphony = settings.maxVoices;
    engine.setParams(params);
    engine.setInstrument(instrument.get());
    engine.seedRandom(settings.seed);

    const int block = std::clamp(settings.blockSize, 32, 8192);
    const auto sr = settings.sampleRate;
    const std::int64_t eventEnd = static_cast<std::int64_t>(std::ceil(closeTime * sr));
    const std::int64_t maxTotal = eventEnd + static_cast<std::int64_t>(settings.maxTailSeconds * sr);
    const std::int64_t silenceHold = static_cast<std::int64_t>(settings.silenceHoldSeconds * sr);
    const float silenceLin = std::pow(10.0f, static_cast<float>(settings.silenceThresholdDb) / 20.0f);

    std::vector<float> outL, outR;
    outL.reserve(static_cast<std::size_t>(eventEnd + sr * 2));
    outR.reserve(outL.capacity());
    std::vector<float> bl(static_cast<std::size_t>(block)), br(static_cast<std::size_t>(block));
    std::vector<EngineEvent> blockEvents;

    std::size_t next = 0;
    std::int64_t pos = 0;
    std::int64_t quietRun = 0;
    float peak = 0.0f;
    report(0.0f, "Rendering");

    while (pos < maxTotal)
    {
        if (cancelled())
        {
            result.cancelled = true;
            result.error = "Rendering was cancelled.";
            return result;
        }
        const int n = static_cast<int>(std::min<std::int64_t>(block, maxTotal - pos));
        blockEvents.clear();
        while (next < events.size())
        {
            const std::int64_t at = static_cast<std::int64_t>(std::llround(events[next].time * sr));
            if (at >= pos + n)
                break;
            EngineEvent ee;
            ee.offset = static_cast<int>(std::max<std::int64_t>(0, at - pos));
            switch (events[next].kind)
            {
                case SequenceEvent::Kind::NoteOn: ee.type = EngineEvent::Type::NoteOn; ee.data1 = events[next].d1; ee.data2 = events[next].d2; break;
                case SequenceEvent::Kind::NoteOff: ee.type = EngineEvent::Type::NoteOff; ee.data1 = events[next].d1; break;
                case SequenceEvent::Kind::Controller: ee.type = EngineEvent::Type::Controller; ee.data1 = events[next].d1; ee.data2 = events[next].d2; break;
                case SequenceEvent::Kind::PitchBend: ee.type = EngineEvent::Type::PitchBend; ee.data2 = events[next].d2; break;
            }
            blockEvents.push_back(ee);
            ++next;
        }

        engine.process(blockEvents.data(), static_cast<int>(blockEvents.size()), bl.data(), br.data(), n);
        result.peakVoices = std::max(result.peakVoices, engine.activeVoices());

        float blockPeak = 0.0f;
        for (int i = 0; i < n; ++i)
            blockPeak = std::max({blockPeak, std::fabs(bl[static_cast<std::size_t>(i)]), std::fabs(br[static_cast<std::size_t>(i)])});
        peak = std::max(peak, blockPeak);
        outL.insert(outL.end(), bl.begin(), bl.begin() + n);
        outR.insert(outR.end(), br.begin(), br.begin() + n);
        pos += n;

        if (pos >= eventEnd && next >= events.size())
        {
            quietRun = blockPeak < silenceLin ? quietRun + n : 0;
            if (quietRun >= silenceHold || (engine.activeVoices() == 0 && blockPeak < silenceLin))
                break;
        }
        if ((pos / block) % 64 == 0)
            report(std::min(0.95f, static_cast<float>(pos) / static_cast<float>(std::max<std::int64_t>(1, eventEnd))), "Rendering");
    }

    // Trim trailing silence below the threshold, leaving a short pad.
    std::int64_t keep = static_cast<std::int64_t>(outL.size());
    const std::int64_t pad = static_cast<std::int64_t>(0.05 * sr);
    while (keep > eventEnd && std::fabs(outL[static_cast<std::size_t>(keep - 1)]) < silenceLin && std::fabs(outR[static_cast<std::size_t>(keep - 1)]) < silenceLin)
        --keep;
    keep = std::min<std::int64_t>(static_cast<std::int64_t>(outL.size()), std::max(keep + pad, eventEnd));
    outL.resize(static_cast<std::size_t>(keep));
    outR.resize(static_cast<std::size_t>(keep));

    if (peak < 1e-6f)
    {
        result.error = "The render produced silence. The instrument may have no samples for the notes in this file.";
        return result;
    }

    // 3. Level management.
    result.peakBeforeGainDb = linToDb(peak);
    double gainDb = 0.0;
    switch (settings.normalize)
    {
        case NormalizeMode::None:
            if (peak > 1.0f)
                result.warnings.push_back("The rendered level exceeds full scale (" + std::to_string(result.peakBeforeGainDb) + " dBFS) and will clip in integer WAV formats.");
            break;
        case NormalizeMode::LimitPeak:
            gainDb = std::min(0.0, settings.targetPeakDb - result.peakBeforeGainDb);
            break;
        case NormalizeMode::NormalizePeak:
            gainDb = settings.targetPeakDb - result.peakBeforeGainDb;
            break;
    }
    result.appliedGainDb = gainDb;
    const float g = static_cast<float>(std::pow(10.0, gainDb / 20.0));
    if (g != 1.0f)
    {
        for (auto& s : outL) s *= g;
        for (auto& s : outR) s *= g;
    }

    result.clip.sampleRate = sr;
    result.clip.channels.push_back(std::move(outL));
    result.clip.channels.push_back(std::move(outR));
    report(1.0f, "Done");
    result.ok = true;
    return result;
}

}  // namespace realtone
