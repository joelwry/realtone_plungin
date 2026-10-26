#include "PresetBar.h"

namespace realtone::ui
{

PresetBar::PresetBar(realtone::studio::RealToneProcessor& processor) : processor_(processor)
{
    caption_.setFont(theme::ui(12.5f, true));
    caption_.setColour(juce::Label::textColourId, theme::textDim);
    addAndMakeVisible(caption_);
    box_.setTextWhenNothingSelected("Preset");
    box_.setTitle("Preset");
    box_.onChange = [this] { choose(box_.getSelectedId() - 1); };
    addAndMakeVisible(box_);
    prev_.onClick = [this] { step(-1); };
    next_.onClick = [this] { step(1); };
    save_.onClick = [this] { promptSave(); };
    delete_.onClick = [this] { deleteCurrent(); };
    prev_.setTooltip("Previous preset");
    next_.setTooltip("Next preset");
    save_.setTooltip("Save the current settings as your own preset");
    delete_.setTooltip("Delete the selected user preset");
    for (auto* b : {&prev_, &next_, &save_, &delete_})
        addAndMakeVisible(b);
    processor_.instruments().addChangeListener(this);
    refresh();
}

PresetBar::~PresetBar() { processor_.instruments().removeChangeListener(this); }

void PresetBar::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (processor_.currentFamily() != family_)
        refresh();
}

void PresetBar::refresh()
{
    family_ = processor_.currentFamily();
    presets_ = processor_.presets().listFor(family_);
    box_.clear(juce::dontSendNotification);
    bool userHeader = false;
    int selected = 0;
    for (int i = 0; i < static_cast<int>(presets_.size()); ++i)
    {
        const auto& pr = presets_[static_cast<std::size_t>(i)];
        if (i == 0)
            box_.addSectionHeading("Factory");
        if (!pr.factory && !userHeader)
        {
            box_.addSectionHeading("My presets");
            userHeader = true;
        }
        box_.addItem(pr.name, i + 1);
        if (pr.name == processor_.currentPresetName())
            selected = i + 1;
    }
    if (selected > 0)
        box_.setSelectedId(selected, juce::dontSendNotification);
    else
        box_.setText(processor_.currentPresetName(), juce::dontSendNotification);
    const int idx = box_.getSelectedId() - 1;
    delete_.setEnabled(idx >= 0 && idx < static_cast<int>(presets_.size()) && !presets_[static_cast<std::size_t>(idx)].factory);
}

void PresetBar::choose(int index)
{
    if (index < 0 || index >= static_cast<int>(presets_.size()))
        return;
    juce::String error;
    const auto& pr = presets_[static_cast<std::size_t>(index)];
    if (!processor_.presets().load(pr, processor_.currentFamily(), error))
    {
        showError(error);
        return;
    }
    processor_.setCurrentPresetName(pr.name);
    delete_.setEnabled(!pr.factory);
}

void PresetBar::step(int delta)
{
    if (presets_.empty())
        return;
    int idx = box_.getSelectedId() - 1;
    idx = idx < 0 ? 0 : (idx + delta + static_cast<int>(presets_.size())) % static_cast<int>(presets_.size());
    box_.setSelectedId(idx + 1, juce::sendNotificationSync);
}

void PresetBar::promptSave()
{
    auto* w = new juce::AlertWindow("Save preset", "Name your preset. It is stored in your user folder.", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", processor_.currentPresetName() == "Default" ? juce::String() : processor_.currentPresetName(), "Name");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<PresetBar>(this), w](int result) {
        std::unique_ptr<juce::AlertWindow> owner(w);
        if (result != 1 || safe == nullptr)
            return;
        const auto name = w->getTextEditorContents("name").trim();
        juce::String error;
        if (!safe->processor_.presets().saveUser(name, error))
        {
            safe->showError(error);
            return;
        }
        safe->processor_.setCurrentPresetName(name);
        safe->refresh();
    }), false);
}

void PresetBar::deleteCurrent()
{
    const int idx = box_.getSelectedId() - 1;
    if (idx < 0 || idx >= static_cast<int>(presets_.size()))
        return;
    juce::String error;
    if (!processor_.presets().deleteUser(presets_[static_cast<std::size_t>(idx)], error))
        showError(error);
    else
    {
        processor_.setCurrentPresetName("Default");
        refresh();
    }
}

void PresetBar::showError(const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Preset", message);
}

void PresetBar::paint(juce::Graphics&) {}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    const bool compact = getWidth() < 520;
    if (!compact)
    {
        caption_.setBounds(r.removeFromLeft(56));
        caption_.setVisible(true);
    }
    else
        caption_.setVisible(false);
    delete_.setBounds(r.removeFromRight(compact ? 58 : 66));
    r.removeFromRight(6);
    save_.setBounds(r.removeFromRight(compact ? 62 : 74));
    r.removeFromRight(6);
    next_.setBounds(r.removeFromRight(30));
    r.removeFromRight(4);
    prev_.setBounds(r.removeFromLeft(30));
    r.removeFromLeft(4);
    box_.setBounds(r);
}

}  // namespace realtone::ui
