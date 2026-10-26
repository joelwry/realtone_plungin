#include "realtone/Resampler.h"

#include <algorithm>
#include <cmath>

namespace realtone {

namespace {

constexpr double kPi = 3.14159265358979323846;

double besselI0(double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 40; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-12 * sum)
            break;
    }
    return sum;
}

}  // namespace

std::vector<float> resample(const std::vector<float>& input, double sourceRate, double targetRate)
{
    if (input.empty() || sourceRate <= 0.0 || targetRate <= 0.0)
        return {};
    if (std::fabs(sourceRate - targetRate) < 1e-9)
        return input;

    const double ratio = targetRate / sourceRate;
    const auto outLen = static_cast<std::size_t>(std::floor(static_cast<double>(input.size()) * ratio));
    std::vector<float> out(outLen);

    // When downsampling, lower the cutoff so content above the new Nyquist is rejected.
    const double cutoff = std::min(1.0, ratio) * 0.97;
    constexpr int halfTaps = 24;
    constexpr double beta = 8.6;
    const double invI0 = 1.0 / besselI0(beta);

    for (std::size_t n = 0; n < outLen; ++n)
    {
        const double srcPos = static_cast<double>(n) / ratio;
        const auto centre = static_cast<std::int64_t>(std::floor(srcPos));
        const double frac = srcPos - static_cast<double>(centre);

        double acc = 0.0;
        for (int k = -halfTaps + 1; k <= halfTaps; ++k)
        {
            const std::int64_t idx = centre + k;
            if (idx < 0 || idx >= static_cast<std::int64_t>(input.size()))
                continue;
            const double x = static_cast<double>(k) - frac;
            const double sinc = std::fabs(x) < 1e-9 ? 1.0 : std::sin(kPi * cutoff * x) / (kPi * cutoff * x);
            const double r = x / halfTaps;
            const double win = std::fabs(r) >= 1.0 ? 0.0 : besselI0(beta * std::sqrt(1.0 - r * r)) * invI0;
            acc += input[static_cast<std::size_t>(idx)] * sinc * win * cutoff;
        }
        out[n] = static_cast<float>(acc);
    }
    return out;
}

AudioClip resampleClip(const AudioClip& input, double targetRate)
{
    AudioClip out;
    out.sampleRate = targetRate;
    for (const auto& ch : input.channels)
        out.channels.push_back(resample(ch, input.sampleRate, targetRate));
    return out;
}

}  // namespace realtone
