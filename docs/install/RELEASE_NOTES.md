**A work-in-progress prototype.** It is unsigned on every platform, and no
human being has yet run the Windows or Linux builds. Please read the table
before installing.

## Changed since alpha.1

- **The Linux zip now contains the standalone application.** It was built on
  every CI run and collected on none of them: the artifact patterns were keyed
  on file extensions and JUCE's Linux standalone has none. It is the one build
  that reaches MIDI without a host in the way, so it is the first thing to try.
- **The Linux build runs on older distributions.** Built on 24.04 it demanded
  glibc 2.38, which excluded Ubuntu 22.04, Mint 21, Pop!_OS 22.04 and Debian 12
  — and excluded them by refusing to load, which in a plugin host is
  indistinguishable from the plugin not being installed. The floor is now 2.35.
- **Everything in the zips is executable again.** Every binary in every alpha.1
  zip shipped without its executable bit, because the CI step that collects the
  builds does not preserve file modes. On macOS this is fatal and silent: the
  `.app` refuses to open at all. It escaped notice because macOS was always
  tested from a local build and never from the published zip.
- Nothing changed in the application itself. Same code as alpha.1.

## What has actually been tested

Compiled is not the same as works, so this says which is which.

| Platform | Compiled | Loaded and validated | Run against real hardware |
|---|---|---|---|
| macOS (AU · VST3 · CLAP · Standalone) | yes | yes — `auval` | yes — a Cymaforma RND over USB |
| iPadOS (AUv3) | yes | — | yes — in AUM |
| Windows (VST3 · CLAP · Standalone) | yes, in CI | VST3 — `pluginval` 8 | **no** |
| Linux (LV2 · VST3 · CLAP · Standalone) | yes, in CI | VST3 — `pluginval` 8 | **no** |

On Windows and Linux, `pluginval --strictness-level 8` loads the VST3,
instantiates it, runs audio through it, and — this is the part that matters for
a WebView UI — opens the editor, opens it *while processing*, and automates it.
All of that passes. So the plugin does load and its window does come up under
WebView2 and WebKitGTK.

What that still does not cover:

- **The CLAP and LV2 builds are unvalidated.** pluginval speaks neither. They
  are compiled and nothing more.
- **No RND has ever been on the other end** of a Windows or Linux build. MIDI
  in a CI container is not MIDI on your desk; the CI log even shows ALSA
  finding no sequencer device, which is expected there and would be a problem
  on real hardware.
- **No human has used it** on either platform.

## Nothing is signed

Every platform will object on first launch, and each is right to:

- **macOS** — Gatekeeper. `xattr -dr com.apple.quarantine` on the plugin, or
  right-click → Open for the app.
- **Windows** — SmartScreen. More info → Run anyway.
- **Linux** — no signing to speak of, but see the runtime dependency below.

## Before it will start

The UI is a WebView, which is a real runtime dependency on two platforms:

- **Linux** needs GTK3 and WebKitGTK 4.1 — `sudo apt install libgtk-3-0 libwebkit2gtk-4.1-0`.
  A blank plugin window almost certainly means these are missing. It also needs
  **glibc 2.35 or newer**: Ubuntu 22.04 and up, Debian 12, Mint 21, Fedora 36.
- **Windows** needs the Edge WebView2 runtime, which ships with Windows 11 and
  current Windows 10. If the window is blank, install it from Microsoft.

Each zip carries an `INSTALL.txt` with the destination folders for that platform.

## What it is

A companion for the Cymaforma RND Synth: capture and send 32-bit patch seeds
over SysEx, keep a searchable library of the ones worth remembering, and
audition a seed's individual tracks. Connect the RND over USB before opening —
it is found and opened automatically.

The device's SysEx protocol is undocumented by the manufacturer; what is known
about it, and how confident we are in each part, is written down in
[docs/PROTOCOL.md](../PROTOCOL.md). A firmware update could invalidate any of
it.

Verify your download against `SHA256SUMS.txt`.
