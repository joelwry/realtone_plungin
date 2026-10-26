# Sample libraries

## What is included

Nothing. The installer and the repository contain no audio. Instruments are downloaded on request from
the upstream source named in a library manifest, and each file is verified against a SHA-256 stored in the
manifest before it is accepted. After that, playing and rendering never need the internet.

## Manifests

`libraries/manifests/*.json` (compiled into the program):

```json
{
  "schema": 1, "id": "vsco2ce", "name": "...", "version": "...",
  "license": { "name": "CC0 1.0 Universal", "url": "...", "attribution": "...",
               "attributionRequired": false, "redistributionAllowed": true, "commercialUseAllowed": true },
  "source": { "homepage": "...", "baseUrl": "https://raw.githubusercontent.com/<repo>/<commit>/",
              "allowedHosts": ["raw.githubusercontent.com"] },
  "files": [ { "path": "Keys/Upright Piano/x.wav", "size": 123, "sha256": "..." } ],
  "instruments": [ { "id": "...", "name": "...", "category": "...", "family": "piano",
                     "sfz": "UprightPiano.sfz", "files": [ "UprightPiano.sfz", "Keys/..." ] } ]
}
```

A manifest is rejected if it has no license, a non-https base URL, a file path that is absolute or contains
`..`, an invalid checksum, or an instrument that names a file that is not listed. The installer refuses a
download host that is not in `allowedHosts`, a library whose license disallows redistribution, and a
destination without enough free disk space.

## Adding a library

1. Establish the license from the author's own statement. Record who may redistribute, whether
   attribution is required, and whether modification and commercial use are allowed. "Free download" is not
   enough. Do not add samples extracted from commercial products.
2. Pin a specific revision of a source that serves the files over https.
3. Write an instrument list like `tools/vsco2ce_instruments.json`, then run:
   `python3 tools/make_pack_manifest.py tools/<list>.json libraries/manifests/<id>.json --cache <folder>`
   It downloads every referenced file, rejects anything that is not really a WAV (for example a Git LFS pointer),
   and writes sizes and SHA-256 values.
4. Add the license and attribution text to `THIRD_PARTY_NOTICES.txt` and `docs/DEPENDENCIES.md`.
5. Listen to every instrument across its range before listing it as available.

## Importing a library you own

Library page > *Import SFZ library...*. The SFZ file and its samples stay where they are. The engine reports opcodes it
does not support and samples it cannot read. RealTone Studio does not verify the license of imported libraries.

## Removal

*Remove* deletes only the files listed in the manifest for that instrument and that no other installed
instrument of the same library still uses, inside the library folder, and then removes folders that became
empty. Files you added yourself to a library folder stay.
