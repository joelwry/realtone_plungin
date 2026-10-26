# User guide

## Using it with Audacity

Audacity can load VST3 *effects* but not instrument plug-ins, so the RealTone Studio VST3 cannot be
played inside Audacity. The workflow that works:

1. **From MIDI.** Open the MIDI file on the **MIDI to WAV** page, choose the tracks and an instrument
   (Play page), then **Render to WAV**. In Audacity: *File > Import > Audio* and pick the WAV.
2. **From a recording.** Export the part you want from Audacity as WAV or MP3 (one clean melodic line:
   a voice, a flute, a bass). Open it on the **Audio to Instrument** page, check the detected notes,
   then **Convert and save WAV**. Import the result back into Audacity.

The original file is only ever read. If you pick the same name as the source the program refuses.

## Pages

### Play
Instruments on the left, performance controls on the right, a keyboard underneath. In a narrow window the
two halves become tabs and the keyboard hides when the window is short.

- **Instrument cards** show status: *Ready*, *Not installed* (with an Install button and the download
  size), *Incomplete* or *Damaged* (Repair). Right-click a card for Verify, Remove and Reinstall.
- **Search and filters** narrow the list; *Installed only* hides what you cannot play yet.
- **Pitch bend wheel**: drag; it returns to centre on release; double-click to centre. A blue marker
  shows bend arriving from a MIDI keyboard. The range is 1, 2, 3, 5, 7, 12 or 24 semitones.
  Pianos, mallets and harp start with bend **off** because they cannot bend a note; switching it on is a
  creative effect.
- **Expression**: vibrato depth and rate (the mod wheel adds vibrato), modulation, expression
  (also MIDI CC11), glide time and *Mono legato* (one note at a time; overlapping notes glide into each other).
- **Dynamics and envelope**: velocity sensitivity and curve, extra attack, release length, humanise.
- **Output**: level, stereo width, quality (Performance / Balanced / Quality), polyphony, sustain pedal
  (CC64, or the on-screen switch).
- **Presets**: factory presets suit the instrument family; *Save...* stores your own in
  `%LOCALAPPDATA%\RealToneStudio\Presets`. A preset holds sound settings, not the instrument.

Choosing an instrument also moves the controls to that family's defaults (vibrato on for strings, pedal on for
piano, and so on).

### MIDI to WAV
Open or drop a `.mid` file. Tracks on channel 10 (drums) are switched off because a pitched instrument
cannot play a drum map. Options: transpose, tempo, velocity scale, timing humanise, longest release tail,
whether to keep pedal/mod wheel/expression and pitch bend. Output: 44.1, 48 or 96 kHz; 16-bit, 24-bit or
32-bit float WAV; limit peaks to -1 dBFS (default), normalise to -1 dBFS, or leave as rendered.
**Preview** renders the first 40 seconds and plays them. Rendering runs in the background with progress and
Cancel; the tail is rendered until the sound has died away, so release tails are never cut off.

Supported: Standard MIDI Files format 0 and 1, tempo changes, SMPTE time, running status, note on/off,
pitch bend and controllers 1, 7, 11, 64, 120, 121, 123. Format 2 files are rejected with a message.

### Audio to Instrument
Open or drop a WAV, MP3 or FLAC. The program finds notes automatically and draws them. Colour shows
confidence (green good, amber doubtful, red poor); click a note to select it, double-click to switch it off,
or use the buttons to move it by a semitone or an octave. **Detection** strictness and **Find notes again**
let you retry. *Keep vibrato and pitch slides* carries the pitch movement into the instrument.
A banner states plainly when the recording looks unsuitable (chords, several instruments, noise); take it
seriously, the result will be wrong in those cases.

### Library
All instruments with status and size, install / repair / verify / remove, the library location, and each
library's license and attribution. *Import SFZ library...* adds an SFZ instrument you already own; it is
played from where it is and RealTone Studio cannot check its license. Downloaded samples live in
`%LOCALAPPDATA%\RealToneStudio\Libraries` unless you move them.

### Settings
Audio output device and MIDI inputs, the sample-memory limit, and a button to reset the window position.

### Projects
*File > Save project* stores the instrument, all performance settings and the MIDI/audio page settings in a
`.rtproject` file. The MIDI and audio files are referenced, not copied; if one has moved you get a message.

## The VST3 plug-in

Install with the installer, rescan plug-ins in the DAW, add **RealTone Studio** as an instrument.

- MIDI note on/off with velocity, pitch bend, CC1, CC7, CC11, CC64, all-notes-off.
- 21 automatable parameters (the pitch bend wheel, mod wheel and sustain pedal are parameters too, so they
  can be automated even when no controller is connected).
- Instrument and preset name are saved with the project, and the instrument loads in the background when the
  project opens. Several instances share loaded samples.
- The editor can be resized in hosts that allow it; the size is remembered with the project.
- Latency is zero. The plug-in reports a 4 second tail.

If an instrument's samples are not installed the plug-in stays silent and the status line says so; it never
plays a different instrument instead.

## Limits

- One melodic line per audio conversion. No chords, no drums, no full mixes.
- Notes in the VSCO sustained instruments end when their recording ends (a few seconds); there are no loop
  points to hold them longer.
- Legato is a pitch glide, not a recorded transition. Bends are pitch shifts of the sampled note.
- Whole instruments are held in memory. Very large libraries need streaming, which is not implemented.
- WAV export only.
- The plug-in editor is drawn the same on every host; high-DPI behaviour depends on the host.
