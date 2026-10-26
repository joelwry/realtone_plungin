# Bundled third-party code

Single-header libraries that are vendored so the core builds with nothing but a C++17 compiler.

| Library | Version | License | Used for |
|---|---|---|---|
| [dr_libs](https://github.com/mackron/dr_libs) (`dr_wav`, `dr_flac`, `dr_mp3`) | dr_wav 0.14.6, dr_flac, dr_mp3 0.7.4 (upstream master, commit `ea33eed`) | Public domain or MIT-0 (your choice) | WAV, FLAC and MP3 decoding |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT | Library manifest parsing |

The licence text for each library is at the end of its header (dr_libs) or in the header comment (json.hpp).
JUCE is not vendored; see `docs/DEPENDENCIES.md`.
