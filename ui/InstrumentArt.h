#pragma once

#include "realtone/InstrumentProfile.h"

#include <juce_graphics/juce_graphics.h>

namespace realtone::ui
{

// Vector artwork for the instrument cards, drawn in code so the product ships no third-party images.
void drawInstrumentArt(juce::Graphics& g, juce::Rectangle<float> area, realtone::Family family, bool dimmed = false);
juce::Colour familyColour(realtone::Family family);

}  // namespace realtone::ui
