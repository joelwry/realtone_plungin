#include "LibraryPanel.h"

#include "InstrumentArt.h"

namespace realtone::ui
{

namespace
{
juce::String mb(std::uint64_t bytes)
{
    const double v = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return v >= 1024.0 ? juce::String(v / 1024.0, 2) + " GB" : juce::String(v, 0) + " MB";
}
}  // namespace

LibraryPanel::LibraryPanel(realtone::studio::InstrumentManager& manager) : manager_(manager), bar_(progress_)
{
    list_.setRowHeight(46);
    list_.setOutlineThickness(1);
    addAndMakeVisible(list_);

    install_.onClick = [this] { if (auto* e = selectedEntry()) manager_.install(e->id); };
    verify_.onClick = [this] { if (auto* e = selectedEntry()) manager_.verify(e->id); };
    remove_.onClick = [this] {
        if (auto* e = selectedEntry())
        {
            const juce::String id(e->id);
            const bool external = e->external;
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, external ? "Remove from list" : "Remove instrument",
                                               external ? "Remove this imported instrument from RealTone Studio? Your SFZ and sample files are not touched."
                                                        : "Delete the downloaded sample files for this instrument from this computer? You can install it again at any time. Files you added yourself are not touched.",
                                               "Remove", "Cancel", this,
                                               juce::ModalCallbackFunction::create([this, id, safe = juce::Component::SafePointer<LibraryPanel>(this)](int r) {
                                                   if (r == 1 && safe != nullptr)
                                                       manager_.remove(id);
                                               }));
        }
    };
    cancel_.onClick = [this] { manager_.cancelJob(); };
    folder_.onClick = [this] { chooseLibraryFolder(); };
    openFolder_.onClick = [this] { manager_.libraryRoot().createDirectory(); manager_.libraryRoot().revealToUser(); };
    import_.onClick = [this] { importSfz(); };
    for (auto* b : {&install_, &verify_, &remove_, &cancel_, &folder_, &openFolder_, &import_})
        addAndMakeVisible(b);
    cancel_.setVisible(false);

    folderLabel_.setFont(theme::mono(12.0f));
    folderLabel_.setColour(juce::Label::textColourId, theme::textDim);
    folderLabel_.setMinimumHorizontalScale(0.6f);
    addAndMakeVisible(folderLabel_);
    jobLabel_.setFont(theme::ui(13.0f));
    jobLabel_.setColour(juce::Label::textColourId, theme::textDim);
    addAndMakeVisible(jobLabel_);
    addChildComponent(bar_);

    details_.setMultiLine(true, true);
    details_.setReadOnly(true);
    details_.setCaretVisible(false);
    details_.setScrollbarsShown(true);
    details_.setFont(theme::ui(13.0f));
    addAndMakeVisible(details_);

    manager_.addChangeListener(this);
    changeListenerCallback(nullptr);
}

LibraryPanel::~LibraryPanel() { manager_.removeChangeListener(this); }

const realtone::InstrumentEntry* LibraryPanel::selectedEntry() const
{
    const int row = list_.getSelectedRow();
    return row >= 0 && row < static_cast<int>(rows_.size()) ? rows_[static_cast<std::size_t>(row)] : nullptr;
}

int LibraryPanel::getNumRows() { return static_cast<int>(rows_.size()); }

void LibraryPanel::paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected)
{
    using namespace theme;
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return;
    const auto& e = *rows_[static_cast<std::size_t>(row)];
    if (selected)
    {
        g.setColour(accentSoft);
        g.fillRect(0, 0, width, height);
    }
    g.setColour(border);
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));

    drawInstrumentArt(g, {8.0f, 7.0f, static_cast<float>(height) - 14.0f, static_cast<float>(height) - 14.0f},
                      realtone::familyFromString(e.family), e.state != realtone::InstallState::Installed);
    const int x = height + 6;
    g.setColour(text);
    g.setFont(ui(14.0f, true));
    g.drawText(juce::String(e.name), x, 4, width - x - 150, 20, juce::Justification::centredLeft);
    g.setColour(textFaint);
    g.setFont(ui(12.0f));
    g.drawText(juce::String(e.packName) + "  -  " + juce::String(e.category), x, 24, width - x - 150, 16, juce::Justification::centredLeft);

    juce::Colour c = ok;
    switch (e.state)
    {
        case realtone::InstallState::Installed: c = ok; break;
        case realtone::InstallState::NotInstalled: c = textFaint; break;
        case realtone::InstallState::Incomplete: c = warn; break;
        case realtone::InstallState::Damaged: c = error; break;
    }
    g.setColour(c);
    g.setFont(ui(12.5f, true));
    juce::String state = e.external && e.state == realtone::InstallState::Installed ? "Imported" : juce::String(realtone::toString(e.state));
    g.drawText(state, width - 140, 6, 130, 18, juce::Justification::centredRight);
    g.setColour(textFaint);
    g.setFont(ui(12.0f));
    g.drawText(e.external ? juce::String() : mb(e.downloadBytes), width - 140, 24, 130, 16, juce::Justification::centredRight);
}

void LibraryPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    const auto selectedId = selectedEntry() != nullptr ? juce::String(selectedEntry()->id) : juce::String();
    rows_.clear();
    for (const auto& e : manager_.entries())
        rows_.push_back(&e);
    list_.updateContent();
    for (int i = 0; i < static_cast<int>(rows_.size()); ++i)
        if (juce::String(rows_[static_cast<std::size_t>(i)]->id) == selectedId)
            list_.selectRow(i, true, false);
    if (list_.getSelectedRow() < 0 && !rows_.empty())
        list_.selectRow(0, true, false);
    list_.repaint();

    folderLabel_.setText(manager_.libraryRoot().getFullPathName(), juce::dontSendNotification);
    const auto job = manager_.job();
    bar_.setVisible(job.running);
    cancel_.setVisible(job.running);
    progress_ = job.progress;
    jobLabel_.setText(job.error.isNotEmpty() ? "Error: " + job.error : (job.title + "  " + job.message).trim(), juce::dontSendNotification);
    jobLabel_.setColour(juce::Label::textColourId, job.error.isNotEmpty() ? theme::error : theme::textDim);
    for (auto* b : {&install_, &verify_, &remove_, &folder_, &import_})
        b->setEnabled(!job.running);
    updateDetails();
    resized();
}

void LibraryPanel::updateDetails()
{
    const auto* e = selectedEntry();
    juce::String t;
    if (e == nullptr)
        t = "Select an instrument to see its details.";
    else
    {
        t << juce::String(e->name) << "\n" << juce::String(e->description) << "\n\n";
        t << "Status: " << realtone::toString(e->state);
        if (e->missingFiles > 0) t << "  (" << e->missingFiles << " file(s) missing)";
        if (e->damagedFiles > 0) t << "  (" << e->damagedFiles << " file(s) damaged)";
        t << "\n";
        if (!e->external)
            t << "Download size: " << mb(e->downloadBytes) << "\n";
        t << "Location: " << juce::String(e->sfzPath.string()) << "\n\n";
        t << "License: " << juce::String(e->license.name) << "\n";
        if (e->license.url.size() > 0) t << juce::String(e->license.url) << "\n";
        if (!e->license.attribution.empty()) t << "\n" << juce::String(e->license.attribution) << "\n";
        if (e->external)
            t << "\nThis library was imported by you. RealTone Studio does not verify its files or its license.\n";
        const auto report = manager_.lastVerifyReport();
        if (report.isNotEmpty() && report.startsWith(juce::String(e->name)))
            t << "\nLast check: " << report << "\n";
    }
    details_.setText(t, false);

    install_.setButtonText(e != nullptr && e->state == realtone::InstallState::Installed ? "Reinstall" : (e != nullptr && e->state != realtone::InstallState::NotInstalled ? "Repair" : "Install"));
    const bool managed = e != nullptr && !e->external;
    install_.setEnabled(managed && !manager_.job().running);
    verify_.setEnabled(managed && e->state != realtone::InstallState::NotInstalled && !manager_.job().running);
    remove_.setEnabled(e != nullptr && (e->external || e->state != realtone::InstallState::NotInstalled) && !manager_.job().running);
    remove_.setButtonText(e != nullptr && e->external ? "Remove from list" : "Remove");
}

void LibraryPanel::chooseLibraryFolder()
{
    chooser_ = std::make_unique<juce::FileChooser>("Choose where instrument libraries are stored", manager_.libraryRoot(), "", true);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this, safe = juce::Component::SafePointer<LibraryPanel>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr)
                                  return;
                              const auto dir = fc.getResult();
                              if (dir != juce::File())
                                  manager_.setLibraryRoot(dir);
                          });
}

void LibraryPanel::importSfz()
{
    chooser_ = std::make_unique<juce::FileChooser>("Choose an SFZ instrument you are licensed to use", juce::File(), "*.sfz");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, safe = juce::Component::SafePointer<LibraryPanel>(this)](const juce::FileChooser& fc) {
                              if (safe == nullptr || fc.getResult() == juce::File())
                                  return;
                              const auto file = fc.getResult();
                              auto* w = new juce::AlertWindow("Import SFZ instrument",
                                                              "RealTone Studio plays your SFZ file and its samples where they are. It does not copy them, "
                                                              "and it cannot check that you have the right to use them.",
                                                              juce::MessageBoxIconType::NoIcon, this);
                              w->addTextEditor("name", file.getFileNameWithoutExtension(), "Name");
                              w->addComboBox("family", {"piano", "electric-keys", "guitar", "bass", "bowed", "brass", "woodwind", "percussion", "mallet", "organ", "harp", "synth", "generic"}, "Behaves like");
                              w->getComboBoxComponent("family")->setSelectedItemIndex(12);
                              w->addButton("Import", 1, juce::KeyPress(juce::KeyPress::returnKey));
                              w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
                              w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w, file](int result) {
                                  std::unique_ptr<juce::AlertWindow> owner(w);
                                  if (result != 1 || safe == nullptr)
                                      return;
                                  safe->manager_.addExternalSfz(file, w->getTextEditorContents("name"), "Imported", w->getComboBoxComponent("family")->getText());
                              }), false);
                          });
}

void LibraryPanel::paint(juce::Graphics& g)
{
    g.setColour(theme::background);
    g.fillAll();
}

void LibraryPanel::resized()
{
    constexpr int gap = 10;
    auto r = getLocalBounds().reduced(gap);

    // Location row
    auto top = r.removeFromTop(theme::controlHeight);
    import_.setBounds(top.removeFromRight(180));
    top.removeFromRight(6);
    openFolder_.setBounds(top.removeFromRight(110));
    top.removeFromRight(6);
    folder_.setBounds(top.removeFromRight(160));
    top.removeFromRight(8);
    folderLabel_.setBounds(top);
    r.removeFromTop(gap);

    // Job status at the bottom
    auto bottom = r.removeFromBottom(bar_.isVisible() ? 44 : 24);
    if (bar_.isVisible())
    {
        cancel_.setBounds(bottom.removeFromRight(80).removeFromBottom(26));
        bottom.removeFromRight(6);
        jobLabel_.setBounds(bottom.removeFromTop(20));
        bar_.setBounds(bottom.removeFromTop(12));
    }
    else
        jobLabel_.setBounds(bottom);
    r.removeFromBottom(6);

    if (r.getWidth() >= 760)
    {
        auto left = r.removeFromLeft(juce::jlimit(380, 560, r.getWidth() * 55 / 100));
        auto buttons = left.removeFromBottom(theme::controlHeight);
        list_.setBounds(left.withTrimmedBottom(gap));
        r.removeFromLeft(gap);
        install_.setBounds(buttons.removeFromLeft(110));
        buttons.removeFromLeft(6);
        verify_.setBounds(buttons.removeFromLeft(90));
        buttons.removeFromLeft(6);
        remove_.setBounds(buttons.removeFromLeft(140));
        details_.setBounds(r);
    }
    else
    {
        auto buttons = r.removeFromBottom(theme::controlHeight);
        const int third = (buttons.getWidth() - 12) / 3;
        install_.setBounds(buttons.removeFromLeft(third));
        buttons.removeFromLeft(6);
        verify_.setBounds(buttons.removeFromLeft(third));
        buttons.removeFromLeft(6);
        remove_.setBounds(buttons);
        r.removeFromBottom(gap);
        const int detailsH = juce::jlimit(90, 200, r.getHeight() / 3);
        details_.setBounds(r.removeFromBottom(detailsH));
        r.removeFromBottom(gap);
        list_.setBounds(r);
    }
}

}  // namespace realtone::ui
