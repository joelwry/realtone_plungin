#include "realtone/InstrumentProfile.h"

#include <algorithm>
#include <cctype>

namespace realtone {

namespace {

InstrumentProfile make(Family f, const char* label)
{
    InstrumentProfile p;
    p.family = f;
    p.label = label;
    return p;
}

InstrumentProfile makeProfile(Family f)
{
    switch (f)
    {
        case Family::Piano:
        {
            auto p = make(f, "Piano");
            p.pitchBendByDefault = false;  // conventional pianos do not bend; enabling it is a creative choice
            p.pedalSupported = true;
            p.articulationNotes = "Sustain pedal (CC64) is honoured. Pitch bend is off by default and available as a creative effect.";
            return p;
        }
        case Family::ElectricKeys:
        {
            auto p = make(f, "Electric keys");
            p.pedalSupported = true;
            p.modWheelVibratoCents = 20.0f;
            return p;
        }
        case Family::Guitar:
        {
            auto p = make(f, "Guitar");
            p.bendRangeSemitones = 2.0f;
            p.vibratoRateHz = 5.0f;
            p.articulationNotes = "String-style bends, mod wheel vibrato.";
            return p;
        }
        case Family::Bass:
        {
            auto p = make(f, "Bass");
            p.bendRangeSemitones = 2.0f;
            return p;
        }
        case Family::Bowed:
        {
            auto p = make(f, "Bowed strings");
            p.bendRangeSemitones = 2.0f;
            p.vibratoDepthCents = 12.0f;
            p.vibratoRateHz = 5.8f;
            p.modWheelVibratoCents = 40.0f;
            p.monoLegato = false;
            p.glideMs = 60.0f;
            p.attackMs = 0.0f;
            p.articulationNotes = "Vibrato from the mod wheel, optional portamento between overlapping notes.";
            return p;
        }
        case Family::Brass:
        {
            auto p = make(f, "Brass");
            p.bendRangeSemitones = 2.0f;
            p.vibratoRateHz = 5.2f;
            p.modWheelVibratoCents = 45.0f;
            p.glideMs = 40.0f;
            p.articulationNotes = "Expression (CC11) shapes the breath dynamics; mod wheel adds vibrato.";
            return p;
        }
        case Family::Woodwind:
        {
            auto p = make(f, "Woodwind");
            p.bendRangeSemitones = 2.0f;
            p.vibratoRateHz = 5.4f;
            p.modWheelVibratoCents = 30.0f;
            p.glideMs = 40.0f;
            return p;
        }
        case Family::Percussion:
        {
            auto p = make(f, "Percussion");
            p.pitchBendByDefault = false;
            return p;
        }
        case Family::Mallet:
        {
            auto p = make(f, "Mallet and bell");
            p.pitchBendByDefault = false;
            return p;
        }
        case Family::Organ:
        {
            auto p = make(f, "Organ");
            p.bendRangeSemitones = 2.0f;
            p.modWheelVibratoCents = 15.0f;
            p.vibratoRateHz = 6.5f;
            return p;
        }
        case Family::Harp:
        {
            auto p = make(f, "Harp");
            p.pitchBendByDefault = false;
            p.pedalSupported = true;
            return p;
        }
        case Family::Synth:
        {
            auto p = make(f, "Synth");
            p.bendRangeSemitones = 2.0f;
            p.glideMs = 80.0f;
            return p;
        }
        case Family::Generic:
            break;
    }
    return make(Family::Generic, "Generic");
}

}  // namespace

const InstrumentProfile& profileFor(Family family)
{
    static const InstrumentProfile profiles[] = {
        makeProfile(Family::Piano),     makeProfile(Family::ElectricKeys), makeProfile(Family::Guitar),
        makeProfile(Family::Bass),      makeProfile(Family::Bowed),        makeProfile(Family::Brass),
        makeProfile(Family::Woodwind),  makeProfile(Family::Percussion),   makeProfile(Family::Mallet),
        makeProfile(Family::Organ),     makeProfile(Family::Harp),         makeProfile(Family::Synth),
        makeProfile(Family::Generic)};
    return profiles[static_cast<int>(family)];
}

const char* familyToString(Family f)
{
    switch (f)
    {
        case Family::Piano: return "piano";
        case Family::ElectricKeys: return "electric-keys";
        case Family::Guitar: return "guitar";
        case Family::Bass: return "bass";
        case Family::Bowed: return "bowed";
        case Family::Brass: return "brass";
        case Family::Woodwind: return "woodwind";
        case Family::Percussion: return "percussion";
        case Family::Mallet: return "mallet";
        case Family::Organ: return "organ";
        case Family::Harp: return "harp";
        case Family::Synth: return "synth";
        case Family::Generic: return "generic";
    }
    return "generic";
}

Family familyFromString(const std::string& in)
{
    std::string s = in;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (int i = 0; i <= static_cast<int>(Family::Generic); ++i)
        if (s == familyToString(static_cast<Family>(i)))
            return static_cast<Family>(i);
    return Family::Generic;
}

}  // namespace realtone
