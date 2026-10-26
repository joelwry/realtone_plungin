#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace realtone::ui
{

// Product header (name, tabs, version) with one page shown at a time. Used by both the plug-in
// editor and the standalone window so they look and navigate the same way.
class TabShell : public juce::Component
{
public:
    explicit TabShell(const juce::String& versionText);
    ~TabShell() override;

    void addPage(const juce::String& name, std::unique_ptr<juce::Component> page);
    void setCurrentPage(int index);
    int currentPage() const { return current_; }
    int numPages() const { return static_cast<int>(pages_.size()); }
    juce::Component* page(int index) { return pages_[static_cast<std::size_t>(index)].component.get(); }
    int minimumWidth() const;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kHeaderHeight = 48;

private:
    struct Page
    {
        juce::String name;
        std::unique_ptr<juce::Component> component;
        std::unique_ptr<juce::TextButton> button;
    };
    std::vector<Page> pages_;
    int current_ = 0;
    juce::String version_;
};

}  // namespace realtone::ui
