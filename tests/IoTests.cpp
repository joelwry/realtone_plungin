#include "Fixtures.h"
#include "TestSupport.h"

#include "realtone/AudioFile.h"
#include "realtone/Instrument.h"
#include "realtone/Library.h"
#include "realtone/Resampler.h"
#include "realtone/Sfz.h"
#include "realtone/Sha256.h"

#include <cstring>
#include <fstream>

using namespace realtone;
using namespace fixtures;

RT_TEST(sha256_known_vectors)
{
    CHECK(sha256Hex("", 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Hex("abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const std::string longer = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(sha256Hex(longer.data(), longer.size()) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // One million 'a' via several updates (exercises the block buffering).
    Sha256 s;
    std::string chunk(1000, 'a');
    for (int i = 0; i < 1000; ++i)
        s.update(chunk.data(), chunk.size());
    CHECK(s.finishHex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

RT_TEST(wav_round_trip_all_formats)
{
    const auto dir = rt_test::scratchDir("wav_rt");
    auto clip = toneClip(440.0, 0.5, 48000.0, 2);
    for (auto fmt : {WavSampleFormat::Pcm16, WavSampleFormat::Pcm24, WavSampleFormat::Float32})
    {
        WavWriteOptions o;
        o.format = fmt;
        const auto path = dir / ("t" + std::to_string(static_cast<int>(fmt)) + ".wav");
        std::string err;
        CHECK_MSG(writeWavFile(path, clip, o, &err), err);
        auto back = decodeAudioFile(path);
        CHECK(back.ok);
        CHECK(back.format == "wav");
        CHECK(back.clip.numChannels() == 2);
        CHECK_NEAR(back.clip.sampleRate, 48000.0, 0.5);
        CHECK(back.clip.numFrames() == clip.numFrames());
        const double tol = fmt == WavSampleFormat::Pcm16 ? 2e-4 : fmt == WavSampleFormat::Pcm24 ? 2e-6 : 1e-9;
        double maxErr = 0;
        for (std::int64_t i = 0; i < clip.numFrames(); ++i)
            maxErr = std::max<double>(maxErr, std::fabs(back.clip.channels[0][static_cast<std::size_t>(i)] - clip.channels[0][static_cast<std::size_t>(i)]));
        CHECK_MSG(maxErr <= tol, "format " << static_cast<int>(fmt) << " error " << maxErr);
    }
}

RT_TEST(wav_writer_is_deterministic_and_refuses_to_overwrite)
{
    const auto dir = rt_test::scratchDir("wav_over");
    auto clip = toneClip(220.0, 0.2, 44100.0);
    WavWriteOptions o;
    o.format = WavSampleFormat::Pcm16;
    CHECK(encodeWav(clip, o) == encodeWav(clip, o));

    const auto path = dir / "out.wav";
    std::string err;
    CHECK(writeWavFile(path, clip, o, &err));
    const auto before = std::filesystem::file_size(path);
    CHECK(!writeWavFile(path, toneClip(220.0, 1.0, 44100.0), o, &err));
    CHECK(!err.empty());
    CHECK(std::filesystem::file_size(path) == before);  // original untouched
    o.overwrite = true;
    CHECK(writeWavFile(path, toneClip(220.0, 1.0, 44100.0), o, &err));
    CHECK(std::filesystem::file_size(path) > before);
    CHECK(!std::filesystem::exists(path.string() + ".part"));
}

RT_TEST(decoder_rejects_invalid_input_gracefully)
{
    const auto dir = rt_test::scratchDir("bad_audio");
    CHECK(!decodeAudioFile(dir / "missing.wav").ok);
    writeText(dir / "empty.wav", "");
    CHECK(!decodeAudioFile(dir / "empty.wav").ok);
    writeText(dir / "text.wav", "this is not audio at all, just some text");
    auto r = decodeAudioFile(dir / "text.wav");
    CHECK(!r.ok);
    CHECK(!r.error.empty());

    // Truncated RIFF header.
    auto bytes = encodeWav(toneClip(440, 0.1, 44100));
    bytes.resize(30);
    CHECK(!decodeAudioMemory(bytes.data(), bytes.size()).ok);

    // Directory instead of file.
    CHECK(!decodeAudioFile(dir).ok);

    // Random bytes with a RIFF prefix.
    std::vector<std::uint8_t> junk(2000, 0x5A);
    std::memcpy(junk.data(), "RIFF", 4);
    std::memcpy(junk.data() + 8, "WAVE", 4);
    CHECK(!decodeAudioMemory(junk.data(), junk.size()).ok);
}

RT_TEST(resampler_preserves_pitch_and_duration)
{
    auto x = tone(1000.0, 1.0, 44100.0, 0.0);
    auto y = resample(x, 44100.0, 48000.0);
    CHECK(y.size() == 48000);
    CHECK_NEAR(estimateHz(y.data() + 1000, 8192, 48000.0) / 1000.0, 1.0, 0.002);
    auto z = resample(x, 44100.0, 22050.0);
    CHECK(z.size() == 22050);
    CHECK_NEAR(estimateHz(z.data() + 1000, 4096, 22050.0) / 1000.0, 1.0, 0.003);
}

RT_TEST(sfz_parser_handles_headers_inheritance_and_comments)
{
    const auto dir = rt_test::scratchDir("sfz1");
    const std::string text =
        "// a comment\n<control>\ndefault_path=Samples\\Sub\\\n#define $VOL 3\n"
        "<global> ampeg_release=0.5 volume=$VOL\n"
        "<group> lovel=10 hivel=90 /* inline */ loop_mode=one_shot\n"
        "<region> sample=a b.wav key=c4\n"
        "<region> sample=b.wav lokey=d#3 hikey=f3 pitch_keycenter=e3 volume=-2 unknown_opcode=7\n"
        "<group> trigger=release\n"
        "<region> sample=c.wav lokey=0 hikey=127\n";
    auto r = parseSfz(text, dir, dir);
    CHECK(r.ok);
    CHECK(r.regions.size() == 3);
    CHECK(r.regions[0].sample == "Samples/Sub/a b.wav");
    CHECK(r.regions[0].lokey == 60 && r.regions[0].hikey == 60 && r.regions[0].pitchKeycenter == 60);
    CHECK(r.regions[0].lovel == 10 && r.regions[0].hivel == 90);
    CHECK(r.regions[0].loopMode == SfzRegion::LoopMode::OneShot);
    CHECK_NEAR(r.regions[0].volumeDb, 3.0, 1e-6);
    CHECK_NEAR(r.regions[0].ampegRelease, 0.5, 1e-6);
    CHECK(r.regions[1].lokey == 51 && r.regions[1].hikey == 53 && r.regions[1].pitchKeycenter == 52);
    CHECK_NEAR(r.regions[1].volumeDb, -2.0, 1e-6);
    CHECK(r.regions[2].trigger == SfzRegion::Trigger::Release);
    CHECK(r.regions[2].lovel == 0 && r.regions[2].hivel == 127);  // group opcodes reset on a new <group>
    CHECK(r.unsupportedOpcodes.count("unknown_opcode") == 1);
}

RT_TEST(sfz_parser_rejects_paths_escaping_the_library)
{
    const auto dir = rt_test::scratchDir("sfz2");
    auto r = parseSfz("<region> sample=../../etc/passwd lokey=0 hikey=1\n<region> sample=ok.wav lokey=0 hikey=1\n", dir, dir);
    CHECK(r.ok);
    CHECK(r.regions.size() == 1);
    CHECK(r.regions[0].sample == "ok.wav");
    CHECK(!r.warnings.empty());

    auto inc = parseSfz("#include \"../outside.sfz\"\n<region> sample=ok.wav\n", dir, dir);
    CHECK(inc.ok);
    CHECK(!inc.warnings.empty());

    CHECK(!parseSfz("<global> volume=3\n", dir, dir).ok);  // no regions at all
}

RT_TEST(note_name_parsing)
{
    CHECK(parseNoteToken("c4") == 60);
    CHECK(parseNoteToken("A4") == 69);
    CHECK(parseNoteToken("f#3") == 54);
    CHECK(parseNoteToken("eb2") == 39);
    CHECK(parseNoteToken("64") == 64);
    CHECK(parseNoteToken("zz") == -1);
    CHECK(parseNoteToken("200") == -1);
}

RT_TEST(instrument_loader_reports_missing_samples_without_substituting)
{
    const auto dir = rt_test::scratchDir("inst_missing");
    auto files = makeInstrumentFiles(dir);
    // Remove all samples for one key group.
    std::filesystem::remove(dir / "samples" / "n60_v0_0.wav");
    std::filesystem::remove(dir / "samples" / "n60_v1_0.wav");
    std::filesystem::remove(dir / "samples" / "n60_v1_1.wav");
    auto r = loadInstrument(files.sfz, files.root);
    CHECK(r.ok);
    CHECK(!r.instrument->report.complete());
    CHECK(r.instrument->report.missingSamples.size() == 3);
    CHECK(r.instrument->attackByKey[60].empty());   // nothing plays on the missing key
    CHECK(!r.instrument->attackByKey[63].empty());  // other keys are unaffected

    LoadOptions strict;
    strict.allowPartial = false;
    auto s = loadInstrument(files.sfz, files.root, strict);
    CHECK(!s.ok);
    CHECK(!s.error.empty());
}

RT_TEST(instrument_loader_fails_clearly_when_nothing_loads)
{
    const auto dir = rt_test::scratchDir("inst_none");
    writeText(dir / "x.sfz", "<region> sample=gone.wav lokey=0 hikey=127\n");
    auto r = loadInstrument(dir / "x.sfz", dir);
    CHECK(!r.ok);
    CHECK(r.error.find("missing") != std::string::npos);
    auto m = loadInstrument(dir / "nope.sfz", dir);
    CHECK(!m.ok);
}

RT_TEST(instrument_loader_cancels_and_enforces_memory_limit)
{
    const auto dir = rt_test::scratchDir("inst_limits");
    auto files = makeInstrumentFiles(dir);
    LoadOptions o;
    std::atomic<bool> cancel{true};
    o.cancel = &cancel;
    auto c = loadInstrument(files.sfz, files.root, o);
    CHECK(!c.ok && c.cancelled);

    LoadOptions small;
    small.maxBytes = 100 * 1024;
    auto s = loadInstrument(files.sfz, files.root, small);
    CHECK(!s.ok);
    CHECK(s.error.find("memory") != std::string::npos);
}

RT_TEST(repeated_load_and_release_does_not_leak_samples)
{
    const auto dir = rt_test::scratchDir("inst_leak");
    auto files = makeInstrumentFiles(dir);
    std::weak_ptr<const SampleData> probe;
    for (int i = 0; i < 6; ++i)
    {
        auto r = loadInstrument(files.sfz, files.root);
        CHECK(r.ok);
        probe = r.instrument->regions.front().sample;
        CHECK(r.instrument->memoryBytes() > 0);
    }
    CHECK(probe.expired());  // each instrument's samples are freed with it
}

// ----------------------------------------------------------------------------- library

namespace {

class DirFetcher : public IFileFetcher
{
public:
    explicit DirFetcher(std::filesystem::path root) : root_(std::move(root)) {}
    int calls = 0;
    std::string corrupt;  // file name fragment to corrupt
    FetchResult fetch(const std::string& url, const std::filesystem::path& dest,
                      const std::function<void(std::uint64_t)>& onBytes, const std::atomic<bool>* cancel) override
    {
        ++calls;
        FetchResult r;
        if (cancel != nullptr && cancel->load()) { r.cancelled = true; return r; }
        const auto name = url.substr(url.rfind('/') + 1);
        const auto src = root_ / name;
        if (!std::filesystem::exists(src)) { r.error = "404"; return r; }
        std::ifstream in(src, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (!corrupt.empty() && name.find(corrupt) != std::string::npos)
            data[0] ^= 0x55;
        std::ofstream(dest, std::ios::binary) << data;
        onBytes(data.size());
        r.ok = true;
        return r;
    }
private:
    std::filesystem::path root_;
};

LibraryPack makePack(const std::filesystem::path& serverDir)
{
    LibraryPack p;
    p.id = "testpack";
    p.name = "Test Pack";
    p.version = "1";
    p.license.name = "CC0 1.0 Universal";
    p.baseUrl = "https://example.invalid/data/";
    p.allowedHosts = {"example.invalid"};
    auto add = [&](const std::string& name, const std::string& content) {
        writeText(serverDir / name, content);
        PackFile f;
        f.path = name;
        f.size = content.size();
        f.sha256 = sha256Hex(content.data(), content.size());
        p.files[name] = f;
    };
    add("a.sfz", "<region> sample=a.wav\n");
    add("a.wav", std::string(5000, 'x'));
    add("b.sfz", "<region> sample=shared.wav\n");
    add("shared.wav", std::string(3000, 'y'));
    p.instruments.push_back({"one", "One", "Keys", "piano", "", "a.sfz", {}, {"a.sfz", "a.wav", "shared.wav"}});
    p.instruments.push_back({"two", "Two", "Keys", "piano", "", "b.sfz", {}, {"b.sfz", "shared.wav"}});
    return p;
}

}  // namespace

RT_TEST(manifest_validation)
{
    auto pack = makePack(rt_test::scratchDir("man_server"));
    auto good = parsePackManifest(packToJson(pack));
    CHECK(good.ok);
    CHECK(good.pack.instruments.size() == 2);

    auto bad = [&](auto mutate) {
        LibraryPack p = pack;
        mutate(p);
        return parsePackManifest(packToJson(p)).ok;
    };
    CHECK(!bad([](LibraryPack& p) { p.baseUrl = "http://example.invalid/"; }));
    CHECK(!bad([](LibraryPack& p) { p.license.name.clear(); }));
    CHECK(!bad([](LibraryPack& p) { auto f = p.files["a.wav"]; f.path = "../evil.wav"; p.files["../evil.wav"] = f; }));
    CHECK(!bad([](LibraryPack& p) { p.instruments[0].files.push_back("unlisted.wav"); }));
    CHECK(!bad([](LibraryPack& p) { p.id = "../x"; }));
    CHECK(!parsePackManifest("{ not json").ok);
    CHECK(isSafeRelativePath("Brass/Trumpet/a b.wav"));
    CHECK(!isSafeRelativePath("C:/windows/x"));
    CHECK(!isSafeRelativePath("a/../b"));
    CHECK(!isSafeRelativePath("/abs"));
}

RT_TEST(library_install_verify_and_remove)
{
    const auto server = rt_test::scratchDir("lib_server");
    const auto root = rt_test::scratchDir("lib_root");
    const auto pack = makePack(server);
    DirFetcher fetcher(server);

    LibraryCatalog cat;
    cat.setLibraryRoot(root);
    cat.addPack(pack);
    cat.refresh();
    CHECK(cat.find("one")->state == InstallState::NotInstalled);

    InstallProgress last;
    auto res = installInstrument(pack, pack.instruments[0], root, fetcher, [&](const InstallProgress& p) { last = p; });
    CHECK_MSG(res.ok, res.error);
    CHECK(res.filesDownloaded == 3);
    CHECK(last.filesDone == 3);
    CHECK(last.bytesDone == last.bytesTotal);
    cat.refresh();
    CHECK(cat.find("one")->state == InstallState::Installed);
    CHECK(cat.find("two")->state == InstallState::Incomplete);  // shared file present, own sfz missing
    CHECK(std::filesystem::exists(root / "testpack" / "pack.json"));

    // Reinstalling downloads nothing.
    fetcher.calls = 0;
    auto again = installInstrument(pack, pack.instruments[0], root, fetcher);
    CHECK(again.ok && fetcher.calls == 0 && again.filesAlreadyPresent == 3);

    // Damage detection: size change is caught cheaply, same-size corruption by the full check.
    {
        std::fstream f(root / "testpack" / "a.wav", std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(10);
        f.put('Z');
    }
    cat.refresh();
    CHECK(cat.find("one")->state == InstallState::Installed);
    CHECK(cat.verifyInstrument("one").state == InstallState::Damaged);
    // Repair by reinstalling.
    CHECK(installInstrument(pack, pack.instruments[0], root, fetcher).ok);
    CHECK(cat.verifyInstrument("one").state == InstallState::Installed);

    // Install the second instrument, then remove the first: the shared file must survive.
    CHECK(installInstrument(pack, pack.instruments[1], root, fetcher).ok);
    auto removed = removeInstrument(pack, pack.instruments[0], root);
    CHECK(removed.ok);
    CHECK(!std::filesystem::exists(root / "testpack" / "a.sfz"));
    CHECK(!std::filesystem::exists(root / "testpack" / "a.wav"));
    CHECK(std::filesystem::exists(root / "testpack" / "shared.wav"));
    cat.refresh();
    CHECK(cat.find("two")->state == InstallState::Installed);

    // A user's own file in the pack folder is never touched.
    writeText(root / "testpack" / "my_notes.txt", "keep me");
    removeInstrument(pack, pack.instruments[1], root);
    CHECK(std::filesystem::exists(root / "testpack" / "my_notes.txt"));
    // Unrelated sibling folders are untouched.
    writeText(root / "othermusic" / "file.wav", "x");
    removeInstrument(pack, pack.instruments[1], root);
    CHECK(std::filesystem::exists(root / "othermusic" / "file.wav"));
}

RT_TEST(library_install_rejects_corrupt_downloads_and_unauthorised_hosts)
{
    const auto server = rt_test::scratchDir("lib_server2");
    const auto root = rt_test::scratchDir("lib_root2");
    auto pack = makePack(server);
    DirFetcher fetcher(server);
    fetcher.corrupt = "a.wav";

    auto res = installInstrument(pack, pack.instruments[0], root, fetcher);
    CHECK(!res.ok);
    CHECK(res.error.find("a.wav") != std::string::npos);
    CHECK(!std::filesystem::exists(root / "testpack" / "a.wav"));         // never stored
    CHECK(!std::filesystem::exists(root / "testpack" / "a.wav.part"));    // no leftovers

    pack.allowedHosts = {"somewhere.else"};
    DirFetcher clean(server);
    auto denied = installInstrument(pack, pack.instruments[0], root, clean);
    CHECK(!denied.ok);
    CHECK(clean.calls == 0);

    pack.allowedHosts = {"example.invalid"};
    pack.license.redistributionAllowed = false;
    CHECK(!installInstrument(pack, pack.instruments[0], root, clean).ok);
}

RT_TEST(library_install_can_be_cancelled)
{
    const auto server = rt_test::scratchDir("lib_server3");
    const auto root = rt_test::scratchDir("lib_root3");
    auto pack = makePack(server);
    DirFetcher fetcher(server);
    std::atomic<bool> cancel{true};
    auto res = installInstrument(pack, pack.instruments[0], root, fetcher, {}, &cancel);
    CHECK(!res.ok && res.cancelled);
}

RT_TEST(url_helpers)
{
    CHECK(urlEncodePath("Keys/Upright Piano/a b.wav") == "Keys/Upright%20Piano/a%20b.wav");
    CHECK(hostOf("https://raw.githubusercontent.com/x/y") == "raw.githubusercontent.com");
    CHECK(hostOf("https://Example.COM:8443/p") == "example.com");
    CHECK(hostOf("no-scheme").empty());
}

RT_TEST(mp3_files_decode_with_correct_format_and_pitch)
{
    auto r = decodeAudioFile(std::filesystem::path(REALTONE_TEST_DATA) / "sine_a4_mono.mp3");
    CHECK_MSG(r.ok, r.error);
    CHECK(r.format == "mp3");
    CHECK(r.clip.numChannels() == 1);
    CHECK_NEAR(r.clip.sampleRate, 44100.0, 0.5);
    CHECK_NEAR(r.clip.durationSeconds(), 1.2, 0.1);
    CHECK_NEAR(estimateHz(r.clip.channels[0].data() + 8000, 8192, r.clip.sampleRate) / 440.0, 1.0, 0.003);

    // A damaged MP3 fails with a message instead of producing noise or crashing.
    std::vector<std::uint8_t> bytes(2000, 0xFF);
    bytes[1] = 0xFB;  // looks like a frame header, followed by junk
    auto bad = decodeAudioMemory(bytes.data(), bytes.size());
    CHECK(!bad.ok || bad.clip.numFrames() < 20000);
}
