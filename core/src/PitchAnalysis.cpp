#include "realtone/PitchAnalysis.h"

#include "realtone/Resampler.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace realtone {

namespace {

struct Frame
{
    double rmsDb = -200.0;
    double freq = 0.0;     // 0 if unvoiced
    double periodicity = 0.0;  // 1 - YIN d'
};

// YIN on one frame. Returns the refined period in samples, or 0 when no dip
// under the threshold exists. `aperiodicity` is the d' value at the chosen lag.
double yinPeriod(const float* x, int window, int minLag, int maxLag, double threshold, std::vector<float>& d,
                 double& aperiodicity, double& shortLagMin)
{
    maxLag = std::min(maxLag, window - 1);
    if (maxLag <= minLag + 2)
        return 0.0;

    // Difference function and cumulative mean normalisation in one pass.
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        double sum = 0.0;
        for (int i = 0; i + tau < window; ++i)
        {
            const double diff = static_cast<double>(x[i]) - x[i + tau];
            sum += diff * diff;
        }
        d[static_cast<std::size_t>(tau)] = static_cast<float>(sum);
    }
    d[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        running += d[static_cast<std::size_t>(tau)];
        d[static_cast<std::size_t>(tau)] = running > 0.0 ? static_cast<float>(d[static_cast<std::size_t>(tau)] * tau / running) : 1.0f;
    }

    int best = -1;
    for (int tau = minLag; tau <= maxLag - 1; ++tau)
    {
        if (d[static_cast<std::size_t>(tau)] < threshold)
        {
            while (tau + 1 <= maxLag - 1 && d[static_cast<std::size_t>(tau + 1)] < d[static_cast<std::size_t>(tau)])
                ++tau;
            best = tau;
            break;
        }
    }
    shortLagMin = 2.0;
    if (best < 0)
    {
        // No dip under the threshold: report the global minimum as an unvoiced candidate.
        float m = 2.0f;
        for (int tau = minLag; tau <= maxLag; ++tau)
            m = std::min(m, d[static_cast<std::size_t>(tau)]);
        aperiodicity = m;
        return 0.0;
    }

    aperiodicity = d[static_cast<std::size_t>(best)];
    // A chord has dips at the periods of its individual notes, shorter than the common period it settles on.
    shortLagMin = 2.0;
    for (int tau = minLag; tau < static_cast<int>(0.8 * best); ++tau)
        shortLagMin = std::min<double>(shortLagMin, d[static_cast<std::size_t>(tau)]);
    // Parabolic interpolation around the minimum.
    const double a = d[static_cast<std::size_t>(best - 1)], b = d[static_cast<std::size_t>(best)], c = d[static_cast<std::size_t>(best + 1)];
    const double denom = a - 2.0 * b + c;
    double shift = std::fabs(denom) > 1e-12 ? 0.5 * (a - c) / denom : 0.0;
    shift = std::clamp(shift, -1.0, 1.0);
    return best + shift;
}

// Re-evaluates the difference function at full rate around a period found on the decimated signal.
double refinePeriod(const float* x, int window, double approxLag)
{
    const int centre = static_cast<int>(std::lround(approxLag));
    const int lo = std::max(2, centre - 3), hi = centre + 3;
    if (window < hi * 2)
        return approxLag;
    double d[8] = {};
    for (int tau = lo; tau <= hi; ++tau)
    {
        double sum = 0.0;
        for (int i = 0; i + tau < window; ++i)
        {
            const double diff = static_cast<double>(x[i]) - x[i + tau];
            sum += diff * diff;
        }
        d[tau - lo] = sum / std::max(1, window - tau);
    }
    int best = 0;
    for (int i = 1; i <= hi - lo; ++i)
        if (d[i] < d[best])
            best = i;
    if (best == 0 || best == hi - lo)
        return best + lo;
    const double a = d[best - 1], b = d[best], c = d[best + 1];
    const double denom = a - 2.0 * b + c;
    const double shift = std::fabs(denom) > 1e-18 ? std::clamp(0.5 * (a - c) / denom, -1.0, 1.0) : 0.0;
    return lo + best + shift;
}

double median(std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2), v.end());
    return v[v.size() / 2];
}

double hzToMidi(double hz) { return 69.0 + 12.0 * std::log2(hz / 440.0); }

}  // namespace

AnalysisReport analyseMonophonic(const AudioClip& source, const AnalysisOptions& options,
                                 const std::function<void(float)>& progress, const std::atomic<bool>* cancel)
{
    AnalysisReport rep;
    if (source.numFrames() < 2048 || source.sampleRate <= 0.0)
    {
        rep.error = "The audio is too short to analyse.";
        return rep;
    }
    rep.durationSeconds = source.durationSeconds();

    // Mono at a low rate keeps YIN affordable; 16 kHz resolves everything below ~2 kHz fundamentals.
    auto mono = source.mixToMono();
    const std::vector<float> fullRate = mono;  // kept for fine period refinement
    const double rate = std::min(options.analysisRate, source.sampleRate);
    if (std::fabs(rate - source.sampleRate) > 1.0)
        mono = resample(mono, source.sampleRate, rate);

    // Remove DC and very low rumble with a one-pole high-pass.
    {
        float prevIn = 0.0f, prevOut = 0.0f;
        const float a = std::exp(-2.0f * 3.14159265f * 30.0f / static_cast<float>(rate));
        for (auto& s : mono)
        {
            const float in = s;
            prevOut = a * (prevOut + in - prevIn);
            prevIn = in;
            s = prevOut;
        }
    }

    const int window = static_cast<int>(std::round(rate * 0.046));     // ~46 ms, 2+ periods down to ~45 Hz
    const int hop = std::max(1, static_cast<int>(std::round(rate * 0.008)));
    const int minLag = std::max(2, static_cast<int>(std::floor(rate / options.maxFrequencyHz)));
    const int maxLag = static_cast<int>(std::ceil(rate / options.minFrequencyHz));
    if (static_cast<int>(mono.size()) < window + hop)
    {
        rep.error = "The audio is too short to analyse.";
        return rep;
    }

    const int numFrames = (static_cast<int>(mono.size()) - window) / hop + 1;
    std::vector<Frame> frames(static_cast<std::size_t>(numFrames));
    std::vector<float> d(static_cast<std::size_t>(maxLag + 2));

    double maxRmsDb = -200.0;
    for (int f = 0; f < numFrames; ++f)
    {
        if (cancel != nullptr && cancel->load())
        {
            rep.error = "Analysis was cancelled.";
            return rep;
        }
        const float* x = mono.data() + static_cast<std::size_t>(f) * hop;
        double energy = 0.0;
        for (int i = 0; i < window; ++i)
            energy += static_cast<double>(x[i]) * x[i];
        const double rms = std::sqrt(energy / window);
        frames[static_cast<std::size_t>(f)].rmsDb = rms > 1e-9 ? 20.0 * std::log10(rms) : -200.0;
        maxRmsDb = std::max(maxRmsDb, frames[static_cast<std::size_t>(f)].rmsDb);
        if (progress && f % 256 == 0)
            progress(0.5f * static_cast<float>(f) / static_cast<float>(numFrames));
    }

    if (maxRmsDb < -70.0)
    {
        rep.error = "The audio is silent or far too quiet to analyse.";
        return rep;
    }

    const double gate = maxRmsDb + options.silenceDb;
    int loudFrames = 0, unclearLoud = 0;
    for (int f = 0; f < numFrames; ++f)
    {
        if (cancel != nullptr && cancel->load())
        {
            rep.error = "Analysis was cancelled.";
            return rep;
        }
        auto& fr = frames[static_cast<std::size_t>(f)];
        if (fr.rmsDb < gate)
            continue;
        ++loudFrames;
        const float* x = mono.data() + static_cast<std::size_t>(f) * hop;
        double aper = 1.0, shortMin = 2.0;
        double period = yinPeriod(x, window, minLag, maxLag, options.periodicityThreshold, d, aper, shortMin);
        fr.periodicity = static_cast<float>(1.0 - std::min(1.0, aper));
        if (period > 0.0)
        {
            const double scale = source.sampleRate / rate;
            const std::size_t start = static_cast<std::size_t>(std::llround(static_cast<double>(f) * hop * scale));
            const int fullWindow = static_cast<int>(std::lround(window * scale));
            if (scale > 1.0 && start + static_cast<std::size_t>(fullWindow) <= fullRate.size())
                period = refinePeriod(fullRate.data() + start, fullWindow, period * scale) / scale;
            fr.freq = rate / period;
            if (fr.freq < options.minFrequencyHz || fr.freq > options.maxFrequencyHz)
                fr.freq = 0.0;
        }
        // Weak periodicity, or dips at shorter lags, point to several simultaneous pitches or noise.
        if (fr.freq > 0.0 && (aper > 0.12 || shortMin < 0.5))
        {
            fr.periodicity *= 0.5;
            ++unclearLoud;
        }
        else if (fr.freq == 0.0)
            ++unclearLoud;
        if (progress && f % 128 == 0)
            progress(0.5f + 0.4f * static_cast<float>(f) / static_cast<float>(numFrames));
    }

    if (loudFrames == 0)
    {
        rep.error = "No audible content was found.";
        return rep;
    }
    rep.polyphonyIndicator = static_cast<double>(unclearLoud) / loudFrames;
    rep.voicedFraction = 1.0 - rep.polyphonyIndicator;

    // Median filter the pitch track (in MIDI units) to remove octave blips.
    std::vector<double> midi(static_cast<std::size_t>(numFrames), 0.0);
    for (int f = 0; f < numFrames; ++f)
        if (frames[static_cast<std::size_t>(f)].freq > 0.0)
            midi[static_cast<std::size_t>(f)] = hzToMidi(frames[static_cast<std::size_t>(f)].freq);
    std::vector<double> smooth = midi;
    for (int f = 0; f < numFrames; ++f)
    {
        if (midi[static_cast<std::size_t>(f)] == 0.0) continue;
        std::vector<double> w;
        for (int k = -2; k <= 2; ++k)
        {
            const int j = f + k;
            if (j >= 0 && j < numFrames && midi[static_cast<std::size_t>(j)] > 0.0)
                w.push_back(midi[static_cast<std::size_t>(j)]);
        }
        smooth[static_cast<std::size_t>(f)] = median(w);
    }

    // Segment into notes.
    const double hopSec = static_cast<double>(hop) / rate;
    struct Seg { int start; int end; };  // end exclusive
    std::vector<Seg> segs;
    int start = -1;
    double segRef = 0.0;
    auto closeSeg = [&](int endFrame) {
        if (start >= 0 && endFrame > start)
            segs.push_back({start, endFrame});
        start = -1;
    };

    int gapRun = 0;
    for (int f = 0; f < numFrames; ++f)
    {
        const bool voiced = smooth[static_cast<std::size_t>(f)] > 0.0;
        if (!voiced)
        {
            if (start >= 0 && ++gapRun > 3)
            {
                closeSeg(f - gapRun + 1);
                gapRun = 0;
            }
            continue;
        }
        gapRun = 0;
        if (start < 0)
        {
            start = f;
            segRef = smooth[static_cast<std::size_t>(f)];
            continue;
        }

        // Pitch jump relative to the running reference of this segment.
        const double dev = smooth[static_cast<std::size_t>(f)] - segRef;
        bool split = std::fabs(dev) > options.splitSemitones;
        if (split)
        {
            // Require the new pitch to persist for 3 frames so a transient does not split a note.
            int agree = 0;
            for (int k = 0; k < 3 && f + k < numFrames; ++k)
                if (std::fabs(smooth[static_cast<std::size_t>(f + k)] - smooth[static_cast<std::size_t>(f)]) < 0.5 &&
                    smooth[static_cast<std::size_t>(f + k)] > 0.0)
                    ++agree;
            split = agree >= 3;
        }
        // Repeated note: sudden level rise on the same pitch.
        if (!split && f >= 3)
        {
            const double rise = frames[static_cast<std::size_t>(f)].rmsDb - frames[static_cast<std::size_t>(f - 3)].rmsDb;
            if (rise > options.onsetRiseDb && frames[static_cast<std::size_t>(f - 1)].rmsDb < frames[static_cast<std::size_t>(f)].rmsDb - 1.0 &&
                f - start > 6)
                split = true;
        }
        if (split)
        {
            closeSeg(f);
            start = f;
            segRef = smooth[static_cast<std::size_t>(f)];
        }
        else
        {
            segRef = 0.9 * segRef + 0.1 * smooth[static_cast<std::size_t>(f)];  // follow slow drift and vibrato
        }
    }
    closeSeg(gapRun > 0 ? numFrames - gapRun : numFrames);

    // Build notes.
    double loudest = -200.0;
    for (const auto& s : segs)
        for (int f = s.start; f < s.end; ++f)
            loudest = std::max(loudest, frames[static_cast<std::size_t>(f)].rmsDb);

    double confSum = 0.0;
    for (const auto& s : segs)
    {
        const double dur = (s.end - s.start) * hopSec;
        if (dur < options.minNoteSeconds)
            continue;

        std::vector<double> pitches;
        double conf = 0.0, peakDb = -200.0;
        int voicedCount = 0;
        for (int f = s.start; f < s.end; ++f)
        {
            if (smooth[static_cast<std::size_t>(f)] > 0.0)
            {
                pitches.push_back(smooth[static_cast<std::size_t>(f)]);
                conf += frames[static_cast<std::size_t>(f)].periodicity;
                ++voicedCount;
            }
            peakDb = std::max(peakDb, frames[static_cast<std::size_t>(f)].rmsDb);
        }
        if (pitches.empty())
            continue;

        DetectedNote n;
        const double med = median(pitches);
        n.midiNote = std::clamp(static_cast<int>(std::lround(med)), 0, 127);
        n.meanCents = static_cast<float>((med - n.midiNote) * 100.0);
        n.startSeconds = s.start * hopSec;
        n.endSeconds = s.end * hopSec;
        n.contourHopSeconds = hopSec;
        n.confidence = static_cast<float>(conf / voicedCount);
        // Map level relative to the loudest note onto a usable MIDI velocity range.
        const double rel = std::clamp((peakDb - (loudest - 30.0)) / 30.0, 0.0, 1.0);
        n.velocity = std::clamp(static_cast<int>(std::lround(28.0 + rel * 99.0)), 1, 127);
        for (int f = s.start; f < s.end; ++f)
            n.contourCents.push_back(smooth[static_cast<std::size_t>(f)] > 0.0
                                         ? static_cast<float>((smooth[static_cast<std::size_t>(f)] - n.midiNote) * 100.0)
                                         : (n.contourCents.empty() ? 0.0f : n.contourCents.back()));
        confSum += n.confidence;
        rep.notes.push_back(std::move(n));
    }

    if (progress)
        progress(1.0f);

    rep.meanConfidence = rep.notes.empty() ? 0.0 : confSum / static_cast<double>(rep.notes.size());
    rep.likelyPolyphonic = rep.polyphonyIndicator > 0.45;

    if (rep.notes.empty())
    {
        rep.error = "No clear melodic notes were found. This tool needs a single, clearly pitched voice.";
        return rep;
    }
    if (rep.likelyPolyphonic)
        rep.warnings.push_back("A large part of this recording has no single clear pitch. It may contain chords, several instruments, drums or heavy noise, so the detected notes will be unreliable.");
    else if (rep.polyphonyIndicator > 0.25)
        rep.warnings.push_back("Parts of this recording have no clear single pitch. Check the detected notes before rendering.");
    if (rep.meanConfidence < 0.75)
        rep.warnings.push_back("Pitch detection confidence is low. Expect wrong or missing notes.");
    int lowConf = 0;
    for (const auto& n : rep.notes)
        if (n.confidence < 0.6f)
            ++lowConf;
    if (lowConf > 0)
        rep.warnings.push_back(std::to_string(lowConf) + " of " + std::to_string(rep.notes.size()) + " notes have low confidence.");

    rep.ok = true;
    return rep;
}

Sequence notesToSequence(const std::vector<DetectedNote>& notes, const SequenceFromNotesOptions& options)
{
    Sequence seq;
    seq.format = 0;
    TrackInfo track;
    track.name = "Converted audio";

    const double range = std::max(0.5, options.bendRangeSemitones);
    auto bendValue = [&](double cents) {
        const double norm = std::clamp(cents / 100.0 / range, -1.0, 1.0);
        return static_cast<int>(std::lround(8192.0 + norm * 8191.0));
    };

    bool bendActive = false;
    for (const auto& n : notes)
    {
        if (!n.enabled)
            continue;
        SequenceEvent on;
        on.time = n.startSeconds;
        on.kind = SequenceEvent::Kind::NoteOn;
        on.data1 = n.midiNote;
        on.data2 = std::max(options.velocityFloor, n.velocity);
        seq.events.push_back(on);

        if (options.preservePitchContour && !n.contourCents.empty())
        {
            float last = 1e9f;
            for (std::size_t i = 0; i < n.contourCents.size(); ++i)
            {
                // The bend is global, so only change it when the contour has moved noticeably.
                if (std::fabs(n.contourCents[i] - last) < options.minContourChangeCents && i != 0)
                    continue;
                last = n.contourCents[i];
                SequenceEvent b;
                b.time = n.startSeconds + static_cast<double>(i) * n.contourHopSeconds;
                b.kind = SequenceEvent::Kind::PitchBend;
                b.data2 = bendValue(n.contourCents[i]);
                seq.events.push_back(b);
                bendActive = true;
            }
        }

        SequenceEvent off;
        off.time = std::max(n.endSeconds, n.startSeconds + 0.02);
        off.kind = SequenceEvent::Kind::NoteOff;
        off.data1 = n.midiNote;
        seq.events.push_back(off);
        ++track.noteCount;
        track.lowestNote = std::min(track.lowestNote, n.midiNote);
        track.highestNote = std::max(track.highestNote, n.midiNote);
    }
    if (bendActive)
    {
        SequenceEvent reset;
        reset.kind = SequenceEvent::Kind::PitchBend;
        reset.data2 = 8192;
        reset.time = seq.events.empty() ? 0.0 : seq.events.back().time + 0.001;
        seq.events.push_back(reset);
    }
    seq.noteCount = track.noteCount;
    seq.tracks.push_back(track);
    seq.sortEvents();
    return seq;
}

}  // namespace realtone
