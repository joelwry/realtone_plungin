#pragma once

#include "PitchBendWheel.h"
#include "Theme.h"

#include "../studio/RealToneProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace realtone::ui
{

// All performance controls, grouped into sections that reflow to the available width and
// scroll vertically when the window is short, so no control is ever clipped or unreachable.
class PerformancePanel : public juce::Component
{
public:
    explicit PerformancePanel(realtone::studio::RealToneProcessor& processor);
    ~PerformancePanel() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    // Height needed to show everything at the given width (used for the scroll area).
    int contentHeightForWidth(int width) const;
    int minimumUsefulWidth() const { return 300; }

private:
    class Section;
    class Content;

    realtone::studio::RealToneProcessor& processor_;
    juce::Viewport viewport_;
    std::unique_ptr<Content> content_;
};

}  // namespace realtone::ui
