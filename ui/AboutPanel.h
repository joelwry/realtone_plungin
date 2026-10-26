#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

class AboutPanel : public juce::Component
{
public:
    explicit AboutPanel(const juce::String& hostDescription);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::TextEditor notices_;
    juce::TextButton openLogs_{"Open log folder"};
    juce::String host_;
};

}  // namespace realtone::ui
