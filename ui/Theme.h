#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace realtone::ui
{

// One place for colours, type and spacing so every screen looks like the same product.
namespace theme
{
inline const juce::Colour background{0xff121418};
inline const juce::Colour surface{0xff1a1d23};
inline const juce::Colour surfaceRaised{0xff222630};
inline const juce::Colour surfaceHover{0xff2a2f3b};
inline const juce::Colour border{0xff2f3542};
inline const juce::Colour text{0xffeceef3};
inline const juce::Colour textDim{0xff9aa1b2};
inline const juce::Colour textFaint{0xff6b7385};
inline const juce::Colour accent{0xffe8a24a};       // warm amber
inline const juce::Colour accentSoft{0xff4a3a20};
inline const juce::Colour ok{0xff6cc18a};
inline const juce::Colour warn{0xffe8b64a};
inline const juce::Colour error{0xffe5675f};
inline const juce::Colour info{0xff6aa7e0};

constexpr int unit = 8;          // base spacing step in logical pixels
constexpr int radius = 8;
constexpr int controlHeight = 30;

juce::Font ui(float size, bool bold = false);
juce::Font mono(float size);
}  // namespace theme

class RealToneLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RealToneLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float startAngle,
                          float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour, bool highlighted,
                              bool down) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY, int buttonW,
                      int buttonH, juce::ComboBox&) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isVertical,
                       int thumbStart, int thumbSize, bool mouseOver, bool mouseDown) override;
    void drawProgressBar(juce::Graphics&, juce::ProgressBar&, int width, int height, double progress,
                         const juce::String& textToShow) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
};

}  // namespace realtone::ui
