#include "PluginEditor.h"

#include "AboutPanel.h"
#include "LibraryPanel.h"
#include "StudioView.h"

#ifndef REALTONE_VERSION
#define REALTONE_VERSION "0.0.0"
#endif

namespace realtone::ui
{

PluginEditor::PluginEditor(realtone::studio::RealToneProcessor& processor)
    : juce::AudioProcessorEditor(processor), processor_(processor), shell_("v" REALTONE_VERSION)
{
    // Read the remembered size before anything below can resize the editor and overwrite it.
    const auto saved = processor.savedEditorSize();
    setLookAndFeel(&laf_);
    shell_.addPage("Play", std::make_unique<StudioView>(processor));
    shell_.addPage("Library", std::make_unique<LibraryPanel>(processor.instruments()));
    shell_.addPage("About", std::make_unique<AboutPanel>("VST3 instrument"));
    addAndMakeVisible(shell_);

    // Resizing: tell the host what we support, and remember the size.
    setResizable(true, false);
    setResizeLimits(kMinWidth, kMinHeight, 3200, 2000);

    const int w = saved.x >= kMinWidth ? saved.x : kDefaultWidth;
    const int h = saved.y >= kMinHeight ? saved.y : kDefaultHeight;
    setSize(w, h);
    constructed_ = true;
}

PluginEditor::~PluginEditor() { setLookAndFeel(nullptr); }

void PluginEditor::paint(juce::Graphics& g) { g.fillAll(theme::background); }

void PluginEditor::resized()
{
    shell_.setBounds(getLocalBounds());
    if (constructed_ && getWidth() >= kMinWidth && getHeight() >= kMinHeight)
        processor_.setSavedEditorSize({getWidth(), getHeight()});
}

}  // namespace realtone::ui
