#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <memory>

namespace realtone::studio
{

// User settings shared by the standalone application and every plugin instance.
// Stored as XML in the user data folder; reads and writes are serialised.
class Settings
{
public:
    static Settings& get();

    juce::String getString(const juce::String& key, const juce::String& fallback = {});
    int getInt(const juce::String& key, int fallback = 0);
    double getDouble(const juce::String& key, double fallback = 0.0);
    bool getBool(const juce::String& key, bool fallback = false);

    void setString(const juce::String& key, const juce::String& value);
    void setInt(const juce::String& key, int value);
    void setDouble(const juce::String& key, double value);
    void setBool(const juce::String& key, bool value);

    // Ordered list stored under one key (e.g. recent files).
    juce::StringArray getList(const juce::String& key);
    void setList(const juce::String& key, const juce::StringArray& values);

    juce::File libraryRoot();
    void setLibraryRoot(const juce::File& dir);
    int sampleMemoryLimitMB();

    // For tests: point the store at a different file.
    void useFile(const juce::File& file);

private:
    Settings();
    void load();
    void save();

    juce::CriticalSection lock_;
    juce::File file_;
    juce::ValueTree tree_;
};

namespace keys
{
inline const char* const libraryRoot = "libraryRoot";
inline const char* const sampleMemoryMB = "sampleMemoryMB";
inline const char* const exportSampleRate = "exportSampleRate";
inline const char* const exportBitDepth = "exportBitDepth";
inline const char* const exportNormalize = "exportNormalize";
inline const char* const lastMidiFolder = "lastMidiFolder";
inline const char* const lastAudioFolder = "lastAudioFolder";
inline const char* const lastExportFolder = "lastExportFolder";
inline const char* const lastInstrument = "lastInstrument";
inline const char* const windowState = "standaloneWindow";
inline const char* const externalInstruments = "externalInstruments";
inline const char* const recentProjects = "recentProjects";
}  // namespace keys

}  // namespace realtone::studio
