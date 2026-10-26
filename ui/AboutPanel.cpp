#include "AboutPanel.h"

#include "../studio/InstrumentManager.h"
#include "../studio/Paths.h"

#ifndef REALTONE_VERSION
#define REALTONE_VERSION "0.0.0"
#endif

namespace realtone::ui
{

AboutPanel::AboutPanel(const juce::String& hostDescription) : host_(hostDescription)
{
    notices_.setMultiLine(true, true);
    notices_.setReadOnly(true);
    notices_.setCaretVisible(false);
    notices_.setFont(theme::mono(12.0f));
    notices_.setText(realtone::studio::bundledLicenseNotices(), false);
    addAndMakeVisible(notices_);
    openLogs_.onClick = [] { realtone::studio::logDirectory().createDirectory(); realtone::studio::logDirectory().revealToUser(); };
    addAndMakeVisible(openLogs_);
}

void AboutPanel::paint(juce::Graphics& g)
{
    using namespace theme;
    g.fillAll(background);
    auto r = getLocalBounds().reduced(16);
    g.setColour(text);
    g.setFont(ui(22.0f, true));
    g.drawText("RealTone Studio", r.removeFromTop(30), juce::Justification::centredLeft);
    g.setColour(textDim);
    g.setFont(ui(14.0f));
    g.drawText("Version " REALTONE_VERSION "  -  " + host_ + "  -  built " __DATE__, r.removeFromTop(22), juce::Justification::centredLeft);
    r.removeFromTop(6);
    g.setColour(textDim);
    g.setFont(ui(13.0f));
    g.drawFittedText("Sample-based instruments, MIDI rendering and monophonic audio conversion. Sample libraries are licensed separately; "
                     "the notices below list every library and component used and their terms.",
                     r.removeFromTop(40), juce::Justification::topLeft, 3);
}

void AboutPanel::resized()
{
    auto r = getLocalBounds().reduced(16);
    r.removeFromTop(104);
    auto bottom = r.removeFromBottom(30);
    openLogs_.setBounds(bottom.removeFromLeft(150));
    r.removeFromBottom(8);
    notices_.setBounds(r);
}

}  // namespace realtone::ui
