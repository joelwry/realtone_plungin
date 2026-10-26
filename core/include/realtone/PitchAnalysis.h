#pragma once

#include "realtone/AudioClip.h"
#include "realtone/Sequence.h"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace realtone {

struct DetectedNote
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    int midiNote = 60;
    int velocity = 80;
    float confidence = 0.0f;       // 0..1, mean periodicity over the note
    float meanCents = 0.0f;        // deviation of the median pitch from the rounded note
    bool enabled = true;           // lets the UI mute a wrongly detected note before rendering
    std::vector<float> contourCents;  // per-hop pitch offset from midiNote (for bend/vibrato preservation)
    double contourHopSeconds = 0.008;
};

struct AnalysisOptions
{
    double minFrequencyHz = 55.0;     // A1
    double maxFrequencyHz = 1760.0;   // A6
    double periodicityThreshold = 0.20;  // YIN threshold; lower is stricter
    double minNoteSeconds = 0.06;
    double silenceDb = -50.0;         // frames quieter than this relative to the clip's loudest frame are unvoiced
    double onsetRiseDb = 6.0;         // level jump that starts a new note on a repeated pitch
    double splitSemitones = 0.65;     // pitch change that starts a new note
    double analysisRate = 16000.0;
};

struct AnalysisReport
{
    bool ok = false;
    std::string error;
    std::vector<DetectedNote> notes;
    double durationSeconds = 0.0;
    double voicedFraction = 0.0;       // share of non-silent frames that had a clear pitch
    double meanConfidence = 0.0;
    double polyphonyIndicator = 0.0;   // share of loud frames with no clear single pitch; high = probably polyphonic or noisy
    bool likelyPolyphonic = false;
    std::vector<std::string> warnings; // user-facing limitations for this particular source
};

// Monophonic pitch tracking (YIN with parabolic interpolation), onset
// segmentation and velocity estimation. Designed for isolated, reasonably clean
// single-voice recordings. It does not attempt polyphonic transcription.
AnalysisReport analyseMonophonic(const AudioClip& source, const AnalysisOptions& options = {},
                                 const std::function<void(float)>& progress = {},
                                 const std::atomic<bool>* cancel = nullptr);

struct SequenceFromNotesOptions
{
    bool preservePitchContour = true;   // emit pitch-bend events for vibrato, slides and tuning drift
    double bendRangeSemitones = 2.0;    // must match the range the renderer's engine uses
    double minContourChangeCents = 4.0; // bend event spacing threshold
    int velocityFloor = 24;
};

Sequence notesToSequence(const std::vector<DetectedNote>& notes, const SequenceFromNotesOptions& options = {});

}  // namespace realtone
