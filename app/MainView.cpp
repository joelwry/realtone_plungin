#include "MainView.h"

#include "../studio/Paths.h"
#include "../studio/Settings.h"
#include "../ui/AboutPanel.h"
#include "../ui/LibraryPanel.h"
#include "../ui/SettingsPanel.h"
#include "../ui/StudioView.h"

namespace realtone::app
{

using namespace realtone::studio;

MainView::MainView(AppContext& context, std::function<void()> resetWindow) : ctx_(context), shell_("v" REALTONE_VERSION)
{
    shell_.addPage("Play", std::make_unique<ui::StudioView>(ctx_.processor));
    auto midi = std::make_unique<ui::MidiRenderPanel>(ctx_);
    midi_ = midi.get();
    shell_.addPage("MIDI to WAV", std::move(midi));
    auto audio = std::make_unique<ui::AudioConvertPanel>(ctx_);
    audio_ = audio.get();
    shell_.addPage("Audio to Instrument", std::move(audio));
    shell_.addPage("Library", std::make_unique<ui::LibraryPanel>(ctx_.processor.instruments()));
    shell_.addPage("Settings", std::make_unique<ui::SettingsPanel>(ctx_, std::move(resetWindow)));
    shell_.addPage("About", std::make_unique<ui::AboutPanel>("Standalone"));
    addAndMakeVisible(shell_);
}

MainView::~MainView() = default;

void MainView::paint(juce::Graphics& g) { g.fillAll(ui::theme::background); }
void MainView::resized() { shell_.setBounds(getLocalBounds()); }

void MainView::showPage(Page p) { shell_.setCurrentPage(static_cast<int>(p)); }

bool MainView::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const auto ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".mid" || ext == ".midi" || ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".rtproject")
            return true;
    }
    return false;
}

void MainView::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
    {
        openFile(juce::File(f));
        return;  // one file at a time
    }
}

void MainView::openFile(const juce::File& file)
{
    const auto ext = file.getFileExtension().toLowerCase();
    if (ext == ".mid" || ext == ".midi")
    {
        showPage(Midi);
        midi_->loadFile(file);
    }
    else if (ext == ".wav" || ext == ".mp3" || ext == ".flac")
    {
        showPage(Audio);
        audio_->loadFile(file);
    }
    else if (ext == ".rtproject")
    {
        juce::String error;
        if (!loadProject(file, error))
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Open project", error);
    }
    else
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Open",
                                               "RealTone Studio opens MIDI files (.mid), audio files (.wav, .mp3, .flac) and projects (.rtproject).");
}

bool MainView::saveProject(const juce::File& file, juce::String& error)
{
    juce::ValueTree v("RealToneProject", {{"version", 1}, {"instrument", ctx_.processor.instruments().selectedId()},
                                          {"preset", ctx_.processor.currentPresetName()}});
    v.appendChild(ctx_.processor.apvts.copyState(), nullptr);
    v.appendChild(midi_->state(), nullptr);
    v.appendChild(audio_->state(), nullptr);
    auto xml = v.createXml();
    const auto tmp = file.withFileExtension("tmp");
    if (xml == nullptr || !xml->writeTo(tmp) || !tmp.moveFileTo(file))
    {
        tmp.deleteFile();
        error = "The project could not be written to " + file.getFullPathName();
        return false;
    }
    project_ = file;
    return true;
}

bool MainView::loadProject(const juce::File& file, juce::String& error)
{
    auto xml = juce::parseXML(file);
    if (xml == nullptr)
    {
        error = "This is not a readable RealTone Studio project: " + file.getFullPathName();
        return false;
    }
    auto v = juce::ValueTree::fromXml(*xml);
    if (!v.isValid() || !v.hasType("RealToneProject"))
    {
        error = "This file is not a RealTone Studio project.";
        return false;
    }
    if (static_cast<int>(v.getProperty("version", 1)) > 1)
    {
        error = "This project was saved by a newer version of RealTone Studio.";
        return false;
    }
    auto params = v.getChildWithName(ctx_.processor.apvts.state.getType());
    if (params.isValid())
        ctx_.processor.apvts.replaceState(params);
    ctx_.processor.setCurrentPresetName(v.getProperty("preset", "Default").toString());

    const auto instrument = v.getProperty("instrument").toString();
    if (instrument.isNotEmpty())
    {
        if (ctx_.processor.instruments().find(instrument) != nullptr)
            ctx_.processor.chooseInstrument(instrument, false);
        else
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Open project",
                                                   "The instrument used by this project is not available on this computer:\n" + instrument);
    }
    midi_->setState(v.getChildWithName("MidiRender"));
    audio_->setState(v.getChildWithName("AudioConvert"));
    project_ = file;
    return true;
}

}  // namespace realtone::app
