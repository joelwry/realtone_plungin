#pragma once

#include "Theme.h"

#include "../studio/BackgroundTask.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

// Progress display for a BackgroundTask: stage text, a bar, and a Cancel button. Hidden while idle,
// but keeps showing the last result message.
class JobBar : public juce::Component, private juce::Timer
{
public:
    explicit JobBar(realtone::studio::BackgroundTask& task);
    ~JobBar() override { stopTimer(); }

    void setMessage(const juce::String& text, juce::Colour colour);
    void resized() override;
    void paint(juce::Graphics&) override;
    static constexpr int kHeight = 52;

private:
    void timerCallback() override;
    realtone::studio::BackgroundTask& task_;
    juce::TextButton cancel_{"Cancel"};
    juce::Label message_;
    double progress_ = 0.0;
    juce::ProgressBar bar_{progress_};
    bool wasRunning_ = false;
};

}  // namespace realtone::ui
