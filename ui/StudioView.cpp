#include "StudioView.h"

namespace realtone::ui
{

StudioView::StudioView(realtone::studio::RealToneProcessor& processor)
    : processor_(processor),
      presetBar_(processor),
      status_(processor),
      keyboard_(processor.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    browser_ = std::make_unique<InstrumentBrowser>(processor.instruments(), [this](const juce::String& id) {
        processor_.chooseInstrument(id, true);
        presetBar_.refresh();
    });
    performance_ = std::make_unique<PerformancePanel>(processor);
    addAndMakeVisible(*browser_);
    addAndMakeVisible(*performance_);
    addAndMakeVisible(presetBar_);
    addAndMakeVisible(status_);

    keyboard_.setAvailableRange(36, 96);
    keyboard_.setScrollButtonsVisible(false);
    keyboard_.setKeyPressBaseOctave(4);
    keyboard_.setMidiChannel(1);
    keyboard_.setVelocity(0.8f, true);
    addAndMakeVisible(keyboard_);

    for (auto* b : {&tabInstruments_, &tabPerformance_})
    {
        b->setClickingTogglesState(false);
        addChildComponent(*b);
    }
    tabInstruments_.onClick = [this] { showTab(0); };
    tabPerformance_.onClick = [this] { showTab(1); };

    browser_->setSelected(processor.instruments().selectedId());
}

StudioView::~StudioView() = default;

void StudioView::showTab(int index)
{
    tab_ = index;
    resized();
}

void StudioView::paint(juce::Graphics&) {}

void StudioView::resized()
{
    constexpr int gap = 10;
    auto r = getLocalBounds().reduced(gap);

    status_.setBounds(r.removeFromBottom(30));
    r.removeFromBottom(6);

    // The keyboard is optional: it disappears when the window is short rather than squeezing the controls.
    const bool showKeyboard = getHeight() >= 520;
    keyboard_.setVisible(showKeyboard);
    if (showKeyboard)
    {
        const int kh = getHeight() >= 700 ? 96 : 72;
        keyboard_.setBounds(r.removeFromBottom(kh));
        keyboard_.setKeyWidth(juce::jmax(12.0f, static_cast<float>(r.getWidth()) / 36.0f));
        r.removeFromBottom(gap);
    }

    presetBar_.setBounds(r.removeFromTop(theme::controlHeight + 2));
    r.removeFromTop(gap);

    layout_ = r.getWidth() >= 940 ? Layout::Wide : Layout::Tabbed;
    if (layout_ == Layout::Wide)
    {
        tabInstruments_.setVisible(false);
        tabPerformance_.setVisible(false);
        const int browserW = juce::jlimit(360, 560, static_cast<int>(static_cast<float>(r.getWidth()) * 0.4f));
        browser_->setBounds(r.removeFromLeft(browserW));
        r.removeFromLeft(gap);
        performance_->setBounds(r);
        browser_->setVisible(true);
        performance_->setVisible(true);
    }
    else
    {
        auto tabs = r.removeFromTop(theme::controlHeight);
        tabInstruments_.setVisible(true);
        tabPerformance_.setVisible(true);
        tabInstruments_.setBounds(tabs.removeFromLeft(tabs.getWidth() / 2 - 3));
        tabs.removeFromLeft(6);
        tabPerformance_.setBounds(tabs);
        tabInstruments_.setToggleState(tab_ == 0, juce::dontSendNotification);
        tabPerformance_.setToggleState(tab_ == 1, juce::dontSendNotification);
        r.removeFromTop(gap);
        browser_->setBounds(r);
        performance_->setBounds(r);
        browser_->setVisible(tab_ == 0);
        performance_->setVisible(tab_ == 1);
    }
}

}  // namespace realtone::ui
