#pragma once

#include <juce_core/juce_core.h>

namespace realtone::studio
{

// Per-user data lives under the user profile, never next to the program, so installs need no
// write access and uninstalling the program leaves projects, presets and libraries alone.
juce::File userDataDirectory();
juce::File defaultLibraryDirectory();
juce::File userPresetDirectory();
juce::File logDirectory();

// Append-only diagnostic log (rotated at ~512 KB). Safe to call from any thread except the
// audio callback.
void logMessage(const juce::String& message);
juce::File logFile();

}  // namespace realtone::studio
