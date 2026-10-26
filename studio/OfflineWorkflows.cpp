#include "OfflineWorkflows.h"

#include "Paths.h"

namespace realtone::studio
{

RenderOutcome renderWithInstrument(InstrumentManager& manager, const realtone::InstrumentEntry& instrument,
                                   const realtone::Sequence& sequence, const realtone::RenderSettings& settings, BackgroundTask& task)
{
    RenderOutcome out;
    task.report(0.0f, "Loading " + juce::String(instrument.name));
    std::string error;
    auto inst = manager.acquire(instrument, error,
                                [&task](float f, const std::string& what) { task.report(0.3f * f, juce::String(what)); }, task.cancelFlag());
    if (task.cancelRequested())
    {
        out.cancelled = true;
        out.error = "Cancelled.";
        return out;
    }
    if (inst == nullptr)
    {
        out.error = error.empty() ? "The instrument could not be loaded." : juce::String(error);
        return out;
    }
    if (!inst->report.complete())
        out.warnings.push_back(std::to_string(inst->report.missingSamples.size() + inst->report.failedSamples.size()) +
                               " sample file(s) of this instrument are missing; notes that use them are silent. Reinstall it from the Library page.");

    auto result = realtone::renderSequence(inst, sequence, settings, [&task](float f, const std::string&) { task.report(0.3f + 0.7f * f, "Rendering"); },
                                           task.cancelFlag());
    out.cancelled = result.cancelled;
    if (!result.ok)
    {
        out.error = juce::String(result.error);
        return out;
    }
    out.ok = true;
    out.clip = std::make_shared<realtone::AudioClip>(std::move(result.clip));
    out.warnings.insert(out.warnings.end(), result.warnings.begin(), result.warnings.end());
    out.peakDb = result.peakBeforeGainDb;
    out.gainDb = result.appliedGainDb;
    out.notes = result.notesRendered;
    out.skipped = result.notesSkipped;
    return out;
}

ImportOutcome importAudio(const juce::File& file, BackgroundTask& task)
{
    ImportOutcome out;
    task.report(0.0f, "Reading " + file.getFileName());
    auto decoded = realtone::decodeAudioFile(std::filesystem::path(file.getFullPathName().toStdString()));
    if (!decoded.ok)
    {
        out.error = juce::String(decoded.error);
        logMessage("Import failed for " + file.getFullPathName() + ": " + out.error);
        return out;
    }
    out.format = juce::String(decoded.format);
    out.clip = std::make_shared<realtone::AudioClip>(std::move(decoded.clip));
    out.ok = true;
    return out;
}

AnalysisOutcome analyseAudio(const realtone::AudioClip& clip, const realtone::AnalysisOptions& options, BackgroundTask& task)
{
    AnalysisOutcome out;
    out.report = realtone::analyseMonophonic(clip, options, [&task](float f) { task.report(f, "Finding notes"); }, task.cancelFlag());
    out.ok = out.report.ok;
    out.error = juce::String(out.report.error);
    return out;
}

ExportOutcome exportWav(const realtone::AudioClip& clip, const juce::File& destination, realtone::WavSampleFormat format, bool allowOverwrite,
                        const juce::File& protectedSource)
{
    ExportOutcome out;
    if (protectedSource != juce::File() && destination.getFullPathName().equalsIgnoreCase(protectedSource.getFullPathName()))
    {
        out.error = "That would overwrite the source file you imported. Choose a different name; the original is never changed.";
        return out;
    }
    if (!destination.getParentDirectory().isDirectory())
    {
        out.error = "The destination folder does not exist: " + destination.getParentDirectory().getFullPathName();
        return out;
    }
    realtone::WavWriteOptions o;
    o.format = format;
    o.overwrite = allowOverwrite;
    std::string err;
    if (!realtone::writeWavFile(std::filesystem::path(destination.getFullPathName().toStdString()), clip, o, &err))
    {
        out.error = juce::String(err);
        logMessage("Export failed: " + out.error);
        return out;
    }
    out.ok = true;
    return out;
}

juce::File suggestOutputFile(const juce::File& folder, const juce::String& stem, const juce::String& instrumentName)
{
    auto clean = [](juce::String s) { return s.replaceCharacters("\\/:*?\"<>|", "_________").trim(); };
    const auto base = clean(stem) + " (" + clean(instrumentName) + ")";
    auto f = folder.getChildFile(base + ".wav");
    for (int i = 2; f.exists() && i < 1000; ++i)
        f = folder.getChildFile(base + " " + juce::String(i) + ".wav");
    return f;
}

}  // namespace realtone::studio
