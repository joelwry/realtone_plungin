#include "NoteRoll.h"

namespace realtone::ui
{

void NoteRoll::setNotes(std::vector<realtone::DetectedNote>* notes, double duration)
{
    notes_ = notes;
    duration_ = juce::jmax(0.1, duration);
    selected_ = -1;
    low_ = 127;
    high_ = 0;
    if (notes_ != nullptr)
        for (const auto& n : *notes_)
        {
            low_ = juce::jmin(low_, n.midiNote);
            high_ = juce::jmax(high_, n.midiNote);
        }
    if (low_ > high_)
    {
        low_ = 48;
        high_ = 84;
    }
    low_ = juce::jmax(0, low_ - 3);
    high_ = juce::jmin(127, juce::jmax(high_ + 3, low_ + 12));
    repaint();
}

void NoteRoll::setPlayhead(double s)
{
    if (std::fabs(s - playhead_) > 0.01)
    {
        playhead_ = s;
        repaint();
    }
}

void NoteRoll::select(int i)
{
    selected_ = i;
    if (onSelectionChanged)
        onSelectionChanged(i);
    repaint();
}

juce::Rectangle<float> NoteRoll::rectFor(const realtone::DetectedNote& n) const
{
    const auto area = getLocalBounds().toFloat().reduced(4.0f);
    const float rowH = area.getHeight() / static_cast<float>(high_ - low_ + 1);
    const float x = area.getX() + static_cast<float>(n.startSeconds / duration_) * area.getWidth();
    const float w = juce::jmax(3.0f, static_cast<float>((n.endSeconds - n.startSeconds) / duration_) * area.getWidth());
    const float y = area.getBottom() - static_cast<float>(n.midiNote - low_ + 1) * rowH;
    return {x, y + 1.0f, w, juce::jmax(3.0f, rowH - 2.0f)};
}

int NoteRoll::noteAt(juce::Point<int> p) const
{
    if (notes_ == nullptr)
        return -1;
    for (int i = static_cast<int>(notes_->size()) - 1; i >= 0; --i)
        if (rectFor((*notes_)[static_cast<std::size_t>(i)]).expanded(2.0f).contains(p.toFloat()))
            return i;
    return -1;
}

void NoteRoll::mouseDown(const juce::MouseEvent& e) { select(noteAt(e.getPosition())); }

void NoteRoll::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int i = noteAt(e.getPosition());
    if (i >= 0)
    {
        auto& n = (*notes_)[static_cast<std::size_t>(i)];
        n.enabled = !n.enabled;
        if (onChanged)
            onChanged();
        repaint();
    }
}

void NoteRoll::paint(juce::Graphics& g)
{
    using namespace theme;
    g.setColour(surface);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), static_cast<float>(radius));
    g.setColour(border);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), static_cast<float>(radius), 1.0f);
    const auto area = getLocalBounds().toFloat().reduced(4.0f);

    // Octave guides
    const float rowH = area.getHeight() / static_cast<float>(high_ - low_ + 1);
    g.setFont(mono(10.0f));
    for (int n = low_; n <= high_; ++n)
        if (n % 12 == 0)
        {
            const float y = area.getBottom() - static_cast<float>(n - low_ + 1) * rowH;
            g.setColour(border);
            g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
            g.setColour(textFaint);
            g.drawText("C" + juce::String(n / 12 - 1), juce::Rectangle<float>(area.getX() + 2.0f, y - 12.0f, 30.0f, 12.0f), juce::Justification::centredLeft);
        }

    if (notes_ == nullptr || notes_->empty())
    {
        g.setColour(textFaint);
        g.setFont(ui(13.0f));
        g.drawText("Detected notes appear here", getLocalBounds(), juce::Justification::centred);
        return;
    }

    for (int i = 0; i < static_cast<int>(notes_->size()); ++i)
    {
        const auto& n = (*notes_)[static_cast<std::size_t>(i)];
        const auto r = rectFor(n);
        juce::Colour c = n.confidence >= 0.8f ? ok : (n.confidence >= 0.6f ? warn : error);
        if (!n.enabled)
            c = textFaint;
        g.setColour(c.withAlpha(n.enabled ? (0.35f + 0.6f * juce::jlimit(0.0f, 1.0f, n.confidence)) : 0.25f));
        g.fillRoundedRectangle(r, 2.5f);
        if (i == selected_)
        {
            g.setColour(text);
            g.drawRoundedRectangle(r.expanded(1.0f), 3.0f, 1.5f);
        }
    }
    if (playhead_ >= 0.0)
    {
        const float x = area.getX() + static_cast<float>(playhead_ / duration_) * area.getWidth();
        g.setColour(text);
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
    }
}

}  // namespace realtone::ui
