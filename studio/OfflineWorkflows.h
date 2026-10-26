#pragma once

#include "BackgroundTask.h"
#include "InstrumentManager.h"

#include "realtone/AudioFile.h"
#include "realtone/PitchAnalysis.h"
#include "realtone/Renderer.h"

#include <juce_core/juce_core.h>

#include <memory>

namespace realtone::studio
{

// The jobs behind the standalone's MIDI and audio pages. Each function blocks, so call it from a
// BackgroundTask; they report progress through the task and stop early when it is cancelled.

struct RenderOutcome
{
    bool ok = false;
    bool cancelled = false;
    juce::String error;
    std::shared_ptr<realtone::AudioClip> clip;
    std::vector<std::string> warnings;
    double peakDb = -200.0;
    double gainDb = 0.0;
    int notes = 0;
    int skipped = 0;
};

RenderOutcome renderWithInstrument(InstrumentManager& manager, const realtone::InstrumentEntry& instrument,
                                   const realtone::Sequence& sequence, const realtone::RenderSettings& settings, BackgroundTask& task);

struct ImportOutcome
{
    bool ok = false;
    juce::String error;
    std::shared_ptr<realtone::AudioClip> clip;
    juce::String format;
};

ImportOutcome importAudio(const juce::File& file, BackgroundTask& task);

struct AnalysisOutcome
{
    bool ok = false;
    juce::String error;
    realtone::AnalysisReport report;
};

AnalysisOutcome analyseAudio(const realtone::AudioClip& clip, const realtone::AnalysisOptions& options, BackgroundTask& task);

struct ExportOutcome
{
    bool ok = false;
    juce::String error;
};

// Writes a WAV. Refuses to touch `protectedSource` (the imported file) and, unless `allowOverwrite`
// is set, refuses to replace anything that already exists.
ExportOutcome exportWav(const realtone::AudioClip& clip, const juce::File& destination, realtone::WavSampleFormat format,
                        bool allowOverwrite, const juce::File& protectedSource);

// File name suggestion that cannot collide with the source: "<stem> (<instrument>).wav", numbered if taken.
juce::File suggestOutputFile(const juce::File& folder, const juce::String& stem, const juce::String& instrumentName);

}  // namespace realtone::studio
