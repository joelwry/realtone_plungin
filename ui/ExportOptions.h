#pragma once

#include "Theme.h"

#include "realtone/AudioFile.h"
#include "realtone/Renderer.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

// Sample rate, bit depth and level handling for WAV export. Remembered between sessions.
class ExportOptionsBox : public juce::Component
{
public:
    ExportOptionsBox();
    void resized() override;
    void paint(juce::Graphics&) override;

    double sampleRate() const;
    realtone::WavSampleFormat format() const;
    realtone::NormalizeMode normalizeMode() const;
    void applyTo(realtone::RenderSettings& s) const;

    juce::ValueTree state() const;
    void setState(const juce::ValueTree& v);

    static constexpr int kPreferredHeight = 128;

private:
    void save();
    juce::Label rateLabel_{{}, "Sample rate"}, bitLabel_{{}, "Bit depth"}, levelLabel_{{}, "Level"};
    juce::ComboBox rate_, bits_, level_;
};

}  // namespace realtone::ui
