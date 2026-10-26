# Install and troubleshooting

## Install

**Installer** (`RealToneStudio-<version>-win64-setup.exe`): needs administrator rights.

- Program: `C:\Program Files\RealTone Studio`
- VST3: `C:\Program Files\Common Files\VST3\RealTone Studio.vst3` (optional component)
- Start menu entry and an entry in *Apps > Installed apps*.

It only writes inside those two folders. Other plug-ins in the VST3 folder are never touched.
Silent install: `setup.exe /S`. Custom folder: `setup.exe /S /D=C:\Tools\RealTone` (must be the last argument).

**Portable zip**: unzip anywhere and run `RealTone Studio.exe`. To use the plug-in, copy the whole
`RealTone Studio.vst3` folder into `C:\Program Files\Common Files\VST3`.

**Check the download**: compare with `SHA256SUMS.txt` from the release:

```powershell
Get-FileHash .\RealToneStudio-0.1.0-win64-setup.exe -Algorithm SHA256
```

The binaries are not code signed, so Windows SmartScreen may say "unknown publisher" the first time.
Choose *More info > Run anyway* only if the checksum matches the release.

## Uninstall

*Settings > Apps > RealTone Studio > Uninstall*, or run `Uninstall.exe` in the program folder.
This removes the program and the VST3 folder. It does **not** delete `%LOCALAPPDATA%\RealToneStudio`
(settings, presets, logs and downloaded sample libraries), and it does not delete your projects or exported
WAV files. Delete that folder yourself to remove the sample libraries.

## Where things are

| What | Where |
|---|---|
| Settings | `%LOCALAPPDATA%\RealToneStudio\settings.xml` |
| User presets | `%LOCALAPPDATA%\RealToneStudio\Presets\*.rtpreset` |
| Sample libraries | `%LOCALAPPDATA%\RealToneStudio\Libraries` (change it on the Library page) |
| Log | `%LOCALAPPDATA%\RealToneStudio\Logs\realtone.log` (*Help > Open log folder*) |

## Troubleshooting

**The plug-in does not appear in my DAW.**
Rescan VST3 plug-ins. Check `C:\Program Files\Common Files\VST3\RealTone Studio.vst3\Contents\x86_64-win\RealTone Studio.vst3`
exists. The plug-in is 64-bit only; a 32-bit host cannot load it. FL Studio: *Options > Manage plugins > Find installed plugins*.
REAPER: *Preferences > Plug-ins > VST > Re-scan*.

**It loads but makes no sound.**
Open the editor and read the status line at the bottom. "Not installed" means the sample files are missing: use
the Library tab to install. "Some sample files are missing" means notes using them are silent: Repair. Make sure
the track is routed to an output and sends MIDI to the plug-in.

**Install fails or is slow.**
Instruments are downloaded from `raw.githubusercontent.com` over https. A proxy or firewall that blocks it
stops the download; the Library page shows the error and the log has details. Partial downloads are never
used: each file is checked against its checksum and re-fetched up to three times. You can cancel and
resume later; finished files are kept.

**"Damaged" instrument.**
A file's size or checksum differs from the manifest (interrupted copy, antivirus quarantine, disk error).
Right-click the card, *Verify files*, then *Reinstall / repair*.

**Not enough memory.**
Settings has a sample-memory limit (default 3 GB). Either raise it or close other instruments/instances.

**No sound from the standalone application's Play page.**
Settings > pick an output device. Previews and the Play page use that device; rendering to WAV does not
need one.

**A converted audio file sounds wrong.**
Read the banner on the Audio to Instrument page. If it warns about low confidence, the source is probably not a single
clear line. Try *Strict* detection, fix notes by hand, or use a MIDI file.

**Window is off-screen after changing monitors.**
*View > Reset window size and position*, or Settings > Reset window. The program also corrects a position
that is no longer reachable when it starts.

**Reporting a problem.**
Attach `realtone.log`, the Windows version, the host and its version, the instrument, and what you did.
