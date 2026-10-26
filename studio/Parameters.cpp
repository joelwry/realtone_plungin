#include "Parameters.h"

#include <cmath>

namespace realtone::studio::param
{

namespace
{

using Float = juce::AudioParameterFloat;
using Bool = juce::AudioParameterBool;
using Choice = juce::AudioParameterChoice;
using juce::NormalisableRange;
using juce::ParameterID;

constexpr int kVersion = 1;

juce::String msText(float v, int) { return v < 10.0f ? juce::String(v, 1) + " ms" : juce::String(juce::roundToInt(v)) + " ms"; }

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<Float>(ParameterID{level, kVersion}, "Output Level", NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withLabel("dB")));
    layout.add(std::make_unique<Float>(ParameterID{expression, kVersion}, "Expression", NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    layout.add(std::make_unique<Float>(ParameterID{modulation, kVersion}, "Modulation", NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<Float>(ParameterID{pitchBend, kVersion}, "Pitch Bend", NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<Bool>(ParameterID{bendEnable, kVersion}, "Pitch Bend Enabled", true));

    juce::StringArray ranges;
    for (float r : kBendRanges)
        ranges.add(juce::String(juce::CharPointer_UTF8("\xC2\xB1")) + juce::String(juce::roundToInt(r)) + " st");
    layout.add(std::make_unique<Choice>(ParameterID{bendRange, kVersion}, "Pitch Bend Range", ranges, 1));

    layout.add(std::make_unique<Float>(ParameterID{vibratoDepth, kVersion}, "Vibrato Depth", NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withLabel("cents")));
    layout.add(std::make_unique<Float>(ParameterID{vibratoRate, kVersion}, "Vibrato Rate", NormalisableRange<float>(0.5f, 12.0f, 0.01f, 0.6f), 5.5f,
                                       juce::AudioParameterFloatAttributes().withLabel("Hz")));
    layout.add(std::make_unique<Float>(ParameterID{glide, kVersion}, "Glide Time", NormalisableRange<float>(0.0f, 1000.0f, 1.0f, 0.4f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction(msText)));
    layout.add(std::make_unique<Bool>(ParameterID{legato, kVersion}, "Mono Legato", false));
    layout.add(std::make_unique<Float>(ParameterID{attack, kVersion}, "Attack", NormalisableRange<float>(0.0f, 1000.0f, 0.5f, 0.4f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction(msText)));
    layout.add(std::make_unique<Float>(ParameterID{release, kVersion}, "Release", NormalisableRange<float>(0.1f, 4.0f, 0.01f, 0.5f), 1.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                                           [](float v, int) { return juce::String(v, 2) + "x"; })));
    layout.add(std::make_unique<Float>(ParameterID{velocitySens, kVersion}, "Velocity Sensitivity", NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    layout.add(std::make_unique<Float>(ParameterID{velocityCurve, kVersion}, "Velocity Curve", NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<Float>(ParameterID{humanize, kVersion}, "Humanize", NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<Float>(ParameterID{width, kVersion}, "Stereo Width", NormalisableRange<float>(0.0f, 2.0f, 0.001f), 1.0f));
    layout.add(std::make_unique<Float>(ParameterID{tune, kVersion}, "Fine Tune", NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withLabel("cents")));
    layout.add(std::make_unique<Choice>(ParameterID{quality, kVersion}, "Quality Mode",
                                        juce::StringArray{"Performance", "Balanced", "Quality"}, 2));
    layout.add(std::make_unique<Choice>(ParameterID{polyphony, kVersion}, "Polyphony", juce::StringArray{"8", "16", "32", "64", "128"}, 3));
    layout.add(std::make_unique<Bool>(ParameterID{sustain, kVersion}, "Sustain Pedal", false));
    layout.add(std::make_unique<Bool>(ParameterID{pedalEnable, kVersion}, "Respond To Pedal", false));
    return layout;
}

int bendRangeIndexFor(float semitones)
{
    int best = 1;
    float bestDiff = 1e9f;
    for (int i = 0; i < kNumBendRanges; ++i)
        if (std::fabs(kBendRanges[i] - semitones) < bestDiff)
        {
            bestDiff = std::fabs(kBendRanges[i] - semitones);
            best = i;
        }
    return best;
}

realtone::EngineParams readEngineParams(juce::AudioProcessorValueTreeState& apvts)
{
    auto raw = [&](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    realtone::EngineParams p;
    p.masterGainDb = raw(level);
    p.expression = raw(expression);
    p.extraBendSemitones = raw(pitchBend);
    p.pitchBendEnabled = raw(bendEnable) > 0.5f;
    p.pitchBendRange = kBendRanges[juce::jlimit(0, kNumBendRanges - 1, static_cast<int>(raw(bendRange)))];
    p.vibratoDepthCents = raw(vibratoDepth);
    p.vibratoRateHz = raw(vibratoRate);
    p.glideMs = raw(glide);
    p.monoLegato = raw(legato) > 0.5f;
    p.attackMs = raw(attack);
    p.releaseScale = raw(release);
    p.velocitySensitivity = raw(velocitySens);
    p.velocityCurve = raw(velocityCurve);
    p.humanizeVelocity = raw(humanize);
    p.stereoWidth = raw(width);
    p.fineTuneCents = raw(tune);
    p.quality = static_cast<realtone::Quality>(juce::jlimit(0, 2, static_cast<int>(raw(quality))));
    p.polyphony = kPolyphonyChoices[juce::jlimit(0, 4, static_cast<int>(raw(polyphony)))];
    p.sustainPedalEnabled = raw(pedalEnable) > 0.5f;
    return p;
}

void applyProfileDefaults(juce::AudioProcessorValueTreeState& apvts, const realtone::InstrumentProfile& profile)
{
    auto setFloat = [&](const char* id, float value) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    auto setBool = [&](const char* id, bool value) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(value ? 1.0f : 0.0f);
    };

    setBool(bendEnable, profile.pitchBendByDefault);
    if (auto* p = apvts.getParameter(bendRange))
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(bendRangeIndexFor(profile.bendRangeSemitones))));
    setFloat(vibratoDepth, profile.vibratoDepthCents);
    setFloat(vibratoRate, profile.vibratoRateHz);
    setFloat(glide, profile.glideMs);
    setBool(legato, profile.monoLegato);
    setFloat(attack, profile.attackMs);
    setFloat(release, profile.releaseScale);
    setBool(pedalEnable, profile.pedalSupported);
}

}  // namespace realtone::studio::param
