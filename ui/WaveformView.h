#pragma once

#include "Theme.h"

#include "realtone/AudioClip.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace realtone::ui
{

class WaveformView : public juce::Component
{
public:
    void setClip(std::shared_ptr<const realtone::AudioClip> clip);
    void setPlayhead(double seconds);
    double duration() const { return clip_ != nullptr ? clip_->durationSeconds() : 0.0; }
    void paint(juce::Graphics&) override;
    void resized() override { peaksWidth_ = -1; }

private:
    void buildPeaks();
    std::shared_ptr<const realtone::AudioClip> clip_;
    std::vector<std::pair<float, float>> peaks_;
    int peaksWidth_ = -1;
    double playhead_ = -1.0;
};

}  // namespace realtone::ui
