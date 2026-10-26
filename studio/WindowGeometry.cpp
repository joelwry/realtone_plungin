#include "WindowGeometry.h"

namespace realtone::studio
{

juce::Rectangle<int> WindowGeometry::initialBounds(juce::Rectangle<int> work)
{
    const int maxW = juce::jmax(1, static_cast<int>(work.getWidth() * 0.9f));
    const int maxH = juce::jmax(1, static_cast<int>(work.getHeight() * 0.9f));
    int w = juce::jmin(kPreferredWidth, maxW);
    int h = juce::jmin(kPreferredHeight, maxH);
    w = juce::jmax(w, juce::jmin(kMinWidth, work.getWidth()));
    h = juce::jmax(h, juce::jmin(kMinHeight, work.getHeight()));
    return juce::Rectangle<int>(w, h).withCentre(work.getCentre());
}

juce::Rectangle<int> WindowGeometry::sanitise(juce::Rectangle<int> saved, const juce::Array<juce::Rectangle<int>>& workAreas)
{
    if (workAreas.isEmpty())
        return saved;

    // Work area that overlaps the saved window most, else the first (primary).
    juce::Rectangle<int> best = workAreas.getFirst();
    int bestOverlap = -1;
    for (const auto& a : workAreas)
    {
        const auto o = a.getIntersection(saved);
        const int area = o.getWidth() * o.getHeight();
        if (area > bestOverlap)
        {
            bestOverlap = area;
            best = a;
        }
    }

    if (saved.isEmpty() || bestOverlap <= 0)
        return initialBounds(best);

    // Clamp size.
    int w = juce::jlimit(juce::jmin(kMinWidth, best.getWidth()), best.getWidth(), saved.getWidth());
    int h = juce::jlimit(juce::jmin(kMinHeight, best.getHeight()), best.getHeight(), saved.getHeight());
    auto r = juce::Rectangle<int>(saved.getX(), saved.getY(), w, h);

    // Keep the title bar reachable: at least 160 px of its width and its full height on screen.
    const int grab = 160;
    if (r.getRight() < best.getX() + grab) r.setX(best.getX());
    if (r.getX() > best.getRight() - grab) r.setX(best.getRight() - w);
    if (r.getY() < best.getY()) r.setY(best.getY());
    if (r.getY() > best.getBottom() - 60) r.setY(best.getBottom() - h);
    return r.constrainedWithin(best);
}

juce::Point<int> WindowGeometry::maximumSize(const juce::Array<juce::Rectangle<int>>& workAreas)
{
    juce::Point<int> m(kPreferredWidth, kPreferredHeight);
    for (const auto& a : workAreas)
    {
        m.x = juce::jmax(m.x, a.getWidth());
        m.y = juce::jmax(m.y, a.getHeight());
    }
    return m;
}

}  // namespace realtone::studio
