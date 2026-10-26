#include "PreviewPlayer.h"

namespace realtone::studio
{

void PreviewPlayer::play(std::shared_ptr<const realtone::AudioClip> clip)
{
    const juce::SpinLock::ScopedLockType sl(lock_);
    clip_ = std::move(clip);
    position_ = 0;
    playing_ = clip_ != nullptr && clip_->numFrames() > 0;
}

void PreviewPlayer::stop()
{
    playing_ = false;
    // Release the clip later from a non-audio thread; the callback only reads it under try-lock.
    const juce::SpinLock::ScopedLockType sl(lock_);
    clip_.reset();
}

double PreviewPlayer::positionSeconds() const
{
    const double sr = deviceRate_.load();
    return sr > 0.0 ? static_cast<double>(position_.load()) / sr : 0.0;
}

double PreviewPlayer::durationSeconds() const
{
    const juce::SpinLock::ScopedLockType sl(lock_);
    return clip_ != nullptr ? clip_->durationSeconds() : 0.0;
}

void PreviewPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    deviceRate_ = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
}

void PreviewPlayer::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* out, int numOut, int numSamples,
                                                     const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr)
            juce::FloatVectorOperations::clear(out[c], numSamples);

    if (!playing_.load())
        return;
    const juce::SpinLock::ScopedTryLockType sl(lock_);
    if (!sl.isLocked() || clip_ == nullptr)
        return;

    const auto frames = clip_->numFrames();
    const auto pos = position_.load();
    const int n = static_cast<int>(juce::jmin<std::int64_t>(numSamples, frames - pos));
    for (int c = 0; c < numOut; ++c)
    {
        if (out[c] == nullptr || n <= 0)
            continue;
        const auto& src = clip_->channels[static_cast<std::size_t>(juce::jmin(c, clip_->numChannels() - 1))];
        juce::FloatVectorOperations::copy(out[c], src.data() + pos, n);
    }
    position_ = pos + n;
    if (pos + n >= frames)
        playing_ = false;
}

}  // namespace realtone::studio
