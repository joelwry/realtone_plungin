#include "Theme.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace realtone::ui
{

namespace theme
{

juce::Font ui(float size, bool bold)
{
#if JUCE_WINDOWS
    return juce::Font(juce::FontOptions("Segoe UI", size, bold ? juce::Font::bold : juce::Font::plain));
#else
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
#endif
}

juce::Font mono(float size)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

}  // namespace theme

RealToneLookAndFeel::RealToneLookAndFeel()
{
    using namespace theme;
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::Label::textColourId, text);
    setColour(juce::TextButton::buttonColourId, surfaceRaised);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, text);
    setColour(juce::ComboBox::backgroundColourId, surfaceRaised);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::arrowColourId, textDim);
    setColour(juce::PopupMenu::backgroundColourId, surfaceRaised);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accentSoft);
    setColour(juce::PopupMenu::highlightedTextColourId, text);
    setColour(juce::TextEditor::backgroundColourId, surface);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, border);
    setColour(juce::TextEditor::focusedOutlineColourId, accent);
    setColour(juce::TextEditor::highlightColourId, accentSoft);
    setColour(juce::CaretComponent::caretColourId, accent);
    setColour(juce::ListBox::backgroundColourId, surface);
    setColour(juce::ListBox::outlineColourId, border);
    setColour(juce::ScrollBar::thumbColourId, textFaint);
    setColour(juce::Slider::thumbColourId, accent);
    setColour(juce::Slider::trackColourId, accent);
    setColour(juce::Slider::rotarySliderFillColourId, accent);
    setColour(juce::Slider::rotarySliderOutlineColourId, border);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, text);
    setColour(juce::ToggleButton::tickColourId, accent);
    setColour(juce::AlertWindow::backgroundColourId, surfaceRaised);
    setColour(juce::AlertWindow::textColourId, text);
    setColour(juce::AlertWindow::outlineColourId, border);
    setColour(juce::TooltipWindow::backgroundColourId, surfaceRaised);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, border);
    setColour(juce::ProgressBar::foregroundColourId, accent);
    setColour(juce::ProgressBar::backgroundColourId, surfaceRaised);
    setColour(juce::TabbedButtonBar::tabOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff10131a));
    setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha(0.7f));
    setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha(0.25f));
    setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffdfe2ea));
    setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff1b1e26));
    setColour(juce::MidiKeyboardComponent::textLabelColourId, juce::Colour(0xff555d70));
}

juce::Font RealToneLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return theme::ui(juce::jmin(15.0f, static_cast<float>(buttonHeight) * 0.5f), false);
}
juce::Font RealToneLookAndFeel::getLabelFont(juce::Label& l) { return l.getFont().withTypefaceStyle("Regular"); }
juce::Font RealToneLookAndFeel::getComboBoxFont(juce::ComboBox&) { return theme::ui(14.0f); }
juce::Font RealToneLookAndFeel::getPopupMenuFont() { return theme::ui(14.0f); }

void RealToneLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(theme::surfaceRaised);
    g.setColour(theme::border);
    g.drawRect(0, 0, width, height);
}

void RealToneLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    g.fillAll(theme::surfaceRaised);
    g.setColour(theme::border);
    g.drawRect(0, 0, width, height);
    g.setColour(theme::text);
    g.setFont(theme::ui(13.0f));
    g.drawFittedText(text, 8, 4, width - 16, height - 8, juce::Justification::centredLeft, 4);
}

void RealToneLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float pos, float startAngle,
                                           float endAngle, juce::Slider& slider)
{
    using namespace theme;
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(4.0f);
    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto area = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());
    const float thickness = juce::jmax(3.0f, diameter * 0.09f);
    const auto arcArea = area.reduced(thickness * 0.5f);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool enabled = slider.isEnabled();

    // Face
    g.setColour(surface);
    g.fillEllipse(area.reduced(thickness + 2.0f));
    g.setColour(border);
    g.drawEllipse(area.reduced(thickness + 2.0f), 1.0f);

    juce::Path track;
    track.addCentredArc(arcArea.getCentreX(), arcArea.getCentreY(), arcArea.getWidth() * 0.5f, arcArea.getHeight() * 0.5f, 0.0f,
                        startAngle, endAngle, true);
    g.setColour(surfaceHover);
    g.strokePath(track, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc: from the centre for bipolar controls, from the start otherwise.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroPos = bipolar ? static_cast<float>(slider.valueToProportionOfLength(0.0)) : 0.0f;
    const float from = startAngle + zeroPos * (endAngle - startAngle);
    juce::Path value;
    value.addCentredArc(arcArea.getCentreX(), arcArea.getCentreY(), arcArea.getWidth() * 0.5f, arcArea.getHeight() * 0.5f, 0.0f,
                        juce::jmin(from, angle), juce::jmax(from, angle), true);
    g.setColour(enabled ? accent : textFaint);
    g.strokePath(value, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer
    const float r = arcArea.getWidth() * 0.5f - thickness * 1.3f;
    const auto centre = arcArea.getCentre();
    const juce::Point<float> tip(centre.x + r * std::sin(angle), centre.y - r * std::cos(angle));
    const juce::Point<float> base(centre.x + r * 0.45f * std::sin(angle), centre.y - r * 0.45f * std::cos(angle));
    g.setColour(enabled ? text : textFaint);
    g.drawLine({base, tip}, 2.0f);

    if (slider.hasKeyboardFocus(false))
    {
        g.setColour(accent.withAlpha(0.5f));
        g.drawEllipse(area.expanded(1.0f), 1.5f);
    }
}

void RealToneLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float,
                                           float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    using namespace theme;
    const bool horizontal = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
    const float trackThickness = 4.0f;
    juce::Rectangle<float> track = horizontal
        ? juce::Rectangle<float>(static_cast<float>(x), y + height * 0.5f - trackThickness * 0.5f, static_cast<float>(width), trackThickness)
        : juce::Rectangle<float>(x + width * 0.5f - trackThickness * 0.5f, static_cast<float>(y), trackThickness, static_cast<float>(height));
    g.setColour(surfaceHover);
    g.fillRoundedRectangle(track, 2.0f);

    juce::Rectangle<float> fill = track;
    if (horizontal)
        fill.setRight(sliderPos);
    else
        fill.setTop(sliderPos);
    g.setColour(slider.isEnabled() ? accent : textFaint);
    g.fillRoundedRectangle(fill, 2.0f);

    const float thumb = 14.0f;
    const auto centre = horizontal ? juce::Point<float>(sliderPos, track.getCentreY()) : juce::Point<float>(track.getCentreX(), sliderPos);
    g.setColour(text);
    g.fillEllipse(juce::Rectangle<float>(thumb, thumb).withCentre(centre));
}

void RealToneLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& colour, bool highlighted, bool down)
{
    using namespace theme;
    auto area = b.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = colour;
    if (!b.isEnabled())
        fill = surface;
    else if (down || b.getToggleState())
        fill = accent.darker(0.35f);
    else if (highlighted)
        fill = surfaceHover;
    g.setColour(fill);
    g.fillRoundedRectangle(area, static_cast<float>(radius) * 0.75f);
    g.setColour(b.getToggleState() ? accent : border);
    g.drawRoundedRectangle(area, static_cast<float>(radius) * 0.75f, 1.0f);
    if (b.hasKeyboardFocus(false))
    {
        g.setColour(accent.withAlpha(0.6f));
        g.drawRoundedRectangle(area.expanded(1.5f), static_cast<float>(radius), 1.5f);
    }
}

void RealToneLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace theme;
    const auto area = b.getLocalBounds().toFloat();
    const float h = juce::jmin(20.0f, area.getHeight());
    const auto sw = juce::Rectangle<float>(h * 1.7f, h * 0.9f).withCentre({area.getX() + h * 0.85f + 2.0f, area.getCentreY()});
    const bool on = b.getToggleState();
    g.setColour(on ? accent : surfaceHover);
    g.fillRoundedRectangle(sw, sw.getHeight() * 0.5f);
    g.setColour(on ? background : textDim);
    const float d = sw.getHeight() - 4.0f;
    g.fillEllipse(juce::Rectangle<float>(d, d).withCentre({on ? sw.getRight() - d * 0.5f - 2.0f : sw.getX() + d * 0.5f + 2.0f, sw.getCentreY()}));
    g.setColour(b.isEnabled() ? (highlighted ? text : text.withAlpha(0.92f)) : textFaint);
    g.setFont(ui(14.0f));
    g.drawFittedText(b.getButtonText(), juce::Rectangle<int>(static_cast<int>(sw.getRight()) + 8, 0, b.getWidth() - static_cast<int>(sw.getRight()) - 8, b.getHeight()),
                     juce::Justification::centredLeft, 1);
    if (b.hasKeyboardFocus(false))
    {
        g.setColour(accent.withAlpha(0.5f));
        g.drawRoundedRectangle(area.reduced(0.5f), 4.0f, 1.0f);
    }
}

void RealToneLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace theme;
    const auto area = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    g.setColour(box.isEnabled() ? surfaceRaised : surface);
    g.fillRoundedRectangle(area, static_cast<float>(radius) * 0.75f);
    g.setColour(box.hasKeyboardFocus(false) ? accent : border);
    g.drawRoundedRectangle(area, static_cast<float>(radius) * 0.75f, 1.0f);

    juce::Path arrow;
    const float cx = static_cast<float>(width) - 16.0f, cy = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(textDim);
    g.fillPath(arrow);
}

void RealToneLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical,
                                        int thumbStart, int thumbSize, bool over, bool down)
{
    using namespace theme;
    auto thumb = vertical ? juce::Rectangle<int>(x + width / 2 - 3, thumbStart, 6, thumbSize)
                          : juce::Rectangle<int>(thumbStart, y + height / 2 - 3, thumbSize, 6);
    g.setColour((down || over) ? textDim : textFaint.withAlpha(0.7f));
    g.fillRoundedRectangle(thumb.toFloat(), 3.0f);
}

void RealToneLookAndFeel::drawProgressBar(juce::Graphics& g, juce::ProgressBar& bar, int width, int height, double progress,
                                          const juce::String& textToShow)
{
    using namespace theme;
    const auto area = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.setColour(surfaceRaised);
    g.fillRoundedRectangle(area, static_cast<float>(height) * 0.5f);
    if (progress >= 0.0 && progress <= 1.0)
    {
        g.setColour(accent);
        g.fillRoundedRectangle(area.withWidth(juce::jmax(static_cast<float>(height), static_cast<float>(width) * static_cast<float>(progress))),
                               static_cast<float>(height) * 0.5f);
    }
    else
    {
        const float phase = static_cast<float>(juce::Time::getMillisecondCounter() % 1200) / 1200.0f;
        const float w = static_cast<float>(width) * 0.3f;
        g.setColour(accent);
        g.fillRoundedRectangle(juce::Rectangle<float>(w, static_cast<float>(height)).withX((static_cast<float>(width) + w) * phase - w).getIntersection(area),
                               static_cast<float>(height) * 0.5f);
    }
    if (textToShow.isNotEmpty())
    {
        g.setColour(text);
        g.setFont(ui(12.0f));
        g.drawText(textToShow, area, juce::Justification::centred);
    }
    juce::ignoreUnused(bar);
}

}  // namespace realtone::ui
