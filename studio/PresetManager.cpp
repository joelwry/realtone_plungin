#include "PresetManager.h"

#include "Parameters.h"
#include "Paths.h"

namespace realtone::studio
{

namespace
{

struct Tweak
{
    const char* id;
    float value;   // real (not normalised) value; bend range is given as an index
};

struct FactoryPreset
{
    const char* name;
    realtone::Family family;       // Generic = offered for every instrument
    std::vector<Tweak> tweaks;     // applied on top of the family's own defaults
};

const std::vector<FactoryPreset>& factoryPresets()
{
    using F = realtone::Family;
    static const std::vector<FactoryPreset> presets = {
        {"Default", F::Generic, {}},
        {"Wide Stage", F::Generic, {{param::width, 1.5f}, {param::level, -2.0f}}},
        {"Intimate Mono", F::Generic, {{param::width, 0.4f}}},
        {"Humanised", F::Generic, {{param::humanize, 0.6f}}},

        {"Soft and Close", F::Piano, {{param::velocitySens, 0.8f}, {param::release, 0.8f}, {param::width, 0.8f}}},
        {"Bright Concert", F::Piano, {{param::velocityCurve, 0.3f}, {param::width, 1.25f}}},
        {"Long Pedal Wash", F::Piano, {{param::release, 2.0f}, {param::pedalEnable, 1.0f}}},
        {"Creative Pitch Bend", F::Piano, {{param::bendEnable, 1.0f}, {param::bendRange, 5.0f}}},  // 12 semitones, explicitly an effect

        {"Expressive Vibrato", F::Bowed, {{param::vibratoDepth, 25.0f}, {param::vibratoRate, 6.0f}}},
        {"Legato Portamento", F::Bowed, {{param::legato, 1.0f}, {param::glide, 140.0f}}},
        {"Soft Entry", F::Bowed, {{param::attack, 90.0f}, {param::release, 1.4f}}},

        {"Lyrical", F::Brass, {{param::vibratoDepth, 14.0f}, {param::attack, 18.0f}}},
        {"Falls and Scoops", F::Brass, {{param::bendRange, 4.0f}, {param::glide, 90.0f}}},  // 7 semitones
        {"Lyrical", F::Woodwind, {{param::vibratoDepth, 16.0f}, {param::attack, 15.0f}}},
        {"Breathy Entry", F::Woodwind, {{param::attack, 45.0f}, {param::velocitySens, 0.7f}}},

        {"Short and Dry", F::Mallet, {{param::release, 0.4f}}},
        {"Ringing", F::Mallet, {{param::release, 2.2f}, {param::width, 1.3f}}},
        {"Short and Dry", F::Percussion, {{param::release, 0.4f}}},

        {"Church Hall", F::Organ, {{param::release, 2.0f}, {param::width, 1.4f}}},
        {"Slow Swell", F::Organ, {{param::attack, 120.0f}}},

        {"Intimate", F::Harp, {{param::release, 0.7f}, {param::width, 0.8f}}},
        {"Open and Airy", F::Harp, {{param::width, 1.5f}, {param::release, 1.4f}}},
    };
    return presets;
}

void setNormalised(juce::AudioProcessorValueTreeState& s, const char* id, float real)
{
    if (auto* p = s.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(real));
}

}  // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& state) : state_(state) {}

juce::File PresetManager::userDirectory() { return userPresetDirectory(); }

bool PresetManager::isValidName(const juce::String& name)
{
    const auto n = name.trim();
    if (n.isEmpty() || n.length() > 64)
        return false;
    return !n.containsAnyOf("\\/:*?\"<>|") && !n.startsWithChar('.');
}

std::vector<PresetInfo> PresetManager::listFor(realtone::Family family) const
{
    std::vector<PresetInfo> out;
    for (const auto& f : factoryPresets())
        if (f.family == realtone::Family::Generic || f.family == family)
            out.push_back({f.name, true, f.family, {}});

    auto dir = userDirectory();
    if (dir.isDirectory())
        for (const auto& file : dir.findChildFiles(juce::File::findFiles, false, "*.rtpreset"))
            out.push_back({file.getFileNameWithoutExtension(), false, realtone::Family::Generic, file});
    std::sort(out.begin(), out.end(), [](const PresetInfo& a, const PresetInfo& b) {
        if (a.factory != b.factory) return a.factory;
        return a.name.compareIgnoreCase(b.name) < 0;
    });
    return out;
}

juce::ValueTree PresetManager::captureParameters() const
{
    juce::ValueTree v("RealTonePreset");
    v.setProperty("version", 1, nullptr);
    for (auto* p : state_.processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*>(p))
        {
            // Pitch bend and the pedal are performance gestures, not part of a sound.
            if (withId->paramID == param::pitchBend || withId->paramID == param::sustain || withId->paramID == param::modulation)
                continue;
            v.setProperty(withId->paramID, withId->getValue(), nullptr);  // stored normalised
        }
    return v;
}

void PresetManager::applyParameters(const juce::ValueTree& values)
{
    for (int i = 0; i < values.getNumProperties(); ++i)
    {
        const auto name = values.getPropertyName(i);
        if (name == juce::Identifier("version"))
            continue;
        if (auto* p = state_.getParameter(name.toString()))
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, static_cast<float>(static_cast<double>(values.getProperty(name)))));
    }
}

bool PresetManager::load(const PresetInfo& preset, realtone::Family family, juce::String& error)
{
    if (preset.factory)
    {
        param::applyProfileDefaults(state_, realtone::profileFor(family));
        // Reset the remaining sound-shaping parameters, then apply the preset's changes.
        setNormalised(state_, param::level, 0.0f);
        setNormalised(state_, param::expression, 1.0f);
        setNormalised(state_, param::velocitySens, 1.0f);
        setNormalised(state_, param::velocityCurve, 0.0f);
        setNormalised(state_, param::humanize, 0.0f);
        setNormalised(state_, param::width, 1.0f);
        setNormalised(state_, param::tune, 0.0f);
        for (const auto& f : factoryPresets())
            if (preset.name == f.name && (f.family == realtone::Family::Generic || f.family == family))
            {
                for (const auto& t : f.tweaks)
                    setNormalised(state_, t.id, t.value);
                return true;
            }
        error = "Unknown factory preset.";
        return false;
    }

    auto xml = juce::parseXML(preset.file);
    if (xml == nullptr)
    {
        error = "The preset file could not be read: " + preset.file.getFullPathName();
        return false;
    }
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid() || !tree.hasType("RealTonePreset"))
    {
        error = "This is not a RealTone Studio preset.";
        return false;
    }
    applyParameters(tree);
    return true;
}

bool PresetManager::saveUser(const juce::String& rawName, juce::String& error)
{
    const auto name = rawName.trim();
    if (!isValidName(name))
    {
        error = "Preset names cannot be empty or contain \\ / : * ? \" < > |";
        return false;
    }
    const auto dir = userDirectory();
    if (!dir.createDirectory().wasOk())
    {
        error = "Cannot create the preset folder: " + dir.getFullPathName();
        return false;
    }
    const auto file = dir.getChildFile(name + ".rtpreset");
    auto xml = captureParameters().createXml();
    const auto tmp = file.withFileExtension("tmp");
    if (xml == nullptr || !xml->writeTo(tmp) || !tmp.moveFileTo(file))
    {
        error = "The preset could not be written.";
        return false;
    }
    return true;
}

bool PresetManager::deleteUser(const PresetInfo& preset, juce::String& error)
{
    if (preset.factory || !preset.file.existsAsFile() || preset.file.getParentDirectory() != userDirectory())
    {
        error = "Only your own presets can be deleted.";
        return false;
    }
    if (!preset.file.deleteFile())
    {
        error = "The preset file could not be deleted.";
        return false;
    }
    return true;
}

}  // namespace realtone::studio
