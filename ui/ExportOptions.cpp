#include "ExportOptions.h"

#include "../studio/Settings.h"

namespace realtone::ui
{

using namespace realtone::studio;

ExportOptionsBox::ExportOptionsBox()
{
    rate_.addItem("44.1 kHz", 1);
    rate_.addItem("48 kHz", 2);
    rate_.addItem("96 kHz", 3);
    bits_.addItem("16-bit PCM", 1);
    bits_.addItem("24-bit PCM", 2);
    bits_.addItem("32-bit float", 3);
    level_.addItem("Limit peaks to -1 dBFS", 1);
    level_.addItem("Normalise to -1 dBFS", 2);
    level_.addItem("Leave level as rendered", 3);

    auto& s = Settings::get();
    rate_.setSelectedId(juce::jlimit(1, 3, s.getInt(keys::exportSampleRate, 2)), juce::dontSendNotification);
    bits_.setSelectedId(juce::jlimit(1, 3, s.getInt(keys::exportBitDepth, 2)), juce::dontSendNotification);
    level_.setSelectedId(juce::jlimit(1, 3, s.getInt(keys::exportNormalize, 1)), juce::dontSendNotification);
    for (auto* c : {&rate_, &bits_, &level_})
    {
        c->onChange = [this] { save(); };
        addAndMakeVisible(c);
    }
    rate_.setTooltip("Sample rate of the exported file. 48 kHz suits video; 44.1 kHz suits music CDs.");
    bits_.setTooltip("24-bit keeps the most detail. 16-bit is smaller and dithered.");
    level_.setTooltip("Rendered audio is kept clear of clipping unless you choose to leave the level alone.");
    for (auto* l : {&rateLabel_, &bitLabel_, &levelLabel_})
    {
        l->setFont(theme::ui(12.5f));
        l->setColour(juce::Label::textColourId, theme::textDim);
        addAndMakeVisible(l);
    }
}

void ExportOptionsBox::save()
{
    auto& s = Settings::get();
    s.setInt(keys::exportSampleRate, rate_.getSelectedId());
    s.setInt(keys::exportBitDepth, bits_.getSelectedId());
    s.setInt(keys::exportNormalize, level_.getSelectedId());
}

double ExportOptionsBox::sampleRate() const
{
    switch (rate_.getSelectedId())
    {
        case 1: return 44100.0;
        case 3: return 96000.0;
        default: return 48000.0;
    }
}

realtone::WavSampleFormat ExportOptionsBox::format() const
{
    switch (bits_.getSelectedId())
    {
        case 1: return realtone::WavSampleFormat::Pcm16;
        case 3: return realtone::WavSampleFormat::Float32;
        default: return realtone::WavSampleFormat::Pcm24;
    }
}

realtone::NormalizeMode ExportOptionsBox::normalizeMode() const
{
    switch (level_.getSelectedId())
    {
        case 2: return realtone::NormalizeMode::NormalizePeak;
        case 3: return realtone::NormalizeMode::None;
        default: return realtone::NormalizeMode::LimitPeak;
    }
}

void ExportOptionsBox::applyTo(realtone::RenderSettings& s) const
{
    s.sampleRate = sampleRate();
    s.normalize = normalizeMode();
    s.targetPeakDb = -1.0f;
}

juce::ValueTree ExportOptionsBox::state() const
{
    return juce::ValueTree("Export", {{"rate", rate_.getSelectedId()}, {"bits", bits_.getSelectedId()}, {"level", level_.getSelectedId()}});
}

void ExportOptionsBox::setState(const juce::ValueTree& v)
{
    if (!v.isValid())
        return;
    rate_.setSelectedId(juce::jlimit(1, 3, static_cast<int>(v.getProperty("rate", 2))), juce::dontSendNotification);
    bits_.setSelectedId(juce::jlimit(1, 3, static_cast<int>(v.getProperty("bits", 2))), juce::dontSendNotification);
    level_.setSelectedId(juce::jlimit(1, 3, static_cast<int>(v.getProperty("level", 1))), juce::dontSendNotification);
}

void ExportOptionsBox::paint(juce::Graphics&) {}

void ExportOptionsBox::resized()
{
    auto r = getLocalBounds();
    auto row = [&](juce::Label& l, juce::ComboBox& c) {
        auto line = r.removeFromTop(theme::controlHeight);
        l.setBounds(line.removeFromLeft(juce::jmin(92, line.getWidth() / 3)));
        c.setBounds(line);
        r.removeFromTop(8);
    };
    row(rateLabel_, rate_);
    row(bitLabel_, bits_);
    row(levelLabel_, level_);
}

}  // namespace realtone::ui
