#include "PathUtil.h"
#include "JuceFetcher.h"

#include <juce_core/juce_core.h>

namespace realtone::studio
{

realtone::FetchResult JuceFetcher::fetch(const std::string& url, const std::filesystem::path& destination,
                                         const std::function<void(std::uint64_t)>& onBytes,
                                         const std::atomic<bool>* cancel)
{
    realtone::FetchResult result;
    if (url.rfind("https://", 0) != 0)
    {
        result.error = "Refusing a non-https download address.";
        return result;
    }

    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(20000)
                       .withNumRedirectsToFollow(5)
                       .withStatusCode(&statusCode);
    auto stream = juce::URL(juce::String(url)).createInputStream(options);
    if (stream == nullptr)
    {
        result.error = "Could not connect (check the internet connection).";
        return result;
    }
    if (statusCode != 0 && statusCode != 200)
    {
        result.error = "The server answered with HTTP status " + std::to_string(statusCode) + ".";
        return result;
    }

    juce::File out(toJuceString(destination));
    out.deleteFile();
    juce::FileOutputStream file(out);
    if (!file.openedOk())
    {
        result.error = "Cannot write to " + destination.string();
        return result;
    }

    std::vector<char> buffer(256 * 1024);
    std::uint64_t total = 0;
    for (;;)
    {
        if (cancel != nullptr && cancel->load())
        {
            result.cancelled = true;
            return result;
        }
        const int got = stream->read(buffer.data(), static_cast<int>(buffer.size()));
        if (got < 0)
        {
            result.error = "The connection was interrupted.";
            return result;
        }
        if (got == 0)
            break;
        if (!file.write(buffer.data(), static_cast<size_t>(got)))
        {
            result.error = "Writing to disk failed (disk full?).";
            return result;
        }
        total += static_cast<std::uint64_t>(got);
        if (onBytes)
            onBytes(total);
    }
    file.flush();
    result.ok = true;
    return result;
}

}  // namespace realtone::studio
