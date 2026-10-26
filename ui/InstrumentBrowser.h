#pragma once

#include "Theme.h"

#include "../studio/InstrumentManager.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace realtone::ui
{

// Searchable, filterable grid of instrument cards. Selecting a card calls onChoose; cards whose
// sample files are missing say so and offer to install them. Nothing is substituted silently.
class InstrumentBrowser : public juce::Component, private juce::ChangeListener, private juce::TextEditor::Listener
{
public:
    InstrumentBrowser(realtone::studio::InstrumentManager& manager, std::function<void(const juce::String&)> onChoose);
    ~InstrumentBrowser() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    void setSelected(const juce::String& id);
    int numVisibleCards() const;

private:
    class Card;
    class Grid;

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void textEditorTextChanged(juce::TextEditor&) override { rebuild(); }
    void rebuild();
    void updateCategories();

    realtone::studio::InstrumentManager& manager_;
    std::function<void(const juce::String&)> onChoose_;
    juce::TextEditor search_;
    juce::ComboBox category_;
    juce::ToggleButton installedOnly_{"Installed only"};
    juce::Label jobLabel_;
    juce::ProgressBar jobBar_;
    double jobProgress_ = 0.0;
    std::unique_ptr<Grid> grid_;
    juce::Viewport viewport_;
    juce::String selected_;
    juce::Label emptyLabel_;
};

}  // namespace realtone::ui
