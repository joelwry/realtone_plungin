#pragma once

#include "realtone/AudioClip.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <memory>

namespace realtone::studio
{

// Plays a rendered clip through the standalone's audio device, next to the live instrument.
// The clip must already be at the device sample rate (the preview render is made at that rate).
class PreviewPlayer : public juce::AudioIODeviceCallback
{
public:
    PreviewPlayer() = default;

    void play(std::shared_ptr<const realtone::AudioClip> clip);
    void stop();
    bool isPlaying() const { return playing_.load(); }
    double positionSeconds() const;
    double durationSeconds() const;
    double deviceSampleRate() const { return deviceRate_.load(); }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override {}
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData,
                                          int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext& context) override;

private:
    juce::SpinLock lock_;  // guards clip_ swaps; the callback only try-locks
    std::shared_ptr<const realtone::AudioClip> clip_;
    std::atomic<bool> playing_{false};
    std::atomic<std::int64_t> position_{0};
    std::atomic<double> deviceRate_{44100.0};
};

}  // namespace realtone::studio
