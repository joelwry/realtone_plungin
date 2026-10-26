#pragma once

#include "TabShell.h"
#include "Theme.h"

#include "../studio/RealToneProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace realtone::ui
{

// The VST3 editor. Resizable in hosts that allow it; its size is stored with the plug-in state.
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(realtone::studio::RealToneProcessor& processor);
    ~PluginEditor() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    static constexpr int kDefaultWidth = 1040;
    static constexpr int kDefaultHeight = 660;
    static constexpr int kMinWidth = 700;
    static constexpr int kMinHeight = 440;

private:
    realtone::studio::RealToneProcessor& processor_;
    RealToneLookAndFeel laf_;
    TabShell shell_;
    juce::TooltipWindow tooltips_{this, 600};
    bool constructed_ = false;
};

}  // namespace realtone::ui
