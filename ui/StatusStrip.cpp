#include "StatusStrip.h"

namespace realtone::ui
{

using State = realtone::studio::LoadStatus::State;

StatusStrip::StatusStrip(realtone::studio::RealToneProcessor& processor) : processor_(processor)
{
    processor_.instruments().addChangeListener(this);
    startTimerHz(20);
}

StatusStrip::~StatusStrip()
{
    stopTimer();
    processor_.instruments().removeChangeListener(this);
}

void StatusStrip::timerCallback()
{
    const float p = processor_.outputPeak.load(std::memory_order_relaxed);
    peak_ = juce::jmax(p, peak_ * 0.85f);
    voices_ = processor_.voiceCount.load(std::memory_order_relaxed);
    repaint();
}

void StatusStrip::paint(juce::Graphics& g)
{
    using namespace theme;
    const auto s = processor_.instruments().status();
    juce::Colour dot = textFaint;
    juce::String msg = "Choose an instrument to start playing.";
    switch (s.state)
    {
        case State::Idle: break;
        case State::Loading: dot = info; msg = s.message.isEmpty() ? "Loading..." : s.message; break;
        case State::Ready: dot = ok; msg = s.message; break;
        case State::ReadyPartial: dot = warn; msg = s.message; break;
        case State::Failed: dot = error; msg = "Could not load: " + s.message; break;
        case State::NotInstalled: dot = warn; msg = s.message; break;
    }

    auto r = getLocalBounds().reduced(10, 0);
    g.setColour(dot);
    g.fillEllipse(static_cast<float>(r.getX()), static_cast<float>(getHeight()) * 0.5f - 4.0f, 8.0f, 8.0f);

    auto right = r.removeFromRight(juce::jmin(200, r.getWidth() / 3));
    g.setColour(textDim);
    g.setFont(ui(13.0f));
    g.drawFittedText(msg, r.withTrimmedLeft(16), juce::Justification::centredLeft, 2);

    g.setFont(mono(12.0f));
    g.setColour(textFaint);
    g.drawText(juce::String(voices_) + " voices", right.removeFromLeft(78), juce::Justification::centredRight);
    auto meter = right.reduced(8, 11).toFloat();
    g.setColour(surfaceRaised);
    g.fillRoundedRectangle(meter, 3.0f);
    const float level = juce::jlimit(0.0f, 1.0f, juce::Decibels::gainToDecibels(peak_ + 1.0e-5f, -60.0f) / 60.0f + 1.0f);
    g.setColour(peak_ > 0.95f ? error : accent);
    g.fillRoundedRectangle(meter.withWidth(meter.getWidth() * level), 3.0f);

    if (s.state == State::Loading)
    {
        g.setColour(accent.withAlpha(0.6f));
        g.fillRect(0.0f, static_cast<float>(getHeight()) - 2.0f, static_cast<float>(getWidth()) * s.progress, 2.0f);
    }
}

}  // namespace realtone::ui
