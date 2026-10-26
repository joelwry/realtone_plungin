#include "TabShell.h"

namespace realtone::ui
{

TabShell::TabShell(const juce::String& versionText) : version_(versionText) {}
TabShell::~TabShell() = default;

void TabShell::addPage(const juce::String& name, std::unique_ptr<juce::Component> page)
{
    Page p;
    p.name = name;
    p.component = std::move(page);
    p.button = std::make_unique<juce::TextButton>(name);
    p.button->setClickingTogglesState(false);
    const int index = static_cast<int>(pages_.size());
    p.button->onClick = [this, index] { setCurrentPage(index); };
    addAndMakeVisible(*p.button);
    addChildComponent(*p.component);
    pages_.push_back(std::move(p));
    if (pages_.size() == 1)
        setCurrentPage(0);
}

void TabShell::setCurrentPage(int index)
{
    current_ = juce::jlimit(0, juce::jmax(0, numPages() - 1), index);
    for (int i = 0; i < numPages(); ++i)
    {
        pages_[static_cast<std::size_t>(i)].component->setVisible(i == current_);
        pages_[static_cast<std::size_t>(i)].button->setToggleState(i == current_, juce::dontSendNotification);
    }
    resized();
}

int TabShell::minimumWidth() const
{
    int w = 190 + 90;  // brand + version
    for (const auto& p : pages_)
        w += 24 + theme::ui(14.0f).getStringWidth(p.name);
    return w;
}

void TabShell::paint(juce::Graphics& g)
{
    using namespace theme;
    g.fillAll(background);
    auto header = getLocalBounds().removeFromTop(kHeaderHeight);
    g.setColour(surface);
    g.fillRect(header);
    g.setColour(border);
    g.drawLine(0.0f, static_cast<float>(kHeaderHeight) - 0.5f, static_cast<float>(getWidth()), static_cast<float>(kHeaderHeight) - 0.5f);

    // Brand mark: a small stack of rising bars (a nod to a sample waveform) and the product name.
    const float x = 16.0f, base = 33.0f;
    g.setColour(accent);
    const float heights[] = {8.0f, 15.0f, 11.0f, 19.0f};
    for (int i = 0; i < 4; ++i)
        g.fillRoundedRectangle(x + static_cast<float>(i) * 5.0f, base - heights[i], 3.0f, heights[i], 1.5f);
    g.setColour(text);
    g.setFont(ui(17.0f, true));
    g.drawText("RealTone", 44, 8, 90, 32, juce::Justification::centredLeft);
    g.setColour(textDim);
    g.setFont(ui(17.0f));
    g.drawText("Studio", 44 + 74, 8, 70, 32, juce::Justification::centredLeft);

    g.setColour(textFaint);
    g.setFont(ui(12.0f));
    g.drawText(version_, getWidth() - 110, 8, 98, 32, juce::Justification::centredRight);
}

void TabShell::resized()
{
    auto header = getLocalBounds().removeFromTop(kHeaderHeight);
    header.removeFromLeft(176);
    header.removeFromRight(110);
    header = header.reduced(0, 9);

    int total = 0;
    for (const auto& p : pages_)
        total += 20 + theme::ui(14.0f).getStringWidth(p.name);
    const bool fit = total + 6 * numPages() <= header.getWidth();
    for (auto& p : pages_)
    {
        const int w = fit ? 20 + theme::ui(14.0f).getStringWidth(p.name) : header.getWidth() / juce::jmax(1, numPages()) - 4;
        p.button->setBounds(header.removeFromLeft(w));
        header.removeFromLeft(fit ? 6 : 4);
    }

    auto body = getLocalBounds().withTrimmedTop(kHeaderHeight);
    for (auto& p : pages_)
        p.component->setBounds(body);
}

}  // namespace realtone::ui
