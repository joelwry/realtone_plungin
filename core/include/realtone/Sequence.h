#pragma once

#include <string>
#include <vector>

namespace realtone {

// A time-ordered list of performance events. Both MIDI files and analysed audio
// are turned into this before rendering, so the renderer has a single input.
struct SequenceEvent
{
    enum class Kind { NoteOff, Controller, PitchBend, NoteOn };  // order matters: at equal times offs sort first

    double time = 0.0;  // seconds from the start
    Kind kind = Kind::NoteOn;
    int channel = 0;
    int data1 = 0;      // note or controller number
    int data2 = 0;      // velocity, controller value, or 14-bit bend (0..16383)
    int track = 0;
};

struct TrackInfo
{
    int index = 0;
    std::string name;
    int noteCount = 0;
    int firstChannel = -1;
    int program = -1;
    int lowestNote = 127;
    int highestNote = 0;
    bool looksLikePercussion = false;  // all notes on MIDI channel 10
};

struct Sequence
{
    std::vector<SequenceEvent> events;  // sorted
    std::vector<TrackInfo> tracks;
    std::vector<std::string> warnings;
    double durationSeconds = 0.0;       // time of the last event
    double initialTempoBpm = 120.0;
    int format = 0;
    int noteCount = 0;

    void sortEvents();
};

}  // namespace realtone
