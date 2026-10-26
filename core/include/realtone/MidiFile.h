#pragma once

#include "realtone/Sequence.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace realtone {

struct MidiParseResult
{
    bool ok = false;
    std::string error;
    Sequence sequence;
};

// Standard MIDI File formats 0 and 1, PPQ and SMPTE time division, tempo changes,
// note on/off, pitch bend, and the controllers the engine understands
// (1, 7, 11, 64, 120, 121, 123). Format 2 (independent sequences) is rejected.
MidiParseResult parseMidiMemory(const std::uint8_t* data, std::size_t size);
MidiParseResult parseMidiFile(const std::filesystem::path& path);

}  // namespace realtone
