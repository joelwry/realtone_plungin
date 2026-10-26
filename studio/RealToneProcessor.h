#pragma once

#include "InstrumentManager.h"
#include "Parameters.h"
#include "PresetManager.h"

#include "realtone/SamplerEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

namespace realtone::studio
{

// The sampler as an audio processor. Used as-is inside the VST3 and, through the same class,
// by the standalone application for live preview, so both behave identically.
class RealToneProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int kStateVersion = 1;

    RealToneProcessor();
    ~RealToneProcessor() override;

    // AudioProcessor
    const juce::String getName() const override { return "RealTone Studio"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Shared pieces the UI works with
    juce::AudioProcessorValueTreeState apvts;
    InstrumentManager& instruments() { return manager_; }
    const InstrumentManager& instruments() const { return manager_; }
    PresetManager& presets() { return presets_; }
    juce::MidiKeyboardState keyboardState;

    // Chooses an instrument and, unless restoring saved state, moves the performance controls
    // to the defaults that suit its family.
    void chooseInstrument(const juce::String& id, bool applyFamilyDefaults);

    // Meters for the UI (written by the audio thread)
    std::atomic<float> incomingPitchBend{0.0f};  // normalised MIDI bend, -1..1
    std::atomic<int> voiceCount{0};
    std::atomic<float> outputPeak{0.0f};

    juce::String currentPresetName() const { return presetName_; }
    void setCurrentPresetName(const juce::String& n) { presetName_ = n; }

    // Size of the plug-in editor, remembered in the saved state
    juce::Point<int> savedEditorSize() const { return editorSize_; }
    void setSavedEditorSize(juce::Point<int> s) { editorSize_ = s; }

    // Test hook: direct engine access is not exposed; this renders through processBlock.
    realtone::Family currentFamily() const { return family_; }

private:
    void timerCallback() override;
    static BusesProperties makeBuses();

    InstrumentManager manager_;
    PresetManager presets_;
    realtone::SamplerEngine engine_;
    realtone::Family family_ = realtone::Family::Generic;

    static constexpr int kMaxEvents = 4096;
    std::array<realtone::EngineEvent, kMaxEvents> events_{};
    juce::AudioBuffer<float> scratch_;
    float lastModulationSent_ = -1.0f;
    bool lastSustainSent_ = false;
    double sampleRate_ = 44100.0;

    juce::String presetName_ = "Default";
    juce::Point<int> editorSize_{0, 0};
    juce::String pendingInstrumentFromState_;
    juce::String lastLoadedFamilyId_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RealToneProcessor)
};

}  // namespace realtone::studio
