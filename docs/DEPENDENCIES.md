# Dependencies and licenses

Versions are pinned; update this file when one changes.

| Component | Version | License | How it is used | Where from |
|---|---|---|---|---|
| JUCE | 8.0.4 | AGPLv3 or commercial | GUI, plug-in wrapper, audio devices | `FetchContent` from github.com/juce-framework/JUCE at tag 8.0.4 |
| Steinberg VST3 SDK | 3.7.12, as bundled in JUCE 8.0.4 | Steinberg VST3 License or GPLv3 (your choice) | VST3 interfaces | inside JUCE |
| dr_wav 0.14.6, dr_flac, dr_mp3 0.7.4 | upstream master, commit ea33eed | public domain / MIT-0 | WAV, FLAC and MP3 decoding | `third_party/dr_libs` |
| minimp3 | inside dr_mp3 | CC0 | MP3 frames | |
| nlohmann/json | 3.11.3 | MIT | library manifests | `third_party/nlohmann` |
| NSIS | 3.x | zlib/libpng | builds the installer | not shipped |
| pluginval | latest release | GPLv3 | validation in CI only | not shipped |
| VSCO 2 Community Edition (SFZ) | commit 6dd651d | CC0 1.0 | instrument samples, downloaded on request | github.com/sgossner/VSCO-2-CE |

## JUCE

JUCE 8 is dual licensed. The applications here are built under the AGPLv3 option, which means anyone who
receives a binary must be able to get the corresponding source, including for network use. That is
satisfied by keeping this repository public and linking the source in releases. If you want to ship a
closed-source product you need a commercial license from Raw Material Software
(https://juce.com/juce-8-licence/). The `core/` library has no JUCE code, so its license can be chosen
separately from the applications.

The AGPL option also requires the program's own license to be compatible. **This repository does not
carry a license file yet.** Adding one is a decision for the project owner and has to be made before a
release is distributed.

## VST3 and trademarks

"VST" is a trademark of Steinberg Media Technologies GmbH. The VST3 SDK bundled in JUCE 8.0.4 is
version 3.7.12 and is offered under either the proprietary Steinberg VST3 License (which needs a signed
agreement with Steinberg before publishing) or the GPLv3 (checked in
`modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt`). This project uses the GPLv3 option,
which is compatible with building the combined work under the AGPLv3. If you take the commercial JUCE
route you should also sign the Steinberg agreement or confirm that the current SDK terms permit your
distribution.

## Samples

Each library's license is stored in its manifest (`libraries/manifests/*.json`), shown in the Library
page and bundled into the application's About page. A manifest without a license is rejected. The
installer will only download from the hosts a manifest lists, over https, and only when the manifest
says redistribution is allowed.

| Library | License | Attribution required | Redistribution | Commercial use | Notes |
|---|---|---|---|---|---|
| VSCO 2 CE | CC0 1.0 | no (given anyway) | yes | yes | Verified from the repository's README and LICENSE (CC0) |

Never add samples from commercial products, and do not assume a free download may be redistributed.
Libraries that have been looked at as candidates but **not** verified or added are listed in
[INSTRUMENT_CATALOGUE](INSTRUMENT_CATALOGUE.md).

## Why the samples are not in the repository

The pinned VSCO selection is about 970 MB. It stays upstream; the manifest records every file's size and
SHA-256, and the installer downloads and verifies them on demand. The repository and the installer
therefore contain no audio.
