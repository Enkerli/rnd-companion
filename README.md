# RND Companion

Capture and send patch seeds on a [Cymaforma RND Synth](https://www.cymaforma.com/rnd-synth),
and keep a curated library of the ones worth remembering.

Part of the Enkerli music suite. Public domain (CC0-1.0).

## Why a standalone first

The plugin formats are the destination — AUv3 on iPad, AU/VST3/CLAP on macOS,
VST3/CLAP/LV2 on Linux. But the one thing that can sink the whole idea is not
the device: it is that **hosts are unreliable about passing plugin-generated
SysEx**, and seeds only move over SysEx. Everything else on the roadmap
degrades gracefully if SysEx does not survive; seed capture and send does not.

So the standalone comes first. It opens CoreMIDI or ALSA directly, which takes
the host out of the picture entirely — anything that misbehaves is the device
or us. Once it works, the same protocol core goes into the plugin shells and
each host can be tested against a known-good reference.

## Layout

```
Source/Protocol/    rnd_protocol — the codec. Plain C++17, no JUCE, no I/O.
Source/Standalone/  The app: MIDI link, seed library, UI.
Tests/              Protocol tests, including a real hardware capture.
docs/PROTOCOL.md    What is known about the wire protocol, and how surely.
```

`rnd_protocol` is deliberately free of JUCE so the plugin shells and a possible
WASM build of the web UI can share it unchanged.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8
```

JUCE is found in this order: an installed `JUCE` package, `$JUCE_PATH`,
`/Applications/JUCE`, then fetched from GitHub.

Tests need no JUCE at all:

```bash
cmake -B build -DRND_BUILD_APP=OFF && cmake --build build && ctest --test-dir build --output-on-failure
```

## Using it

Connect the RND over USB, press **Find RND** (it matches any port with "RND" in
the name; pick by hand if yours differs).

- **Seeds arrive on their own.** The device broadcasts its seed whenever it
  changes, silently. You do not need to poll.
- **Read device** asks for a full status dump. It costs a brief audible mute,
  which is why it is a button and not a timer.
- A full dump is when a seed's musical metadata is known, so that is when the
  library writes an entry without being asked. **Add to library** captures
  whatever is known right now.
- **Scale** and **Tonic** lock on the hardware and change what a seed produces.
  Only a power cycle clears it. The UI says so; believe it.
- **Reverb** applies to the analog stereo mix out only. The four USB stems stay
  dry, so audition it on headphones.

The library lives at `~/Library/Application Support/RND Companion/seed-library.json`
on macOS and exports to plain JSON.

## What this does not do

It does not reimplement the device's patch generator, so it cannot predict what
a seed will sound like or search seed space offline. Every piece of musical
metadata comes from what the hardware actually reported. See the last section of
[docs/PROTOCOL.md](docs/PROTOCOL.md) for why.

## Roadmap

- [x] Protocol core with tests against a hardware capture
- [x] Standalone: capture, send, curate, live scale/tonic/volume/reverb
- [ ] Per-host SysEx round-trip check, then the plugin shells (CLAP and AUv3
      first — cleanest note-effect semantics)
- [ ] Controller channel routing
- [ ] MIDI clock toggle, once the tempo field is calibrated
- [ ] BIP39 three-word seed phrases, to match how Seed Lab writes them
