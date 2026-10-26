# Development checklist

Status against the original plan. `[x]` is done and tested, `[~]` partly, `[ ]` not done.

## Foundation
- [x] CMake build, pinned JUCE, C++17
- [x] Core library independent of JUCE, builds with GCC, Clang, MSVC and MinGW
- [x] Linux and Windows CI, packaging scripts
- [x] Standalone and VST3 targets compile and run
- [x] Window resizing implemented with native chrome; automated native-window checks in CI
- [ ] Window checks by hand on real hardware at several DPI settings

## Sampler engine
- [x] Velocity layers, round robin, release samples, loops, choke groups
- [x] Interpolation modes, sample-rate conversion, correct stereo
- [x] Voice allocation and stealing with fade
- [x] No allocation on the audio path (tested)
- [x] Pitch bend (selectable range), vibrato, glide, attack/release, expression, velocity response, humanise, width
- [x] Per-family behaviour (piano does not bend by default)
- [x] Sample cache shared between instances
- [ ] Disk streaming for very large libraries
- [ ] Sustain extension for libraries without loops (loop-point search and crossfade)
- [ ] Recorded legato / slide / hammer-on transitions (need libraries that have them)
- [ ] SFZ: filters, LFOs, keyswitches, CC-conditioned regions
- [ ] 16-bit sample storage option to halve memory

## Instruments and libraries
- [x] Manifest format with per-file SHA-256, license, attribution, allowed hosts
- [x] Installer, repair, verify, remove, cancel, retry, disk-space check
- [x] Import of user SFZ libraries
- [x] 20 instruments (CC0)
- [ ] Grand piano, electric piano, guitars, basses, saxophones, drums and the other missing families: each needs a
      library with a verified license (see INSTRUMENT_CATALOGUE)
- [ ] Per-instrument artwork beyond the drawn family glyphs

## Conversion
- [x] MIDI import and render (format 0/1)
- [x] WAV, MP3, FLAC import
- [x] Monophonic analysis with confidence, warnings, editing, pitch contour to bend events
- [ ] Polyphonic transcription (needs a model with verified license, size and latency)
- [ ] Source separation
- [ ] MP3 export (needs a licensed encoder)
- [ ] Audacity effect plug-in (decision: not built; see ARCHITECTURE)

## Interface
- [x] Instrument browser, search, categories, status, install, preview keyboard
- [x] Pitch bend wheel, performance panels, presets
- [x] MIDI, audio, library, settings, about pages; progress and errors
- [x] Resizable standalone window with saved geometry; resizable editor
- [ ] Keyboard navigation audit and screen-reader labels
- [ ] Localisation

## Release
- [x] Installer and portable zip built by CI, SHA-256 sums
- [x] Silent install, reinstall and uninstall verified on a Windows runner (user data and unrelated plug-ins untouched)
- [ ] Code signing (needs a certificate; the build script signs when one is supplied)
- [ ] Project license chosen and added (blocks public binaries; see DEPENDENCIES)
- [ ] Manual host testing in FL Studio and REAPER

## Next most valuable task

Add a second, larger instrument source that is both well recorded and clearly redistributable (a grand piano
first), and implement disk streaming for it. Without streaming a multi-gigabyte piano cannot be loaded.
