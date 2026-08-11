# RND Synth wire protocol

What is known about talking to a Cymaforma RND Synth, and how confident we are
about each part. **None of this is a published spec.** Cymaforma documents the
device's MIDI implementation only in general terms; the SysEx vocabulary below
was read off a working web app and confirmed against one hardware capture.

Two sources:

- **Seed Lab** (<https://redteam.fr/seed-lab/>), a third-party web app whose
  client JavaScript is unobfuscated. It drives the device over Web MIDI.
- **`Tests/fixtures/CymaRNDfirmUp.mid`**, ~40 seconds recorded from real
  hardware in Logic Pro, containing a performance and one status dump.

Firmware updates (`updater.cymaforma.com`) can invalidate any of this. The
standalone logs unrecognised SysEx rather than dropping it, so a change of
vocabulary shows up as a message instead of as silence.

## Transport

Class-compliant USB MIDI. Nothing exotic: no WebUSB, no serial, no BLE. The
web app reaches it through the Web MIDI API with `sysex: true`, which is why it
needs Chromium; native code just opens the port.

The device presents four MIDI channels in and out, MIDI clock in over both
USB-C and TRS, and analog sync on two TS jacks. Audio is separate: four USB-C
tracks pre-reverb, plus a post-reverb analog stereo mix.

## SysEx framing

```
F0 6F 62 78 <cmd> [payload…] F7
```

`6F 62 78` is ASCII `obx`. Note that `0x6F` sits inside the MMA-allocated
single-byte manufacturer range, **not** the `0x7D` non-commercial slot — so the
tag alone does not prove the sender is an RND. Match the port name as well.

### Commands

| cmd | Direction | Payload | Confidence |
|---|---|---|---|
| `0x10` | both | 5 bytes: the seed | **Confirmed** — sent by Seed Lab, seen from hardware |
| `0x11` | host → device | 1 byte: play-lock value (`0x00` observed) | **Confirmed** as sent; effect inferred |
| `0x20` | device → host | none | **Observed once**, purpose inferred |
| `0x21` | device → host | 5 bytes: globals | **Confirmed** |
| `0x22` | device → host | 3 bytes + NUL-terminated ASCII | **Confirmed**, two fields unexplained |

Nothing else has been seen. Whether other command bytes exist — to set tempo,
to clear the scale/tonic lock, to select an engine — is simply unknown.

### `0x10` — seed

A 32-bit seed as five 7-bit bytes, least-significant septet first; the fifth
byte carries only the top nibble.

```
seed = b0 | b1<<7 | b2<<14 | b3<<21 | (b4 & 0x0F)<<28
```

Captured: `F0 6F 62 78 10 67 59 10 52 0A F7` → `0xaa442ce7`.

**The device sends this unsolicited whenever its seed changes.** That is the
cheap way to follow the hardware — silent, no polling, no request needed.

### `0x11` — unlock and dump

`F0 6F 62 78 11 00 F7` sets the play-lock to 0 and triggers a status dump. It
causes a brief audible mute, so it belongs on an explicit user action, never on
a timer. It is the only known way to *ask* for state.

The play-lock appears to be distinct from the scale/tonic lock described below;
nothing observed clears that one except a power cycle.

### `0x20` — dump begins

Empty payload. Seed Lab does not handle this command at all, so its meaning is
our inference from position. Live observation (2026-08-10) confirms a stable
cycle, repeated for as long as the device is talking:

```
10 (seed) -> 20 (dump begin) -> 21 (globals) -> 22 x N (one per track) -> repeat
```

Treated here as "a new patch description follows", which is why it clears the
accumulated engine list.

**The device re-broadcasts this whole cycle at a high rate** — roughly 500
frames per second was measured after loading a seed. It is not constant: the
device was silent at other times, so what starts and stops the stream is not
yet known. Two consequences. Passive capture gets the complete status without
ever sending `0x11`, so the mute is avoidable in practice. And anything that
reacts per frame must be cheap: writing a file per globals frame, for one
example, is a few hundred writes a second.

### `0x21` — globals

| Byte | Meaning |
|---|---|
| 0 | patch mode |
| 1–2 | tempo, 14-bit, low septet first |
| 3 | tonic, pitch class 0–11 |
| 4 | scale index, 0–19 |

Captured: `02 7D 00 02 11` → mode 2, 125 BPM, tonic D, scale 17 (prometheus).

**The tonic byte is not stable.** Across consecutive dumps from one running
patch — same seed, same tempo, same scale — byte 3 was observed cycling through
0x0B, 0x06, 0x09 and 0x04 (B, F♯, A, E). Either it reports the *current* root
of the running material rather than a patch setting, or it is not the tonic at
all. Seed Lab treats it as a static tonic. Do not build anything that assumes
it holds still until this is understood.

**Tempo caveat.** The capture's note grid is built from a single ~0.385 s pulse,
which does not divide evenly into 125 BPM — closer to a 4:5 relationship. The
recording was made in Logic at 120 BPM, but that only affects how ticks are
labelled, not the real-time intervals, so the mismatch is real and unexplained.
Do not derive a clock from this field without measuring first.

### `0x22` — track engine

| Byte | Meaning |
|---|---|
| 0 | track index, 0-based |
| 1 | unknown, **varies** |
| 2 | unknown, **varies** |
| 3… | engine name, ASCII, NUL-terminated |

Seed Lab skips bytes 1 and 2 without comment. They are not constants — observed
pairs so far:

| Frame | Bytes 1,2 | Engine |
|---|---|---|
| `00 00 01 …` | 0x00, 0x01 | FM |
| `00 02 00 …` | 0x02, 0x00 | Plucked String |
| `01 00 01 …` | 0x00, 0x01 | Acid (303) |
| `02 00 00 …` | 0x00, 0x00 | Speech |

They plausibly carry the track's role and some engine variant, but that is a
guess. Engine names seen to date: `FM`, `Speech`, `Plucked String`, `Acid (303)`.

A full multi-track dump sends one frame per track in ascending track order,
confirmed live with a three-track patch.

## Channel messages

| What | Message | Notes |
|---|---|---|
| Scale | CC9 on ch 1 | 20 bands over 0–127; send the band midpoint, `floor(3.2 + 6.4 × index)` |
| Tonic | note on/off, ch 1 | note `60 + pitchClass`, ~80–100 ms pulse |
| Volume | CC7 on ch 1–5 | ch 1 master, ch 2–5 per-track takeover |
| Reverb | CC91 on ch 1–5 | **analog stereo mix out only** — the four USB stems stay dry |

**Scale and tonic lock.** Setting either sticks on the hardware and changes
which engines a given seed produces. A seed loaded after a lock does not sound
like the same seed loaded before it. Only a power cycle is known to clear this.

## The note stream

Confirmed by the capture: the device emits its internal sequences as ordinary
note-on/note-off, one MIDI channel per track. In the capture, a 3-track patch
produced 78 note-ons across channels 1–3 (24 / 17 / 37), with real velocity
variation on two of the three tracks and a fixed velocity on the third.

This means capturing sequences needs no protocol work — it is a plain MIDI
recording in whatever host you like.

## Not implemented here

Seed Lab also reimplements the device's patch generator in JavaScript
(MT19937 seeded with the 32-bit seed, then a fixed draw order producing tempo,
track count, roles, tonic, scale, sequences, and engine choices). That is what
lets it search seed space offline.

We deliberately do not port it. It is someone else's reverse-engineering work
carrying firmware-derived constants, published without a licence, and it would
silently diverge on the next firmware update. Every piece of musical metadata
in this project comes from what the hardware actually reported.
