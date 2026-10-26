#include "SmfBuilder.h"
#include "StudioTestSupport.h"
#include "TestSupport.h"

#include "../app/MainView.h"
#include "../studio/WindowGeometry.h"
#include "../ui/Theme.h"

#include "realtone/Sha256.h"

#include <cstdlib>

using namespace realtone;
using namespace realtone::studio;
using namespace fixtures;

namespace
{

juce::File outputDir()
{
    const char* env = std::getenv("REALTONE_TEST_OUTPUT");
    juce::File dir(env != nullptr ? juce::String(env) : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("realtone_ui").getFullPathName());
    dir.createDirectory();
    return dir;
}

void savePng(juce::Component& c, const juce::String& name)
{
    auto image = c.createComponentSnapshot(c.getLocalBounds(), true, 1.0f);
    auto file = outputDir().getChildFile(name + ".png");
    file.deleteFile();
    juce::FileOutputStream out(file);
    if (out.openedOk())
    {
        juce::PNGImageFormat png;
        png.writeImageToStream(image, out);
    }
}

juce::File melodyMidi(const std::filesystem::path& dir)
{
    smf::SmfBuilder b;
    std::vector<std::uint8_t> meta, notes;
    smf::tempo(meta, 0, 500000);
    int t = 0;
    for (int n : {60, 62, 64, 65, 67})
    {
        smf::note(notes, t == 0 ? 0 : 0, 0, n, 100, true);
        smf::note(notes, 480, 0, n, 0, false);
        ++t;
    }
    b.tracks = {meta, notes};
    const auto path = dir / "melody.mid";
    const auto bytes = b.bytes();
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return juce::File(juce::String(path.string()));
}

juce::File melodyWav(const std::filesystem::path& dir, const std::vector<int>& midi, const char* name = "melody.wav", double sr = 44100.0)
{
    AudioClip c;
    c.sampleRate = sr;
    std::vector<float> x;
    for (int m : midi)
    {
        auto t = tone(midiToHz(m), 0.45, sr, 0.5, 0.5);
        // short attack so the note onsets are clear
        for (std::size_t i = 0; i < 400 && i < t.size(); ++i)
            t[i] *= static_cast<float>(i) / 400.0f;
        x.insert(x.end(), t.begin(), t.end());
        x.insert(x.end(), static_cast<std::size_t>(0.05 * sr), 0.0f);
    }
    c.channels.push_back(std::move(x));
    WavWriteOptions o;
    o.overwrite = true;
    writeWavFile(dir / name, c, o);
    return juce::File(juce::String((dir / name).string()));
}

std::string hashOf(const juce::File& f) { return sha256File(std::filesystem::path(f.getFullPathName().toStdString())); }

struct Rig
{
    std::filesystem::path dir;
    AppContext ctx;
    juce::String instrumentId;
    explicit Rig(const std::string& name)
    {
        dir = rt_test::scratchDir(name);
        auto files = makeInstrumentFiles(dir / "inst");
        instrumentId = studio_test::loadFixture(ctx.processor, files.sfz, "generic");
    }
};

bool waitIdle(const std::function<bool()>& busy, int ms = 60000)
{
    const bool ok = studio_test::pumpUntil([&] { return !busy(); }, ms);
    // The completion callback is posted to the message thread just after the job stops.
    studio_test::pumpUntil([] { return false; }, 120);
    return ok;
}

}  // namespace

RT_TEST(window_geometry_opens_a_usable_window_on_any_screen)
{
    using G = WindowGeometry;
    // Large desktop: preferred size, centred.
    auto a = G::initialBounds({0, 0, 2560, 1400});
    CHECK(a.getWidth() == G::kPreferredWidth && a.getHeight() == G::kPreferredHeight);
    CHECK(a.getCentre() == juce::Point<int>(1280, 700));
    // Small laptop: never larger than 90% of the work area.
    auto b = G::initialBounds({0, 0, 1366, 728});
    CHECK(b.getWidth() <= 1230 && b.getHeight() <= 656);
    CHECK(b.getWidth() >= G::kMinWidth && b.getHeight() >= G::kMinHeight);
    // Tiny screen: fill it rather than overflow.
    auto c = G::initialBounds({0, 0, 800, 500});
    CHECK(c.getWidth() <= 800 && c.getHeight() <= 500);
}

RT_TEST(window_geometry_rescues_unreachable_saved_positions)
{
    using G = WindowGeometry;
    juce::Array<juce::Rectangle<int>> one;
    one.add({0, 0, 1920, 1040});
    juce::Array<juce::Rectangle<int>> two = one;
    two.add({1920, 0, 2560, 1400});

    // Fine as it is.
    CHECK(G::sanitise({100, 80, 1200, 760}, one) == juce::Rectangle<int>(100, 80, 1200, 760));
    // Saved on a second monitor that has been unplugged: pulled back onto the remaining one.
    auto moved = G::sanitise({2200, 100, 1200, 800}, one);
    CHECK(one.getFirst().contains(moved));
    // Still valid while that monitor exists.
    CHECK(G::sanitise({2200, 100, 1200, 800}, two).getX() >= 1920);
    // Title bar above the screen.
    CHECK(G::sanitise({100, -300, 1200, 800}, one).getY() >= 0);
    // Absurdly small or large sizes are corrected.
    auto tiny = G::sanitise({100, 100, 200, 100}, one);
    CHECK(tiny.getWidth() >= G::kMinWidth && tiny.getHeight() >= G::kMinHeight);
    auto huge = G::sanitise({0, 0, 9000, 9000}, one);
    CHECK(one.getFirst().contains(huge));
    // Empty rectangle -> sensible default.
    auto def = G::sanitise({}, one);
    CHECK(def.getWidth() >= G::kMinWidth);
    // Largest allowed size is the biggest monitor.
    CHECK(G::maximumSize(two).x == 2560 && G::maximumSize(two).y == 1400);
}

RT_TEST(main_window_content_lays_out_on_every_page_at_laptop_and_desktop_sizes)
{
    Rig rig("app_pages");
    app::MainView view(rig.ctx, [] {});
    CHECK(view.minimumWidth() <= WindowGeometry::kMinWidth);  // all tabs fit in the minimum window

    struct Size { int w, h; };
    const Size sizes[] = {{900, 580}, {1024, 700}, {1280, 800}, {1920, 1040}, {2560, 1400}};
    const char* names[] = {"play", "midi", "audio", "library", "settings", "about"};
    for (auto s : sizes)
    {
        view.setSize(s.w, s.h);
        for (int p = 0; p < 6; ++p)
        {
            view.showPage(static_cast<app::MainView::Page>(p));
            juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            auto* page = view.shell().page(p);
            CHECK(page->isVisible());
            CHECK(page->getWidth() == s.w);
            // Content pages scroll vertically; nothing may be wider than the page.
            if (auto* vp = page->findChildWithID("never"))
                (void)vp;
            for (auto* child : page->getChildren())
                CHECK_MSG(child->getRight() <= s.w + 1, names[p] << " child extends past the window at " << s.w);
            savePng(view, juce::String("page_") + names[p] + "_" + juce::String(s.w) + "x" + juce::String(s.h));
        }
    }
}

RT_TEST(midi_workflow_renders_a_wav_and_never_touches_the_source)
{
    Rig rig("app_midi");
    app::MainView view(rig.ctx, [] {});
    view.setSize(1240, 800);
    auto midi = melodyMidi(rig.dir);
    const auto before = hashOf(midi);

    view.openFile(midi);
    CHECK(view.midiPanel().hasMidi());
    CHECK(view.shell().currentPage() == app::MainView::Midi);

    const auto out = juce::File(juce::String((rig.dir / "out.wav").string()));
    view.midiPanel().renderTo(out, false);
    CHECK(view.midiPanel().isBusy());
    CHECK(waitIdle([&] { return view.midiPanel().isBusy(); }));
    CHECK_MSG(out.existsAsFile(), view.midiPanel().lastResult());
    CHECK(view.midiPanel().lastResult().startsWith("Saved"));

    auto back = decodeAudioFile(std::filesystem::path(out.getFullPathName().toStdString()));
    CHECK(back.ok);
    CHECK(back.clip.numChannels() == 2);
    CHECK_NEAR(back.clip.sampleRate, 48000.0, 0.5);
    CHECK(back.clip.durationSeconds() >= 2.5 && back.clip.durationSeconds() < 6.0);  // five half-second notes plus release
    CHECK(back.clip.peak() > 0.05f && back.clip.peak() <= 1.0f);
    CHECK_NEAR(estimateHz(back.clip.channels[0].data() + 4800, 8192, 48000.0) / midiToHz(60), 1.0, 0.01);
    CHECK(hashOf(midi) == before);

    // Re-rendering to the same file without permission is refused and leaves it intact.
    const auto sizeBefore = out.getSize();
    view.midiPanel().renderTo(out, false);
    waitIdle([&] { return view.midiPanel().isBusy(); });
    CHECK(view.midiPanel().lastResult().startsWith("Could not save"));
    CHECK(out.getSize() == sizeBefore);

    // Writing over the source is refused even when overwriting is allowed.
    view.midiPanel().renderTo(midi, true);
    waitIdle([&] { return view.midiPanel().isBusy(); });
    CHECK(hashOf(midi) == before);
    CHECK(view.midiPanel().lastResult().contains("Could not save"));
}

RT_TEST(midi_workflow_reports_bad_files_and_missing_instruments)
{
    Rig rig("app_midi_bad");
    app::MainView view(rig.ctx, [] {});
    view.setSize(1240, 800);

    const auto junk = juce::File(juce::String((rig.dir / "junk.mid").string()));
    junk.replaceWithText("definitely not midi");
    view.openFile(junk);
    CHECK(!view.midiPanel().hasMidi());
    CHECK(view.midiPanel().lastResult().contains("Cannot use this file"));

    // A valid file but no instrument installed for the selection: the render says so instead of making sound.
    view.openFile(melodyMidi(rig.dir));
    CHECK(view.midiPanel().hasMidi());
    const InstrumentEntry* missing = nullptr;
    for (const auto& e : rig.ctx.processor.instruments().entries())
        if (!e.external && e.state == InstallState::NotInstalled)
            missing = &e;
    CHECK(missing != nullptr);
    if (missing != nullptr)
    {
        rig.ctx.processor.chooseInstrument(juce::String(missing->id), false);
        view.midiPanel().renderTo(juce::File(juce::String((rig.dir / "nope.wav").string())), false);
        waitIdle([&] { return view.midiPanel().isBusy(); });
        CHECK(!juce::File(juce::String((rig.dir / "nope.wav").string())).exists());
        CHECK(view.midiPanel().lastResult().contains("Render failed") || view.midiPanel().lastResult().contains("not"));
    }
}

RT_TEST(audio_workflow_converts_a_clean_melody_and_keeps_the_original)
{
    Rig rig("app_audio");
    app::MainView view(rig.ctx, [] {});
    view.setSize(1240, 800);
    const std::vector<int> melody = {60, 64, 67, 72, 67, 64};
    auto wav = melodyWav(rig.dir, melody);
    const auto before = hashOf(wav);

    view.openFile(wav);
    CHECK(view.shell().currentPage() == app::MainView::Audio);
    CHECK(waitIdle([&] { return view.audioPanel().isBusy(); }));
    CHECK(view.audioPanel().hasAudio());
    auto& notes = view.audioPanel().notes();
    CHECK_MSG(notes.size() == melody.size(), "found " << notes.size() << " notes");
    for (std::size_t i = 0; i < std::min(notes.size(), melody.size()); ++i)
        CHECK(notes[i].midiNote == melody[i]);

    // The user mutes one note; it is not rendered.
    notes[2].enabled = false;
    const auto out = juce::File(juce::String((rig.dir / "converted.wav").string()));
    view.audioPanel().convertTo(out, false);
    CHECK(waitIdle([&] { return view.audioPanel().isBusy(); }));
    CHECK_MSG(out.existsAsFile(), view.audioPanel().lastResult());
    auto back = decodeAudioFile(std::filesystem::path(out.getFullPathName().toStdString()));
    CHECK(back.ok && back.clip.numChannels() == 2);
    CHECK(back.clip.durationSeconds() > 2.5);
    CHECK(hashOf(wav) == before);

    // Converting onto the source file is refused.
    view.audioPanel().convertTo(wav, true);
    waitIdle([&] { return view.audioPanel().isBusy(); });
    CHECK(hashOf(wav) == before);
    CHECK(view.audioPanel().lastResult().contains("overwrite the source"));
}

RT_TEST(audio_workflow_imports_mp3_and_warns_about_unsuitable_sources)
{
    Rig rig("app_audio2");
    app::MainView view(rig.ctx, [] {});
    view.setSize(1240, 800);

    view.openFile(juce::File(juce::String((std::filesystem::path(REALTONE_TEST_DATA) / "sine_a4_mono.mp3").string())));
    CHECK(waitIdle([&] { return view.audioPanel().isBusy(); }));
    CHECK(view.audioPanel().hasAudio());
    CHECK(view.audioPanel().notes().size() == 1);
    if (!view.audioPanel().notes().empty())
        CHECK(view.audioPanel().notes()[0].midiNote == 69);

    // Corrupt file: clear message, page stays usable.
    const auto bad = juce::File(juce::String((rig.dir / "bad.wav").string()));
    bad.replaceWithText("RIFFxxxxWAVEnonsense");
    view.audioPanel().loadFile(bad);
    waitIdle([&] { return view.audioPanel().isBusy(); });
    CHECK(!view.audioPanel().hasAudio());
    CHECK(view.audioPanel().lastResult().contains("Cannot use this file"));

    // Dense noise: nothing trustworthy to convert, and the banner says so.
    AudioClip noise;
    noise.sampleRate = 44100.0;
    noise.channels.emplace_back(44100 * 2);
    std::mt19937 rng(5);
    std::normal_distribution<float> nd(0.0f, 0.2f);
    for (auto& s : noise.channels[0]) s = nd(rng);
    WavWriteOptions wo;
    wo.overwrite = true;
    const auto noisePath = rig.dir / "noise.wav";
    writeWavFile(noisePath, noise, wo);
    view.audioPanel().loadFile(juce::File(juce::String(noisePath.string())));
    waitIdle([&] { return view.audioPanel().isBusy(); });
    studio_test::pumpUntil([] { return false; }, 200);
    const auto banner = view.audioPanel().bannerText();
    CHECK(banner.isNotEmpty());
    CHECK(banner.contains("single") || banner.contains("confidence") || banner.contains("Nothing usable") || banner.contains("polyphonic") ||
          banner.contains("no single clear pitch"));
}

RT_TEST(conversion_can_be_cancelled_and_leaves_no_partial_file)
{
    Rig rig("app_cancel");
    app::MainView view(rig.ctx, [] {});
    view.setSize(1240, 800);
    // A long melody so the job is still running when we cancel.
    std::vector<int> melody;
    for (int i = 0; i < 40; ++i) melody.push_back(55 + (i * 5) % 20);
    auto wav = melodyWav(rig.dir, melody, "long.wav");
    view.openFile(wav);
    waitIdle([&] { return view.audioPanel().isBusy(); });
    CHECK(view.audioPanel().hasAudio());

    const auto out = juce::File(juce::String((rig.dir / "cancelled.wav").string()));
    view.audioPanel().convertTo(out, false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    // Cancel through the same path the Cancel button uses.
    for (auto* c : view.audioPanel().getChildren())
        (void)c;
    // The button lives inside the job bar; fire it by finding the text.
    std::function<void(juce::Component*)> clickCancel = [&](juce::Component* c) {
        if (auto* b = dynamic_cast<juce::TextButton*>(c))
            if (b->getButtonText() == "Cancel" && b->isVisible())
                b->triggerClick();
        for (auto* ch : c->getChildren())
            clickCancel(ch);
    };
    clickCancel(&view.audioPanel());
    CHECK(waitIdle([&] { return view.audioPanel().isBusy(); }));
    // Either it was cancelled before writing, or it finished first; never a half-written file.
    CHECK(!juce::File(out.getFullPathName() + ".part").exists());
    if (out.existsAsFile())
        CHECK(decodeAudioFile(std::filesystem::path(out.getFullPathName().toStdString())).ok);
}

RT_TEST(closing_the_window_while_a_job_runs_is_safe)
{
    Rig rig("app_shutdown");
    std::vector<int> melody;
    for (int i = 0; i < 40; ++i) melody.push_back(55 + (i * 3) % 24);
    auto wav = melodyWav(rig.dir, melody, "long.wav");
    {
        auto view = std::make_unique<app::MainView>(rig.ctx, [] {});
        view->setSize(1240, 800);
        view->openFile(wav);
        studio_test::pumpUntil([] { return false; }, 100);
        // destroyed while import/analysis may still be running
    }
    studio_test::pumpUntil([] { return false; }, 300);
    CHECK(true);
}

RT_TEST(projects_save_and_restore_instrument_settings_and_page_state)
{
    Rig rig("app_project");
    app::MainView view(rig.ctx, [] {});
    view.setSize(1240, 800);
    auto midi = melodyMidi(rig.dir);
    view.openFile(midi);
    rig.ctx.processor.apvts.getParameter("vibdepth")->setValueNotifyingHost(0.4f);
    rig.ctx.processor.apvts.getParameter("attack")->setValueNotifyingHost(0.3f);
    const auto vib = rig.ctx.processor.apvts.getRawParameterValue("vibdepth")->load();

    const auto project = juce::File(juce::String((rig.dir / "song.rtproject").string()));
    juce::String err;
    CHECK(view.saveProject(project, err));
    CHECK(project.existsAsFile());
    CHECK(!project.withFileExtension("tmp").exists());

    // New session
    Rig other("app_project2");
    other.ctx.processor.instruments().addExternalSfz(juce::File(juce::String((rig.dir / "inst" / "test.sfz").string())), "Fixture", "Test", "generic");
    app::MainView view2(other.ctx, [] {});
    view2.setSize(1240, 800);
    CHECK(view2.loadProject(project, err));
    CHECK_NEAR(other.ctx.processor.apvts.getRawParameterValue("vibdepth")->load(), vib, 0.2);
    CHECK(view2.midiPanel().hasMidi());
    CHECK(view2.midiPanel().midiFile() == midi);
    CHECK(other.ctx.processor.instruments().selectedId() == rig.instrumentId);

    // Broken and foreign project files are rejected with a message.
    const auto junk = juce::File(juce::String((rig.dir / "junk.rtproject").string()));
    junk.replaceWithText("not xml");
    CHECK(!view2.loadProject(junk, err) && err.isNotEmpty());
    junk.replaceWithText("<Something/>");
    CHECK(!view2.loadProject(junk, err));
    junk.replaceWithText("<RealToneProject version=\"9\"/>");
    CHECK(!view2.loadProject(junk, err));
}

RT_TEST(drag_and_drop_accepts_supported_files_only)
{
    Rig rig("app_dnd");
    app::MainView view(rig.ctx, [] {});
    CHECK(view.isInterestedInFileDrag({"song.mid"}));
    CHECK(view.isInterestedInFileDrag({"take.MP3"}));
    CHECK(view.isInterestedInFileDrag({"a.wav"}));
    CHECK(view.isInterestedInFileDrag({"x.rtproject"}));
    CHECK(!view.isInterestedInFileDrag({"notes.txt", "photo.jpg"}));
}

RT_TEST(pages_with_content_render_for_visual_review)
{
    Rig rig("app_visual");
    app::MainView view(rig.ctx, [] {});
    for (auto size : {juce::Point<int>(1240, 800), juce::Point<int>(900, 580)})
    {
        view.setSize(size.x, size.y);
        view.openFile(melodyMidi(rig.dir));
        view.showPage(app::MainView::Midi);
        savePng(view, "content_midi_" + juce::String(size.x));

        std::vector<int> melody = {60, 62, 64, 65, 67, 69, 71, 72, 71, 67, 64, 60};
        view.openFile(melodyWav(rig.dir, melody));
        studio_test::pumpUntil([&] { return view.audioPanel().notes().size() >= 12 && !view.audioPanel().isBusy(); }, 30000);
        if (view.audioPanel().notes().size() > 3)
            view.audioPanel().notes()[3].enabled = false;
        view.showPage(app::MainView::Audio);
        view.audioPanel().resized();
        savePng(view, "content_audio_" + juce::String(size.x));
    }
    CHECK(true);
}
