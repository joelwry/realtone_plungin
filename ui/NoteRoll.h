#pragma once

#include "Theme.h"

#include "realtone/PitchAnalysis.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace realtone::ui
{

// Piano-roll view of the notes found in an audio file, so the user can see what will be played and
// switch off or fix obvious mistakes before rendering. Notes the analyser is unsure about are drawn
// paler. Click selects, double-click mutes or restores a note.
class NoteRoll : public juce::Component
{
public:
    void setNotes(std::vector<realtone::DetectedNote>* notes, double durationSeconds);
    void setPlayhead(double seconds);
    int selected() const { return selected_; }
    void select(int index);
    std::function<void()> onChanged;
    std::function<void(int)> onSelectionChanged;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    int noteAt(juce::Point<int> p) const;

private:
    juce::Rectangle<float> rectFor(const realtone::DetectedNote& n) const;
    std::vector<realtone::DetectedNote>* notes_ = nullptr;
    double duration_ = 0.0;
    double playhead_ = -1.0;
    int selected_ = -1;
    int low_ = 48, high_ = 84;
};

}  // namespace realtone::ui
