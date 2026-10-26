#include "RealToneProcessor.h"

#include "Paths.h"
#include "Settings.h"

#include "../ui/PluginEditor.h"

namespace realtone::studio
{

juce::AudioProcessor::BusesProperties RealToneProcessor::makeBuses()
{
    return BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true);
}

RealToneProcessor::RealToneProcessor()
    : AudioProcessor(makeBuses()),
      apvts(*this, nullptr, "RealToneState", param::createLayout()),
      presets_(apvts)
{
    engine_.prepare(44100.0, 128);
    engine_.setInstrument(nullptr);
    startTimerHz(10);  // drains retired instruments off the audio thread
}

RealToneProcessor::~RealToneProcessor() { stopTimer(); }

void RealToneProcessor::timerCallback() { manager_.collectGarbage(); }

bool RealToneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && layouts.getMainInputChannelSet().isDisabled();
}

void RealToneProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;
    const auto* current = engine_.instrument();
    engine_.prepare(sampleRate, 128);
    engine_.setParams(param::readEngineParams(apvts));
    engine_.setInstrument(current);
    scratch_.setSize(2, juce::jmax(1, samplesPerBlock), false, false, true);
    lastModulationSent_ = -1.0f;
    lastSustainSent_ = false;
}

void RealToneProcessor::chooseInstrument(const juce::String& id, bool applyFamilyDefaults)
{
    if (applyFamilyDefaults)
    {
        if (const auto* e = manager_.find(id))
        {
            family_ = realtone::familyFromString(e->family);
            param::applyProfileDefaults(apvts, realtone::profileFor(family_));
        }
    }
    else if (const auto* e = manager_.find(id))
        family_ = realtone::familyFromString(e->family);

    manager_.selectInstrument(id);
    Settings::get().setString(keys::lastInstrument, id);
}

void RealToneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numFrames = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // New instrument from the loader thread?
    const realtone::Instrument* incoming = nullptr;
    if (manager_.swap().poll(incoming))
        engine_.setInstrument(incoming);

    engine_.setParams(param::readEngineParams(apvts));

    // Notes from the on-screen keyboard join the host's MIDI.
    keyboardState.processNextMidiBuffer(midi, 0, numFrames, true);

    int n = 0;
    auto push = [&](realtone::EngineEvent::Type type, int offset, int d1, int d2) {
        if (n < kMaxEvents)
            events_[static_cast<std::size_t>(n++)] = {juce::jlimit(0, juce::jmax(0, numFrames - 1), offset), type, d1, d2};
    };
    using T = realtone::EngineEvent::Type;

    // Parameter-driven controllers go in as ordinary events at the start of the block.
    const float mod = apvts.getRawParameterValue(param::modulation)->load();
    if (mod != lastModulationSent_)
    {
        lastModulationSent_ = mod;
        push(T::Controller, 0, 1, juce::roundToInt(mod * 127.0f));
    }
    const bool sustain = apvts.getRawParameterValue(param::sustain)->load() > 0.5f;
    if (sustain != lastSustainSent_)
    {
        lastSustainSent_ = sustain;
        push(T::Controller, 0, 64, sustain ? 127 : 0);
    }

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pos = meta.samplePosition;
        if (msg.isNoteOn())
            push(T::NoteOn, pos, msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff())
            push(T::NoteOff, pos, msg.getNoteNumber(), 0);
        else if (msg.isPitchWheel())
        {
            push(T::PitchBend, pos, 0, msg.getPitchWheelValue());
            incomingPitchBend.store((static_cast<float>(msg.getPitchWheelValue()) - 8192.0f) / 8192.0f, std::memory_order_relaxed);
        }
        else if (msg.isController())
        {
            const int cc = msg.getControllerNumber();
            // The pedal and mod wheel parameters are the host-facing equivalents of these two.
            push(T::Controller, pos, cc, msg.getControllerValue());
        }
        else if (msg.isAllNotesOff())
            push(T::AllNotesOff, pos, 0, 0);
        else if (msg.isAllSoundOff())
            push(T::AllSoundOff, pos, 0, 0);
    }
    midi.clear();

    // The engine wants events in time order; hosts normally deliver them that way already, but
    // the merge with the on-screen keyboard and parameter events above can interleave them.
    std::stable_sort(events_.begin(), events_.begin() + n,
                     [](const realtone::EngineEvent& a, const realtone::EngineEvent& b) { return a.offset < b.offset; });

    float* left = nullptr;
    float* right = nullptr;
    if (numChannels >= 2)
    {
        left = buffer.getWritePointer(0);
        right = buffer.getWritePointer(1);
    }
    else if (numChannels == 1)
    {
        if (scratch_.getNumSamples() < numFrames)
            scratch_.setSize(2, numFrames, false, false, true);  // only if the host exceeds its announced block size
        left = buffer.getWritePointer(0);
        right = scratch_.getWritePointer(1);
    }
    else
        return;

    engine_.process(events_.data(), n, left, right, numFrames);

    if (numChannels == 1)
    {
        for (int i = 0; i < numFrames; ++i)
            left[i] = 0.5f * (left[i] + right[i]);
    }

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear(ch, 0, numFrames);

    voiceCount.store(engine_.activeVoices(), std::memory_order_relaxed);
    outputPeak.store(buffer.getMagnitude(0, numFrames), std::memory_order_relaxed);
}

// ------------------------------------------------------------------------------- state

void RealToneProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("stateVersion", kStateVersion, nullptr);
    state.setProperty("instrument", manager_.selectedId(), nullptr);
    state.setProperty("preset", presetName_, nullptr);
    state.setProperty("editorWidth", editorSize_.x, nullptr);
    state.setProperty("editorHeight", editorSize_.y, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void RealToneProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType()))
    {
        logMessage("Ignored unreadable plug-in state (" + juce::String(sizeInBytes) + " bytes).");
        return;
    }
    const int version = xml->getIntAttribute("stateVersion", 1);
    if (version > kStateVersion)
        logMessage("Plug-in state was saved by a newer version (" + juce::String(version) + "); reading what is understood.");

    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid())
        return;

    const auto instrument = tree.getProperty("instrument").toString();
    presetName_ = tree.getProperty("preset", "Default").toString();
    editorSize_ = {juce::jlimit(0, 8000, static_cast<int>(tree.getProperty("editorWidth", 0))),
                   juce::jlimit(0, 8000, static_cast<int>(tree.getProperty("editorHeight", 0)))};

    // Unknown parameter IDs (from other versions) are dropped by replaceState; known ones are restored.
    apvts.replaceState(tree);

    if (instrument.isNotEmpty())
        chooseInstrument(instrument, false);
}

juce::AudioProcessorEditor* RealToneProcessor::createEditor() { return new realtone::ui::PluginEditor(*this); }

}  // namespace realtone::studio

// Entry point used by the VST3 wrapper.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new realtone::studio::RealToneProcessor(); }
