# Testing

## What runs automatically

| Suite | Where | What it covers |
|---|---|---|
| `realtone_tests` (core) | every build, Linux, and MinGW under Wine during development | Engine, SFZ, MIDI, renderer, analysis, library, WAV/MP3 |
| `realtone_studio_tests` | every build, Linux under Xvfb, Windows CI | Processor through `processBlock`, state, presets, parameters, editor layout, standalone workflows |
| pluginval, strictness 10 | CI, Linux and Windows | Plug-in contract: open/close, state, automation, editor, thread safety, fuzzing |
| `check-real-library.ps1` | Windows CI | Downloads one real instrument, verifies it, renders the demo MIDI, detects corruption |
| `verify-installer.ps1` | Windows CI | Silent install, files and registry, foreign plug-in untouched, user data untouched, reinstall, uninstall |
| `smoke-test-window.ps1` | Windows CI | Native window behaviour through the Win32 API (see below) |

Run locally (Linux):

```sh
./build/tests/realtone_tests [name-filter]
xvfb-run -a ./build/tests_studio/realtone_studio_tests_artefacts/Release/realtone_studio_tests [name-filter]
```

Both print one line per test and exit non-zero on failure. Screenshots of every page at several sizes are
written to `$REALTONE_TEST_OUTPUT` for visual review.

### Engine

Pitch accuracy at 44.1/48/96 kHz and all three interpolation modes; polyphony; velocity layers and monotonic
level; round-robin cycling; release triggers; sustain pedal; pitch bend at ±2/±7/±12 with return to centre;
bend disabled for piano profile; host bend parameter; vibrato from the mod wheel; mono legato glide;
loops sustaining beyond the sample; unlooped samples ending cleanly; voice stealing without clicks; a
4,000-event random storm staying finite; deterministic output with a fixed seed; silence (not another sound)
with no instrument; **no heap allocation on the audio path** (global `operator new` hook).

### Files and conversion

WAV round trip in three formats; deterministic writer; refusal to overwrite; invalid, empty, truncated and
junk input for audio and MIDI (including 300 randomly corrupted MIDI files); MP3 decode with pitch check;
resampler pitch and length; MIDI tempo maps, running status, hanging notes; render length, sample rate,
channel count, timing of note onsets, tail not truncated, no clipping, cancel and progress; the analyser on
clean melodies, repeats, semitone steps over a ringing tail, vibrato, other sample rates, high notes, silence,
noise and chords; an audio -> instrument -> audio round trip that recovers the same notes.

### Library

Manifest validation (http, missing license, `..` paths, unlisted files); install, resume, repair; damaged and
missing detection; corrupt download rejected and never stored; unauthorised host refused; cancel; removal
keeps shared files, files you added and sibling folders.

### Plug-in and UI

State round trip, corrupt and foreign state ignored, parameter ranges and IDs, presets, rapid instrument
switching, sample-rate and block-size changes, editor opens/closes repeatedly while audio runs, editor size
remembered, no clipped controls at 700x440 up to 2560x1440 on every page, WCAG-style contrast of text colours,
window geometry rules, MIDI and audio workflows end to end through the real pages, source files untouched.

## Window behaviour

`smoke-test-window.ps1` starts the real executable on a Windows runner and checks: caption, system menu,
sizing border, minimise and maximise buttons (window style bits); `WM_NCHITTEST` answers for the four edges, four
corners and the title bar (so resizing from every edge and corner, and dragging by the title bar, are available);
resizing to 1100x700; refusal to go below 900x580 or beyond the screen; maximise, restore, minimise, restore;
double-click on the title bar maximising and restoring; the app still responding; clean exit from the close button.

These are Win32 messages sent by a script. They prove the window is a real native resizable window; they do not
replace someone dragging it with a mouse. Not automated: dragging, behaviour at 125/150/200% display scaling,
moving between two monitors of different DPI.

## Manual checklist (not yet done on real hardware)

- [ ] Drag the title bar, every edge and every corner; double-click to maximise and restore.
- [ ] Move between two monitors, one at 100% and one at 150% scaling; check text and layout.
- [ ] At 125%, 150%, 200% scaling: nothing clipped on every page at 900x580.
- [ ] FL Studio: scan finds the plug-in; MIDI from a controller plays it; pitch bend, mod wheel, sustain pedal;
      automate Level and Vibrato Depth; save, close and reopen the project (instrument and settings return);
      resize the editor; open several instances.
- [ ] REAPER: the same; also offline render (bounce) of a MIDI item matches live playback.
- [ ] Audacity: import the WAV rendered from MIDI and from audio.
- [ ] Install on a machine with a non-ASCII user name; check the library and presets paths.
- [ ] Install over a previous version; uninstall; confirm user data is intact.
- [ ] Listen to every instrument over its range at low, medium and high velocity; check release tails and
      pitch bend.

## Results

Latest full CI run (Linux Ubuntu 24.04, Windows Server 2022 with MSVC 19.44), all green:

- Linux: core tests, studio tests, pluginval strictness 10 including editor tests.
- Windows: build with MSVC; core tests; studio tests; real-library check (download, SHA-256 verify, render, corruption detected);
  pluginval strictness 10 without GUI tests and again with editor tests; installer: silent install, files and registry,
  Start menu entry, unrelated plug-in untouched, pluginval strictness 5 on the installed copy, native-window checks on the
  installed executable, reinstall, silent uninstall, user data untouched.
- The window checks ran on a 1024x768, 96 dpi virtual display. They are Win32 messages sent by a script, not a mouse.
- The CI runner's pluginval.exe returns exit code 1 even after printing `SUCCESS`, so CI reads the verdict from its output.

Not covered by any of the above: the manual checklist. Results of manual runs belong in the changelog.
