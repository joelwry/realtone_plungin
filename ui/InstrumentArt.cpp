#include "InstrumentArt.h"

#include "Theme.h"

namespace realtone::ui
{

juce::Colour familyColour(realtone::Family f)
{
    using F = realtone::Family;
    switch (f)
    {
        case F::Piano: return juce::Colour(0xff7a8cc4);
        case F::ElectricKeys: return juce::Colour(0xffb58be0);
        case F::Guitar: return juce::Colour(0xffd08a58);
        case F::Bass: return juce::Colour(0xff9a6f5a);
        case F::Bowed: return juce::Colour(0xffd17a6e);
        case F::Brass: return juce::Colour(0xffe0b24f);
        case F::Woodwind: return juce::Colour(0xff72bfa0);
        case F::Percussion: return juce::Colour(0xffc2905a);
        case F::Mallet: return juce::Colour(0xff6fb7d9);
        case F::Organ: return juce::Colour(0xffa08fd6);
        case F::Harp: return juce::Colour(0xffe08fb0);
        case F::Synth: return juce::Colour(0xff58c9d0);
        case F::Generic: break;
    }
    return juce::Colour(0xff8f98ad);
}

namespace
{

void drawGlyph(juce::Graphics& g, juce::Rectangle<float> a, realtone::Family family, juce::Colour c)
{
    using F = realtone::Family;
    const float w = a.getWidth(), h = a.getHeight();
    const auto x0 = a.getX(), y0 = a.getY();
    g.setColour(c);

    switch (family)
    {
        case F::Piano:
        case F::ElectricKeys:
        {
            const int keys = 7;
            const float kw = w / keys;
            for (int i = 0; i < keys; ++i)
                g.fillRoundedRectangle(x0 + i * kw + 1.0f, y0 + h * 0.15f, kw - 2.0f, h * 0.75f, 2.0f);
            g.setColour(juce::Colour(0xff15171c));
            for (int i : {0, 1, 3, 4, 5})
                g.fillRoundedRectangle(x0 + (i + 1) * kw - kw * 0.28f, y0 + h * 0.15f, kw * 0.56f, h * 0.46f, 1.5f);
            break;
        }
        case F::Harp:
        {
            juce::Path frame;
            frame.startNewSubPath(x0 + w * 0.2f, y0 + h * 0.9f);
            frame.quadraticTo(x0 + w * 0.05f, y0 + h * 0.2f, x0 + w * 0.3f, y0 + h * 0.08f);
            frame.quadraticTo(x0 + w * 0.75f, y0 + h * 0.15f, x0 + w * 0.85f, y0 + h * 0.9f);
            g.strokePath(frame, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            for (int i = 0; i < 7; ++i)
            {
                const float fx = x0 + w * (0.3f + i * 0.085f);
                g.drawLine(fx, y0 + h * (0.16f + i * 0.02f), fx - w * 0.02f, y0 + h * 0.88f, 1.2f);
            }
            break;
        }
        case F::Mallet:
        {
            for (int i = 0; i < 5; ++i)
            {
                const float bh = h * (0.72f - i * 0.1f);
                g.fillRoundedRectangle(x0 + i * w * 0.2f + 2.0f, y0 + (h - bh) * 0.5f, w * 0.15f, bh, 3.0f);
            }
            g.setColour(c.withAlpha(0.55f));
            g.drawLine(x0 + w * 0.05f, y0 + h * 0.12f, x0 + w * 0.95f, y0 + h * 0.12f, 1.5f);
            break;
        }
        case F::Organ:
        {
            for (int i = 0; i < 6; ++i)
            {
                const float ph = h * (0.45f + 0.45f * std::abs(std::sin(i * 0.9f + 0.4f)));
                g.fillRoundedRectangle(x0 + i * w / 6.0f + 2.0f, y0 + h - ph - h * 0.05f, w / 6.0f - 4.0f, ph, 3.0f);
                g.setColour(juce::Colour(0xff15171c));
                g.fillEllipse(x0 + i * w / 6.0f + w / 12.0f - 2.5f, y0 + h - ph + h * 0.04f, 5.0f, 7.0f);
                g.setColour(c);
            }
            break;
        }
        case F::Bowed:
        case F::Guitar:
        case F::Bass:
        {
            juce::Path body;
            body.addEllipse(x0 + w * 0.2f, y0 + h * 0.5f, w * 0.6f, h * 0.45f);
            body.addEllipse(x0 + w * 0.26f, y0 + h * 0.3f, w * 0.48f, h * 0.38f);
            g.fillPath(body);
            g.setColour(c);
            g.drawLine(x0 + w * 0.5f, y0 + h * 0.5f, x0 + w * 0.5f, y0 + h * 0.04f, 3.0f);
            g.setColour(juce::Colour(0xff15171c));
            g.fillEllipse(x0 + w * 0.46f, y0 + h * 0.64f, w * 0.08f, w * 0.08f);
            if (family == F::Bowed)
            {
                g.setColour(c.withAlpha(0.8f));
                g.drawLine(x0 + w * 0.05f, y0 + h * 0.85f, x0 + w * 0.95f, y0 + h * 0.4f, 2.0f);
            }
            break;
        }
        case F::Brass:
        {
            juce::Path p;
            p.startNewSubPath(x0 + w * 0.08f, y0 + h * 0.5f);
            p.lineTo(x0 + w * 0.6f, y0 + h * 0.5f);
            p.lineTo(x0 + w * 0.94f, y0 + h * 0.22f);
            p.lineTo(x0 + w * 0.94f, y0 + h * 0.78f);
            p.lineTo(x0 + w * 0.6f, y0 + h * 0.5f);
            g.fillPath(p);
            g.strokePath(juce::Path(), juce::PathStrokeType(1.0f));
            g.drawEllipse(x0 + w * 0.12f, y0 + h * 0.5f, w * 0.34f, h * 0.3f, 3.0f);
            g.fillEllipse(x0 + w * 0.02f, y0 + h * 0.45f, w * 0.1f, h * 0.1f);
            break;
        }
        case F::Woodwind:
        {
            juce::Path tube;
            tube.addRoundedRectangle(x0 + w * 0.05f, y0 + h * 0.42f, w * 0.9f, h * 0.16f, 5.0f);
            g.fillPath(tube);
            g.setColour(juce::Colour(0xff15171c));
            for (int i = 0; i < 5; ++i)
                g.fillEllipse(x0 + w * (0.28f + i * 0.13f), y0 + h * 0.46f, h * 0.08f, h * 0.08f);
            break;
        }
        case F::Percussion:
        {
            g.fillEllipse(x0 + w * 0.12f, y0 + h * 0.2f, w * 0.76f, h * 0.24f);
            g.setColour(c.withAlpha(0.75f));
            g.fillRoundedRectangle(x0 + w * 0.12f, y0 + h * 0.32f, w * 0.76f, h * 0.45f, 8.0f);
            g.setColour(juce::Colour(0xff15171c));
            g.fillEllipse(x0 + w * 0.16f, y0 + h * 0.22f, w * 0.68f, h * 0.2f);
            break;
        }
        case F::Synth:
        case F::Generic:
        default:
        {
            juce::Path wave;
            wave.startNewSubPath(x0, y0 + h * 0.5f);
            for (int i = 0; i <= 40; ++i)
            {
                const float t = static_cast<float>(i) / 40.0f;
                const float amp = family == F::Synth ? (std::fmod(t * 3.0f, 1.0f) * 2.0f - 1.0f) : std::sin(t * 6.28318f * 2.0f);
                wave.lineTo(x0 + t * w, y0 + h * 0.5f - amp * h * 0.32f);
            }
            g.strokePath(wave, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
    }
}

}  // namespace

void drawInstrumentArt(juce::Graphics& g, juce::Rectangle<float> area, realtone::Family family, bool dimmed)
{
    const auto c = familyColour(family);
    juce::ColourGradient bg(c.withMultipliedBrightness(0.35f).withMultipliedSaturation(0.8f), area.getX(), area.getY(),
                            c.withMultipliedBrightness(0.16f), area.getRight(), area.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(c.withAlpha(dimmed ? 0.12f : 0.25f));
    g.drawRoundedRectangle(area.reduced(0.5f), 10.0f, 1.0f);
    drawGlyph(g, area.reduced(area.getWidth() * 0.18f), family, dimmed ? c.withAlpha(0.35f) : c.brighter(0.25f));
}

}  // namespace realtone::ui
