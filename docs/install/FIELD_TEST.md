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

## Where this stands

**Linux — 2026-08-12, Ubuntu 26.04 (glibc 2.43, WebKitGTK 2.52.3), plugin and
standalone, against real hardware.**

Working: the WebView renders; auto-connect finds the device unaided (it enumerates
as `RND Synth MIDI 1`, so the `RND` substring match holds on ALSA exactly as it
does on CoreMIDI); reading the device; sending a seed; mute and solo; changing
the scale.

Two things came out of it that are not bugs in the plugin but do change how it
should be used, both written up under "Linux gotchas" below: the `Both`
transport can feed a MIDI loop, and the hardware port is exclusive.

Still open everywhere: export and import through the native file dialogs, the
library surviving a restart, and the CLAP and LV2 builds specifically. Windows
is entirely untested.

## Linux gotchas

**`Both` can loop.** It sends every command down the direct port *and* into the
host stream. If the host stream also reaches the same device — easy to arrange
with virtual ports, and easy to arrange by accident — the device receives each
command twice, and a note-on/note-off pair like the tonic pulse can double up.
Inbound RND SysEx is swallowed rather than forwarded, so a runaway is guarded
against, but nothing dedupes the outbound side. **Use `Direct MIDI port` unless
you have a specific reason not to, and know your host's routing before choosing
`Both`.**

**The hardware port is exclusive.** Whoever opens the RND first keeps it: the
standalone and the plugin cannot both hold it, and a DAW that has grabbed it
locks the companion out entirely ("busy"). This is not a Linux quirk to route
around so much as the reason the host transport exists at all — when the DAW
owns the port, `Host MIDI stream` is the way in. A virtual port or PipeWire
graph is the other way, and on a machine with ten virtual ports already
enumerated that is likely the more comfortable one.

Expect the same on Windows, where WinMM is exclusive too.

---

## 0. Before anything else: will the Linux binary even start?

**Run this first.** It takes two seconds and can save you an evening.

```bash
ldd --version | head -1
```

The Linux build needs **glibc 2.35 or newer**, which covers Ubuntu 22.04 and
anything more recent, Debian 12, Mint 21 and Fedora 36 up.

It briefly needed 2.38: built on `ubuntu-latest` (24.04) it picked up
`__isoc23_*` symbols that Ubuntu 22.04, Mint 21, Pop!_OS 22.04 and Debian 12 do
not have. The Linux CI leg is now pinned to 22.04, so unless your machine is
older than that this should simply pass.

If it does report `version 'GLIBC_2.xx' not found` — or if a host silently
refuses to scan the plugin, which is the same failure wearing a disguise — stop
and say so. The fix is on our end, not yours; do not debug it as a plugin bug.

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

- [x] Seed appears, and matches what the device shows
- [x] Tempo, root and scale populate
- [ ] Engine names populate (all four tracks, not blank) — unconfirmed on Linux
- [ ] **The log stays quiet.** Any `Damaged RND frame from host` line is a real
      finding — copy the hex out, it tells us where the frame was cut.

## 3. Does sending a seed change the hardware?

The core function of the whole application.

- [x] Type a seed you know, send it, confirm the RND lands on it
- [ ] Capture a seed, confirm it enters the library
- [ ] Send one back from the library

## 4. Track auditioning

Per-track volume on channels 2–5, which is what makes hearing one track of a
seed possible at all.

- [x] Mute one track — that track alone goes silent
- [x] Solo one track — the other three go silent
- [ ] Unmute everything — all four come back

## 5. The UI itself

`pluginval` proved the editor opens. Nobody has ever looked at it.

- [x] It paints, rather than showing a blank or white rectangle
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

- [x] Standalone `RND Companion` — the cleanest test, no host involved
- [x] A plugin build, direct transport
- [x] Bitwig Studio — CLAP (2026-08-12; no validator has ever seen this format,
      so this is the only evidence CLAP works anywhere but macOS)
- [ ] LV2 — the last format nothing has ever loaded. Ardour is the natural host
      but a heavy install for one check; `jalv` is a few megabytes and enough:
      `sudo apt install jalv lilv-utils`, then `lv2ls | grep -i rnd` to confirm
      the bundle is even discoverable, and `jalv.gtk <the-uri>` to open it.
      Carla is the other light option.

**Windows** — needs a reboot into it. Wine is not a substitute: it would test
Wine's VST3 shim and its WebView2 story rather than Windows, so a pass would
mean little and a failure even less.

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
