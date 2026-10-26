# Architecture

## Layers

```
 app/ (standalone)      plugin/ (VST3)
        \                   /
         ui/  (components, theme)          <- JUCE
         studio/ (processor, params,
                  presets, manager)         <- JUCE
                 |
 core/  engine, SFZ, MIDI, renderer,
        analysis, library installer         <- C++17 only, no JUCE
```

`core/` knows nothing about windows, plug-in formats or JUCE, so it is tested directly, builds with
MinGW, and can be driven from `realtone-cli`. `studio/` and `ui/` are compiled into both the plug-in
and the application (as an INTERFACE library, the pattern JUCE recommends) so the two behave identically.

### core/

| Module | Purpose |
|---|---|
| `AudioFile` | Decode WAV, FLAC, MP3 (dr_libs, public domain). Write WAV 16/24-bit PCM or 32-bit float, via a temporary file, never over an existing file unless told to |
| `Sfz` | Parse the SFZ subset below. Rejects sample and `#include` paths that leave the library folder |
| `Instrument` | Load the samples an SFZ file references on worker threads; per-key region index; report of missing and unreadable files |
| `InstrumentProfile` | Per-family performance defaults (does the instrument bend, take a pedal, glide, vibrato) |
| `SamplerEngine` | The real-time engine |
| `InstrumentSwap` | Lock-free hand-off of a loaded instrument to the audio thread, and of the old one back for disposal |
| `MidiFile` | Standard MIDI File parser into a `Sequence` |
| `Renderer` | Offline faster-than-real-time render of a `Sequence` through the engine |
| `PitchAnalysis` | Monophonic note detection and conversion of notes to a `Sequence` |
| `Library` | Manifests, catalog (installed / incomplete / damaged), installer, remover; the download transport is an interface |
| `Resampler`, `Sha256` | Utilities |

### The sampler engine

- `prepare()` allocates everything (voices, interpolation tables). After that `process()`,
  `handleEvent()` and `setParams()` never allocate, lock or throw. A test replaces `operator new` and
  fails if the audio path touches the heap.
- Events are applied at their exact sample offset by splitting the block; modulation (bend, vibrato,
  glide) is evaluated every 32 samples and the playback ratio is ramped across each chunk.
- Voices: `polyphony` live voices plus 16 spare slots, so a stolen voice can fade out over about 4 ms
  while the new note starts. Victim order: already releasing (quietest first), then held only by the
  pedal, then oldest.
- Quality modes: linear (Performance), 4-point Hermite (Balanced), 8-point Kaiser-windowed sinc with a
  1024-phase table (Quality). The sinc does not low-pass when pitching up, so large upward shifts of
  bright material can alias; the instruments cover the range with enough samples that this stays small.
- Round robin uses a per-key counter (`seq_length` / `seq_position`) and `lorand` / `hirand` with a
  seedable generator, so offline renders are repeatable.
- Piano-like and mallet instruments start with pitch bend off. It is a creative option, not a default.
- Legato is a monophonic mode that releases the previous note and starts the new one with a pitch
  offset that decays to zero (portamento). Recorded legato transitions are not used because the free
  libraries do not provide them.

### Threads

| Thread | Does | Never does |
|---|---|---|
| Audio callback | `RealToneProcessor::processBlock` -> engine | allocation, locks, file or network access, logging |
| Message thread | UI, parameter changes, instrument selection, applying finished jobs | decoding or rendering |
| Loader pool (2) | Decode samples, install / verify / remove libraries | touching the UI directly (results are posted) |
| `BackgroundTask` | One job per page: import, analyse, preview, export | |

Instrument changes: the loader builds an `Instrument`, the message thread posts it to `InstrumentSwap`,
the audio thread picks it up at the top of the next block. The previous instrument goes back through a
fixed retire list and is destroyed on the message thread (`collectGarbage` runs from a timer). Samples
are shared between plug-in instances through a process-wide cache of weak pointers.

### SFZ support

Opcodes honoured: `sample`, `default_path`, `key`, `lokey`, `hikey`, `pitch_keycenter`, `pitch_keytrack`,
`lovel`, `hivel`, `lorand`, `hirand`, `seq_length`, `seq_position`, `trigger` (attack, release, first,
legato), `loop_mode`, `loop_start`, `loop_end`, `offset`, `end`, `volume`, `pan`, `tune`, `transpose`,
`amp_veltrack`, `ampeg_attack/hold/decay/sustain/release`, `group`, `off_by`; `<control> <global> <master>
<group> <region>` inheritance; `#define`, `#include`; WAV smpl-chunk loops. Everything else is listed
in the instrument's load report instead of being dropped silently. Not supported: filters, LFOs and
other modulation, CC-conditioned regions, keyswitches, `<curve>`, generators, `sw_*`.

### Why not sfizz

sfizz (BSD-2-Clause, https://github.com/sfztools/sfizz) is the obvious existing SFZ engine. Its
repository shows a latest release of 1.2.3 and a last commit on `develop` in March 2025, so it is
quiet rather than abandoned. It was not adopted for this version because:

1. the engine needed behaviour that is specific to this product (per-family bend policy, glide in mono
   mode, a no-allocation guarantee that is proven by test, exact repeatability of offline renders) and
   the smaller code base made that quicker to verify than adapting a general player;
2. a MinGW/MSVC/Linux build of the core with no third-party engine dependency keeps CI simple.

The cost is a smaller SFZ feature set and no disk streaming. If libraries that need more of SFZ are
added, sfizz is the first thing to evaluate; `Instrument` and `SamplerEngine` are the only places that
would change. This is a design decision, not a measured comparison: sfizz was not built or benchmarked
for this project.

### Memory

Instruments are decoded to 32-bit float and held in RAM (upright piano about 320 MB, most others
15-90 MB). A configurable limit (Settings, default 3 GB) refuses an instrument that would exceed it.
Streaming from disk is not implemented; it is the main item for supporting large multi-gigabyte
libraries.

## Audio-to-instrument conversion

1. Decode and downmix; resample to 16 kHz for analysis.
2. YIN pitch tracking (46 ms window, 8 ms hop) with the period refined at the original rate.
3. Frames with a weak or ambiguous period (high aperiodicity, or dips at shorter lags that suggest
   several pitches) count towards an "unclear pitch" share. A high share raises a warning that the
   source is probably polyphonic or noisy.
4. Median filtering, segmentation by pitch steps (measured against the pitch a few frames earlier, so
   steps that develop across the analysis window are caught) and level rises for repeated notes.
5. Velocity from the note's peak level relative to the loudest note; per-note confidence.
6. `Sequence` with optional pitch-bend events carrying vibrato and slides (skipped for instruments that
   do not bend), rendered through the normal renderer.

The result is shown in a note roll (confidence by colour) where notes can be muted, shifted a semitone
or an octave, and the detection can be re-run with a different strictness.

Polyphonic transcription needs a trained model. None is included, because no model with a verified
license, size and latency has been chosen. See [DEVELOPMENT_CHECKLIST](DEVELOPMENT_CHECKLIST.md).

## Why there is no Audacity effect

Audacity hosts VST3 *effects*. An effect receives audio and returns audio of the same length in real
time blocks; it cannot choose an instrument library, show a progress bar for a multi-second analysis,
or change the length of the selection. The workable alternative would be an effect that analyses the
selection on a background thread and plays the result back, which would still depend on host features
that were not verified here. The standalone application already covers the workflow
(Audacity -> export WAV -> RealTone Studio -> WAV -> Audacity) with real progress and cancellation, so the
effect was not built.

## State, settings and files

- Plug-in state: XML in a versioned envelope (`stateVersion`) holding the parameter tree, the instrument
  id, the preset name and the editor size. Unknown or damaged state is ignored, never partly applied.
- User data: `%LOCALAPPDATA%\RealToneStudio` (settings, presets, logs, libraries). The uninstaller leaves
  it alone.
- Projects (`.rtproject`): XML with parameters, instrument id, MIDI/audio file paths and page settings.
  Files are referenced, not copied.

## Window management

The standalone window is a JUCE `DocumentWindow` with the native title bar and system buttons, so
dragging, edge and corner resizing, double-click maximise, minimise, snapping and per-monitor DPI come
from Windows. The code adds size limits (minimum 900 x 580, maximum the largest monitor), restores the
last size and position, and moves a window that was saved on a monitor that is gone back onto a
visible one (`WindowGeometry`). The plug-in editor calls `setResizable(true, false)` with limits, which
JUCE advertises to VST3 hosts, and saves its size in the plug-in state. Pages scroll vertically instead
of clipping, and switch layout at fixed widths.
