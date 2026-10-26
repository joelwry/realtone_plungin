#include "Fixtures.h"
#include "TestSupport.h"

#include "realtone/SamplerEngine.h"

#include <random>

using namespace realtone;
using namespace fixtures;

namespace {

struct Rig
{
    std::filesystem::path dir;
    TestInstrument files;
    std::shared_ptr<Instrument> inst;
    SamplerEngine eng;

    explicit Rig(const std::string& name, double sr = 44100.0, bool release = false, bool looped = false, int voices = 64)
    {
        dir = rt_test::scratchDir(name);
        files = makeInstrumentFiles(dir, release, 44100.0, looped);
        inst = loadOrDie(files);
        eng.prepare(sr, voices);
        EngineParams p;
        p.pitchBendEnabled = true;
        p.pitchBendRange = 2.0f;
        eng.setParams(p);
        eng.setInstrument(inst.get());
    }
};

double peakOf(const Rendered& r)
{
    double p = 0;
    for (float s : r.l) p = std::max<double>(p, std::fabs(s));
    for (float s : r.r) p = std::max<double>(p, std::fabs(s));
    return p;
}

bool allFinite(const Rendered& r)
{
    for (float s : r.l) if (!std::isfinite(s)) return false;
    for (float s : r.r) if (!std::isfinite(s)) return false;
    return true;
}

}  // namespace

RT_TEST(engine_note_on_produces_sound)
{
    Rig rig("eng_basic");
    CHECK(rig.inst != nullptr);
    CHECK(rig.inst->report.complete());
    auto out = run(rig.eng, 22050, {noteOn(60, 100)});
    CHECK(allFinite(out));
    CHECK(peakOf(out) > 0.01);
    CHECK(rig.eng.activeVoices() > 0);
}

RT_TEST(engine_plays_correct_pitch_at_several_sample_rates)
{
    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        Rig rig("eng_pitch", sr);
        for (int note : {48, 60, 72, 84})
        {
            rig.eng.reset();
            auto out = run(rig.eng, static_cast<int>(sr * 0.5), {noteOn(note, 110)});
            const double hz = estimateHz(out.l.data() + static_cast<int>(sr * 0.1), 8192, sr);
            CHECK_NEAR(hz / midiToHz(note), 1.0, 0.004);
        }
    }
}

RT_TEST(engine_note_between_sample_roots_is_tuned)
{
    // Key 61 is one semitone above the sample root 60 -> pitched by interpolation.
    Rig rig("eng_tune");
    auto out = run(rig.eng, 22050, {noteOn(61, 100)});
    const double hz = estimateHz(out.l.data() + 4000, 8192, 44100.0);
    CHECK_NEAR(hz / midiToHz(61), 1.0, 0.004);
}

RT_TEST(engine_quality_modes_all_in_tune)
{
    for (auto q : {Quality::Performance, Quality::Balanced, Quality::High})
    {
        Rig rig("eng_quality");
        auto p = rig.eng.params();
        p.quality = q;
        rig.eng.setParams(p);
        auto out = run(rig.eng, 22050, {noteOn(61, 100)});
        const double hz = estimateHz(out.l.data() + 4000, 8192, 44100.0);
        CHECK_NEAR(hz / midiToHz(61), 1.0, 0.006);
        CHECK(allFinite(out));
    }
}

RT_TEST(engine_polyphony_adds_voices)
{
    Rig rig("eng_poly");
    run(rig.eng, 4096, {noteOn(60, 100), noteOn(64, 100), noteOn(67, 100)});
    CHECK(rig.eng.activeVoices() == 3);
}

RT_TEST(engine_note_off_releases_to_silence)
{
    Rig rig("eng_release");
    std::vector<EngineEvent> ev = {noteOn(60, 100), noteOff(60, 8000)};
    auto out = run(rig.eng, 44100, ev);
    const int tail = 44100 - 4000;
    // Released at 8000 with a 0.3 s release: well below -60 dB after one second.
    CHECK(rms(out.l.data() + tail, 4000) < 0.0005);
    CHECK(rms(out.l.data() + 4000, 2000) > 0.01);
    CHECK(rig.eng.activeVoices() == 0);
}

RT_TEST(engine_velocity_changes_level_monotonically)
{
    double prev = 0;
    for (int vel : {20, 40, 63, 90, 127})
    {
        Rig rig("eng_vel");
        auto out = run(rig.eng, 8192, {noteOn(60, vel)});
        const double level = rms(out.l.data() + 2000, 4000);
        // The layer split at 64 means the loudest layer is also louder as recorded.
        CHECK_MSG(level > prev, "velocity " << vel);
        prev = level;
    }
}

RT_TEST(engine_velocity_sensitivity_zero_flattens_dynamics)
{
    Rig rig("eng_vel0");
    auto p = rig.eng.params();
    p.velocitySensitivity = 0.0f;
    rig.eng.setParams(p);
    auto a = run(rig.eng, 8192, {noteOn(60, 70)});
    rig.eng.reset();
    auto b = run(rig.eng, 8192, {noteOn(60, 127)});
    CHECK_NEAR(rms(a.l.data() + 2000, 4000) / rms(b.l.data() + 2000, 4000), 1.0, 0.05);
}

RT_TEST(engine_pitch_bend_ranges_and_return_to_centre)
{
    for (float range : {2.0f, 7.0f, 12.0f})
    {
        Rig rig("eng_bend");
        auto p = rig.eng.params();
        p.pitchBendRange = range;
        rig.eng.setParams(p);

        run(rig.eng, 4096, {noteOn(60, 100)});
        auto base = run(rig.eng, 8192);
        const double hz0 = estimateHz(base.l.data(), 8192, 44100.0);

        auto up = run(rig.eng, 16384, {bend(16383)});
        const double hzUp = estimateHz(up.l.data() + 8192, 8192, 44100.0);
        CHECK_NEAR(hzUp / hz0, std::pow(2.0, range / 12.0), 0.01);

        auto down = run(rig.eng, 16384, {bend(0)});
        const double hzDown = estimateHz(down.l.data() + 8192, 8192, 44100.0);
        CHECK_NEAR(hzDown / hz0, std::pow(2.0, -range / 12.0), 0.01);

        auto centre = run(rig.eng, 16384, {bend(8192)});
        const double hzC = estimateHz(centre.l.data() + 8192, 8192, 44100.0);
        CHECK_NEAR(hzC / hz0, 1.0, 0.003);
        CHECK_NEAR(rig.eng.pitchBendSemitones(), 0.0, 1e-3);
    }
}

RT_TEST(engine_pitch_bend_can_be_disabled_for_piano_like_profiles)
{
    Rig rig("eng_bend_off");
    auto p = paramsFromProfile(profileFor(Family::Piano));
    CHECK(!p.pitchBendEnabled);
    rig.eng.setParams(p);
    run(rig.eng, 4096, {noteOn(60, 100)});
    auto out = run(rig.eng, 16384, {bend(16383)});
    const double hz = estimateHz(out.l.data() + 8192, 8192, 44100.0);
    CHECK_NEAR(hz / midiToHz(60), 1.0, 0.004);

    // Creative option: explicitly enabled bending works on the same instrument.
    p.pitchBendEnabled = true;
    rig.eng.setParams(p);
    auto out2 = run(rig.eng, 16384);
    const double hz2 = estimateHz(out2.l.data() + 8192, 8192, 44100.0);
    CHECK(hz2 > hz * 1.05);
}

RT_TEST(engine_host_bend_parameter_adds_to_midi_bend)
{
    Rig rig("eng_extra_bend");
    auto p = rig.eng.params();
    p.pitchBendRange = 12.0f;
    p.extraBendSemitones = 0.5f;  // half of range = 6 semitones
    rig.eng.setParams(p);
    run(rig.eng, 4096, {noteOn(60, 100)});
    auto out = run(rig.eng, 16384);
    CHECK_NEAR(estimateHz(out.l.data() + 8192, 8192, 44100.0) / midiToHz(60), std::pow(2.0, 6.0 / 12.0), 0.01);
}

RT_TEST(engine_vibrato_modulates_pitch_from_mod_wheel)
{
    Rig rig("eng_vib");
    auto p = rig.eng.params();
    p.modWheelVibratoCents = 100.0f;
    p.vibratoRateHz = 5.0f;
    rig.eng.setParams(p);
    run(rig.eng, 2048, {noteOn(60, 100), cc(1, 127)});
    // Sample the instantaneous pitch at several points over one second; vibrato must move it.
    double lo = 1e9, hi = 0;
    for (int i = 0; i < 20; ++i)
    {
        auto out = run(rig.eng, 2048);
        const double hz = estimateHz(out.l.data(), 2048, 44100.0, 100, 1000);
        if (hz > 0) { lo = std::min(lo, hz); hi = std::max(hi, hz); }
    }
    CHECK(hi / lo > 1.02);
    // Without mod wheel the pitch is stable.
    Rig steady("eng_vib2");
    run(steady.eng, 2048, {noteOn(60, 100)});
    double lo2 = 1e9, hi2 = 0;
    for (int i = 0; i < 10; ++i)
    {
        auto out = run(steady.eng, 2048);
        const double hz = estimateHz(out.l.data(), 2048, 44100.0, 100, 1000);
        if (hz > 0) { lo2 = std::min(lo2, hz); hi2 = std::max(hi2, hz); }
    }
    CHECK(hi2 / lo2 < 1.005);
}

RT_TEST(engine_sustain_pedal_holds_released_notes)
{
    Rig rig("eng_pedal");
    auto p = rig.eng.params();
    p.sustainPedalEnabled = true;
    rig.eng.setParams(p);
    // Pedal down, press and release, wait one second: still sounding.
    auto held = run(rig.eng, 44100, {cc(64, 127), noteOn(60, 100), noteOff(60, 4000)});
    CHECK(rms(held.l.data() + 40000, 3000) > 0.01);
    CHECK(rig.eng.activeVoices() == 1);
    // Pedal up releases it.
    auto freed = run(rig.eng, 44100, {cc(64, 0)});
    CHECK(rms(freed.l.data() + 40000, 3000) < 0.001);
    CHECK(rig.eng.activeVoices() == 0);
}

RT_TEST(engine_pedal_ignored_when_instrument_does_not_support_it)
{
    Rig rig("eng_nopedal");
    auto p = rig.eng.params();
    p.sustainPedalEnabled = false;
    rig.eng.setParams(p);
    auto out = run(rig.eng, 44100, {cc(64, 127), noteOn(60, 100), noteOff(60, 4000)});
    CHECK(rms(out.l.data() + 40000, 3000) < 0.001);
}

RT_TEST(engine_round_robin_cycles_through_variations)
{
    Rig rig("eng_rr");
    auto p = rig.eng.params();
    p.velocitySensitivity = 0.0f;
    rig.eng.setParams(p);
    // The fixture's two round-robin samples differ in level (0.6 vs 0.4), so the cycle is measurable.
    std::vector<double> levels;
    for (int i = 0; i < 4; ++i)
    {
        rig.eng.handleEvent(noteOn(60, 110));
        auto out = run(rig.eng, 6000);
        levels.push_back(rms(out.l.data() + 1500, 3000));
        rig.eng.handleEvent(noteOff(60));
        run(rig.eng, 30000);
    }
    CHECK(levels[0] > levels[1] * 1.3);
    CHECK_NEAR(levels[0], levels[2], levels[0] * 0.05);
    CHECK_NEAR(levels[1], levels[3], levels[1] * 0.05);
}

RT_TEST(engine_stealing_never_exceeds_polyphony_and_stays_finite)
{
    Rig rig("eng_steal", 44100.0, false, false, 4);
    std::vector<EngineEvent> ev;
    for (int i = 0; i < 40; ++i)
        ev.push_back(noteOn(40 + i, 100, i * 300));
    auto out = run(rig.eng, 44100, ev, 128);
    CHECK(allFinite(out));
    // Stolen voices fade within a few ms, so live voices settle at the limit.
    CHECK(rig.eng.activeVoices() <= 4 + 16);
    int live = 0;
    run(rig.eng, 2048);
    live = rig.eng.activeVoices();
    CHECK(live <= 4);
}

RT_TEST(engine_voice_stealing_has_no_clicks)
{
    Rig rig("eng_steal_click", 44100.0, false, true, 2);
    std::vector<EngineEvent> ev = {noteOn(60, 100), noteOn(64, 100, 5000), noteOn(67, 100, 10000), noteOn(71, 100, 15000)};
    auto out = run(rig.eng, 30000, ev, 64);
    double maxJump = 0;
    for (std::size_t i = 1; i < out.l.size(); ++i)
        maxJump = std::max<double>(maxJump, std::fabs(out.l[i] - out.l[i - 1]));
    CHECK_MSG(maxJump < 0.35, "largest sample-to-sample jump " << maxJump);
}

RT_TEST(engine_random_event_storm_stays_finite)
{
    Rig rig("eng_fuzz", 48000.0, true, false, 16);
    std::mt19937 rng(7);
    std::vector<EngineEvent> ev;
    for (int i = 0; i < 4000; ++i)
    {
        const int at = static_cast<int>(rng() % 200000);
        switch (rng() % 7)
        {
            case 0: case 1: case 2: ev.push_back(noteOn(static_cast<int>(rng() % 128), 1 + static_cast<int>(rng() % 127), at)); break;
            case 3: case 4: ev.push_back(noteOff(static_cast<int>(rng() % 128), at)); break;
            case 5: ev.push_back(bend(static_cast<int>(rng() % 16384), at)); break;
            default: ev.push_back(cc(static_cast<int>(rng() % 128), static_cast<int>(rng() % 128), at)); break;
        }
    }
    std::stable_sort(ev.begin(), ev.end(), [](const EngineEvent& a, const EngineEvent& b) { return a.offset < b.offset; });
    auto out = run(rig.eng, 200000, ev, 480);
    CHECK(allFinite(out));
    CHECK(peakOf(out) < 16.0);
    rig.eng.handleEvent({0, EngineEvent::Type::AllSoundOff, 0, 0});
    run(rig.eng, 4096);
    CHECK(rig.eng.activeVoices() == 0);
}

RT_TEST(engine_looped_sample_sustains_far_beyond_its_length)
{
    Rig rig("eng_loop", 44100.0, false, true);
    auto out = run(rig.eng, 44100 * 4, {noteOn(60, 100)});
    // The source is 1 s long; with the loop the note is still strong at 3.5 s.
    CHECK(rms(out.l.data() + 44100 * 3 + 20000, 4000) > 0.05);
    const double hz = estimateHz(out.l.data() + 44100 * 3, 8192, 44100.0);
    CHECK_NEAR(hz / midiToHz(60), 1.0, 0.004);
}

RT_TEST(engine_unlooped_sample_ends_without_click)
{
    Rig rig("eng_noloop");
    auto out = run(rig.eng, 44100 * 4, {noteOn(60, 100)});
    CHECK(rig.eng.activeVoices() == 0);  // 3 s sample has finished
    CHECK(rms(out.l.data() + 44100 * 3 + 2000, 4000) < 1e-4);
}

RT_TEST(engine_release_trigger_regions_fire_on_note_off)
{
    Rig rig("eng_reltrig", 44100.0, true);
    CHECK(rig.inst->hasReleaseRegions());
    auto out = run(rig.eng, 30000, {noteOn(60, 100), noteOff(60, 10000)});
    // The attack sample alone would decay smoothly; the release sample adds a new burst after the note-off.
    auto without = Rig("eng_reltrig_b", 44100.0, false);
    auto ref = run(without.eng, 30000, {noteOn(60, 100), noteOff(60, 10000)});
    double extra = 0;
    for (int i = 10000; i < 14000; ++i)
        extra += std::fabs(out.l[static_cast<std::size_t>(i)]) - std::fabs(ref.l[static_cast<std::size_t>(i)]);
    CHECK(extra > 0.5);
}

RT_TEST(engine_master_gain_and_expression_scale_output)
{
    Rig rig("eng_gain");
    auto a = run(rig.eng, 8192, {noteOn(60, 100)});
    const double base = rms(a.l.data() + 2000, 4000);

    Rig rig2("eng_gain2");
    auto p = rig2.eng.params();
    p.masterGainDb = -6.0f;
    rig2.eng.setParams(p);
    auto b = run(rig2.eng, 8192, {noteOn(60, 100)});
    CHECK_NEAR(rms(b.l.data() + 2000, 4000) / base, std::pow(10.0, -6.0 / 20.0), 0.02);

    Rig rig3("eng_gain3");
    auto c = run(rig3.eng, 8192, {cc(11, 64), noteOn(60, 100)});
    CHECK_NEAR(rms(c.l.data() + 2000, 4000) / base, 64.0 / 127.0, 0.03);
}

RT_TEST(engine_stereo_width_zero_is_mono)
{
    Rig rig("eng_width");
    auto p = rig.eng.params();
    p.stereoWidth = 0.0f;
    rig.eng.setParams(p);
    auto out = run(rig.eng, 4096, {noteOn(60, 100)});
    for (std::size_t i = 0; i < out.l.size(); ++i)
        CHECK(std::fabs(out.l[i] - out.r[i]) < 1e-6);
}

RT_TEST(engine_monophonic_legato_glides_between_notes)
{
    Rig rig("eng_glide");
    auto p = rig.eng.params();
    p.monoLegato = true;
    p.glideMs = 200.0f;
    rig.eng.setParams(p);
    run(rig.eng, 8192, {noteOn(60, 100)});
    // New overlapping note: the old one hands over, the new one starts at the old pitch.
    auto start = run(rig.eng, 2048, {noteOn(67, 100)});
    const double hzStart = estimateHz(start.l.data() + 256, 1536, 44100.0, 100, 1500);
    auto end = run(rig.eng, 44100);
    const double hzEnd = estimateHz(end.l.data() + 30000, 8192, 44100.0, 100, 1500);
    CHECK(hzStart < midiToHz(67) * 0.95);
    CHECK_NEAR(hzEnd / midiToHz(67), 1.0, 0.01);
    CHECK(rig.eng.activeVoices() <= 2);
}

RT_TEST(engine_changing_instrument_silences_old_voices)
{
    Rig rig("eng_swap");
    run(rig.eng, 4096, {noteOn(60, 100)});
    CHECK(rig.eng.activeVoices() == 1);
    rig.eng.setInstrument(nullptr);
    CHECK(rig.eng.activeVoices() == 0);
    auto out = run(rig.eng, 2048, {noteOn(60, 100)});
    CHECK(peakOf(out) == 0.0);  // no instrument: silence, never a substitute sound
}

RT_TEST(engine_deterministic_with_same_seed)
{
    auto render = [](std::uint32_t seed) {
        Rig rig("eng_det");
        auto p = rig.eng.params();
        p.humanizeVelocity = 1.0f;
        rig.eng.setParams(p);
        rig.eng.seedRandom(seed);
        return run(rig.eng, 20000, {noteOn(60, 90), noteOn(64, 90, 3000), noteOn(67, 90, 6000)});
    };
    auto a = render(5), b = render(5), c = render(6);
    CHECK(a.l == b.l);
    CHECK(a.l != c.l);
}
