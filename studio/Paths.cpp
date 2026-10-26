#include "Paths.h"

namespace realtone::studio
{

juce::File userDataDirectory()
{
    juce::File base;
#if JUCE_WINDOWS
    // Local, not roaming: libraries are large and machine specific.
    const auto local = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", {});
    base = local.isNotEmpty() ? juce::File(local) : juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#else
    base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#endif
    return base.getChildFile("RealToneStudio");
}

juce::File defaultLibraryDirectory() { return userDataDirectory().getChildFile("Libraries"); }
juce::File userPresetDirectory() { return userDataDirectory().getChildFile("Presets"); }
juce::File logDirectory() { return userDataDirectory().getChildFile("Logs"); }
juce::File logFile() { return logDirectory().getChildFile("realtone.log"); }

void logMessage(const juce::String& message)
{
    static juce::CriticalSection lock;
    const juce::ScopedLock sl(lock);

    const auto file = logFile();
    file.getParentDirectory().createDirectory();
    if (file.getSize() > 512 * 1024)
    {
        const auto old = file.withFileExtension("old.log");
        old.deleteFile();
        file.moveFileTo(old);
    }
    file.appendText(juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S  ") + message + "\n");
}

}  // namespace realtone::studio
