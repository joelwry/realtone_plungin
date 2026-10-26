#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace realtone::ui
{

// A page whose content scrolls vertically when the window is too short. The owner adds controls
// to content() and supplies a layout function that returns the height needed for a given width, so
// narrow windows reflow instead of clipping.
class ScrollPage : public juce::Component
{
public:
    ScrollPage()
    {
        viewport_.setViewedComponent(&content_, false);
        viewport_.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport_);
    }
    ~ScrollPage() override { viewport_.setViewedComponent(nullptr, false); }

    juce::Component& content() { return content_; }
    juce::Viewport& viewport() { return viewport_; }
    std::function<int(int width)> layout;  // sets child bounds, returns required height

    void relayout()
    {
        const int w = juce::jmax(1, viewport_.getWidth() - viewport_.getScrollBarThickness());
        const int h = layout ? layout(w) : viewport_.getHeight();
        content_.setSize(w, juce::jmax(h, viewport_.getHeight()));
    }

    void resized() override
    {
        viewport_.setBounds(getLocalBounds());
        relayout();
    }
    void paint(juce::Graphics& g) override { g.fillAll(theme::background); }

private:
    juce::Component content_;
    juce::Viewport viewport_;
};

}  // namespace realtone::ui
