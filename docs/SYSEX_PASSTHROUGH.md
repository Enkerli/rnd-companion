# Does this host pass SysEx?

Seeds only move over SysEx. If a host strips or rewrites it, the plugin plan for
that host is dead and everything else is wasted effort — so this is the first
thing to measure, before any plugin UI gets written.

`RndSysExProbe` is the instrument. It sends four known seed frames on demand and
reports every frame it receives, so **out** and **in** can be judged separately.

## The four test seeds

| Seed | Why |
|---|---|
| `0x00000000` | every septet `0x00` |
| `0xFFFFFFFF` | every septet `0x7F`, last nibble `0x0F` — the widest legal data bytes |
| `0xAA442CE7` | the seed the real hardware sent, from the capture |
| `0x0FEDCBA9` | a walking pattern landing on distinct septets |

A host that clamps, reorders, or truncates data bytes cannot produce these by
accident. The probe reports a received frame as *one of ours, intact* only when
the bytes decode back to one of these values.

**Plus one unique seed per burst.** Each press of Send appends a fifth frame,
`0x5E5D000N`, that has never been sent before. This is what makes the OUT
direction provable: the RND echoes whatever seed it just loaded, so seeing
`0x5E5D000N` come back means *this* burst reached the device. Without it, every
burst ended on the same value and "the device is playing `0x0FEDCBA9`" was
equally consistent with "your frames got through" and "it was already there".
The probe now reports **BOTH DIRECTIONS WORK** on that echo.

## Reading the verdict

The probe's own window states it, and names the format and host it is running
in, so a screenshot is a complete result.

| What it says | What it means |
|---|---|
| Host is not calling processBlock | Not a SysEx result. Arm the track or roll the transport. |
| This host DAMAGES RND SysEx | Our frames arrive **broken**. The worst case, because it looks like it works. Disqualifying. |
| SysEx IN works: N test frames arrived intact | Inbound passthrough confirmed. |
| SysEx reaches this plugin (N other frames, intact) | Somebody else's SysEx got through whole — the path is open. Now send an RND frame to confirm end to end. |
| Receiving traffic, but no SysEx yet | MIDI flows, SysEx has not been tried. |

**"Other SysEx, intact" is a pass, not a failure.** Hosts emit their own SysEx —
Logic sends MIDI Tuning Standard dumps to instrument tracks — and a host that
hands over a 400-byte foreign frame with its checksum unharmed is demonstrably
not stripping SysEx. The probe originally lumped "not an RND frame" together
with "damaged" and reported Logic as a host that rewrites SysEx. It does not.
Only frames carrying our own `6F 62 78` tag can count as damage.

For the **out** direction the probe cannot judge itself: watch the far end.

## Baseline: the rig works

Verified 2026-08-10, no host involved — the probe's own Standalone build with
its MIDI output pointed straight at the RND:

- 4 test frames sent
- the device loaded `0x0FEDCBA9` and began broadcasting its status
- 2076 frames received, **346 of them our seed, 0 unparseable**

So the probe, the codec, and the device are all sound. Anything that fails from
here is the host.

## Procedure

You need two things talking: something that sends, and something that watches.
`RND Companion` serves as both — point it at an IAC bus (macOS) or a virtual
port and it will log any RND frame it sees.

### OUT — plugin to the world

1. Open `RND Companion`, set its **MIDI input** to the port the host will send
   to (IAC Driver Bus 1, or the RND itself).
2. Load `RND SysEx Probe` on a track in the host, as a MIDI/note effect.
3. Route that track's MIDI output to the same port.
4. Roll the transport so `processBlock` runs.
5. Press **Send test burst**.
6. Four seeds should appear in RND Companion's log. Fewer than four means
   dropping; different values mean rewriting.

### IN — world to plugin

1. Set `RND Companion`'s **MIDI output** to a port the host is listening on.
2. With the probe loaded and the transport rolling, press **Send** or
   **Random** in RND Companion.
3. The probe should log the seed as *one of ours, intact*.

An RND on the same port broadcasts constantly, which drowns the log — leave
**Log all traffic** off unless you are inspecting the device itself.

## Host notes

**AUM (iPadOS, AUv3).** Load the probe as a MIDI processor. AUM's routing matrix
makes both directions straightforward; send its MIDI out to a virtual port and
watch from RND Companion on the same iPad, or over network MIDI from the Mac.

**Logic Pro (macOS, AU).** The probe is an `aumi` MIDI FX, so it goes in the MIDI
FX slot of a software instrument track. For the out direction the instrument
should be **External Instrument** pointed at the target port. Logic is the one
most likely to surprise us, since MIDI FX output is routed to the instrument
rather than to a port directly.

**Bitwig Studio (macOS, CLAP and VST3).** Test both formats — they take
different paths through the host. Use a **HW Instrument** device for the out
direction. Worth testing the CLAP first: it has real note-effect semantics,
where VST3 has no true MIDI-effect concept and JUCE builds the archetype as an
audio-passthrough Fx.

## Results

Fill in as they are measured. "Rewrites" is a worse answer than "strips" — it
fails silently.

| Host | Format | OUT | IN | Notes |
|---|---|---|---|---|
| (none — direct CoreMIDI) | Standalone | likely pass | **pass** | Baseline, 2026-08-10. 346 intact, 0 damaged. |
| AUM (iPadOS) | AUv3 | likely pass | **pass** | 2026-08-10: 4416 frames, **736 of ours intact, 0 damaged**. Full dump cycle through an AUv3. Best result so far. |
| Logic Pro | AU | ? | **likely pass** | Delivered two intact universal SysEx frames of its own (Master Fine Tuning, and a 400-byte MIDI Tuning bulk dump with valid checksum). RND frames not yet fed in. |
| Bitwig Studio | CLAP | ? | not yet exercised | Probe sent 4, received 0 — but as a Note FX its input is the *track's* MIDI input, not the HW Instrument's return. Set the track input to RND Synth before concluding anything. |
| Bitwig Studio | VST3 | | | |

"Likely pass" on OUT means the device was seen playing a seed the burst sent,
but that seed was not unique to the burst. Re-run those with the per-burst seed
and they become plain passes or plain failures.

### Why "sent 4, received 0" is not a result

A Note FX or MIDI FX sees the MIDI arriving at the *track*, not what comes back
from the hardware downstream of it. In Bitwig, a HW Instrument's return path
does not feed the Note FX slot ahead of it. To test the IN direction, the
track's MIDI input has to be the port the frames are coming from.

The OUT direction cannot be judged from the probe's own window at all — it has
no way to see past the host. Watch the far end in RND Companion.
