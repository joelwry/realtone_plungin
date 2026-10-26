#pragma once

#include "Theme.h"

#include "../studio/InstrumentManager.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

// Install, repair, verify and remove instrument libraries; choose where they live; import
// SFZ libraries the user already owns; read each library's license and attribution.
class LibraryPanel : public juce::Component, private juce::ListBoxModel, private juce::ChangeListener
{
public:
    explicit LibraryPanel(realtone::studio::InstrumentManager& manager);
    ~LibraryPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged(int) override { updateDetails(); }

    juce::ListBox& list() { return list_; }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    const realtone::InstrumentEntry* selectedEntry() const;
    void updateDetails();
    void chooseLibraryFolder();
    void importSfz();

    realtone::studio::InstrumentManager& manager_;
    juce::ListBox list_{"instruments", this};
    juce::TextButton install_{"Install"}, verify_{"Verify"}, remove_{"Remove"}, cancel_{"Cancel"};
    juce::TextButton folder_{"Change location..."}, openFolder_{"Open folder"}, import_{"Import SFZ library..."};
    juce::Label folderLabel_, jobLabel_;
    juce::ProgressBar bar_;
    double progress_ = 0.0;
    juce::TextEditor details_;
    std::unique_ptr<juce::FileChooser> chooser_;
    std::vector<const realtone::InstrumentEntry*> rows_;
};

}  // namespace realtone::ui
