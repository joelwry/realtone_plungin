#include "AppContext.h"

#include "Parameters.h"
#include "Paths.h"
#include "Settings.h"

namespace realtone::studio
{

AppContext::AppContext()
{
    // Restore the previous audio setup if there is one, otherwise take the system defaults.
    std::unique_ptr<juce::XmlElement> saved;
    const auto xmlText = Settings::get().getString("audioDeviceState");
    if (xmlText.isNotEmpty())
        saved = juce::parseXML(xmlText);

    const auto error = deviceManager.initialise(0, 2, saved.get(), true);
    if (error.isNotEmpty())
    {
        audioProblem_ = error;
        logMessage("Audio device: " + error);
    }
    audioOk_ = deviceManager.getCurrentAudioDevice() != nullptr;
    if (!audioOk_ && audioProblem_.isEmpty())
        audioProblem_ = "No audio output device is available.";

    player.setProcessor(&processor);
    deviceManager.addAudioCallback(&player);
    deviceManager.addAudioCallback(&preview);

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled(device.identifier, true);
        deviceManager.addMidiInputDeviceCallback(device.identifier, &player);
    }
}

AppContext::~AppContext()
{
    saveAudioSettings();
    deviceManager.removeAudioCallback(&preview);
    deviceManager.removeAudioCallback(&player);
    player.setProcessor(nullptr);
    deviceManager.closeAudioDevice();
}

void AppContext::saveAudioSettings()
{
    if (auto xml = deviceManager.createStateXml())
        Settings::get().setString("audioDeviceState", xml->toString());
}

double AppContext::deviceSampleRate() const
{
    if (auto* d = deviceManager.getCurrentAudioDevice())
        return d->getCurrentSampleRate();
    return 48000.0;
}

realtone::RenderSettings AppContext::baseRenderSettings() const
{
    realtone::RenderSettings s;
    s.engine = param::readEngineParams(const_cast<juce::AudioProcessorValueTreeState&>(processor.apvts));
    s.engine.extraBendSemitones = 0.0f;  // the live bend gesture is not part of a render
    s.engine.polyphony = 128;
    s.maxVoices = 128;
    return s;
}

const realtone::InstrumentEntry* AppContext::currentInstrument() const
{
    const auto id = processor.instruments().selectedId();
    return id.isEmpty() ? nullptr : processor.instruments().find(id);
}

juce::String AppContext::currentInstrumentName() const
{
    const auto* e = currentInstrument();
    return e != nullptr ? juce::String(e->name) : juce::String();
}

}  // namespace realtone::studio
