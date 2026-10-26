#pragma once

#include "realtone/Instrument.h"
#include "realtone/InstrumentProfile.h"

#include <array>
#include <cstdint>
#include <vector>

namespace realtone {

enum class Quality
{
    Performance,  // linear interpolation
    Balanced,     // 4-point Hermite
    High          // 8-point windowed sinc
};

// Everything the player can change. Plain data so a plugin can fill it from its
// parameter set at the top of each block and hand it over without locking.
struct EngineParams
{
    float masterGainDb = 0.0f;
    float pitchBendRange = 2.0f;       // semitones, applied to both directions
    bool pitchBendEnabled = true;
    float extraBendSemitones = 0.0f;   // host-automatable bend that adds to MIDI pitch bend, -1..1 of range
    float fineTuneCents = 0.0f;
    float vibratoDepthCents = 0.0f;
    float vibratoRateHz = 5.5f;
    float modWheelVibratoCents = 35.0f;
    bool monoLegato = false;
    float glideMs = 0.0f;
    float attackMs = 0.0f;             // added to each region's own attack
    float releaseScale = 1.0f;
    float expression = 1.0f;           // 0..1, multiplied with CC11
    float velocitySensitivity = 1.0f;  // 0 = fixed level, 1 = full dynamic range
    float velocityCurve = 0.0f;        // -1 (softer response) .. 1 (harder response)
    bool sustainPedalEnabled = true;
    float humanizeVelocity = 0.0f;     // 0..1
    float stereoWidth = 1.0f;          // 0 = mono, 1 = as recorded, 2 = widened
    Quality quality = Quality::High;
    int polyphony = 64;
};

EngineParams paramsFromProfile(const InstrumentProfile& profile);

struct EngineEvent
{
    enum class Type : std::uint8_t { NoteOn, NoteOff, PitchBend, Controller, AllNotesOff, AllSoundOff };

    int offset = 0;  // frame within the block
    Type type = Type::NoteOn;
    int data1 = 0;   // note / controller number
    int data2 = 0;   // velocity / controller value / 14-bit bend (0..16383, centre 8192)
};

// Sample playback engine. prepare() allocates; everything else is allocation-free
// and lock-free so it can be called from an audio callback. One instance is
// not thread safe: all calls come from the same thread.
class SamplerEngine
{
public:
    SamplerEngine();

    void prepare(double sampleRate, int maxVoices = 64);
    void setInstrument(const Instrument* instrument);  // stops all sound
    const Instrument* instrument() const { return instrument_; }
    void setParams(const EngineParams& params);
    const EngineParams& params() const { return params_; }

    void handleEvent(const EngineEvent& e);

    // Renders `numFrames` into l/r (overwritten). `events` must be sorted by offset.
    void process(const EngineEvent* events, int numEvents, float* left, float* right, int numFrames);

    void reset();  // silence immediately and restore controller defaults

    int activeVoices() const;
    float pitchBendSemitones() const { return bendCurrent_; }
    double sampleRate() const { return sampleRate_; }
    // Deterministic humanisation / round-robin randomness for offline renders.
    void seedRandom(std::uint32_t seed) { rng_ = seed != 0 ? seed : 1u; }

private:
    enum class Stage : std::uint8_t { Idle, Attack, Hold, Decay, Sustain, Release };

    struct Voice
    {
        Stage stage = Stage::Idle;
        const Region* region = nullptr;
        const float* left = nullptr;
        const float* right = nullptr;
        std::int64_t frames = 0;
        std::int64_t fastEnd = 0;    // interpolation may read freely below this frame index
        std::int64_t loopStartI = 0;
        std::int64_t loopLen = 0;
        bool looping = false;
        bool loopWhileHeld = false;

        int note = 0;
        bool noteHeld = false;       // key still down
        bool pedalHeld = false;      // released by the player but kept alive by the sustain pedal
        bool releaseTrigger = false;
        bool oneShot = false;
        bool stolen = false;
        std::uint64_t order = 0;

        double pos = 0.0;
        double endFrame = 0.0;
        double sourceRatio = 1.0;    // sample rate ratio
        double baseSemis = 0.0;
        double glideSemis = 0.0;
        double ratio = 1.0;          // last per-chunk playback ratio

        float gain = 1.0f;
        float panL = 1.0f, panR = 1.0f;

        float env = 0.0f;
        float attackInc = 1.0f;
        int holdLeft = 0;
        float decayCoeff = 1.0f;
        float sustainLevel = 1.0f;
        float releaseCoeff = 0.999f;
    };

    static constexpr int kChunk = 32;
    static constexpr int kSincTaps = 8;
    static constexpr int kSincPhases = 1024;

    void noteOn(int note, int velocity);
    void noteOff(int note);
    void controller(int number, int value);
    void startVoice(const Region& region, int note, int velocity, bool releaseTrigger, double glideFrom);
    Voice* allocateVoice();
    void beginRelease(Voice& v, float seconds, bool fast = false);
    void releaseNoteVoices(int note, bool respectPedal);
    void releaseAllHeld();
    void renderChunk(float* l, float* r, int n);
    template <Quality Q>
    void renderVoiceT(Voice& v, float* l, float* r, int n, double newRatio);
    float nextRandom();
    void buildSincTable();

    template <Quality Q>
    void fetch(const Voice& v, double pos, float& outL, float& outR) const;
    static float sampleAt(const Voice& v, const float* a, std::int64_t idx);

    double sampleRate_ = 44100.0;
    EngineParams params_;
    const Instrument* instrument_ = nullptr;

    std::vector<Voice> voices_;
    int polyphonyLimit_ = 64;
    std::uint64_t orderCounter_ = 0;
    std::uint32_t rng_ = 0x9e3779b9u;

    std::array<std::uint8_t, 128> noteVelocity_{};
    std::array<std::uint16_t, 128> seqCounter_{};
    std::array<bool, 128> keyDown_{};
    int heldCount_ = 0;
    std::array<std::uint8_t, 16> monoStack_{};
    int monoStackSize_ = 0;

    // Controllers
    float bendTarget_ = 0.0f;   // semitones
    float bendCurrent_ = 0.0f;
    float modWheel_ = 0.0f;     // 0..1
    float expressionCc_ = 1.0f; // CC11 as 0..1
    float volumeCc_ = 1.0f;     // CC7 as 0..1
    bool pedalDown_ = false;

    float vibratoPhase_ = 0.0f;
    float vibratoDepthCurrent_ = 0.0f;
    float levelCurrent_ = 1.0f;

    std::vector<float> sincTable_;
    std::vector<float> scratchL_, scratchR_;
};

}  // namespace realtone
