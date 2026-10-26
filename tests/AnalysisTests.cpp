#include "Fixtures.h"
#include "TestSupport.h"

#include "realtone/MidiFile.h"
#include "realtone/PitchAnalysis.h"
#include "realtone/Renderer.h"

#include <random>

using namespace realtone;
using namespace fixtures;

namespace {

struct Note { double start, dur; int midi; double vibratoCents = 0; double amp = 0.5; };

AudioClip melody(const std::vector<Note>& notes, double rate = 44100.0, double tail = 0.3)
{
    double end = 0;
    for (auto& n : notes) end = std::max(end, n.start + n.dur);
    AudioClip c;
    c.sampleRate = rate;
    std::vector<float> x(static_cast<std::size_t>((end + tail) * rate), 0.0f);
    for (auto& n : notes)
    {
        double phase = 0;
        const auto i0 = static_cast<std::size_t>(n.start * rate);
        const auto len = static_cast<std::size_t>(n.dur * rate);
        for (std::size_t i = 0; i < len && i0 + i < x.size(); ++i)
        {
            const double t = static_cast<double>(i) / rate;
            const double cents = n.vibratoCents * std::sin(2 * kPi * 5.5 * t);
            const double hz = midiToHz(n.midi + cents / 100.0);
            phase += 2 * kPi * hz / rate;
            const double env = std::min(1.0, t / 0.015) * std::min(1.0, (n.dur - t) / 0.02) * std::exp(-0.4 * t);
            x[i0 + i] += static_cast<float>(n.amp * env * (std::sin(phase) + 0.5 * std::sin(2 * phase) + 0.25 * std::sin(3 * phase)) / 1.75);
        }
    }
    c.channels.push_back(std::move(x));
    return c;
}

}  // namespace

RT_TEST(analysis_detects_a_clean_monophonic_melody)
{
    const std::vector<Note> notes = {{0.10, 0.40, 60}, {0.60, 0.40, 62}, {1.10, 0.40, 64}, {1.60, 0.40, 65}, {2.10, 0.50, 67}};
    auto rep = analyseMonophonic(melody(notes));
    CHECK_MSG(rep.ok, rep.error);
    CHECK(rep.notes.size() == notes.size());
    for (std::size_t i = 0; i < std::min(rep.notes.size(), notes.size()); ++i)
    {
        CHECK_MSG(rep.notes[i].midiNote == notes[i].midi, "note " << i << " got " << rep.notes[i].midiNote);
        CHECK_NEAR(rep.notes[i].startSeconds, notes[i].start, 0.05);
        CHECK_NEAR(rep.notes[i].endSeconds, notes[i].start + notes[i].dur, 0.06);
        CHECK(rep.notes[i].confidence > 0.8f);
    }
    CHECK(!rep.likelyPolyphonic);
    CHECK(rep.meanConfidence > 0.8);
}

RT_TEST(analysis_splits_repeated_and_legato_notes)
{
    // Repeated pitch separated by a short gap, then an immediate legato step.
    const std::vector<Note> notes = {{0.10, 0.30, 69}, {0.50, 0.30, 69}, {0.90, 0.30, 69}, {1.20, 0.40, 71}};
    auto rep = analyseMonophonic(melody(notes));
    CHECK_MSG(rep.ok, rep.error);
    CHECK_MSG(rep.notes.size() == 4, "got " << rep.notes.size());
    if (rep.notes.size() == 4)
    {
        CHECK(rep.notes[2].midiNote == 69 && rep.notes[3].midiNote == 71);
        CHECK_NEAR(rep.notes[3].startSeconds, 1.2, 0.06);
    }
}

RT_TEST(analysis_keeps_vibrato_inside_one_note_and_records_contour)
{
    const std::vector<Note> notes = {{0.1, 1.0, 64, 40.0}};
    auto rep = analyseMonophonic(melody(notes));
    CHECK_MSG(rep.ok, rep.error);
    CHECK(rep.notes.size() == 1);
    if (!rep.notes.empty())
    {
        CHECK(rep.notes[0].midiNote == 64);
        float lo = 1e9f, hi = -1e9f;
        for (float c : rep.notes[0].contourCents) { lo = std::min(lo, c); hi = std::max(hi, c); }
        CHECK(hi - lo > 40.0f);   // the +/-40 cent vibrato shows in the contour
    }
}

RT_TEST(analysis_estimates_velocity_from_level)
{
    const std::vector<Note> notes = {{0.1, 0.4, 60, 0, 0.5}, {0.7, 0.4, 62, 0, 0.05}};
    auto rep = analyseMonophonic(melody(notes));
    CHECK(rep.ok);
    CHECK(rep.notes.size() == 2);
    if (rep.notes.size() == 2)
        CHECK(rep.notes[0].velocity > rep.notes[1].velocity + 20);
}

RT_TEST(analysis_warns_on_polyphonic_and_noisy_sources)
{
    // Three simultaneous unrelated pitches.
    AudioClip chord = melody({{0.1, 2.0, 60}, {0.1, 2.0, 64}, {0.1, 2.0, 68}});
    for (std::size_t i = 0; i < chord.channels[0].size(); ++i)
        chord.channels[0][i] *= 0.6f;
    auto rep = analyseMonophonic(chord);
    // Either nothing usable is found or the report must carry a clear warning.
    CHECK(!rep.ok || !rep.warnings.empty() || rep.likelyPolyphonic);

    std::mt19937 rng(1);
    std::normal_distribution<float> nd(0.0f, 0.2f);
    AudioClip noise;
    noise.sampleRate = 44100.0;
    noise.channels.emplace_back(44100 * 2);
    for (auto& s : noise.channels[0]) s = nd(rng);
    auto nrep = analyseMonophonic(noise);
    CHECK(!nrep.ok || nrep.likelyPolyphonic || !nrep.warnings.empty());
    if (nrep.ok)
        CHECK(nrep.meanConfidence < 0.9);
}

RT_TEST(analysis_rejects_silence_and_short_input)
{
    AudioClip silence;
    silence.sampleRate = 44100.0;
    silence.channels.emplace_back(44100, 0.0f);
    auto a = analyseMonophonic(silence);
    CHECK(!a.ok && !a.error.empty());

    AudioClip tiny;
    tiny.sampleRate = 44100.0;
    tiny.channels.emplace_back(100, 0.1f);
    CHECK(!analyseMonophonic(tiny).ok);
}

RT_TEST(analysis_handles_other_sample_rates_and_stereo)
{
    for (double sr : {22050.0, 48000.0, 96000.0})
    {
        auto clip = melody({{0.1, 0.5, 57}, {0.7, 0.5, 60}}, sr);
        clip.channels.push_back(clip.channels[0]);  // stereo
        auto rep = analyseMonophonic(clip);
        CHECK_MSG(rep.ok, "rate " << sr << ": " << rep.error);
        CHECK(rep.notes.size() == 2);
        if (rep.notes.size() == 2)
            CHECK(rep.notes[0].midiNote == 57 && rep.notes[1].midiNote == 60);
    }
}

RT_TEST(analysis_supports_cancellation)
{
    std::atomic<bool> cancel{true};
    auto rep = analyseMonophonic(melody({{0.1, 1.0, 60}}), {}, {}, &cancel);
    CHECK(!rep.ok);
}

RT_TEST(audio_to_instrument_end_to_end_keeps_the_melody)
{
    const std::vector<Note> notes = {{0.10, 0.45, 60}, {0.65, 0.45, 64}, {1.20, 0.45, 67}, {1.75, 0.60, 72}};
    auto rep = analyseMonophonic(melody(notes));
    CHECK(rep.ok);

    auto seq = notesToSequence(rep.notes);
    CHECK(seq.noteCount == 4);

    const auto dir = rt_test::scratchDir("e2e_inst");
    auto inst = loadOrDie(makeInstrumentFiles(dir));
    RenderSettings s;
    s.sampleRate = 44100.0;
    s.engine.pitchBendEnabled = true;
    s.engine.pitchBendRange = 2.0f;
    auto out = renderSequence(inst, seq, s);
    CHECK_MSG(out.ok, out.error);
    CHECK(out.clip.durationSeconds() >= 2.2);

    // Re-analysing the rendered result recovers the same notes: the conversion is faithful.
    AudioClip mono;
    mono.sampleRate = out.clip.sampleRate;
    mono.channels.push_back(out.clip.channels[0]);
    auto again = analyseMonophonic(mono);
    CHECK_MSG(again.ok, again.error);
    CHECK(again.notes.size() == 4);
    if (again.notes.size() == 4)
        for (std::size_t i = 0; i < 4; ++i)
            CHECK(again.notes[i].midiNote == notes[i].midi);
}

RT_TEST(notes_can_be_disabled_before_rendering)
{
    auto rep = analyseMonophonic(melody({{0.1, 0.4, 60}, {0.6, 0.4, 62}, {1.1, 0.4, 64}}));
    CHECK(rep.ok && rep.notes.size() == 3);
    rep.notes[1].enabled = false;
    auto seq = notesToSequence(rep.notes);
    CHECK(seq.noteCount == 2);
}

RT_TEST(analysis_splits_a_semitone_step_when_the_old_note_rings_over)
{
    // Second note enters while the first is still decaying (sampled instruments with long releases).
    AudioClip c;
    c.sampleRate = 44100.0;
    c.channels.emplace_back(static_cast<std::size_t>(44100 * 3), 0.0f);
    auto add = [&](double start, double dur, double fade, int midi, double amp) {
        double phase = 0;
        const double hz = midiToHz(midi);
        for (std::size_t i = 0; i < static_cast<std::size_t>((dur + fade) * 44100); ++i)
        {
            const double t = static_cast<double>(i) / 44100.0;
            phase += 2 * kPi * hz / 44100.0;
            const double env = std::min(1.0, t / 0.02) * (t < dur ? 1.0 : std::max(0.0, 1.0 - (t - dur) / fade));
            const auto idx = static_cast<std::size_t>(start * 44100) + i;
            if (idx < c.channels[0].size())
                c.channels[0][idx] += static_cast<float>(amp * env * (std::sin(phase) + 0.4 * std::sin(2 * phase)));
        }
    };
    add(0.1, 1.0, 0.15, 72, 0.4);  // rings on briefly after its nominal end
    add(1.1, 1.0, 0.3, 71, 0.4);
    auto rep = analyseMonophonic(c);
    CHECK(rep.ok);
    CHECK_MSG(rep.notes.size() == 2, "got " << rep.notes.size());
    if (rep.notes.size() == 2)
    {
        CHECK(rep.notes[0].midiNote == 72 && rep.notes[1].midiNote == 71);
        CHECK_NEAR(rep.notes[1].startSeconds, 1.1, 0.08);
    }
}

RT_TEST(analysis_pitches_high_notes_accurately)
{
    // Decimated analysis loses resolution above ~1 kHz unless the period is refined at full rate.
    auto rep = analyseMonophonic(melody({{0.1, 0.4, 84}, {0.6, 0.4, 86}, {1.1, 0.4, 88}, {1.6, 0.4, 91}}));
    CHECK(rep.ok);
    CHECK(rep.notes.size() == 4);
    const int expect[] = {84, 86, 88, 91};
    for (std::size_t i = 0; i < std::min<std::size_t>(rep.notes.size(), 4); ++i)
    {
        CHECK(rep.notes[i].midiNote == expect[i]);
        CHECK(std::fabs(rep.notes[i].meanCents) < 12.0f);
    }
}
