**A work-in-progress prototype.** It is unsigned on every platform, and two of
the three platform builds have never been launched by anybody. Please read the
table before installing.

## What has actually been tested

Compiled is not the same as works, so this says which is which.

| Platform | Compiled | Loaded and validated | Run against real hardware |
|---|---|---|---|
| macOS (AU · VST3 · CLAP · Standalone) | yes | yes — `auval` | yes — a Cymaforma RND over USB |
| iPadOS (AUv3) | yes | — | yes — in AUM |
| Windows (VST3 · CLAP · Standalone) | yes, in CI | **no** | **no** |
| Linux (LV2 · VST3 · CLAP) | yes, in CI | **no** | **no** |

The Windows and Linux binaries have been built and nothing more. No host has
opened them, no plugin validator has seen them, and no RND has been on the other
end. Treat a first run as an experiment.

## Nothing is signed

Every platform will object on first launch, and each is right to:

- **macOS** — Gatekeeper. `xattr -dr com.apple.quarantine` on the plugin, or
  right-click → Open for the app.
- **Windows** — SmartScreen. More info → Run anyway.
- **Linux** — no signing to speak of, but see the runtime dependency below.

## Before it will start

The UI is a WebView, which is a real runtime dependency on two platforms:

- **Linux** needs GTK3 and WebKitGTK 4.1 — `sudo apt install libgtk-3-0 libwebkit2gtk-4.1-0`.
  A blank plugin window almost certainly means these are missing.
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
