# Field test: Linux and Windows

CI has taken these builds as far as CI can. `pluginval` at strictness 8 loads the
VST3 on both platforms, runs audio through it, and opens and automates the
editor — so we know the plugin instantiates and the WebView comes up. What no
machine in a data centre can tell us is whether any of it works with an RND
plugged into it, because a CI container has no MIDI and no ears.

That is what this checklist is for. Work down it in order; it is sorted by how
much we learn per minute, and the first few items are where the failures are
most likely to be.

A dual-boot machine is the ideal instrument here: same RND, same USB cable, same
everything, so any difference between the two halves is the operating system and
nothing else. Note anything that differs between them.

---

## 0. Before anything else: will the Linux binary even start?

**Run this first.** It takes two seconds and can save you an evening.

```bash
ldd --version | head -1
```

The shipped Linux build needs **glibc 2.38 or newer**. It is built on
`ubuntu-latest` (24.04, glibc 2.39) and picked up `__isoc23_*` symbols that do
not exist in older releases.

| Distro | glibc | Shipped build runs? |
|---|---|---|
| Ubuntu 24.04+, Mint 22, Fedora 39+, Debian 13 | ≥ 2.38 | yes |
| Ubuntu 22.04, Pop!_OS 22.04, Mint 21, Debian 12 | 2.35–2.36 | **no** |

If yours is below 2.38 you will get `version 'GLIBC_2.38' not found` (or a host
that silently refuses to scan the plugin, which is the same thing wearing a
disguise). Stop and say so — the fix is on our end, either moving the Linux CI
leg to an older runner or building on your machine. Do not spend time debugging
it as if it were a plugin bug.

Then check the WebView runtime, which is a genuine dependency and not optional:

```bash
sudo apt install libgtk-3-0 libwebkit2gtk-4.1-0
```

A plugin window that opens but stays blank almost always means this is missing.
The binary will accept either WebKitGTK 4.1 or 4.0.

## 1. What is the RND actually called?

This is the highest-value thirty seconds in the list.

Auto-connect looks for the literal substring `RND` in the MIDI port name, case
insensitive. That works on macOS, where CoreMIDI reports the device's USB
product string. Neither of the other two platforms promises the same:

- **Linux/ALSA** reports a client and port name that may be the card name, or
  something generic like `USB MIDI Device`.
- **Windows/WinMM** truncates names to 31 characters and is fond of replacing
  them with the interface name.

So: open the app, open the MIDI panel, and **write down the exact strings** in
the input and output dropdowns on each OS. Verbatim, including case.

- [ ] Linux input name: `________________`
- [ ] Linux output name: `________________`
- [ ] Windows input name: `________________`
- [ ] Windows output name: `________________`
- [ ] Did it connect on its own, or did you have to pick the ports by hand?

If the name has no `RND` in it, the app is working exactly as designed and the
design is wrong. That is a one-line fix once we know what to match on, so the
names above are the deliverable, not a workaround.

## 2. Does a status dump arrive intact?

Press **Find RND**, then **Read Device**. The device mutes briefly and answers
with a burst, then broadcasts continuously at roughly 500 frames a second.

That sustained rate is the thing to watch. It is far above what a MIDI port
normally carries, and the SysEx input buffering differs on every platform —
WinMM hands back long messages through a fixed pool of headers, ALSA through
its own. If frames are being dropped or truncated we will see it here first.

- [ ] Seed appears, and matches what the device shows
- [ ] Tempo, root and scale populate
- [ ] Engine names populate (all four tracks, not blank)
- [ ] **The log stays quiet.** Any `Damaged RND frame from host` line is a real
      finding — copy the hex out, it tells us where the frame was cut.

## 3. Does sending a seed change the hardware?

The core function of the whole application.

- [ ] Type a seed you know, send it, confirm the RND lands on it
- [ ] Capture a seed, confirm it enters the library
- [ ] Send one back from the library

## 4. Track auditioning

Per-track volume on channels 2–5, which is what makes hearing one track of a
seed possible at all.

- [ ] Mute one track — that track alone goes silent
- [ ] Solo one track — the other three go silent
- [ ] Unmute everything — all four come back

## 5. The UI itself

`pluginval` proved the editor opens. Nobody has ever looked at it.

- [ ] It paints, rather than showing a blank or white rectangle
- [ ] Fonts and spacing are sane; nothing overlaps or is clipped
- [ ] The MIDI popover opens and stays open (it closed itself on iPad once)
- [ ] Resizing behaves
- [ ] A screenshot of each OS, please

## 6. The library on disk

- [ ] Ratings, notes and mutes survive quitting and reopening
- [ ] Export writes a file — the native save dialog appears and works
- [ ] Import reads it back
- [ ] **Cross-platform:** copy a library exported on the Mac onto this machine
      and import it. Same envelope, so it should simply load; if it does not,
      that is a bug worth knowing about.

## 7. Hosts

The default transport is **Direct MIDI port** — the app owns the port itself and
asks nothing of the host. That is the one that must work, and it is the one to
test first everywhere.

The **Host MIDI stream** transport is a bonus that depends on the host being
willing to carry SysEx to hardware. We already know AUM does and that Logic and
Bitwig do not. Treat a Bitwig failure there as confirmation, not a regression.

Reaper exists on both halves of your dual boot and is the most permissive host
of the three, so it is the best first try and the best place to compare the two
operating systems directly.

**Linux**

- [ ] Reaper — VST3, direct transport
- [ ] Reaper — CLAP (no validator has ever seen this format)
- [ ] Ardour — LV2 (likewise, and Ardour is the best LV2 host to try it in)
- [ ] Bitwig — VST3 and CLAP

**Windows**

- [ ] Standalone `RND Companion.exe` — the cleanest test, no host involved
- [ ] Reaper — VST3, direct transport
- [ ] Reaper — CLAP
- [ ] Bitwig or Ableton — VST3

CLAP and LV2 are the formats with no automated coverage whatsoever, since
`pluginval` speaks neither. Anything you can tell us about them is new
information.

---

## What to send back

- The exact MIDI port names from step 1, both platforms
- The contents of the log pane after a Read Device
- Screenshots of the UI on each OS
- Distro and version, and the `ldd --version` output
- Anything that differed between Linux and Windows on identical hardware

A failure with the log text attached is worth more than a pass.
