# RealTone Studio

A sample-based instrument player for Windows, built around real acoustic recordings.

- **Standalone application**: play instruments, render MIDI files to WAV, and turn a clean
  single-voice WAV/MP3 recording into a recording of another instrument. Rendered WAV files drop
  straight into Audacity.
- **VST3 instrument**: the same engine as a plug-in for DAWs such as FL Studio and REAPER, with
  MIDI, velocity, pitch bend, mod wheel, expression, sustain pedal, presets and host automation.
- **`realtone-cli`**: the render and analysis pipeline from the command line.

Audacity cannot host instrument plug-ins (VSTi). The plug-in works in DAWs; the standalone
application is the route into Audacity. See [Using it with Audacity](docs/USER_GUIDE.md#using-it-with-audacity).

## Status

This is an early release (0.1.0). The engine, both applications and the installer are working; the
instrument library is deliberately small because every sound has to be redistributable.

| Area | State |
|---|---|
| Sampler engine (velocity layers, round robin, release samples, loops, pitch bend, vibrato, glide, sustain pedal, voice stealing, three interpolation qualities) | Working, unit tested |
| SFZ playback (the subset listed in [ARCHITECTURE](docs/ARCHITECTURE.md#sfz-support)) | Working |
| Instruments | 20 from the CC0 VSCO 2 Community Edition: upright piano, harp, marimba, xylophone, glockenspiel, tubular bells, organ (2), flute, oboe, clarinet, bassoon, trumpet, trombone, French horn, violin / viola / cello sections, solo violin, violin pizzicato. See [INSTRUMENT_CATALOGUE](docs/INSTRUMENT_CATALOGUE.md) for what is and is not available |
| Library installer (download, SHA-256 check, repair, remove, import your own SFZ) | Working |
| MIDI file to WAV | Working (format 0 and 1) |
| Audio to instrument | Working for clean monophonic sources. Polyphonic transcription is **not** implemented |
| VST3 | Builds, passes pluginval at strictness 10 on Linux and Windows CI |
| Resizable native window, resizable plug-in editor | Working. Message-level checks run in CI; see [TESTING](docs/TESTING.md) for what still needs hands-on testing |
| Installer, portable zip, checksums | Built and tested by CI. Binaries are **not code signed** |
| Audacity effect plug-in for conversion | Not built. See [ARCHITECTURE](docs/ARCHITECTURE.md#why-there-is-no-audacity-effect) |
| MP3 export | Not implemented (WAV only) |

Known limits are listed in the [changelog](CHANGELOG.md) and the [user guide](docs/USER_GUIDE.md#limits).

## Download

Installers and the portable zip are attached to the
[GitHub releases](https://github.com/joelwry/realtone_plungin/releases) once a version has been tagged and
built by CI, together with `SHA256SUMS.txt`. Nothing is hosted anywhere else.

## Quick start

1. Run the installer (or unzip the portable package).
2. Open **RealTone Studio**, go to **Library**, and install an instrument. Samples are downloaded
   from the pinned upstream repository and verified against SHA-256 checksums.
3. **Play** page: choose the instrument and play it from a MIDI keyboard or the on-screen keyboard.
4. **MIDI to WAV** or **Audio to Instrument**: load a file, pick options, save a WAV.
5. In a DAW, rescan VST3 plug-ins and load **RealTone Studio** as an instrument.

## Building

Windows (the supported release build), Visual Studio 2022 with the C++ desktop workload, CMake 3.22+,
Git and NSIS 3:

```powershell
pwsh packaging\build-windows.ps1 -Version 0.1.0
```

This configures, builds, runs the tests, and writes the installer, portable zip and checksums to `dist\`.
JUCE 8.0.4 is fetched by CMake. Use `-JuceDir C:\src\JUCE` to use an existing checkout.

Linux (development and CI; builds the same code, no installer):

```sh
sudo apt-get install ninja-build xvfb libasound2-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libfreetype-dev libgl1-mesa-dev libcurl4-openssl-dev libfontconfig1-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/tests/realtone_tests
xvfb-run -a ./build/tests_studio/realtone_studio_tests_artefacts/Release/realtone_studio_tests
```

The core library (engine, MIDI, analysis, library installer, CLI) has no JUCE dependency and builds
with any C++17 compiler: `cmake -S . -B build -DREALTONE_BUILD_APP=OFF`. JUCE 8 dropped MinGW
support, so a Windows build of the applications needs MSVC (or clang-cl).

## Repository layout

| Path | What is in it |
|---|---|
| `core/` | Engine and file handling, no JUCE: sampler, SFZ, MIDI, renderer, pitch analysis, library installer |
| `studio/` | JUCE glue shared by plug-in and app: processor, parameters, presets, instrument manager, settings |
| `ui/` | Shared interface components and theme |
| `plugin/` | VST3 target |
| `app/` | Standalone application |
| `cli/` | `realtone-cli` |
| `tests/`, `tests_studio/` | Core tests, JUCE-level tests |
| `libraries/manifests/` | Library manifests: file lists, sizes, SHA-256, license and attribution |
| `packaging/` | Windows build script, NSIS installer, install and window checks |
| `tools/` | Manifest generator and demo MIDI generator |
| `docs/` | Architecture, dependencies, testing, guides |
| `.github/workflows/` | CI: Linux and Windows builds, validation, release |

## Documentation

[User guide](docs/USER_GUIDE.md) ·
[Install and troubleshooting](docs/INSTALL_AND_TROUBLESHOOTING.md) ·
[Sample libraries](docs/LIBRARIES.md) ·
[Instrument catalogue](docs/INSTRUMENT_CATALOGUE.md) ·
[Architecture](docs/ARCHITECTURE.md) ·
[Dependencies and licenses](docs/DEPENDENCIES.md) ·
[Testing](docs/TESTING.md) ·
[Development checklist](docs/DEVELOPMENT_CHECKLIST.md)

## License

The project has no license file yet; until one is added the code is all rights reserved by its author.
Be aware that the applications link JUCE, which is AGPLv3 or commercial: distributing binaries means
either publishing the full source under the AGPL or buying a JUCE license
([details](docs/DEPENDENCIES.md#juce)). The `core/` library does not depend on JUCE.
Sample libraries carry their own licenses; see [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt).
