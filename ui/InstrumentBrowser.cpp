#include "../studio/PathUtil.h"
#include "InstrumentBrowser.h"

#include "InstrumentArt.h"

#include "realtone/InstrumentProfile.h"

namespace realtone::ui
{

using realtone::studio::InstrumentManager;

class InstrumentBrowser::Card : public juce::Component, public juce::SettableTooltipClient
{
public:
    Card(InstrumentManager& m, const realtone::InstrumentEntry& e, bool selected, std::function<void(const juce::String&)> choose)
        : manager_(m), id_(e.id), name_(e.name), category_(e.category), description_(e.description), state_(e.state),
          family_(realtone::familyFromString(e.family)), selected_(selected), choose_(std::move(choose)),
          bytes_(e.downloadBytes), external_(e.external), missing_(e.missingFiles + e.damagedFiles)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip(description_.isNotEmpty() ? description_ : name_);
        if (state_ != realtone::InstallState::Installed && !external_)
        {
            action_.setButtonText(state_ == realtone::InstallState::NotInstalled
                                      ? "Install  " + sizeText(bytes_)
                                      : "Repair  " + sizeText(bytes_));
            action_.onClick = [this] { manager_.install(id_); };
            addAndMakeVisible(action_);
        }
        setWantsKeyboardFocus(true);
    }

    static juce::String sizeText(std::uint64_t b)
    {
        const double mb = static_cast<double>(b) / (1024.0 * 1024.0);
        return mb >= 1024.0 ? juce::String(mb / 1024.0, 1) + " GB" : juce::String(juce::roundToInt(mb)) + " MB";
    }

    void paint(juce::Graphics& g) override
    {
        using namespace theme;
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        const bool installed = state_ == realtone::InstallState::Installed;
        g.setColour(selected_ ? accentSoft : (isMouseOver() ? surfaceHover : surfaceRaised));
        g.fillRoundedRectangle(r, static_cast<float>(radius));
        g.setColour(selected_ ? accent : border);
        g.drawRoundedRectangle(r, static_cast<float>(radius), selected_ ? 1.5f : 1.0f);

        const float art = static_cast<float>(juce::jmin(getHeight() - 20, 72));
        drawInstrumentArt(g, {r.getX() + 10.0f, r.getY() + 10.0f, art, art}, family_, !installed);

        const auto textArea = juce::Rectangle<int>(static_cast<int>(art) + 22, 10, getWidth() - static_cast<int>(art) - 32, getHeight() - 20);
        g.setColour(installed ? text : textDim);
        g.setFont(ui(15.0f, true));
        g.drawFittedText(name_, textArea.withHeight(20), juce::Justification::centredLeft, 1);
        g.setColour(textFaint);
        g.setFont(ui(12.0f));
        g.drawFittedText(category_, textArea.withTrimmedTop(20).withHeight(16), juce::Justification::centredLeft, 1);

        // Status line
        juce::Colour dot = ok;
        juce::String status = external_ ? "Imported" : "Ready";
        if (state_ == realtone::InstallState::NotInstalled) { dot = textFaint; status = "Not installed"; }
        else if (state_ == realtone::InstallState::Incomplete) { dot = warn; status = juce::String(missing_) + " file(s) missing"; }
        else if (state_ == realtone::InstallState::Damaged) { dot = error; status = "Damaged files"; }
        if (external_ && state_ != realtone::InstallState::Installed) { dot = error; status = "SFZ file not found"; }
        if (!action_.isVisible() || !action_.isShowing())
        {
            g.setColour(dot);
            g.fillEllipse(static_cast<float>(textArea.getX()), static_cast<float>(textArea.getBottom()) - 12.0f, 8.0f, 8.0f);
            g.setColour(textDim);
            g.setFont(ui(12.0f));
            g.drawText(status, textArea.getX() + 14, textArea.getBottom() - 16, textArea.getWidth() - 14, 16, juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        const int art = juce::jmin(getHeight() - 20, 72);
        action_.setBounds(art + 22, getHeight() - 36, getWidth() - art - 34, 24);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showMenu();
            return;
        }
        if (!e.mouseWasDraggedSinceMouseDown() && getLocalBounds().contains(e.getPosition()))
            choose_(id_);
    }
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::returnKey || k == juce::KeyPress::spaceKey)
        {
            choose_(id_);
            return true;
        }
        return false;
    }

    // Name used by tests and accessibility
    const juce::String& id() const { return id_; }

private:
    void showMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Select");
        if (!external_)
        {
            menu.addSeparator();
            menu.addItem(2, state_ == realtone::InstallState::Installed ? "Reinstall / repair" : "Install", true);
            menu.addItem(3, "Verify files", state_ != realtone::InstallState::NotInstalled);
            menu.addItem(4, "Remove from this computer", state_ != realtone::InstallState::NotInstalled);
        }
        else
            menu.addItem(5, "Remove from list");
        menu.showMenuAsync({}, [this, safe = juce::Component::SafePointer<Card>(this)](int r) {
            if (safe == nullptr) return;
            switch (r)
            {
                case 1: choose_(id_); break;
                case 2: manager_.install(id_); break;
                case 3: manager_.verify(id_); break;
                case 4: manager_.remove(id_); break;
                case 5: manager_.removeExternal(id_); break;
                default: break;
            }
        });
    }

    InstrumentManager& manager_;
    juce::String id_, name_, category_, description_;
    realtone::InstallState state_;
    realtone::Family family_;
    bool selected_;
    std::function<void(const juce::String&)> choose_;
    std::uint64_t bytes_;
    bool external_;
    int missing_;
    juce::TextButton action_;
};

class InstrumentBrowser::Grid : public juce::Component
{
public:
    static constexpr int kCardMinWidth = 250;
    static constexpr int kCardHeight = 100;
    static constexpr int kGap = 10;

    void setCards(std::vector<std::unique_ptr<Card>> cards)
    {
        removeAllChildren();
        cards_ = std::move(cards);
        for (auto& c : cards_)
            addAndMakeVisible(*c);
    }

    int heightForWidth(int w) const
    {
        const int cols = columnsFor(w);
        const int rows = (static_cast<int>(cards_.size()) + cols - 1) / juce::jmax(1, cols);
        return rows * (kCardHeight + kGap) + kGap;
    }

    static int columnsFor(int w) { return juce::jmax(1, (w - kGap) / (kCardMinWidth + kGap)); }

    void resized() override
    {
        const int cols = columnsFor(getWidth());
        const int cardW = (getWidth() - kGap * (cols + 1)) / cols;
        for (int i = 0; i < static_cast<int>(cards_.size()); ++i)
            cards_[static_cast<std::size_t>(i)]->setBounds(kGap + (i % cols) * (cardW + kGap), kGap + (i / cols) * (kCardHeight + kGap), cardW, kCardHeight);
    }

    int count() const { return static_cast<int>(cards_.size()); }

private:
    std::vector<std::unique_ptr<Card>> cards_;
};

InstrumentBrowser::InstrumentBrowser(InstrumentManager& manager, std::function<void(const juce::String&)> onChoose)
    : manager_(manager), onChoose_(std::move(onChoose)), jobBar_(jobProgress_)
{
    search_.setTextToShowWhenEmpty("Search instruments", theme::textFaint);
    search_.setFont(theme::ui(14.0f));
    search_.addListener(this);
    search_.setIndents(10, 6);
    addAndMakeVisible(search_);

    category_.setTextWhenNothingSelected("All categories");
    category_.onChange = [this] { rebuild(); };
    addAndMakeVisible(category_);

    installedOnly_.onClick = [this] { rebuild(); };
    addAndMakeVisible(installedOnly_);

    jobLabel_.setFont(theme::ui(12.0f));
    jobLabel_.setColour(juce::Label::textColourId, theme::textDim);
    addChildComponent(jobLabel_);
    addChildComponent(jobBar_);

    grid_ = std::make_unique<Grid>();
    viewport_.setViewedComponent(grid_.get(), false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);

    emptyLabel_.setJustificationType(juce::Justification::centred);
    emptyLabel_.setColour(juce::Label::textColourId, theme::textDim);
    emptyLabel_.setFont(theme::ui(14.0f));
    addChildComponent(emptyLabel_);

    manager_.addChangeListener(this);
    updateCategories();
    rebuild();
}

InstrumentBrowser::~InstrumentBrowser()
{
    manager_.removeChangeListener(this);
    viewport_.setViewedComponent(nullptr, false);
}

void InstrumentBrowser::setSelected(const juce::String& id)
{
    selected_ = id;
    rebuild();
}

int InstrumentBrowser::numVisibleCards() const { return grid_->count(); }

void InstrumentBrowser::updateCategories()
{
    const auto current = category_.getText();
    category_.clear(juce::dontSendNotification);
    category_.addItem("All categories", 1);
    juce::StringArray cats;
    for (const auto& e : manager_.entries())
        cats.addIfNotAlreadyThere(realtone::studio::fromStd(e.category));
    cats.sort(true);
    int id = 2;
    for (const auto& c : cats)
        category_.addItem(c, id++);
    const int idx = cats.indexOf(current);
    category_.setSelectedId(idx >= 0 ? idx + 2 : 1, juce::dontSendNotification);
}

void InstrumentBrowser::changeListenerCallback(juce::ChangeBroadcaster*)
{
    const auto job = manager_.job();
    jobLabel_.setVisible(job.running || job.error.isNotEmpty());
    jobBar_.setVisible(job.running);
    jobProgress_ = job.progress;
    jobLabel_.setText(job.error.isNotEmpty() ? "Error: " + job.error : job.title + "  " + job.message, juce::dontSendNotification);
    jobLabel_.setColour(juce::Label::textColourId, job.error.isNotEmpty() ? theme::error : theme::textDim);
    resized();
    selected_ = manager_.selectedId();
    updateCategories();
    rebuild();
}

void InstrumentBrowser::rebuild()
{
    const auto query = search_.getText().trim().toLowerCase();
    const bool onlyInstalled = installedOnly_.getToggleState();
    const int catId = category_.getSelectedId();
    const juce::String cat = catId > 1 ? category_.getText() : juce::String();

    std::vector<std::unique_ptr<Card>> cards;
    for (const auto& e : manager_.entries())
    {
        if (onlyInstalled && e.state != realtone::InstallState::Installed)
            continue;
        if (cat.isNotEmpty() && realtone::studio::fromStd(e.category) != cat)
            continue;
        if (query.isNotEmpty())
        {
            juce::String hay = (realtone::studio::fromStd(e.name) + " " + realtone::studio::fromStd(e.category) + " " + realtone::studio::fromStd(e.description)).toLowerCase();
            for (const auto& t : e.tags)
                hay += " " + juce::String(t).toLowerCase();
            bool all = true;
            for (const auto& word : juce::StringArray::fromTokens(query, " ", ""))
                if (!hay.contains(word))
                    all = false;
            if (!all)
                continue;
        }
        cards.push_back(std::make_unique<Card>(manager_, e, selected_ == realtone::studio::fromStd(e.id), [safe = juce::Component::SafePointer<InstrumentBrowser>(this)](const juce::String& id) {
            // Rebuilding replaces the card that was just clicked, so do it after the click has finished.
            juce::MessageManager::callAsync([safe, id]() {
                if (safe == nullptr)
                    return;
                safe->selected_ = id;
                if (safe->onChoose_)
                    safe->onChoose_(id);
                safe->rebuild();
            });
        }));
    }
    const bool empty = cards.empty();
    grid_->setCards(std::move(cards));
    emptyLabel_.setText(manager_.entries().empty() ? "No instruments are defined." : "No instruments match.\nTry clearing the search or the filters.",
                        juce::dontSendNotification);
    emptyLabel_.setVisible(empty);
    resized();
}

void InstrumentBrowser::paint(juce::Graphics& g)
{
    g.setColour(theme::surface);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), static_cast<float>(theme::radius));
    g.setColour(theme::border);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), static_cast<float>(theme::radius), 1.0f);
}

void InstrumentBrowser::resized()
{
    auto r = getLocalBounds().reduced(10);
    // Wide: one row of filters. Narrow: search on its own row.
    auto top = r.removeFromTop(theme::controlHeight);
    if (r.getWidth() >= 440)
    {
        installedOnly_.setBounds(top.removeFromRight(130));
        top.removeFromRight(8);
        category_.setBounds(top.removeFromRight(160));
        top.removeFromRight(8);
        search_.setBounds(top);
    }
    else
    {
        search_.setBounds(top);
        r.removeFromTop(6);
        auto row = r.removeFromTop(theme::controlHeight);
        installedOnly_.setBounds(row.removeFromRight(juce::jmin(130, row.getWidth() / 2)));
        row.removeFromRight(6);
        category_.setBounds(row);
    }
    r.removeFromTop(8);

    if (jobLabel_.isVisible() || jobBar_.isVisible())
    {
        auto strip = r.removeFromBottom(jobBar_.isVisible() ? 38 : 22);
        jobLabel_.setBounds(strip.removeFromTop(18));
        if (jobBar_.isVisible())
            jobBar_.setBounds(strip.removeFromTop(10).reduced(0, 1));
        r.removeFromBottom(4);
    }
    viewport_.setBounds(r);
    const int w = viewport_.getMaximumVisibleWidth();
    grid_->setSize(w, juce::jmax(grid_->heightForWidth(w), r.getHeight()));
    emptyLabel_.setBounds(r);
}

}  // namespace realtone::ui
