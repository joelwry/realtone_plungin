#!/usr/bin/env python3
"""Writes examples/demo_phrase.mid: a short original phrase with a bass line, a pedal and a bend,
used to try the MIDI render path without needing any third-party material."""
import struct
import sys

PPQ = 480


def vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(out))


def track(events):
    # events: (abs_tick, bytes)
    events.sort(key=lambda e: e[0])
    data = b""
    last = 0
    for t, msg in events:
        data += vlq(t - last) + msg
        last = t
    data += vlq(0) + b"\xff\x2f\x00"
    return b"MTrk" + struct.pack(">I", len(data)) + data


def note(ev, t, d, n, v, ch=0):
    ev.append((t, bytes([0x90 | ch, n, v])))
    ev.append((t + d, bytes([0x80 | ch, n, 0])))


meta = [(0, b"\xff\x03" + bytes([len(b"Tempo")]) + b"Tempo"),
        (0, b"\xff\x51\x03" + (500000).to_bytes(3, "big")),            # 120 bpm
        (PPQ * 8, b"\xff\x51\x03" + (545454).to_bytes(3, "big"))]     # slight ritardando for the last bar

melody = [(0, b"\xff\x03" + bytes([len(b"Melody")]) + b"Melody")]
phrase = [(64, 1, 90), (67, 1, 84), (72, 2, 100), (71, 1, 78), (67, 1, 82), (69, 2, 92),
          (65, 1, 86), (69, 1, 80), (74, 2, 104), (72, 1, 76), (67, 1, 70), (60, 4, 96)]
t = 0
for n, beats, vel in phrase:
    note(melody, t, int(PPQ * beats * 0.95), n, vel)
    t += PPQ * beats
# a gentle bend up into the last phrase note and back
melody.append((PPQ * 12, bytes([0xE0, 0x00, 0x40])))
melody.append((PPQ * 12 + 120, bytes([0xE0, 0x00, 0x50])))
melody.append((PPQ * 12 + 240, bytes([0xE0, 0x00, 0x40])))
melody.append((PPQ * 13, bytes([0xB0, 1, 90])))   # mod wheel up (vibrato)

bass = [(0, b"\xff\x03" + bytes([len(b"Bass")]) + b"Bass")]
bass.append((0, bytes([0xB0 | 1, 64, 127])))      # sustain pedal down on channel 2
for bar, chord in enumerate([(48, 55, 64), (53, 60, 65), (55, 62, 67), (48, 55, 64)]):
    for n in chord:
        note(bass, bar * PPQ * 4, PPQ * 4 - 20, n, 64, ch=1)
bass.append((PPQ * 15, bytes([0xB0 | 1, 64, 0])))

header = b"MThd" + struct.pack(">IHHH", 6, 1, 3, PPQ)
out = sys.argv[1] if len(sys.argv) > 1 else "examples/demo_phrase.mid"
with open(out, "wb") as f:
    f.write(header + track(meta) + track(melody) + track(bass))
print("wrote", out)
