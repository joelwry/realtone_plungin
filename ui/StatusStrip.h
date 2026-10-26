#pragma once

#include "Theme.h"

#include "../studio/RealToneProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

// One line that always says what the instrument is doing: loading, ready, missing files, or failed.
class StatusStrip : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit StatusStrip(realtone::studio::RealToneProcessor& processor);
    ~StatusStrip() override;
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }

    realtone::studio::RealToneProcessor& processor_;
    float peak_ = 0.0f;
    int voices_ = 0;
};

}  // namespace realtone::ui
