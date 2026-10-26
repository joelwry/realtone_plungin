# Instrument catalogue

The planned families, and what actually exists today. An instrument is only listed as available when it has
a playable, redistributable sound source that has been loaded and rendered by the test suite or by hand.
Nothing is substituted for a missing instrument.

All current instruments come from **VS Chamber Orchestra: Community Edition** (CC0), pinned to commit
`6dd651d` of the SFZ branch. Instruments are single articulations: where a family has several in the
library only the sustained one with vibrato (or the plain sustained one) is listed so far.

| # | Family | Status | Source |
|---|---|---|---|
| 1 | Acoustic grand piano | Not available | |
| 2 | Upright piano | **Available** | VSCO 2 CE "Upright Piano" |
| 3 | Electric piano | Not available | |
| 4 | Steel-string acoustic guitar | Not available | |
| 5 | Nylon-string classical guitar | Not available | |
| 6 | Electric clean guitar | Not available | |
| 7 | Electric lead guitar | Not available | |
| 8 | Electric bass guitar | Not available | |
| 9 | Acoustic fingerstyle bass | Not available (the library also holds double-bass pizzicato samples that are not listed yet) | |
| 10 | Violin | **Available** (section with vibrato, solo with vibrato, section pizzicato) | VSCO 2 CE |
| 11 | Viola | **Available** (section with vibrato) | VSCO 2 CE |
| 12 | Cello | **Available** (section with vibrato) | VSCO 2 CE |
| 13 | Orchestral string ensemble | Partly: violin, viola and cello sections are separate instruments; there is no combined patch | VSCO 2 CE |
| 14 | Tenor saxophone | Not available | |
| 15 | Alto saxophone | Not available | |
| 16 | Trumpet | **Available** (sustained with vibrato) | VSCO 2 CE |
| 17 | Trombone | **Available** (sustained) | VSCO 2 CE |
| 18 | Flute | **Available** (sustained with vibrato) | VSCO 2 CE |
| 19 | Clarinet | **Available** (sustained) | VSCO 2 CE |
| 20 | Oboe | **Available** (sustained with vibrato) | VSCO 2 CE |
| 21 | Acoustic drum kit | Not available | |
| 22 | Congas and bongos | Not available | |
| 23 | Talking drum | Not available | |
| 24 | Djembe | Not available | |
| 25 | Shaker and tambourine | Not available | |
| 26 | Marimba | **Available** | VSCO 2 CE |
| 27 | Kalimba | Not available | |
| 28 | Kora | Not available | |
| 29 | Mandolin | Not available | |
| 30 | Banjo | Not available | |
| 31 | Harp | **Available** | VSCO 2 CE |
| 32 | Acoustic guitar harmonics | Not available | |
| 33 | Organ | **Available** (loud and quiet registrations) | VSCO 2 CE |
| 34 | Church-style organ | Not claimed: the library's description of its organ was not verified | |
| 35 | Synth bass | Not available | |
| 36 | Warm analog-style synthesizer | Not available (the engine plays samples only; there is no oscillator synth) | |
| 37 | Bell and mallet instruments | **Available** (glockenspiel, xylophone, tubular bells; plus marimba) | VSCO 2 CE |
| 38 | Pan flute | Not available | |
| 39 | Harmonica | Not available | |
| 40 | Accordion | Not available | |

Also in the same upstream library but not yet listed: bassoon is listed; French horn is listed; tuba, piccolo,
muted trumpets, timpani, other string articulations (staccato, tremolo, spiccato), GM-style percussion.

## Getting the missing ones

Each needs a library whose license permits redistribution, checked against the author's own statement, then the
steps in [LIBRARIES](LIBRARIES.md). Places that publish SFZ libraries with explicit licenses are the first to look
(for example the `sfzinstruments` organisation on GitHub, the Salamander Grand Piano by Alexander Holm, and the
University of Iowa Musical Instrument Samples). **None of those has been verified or added yet**; the license of
each must be read from the source before use, and some require attribution or forbid redistribution of modified
samples. Guitars, saxophones and drum kits are the largest gaps.

An instrument that needs an SFZ feature the engine lacks (for example keyswitch articulations, round-robin
across velocity groups with `seq_*` and `lorand`, filters, or CC-conditioned regions) needs engine work first; the
engine lists unsupported opcodes in the load report so this is visible.
