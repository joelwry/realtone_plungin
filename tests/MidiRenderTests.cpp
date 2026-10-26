#include "Fixtures.h"
#include "TestSupport.h"

#include "SmfBuilder.h"

#include "realtone/MidiFile.h"
#include "realtone/Renderer.h"

#include <cstring>
#include <random>

using namespace realtone;
using namespace fixtures;
using namespace smf;

namespace {

std::vector<std::uint8_t> twoNoteFile()
{
    SmfBuilder b;
    std::vector<std::uint8_t> meta, notes;
    tempo(meta, 0, 500000);             // 120 bpm
    note(notes, 0, 0, 60, 100, true);
    note(notes, 480, 0, 60, 0, false);  // 0.5 s
    note(notes, 0, 0, 64, 80, true);
    note(notes, 480, 0, 64, 0, false);
    b.tracks = {meta, notes};
    return b.bytes();
}

struct Env
{
    std::shared_ptr<Instrument> inst;
    Env()
    {
        auto dir = rt_test::scratchDir("midi_inst");
        inst = loadOrDie(makeInstrumentFiles(dir));
    }
};

}  // namespace

RT_TEST(midi_parser_reads_notes_tempo_and_velocity)
{
    auto bytes = twoNoteFile();
    auto r = parseMidiMemory(bytes.data(), bytes.size());
    CHECK(r.ok);
    CHECK(r.sequence.noteCount == 2);
    CHECK_NEAR(r.sequence.initialTempoBpm, 120.0, 1e-6);
    CHECK(r.sequence.events.size() == 4);
    CHECK_NEAR(r.sequence.events[0].time, 0.0, 1e-9);
    CHECK(r.sequence.events[0].data2 == 100);
    // Second note starts exactly when the first ends; the off sorts before the on.
    CHECK_NEAR(r.sequence.events[1].time, 0.5, 1e-9);
    CHECK(r.sequence.events[1].kind == SequenceEvent::Kind::NoteOff);
    CHECK(r.sequence.events[2].kind == SequenceEvent::Kind::NoteOn);
    CHECK_NEAR(r.sequence.durationSeconds, 1.0, 1e-9);
    CHECK(r.sequence.tracks[1].noteCount == 2);
}

RT_TEST(midi_parser_follows_tempo_changes)
{
    SmfBuilder b;
    std::vector<std::uint8_t> t;
    tempo(t, 0, 500000);
    tempo(t, 480, 250000);  // after one beat, double speed
    note(t, 0, 0, 60, 90, true);
    note(t, 960, 0, 60, 0, false);  // 960 ticks at 250000us/quarter = 0.5 s
    b.tracks = {t};
    b.format = 0;
    auto bytes = b.bytes();
    auto r = parseMidiMemory(bytes.data(), bytes.size());
    CHECK(r.ok);
    CHECK_NEAR(r.sequence.events[0].time, 0.5, 1e-9);
    CHECK_NEAR(r.sequence.events[1].time, 1.0, 1e-9);
}

RT_TEST(midi_parser_supports_running_status_and_note_on_zero_velocity)
{
    SmfBuilder b;
    std::vector<std::uint8_t> t;
    SmfBuilder::vlq(t, 0); t.insert(t.end(), {0x90, 60, 100});
    SmfBuilder::vlq(t, 240); t.insert(t.end(), {60, 0});           // running status, velocity 0 = off
    SmfBuilder::vlq(t, 0); t.insert(t.end(), {64, 90});            // running status note on
    SmfBuilder::vlq(t, 240); t.insert(t.end(), {64, 0});
    b.tracks = {t};
    b.format = 0;
    auto bytes = b.bytes();
    auto r = parseMidiMemory(bytes.data(), bytes.size());
    CHECK(r.ok);
    CHECK(r.sequence.noteCount == 2);
    int offs = 0;
    for (auto& e : r.sequence.events) if (e.kind == SequenceEvent::Kind::NoteOff) ++offs;
    CHECK(offs == 2);
}

RT_TEST(midi_parser_reads_pitch_bend_and_controllers)
{
    SmfBuilder b;
    std::vector<std::uint8_t> t;
    note(t, 0, 0, 60, 100, true);
    SmfBuilder::vlq(t, 10); t.insert(t.end(), {0xE0, 0x00, 0x60});   // bend 12288
    SmfBuilder::vlq(t, 10); t.insert(t.end(), {0xB0, 64, 127});
    SmfBuilder::vlq(t, 10); t.insert(t.end(), {0xB0, 99, 5});        // NRPN: ignored
    note(t, 10, 0, 60, 0, false);
    b.tracks = {t};
    b.format = 0;
    auto bytes = b.bytes();
    auto r = parseMidiMemory(bytes.data(), bytes.size());
    CHECK(r.ok);
    int bends = 0, ccs = 0;
    for (auto& e : r.sequence.events)
    {
        if (e.kind == SequenceEvent::Kind::PitchBend) { ++bends; CHECK(e.data2 == 12288); }
        if (e.kind == SequenceEvent::Kind::Controller) { ++ccs; CHECK(e.data1 == 64); }
    }
    CHECK(bends == 1 && ccs == 1);
}

RT_TEST(midi_parser_rejects_bad_files_gracefully)
{
    std::vector<std::uint8_t> junk = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    CHECK(!parseMidiMemory(junk.data(), junk.size()).ok);
    CHECK(!parseMidiMemory(junk.data(), 0).ok);

    SmfBuilder b2;
    b2.format = 2;
    b2.tracks = {{0x00, 0x90, 60, 100}};
    auto f2 = b2.bytes();
    auto r2 = parseMidiMemory(f2.data(), f2.size());
    CHECK(!r2.ok);
    CHECK(r2.error.find("format 2") != std::string::npos);

    auto good = twoNoteFile();
    for (std::size_t cut : {20u, 33u, 50u})
    {
        auto r = parseMidiMemory(good.data(), std::min(cut, good.size()));
        (void)r;  // must not crash; result may be ok with warnings or a clean failure
    }
    // Empty (note-less) file.
    SmfBuilder b3;
    b3.tracks = {{0x00, 0xFF, 0x03, 0x01, 'x'}};
    auto f3 = b3.bytes();
    CHECK(!parseMidiMemory(f3.data(), f3.size()).ok);

    // Fuzz: flipping bytes never crashes.
    std::mt19937 rng(3);
    for (int i = 0; i < 300; ++i)
    {
        auto copy = good;
        for (int k = 0; k < 3; ++k)
            copy[rng() % copy.size()] = static_cast<std::uint8_t>(rng());
        parseMidiMemory(copy.data(), copy.size());
    }
}

RT_TEST(midi_parser_closes_hanging_notes)
{
    SmfBuilder b;
    std::vector<std::uint8_t> t;
    note(t, 0, 0, 60, 100, true);
    SmfBuilder::vlq(t, 480);
    t.insert(t.end(), {0xB0, 7, 100});
    b.tracks = {t};
    b.format = 0;
    auto bytes = b.bytes();
    auto r = parseMidiMemory(bytes.data(), bytes.size());
    CHECK(r.ok);
    CHECK(!r.sequence.warnings.empty());
    int offs = 0;
    for (auto& e : r.sequence.events) if (e.kind == SequenceEvent::Kind::NoteOff) ++offs;
    CHECK(offs == 1);
}

RT_TEST(render_midi_produces_expected_wav_properties)
{
    Env env;
    auto bytes = twoNoteFile();
    auto midi = parseMidiMemory(bytes.data(), bytes.size());
    for (double sr : {44100.0, 48000.0})
    {
        RenderSettings s;
        s.sampleRate = sr;
        auto r = renderSequence(env.inst, midi.sequence, s);
        CHECK_MSG(r.ok, r.error);
        CHECK(r.clip.numChannels() == 2);
        CHECK_NEAR(r.clip.sampleRate, sr, 0.01);
        // Two half-second notes: at least 1 s of content, plus a bounded release tail.
        CHECK(r.clip.durationSeconds() >= 1.0);
        CHECK(r.clip.durationSeconds() < 1.0 + 1.5);
        CHECK(r.clip.peak() <= 1.0f);
        CHECK(r.notesRendered == 2);

        const auto path = rt_test::scratchDir("render_out") / "out.wav";
        WavWriteOptions o;
        std::string err;
        CHECK(writeWavFile(path, r.clip, o, &err));
        auto back = decodeAudioFile(path);
        CHECK(back.ok && back.clip.numChannels() == 2 && back.clip.numFrames() == r.clip.numFrames());
        CHECK_NEAR(back.clip.sampleRate, sr, 0.5);
    }
}

RT_TEST(render_preserves_timing_of_note_onsets)
{
    Env env;
    auto bytes = twoNoteFile();
    auto midi = parseMidiMemory(bytes.data(), bytes.size());
    RenderSettings s;
    s.sampleRate = 48000.0;
    auto r = renderSequence(env.inst, midi.sequence, s);
    CHECK(r.ok);
    // Silence before the first onset is below the second onset's energy; the second note (E4)
    // must begin at 0.5 s: its pitch appears there.
    const auto& l = r.clip.channels[0];
    const double hz1 = estimateHz(l.data() + 4800, 8192, 48000.0);
    const double hz2 = estimateHz(l.data() + 24000 + 4800, 8192, 48000.0);
    CHECK_NEAR(hz1 / midiToHz(60), 1.0, 0.01);
    CHECK_NEAR(hz2 / midiToHz(64), 1.0, 0.01);
    CHECK(rms(l.data(), 200) > 0.0);
}

RT_TEST(render_is_deterministic)
{
    Env env;
    auto bytes = twoNoteFile();
    auto midi = parseMidiMemory(bytes.data(), bytes.size());
    RenderSettings s;
    s.timingHumanizeMs = 8.0;
    s.engine.humanizeVelocity = 0.8f;
    auto a = renderSequence(env.inst, midi.sequence, s);
    auto b = renderSequence(env.inst, midi.sequence, s);
    CHECK(a.ok && b.ok);
    CHECK(encodeWav(a.clip) == encodeWav(b.clip));
    s.seed = 99;
    auto c = renderSequence(env.inst, midi.sequence, s);
    CHECK(encodeWav(a.clip) != encodeWav(c.clip));
}

RT_TEST(render_tail_is_not_truncated)
{
    Env env;
    auto bytes = twoNoteFile();
    auto midi = parseMidiMemory(bytes.data(), bytes.size());
    RenderSettings s;
    s.engine.releaseScale = 3.0f;  // 0.9 s release
    s.normalize = NormalizeMode::None;
    auto r = renderSequence(env.inst, midi.sequence, s);
    CHECK(r.ok);
    const auto& l = r.clip.channels[0];
    const std::int64_t endOfNotes = static_cast<std::int64_t>(1.0 * s.sampleRate);
    CHECK(r.clip.numFrames() > endOfNotes + static_cast<std::int64_t>(0.5 * s.sampleRate));
    // Energy right after the last note-off is still present, and the very end is quiet.
    CHECK(rms(l.data() + endOfNotes + 2000, 2000) > 1e-4);
    CHECK(rms(l.data() + l.size() - 500, 400) < 5e-4);
}

RT_TEST(render_level_management_prevents_clipping)
{
    Env env;
    SmfBuilder b;
    std::vector<std::uint8_t> t;
    for (int n : {48, 52, 55, 60, 64, 67, 72, 76, 79}) note(t, 0, 0, n, 127, true);
    for (int n : {48, 52, 55, 60, 64, 67, 72, 76, 79}) note(t, n == 48 ? 960 : 0, 0, n, 0, false);
    b.tracks = {t};
    b.format = 0;
    auto bytes = b.bytes();
    auto midi = parseMidiMemory(bytes.data(), bytes.size());

    RenderSettings s;
    s.normalize = NormalizeMode::NormalizePeak;
    s.targetPeakDb = -3.0f;
    s.engine.masterGainDb = 12.0f;
    auto r = renderSequence(env.inst, midi.sequence, s);
    CHECK(r.ok);
    CHECK_NEAR(20.0 * std::log10(r.clip.peak()), -3.0, 0.05);

    s.normalize = NormalizeMode::None;
    auto raw = renderSequence(env.inst, midi.sequence, s);
    CHECK(raw.ok);
    if (raw.clip.peak() > 1.0f)
        CHECK(!raw.warnings.empty());
}

RT_TEST(render_options_transpose_track_filter_and_errors)
{
    Env env;
    auto bytes = twoNoteFile();
    auto midi = parseMidiMemory(bytes.data(), bytes.size());

    RenderSettings s;
    s.transposeSemitones = 12;
    auto up = renderSequence(env.inst, midi.sequence, s);
    CHECK(up.ok);
    CHECK_NEAR(estimateHz(up.clip.channels[0].data() + 4800, 8192, 48000.0) / midiToHz(72), 1.0, 0.01);

    s = RenderSettings{};
    s.enabledTracks = {0};  // the tempo-only track: no notes
    auto none = renderSequence(env.inst, midi.sequence, s);
    CHECK(!none.ok);
    CHECK(!none.error.empty());

    s = RenderSettings{};
    s.transposeSemitones = 100;
    CHECK(!renderSequence(env.inst, midi.sequence, s).ok);

    s = RenderSettings{};
    CHECK(!renderSequence(nullptr, midi.sequence, s).ok);  // no instrument loaded
    s.sampleRate = 100;
    CHECK(!renderSequence(env.inst, midi.sequence, s).ok);
}

RT_TEST(render_reports_progress_and_supports_cancellation)
{
    Env env;
    auto bytes = twoNoteFile();
    auto midi = parseMidiMemory(bytes.data(), bytes.size());
    int calls = 0;
    float last = -1;
    RenderSettings s;
    auto r = renderSequence(env.inst, midi.sequence, s, [&](float f, const std::string&) { ++calls; last = f; });
    CHECK(r.ok);
    CHECK(calls >= 2);
    CHECK_NEAR(last, 1.0, 1e-6);

    std::atomic<bool> cancel{true};
    auto c = renderSequence(env.inst, midi.sequence, s, {}, &cancel);
    CHECK(!c.ok && c.cancelled);
}

RT_TEST(render_with_instrument_missing_samples_for_notes_reports_silence)
{
    const auto dir = rt_test::scratchDir("midi_sparse");
    auto files = makeInstrumentFiles(dir);
    // Keep only a high register: the MIDI file's notes have no samples.
    for (int c = 36; c <= 66; c += 3)
        for (auto* v : {"_v0_0", "_v1_0", "_v1_1"})
            std::filesystem::remove(dir / "samples" / ("n" + std::to_string(c) + v + ".wav"));
    auto inst = loadOrDie(files);
    auto bytes = twoNoteFile();
    auto midi = parseMidiMemory(bytes.data(), bytes.size());
    auto r = renderSequence(inst, midi.sequence, RenderSettings{});
    CHECK(!r.ok);
    CHECK(r.error.find("silence") != std::string::npos);
}
