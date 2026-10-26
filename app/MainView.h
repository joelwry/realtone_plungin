#pragma once

#include "../ui/AudioConvertPanel.h"
#include "../ui/MidiRenderPanel.h"
#include "../ui/TabShell.h"

#include "../studio/AppContext.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace realtone::app
{

// Content of the standalone window: all pages plus project save/load and file drag-and-drop.
class MainView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    enum Page { Play = 0, Midi, Audio, Library, Settings, About };

    MainView(realtone::studio::AppContext& context, std::function<void()> resetWindow);
    ~MainView() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

    void showPage(Page p);
    void openFile(const juce::File& file);

    // Projects: instrument, performance settings, MIDI and audio page state.
    bool saveProject(const juce::File& file, juce::String& error);
    bool loadProject(const juce::File& file, juce::String& error);
    juce::File currentProject() const { return project_; }

    realtone::ui::MidiRenderPanel& midiPanel() { return *midi_; }
    realtone::ui::AudioConvertPanel& audioPanel() { return *audio_; }
    realtone::ui::TabShell& shell() { return shell_; }
    int minimumWidth() const { return shell_.minimumWidth(); }

private:
    realtone::studio::AppContext& ctx_;
    realtone::ui::TabShell shell_;
    realtone::ui::MidiRenderPanel* midi_ = nullptr;
    realtone::ui::AudioConvertPanel* audio_ = nullptr;
    juce::File project_;
    juce::TooltipWindow tooltips_{this, 600};
};

}  // namespace realtone::app
