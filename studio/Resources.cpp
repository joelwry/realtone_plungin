#include "InstrumentManager.h"

#include <BinaryData.h>

namespace realtone::studio
{

std::vector<juce::String> bundledManifestTexts()
{
    std::vector<juce::String> out;
    out.push_back(juce::String::fromUTF8(BinaryData::vsco2ce_json, BinaryData::vsco2ce_jsonSize));
    return out;
}

juce::String bundledLicenseNotices()
{
    return juce::String::fromUTF8(BinaryData::THIRD_PARTY_NOTICES_txt, BinaryData::THIRD_PARTY_NOTICES_txtSize);
}

}  // namespace realtone::studio
