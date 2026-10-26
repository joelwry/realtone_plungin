#pragma once

#include "Fixtures.h"

#include "../studio/RealToneProcessor.h"
#include "../studio/Settings.h"

#include <juce_events/juce_events.h>

namespace studio_test
{

inline void useIsolatedSettings(const std::filesystem::path& dir)
{
    realtone::studio::Settings::get().useFile(juce::File(juce::String(dir.string())).getChildFile("settings.xml"));
}

// Runs the message loop until `done` returns true or the timeout passes.
template <typename F>
bool pumpUntil(F done, int timeoutMs = 20000)
{
    const auto end = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(timeoutMs);
    while (!done())
    {
        if (juce::Time::getMillisecondCounter() > end)
            return false;
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
    return true;
}

// Registers the fixture instrument as an imported library and waits for it to load.
inline juce::String loadFixture(realtone::studio::RealToneProcessor& p, const std::filesystem::path& sfz, const juce::String& family = "generic",
                                bool applyDefaults = true)
{
    const auto id = p.instruments().addExternalSfz(juce::File(juce::String(sfz.string())), "Fixture", "Test", family);
    p.chooseInstrument(id, applyDefaults);
    pumpUntil([&] {
        const auto s = p.instruments().status().state;
        return s == realtone::studio::LoadStatus::State::Ready || s == realtone::studio::LoadStatus::State::ReadyPartial ||
               s == realtone::studio::LoadStatus::State::Failed;
    });
    return id;
}

// Delivers the pending instrument to the audio path and renders `blocks` blocks of `block` frames.
struct Out
{
    std::vector<float> l, r;
};

inline Out render(realtone::studio::RealToneProcessor& p, int blocks, int block, const std::vector<std::pair<int, juce::MidiMessage>>& midiAtBlock0 = {},
                  int firstBlockIndexForMidi = 0)
{
    Out out;
    juce::AudioBuffer<float> buf(2, block);
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        juce::MidiBuffer midi;
        if (b == firstBlockIndexForMidi)
            for (const auto& [pos, msg] : midiAtBlock0)
                midi.addEvent(msg, pos);
        p.processBlock(buf, midi);
        out.l.insert(out.l.end(), buf.getReadPointer(0), buf.getReadPointer(0) + block);
        out.r.insert(out.r.end(), buf.getReadPointer(1), buf.getReadPointer(1) + block);
    }
    return out;
}

}  // namespace studio_test
