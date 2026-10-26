#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "realtone/InstrumentProfile.h"
#include "realtone/SamplerEngine.h"

namespace realtone::studio::param
{

// IDs are part of the saved-state and automation contract: never rename or reuse one.
inline constexpr const char* level = "level";
inline constexpr const char* expression = "expression";
inline constexpr const char* modulation = "modulation";
inline constexpr const char* pitchBend = "pitchbend";
inline constexpr const char* bendEnable = "bendenable";
inline constexpr const char* bendRange = "bendrange";
inline constexpr const char* vibratoDepth = "vibdepth";
inline constexpr const char* vibratoRate = "vibrate";
inline constexpr const char* glide = "glide";
inline constexpr const char* legato = "legato";
inline constexpr const char* attack = "attack";
inline constexpr const char* release = "release";
inline constexpr const char* velocitySens = "velsens";
inline constexpr const char* velocityCurve = "velcurve";
inline constexpr const char* humanize = "humanize";
inline constexpr const char* width = "width";
inline constexpr const char* tune = "tune";
inline constexpr const char* quality = "quality";
inline constexpr const char* polyphony = "polyphony";
inline constexpr const char* sustain = "sustain";
inline constexpr const char* pedalEnable = "pedalenable";

// Selectable pitch-bend ranges in semitones; the choice parameter stores the index.
inline constexpr float kBendRanges[] = {1.0f, 2.0f, 3.0f, 5.0f, 7.0f, 12.0f, 24.0f};
inline constexpr int kNumBendRanges = 7;
inline constexpr int kPolyphonyChoices[] = {8, 16, 32, 64, 128};

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

int bendRangeIndexFor(float semitones);

// Current parameter values as engine settings. Reads atomics only; safe on the audio thread.
realtone::EngineParams readEngineParams(juce::AudioProcessorValueTreeState& apvts);

// Moves the performance parameters to the defaults that suit an instrument family.
void applyProfileDefaults(juce::AudioProcessorValueTreeState& apvts, const realtone::InstrumentProfile& profile);

}  // namespace realtone::studio::param
