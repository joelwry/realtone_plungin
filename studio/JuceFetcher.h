#pragma once

#include "realtone/Library.h"

namespace realtone::studio
{

// IFileFetcher over juce::URL (WinINet on Windows, libcurl elsewhere). https only.
class JuceFetcher : public realtone::IFileFetcher
{
public:
    realtone::FetchResult fetch(const std::string& url, const std::filesystem::path& destination,
                                const std::function<void(std::uint64_t)>& onBytes,
                                const std::atomic<bool>* cancel) override;
};

}  // namespace realtone::studio
