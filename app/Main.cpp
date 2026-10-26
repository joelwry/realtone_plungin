#include "MainView.h"

#include "../studio/AppContext.h"
#include "../studio/Paths.h"
#include "../studio/Settings.h"
#include "../studio/WindowGeometry.h"
#include "../ui/Theme.h"

#include <juce_gui_extra/juce_gui_extra.h>

using namespace realtone;
using namespace realtone::studio;

namespace
{

juce::Array<juce::Rectangle<int>> workAreas()
{
    juce::Array<juce::Rectangle<int>> areas;
    for (const auto& d : juce::Desktop::getInstance().getDisplays().displays)
        areas.add(d.userArea);
    return areas;
}

}  // namespace

class MainWindow : public juce::DocumentWindow, public juce::MenuBarModel, private juce::Timer
{
public:
    explicit MainWindow(AppContext& context)
        : DocumentWindow("RealTone Studio", ui::theme::background, DocumentWindow::allButtons), ctx_(context)
    {
        // Real operating-system window frame: native title bar, drag, double-click to maximise,
        // and edge/corner resizing all come from the system.
        setUsingNativeTitleBar(true);
        setResizable(true, true);

        view_ = new app::MainView(context, [this] { resetGeometry(); });
        setContentOwned(view_, false);

        const auto areas = workAreas();
        const auto maxSize = WindowGeometry::maximumSize(areas);
        const int minW = juce::jmax(WindowGeometry::kMinWidth, view_->minimumWidth());
        setResizeLimits(minW, WindowGeometry::kMinHeight, maxSize.x, maxSize.y);

        restoreGeometry();
        setMenuBar(this);
        setVisible(true);
        startTimer(600);
    }

    ~MainWindow() override
    {
        stopTimer();
        setMenuBar(nullptr);
        saveGeometry();
        clearContentComponent();
    }

    app::MainView& view() { return *view_; }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }

    void resetGeometry()
    {
        Settings::get().setString(keys::windowState, {});
        setFullScreen(false);
        const auto areas = workAreas();
        setBounds(WindowGeometry::initialBounds(areas.isEmpty() ? juce::Rectangle<int>(0, 0, 1280, 800) : areas.getFirst()));
    }

    // MenuBarModel ---------------------------------------------------------------------
    juce::StringArray getMenuBarNames() override { return {"File", "View", "Help"}; }

    juce::PopupMenu getMenuForIndex(int index, const juce::String&) override
    {
        juce::PopupMenu m;
        if (index == 0)
        {
            m.addItem(1, "Open MIDI file...");
            m.addItem(2, "Open audio file...");
            m.addSeparator();
            m.addItem(3, "Open project...");
            m.addItem(4, "Save project", view_->currentProject() != juce::File());
            m.addItem(5, "Save project as...");
            juce::PopupMenu recent;
            const auto list = Settings::get().getList(keys::recentProjects);
            for (int i = 0; i < list.size(); ++i)
                recent.addItem(100 + i, juce::File(list[i]).getFileName(), juce::File(list[i]).existsAsFile());
            m.addSubMenu("Recent projects", recent, list.size() > 0);
            m.addSeparator();
            m.addItem(9, "Quit");
        }
        else if (index == 1)
        {
            m.addItem(20, "Play");
            m.addItem(21, "MIDI to WAV");
            m.addItem(22, "Audio to Instrument");
            m.addItem(23, "Library");
            m.addSeparator();
            m.addItem(30, "Reset window size and position");
        }
        else
        {
            m.addItem(40, "About RealTone Studio");
            m.addItem(41, "Open log folder");
        }
        return m;
    }

    void menuItemSelected(int id, int) override
    {
        switch (id)
        {
            case 1: view_->showPage(app::MainView::Midi); break;
            case 2: view_->showPage(app::MainView::Audio); break;
            case 3: openProjectDialog(); break;
            case 4: { juce::String e; if (!view_->saveProject(view_->currentProject(), e)) showError("Save project", e); break; }
            case 5: saveProjectAs(); break;
            case 9: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 20: view_->showPage(app::MainView::Play); break;
            case 21: view_->showPage(app::MainView::Midi); break;
            case 22: view_->showPage(app::MainView::Audio); break;
            case 23: view_->showPage(app::MainView::Library); break;
            case 30: resetGeometry(); break;
            case 40: view_->showPage(app::MainView::About); break;
            case 41: logDirectory().createDirectory(); logDirectory().revealToUser(); break;
            default:
                if (id >= 100)
                {
                    const auto list = Settings::get().getList(keys::recentProjects);
                    if (id - 100 < list.size())
                        view_->openFile(juce::File(list[id - 100]));
                }
                break;
        }
        if (id == 1)
            view_->midiPanel().resized();  // keep the page fresh after switching
    }

private:
    void timerCallback() override
    {
        // Debounced: remember the window layout shortly after the user stops moving or resizing it.
        const auto now = getWindowStateAsString();
        if (now != lastSaved_)
        {
            lastSaved_ = now;
            Settings::get().setString(keys::windowState, now);
        }
    }

    void saveGeometry()
    {
        lastSaved_ = getWindowStateAsString();
        Settings::get().setString(keys::windowState, lastSaved_);
    }

    void restoreGeometry()
    {
        const auto areas = workAreas();
        const auto state = Settings::get().getString(keys::windowState);
        bool restored = false;
        if (state.isNotEmpty())
            restored = restoreWindowStateFromString(state);
        if (!restored)
        {
            setBounds(WindowGeometry::initialBounds(areas.isEmpty() ? juce::Rectangle<int>(0, 0, 1280, 800) : areas.getFirst()));
        }
        else if (!isFullScreen())
        {
            // A saved position may refer to a monitor that is no longer there.
            const auto fixed = WindowGeometry::sanitise(getBounds(), areas);
            if (fixed != getBounds())
                setBounds(fixed);
        }
        lastSaved_ = getWindowStateAsString();
    }

    void showError(const juce::String& title, const juce::String& message)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, message);
    }

    void openProjectDialog()
    {
        chooser_ = std::make_unique<juce::FileChooser>("Open a project", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.rtproject");
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
            if (fc.getResult() != juce::File())
                openProject(fc.getResult());
        });
    }

    void saveProjectAs()
    {
        chooser_ = std::make_unique<juce::FileChooser>("Save project as", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Untitled.rtproject"),
                                                       "*.rtproject");
        chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this](const juce::FileChooser& fc) {
                                  if (fc.getResult() == juce::File())
                                      return;
                                  juce::String error;
                                  const auto file = fc.getResult().withFileExtension("rtproject");
                                  if (!view_->saveProject(file, error))
                                      showError("Save project", error);
                                  else
                                      rememberProject(file);
                              });
    }

    void openProject(const juce::File& file)
    {
        juce::String error;
        if (!view_->loadProject(file, error))
            showError("Open project", error);
        else
            rememberProject(file);
    }

    void rememberProject(const juce::File& file)
    {
        auto list = Settings::get().getList(keys::recentProjects);
        list.removeString(file.getFullPathName());
        list.insert(0, file.getFullPathName());
        while (list.size() > 8)
            list.remove(list.size() - 1);
        Settings::get().setList(keys::recentProjects, list);
        menuItemsChanged();
    }

    AppContext& ctx_;
    app::MainView* view_ = nullptr;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::String lastSaved_;
};

class RealToneApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "RealTone Studio"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override
    {
        logMessage("RealTone Studio " + getApplicationVersion() + " starting");
        juce::LookAndFeel::setDefaultLookAndFeel(&laf_);

        context_ = std::make_unique<AppContext>();
        window_ = std::make_unique<MainWindow>(*context_);

        const auto last = Settings::get().getString(keys::lastInstrument);
        if (last.isNotEmpty() && context_->processor.instruments().find(last) != nullptr)
            context_->processor.chooseInstrument(last, false);
        else if (auto& e = context_->processor.instruments().entries(); !e.empty())
        {
            // First start: choose the first instrument that is actually installed, if any.
            for (const auto& entry : e)
                if (entry.state == realtone::InstallState::Installed)
                {
                    context_->processor.chooseInstrument(juce::String(entry.id), true);
                    break;
                }
        }
        openFromCommandLine(commandLine);
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        if (window_ != nullptr)
            window_->toFront(true);
        openFromCommandLine(commandLine);
    }

    void shutdown() override
    {
        window_.reset();
        context_.reset();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        logMessage("RealTone Studio closed");
    }

    void systemRequestedQuit() override { quit(); }

private:
    void openFromCommandLine(const juce::String& commandLine)
    {
        const auto args = juce::StringArray::fromTokens(commandLine, true);
        for (const auto& a : args)
        {
            const juce::File f(a.unquoted());
            if (f.existsAsFile() && window_ != nullptr)
            {
                window_->view().openFile(f);
                break;
            }
        }
    }

    realtone::ui::RealToneLookAndFeel laf_;
    std::unique_ptr<AppContext> context_;
    std::unique_ptr<MainWindow> window_;
};

START_JUCE_APPLICATION(RealToneApp)
