#pragma once

#include "ScrollPage.h"

#include "../studio/AppContext.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace realtone::ui
{

class SettingsPanel : public ScrollPage
{
public:
    SettingsPanel(realtone::studio::AppContext& context, std::function<void()> resetWindow);

private:
    int doLayout(int width);
    realtone::studio::AppContext& ctx_;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> devices_;
    juce::Label audioTitle_{{}, "AUDIO AND MIDI"}, audioNote_, memTitle_{{}, "SAMPLE MEMORY"}, memNote_, windowTitle_{{}, "WINDOW"};
    juce::Slider memory_;
    juce::TextButton resetWindow_{"Reset window size and position"};
};

}  // namespace realtone::ui
