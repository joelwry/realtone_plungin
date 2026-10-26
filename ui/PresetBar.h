#pragma once

#include "Theme.h"

#include "../studio/RealToneProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

class PresetBar : public juce::Component, private juce::ChangeListener
{
public:
    explicit PresetBar(realtone::studio::RealToneProcessor& processor);
    ~PresetBar() override;

    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void choose(int index);
    void step(int delta);
    void promptSave();
    void deleteCurrent();
    void showError(const juce::String& message);

    realtone::studio::RealToneProcessor& processor_;
    std::vector<realtone::studio::PresetInfo> presets_;
    juce::ComboBox box_;
    juce::TextButton prev_{"<"}, next_{">"}, save_{"Save..."}, delete_{"Delete"};
    juce::Label caption_{{}, "Preset"};
    realtone::Family family_ = realtone::Family::Generic;
};

}  // namespace realtone::ui
