#pragma once

#include "ExportOptions.h"
#include "JobBar.h"
#include "NoteRoll.h"
#include "ScrollPage.h"
#include "WaveformView.h"

#include "../studio/AppContext.h"
#include "../studio/OfflineWorkflows.h"

namespace realtone::ui
{

// Standalone page: turn a clean single-voice WAV or MP3 recording into the chosen instrument.
// The analysis is monophonic; the page says so, shows what was found, and lets the user fix mistakes.
class AudioConvertPanel : public ScrollPage, private juce::Timer
{
public:
    explicit AudioConvertPanel(realtone::studio::AppContext& context);
    ~AudioConvertPanel() override;

    void loadFile(const juce::File& file);
    juce::ValueTree state() const;
    void setState(const juce::ValueTree& v);

    bool hasAudio() const { return source_ != nullptr; }
    std::vector<realtone::DetectedNote>& notes() { return notes_; }
    // Used by tests and the project loader.
    void convertTo(const juce::File& destination, bool allowOverwrite);
    bool isBusy() const { return task_.isRunning(); }
    juce::String lastResult() const { return resultText_; }
    juce::String bannerText() const { return banner_.getText(); }

private:
    void timerCallback() override;
    int doLayout(int width);
    void chooseFile();
    void startAnalysis();
    void startPreview();
    void chooseDestination();
    void updateBanner();
    void updateInfo();
    void editSelected(int semitones, bool toggleMute);
    void setResult(const juce::String& text, juce::Colour colour);
    realtone::SequenceFromNotesOptions sequenceOptions(const realtone::InstrumentProfile& profile, bool& bendsUsed) const;
    realtone::RenderSettings makeSettings(double sampleRate, const realtone::SequenceFromNotesOptions& so, bool bends) const;

    realtone::studio::AppContext& ctx_;
    realtone::studio::BackgroundTask task_{"RealTone audio conversion"};
    std::unique_ptr<juce::FileChooser> chooser_;

    juce::File file_;
    std::shared_ptr<realtone::AudioClip> source_;
    juce::String sourceInfo_;
    std::vector<realtone::DetectedNote> notes_;
    realtone::AnalysisReport report_;
    bool analysed_ = false;
    juce::String resultText_;
    juce::File lastExport_;

    juce::TextButton open_{"Open audio file..."}, analyse_{"Find notes again"}, preview_{"Preview"}, stop_{"Stop"},
        convert_{"Convert and save WAV..."}, reveal_{"Show in folder"};
    juce::TextButton mute_{"Mute / restore"}, down12_{"-12"}, down1_{"-1"}, up1_{"+1"}, up12_{"+12"};
    juce::Label fileLabel_, instrument_, hint_, sensitivityL_{{}, "Detection"}, bendRangeL_{{}, "Bend range"}, transposeL_{{}, "Transpose (semitones)"};
    juce::TextEditor banner_;
    juce::ComboBox sensitivity_, bendRange_;
    juce::Slider transpose_;
    juce::ToggleButton preserveBends_{"Keep vibrato and pitch slides"};
    juce::Label sectionSource_{{}, "SOURCE"}, sectionNotes_{{}, "DETECTED NOTES"}, sectionOutput_{{}, "OUTPUT"};
    WaveformView waveform_;
    NoteRoll roll_;
    ExportOptionsBox export_;
    JobBar jobBar_{task_};
};

}  // namespace realtone::ui
