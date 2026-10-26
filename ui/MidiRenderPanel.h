#pragma once

#include "ExportOptions.h"
#include "JobBar.h"
#include "ScrollPage.h"
#include "WaveformView.h"

#include "../studio/AppContext.h"
#include "../studio/OfflineWorkflows.h"

#include "realtone/MidiFile.h"

namespace realtone::ui
{

// Standalone page: load a MIDI file, pick tracks, render it through the chosen instrument and
// export a WAV that can be dropped into Audacity (or any editor).
class MidiRenderPanel : public ScrollPage, private juce::ListBoxModel, private juce::Timer
{
public:
    explicit MidiRenderPanel(realtone::studio::AppContext& context);
    ~MidiRenderPanel() override;

    void loadFile(const juce::File& file);
    juce::ValueTree state() const;
    void setState(const juce::ValueTree& v);

    bool hasMidi() const { return loaded_; }
    juce::File midiFile() const { return file_; }
    // Used by tests and by the project loader.
    void renderTo(const juce::File& destination, bool allowOverwrite);
    bool isBusy() const { return task_.isRunning(); }
    juce::String lastResult() const { return resultText_; }

private:
    struct Row
    {
        realtone::TrackInfo info;
        bool enabled = true;
    };

    int getNumRows() override { return static_cast<int>(rows_.size()); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void timerCallback() override;

    int doLayout(int width);
    void chooseFile();
    void chooseDestination();
    void startPreview();
    void updateSummary();
    void updateInstrumentLine();
    realtone::RenderSettings makeSettings(double sampleRate) const;
    void setResult(const juce::String& text, juce::Colour colour);

    realtone::studio::AppContext& ctx_;
    realtone::studio::BackgroundTask task_{"RealTone MIDI render"};
    std::unique_ptr<juce::FileChooser> chooser_;

    bool loaded_ = false;
    juce::File file_;
    realtone::Sequence sequence_;
    std::vector<Row> rows_;
    juce::String resultText_;
    juce::File lastExport_;

    juce::TextButton open_{"Open MIDI file..."}, preview_{"Preview"}, stop_{"Stop"}, render_{"Render to WAV..."}, reveal_{"Show in folder"};
    juce::Label fileLabel_, summary_, instrument_, hint_;
    juce::ListBox tracks_{"tracks", this};
    juce::TextEditor warnings_;
    juce::Slider transpose_, tempo_, velocity_, humanize_, tail_;
    juce::Label transposeL_{{}, "Transpose (semitones)"}, tempoL_{{}, "Tempo"}, velocityL_{{}, "Velocity"}, humanizeL_{{}, "Timing humanise (ms)"}, tailL_{{}, "Longest release tail (s)"};
    juce::ToggleButton controllers_{"Keep sustain pedal, mod wheel and expression"}, bend_{"Keep pitch bend"};
    ExportOptionsBox export_;
    JobBar jobBar_{task_};
    juce::Label sectionTracks_{{}, "TRACKS"}, sectionEdit_{{}, "PERFORMANCE"}, sectionExport_{{}, "OUTPUT"};
};

}  // namespace realtone::ui
