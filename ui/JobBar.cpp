#include "JobBar.h"

namespace realtone::ui
{

JobBar::JobBar(realtone::studio::BackgroundTask& task) : task_(task)
{
    message_.setFont(theme::ui(13.0f));
    message_.setMinimumHorizontalScale(0.8f);
    message_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(message_);
    addChildComponent(bar_);
    addChildComponent(cancel_);
    cancel_.onClick = [this] { task_.cancel(); };
    startTimerHz(15);
}

void JobBar::setMessage(const juce::String& text, juce::Colour colour)
{
    message_.setText(text, juce::dontSendNotification);
    message_.setColour(juce::Label::textColourId, colour);
}

void JobBar::timerCallback()
{
    const bool running = task_.isRunning();
    if (running)
    {
        progress_ = task_.progress();
        message_.setText(task_.stage(), juce::dontSendNotification);
        message_.setColour(juce::Label::textColourId, theme::textDim);
    }
    if (running != wasRunning_)
    {
        wasRunning_ = running;
        bar_.setVisible(running);
        cancel_.setVisible(running);
        resized();
    }
}

void JobBar::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop(24);
    if (cancel_.isVisible())
        cancel_.setBounds(top.removeFromRight(80).withHeight(24));
    message_.setBounds(top);
    r.removeFromTop(4);
    bar_.setBounds(r.removeFromTop(10));
}

void JobBar::paint(juce::Graphics&) {}

}  // namespace realtone::ui
