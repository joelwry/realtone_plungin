#include "PerformancePanel.h"

#include "../studio/Parameters.h"

#include <functional>

namespace realtone::ui
{

namespace p = realtone::studio::param;
using APVTS = juce::AudioProcessorValueTreeState;

namespace
{

// A knob with a caption underneath and the value in a text box.
class Knob : public juce::Component, public juce::SettableTooltipClient
{
public:
    Knob(APVTS& s, const char* id, const juce::String& caption, const juce::String& tip)
        : attachment_(s, id, slider_)
    {
        label_.setText(caption, juce::dontSendNotification);
        slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        // Colours are set on the slider itself (the text box is built at construction, before the
        // component has a parent and therefore before our look and feel is reachable).
        slider_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider_.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider_.setColour(juce::Slider::textBoxTextColourId, theme::text);
        slider_.setColour(juce::Slider::textBoxHighlightColourId, theme::accentSoft);
        slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 18);
        slider_.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider_.setPopupDisplayEnabled(false, false, nullptr);
        slider_.setTitle(caption);
        slider_.setTooltip(tip);
        addAndMakeVisible(slider_);
        label_.setFont(theme::ui(12.5f));
        label_.setJustificationType(juce::Justification::centred);
        label_.setColour(juce::Label::textColourId, theme::textDim);
        label_.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label_);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label_.setBounds(r.removeFromTop(18));
        slider_.setBounds(r);
    }
    // The slider's text box is created before the component has a parent, i.e. with the default
    // look and feel; rebuild it once the real one is reachable.
    void parentHierarchyChanged() override
    {
        if (!refreshed_ && getParentComponent() != nullptr)
        {
            refreshed_ = true;
            sendLookAndFeelChange();
        }
    }
    bool refreshed_ = false;
    juce::Slider slider_;
    juce::Label label_;
    APVTS::SliderAttachment attachment_;
};

class ToggleCell : public juce::Component
{
public:
    ToggleCell(APVTS& s, const char* id, const juce::String& caption, const juce::String& tip) : attachment_(s, id, button_)
    {
        button_.setButtonText(caption);
        button_.setTooltip(tip);
        addAndMakeVisible(button_);
    }
    void resized() override { button_.setBounds(getLocalBounds().withSizeKeepingCentre(getWidth(), 24)); }
    juce::ToggleButton button_;
    APVTS::ButtonAttachment attachment_;
};

class ChoiceCell : public juce::Component
{
public:
    ChoiceCell(APVTS& s, const char* id, const juce::String& caption, const juce::String& tip)
    {
        // Items first, then the attachment, so the stored choice is shown straight away.
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(s.getParameter(id)))
            box_.addItemList(param->choices, 1);
        attachment_ = std::make_unique<APVTS::ComboBoxAttachment>(s, id, box_);
        box_.setTooltip(tip);
        box_.setTitle(caption);
        addAndMakeVisible(box_);
        label_.setText(caption, juce::dontSendNotification);
        label_.setFont(theme::ui(12.5f));
        label_.setColour(juce::Label::textColourId, theme::textDim);
        label_.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label_);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label_.setBounds(r.removeFromTop(18));
        box_.setBounds(r.removeFromTop(theme::controlHeight));
    }
    juce::ComboBox box_;
    juce::Label label_;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment_;
};

}  // namespace

class PerformancePanel::Section : public juce::Component
{
public:
    static constexpr int kHeader = 30;
    static constexpr int kCellW = 96;
    static constexpr int kKnobH = 112;
    static constexpr int kRowH = 56;

    explicit Section(const juce::String& title) : title_(title) {}

    void add(std::unique_ptr<juce::Component> c, int cellsWide = 1, int height = kKnobH)
    {
        addAndMakeVisible(*c);
        items_.push_back({std::move(c), cellsWide, height});
    }

    // Sections with a bespoke arrangement install their own layout function.
    std::function<int(Section&, int width, bool apply)> custom;
    std::vector<juce::Component*> childList() const
    {
        std::vector<juce::Component*> v;
        for (auto& it : items_)
            v.push_back(it.comp.get());
        return v;
    }

    // Lays items out in rows of `columns` cells. Returns the height used.
    int layoutFor(int width, bool apply)
    {
        if (custom)
            return custom(*this, width, apply);
        const int pad = 12;
        const int inner = width - pad * 2;
        const int columns = juce::jmax(1, inner / kCellW);
        const int cellW = inner / columns;
        int x = 0, y = kHeader, rowH = 0;
        for (auto& it : items_)
        {
            const int span = juce::jmin(it.span, columns);
            if (x + span > columns)
            {
                x = 0;
                y += rowH;
                rowH = 0;
            }
            if (apply)
                it.comp->setBounds(pad + x * cellW + 2, y, span * cellW - 4, it.height);
            x += span;
            rowH = juce::jmax(rowH, it.height);
        }
        return y + rowH + 10;
    }

    void resized() override { layoutFor(getWidth(), true); }

    void paint(juce::Graphics& g) override
    {
        using namespace theme;
        g.setColour(surface);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), static_cast<float>(radius));
        g.setColour(border);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), static_cast<float>(radius), 1.0f);
        g.setColour(textDim);
        g.setFont(ui(12.5f, true));
        g.drawText(title_.toUpperCase(), 14, 8, getWidth() - 28, 18, juce::Justification::centredLeft);
    }

private:
    struct Item
    {
        std::unique_ptr<juce::Component> comp;
        int span;
        int height;
    };
    juce::String title_;
    std::vector<Item> items_;
};

class PerformancePanel::Content : public juce::Component
{
public:
    explicit Content(realtone::studio::RealToneProcessor& proc)
    {
        auto& s = proc.apvts;

        auto pitch = std::make_unique<Section>("Pitch");
        pitch->add(std::make_unique<PitchBendWheel>(proc), 1, 232);
        pitch->add(std::make_unique<ToggleCell>(s, p::bendEnable, "Pitch bend", "Turn pitch bend on or off. Off by default for pianos; enabling it there is a creative effect."), 2, 30);
        pitch->add(std::make_unique<ChoiceCell>(s, p::bendRange, "Bend range", "Semitones the wheel and MIDI pitch bend reach in each direction."), 2, 54);
        pitch->add(std::make_unique<Knob>(s, p::tune, "Fine tune", "Tuning offset in cents."), 1);
        pitch->custom = [](Section& sec, int width, bool apply) {
            // Wheel on the left, its settings stacked on the right.
            auto kids = sec.childList();  // wheel, toggle, range, tune
            const int pad = 14, top = Section::kHeader, wheelW = 104, wheelH = 236;
            const int rightX = pad + wheelW + 14;
            const int rightW = juce::jmax(120, width - rightX - pad);
            if (apply)
            {
                kids[0]->setBounds(pad, top, wheelW, wheelH);
                kids[1]->setBounds(rightX, top + 4, rightW, 26);
                kids[2]->setBounds(rightX, top + 44, rightW, 54);
                kids[3]->setBounds(rightX, top + 112, juce::jmin(rightW, 110), Section::kKnobH);
            }
            return top + wheelH + 12;
        };
        sections_.push_back(std::move(pitch));

        auto expr = std::make_unique<Section>("Expression");
        expr->add(std::make_unique<Knob>(s, p::vibratoDepth, "Vibrato depth", "Always-on vibrato depth in cents. The mod wheel adds to it."));
        expr->add(std::make_unique<Knob>(s, p::vibratoRate, "Vibrato rate", "Vibrato speed in Hz."));
        expr->add(std::make_unique<Knob>(s, p::modulation, "Modulation", "Same as the mod wheel (CC1): adds vibrato."));
        expr->add(std::make_unique<Knob>(s, p::expression, "Expression", "Overall dynamics. Also follows MIDI CC11."));
        expr->add(std::make_unique<Knob>(s, p::glide, "Glide", "Portamento time between overlapping notes in mono legato mode."));
        expr->add(std::make_unique<ToggleCell>(s, p::legato, "Mono legato", "Play one note at a time; overlapping notes glide into each other."), 2, 30);
        sections_.push_back(std::move(expr));

        auto dyn = std::make_unique<Section>("Dynamics and envelope");
        dyn->add(std::make_unique<Knob>(s, p::velocitySens, "Velocity", "How strongly key velocity changes the level."));
        dyn->add(std::make_unique<Knob>(s, p::velocityCurve, "Vel. curve", "Softer (left) or harder (right) velocity response."));
        dyn->add(std::make_unique<Knob>(s, p::attack, "Attack", "Extra attack time added to the recorded attack."));
        dyn->add(std::make_unique<Knob>(s, p::release, "Release", "Scales how long notes ring out after the key is released."));
        dyn->add(std::make_unique<Knob>(s, p::humanize, "Humanize", "Random variation in velocity from note to note."));
        sections_.push_back(std::move(dyn));

        auto out = std::make_unique<Section>("Output");
        out->add(std::make_unique<Knob>(s, p::level, "Level", "Output level in dB."));
        out->add(std::make_unique<Knob>(s, p::width, "Width", "0 is mono, 1 as recorded, 2 widened."));
        out->add(std::make_unique<ChoiceCell>(s, p::quality, "Quality", "Performance uses less CPU; Quality gives the cleanest pitch shifting."), 2, 54);
        out->add(std::make_unique<ChoiceCell>(s, p::polyphony, "Polyphony", "Maximum simultaneous voices."), 2, 54);
        out->add(std::make_unique<ToggleCell>(s, p::pedalEnable, "Respond to pedal", "Honour the sustain pedal (CC64)."), 2, 30);
        out->add(std::make_unique<ToggleCell>(s, p::sustain, "Sustain pedal", "On-screen sustain pedal."), 2, 30);
        sections_.push_back(std::move(out));

        for (auto& sec : sections_)
            addAndMakeVisible(*sec);
    }

    // Packs sections into columns. Returns the height. `apply` positions the components.
    int layoutFor(int width, bool apply)
    {
        const int gap = 10;
        const int minSectionW = 5 * Section::kCellW + 24;
        const int columns = juce::jmax(1, juce::jmin(static_cast<int>(sections_.size()), (width + gap) / (minSectionW + gap)));
        const int colW = (width - gap * (columns - 1)) / columns;

        std::vector<int> colY(static_cast<std::size_t>(columns), 0);
        for (auto& sec : sections_)
        {
            // Shortest column first keeps the packing balanced.
            std::size_t best = 0;
            for (std::size_t c = 1; c < colY.size(); ++c)
                if (colY[c] < colY[best])
                    best = c;
            const int h = sec->layoutFor(colW, false);
            if (apply)
                sec->setBounds(static_cast<int>(best) * (colW + gap), colY[best], colW, h);
            colY[best] += h + gap;
        }
        return *std::max_element(colY.begin(), colY.end());
    }

    std::vector<std::unique_ptr<Section>> sections_;
};

PerformancePanel::PerformancePanel(realtone::studio::RealToneProcessor& processor) : processor_(processor)
{
    content_ = std::make_unique<Content>(processor);
    viewport_.setViewedComponent(content_.get(), false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);
}

PerformancePanel::~PerformancePanel() { viewport_.setViewedComponent(nullptr, false); }

int PerformancePanel::contentHeightForWidth(int width) const { return content_->layoutFor(width, false); }

void PerformancePanel::paint(juce::Graphics&) {}

void PerformancePanel::resized()
{
    viewport_.setBounds(getLocalBounds());
    const int w = viewport_.getWidth() - viewport_.getScrollBarThickness();
    content_->setSize(w, content_->layoutFor(w, false));
    content_->layoutFor(w, true);
}

}  // namespace realtone::ui
