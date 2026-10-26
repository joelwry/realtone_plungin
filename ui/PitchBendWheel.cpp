#include "PitchBendWheel.h"

#include "../studio/Parameters.h"

namespace realtone::ui
{

namespace p = realtone::studio::param;

PitchBendWheel::PitchBendWheel(realtone::studio::RealToneProcessor& processor)
    : processor_(processor),
      attachment_(*processor.apvts.getParameter(p::pitchBend),
                  [this](float v) {
                      value_ = juce::jlimit(-1.0f, 1.0f, v);
                      repaint();
                  })
{
    attachment_.sendInitialUpdate();
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setTooltip("Pitch bend. Drag up or down, it returns to centre when released. Double-click to centre.");
    startTimerHz(30);
}

PitchBendWheel::~PitchBendWheel()
{
    stopTimer();
    closeGesture();
}

float PitchBendWheel::currentRangeSemitones() const
{
    const auto idx = static_cast<int>(processor_.apvts.getRawParameterValue(p::bendRange)->load());
    return p::kBendRanges[juce::jlimit(0, p::kNumBendRanges - 1, idx)];
}

bool PitchBendWheel::bendEnabled() const { return processor_.apvts.getRawParameterValue(p::bendEnable)->load() > 0.5f; }

juce::Rectangle<float> PitchBendWheel::trackBounds() const
{
    auto r = getLocalBounds().toFloat().reduced(6.0f);
    r.removeFromBottom(34.0f);  // readout
    return r.withSizeKeepingCentre(juce::jmin(r.getWidth(), 64.0f), r.getHeight());
}

void PitchBendWheel::openGesture()
{
    if (!gestureOpen_)
    {
        gestureOpen_ = true;
        attachment_.beginGesture();
    }
}

void PitchBendWheel::closeGesture()
{
    if (gestureOpen_)
    {
        gestureOpen_ = false;
        attachment_.endGesture();
    }
}

void PitchBendWheel::setFromY(float y)
{
    const auto t = trackBounds();
    const float norm = 1.0f - juce::jlimit(0.0f, 1.0f, (y - t.getY()) / juce::jmax(1.0f, t.getHeight()));
    float v = norm * 2.0f - 1.0f;
    if (std::fabs(v) < 0.03f)
        v = 0.0f;  // small dead zone so the centre is easy to hit
    value_ = v;
    attachment_.setValueAsPartOfGesture(v);
    repaint();
}

void PitchBendWheel::mouseDown(const juce::MouseEvent& e)
{
    dragging_ = true;
    springBack_ = false;
    openGesture();
    setFromY(e.position.y);
}

void PitchBendWheel::mouseDrag(const juce::MouseEvent& e) { setFromY(e.position.y); }

void PitchBendWheel::mouseUp(const juce::MouseEvent&)
{
    dragging_ = false;
    springBack_ = true;  // eased back to zero by the timer; the gesture ends when it arrives
}

void PitchBendWheel::mouseDoubleClick(const juce::MouseEvent&)
{
    openGesture();
    attachment_.setValueAsPartOfGesture(0.0f);
    closeGesture();
    value_ = 0.0f;
    springBack_ = false;
    repaint();
}

void PitchBendWheel::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    openGesture();
    value_ = juce::jlimit(-1.0f, 1.0f, value_ + w.deltaY * 0.5f);
    attachment_.setValueAsPartOfGesture(value_);
    springBack_ = true;
}

void PitchBendWheel::timerCallback()
{
    const float incoming = processor_.incomingPitchBend.load(std::memory_order_relaxed);
    if (std::fabs(incoming - midiValue_) > 1e-4f)
    {
        midiValue_ = incoming;
        repaint();
    }
    if (springBack_ && !dragging_)
    {
        value_ *= 0.72f;
        if (std::fabs(value_) < 0.01f)
        {
            value_ = 0.0f;
            springBack_ = false;
            attachment_.setValueAsPartOfGesture(0.0f);
            closeGesture();
        }
        else
            attachment_.setValueAsPartOfGesture(value_);
        repaint();
    }
}

void PitchBendWheel::paint(juce::Graphics& g)
{
    using namespace theme;
    const bool enabled = bendEnabled();
    const auto t = trackBounds();
    const float range = currentRangeSemitones();

    // Wheel body
    juce::ColourGradient body(juce::Colour(0xff0e1013), t.getX(), 0.0f, juce::Colour(0xff2a2f3a), t.getCentreX(), 0.0f, true);
    g.setGradientFill(body);
    g.fillRoundedRectangle(t, 14.0f);
    g.setColour(border);
    g.drawRoundedRectangle(t.reduced(0.5f), 14.0f, 1.0f);

    // Scale: semitone ticks, centre notch
    g.setFont(ui(10.0f));
    const int semis = static_cast<int>(range);
    for (int s = -semis; s <= semis; ++s)
    {
        const float yy = t.getCentreY() - (static_cast<float>(s) / range) * t.getHeight() * 0.5f;
        const bool major = (s == 0) || (s % (range > 7 ? 5 : 1) == 0);
        g.setColour(s == 0 ? accent : textFaint.withAlpha(enabled ? 0.9f : 0.4f));
        g.drawLine(t.getX() + 6.0f, yy, t.getX() + (major ? 20.0f : 13.0f), yy, s == 0 ? 2.0f : 1.0f);
        g.drawLine(t.getRight() - (major ? 20.0f : 13.0f), yy, t.getRight() - 6.0f, yy, s == 0 ? 2.0f : 1.0f);
    }

    // Grip
    const float y = t.getCentreY() - value_ * t.getHeight() * 0.5f;
    const auto grip = juce::Rectangle<float>(t.getWidth() - 8.0f, 26.0f).withCentre({t.getCentreX(), y});
    g.setColour(enabled ? accent : textFaint);
    g.fillRoundedRectangle(grip, 8.0f);
    g.setColour(background.withAlpha(0.55f));
    for (int i = -1; i <= 1; ++i)
        g.drawLine(grip.getX() + 8.0f, grip.getCentreY() + i * 5.0f, grip.getRight() - 8.0f, grip.getCentreY() + i * 5.0f, 1.2f);

    // Incoming MIDI bend marker (drawn only when it moves away from centre)
    if (std::fabs(midiValue_) > 0.005f)
    {
        const float my = t.getCentreY() - midiValue_ * t.getHeight() * 0.5f;
        g.setColour(info);
        juce::Path tri;
        tri.addTriangle(t.getRight() + 2.0f, my, t.getRight() + 9.0f, my - 5.0f, t.getRight() + 9.0f, my + 5.0f);
        g.fillPath(tri);
    }

    // Readout
    const float shown = (enabled ? (value_ + (std::fabs(midiValue_) > 0.005f ? midiValue_ : 0.0f)) : 0.0f) * range;
    auto readout = getLocalBounds().toFloat().removeFromBottom(34.0f);
    g.setColour(enabled ? text : textFaint);
    g.setFont(mono(14.0f));
    g.drawText((shown >= 0 ? "+" : "") + juce::String(shown, 2) + " st", readout.removeFromTop(18.0f), juce::Justification::centred);
    g.setColour(textFaint);
    g.setFont(ui(11.0f));
    g.drawText(enabled ? juce::String::fromUTF8("range \xC2\xB1") + juce::String(juce::roundToInt(range)) : juce::String("bend off"), readout,
               juce::Justification::centred);
}

}  // namespace realtone::ui
