# Changelog

## 0.1.0 (unreleased)

First public version.

### Added
- Sampler engine: velocity layers, round-robin, release-trigger samples, sample loops, choke groups,
  sustain pedal, voice stealing with a fade, linear / Hermite / 8-point sinc interpolation, pitch bend
  with selectable range, vibrato (mod wheel and fixed), portamento in mono-legato mode, humanisation,
  stereo width, per-family performance profiles (a piano does not bend by default).
- SFZ parser for the opcode subset used by common free libraries; unsupported opcodes are reported.
- Library system: manifests with per-file SHA-256, installer with resume, retry, cancel, disk-space
  check and host allow-list, repair, verify, safe removal, import of user SFZ libraries.
- MIDI file import (format 0 and 1, tempo changes, SMPTE, running status) and offline rendering to WAV
  (16/24-bit PCM or 32-bit float, 44.1/48/96 kHz) with tail handling and level management.
- Audio import (WAV, FLAC, MP3) and monophonic note detection (YIN), with confidence scores,
  polyphony/noise warnings, a note editor, and conversion through any instrument.
- VST3 instrument with 21 automatable parameters, presets, state save/restore and a resizable editor.
- Standalone application: native resizable window with remembered size and position, projects,
  drag and drop, audio and MIDI device selection.
- `realtone-cli` for rendering, converting, analysing and verifying libraries.
- 20 instruments from VSCO 2 Community Edition (CC0).
- Windows installer (NSIS) and portable zip, built and tested by CI.

### Known limits
- Samples are loaded into memory in full; there is no disk streaming yet. The upright piano uses about 320 MB.
- The VSCO sustained instruments are single recordings without loop points, so a note held longer than
  the recording ends. Legato is a pitch glide, not a recorded transition.
- Audio conversion handles one melodic line at a time. Chords, drums and full mixes are not transcribed.
- No MP3 export. No Audacity effect plug-in.
- Binaries are not code signed, so Windows SmartScreen may warn on first run.
