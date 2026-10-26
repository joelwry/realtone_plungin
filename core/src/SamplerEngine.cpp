#include "realtone/SamplerEngine.h"

#include <algorithm>
#include <cmath>

namespace realtone {

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kExtraVoices = 16;   // headroom for voices fading out after being stolen
constexpr int kEndFadeFrames = 64;

inline float dbToLin(float db) { return std::pow(10.0f, db * 0.05f); }

double besselI0(double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 40; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}

}  // namespace

EngineParams paramsFromProfile(const InstrumentProfile& p)
{
    EngineParams e;
    e.pitchBendEnabled = p.pitchBendByDefault;
    e.pitchBendRange = p.bendRangeSemitones;
    e.vibratoDepthCents = p.vibratoDepthCents;
    e.vibratoRateHz = p.vibratoRateHz;
    e.modWheelVibratoCents = p.modWheelVibratoCents;
    e.monoLegato = p.monoLegato;
    e.glideMs = p.glideMs;
    e.attackMs = p.attackMs;
    e.releaseScale = p.releaseScale;
    e.sustainPedalEnabled = p.pedalSupported;
    return e;
}

SamplerEngine::SamplerEngine() { prepare(44100.0, 64); }

void SamplerEngine::buildSincTable()
{
    sincTable_.assign(static_cast<std::size_t>(kSincPhases + 1) * kSincTaps, 0.0f);
    constexpr double pi = 3.14159265358979323846;
    constexpr double cutoff = 0.95;
    constexpr double beta = 8.0;
    const double norm = 1.0 / besselI0(beta);
    for (int p = 0; p <= kSincPhases; ++p)
    {
        const double frac = static_cast<double>(p) / kSincPhases;
        double sum = 0.0;
        double taps[kSincTaps];
        for (int k = 0; k < kSincTaps; ++k)
        {
            const double x = (k - 3) - frac;  // distance from the interpolation point
            const double s = std::fabs(x) < 1e-12 ? 1.0 : std::sin(pi * cutoff * x) / (pi * cutoff * x);
            const double r = x / 4.0;
            const double w = std::fabs(r) >= 1.0 ? 0.0 : besselI0(beta * std::sqrt(1.0 - r * r)) * norm;
            taps[k] = s * w;
            sum += taps[k];
        }
        for (int k = 0; k < kSincTaps; ++k)
            sincTable_[static_cast<std::size_t>(p) * kSincTaps + k] = static_cast<float>(taps[k] / sum);
    }
}

void SamplerEngine::prepare(double sampleRate, int maxVoices)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxVoices = std::clamp(maxVoices, 1, 512);
    voices_.assign(static_cast<std::size_t>(maxVoices + kExtraVoices), Voice{});
    polyphonyLimit_ = maxVoices;
    if (sincTable_.empty())
        buildSincTable();
    reset();
}

void SamplerEngine::setInstrument(const Instrument* instrument)
{
    reset();
    instrument_ = instrument;
}

void SamplerEngine::setParams(const EngineParams& p)
{
    params_ = p;
    params_.polyphony = std::clamp(p.polyphony, 1, static_cast<int>(voices_.size()) - kExtraVoices);
    polyphonyLimit_ = params_.polyphony;
}

void SamplerEngine::reset()
{
    for (auto& v : voices_)
        v = Voice{};
    noteVelocity_.fill(0);
    seqCounter_.fill(0);
    keyDown_.fill(false);
    heldCount_ = 0;
    monoStackSize_ = 0;
    bendTarget_ = 0.0f;
    bendCurrent_ = 0.0f;
    modWheel_ = 0.0f;
    expressionCc_ = 1.0f;
    volumeCc_ = 1.0f;
    pedalDown_ = false;
    vibratoPhase_ = 0.0f;
    vibratoDepthCurrent_ = 0.0f;
    levelCurrent_ = -1.0f;  // negative: snap to target on the first chunk
}

int SamplerEngine::activeVoices() const
{
    int n = 0;
    for (const auto& v : voices_)
        if (v.stage != Stage::Idle)
            ++n;
    return n;
}

float SamplerEngine::nextRandom()
{
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_ >> 8) * (1.0f / 16777216.0f);
}

void SamplerEngine::handleEvent(const EngineEvent& e)
{
    switch (e.type)
    {
        case EngineEvent::Type::NoteOn:
            if (e.data2 == 0)
                noteOff(e.data1);
            else
                noteOn(e.data1, e.data2);
            break;
        case EngineEvent::Type::NoteOff:
            noteOff(e.data1);
            break;
        case EngineEvent::Type::PitchBend:
            bendTarget_ = std::clamp((static_cast<float>(e.data2) - 8192.0f) / 8192.0f, -1.0f, 1.0f);
            break;
        case EngineEvent::Type::Controller:
            controller(e.data1, e.data2);
            break;
        case EngineEvent::Type::AllNotesOff:
            for (auto& v : voices_)
                if (v.stage != Stage::Idle && v.stage != Stage::Release && !v.releaseTrigger)
                    beginRelease(v, v.region->def.ampegRelease * params_.releaseScale);
            keyDown_.fill(false);
            heldCount_ = 0;
            monoStackSize_ = 0;
            break;
        case EngineEvent::Type::AllSoundOff:
            for (auto& v : voices_)
                if (v.stage != Stage::Idle)
                    beginRelease(v, 0.005f, true);
            keyDown_.fill(false);
            heldCount_ = 0;
            monoStackSize_ = 0;
            break;
    }
}

void SamplerEngine::controller(int number, int value)
{
    const float norm = std::clamp(value, 0, 127) / 127.0f;
    switch (number)
    {
        case 1: modWheel_ = norm; break;
        case 7: volumeCc_ = norm; break;
        case 11: expressionCc_ = norm; break;
        case 64:
        {
            const bool down = value >= 64;
            if (pedalDown_ && !down)
            {
                pedalDown_ = false;
                releaseAllHeld();
            }
            else
                pedalDown_ = down;
            break;
        }
        case 120: handleEvent({0, EngineEvent::Type::AllSoundOff, 0, 0}); break;
        case 123: handleEvent({0, EngineEvent::Type::AllNotesOff, 0, 0}); break;
        case 121:
            modWheel_ = 0.0f;
            expressionCc_ = 1.0f;
            bendTarget_ = 0.0f;
            if (pedalDown_)
            {
                pedalDown_ = false;
                releaseAllHeld();
            }
            break;
        default: break;
    }
}

SamplerEngine::Voice* SamplerEngine::allocateVoice()
{
    int live = 0;
    for (const auto& v : voices_)
        if (v.stage != Stage::Idle && !v.stolen)
            ++live;

    if (live >= polyphonyLimit_)
    {
        // Prefer a voice that is already dying, then pedal-held, then the oldest.
        Voice* victim = nullptr;
        auto better = [](const Voice* a, const Voice* b) {
            if (b == nullptr) return true;
            const int ra = a->stage == Stage::Release ? 0 : a->pedalHeld ? 1 : 2;
            const int rb = b->stage == Stage::Release ? 0 : b->pedalHeld ? 1 : 2;
            if (ra != rb) return ra < rb;
            if (ra == 0) return a->env < b->env;
            return a->order < b->order;
        };
        for (auto& v : voices_)
            if (v.stage != Stage::Idle && !v.stolen && better(&v, victim))
                victim = &v;
        if (victim != nullptr)
        {
            victim->stolen = true;
            victim->noteHeld = false;
            victim->pedalHeld = false;
            beginRelease(*victim, 0.004f, true);
        }
    }

    for (auto& v : voices_)
        if (v.stage == Stage::Idle)
            return &v;

    // Pool exhausted: recycle whichever voice is quietest.
    Voice* quietest = &voices_.front();
    for (auto& v : voices_)
        if (v.env < quietest->env)
            quietest = &v;
    *quietest = Voice{};
    return quietest;
}

void SamplerEngine::beginRelease(Voice& v, float seconds, bool fast)
{
    if (v.stage == Stage::Idle)
        return;
    const float minTime = fast ? 0.002f : 0.006f;
    const float t = std::max(seconds, minTime);
    v.releaseCoeff = std::exp(std::log(0.001f) / (t * static_cast<float>(sampleRate_)));
    v.stage = Stage::Release;
    v.noteHeld = false;
    v.pedalHeld = false;
    if (v.loopWhileHeld)
        v.looping = false;  // loop_sustain: play on through the sample's tail
}

void SamplerEngine::startVoice(const Region& region, int note, int velocity, bool releaseTrigger, double glideFrom)
{
    Voice* vp = allocateVoice();
    if (vp == nullptr)
        return;
    Voice& v = *vp;
    v = Voice{};

    const SfzRegion& d = region.def;
    const SampleData& s = *region.sample;

    v.region = &region;
    v.left = s.left.data();
    v.right = s.stereo() ? s.right.data() : nullptr;
    v.frames = s.frames;
    v.note = note;
    v.noteHeld = !releaseTrigger;
    v.releaseTrigger = releaseTrigger;
    v.oneShot = releaseTrigger || d.loopMode == SfzRegion::LoopMode::OneShot;
    v.order = ++orderCounter_;

    v.looping = region.looping;
    v.loopWhileHeld = region.loopWhileHeld;
    v.loopStartI = static_cast<std::int64_t>(region.loopStart);
    v.loopLen = static_cast<std::int64_t>(region.loopEnd) - v.loopStartI + 1;
    v.fastEnd = v.looping ? static_cast<std::int64_t>(region.loopEnd) - kSincTaps : v.frames + 8;

    v.pos = region.startFrame;
    v.endFrame = region.endFrame;
    v.sourceRatio = s.sampleRate / sampleRate_;
    v.baseSemis = (note - d.pitchKeycenter) * (d.pitchKeytrack / 100.0) + d.tuneCents / 100.0;
    v.glideSemis = glideFrom;
    v.ratio = v.sourceRatio * std::exp2(v.baseSemis / 12.0);

    // Velocity -> level. The quadratic curve matches the usual SFZ convention.
    const float vel = static_cast<float>(velocity) / 127.0f;
    const float expo = 2.0f * std::exp2(std::clamp(params_.velocityCurve, -1.0f, 1.0f));
    const float track = std::clamp(d.ampVeltrack / 100.0f * params_.velocitySensitivity, -1.0f, 1.0f);
    const float velGain = track >= 0.0f ? (1.0f - track) + track * std::pow(vel, expo)
                                        : (1.0f + track) + (-track) * std::pow(1.0f - vel, expo);
    v.gain = region.gain * velGain;
    v.panL = region.panLeft;
    v.panR = region.panRight;

    const float sr = static_cast<float>(sampleRate_);
    const float attack = d.ampegAttack + params_.attackMs * 0.001f;
    v.attackInc = 1.0f / std::max(48.0f, attack * sr);
    v.holdLeft = static_cast<int>(d.ampegHold * sr);
    v.sustainLevel = d.ampegSustain / 100.0f;
    v.decayCoeff = d.ampegDecay > 0.0f ? std::exp(std::log(0.001f) / (d.ampegDecay * sr)) : 0.0f;
    v.env = 0.0f;
    v.stage = Stage::Attack;
    v.releaseCoeff = std::exp(std::log(0.001f) / (std::max(0.006f, d.ampegRelease * params_.releaseScale) * sr));
}

void SamplerEngine::noteOn(int note, int velocity)
{
    if (instrument_ == nullptr || note < 0 || note > 127)
        return;

    if (params_.humanizeVelocity > 0.0f)
        velocity += static_cast<int>(std::lround((nextRandom() * 2.0f - 1.0f) * params_.humanizeVelocity * 12.0f));
    velocity = std::clamp(velocity, 1, 127);

    const bool anyHeld = heldCount_ > 0;
    if (!keyDown_[note])
    {
        keyDown_[note] = true;
        ++heldCount_;
    }
    noteVelocity_[note] = static_cast<std::uint8_t>(velocity);

    // Choke groups.
    const auto& keyRegions = instrument_->attackByKey[note];
    for (auto idx : keyRegions)
    {
        const auto& d = instrument_->regions[idx].def;
        if (d.group == 0)
            continue;
        for (auto& v : voices_)
            if (v.stage != Stage::Idle && v.stage != Stage::Release && v.region->def.offBy == d.group)
                beginRelease(v, 0.02f, true);
    }

    // Monophonic legato: the previous note hands over to the new one, optionally gliding.
    double glideFrom = 0.0;
    if (params_.monoLegato)
    {
        int prevNote = -1;
        std::uint64_t newest = 0;
        for (auto& v : voices_)
            if (v.stage != Stage::Idle && v.stage != Stage::Release && !v.releaseTrigger && v.order >= newest)
            {
                newest = v.order;
                prevNote = v.note;
            }
        if (prevNote >= 0 && params_.glideMs > 0.0f)
            glideFrom = static_cast<double>(prevNote - note);
        for (auto& v : voices_)
            if (v.stage != Stage::Idle && v.stage != Stage::Release && !v.releaseTrigger)
                beginRelease(v, 0.04f, true);

        if (monoStackSize_ < static_cast<int>(monoStack_.size()))
            monoStack_[static_cast<std::size_t>(monoStackSize_++)] = static_cast<std::uint8_t>(note);
    }

    const float rnd = nextRandom();
    const unsigned seq = seqCounter_[note];
    bool usedSequence = false;
    for (auto idx : keyRegions)
    {
        const Region& r = instrument_->regions[idx];
        const SfzRegion& d = r.def;
        if (velocity < d.lovel || velocity > d.hivel)
            continue;
        if (rnd < d.lorand || rnd >= d.hirand)
            continue;
        if (d.trigger == SfzRegion::Trigger::First && anyHeld)
            continue;
        if (d.trigger == SfzRegion::Trigger::Legato && !anyHeld)
            continue;
        if (d.seqLength > 1)
        {
            usedSequence = true;
            if (static_cast<int>(seq % static_cast<unsigned>(d.seqLength)) + 1 != d.seqPosition)
                continue;
        }
        startVoice(r, note, velocity, false, glideFrom);
    }
    if (usedSequence)
        ++seqCounter_[note];
}

void SamplerEngine::releaseNoteVoices(int note, bool respectPedal)
{
    const bool pedalHolds = respectPedal && pedalDown_ && params_.sustainPedalEnabled;
    for (auto& v : voices_)
    {
        if (v.stage == Stage::Idle || v.stage == Stage::Release || v.releaseTrigger || v.note != note || !v.noteHeld)
            continue;
        if (v.oneShot)
        {
            v.noteHeld = false;
            continue;
        }
        if (pedalHolds)
        {
            v.noteHeld = false;
            v.pedalHeld = true;
        }
        else
            beginRelease(v, v.region->def.ampegRelease * params_.releaseScale);
    }
}

void SamplerEngine::releaseAllHeld()
{
    for (auto& v : voices_)
        if (v.stage != Stage::Idle && v.stage != Stage::Release && v.pedalHeld)
            beginRelease(v, v.region->def.ampegRelease * params_.releaseScale);
}

void SamplerEngine::noteOff(int note)
{
    if (instrument_ == nullptr || note < 0 || note > 127)
        return;
    if (keyDown_[note])
    {
        keyDown_[note] = false;
        --heldCount_;
    }

    // Mono stack bookkeeping: if the sounding note is released while others are
    // still held, return to the most recent one.
    bool retriggerPrevious = false;
    int previous = -1;
    if (params_.monoLegato)
    {
        const bool wasTop = monoStackSize_ > 0 && monoStack_[static_cast<std::size_t>(monoStackSize_ - 1)] == note;
        int w = 0;
        for (int i = 0; i < monoStackSize_; ++i)
            if (monoStack_[static_cast<std::size_t>(i)] != note)
                monoStack_[static_cast<std::size_t>(w++)] = monoStack_[static_cast<std::size_t>(i)];
        monoStackSize_ = w;
        if (wasTop && monoStackSize_ > 0)
        {
            retriggerPrevious = true;
            previous = monoStack_[static_cast<std::size_t>(monoStackSize_ - 1)];
        }
    }

    if (retriggerPrevious)
    {
        // Hand back to the earlier held note instead of releasing into silence.
        for (auto& v : voices_)
            if (v.stage != Stage::Idle && v.stage != Stage::Release && !v.releaseTrigger && v.note == note)
                beginRelease(v, 0.04f, true);
        const double glideFrom = params_.glideMs > 0.0f ? static_cast<double>(note - previous) : 0.0;
        const int vel = std::max<int>(1, noteVelocity_[previous]);
        const auto& keyRegions = instrument_->attackByKey[previous];
        for (auto idx : keyRegions)
        {
            const Region& r = instrument_->regions[idx];
            if (vel >= r.def.lovel && vel <= r.def.hivel && r.def.seqLength == 1 && r.def.lorand == 0.0f)
                startVoice(r, previous, vel, false, glideFrom);
        }
    }
    else
        releaseNoteVoices(note, true);

    // Release-trigger regions (key noise, piano release samples).
    const int vel = std::max<int>(1, noteVelocity_[note]);
    const float rnd = nextRandom();
    for (auto idx : instrument_->releaseByKey[note])
    {
        const Region& r = instrument_->regions[idx];
        if (vel < r.def.lovel || vel > r.def.hivel || rnd < r.def.lorand || rnd >= r.def.hirand)
            continue;
        startVoice(r, note, vel, true, 0.0);
    }
}

void SamplerEngine::process(const EngineEvent* events, int numEvents, float* left, float* right, int numFrames)
{
    int pos = 0, ev = 0;
    while (pos < numFrames)
    {
        while (ev < numEvents && events[ev].offset <= pos)
            handleEvent(events[ev++]);
        int next = numFrames;
        if (ev < numEvents)
            next = std::min(next, std::max(events[ev].offset, pos + 1));
        const int len = std::min(next - pos, kChunk);
        renderChunk(left + pos, right + pos, len);
        pos += len;
    }
    // Anything scheduled past the end of the block still has to be delivered.
    while (ev < numEvents)
        handleEvent(events[ev++]);
}

void SamplerEngine::renderChunk(float* l, float* r, int n)
{
    std::fill(l, l + n, 0.0f);
    std::fill(r, r + n, 0.0f);

    const float sr = static_cast<float>(sampleRate_);
    const float chunkSec = static_cast<float>(n) / sr;

    // Pitch: bend, vibrato and tuning are shared by every voice.
    const float bendSemisTarget = params_.pitchBendEnabled
                                      ? (bendTarget_ + std::clamp(params_.extraBendSemitones, -1.0f, 1.0f)) * params_.pitchBendRange
                                      : 0.0f;
    const float bendSmooth = 1.0f - std::exp(-chunkSec / 0.004f);
    bendCurrent_ += (bendSemisTarget - bendCurrent_) * bendSmooth;

    const float depthTarget = (params_.vibratoDepthCents + modWheel_ * params_.modWheelVibratoCents) * 0.01f;
    vibratoDepthCurrent_ += (depthTarget - vibratoDepthCurrent_) * (1.0f - std::exp(-chunkSec / 0.05f));
    vibratoPhase_ += params_.vibratoRateHz * chunkSec;
    vibratoPhase_ -= std::floor(vibratoPhase_);
    const float vibrato = vibratoDepthCurrent_ * std::sin(kTwoPi * vibratoPhase_);

    const double globalSemis = static_cast<double>(bendCurrent_) + vibrato + params_.fineTuneCents * 0.01;
    const double glideDecay = params_.glideMs > 0.0f ? std::exp(-static_cast<double>(chunkSec) / (params_.glideMs * 0.001 / 3.0)) : 0.0;

    for (auto& v : voices_)
    {
        if (v.stage == Stage::Idle)
            continue;
        v.glideSemis *= glideDecay;
        if (std::fabs(v.glideSemis) < 1e-4)
            v.glideSemis = 0.0;
        const double newRatio = v.sourceRatio * std::exp2((v.baseSemis + v.glideSemis + globalSemis) / 12.0);
        switch (params_.quality)
        {
            case Quality::Performance: renderVoiceT<Quality::Performance>(v, l, r, n, newRatio); break;
            case Quality::Balanced: renderVoiceT<Quality::Balanced>(v, l, r, n, newRatio); break;
            case Quality::High: renderVoiceT<Quality::High>(v, l, r, n, newRatio); break;
        }
    }

    // Output stage: level ramp, stereo width, sanitising.
    const float levelTarget = dbToLin(params_.masterGainDb) * volumeCc_ * volumeCc_ * params_.expression * expressionCc_;
    if (levelCurrent_ < 0.0f)
        levelCurrent_ = levelTarget;
    const float levelStep = (levelTarget - levelCurrent_) / static_cast<float>(n);
    const float width = std::clamp(params_.stereoWidth, 0.0f, 2.0f);
    float level = levelCurrent_;
    for (int i = 0; i < n; ++i)
    {
        level += levelStep;
        float a = l[i] * level, b = r[i] * level;
        if (width != 1.0f)
        {
            const float mid = 0.5f * (a + b), side = 0.5f * (a - b) * width;
            a = mid + side;
            b = mid - side;
        }
        if (!std::isfinite(a)) a = 0.0f;
        if (!std::isfinite(b)) b = 0.0f;
        l[i] = std::clamp(a, -16.0f, 16.0f);
        r[i] = std::clamp(b, -16.0f, 16.0f);
    }
    levelCurrent_ = levelTarget;
}

float SamplerEngine::sampleAt(const Voice& v, const float* a, std::int64_t idx)
{
    if (v.looping && idx > v.loopStartI + v.loopLen - 1)
        idx = v.loopStartI + (idx - v.loopStartI) % v.loopLen;
    if (idx < 0)
        idx = 0;
    if (idx >= v.frames)
        return 0.0f;
    return a[idx];
}

template <Quality Q>
inline void SamplerEngine::fetch(const Voice& v, double pos, float& outL, float& outR) const
{
    const std::int64_t i0 = static_cast<std::int64_t>(pos);
    const float f = static_cast<float>(pos - static_cast<double>(i0));
    const bool fast = i0 >= 3 && i0 < v.fastEnd - 4;
    const bool stereo = v.right != nullptr;

    auto interp = [&](const float* a) -> float {
        if constexpr (Q == Quality::Performance)
        {
            const float x0 = fast ? a[i0] : sampleAt(v, a, i0);
            const float x1 = fast ? a[i0 + 1] : sampleAt(v, a, i0 + 1);
            return x0 + f * (x1 - x0);
        }
        else if constexpr (Q == Quality::Balanced)
        {
            float xm, x0, x1, x2;
            if (fast)
            {
                xm = a[i0 - 1]; x0 = a[i0]; x1 = a[i0 + 1]; x2 = a[i0 + 2];
            }
            else
            {
                xm = sampleAt(v, a, i0 - 1); x0 = sampleAt(v, a, i0); x1 = sampleAt(v, a, i0 + 1); x2 = sampleAt(v, a, i0 + 2);
            }
            const float c0 = x0;
            const float c1 = 0.5f * (x1 - xm);
            const float c2 = xm - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + c0;
        }
        else
        {
            const float* taps = &sincTable_[static_cast<std::size_t>(f * kSincPhases + 0.5f) * kSincTaps];
            float acc = 0.0f;
            if (fast)
            {
                const float* p = a + i0 - 3;
                for (int k = 0; k < kSincTaps; ++k)
                    acc += p[k] * taps[k];
            }
            else
            {
                for (int k = 0; k < kSincTaps; ++k)
                    acc += sampleAt(v, a, i0 - 3 + k) * taps[k];
            }
            return acc;
        }
    };

    outL = interp(v.left);
    outR = stereo ? interp(v.right) : outL;
}

template <Quality Q>
void SamplerEngine::renderVoiceT(Voice& v, float* l, float* r, int n, double newRatio)
{
    double pos = v.pos;
    double ratio = v.ratio;
    const double ratioInc = (newRatio - ratio) / n;
    const bool stereo = v.right != nullptr;
    const bool looping = v.looping;

    for (int i = 0; i < n; ++i)
    {
        float env = v.env;
        switch (v.stage)
        {
            case Stage::Attack:
                env += v.attackInc;
                if (env >= 1.0f)
                {
                    env = 1.0f;
                    v.stage = v.holdLeft > 0 ? Stage::Hold : (v.decayCoeff > 0.0f && v.sustainLevel < 1.0f ? Stage::Decay : Stage::Sustain);
                }
                break;
            case Stage::Hold:
                if (--v.holdLeft <= 0)
                    v.stage = (v.decayCoeff > 0.0f && v.sustainLevel < 1.0f) ? Stage::Decay : Stage::Sustain;
                break;
            case Stage::Decay:
                env = v.sustainLevel + (env - v.sustainLevel) * v.decayCoeff;
                if (env - v.sustainLevel < 1e-4f)
                {
                    env = v.sustainLevel;
                    v.stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                env = v.sustainLevel;
                break;
            case Stage::Release:
                env *= v.releaseCoeff;
                if (env < 3e-5f)
                {
                    v.stage = Stage::Idle;
                    v.env = 0.0f;
                    return;
                }
                break;
            case Stage::Idle:
                return;
        }
        v.env = env;

        float sl, sr;
        fetch<Q>(v, pos, sl, sr);

        float e = env * v.gain;
        if (!looping)
        {
            const double remaining = v.endFrame - pos;
            if (remaining < kEndFadeFrames)
                e *= static_cast<float>(std::max(0.0, remaining)) * (1.0f / kEndFadeFrames);
        }
        if (stereo)
        {
            l[i] += sl * e * v.panL;
            r[i] += sr * e * v.panR;
        }
        else
        {
            l[i] += sl * e * v.panL;
            r[i] += sl * e * v.panR;
        }

        pos += ratio;
        ratio += ratioInc;

        const bool loopNow = looping && !(v.loopWhileHeld && v.stage == Stage::Release);
        if (loopNow)
        {
            const double loopEnd = static_cast<double>(v.loopStartI + v.loopLen - 1);
            if (pos > loopEnd)
                pos -= static_cast<double>(v.loopLen);
        }
        else if (pos >= v.endFrame)
        {
            v.stage = Stage::Idle;
            v.env = 0.0f;
            return;
        }
    }
    v.pos = pos;
    v.ratio = newRatio;
}

}  // namespace realtone
