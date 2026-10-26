#include "SettingsPanel.h"

#include "Theme.h"

#include "../studio/Settings.h"

namespace realtone::ui
{

using namespace realtone::studio;

SettingsPanel::SettingsPanel(AppContext& context, std::function<void()> resetWindow) : ctx_(context)
{
    auto& c = content();
    for (auto* l : {&audioTitle_, &memTitle_, &windowTitle_})
    {
        l->setFont(theme::ui(12.5f, true));
        l->setColour(juce::Label::textColourId, theme::textDim);
        c.addAndMakeVisible(l);
    }
    for (auto* l : {&audioNote_, &memNote_})
    {
        l->setFont(theme::ui(13.0f));
        l->setColour(juce::Label::textColourId, theme::textDim);
        c.addAndMakeVisible(l);
    }
    audioNote_.setText(ctx_.audioRunning() ? "Choose the output device for live playing and previews, and which MIDI keyboards are active."
                                           : "No audio output is running: " + ctx_.audioProblem() + " Rendering and exporting still work.",
                       juce::dontSendNotification);
    if (!ctx_.audioRunning())
        audioNote_.setColour(juce::Label::textColourId, theme::warn);

    devices_ = std::make_unique<juce::AudioDeviceSelectorComponent>(ctx_.deviceManager, 0, 0, 0, 2, true, false, true, false);
    c.addAndMakeVisible(*devices_);

    memNote_.setText("Instruments are loaded into memory so they can play without delay. A very large instrument is refused when it would exceed this limit.",
                     juce::dontSendNotification);
    memory_.setSliderStyle(juce::Slider::LinearHorizontal);
    memory_.setRange(512, 16384, 128);
    memory_.setValue(Settings::get().sampleMemoryLimitMB(), juce::dontSendNotification);
    memory_.setTextValueSuffix(" MB");
    memory_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 22);
    memory_.onValueChange = [this] { Settings::get().setInt(keys::sampleMemoryMB, juce::roundToInt(memory_.getValue())); };
    c.addAndMakeVisible(memory_);

    resetWindow_.onClick = std::move(resetWindow);
    c.addAndMakeVisible(resetWindow_);
    layout = [this](int w) { return doLayout(w); };
}

int SettingsPanel::doLayout(int width)
{
    const int pad = 16;
    const int inner = width - pad * 2;
    int y = pad;
    audioTitle_.setBounds(pad, y, inner, 20);
    y += 24;
    audioNote_.setBounds(pad, y, inner, inner < 640 ? 40 : 22);
    y += (inner < 640 ? 44 : 28);
    devices_->setBounds(pad, y, inner, 300);
    y += 316;
    memTitle_.setBounds(pad, y, inner, 20);
    y += 24;
    memNote_.setBounds(pad, y, inner, inner < 640 ? 40 : 22);
    y += (inner < 640 ? 46 : 30);
    memory_.setBounds(pad, y, juce::jmin(inner, 520), 24);
    y += 40;
    windowTitle_.setBounds(pad, y, inner, 20);
    y += 26;
    resetWindow_.setBounds(pad, y, 280, theme::controlHeight);
    y += theme::controlHeight + pad;
    return y;
}

}  // namespace realtone::ui
