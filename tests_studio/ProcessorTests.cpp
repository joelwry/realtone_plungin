#include "StudioTestSupport.h"
#include "TestSupport.h"

#include "../studio/Parameters.h"
#include "../studio/PresetManager.h"

using namespace realtone;
using namespace realtone::studio;
using namespace fixtures;
namespace P = realtone::studio::param;

namespace
{

void setParam(RealToneProcessor& p, const char* id, float realValue)
{
    auto* prm = p.apvts.getParameter(id);
    prm->setValueNotifyingHost(prm->convertTo0to1(realValue));
}

struct Setup
{
    std::filesystem::path dir;
    RealToneProcessor proc;
    juce::String id;
    explicit Setup(const std::string& name, const juce::String& family = "generic", bool looped = false, bool defaults = true)
    {
        dir = rt_test::scratchDir(name);
        auto files = makeInstrumentFiles(dir, false, 44100.0, looped);
        proc.setPlayConfigDetails(0, 2, 44100.0, 512);
        proc.prepareToPlay(44100.0, 512);
        id = studio_test::loadFixture(proc, files.sfz, family, defaults);
    }
};

double rmsOf(const std::vector<float>& v, int from, int n)
{
    return rms(v.data() + from, n);
}

}  // namespace

RT_TEST(processor_plays_midi_notes_and_releases_them)
{
    Setup s("proc_basic");
    CHECK(s.proc.instruments().status().state == LoadStatus::State::Ready);
    auto on = juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100);
    auto off = juce::MidiMessage::noteOff(1, 60);

    auto first = studio_test::render(s.proc, 40, 512, {{0, on}});
    CHECK(rmsOf(first.l, 4096, 4096) > 0.01);
    CHECK(s.proc.voiceCount.load() >= 1);
    CHECK_NEAR(estimateHz(first.l.data() + 4096, 8192, 44100.0) / midiToHz(60), 1.0, 0.005);

    auto released = studio_test::render(s.proc, 100, 512, {{0, off}});
    CHECK(rmsOf(released.l, released.l.size() - 4096, 4096) < 1e-3);
    CHECK(s.proc.voiceCount.load() == 0);
}

RT_TEST(processor_polyphony_and_velocity)
{
    Setup s("proc_poly");
    auto chord = studio_test::render(s.proc, 20, 512,
                                     {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100)},
                                      {0, juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100)},
                                      {0, juce::MidiMessage::noteOn(1, 67, (juce::uint8) 100)}});
    CHECK(s.proc.voiceCount.load() == 3);

    Setup soft("proc_vel1"), hard("proc_vel2");
    auto a = studio_test::render(soft.proc, 10, 512, {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 30)}});
    auto b = studio_test::render(hard.proc, 10, 512, {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 120)}});
    CHECK(rmsOf(b.l, 2048, 2048) > 1.5 * rmsOf(a.l, 2048, 2048));
}

RT_TEST(processor_pitch_bend_from_midi_and_from_the_automatable_parameter)
{
    Setup s("proc_bend");
    setParam(s.proc, P::bendEnable, 1.0f);
    setParam(s.proc, P::bendRange, 2.0f);  // index 2 = 3 semitones; set explicitly below
    auto* range = s.proc.apvts.getParameter(P::bendRange);
    range->setValueNotifyingHost(range->convertTo0to1(5.0f));  // 12 semitones

    studio_test::render(s.proc, 10, 512, {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100)}});
    auto base = studio_test::render(s.proc, 30, 512);
    const double hz0 = estimateHz(base.l.data() + 4096, 8192, 44100.0);

    auto up = studio_test::render(s.proc, 40, 512, {{0, juce::MidiMessage::pitchWheel(1, 16383)}});
    CHECK_NEAR(estimateHz(up.l.data() + 12288, 8192, 44100.0) / hz0, 2.0, 0.03);
    CHECK(s.proc.incomingPitchBend.load() > 0.99f);

    auto centre = studio_test::render(s.proc, 40, 512, {{0, juce::MidiMessage::pitchWheel(1, 8192)}});
    CHECK_NEAR(estimateHz(centre.l.data() + 12288, 8192, 44100.0) / hz0, 1.0, 0.01);

    // Host automation of the "Pitch Bend" parameter bends too.
    setParam(s.proc, P::pitchBend, -0.5f);
    auto down = studio_test::render(s.proc, 40, 512);
    CHECK_NEAR(estimateHz(down.l.data() + 12288, 8192, 44100.0) / hz0, std::pow(2.0, -6.0 / 12.0), 0.02);
}

RT_TEST(processor_piano_profile_does_not_bend_by_default)
{
    Setup s("proc_piano", "piano", false, true);
    CHECK(s.proc.currentFamily() == realtone::Family::Piano);
    CHECK(s.proc.apvts.getRawParameterValue(P::bendEnable)->load() < 0.5f);
    CHECK(s.proc.apvts.getRawParameterValue(P::pedalEnable)->load() > 0.5f);
    studio_test::render(s.proc, 10, 512, {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100)}});
    auto out = studio_test::render(s.proc, 40, 512, {{0, juce::MidiMessage::pitchWheel(1, 16383)}});
    CHECK_NEAR(estimateHz(out.l.data() + 12288, 8192, 44100.0) / midiToHz(60), 1.0, 0.005);
}

RT_TEST(processor_sustain_pedal_and_modulation_parameters_reach_the_engine)
{
    Setup s("proc_pedal", "piano");
    auto pedal = juce::MidiMessage::controllerEvent(1, 64, 127);
    auto on = juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100);
    auto off = juce::MidiMessage::noteOff(1, 60);
    studio_test::render(s.proc, 4, 512, {{0, pedal}, {10, on}});
    studio_test::render(s.proc, 4, 512, {{0, off}});
    auto held = studio_test::render(s.proc, 40, 512);
    CHECK(rmsOf(held.l, 15000, 4000) > 0.01);   // still sounding after note-off
    auto freed = studio_test::render(s.proc, 100, 512, {{0, juce::MidiMessage::controllerEvent(1, 64, 0)}});
    CHECK(rmsOf(freed.l, freed.l.size() - 4000, 4000) < 1e-3);

    // The on-screen sustain parameter behaves like the pedal.
    setParam(s.proc, P::sustain, 1.0f);
    studio_test::render(s.proc, 4, 512, {{0, on}});
    studio_test::render(s.proc, 4, 512, {{0, off}});
    auto again = studio_test::render(s.proc, 40, 512);
    CHECK(rmsOf(again.l, 15000, 4000) > 0.01);
}

RT_TEST(processor_without_an_instrument_is_silent)
{
    RealToneProcessor p;
    p.setPlayConfigDetails(0, 2, 44100.0, 512);
    p.prepareToPlay(44100.0, 512);
    auto out = studio_test::render(p, 10, 512, {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100)}});
    for (float v : out.l)
        CHECK(v == 0.0f);
}

RT_TEST(processor_not_installed_instrument_never_falls_back_to_another_sound)
{
    Setup s("proc_missing");
    // Pick a catalog instrument whose samples are not on this machine.
    const auto& entries = s.proc.instruments().entries();
    const InstrumentEntry* missing = nullptr;
    for (const auto& e : entries)
        if (!e.external && e.state == InstallState::NotInstalled)
            missing = &e;
    CHECK(missing != nullptr);
    if (missing == nullptr)
        return;
    s.proc.chooseInstrument(juce::String(missing->id), true);
    CHECK(s.proc.instruments().status().state == LoadStatus::State::NotInstalled);
    CHECK(s.proc.instruments().status().message.contains("not installed"));
    auto out = studio_test::render(s.proc, 10, 512, {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100)}});
    double peak = 0;
    for (float v : out.l) peak = std::max<double>(peak, std::fabs(v));
    CHECK(peak == 0.0);
}

RT_TEST(processor_handles_sample_rate_changes_and_block_sizes)
{
    Setup s("proc_sr");
    for (double sr : {44100.0, 48000.0, 96000.0})
        for (int block : {32, 256, 1024})
        {
            s.proc.setPlayConfigDetails(0, 2, sr, block);
            s.proc.prepareToPlay(sr, block);
            auto out = studio_test::render(s.proc, static_cast<int>(sr * 0.4 / block) + 1, block, {{0, juce::MidiMessage::noteOn(1, 69, (juce::uint8) 100)}});
            const double hz = estimateHz(out.l.data() + static_cast<int>(sr * 0.1), 4096, sr);
            CHECK_MSG(std::fabs(hz / 440.0 - 1.0) < 0.006, "sr " << sr << " block " << block << " hz " << hz);
            for (float v : out.l) CHECK(std::isfinite(v));
            s.proc.reset();
        }
}

RT_TEST(processor_supports_stereo_and_mono_outputs_only)
{
    RealToneProcessor p;
    auto layout = [&](const juce::AudioChannelSet& out, const juce::AudioChannelSet& in) {
        juce::AudioProcessor::BusesLayout l;
        l.outputBuses.add(out);
        if (!in.isDisabled()) l.inputBuses.add(in);
        return p.checkBusesLayoutSupported(l);
    };
    CHECK(layout(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::disabled()));
    CHECK(layout(juce::AudioChannelSet::mono(), juce::AudioChannelSet::disabled()));
    CHECK(!layout(juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::disabled()));
    CHECK(p.acceptsMidi());
    CHECK(!p.producesMidi());
    CHECK(p.hasEditor());
}

RT_TEST(processor_state_round_trips_parameters_and_instrument)
{
    Setup a("proc_state");
    setParam(a.proc, P::vibratoDepth, 37.5f);
    setParam(a.proc, P::attack, 120.0f);
    setParam(a.proc, P::width, 1.7f);
    setParam(a.proc, P::level, -6.0f);
    auto* quality = a.proc.apvts.getParameter(P::quality);
    quality->setValueNotifyingHost(quality->convertTo0to1(0.0f));
    a.proc.setCurrentPresetName("My sound");
    a.proc.setSavedEditorSize({1200, 700});

    juce::MemoryBlock saved;
    a.proc.getStateInformation(saved);
    CHECK(saved.getSize() > 50);

    // A new instance (as when a DAW reopens a project) restores everything, including the instrument.
    RealToneProcessor b;
    b.setPlayConfigDetails(0, 2, 44100.0, 512);
    b.prepareToPlay(44100.0, 512);
    b.instruments().addExternalSfz(juce::File(juce::String((a.dir / "test.sfz").string())), "Fixture", "Test", "generic");
    b.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    studio_test::pumpUntil([&] { return b.instruments().status().state == LoadStatus::State::Ready; });

    CHECK_NEAR(b.apvts.getRawParameterValue(P::vibratoDepth)->load(), 37.5, 0.1);
    CHECK_NEAR(b.apvts.getRawParameterValue(P::attack)->load(), 120.0, 1.0);
    CHECK_NEAR(b.apvts.getRawParameterValue(P::width)->load(), 1.7, 0.01);
    CHECK_NEAR(b.apvts.getRawParameterValue(P::level)->load(), -6.0, 0.1);
    CHECK(static_cast<int>(b.apvts.getRawParameterValue(P::quality)->load()) == 0);
    CHECK(b.currentPresetName() == "My sound");
    CHECK(b.savedEditorSize().x == 1200 && b.savedEditorSize().y == 700);
    CHECK(b.instruments().selectedId() == a.id);
    CHECK(b.instruments().status().state == LoadStatus::State::Ready);

    // Restored state must sound the same as the original (instrument really loaded).
    auto out = studio_test::render(b, 20, 512, {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100)}});
    CHECK(rmsOf(out.l, 4096, 4096) > 0.001);
}

RT_TEST(processor_ignores_corrupt_or_foreign_state)
{
    RealToneProcessor p;
    const char junk[] = "this is not a plug-in state";
    p.setStateInformation(junk, sizeof(junk));
    p.setStateInformation(nullptr, 0);

    juce::MemoryBlock foreign;
    juce::XmlElement other("SomethingElse");
    p.copyXmlToBinary(other, foreign);
    p.setStateInformation(foreign.getData(), static_cast<int>(foreign.getSize()));

    // Truncated real state
    juce::MemoryBlock good;
    p.getStateInformation(good);
    p.setStateInformation(good.getData(), static_cast<int>(good.getSize() / 2));
    CHECK(p.apvts.getRawParameterValue(P::level) != nullptr);

    // Parameters unknown to this version are dropped, known ones restored.
    juce::MemoryBlock future;
    {
        auto st = p.apvts.copyState();
        st.setProperty("stateVersion", 99, nullptr);
        juce::ValueTree extra("PARAM");
        extra.setProperty("id", "someNewParam", nullptr);
        extra.setProperty("value", 0.5, nullptr);
        st.appendChild(extra, nullptr);
        if (auto xml = st.createXml())
            p.copyXmlToBinary(*xml, future);
    }
    p.setStateInformation(future.getData(), static_cast<int>(future.getSize()));
    CHECK(p.apvts.getParameter("someNewParam") == nullptr);
}

RT_TEST(presets_save_load_and_delete_user_presets)
{
    Setup s("proc_presets", "bowed");
    auto& pm = s.proc.presets();
    setParam(s.proc, P::vibratoDepth, 44.0f);
    setParam(s.proc, P::release, 2.5f);
    setParam(s.proc, P::pitchBend, 0.7f);   // gesture parameters are not stored in presets

    juce::String err;
    CHECK(!pm.saveUser("bad/name", err));
    CHECK(!pm.saveUser("   ", err));
    CHECK(pm.saveUser("Unit Test Sound", err));

    setParam(s.proc, P::vibratoDepth, 0.0f);
    setParam(s.proc, P::release, 1.0f);

    auto list = pm.listFor(realtone::Family::Bowed);
    const PresetInfo* user = nullptr;
    for (const auto& p : list)
        if (!p.factory && p.name == "Unit Test Sound")
            user = &p;
    CHECK(user != nullptr);
    if (user != nullptr)
    {
        CHECK(pm.load(*user, realtone::Family::Bowed, err));
        CHECK_NEAR(s.proc.apvts.getRawParameterValue(P::vibratoDepth)->load(), 44.0, 0.2);
        CHECK_NEAR(s.proc.apvts.getRawParameterValue(P::release)->load(), 2.5, 0.05);
        CHECK(pm.deleteUser(*user, err));
    }

    // Factory presets exist for the family and apply their changes.
    bool foundFactory = false;
    for (const auto& p : pm.listFor(realtone::Family::Bowed))
        if (p.factory && p.name == "Legato Portamento")
        {
            foundFactory = true;
            CHECK(pm.load(p, realtone::Family::Bowed, err));
            CHECK(s.proc.apvts.getRawParameterValue(P::legato)->load() > 0.5f);
            CHECK(s.proc.apvts.getRawParameterValue(P::glide)->load() > 100.0f);
        }
    CHECK(foundFactory);
    // Piano presets are not offered to violins and vice versa.
    for (const auto& p : pm.listFor(realtone::Family::Bowed))
        CHECK(p.name != "Soft and Close");
}

RT_TEST(parameters_are_automatable_with_stable_ids_and_sane_ranges)
{
    RealToneProcessor p;
    const char* ids[] = {P::level, P::expression, P::modulation, P::pitchBend, P::bendEnable, P::bendRange, P::vibratoDepth, P::vibratoRate,
                         P::glide, P::legato, P::attack, P::release, P::velocitySens, P::velocityCurve, P::humanize, P::width, P::tune,
                         P::quality, P::polyphony, P::sustain, P::pedalEnable};
    CHECK(p.getParameters().size() == static_cast<int>(sizeof(ids) / sizeof(ids[0])));
    for (auto* id : ids)
    {
        auto* prm = p.apvts.getParameter(id);
        CHECK_MSG(prm != nullptr, id);
        if (prm == nullptr) continue;
        CHECK(prm->isAutomatable());
        // Round trip through normalised values and text must not throw or produce NaN.
        for (float n : {0.0f, 0.25f, 0.5f, 1.0f})
        {
            prm->setValueNotifyingHost(n);
            CHECK(std::isfinite(prm->convertFrom0to1(n)));
            CHECK(prm->getText(n, 32).isNotEmpty());
        }
    }
    CHECK(P::bendRangeIndexFor(7.0f) == 4);
    CHECK(P::bendRangeIndexFor(12.0f) == 5);
    CHECK(P::bendRangeIndexFor(2.0f) == 1);
}

RT_TEST(instrument_loading_is_off_the_audio_thread_and_handles_rapid_switching)
{
    Setup s("proc_switch");
    // Select repeatedly and quickly; only the last request may win, and rendering never glitches.
    const auto id = s.id;
    for (int i = 0; i < 6; ++i)
        s.proc.chooseInstrument(id, false);
    for (int i = 0; i < 20; ++i)
        studio_test::render(s.proc, 4, 256, {{0, juce::MidiMessage::noteOn(1, 60 + i % 5, (juce::uint8) 90)}});
    CHECK(studio_test::pumpUntil([&] { return s.proc.instruments().status().state == LoadStatus::State::Ready; }));
    auto out = studio_test::render(s.proc, 20, 512, {{0, juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100)}});
    for (float v : out.l) CHECK(std::isfinite(v));
    CHECK(rmsOf(out.l, 4096, 4096) > 0.01);
}
