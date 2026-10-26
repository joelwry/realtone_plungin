#pragma once

#include "InstrumentBrowser.h"
#include "PerformancePanel.h"
#include "PresetBar.h"
#include "StatusStrip.h"
#include "Theme.h"

#include "../studio/RealToneProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

// The "Play" page shared by the plug-in and the standalone application: instruments on one side,
// performance controls on the other, a keyboard and a status line underneath. Below a minimum
// width the two halves become two tabs instead of squeezing.
class StudioView : public juce::Component
{
public:
    explicit StudioView(realtone::studio::RealToneProcessor& processor);
    ~StudioView() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    enum class Layout { Wide, Tabbed };
    Layout layout() const { return layout_; }
    InstrumentBrowser& browser() { return *browser_; }
    PerformancePanel& performance() { return *performance_; }
    juce::MidiKeyboardComponent& keyboard() { return keyboard_; }
    bool keyboardVisible() const { return keyboard_.isVisible(); }

private:
    void showTab(int index);

    realtone::studio::RealToneProcessor& processor_;
    std::unique_ptr<InstrumentBrowser> browser_;
    std::unique_ptr<PerformancePanel> performance_;
    PresetBar presetBar_;
    StatusStrip status_;
    juce::MidiKeyboardComponent keyboard_;
    juce::TextButton tabInstruments_{"Instruments"}, tabPerformance_{"Performance"};
    Layout layout_ = Layout::Wide;
    int tab_ = 0;
};

}  // namespace realtone::ui
