#pragma once

#include <juce_core/juce_core.h>

#include <filesystem>

namespace realtone::studio
{

// std::filesystem::path and juce::String meet at every file the core touches. On Windows a plain
// narrow string is read in the ANSI code page, which breaks profile folders such as C:\Users\Zoë, so
// always go through these.
inline std::filesystem::path toPath(const juce::String& s)
{
#if JUCE_WINDOWS
    return std::filesystem::path(std::wstring(s.toWideCharPointer()));
#else
    return std::filesystem::path(s.toStdString());
#endif
}

inline std::filesystem::path toPath(const juce::File& f) { return toPath(f.getFullPathName()); }

inline juce::String toJuceString(const std::filesystem::path& p)
{
#if JUCE_WINDOWS
    return juce::String(p.wstring().c_str());
#else
    return juce::String::fromUTF8(p.string().c_str());
#endif
}

inline juce::String fromStd(const std::string& utf8) { return juce::String::fromUTF8(utf8.c_str(), static_cast<int>(utf8.size())); }

inline juce::File toJuceFile(const std::filesystem::path& p) { return juce::File(toJuceString(p)); }

}  // namespace realtone::studio
