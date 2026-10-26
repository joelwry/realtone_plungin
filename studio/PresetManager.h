#pragma once

#include "realtone/InstrumentProfile.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace realtone::studio
{

struct PresetInfo
{
    juce::String name;
    bool factory = false;
    realtone::Family family = realtone::Family::Generic;  // factory presets only
    juce::File file;                                      // user presets only
};

// Factory presets are built in; user presets are small XML files in the user data folder.
// A preset stores the performance parameters only. The instrument is chosen separately, so a
// preset made for one violin can be tried on another.
class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& state);

    // Presets that make sense for an instrument family, factory first, then the user's own.
    std::vector<PresetInfo> listFor(realtone::Family family) const;

    bool load(const PresetInfo& preset, realtone::Family family, juce::String& error);
    bool saveUser(const juce::String& name, juce::String& error);
    bool deleteUser(const PresetInfo& preset, juce::String& error);

    static juce::File userDirectory();
    static bool isValidName(const juce::String& name);

    // Used by tests and by the UI.
    juce::ValueTree captureParameters() const;
    void applyParameters(const juce::ValueTree& values);

private:
    juce::AudioProcessorValueTreeState& state_;
};

}  // namespace realtone::studio
