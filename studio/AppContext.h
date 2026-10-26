#pragma once

#include "PreviewPlayer.h"
#include "RealToneProcessor.h"

#include "realtone/Renderer.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace realtone::studio
{

// Everything the standalone application shares between its pages: the live instrument, the audio
// device, the preview player and the helpers that turn the Play page's settings into render settings.
class AppContext
{
public:
    AppContext();
    ~AppContext();

    RealToneProcessor processor;
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer player;
    PreviewPlayer preview;

    bool audioRunning() const { return audioOk_; }
    juce::String audioProblem() const { return audioProblem_; }
    double deviceSampleRate() const;

    // Render settings that carry the Play page's performance controls (without the live pitch-bend gesture).
    realtone::RenderSettings baseRenderSettings() const;

    // The instrument currently chosen on the Play page, or null.
    const realtone::InstrumentEntry* currentInstrument() const;
    juce::String currentInstrumentName() const;

    void saveAudioSettings();

private:
    bool audioOk_ = false;
    juce::String audioProblem_;
};

}  // namespace realtone::studio
