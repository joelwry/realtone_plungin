// realtone-cli: headless access to the instrument engine, used for batch rendering,
// automated checks and as a scriptable route into Audacity workflows.

#include "realtone/AudioFile.h"
#include "realtone/Instrument.h"
#include "realtone/Library.h"
#include "realtone/MidiFile.h"
#include "realtone/PitchAnalysis.h"
#include "realtone/Renderer.h"
#include "realtone/Sha256.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

using namespace realtone;
namespace fs = std::filesystem;

namespace {

constexpr const char* kUsage = R"(realtone-cli - RealTone Studio command line

Usage:
  realtone-cli info     --instrument FILE.sfz [--root DIR]
  realtone-cli render   --instrument FILE.sfz --midi FILE.mid --out FILE.wav [options]
  realtone-cli convert  --instrument FILE.sfz --audio FILE.(wav|mp3|flac) --out FILE.wav [options]
  realtone-cli analyse  --audio FILE.(wav|mp3|flac)
  realtone-cli verify   --manifest FILE.json --library DIR [--instrument ID]

Render/convert options:
  --rate HZ            output sample rate (default 48000)
  --bits 16|24|32f     output format (default 24)
  --transpose N        semitones
  --tempo X            tempo scale (render only)
  --tracks 1,2         only render these track indexes (see the track list printed by render)
  --family NAME        start from that family's performance defaults (piano, bowed, brass, ...)
  --bend-range N       pitch bend range in semitones (default 2)
  --no-bend            ignore pitch bend events (the default for piano-like families)
  --release X          release length multiplier (default 1)
  --normalize MODE     none | limit | peak  (default limit, target -1 dBFS)
  --seed N             seed for humanisation (default 1)
  --overwrite          replace an existing output file

The input files are never modified, and an existing output file is not replaced without --overwrite.
)";

using Args = std::map<std::string, std::string>;

Args parseArgs(int argc, char** argv, int from)
{
    Args a;
    for (int i = from; i < argc; ++i)
    {
        std::string k = argv[i];
        if (k.rfind("--", 0) != 0)
        {
            std::fprintf(stderr, "Unexpected argument: %s\n", k.c_str());
            std::exit(2);
        }
        k = k.substr(2);
        if (k == "overwrite" || k == "no-bend")
            a[k] = "1";
        else if (i + 1 < argc)
            a[k] = argv[++i];
        else
        {
            std::fprintf(stderr, "Missing value for --%s\n", k.c_str());
            std::exit(2);
        }
    }
    return a;
}

std::string get(const Args& a, const char* k, const char* def = "")
{
    auto it = a.find(k);
    return it == a.end() ? def : it->second;
}

std::shared_ptr<Instrument> loadInst(const Args& a)
{
    const fs::path sfz = get(a, "instrument");
    if (sfz.empty())
    {
        std::fprintf(stderr, "--instrument is required\n");
        return nullptr;
    }
    const fs::path root = a.count("root") ? fs::path(get(a, "root")) : sfz.parent_path();
    LoadOptions o;
    o.progress = [](float f, const std::string& s) { std::fprintf(stderr, "\r  loading %3.0f%%  %-40s", f * 100.0f, s.c_str()); };
    auto r = loadInstrument(sfz, root, o);
    std::fprintf(stderr, "\n");
    if (!r.ok)
    {
        std::fprintf(stderr, "Cannot load instrument: %s\n", r.error.c_str());
        return nullptr;
    }
    const auto& rep = r.instrument->report;
    if (!rep.complete())
        std::fprintf(stderr, "Warning: %zu sample file(s) missing, %zu unreadable. Notes using them will be silent.\n",
                     rep.missingSamples.size(), rep.failedSamples.size());
    return r.instrument;
}

RenderSettings settingsFrom(const Args& a)
{
    RenderSettings s;
    s.sampleRate = std::atof(get(a, "rate", "48000").c_str());
    s.transposeSemitones = std::atoi(get(a, "transpose", "0").c_str());
    s.tempoScale = std::atof(get(a, "tempo", "1").c_str());
    if (a.count("family"))
        s.engine = paramsFromProfile(profileFor(familyFromString(get(a, "family"))));
    s.engine.pitchBendRange = static_cast<float>(std::atof(get(a, "bend-range", "2").c_str()));
    if (a.count("no-bend"))
        s.engine.pitchBendEnabled = false;
    else if (!a.count("family"))
        s.engine.pitchBendEnabled = true;
    s.engine.releaseScale = static_cast<float>(std::atof(get(a, "release", "1").c_str()));
    s.seed = static_cast<std::uint32_t>(std::atoi(get(a, "seed", "1").c_str()));
    {
        std::stringstream ts(get(a, "tracks"));
        std::string tok;
        while (std::getline(ts, tok, ','))
            if (!tok.empty())
                s.enabledTracks.push_back(std::atoi(tok.c_str()));
    }
    const auto n = get(a, "normalize", "limit");
    s.normalize = n == "none" ? NormalizeMode::None : n == "peak" ? NormalizeMode::NormalizePeak : NormalizeMode::LimitPeak;
    return s;
}

bool writeOut(const Args& a, const AudioClip& clip)
{
    WavWriteOptions w;
    const auto bits = get(a, "bits", "24");
    w.format = bits == "16" ? WavSampleFormat::Pcm16 : bits == "32f" ? WavSampleFormat::Float32 : WavSampleFormat::Pcm24;
    w.overwrite = a.count("overwrite") != 0;
    std::string err;
    if (!writeWavFile(get(a, "out"), clip, w, &err))
    {
        std::fprintf(stderr, "Cannot write output: %s\n", err.c_str());
        return false;
    }
    std::printf("Wrote %s  (%.2f s, %.0f Hz, %d ch)\n", get(a, "out").c_str(), clip.durationSeconds(), clip.sampleRate, clip.numChannels());
    return true;
}

int cmdInfo(const Args& a)
{
    auto inst = loadInst(a);
    if (!inst)
        return 1;
    const auto& r = inst->report;
    std::printf("Regions declared: %zu, playable: %zu\n", r.regionsDeclared, r.regionsPlayable);
    std::printf("Sample memory: %.1f MB\n", static_cast<double>(inst->memoryBytes()) / (1024.0 * 1024.0));
    std::printf("Release-trigger regions: %s\n", inst->hasReleaseRegions() ? "yes" : "no");
    for (auto& op : r.unsupportedOpcodes) std::printf("Unsupported opcode (ignored): %s\n", op.c_str());
    for (auto& w : r.warnings) std::printf("Warning: %s\n", w.c_str());
    for (auto& m : r.missingSamples) std::printf("Missing: %s\n", m.c_str());
    return r.complete() ? 0 : 3;
}

int cmdRender(const Args& a)
{
    auto inst = loadInst(a);
    if (!inst)
        return 1;
    auto midi = parseMidiFile(get(a, "midi"));
    if (!midi.ok)
    {
        std::fprintf(stderr, "MIDI error: %s\n", midi.error.c_str());
        return 1;
    }
    for (auto& w : midi.sequence.warnings) std::fprintf(stderr, "MIDI warning: %s\n", w.c_str());
    std::printf("MIDI: %d notes, %zu track(s), %.1f bpm, %.1f s\n", midi.sequence.noteCount, midi.sequence.tracks.size(),
                midi.sequence.initialTempoBpm, midi.sequence.durationSeconds);
    for (const auto& t : midi.sequence.tracks)
        std::printf("  track %d \"%s\": %d notes%s\n", t.index, t.name.c_str(), t.noteCount, t.looksLikePercussion ? " (percussion channel)" : "");
    auto res = renderSequence(inst, midi.sequence, settingsFrom(a), [](float f, const std::string&) {
        std::fprintf(stderr, "\r  rendering %3.0f%%", f * 100.0f);
    });
    std::fprintf(stderr, "\n");
    if (!res.ok)
    {
        std::fprintf(stderr, "Render failed: %s\n", res.error.c_str());
        return 1;
    }
    for (auto& w : res.warnings) std::fprintf(stderr, "Warning: %s\n", w.c_str());
    std::printf("Notes rendered: %d (skipped %d), peak voices %d, peak %.1f dBFS before gain, gain %.1f dB\n",
                res.notesRendered, res.notesSkipped, res.peakVoices, res.peakBeforeGainDb, res.appliedGainDb);
    return writeOut(a, res.clip) ? 0 : 1;
}

void printAnalysis(const AnalysisReport& rep)
{
    std::printf("Detected %zu notes, mean confidence %.2f, unclear-pitch share %.0f%%\n", rep.notes.size(), rep.meanConfidence,
                rep.polyphonyIndicator * 100.0);
    for (auto& w : rep.warnings) std::printf("Warning: %s\n", w.c_str());
    for (auto& n : rep.notes)
        std::printf("  %7.3f - %7.3f  note %3d  vel %3d  conf %.2f\n", n.startSeconds, n.endSeconds, n.midiNote, n.velocity, n.confidence);
}

int cmdAnalyse(const Args& a)
{
    auto d = decodeAudioFile(get(a, "audio"));
    if (!d.ok)
    {
        std::fprintf(stderr, "Cannot read audio: %s\n", d.error.c_str());
        return 1;
    }
    auto rep = analyseMonophonic(d.clip);
    if (!rep.ok)
    {
        std::fprintf(stderr, "Analysis failed: %s\n", rep.error.c_str());
        return 1;
    }
    printAnalysis(rep);
    return 0;
}

int cmdConvert(const Args& a)
{
    auto inst = loadInst(a);
    if (!inst)
        return 1;
    auto d = decodeAudioFile(get(a, "audio"));
    if (!d.ok)
    {
        std::fprintf(stderr, "Cannot read audio: %s\n", d.error.c_str());
        return 1;
    }
    auto rep = analyseMonophonic(d.clip);
    if (!rep.ok)
    {
        std::fprintf(stderr, "Analysis failed: %s\n", rep.error.c_str());
        return 1;
    }
    printAnalysis(rep);
    SequenceFromNotesOptions so;
    auto rs = settingsFrom(a);
    so.bendRangeSemitones = rs.engine.pitchBendRange;
    auto seq = notesToSequence(rep.notes, so);
    auto res = renderSequence(inst, seq, rs);
    if (!res.ok)
    {
        std::fprintf(stderr, "Render failed: %s\n", res.error.c_str());
        return 1;
    }
    return writeOut(a, res.clip) ? 0 : 1;
}

int cmdVerify(const Args& a)
{
    std::ifstream in(get(a, "manifest"));
    std::stringstream ss;
    ss << in.rdbuf();
    auto m = parsePackManifest(ss.str());
    if (!m.ok)
    {
        std::fprintf(stderr, "Manifest error: %s\n", m.error.c_str());
        return 1;
    }
    LibraryCatalog cat;
    cat.setLibraryRoot(get(a, "library"));
    cat.addPack(m.pack);
    int bad = 0;
    for (const auto& i : m.pack.instruments)
    {
        if (a.count("instrument") && get(a, "instrument") != i.id)
            continue;
        auto e = cat.verifyInstrument(i.id);
        std::printf("%-32s %-14s missing %d, damaged %d\n", i.id.c_str(), toString(e.state), e.missingFiles, e.damagedFiles);
        if (e.state == InstallState::Damaged || e.state == InstallState::Incomplete)
            ++bad;
    }
    return bad == 0 ? 0 : 3;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0)
    {
        std::fputs(kUsage, stdout);
        return argc < 2 ? 2 : 0;
    }
    const std::string cmd = argv[1];
    const Args a = parseArgs(argc, argv, 2);
    if (cmd == "info") return cmdInfo(a);
    if (cmd == "render") return cmdRender(a);
    if (cmd == "convert") return cmdConvert(a);
    if (cmd == "analyse") return cmdAnalyse(a);
    if (cmd == "verify") return cmdVerify(a);
    std::fprintf(stderr, "Unknown command: %s\n\n%s", cmd.c_str(), kUsage);
    return 2;
}
