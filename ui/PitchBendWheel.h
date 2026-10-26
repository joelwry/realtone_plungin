#pragma once

#include "Theme.h"

#include "../studio/RealToneProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

// Prominent pitch-bend control: centred at zero, springs back on release, shows the bend in
// semitones and, as a second marker, any bend arriving from a MIDI controller or the host.
class PitchBendWheel : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit PitchBendWheel(realtone::studio::RealToneProcessor& processor);
    ~PitchBendWheel() override;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // -1..1
    float value() const { return value_; }

private:
    void timerCallback() override;
    void setFromY(float y);
    void openGesture();
    void closeGesture();
    float currentRangeSemitones() const;
    bool bendEnabled() const;
    juce::Rectangle<float> trackBounds() const;

    realtone::studio::RealToneProcessor& processor_;
    juce::ParameterAttachment attachment_;
    float value_ = 0.0f;
    float midiValue_ = 0.0f;
    bool dragging_ = false;
    bool springBack_ = false;
    bool gestureOpen_ = false;
};

}  // namespace realtone::ui
