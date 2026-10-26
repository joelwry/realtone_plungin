#pragma once

#include <juce_graphics/juce_graphics.h>

namespace realtone::studio
{

// Decides where the standalone window opens. Saved positions are only reused when they are still
// reachable (a monitor may have been unplugged or the resolution changed since last time).
struct WindowGeometry
{
    static constexpr int kMinWidth = 900;
    static constexpr int kMinHeight = 580;
    static constexpr int kPreferredWidth = 1240;
    static constexpr int kPreferredHeight = 800;

    // Default size: the preferred size, but never more than 90% of the work area, and never below the minimum
    // (unless the work area itself is smaller, in which case the window fills it).
    static juce::Rectangle<int> initialBounds(juce::Rectangle<int> workArea);

    // Returns `saved` if it is usable on one of the work areas (title bar reachable, size within
    // limits); otherwise a corrected or default rectangle on the work area closest to it.
    static juce::Rectangle<int> sanitise(juce::Rectangle<int> saved, const juce::Array<juce::Rectangle<int>>& workAreas);

    // Largest size the window may take: the largest work area (so it can be maximised on any monitor).
    static juce::Point<int> maximumSize(const juce::Array<juce::Rectangle<int>>& workAreas);
};

}  // namespace realtone::studio
