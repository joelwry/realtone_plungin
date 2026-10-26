#include "WaveformView.h"

namespace realtone::ui
{

void WaveformView::setClip(std::shared_ptr<const realtone::AudioClip> clip)
{
    clip_ = std::move(clip);
    peaksWidth_ = -1;
    repaint();
}

void WaveformView::setPlayhead(double seconds)
{
    if (std::fabs(seconds - playhead_) > 0.01)
    {
        playhead_ = seconds;
        repaint();
    }
}

void WaveformView::buildPeaks()
{
    peaksWidth_ = getWidth();
    peaks_.assign(static_cast<std::size_t>(juce::jmax(1, peaksWidth_)), {0.0f, 0.0f});
    if (clip_ == nullptr || clip_->numFrames() == 0 || peaksWidth_ <= 0)
        return;
    const auto frames = clip_->numFrames();
    for (int x = 0; x < peaksWidth_; ++x)
    {
        const auto a = frames * x / peaksWidth_;
        const auto b = juce::jmax<std::int64_t>(a + 1, frames * (x + 1) / peaksWidth_);
        float lo = 0.0f, hi = 0.0f;
        for (const auto& ch : clip_->channels)
            for (auto i = a; i < b; i += juce::jmax<std::int64_t>(1, (b - a) / 64))
            {
                lo = juce::jmin(lo, ch[static_cast<std::size_t>(i)]);
                hi = juce::jmax(hi, ch[static_cast<std::size_t>(i)]);
            }
        peaks_[static_cast<std::size_t>(x)] = {lo, hi};
    }
}

void WaveformView::paint(juce::Graphics& g)
{
    using namespace theme;
    g.setColour(surface);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), static_cast<float>(radius));
    g.setColour(border);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), static_cast<float>(radius), 1.0f);
    if (clip_ == nullptr)
    {
        g.setColour(textFaint);
        g.setFont(ui(13.0f));
        g.drawText("No audio loaded", getLocalBounds(), juce::Justification::centred);
        return;
    }
    if (peaksWidth_ != getWidth())
        buildPeaks();
    const auto r = getLocalBounds().reduced(2);
    const float mid = static_cast<float>(r.getCentreY());
    const float half = static_cast<float>(r.getHeight()) * 0.5f - 2.0f;
    g.setColour(accent.withAlpha(0.85f));
    for (int x = 0; x < juce::jmin(peaksWidth_, r.getWidth()); ++x)
    {
        const auto [lo, hi] = peaks_[static_cast<std::size_t>(x)];
        g.drawVerticalLine(r.getX() + x, mid - hi * half, mid - lo * half + 1.0f);
    }
    if (playhead_ >= 0.0 && clip_->durationSeconds() > 0.0)
    {
        const float px = static_cast<float>(r.getX()) + static_cast<float>(playhead_ / clip_->durationSeconds()) * static_cast<float>(r.getWidth());
        g.setColour(text);
        g.drawVerticalLine(juce::roundToInt(px), static_cast<float>(r.getY()), static_cast<float>(r.getBottom()));
    }
}

}  // namespace realtone::ui
