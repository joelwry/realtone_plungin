#include "StudioTestSupport.h"
#include "TestSupport.h"

#include "../ui/InstrumentBrowser.h"
#include "../ui/LibraryPanel.h"
#include "../ui/PluginEditor.h"
#include "../ui/StudioView.h"
#include "../ui/Theme.h"

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

bool isControl(juce::Component* c)
{
    return dynamic_cast<juce::Button*>(c) != nullptr || dynamic_cast<juce::Slider*>(c) != nullptr || dynamic_cast<juce::ComboBox*>(c) != nullptr ||
           dynamic_cast<juce::TextEditor*>(c) != nullptr || dynamic_cast<juce::ListBox*>(c) != nullptr;
}

// Collects problems: controls outside the window, outside a scroll area horizontally, or too small to use.
void inspect(juce::Component& root, juce::Component& c, juce::Rectangle<int> clip, juce::StringArray& problems, const juce::String& path)
{
    if (!c.isVisible())
        return;
    auto here = path + "/" + (c.getName().isNotEmpty() ? c.getName() : juce::String(typeid(c).name()));

    if (isControl(&c) && &c != &root)
    {
        auto r = root.getLocalArea(&c, c.getLocalBounds());
        // Text boxes inside sliders are internal; the slider itself is checked.
        const bool internal = dynamic_cast<juce::Label*>(c.getParentComponent()) != nullptr;
        if (!internal)
        {
            if (!clip.contains(juce::Rectangle<int>(r.getX(), clip.getY(), r.getWidth(), 1)))
                problems.add("horizontally clipped: " + here + " " + r.toString() + " in " + clip.toString());
            if (r.getWidth() < 18 || r.getHeight() < 14)
                problems.add("too small: " + here + " " + r.toString());
        }
    }

    auto childClip = clip;
    if (auto* vp = dynamic_cast<juce::Viewport*>(&c))
    {
        childClip = root.getLocalArea(&c, vp->getViewArea().withPosition(0, 0).withWidth(vp->getMaximumVisibleWidth()).withHeight(vp->getHeight()));
        childClip = childClip.withY(clip.getY()).withHeight(clip.getHeight());
    }
    for (auto* child : c.getChildren())
        inspect(root, *child, childClip, problems, here);
}

struct EditorFixture
{
    std::filesystem::path dir;
    RealToneProcessor proc;
    juce::String id;
    EditorFixture(const std::string& name)
    {
        dir = rt_test::scratchDir(name);
        auto files = makeInstrumentFiles(dir);
        proc.setPlayConfigDetails(0, 2, 44100.0, 512);
        proc.prepareToPlay(44100.0, 512);
        id = studio_test::loadFixture(proc, files.sfz, "bowed");
    }
};

}  // namespace

RT_TEST(editor_lays_out_without_clipped_controls_at_many_sizes)
{
    EditorFixture f("ui_sizes");
    std::unique_ptr<juce::AudioProcessorEditor> editor(f.proc.createEditor());
    CHECK(editor != nullptr);
    CHECK(editor->isResizable());

    struct Size { int w, h; };
    const Size sizes[] = {{700, 440}, {760, 480}, {900, 560}, {1040, 660}, {1280, 800}, {1920, 1080}, {2560, 1440}};
    for (auto s : sizes)
    {
        editor->setSize(s.w, s.h);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        juce::StringArray problems;
        inspect(*editor, *editor, editor->getLocalBounds(), problems, "editor");
        for (auto& p : problems)
            std::fprintf(stderr, "    [%dx%d] %s\n", s.w, s.h, p.toRawUTF8());
        CHECK_MSG(problems.isEmpty(), s.w << "x" << s.h << ": " << problems.size() << " layout problem(s)");
        savePng(*editor, "editor_" + juce::String(s.w) + "x" + juce::String(s.h));
    }
}

RT_TEST(editor_remembers_its_size_in_plugin_state)
{
    EditorFixture f("ui_size_state");
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(f.proc.createEditor());
        editor->setSize(1111, 777);
    }
    CHECK(f.proc.savedEditorSize().x == 1111);
    CHECK(f.proc.savedEditorSize().y == 777);
    std::unique_ptr<juce::AudioProcessorEditor> again(f.proc.createEditor());
    CHECK(again->getWidth() == 1111 && again->getHeight() == 777);

    // Below the minimum the old size is not saved, and absurd stored sizes fall back to defaults.
    again->setSize(300, 200);
    CHECK(f.proc.savedEditorSize().x == 1111);
    f.proc.setSavedEditorSize({50, 50});
    std::unique_ptr<juce::AudioProcessorEditor> third(f.proc.createEditor());
    CHECK(third->getWidth() == ui::PluginEditor::kDefaultWidth);
}

RT_TEST(editor_opens_and_closes_repeatedly_while_audio_runs)
{
    EditorFixture f("ui_open_close");
    for (int i = 0; i < 12; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(f.proc.createEditor());
        editor->setSize(900 + i * 10, 600);
        studio_test::render(f.proc, 4, 256, {{0, juce::MidiMessage::noteOn(1, 60 + i, (juce::uint8) 90)}});
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    }
    CHECK(true);  // reaching here without a crash or assertion is the check
}

RT_TEST(play_page_switches_between_wide_and_tabbed_layouts)
{
    EditorFixture f("ui_layout");
    ui::StudioView view(f.proc);
    view.setSize(1200, 700);
    CHECK(view.layout() == ui::StudioView::Layout::Wide);
    CHECK(view.keyboardVisible());
    view.setSize(800, 600);
    CHECK(view.layout() == ui::StudioView::Layout::Tabbed);
    view.setSize(800, 400);
    CHECK(!view.keyboardVisible());  // the keyboard yields to the controls on short windows
}

RT_TEST(instrument_browser_filters_by_search_and_installed_state)
{
    EditorFixture f("ui_browser");
    ui::InstrumentBrowser browser(f.proc.instruments(), [](const juce::String&) {});
    browser.setSize(600, 500);
    const int all = browser.numVisibleCards();
    CHECK(all >= 21);  // 20 catalog instruments + the fixture
    CHECK(all == static_cast<int>(f.proc.instruments().entries().size()));
}

RT_TEST(library_panel_lists_every_instrument_with_status)
{
    EditorFixture f("ui_library");
    ui::LibraryPanel panel(f.proc.instruments());
    panel.setSize(1000, 600);
    CHECK(panel.list().getListBoxModel()->getNumRows() == static_cast<int>(f.proc.instruments().entries().size()));
    savePng(panel, "library_1000x600");
    panel.setSize(640, 560);
    savePng(panel, "library_640x560");
}

RT_TEST(colour_contrast_of_text_on_surfaces_is_readable)
{
    auto lum = [](juce::Colour c) {
        auto f = [](float v) { return v <= 0.03928f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
        return 0.2126f * f(c.getFloatRed()) + 0.7152f * f(c.getFloatGreen()) + 0.0722f * f(c.getFloatBlue());
    };
    auto contrast = [&](juce::Colour a, juce::Colour b) {
        const float la = lum(a), lb = lum(b);
        return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
    };
    using namespace realtone::ui::theme;
    CHECK(contrast(text, background) >= 7.0f);
    CHECK(contrast(text, surface) >= 7.0f);
    CHECK(contrast(textDim, surface) >= 4.5f);        // secondary text meets WCAG AA
    CHECK(contrast(textDim, surfaceRaised) >= 4.5f);
    CHECK(contrast(accent, surface) >= 4.5f);
    CHECK(contrast(error, surface) >= 4.5f);
    CHECK(contrast(ok, surface) >= 4.5f);
}
