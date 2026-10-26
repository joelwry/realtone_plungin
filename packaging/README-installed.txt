RealTone Studio
===============

Sample-based instruments for Windows: a standalone application for rendering MIDI files and
single-voice audio to WAV, and a VST3 instrument for DAWs.

Start
-----
1. Open "RealTone Studio" from the Start menu.
2. Library page: install an instrument (the sample files are downloaded once, about 4 to 250 MB
   each, and checked against SHA-256 checksums). Nothing needs the internet afterwards.
3. Play page: choose the instrument and play it with a MIDI keyboard or the on-screen keyboard.
4. MIDI to WAV / Audio to Instrument pages: render a file and save a WAV. Import the WAV into
   Audacity (File > Import > Audio).

Audacity cannot host instrument plug-ins (VSTi), so RealTone Studio's VST3 instrument works in
DAWs such as FL Studio and REAPER, and Audacity users render audio with the standalone application.

Where things are
----------------
Program:            C:\Program Files\RealTone Studio
VST3 plug-in:       C:\Program Files\Common Files\VST3\RealTone Studio.vst3
Your data:          %LOCALAPPDATA%\RealToneStudio   (settings, presets, logs, sample libraries)
                    The uninstaller never deletes this folder.

More: docs\USER_GUIDE.md, docs\INSTALL_AND_TROUBLESHOOTING.md, docs\LIBRARIES.md, THIRD_PARTY_NOTICES.txt
