#pragma once

#include <string>

namespace realtone {

enum class Family
{
    Piano,
    ElectricKeys,
    Guitar,
    Bass,
    Bowed,
    Brass,
    Woodwind,
    Percussion,
    Mallet,
    Organ,
    Harp,
    Synth,
    Generic
};

// Performance behaviour that differs between instrument families. The UI and
// the preset system start from these defaults, so a piano does not get a pitch
// wheel that bends strings, and a violin gets vibrato and glide enabled.
struct InstrumentProfile
{
    Family family = Family::Generic;
    const char* label = "Generic";

    bool pitchBendByDefault = true;   // false: bend is a creative option, off until the user enables it
    float bendRangeSemitones = 2.0f;
    bool pedalSupported = false;

    float vibratoDepthCents = 0.0f;   // always-on vibrato depth
    float vibratoRateHz = 5.5f;
    float modWheelVibratoCents = 35.0f;  // extra depth at full CC1

    bool monoLegato = false;          // one voice at a time, with glide between overlapping notes
    float glideMs = 0.0f;

    float attackMs = 0.0f;
    float releaseScale = 1.0f;

    const char* articulationNotes = "";
};

const InstrumentProfile& profileFor(Family family);
Family familyFromString(const std::string& name);
const char* familyToString(Family family);

}  // namespace realtone
