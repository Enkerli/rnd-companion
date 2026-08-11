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

## Reading the verdict

The probe's own window states it, and names the format and host it is running
in, so a screenshot is a complete result.

| What it says | What it means |
|---|---|
| Host is not calling processBlock | Not a SysEx result. Arm the track or roll the transport. |
| SysEx IN works: N test frames arrived intact | Inbound passthrough confirmed. |
| Frames arrive but do not parse | The host is **rewriting** SysEx — the worst case, because it looks like it works. |
| Receiving RND traffic, but none of our test seeds | Inbound works for the device, but the generator is not reaching this track. |

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
| (none — direct CoreMIDI) | Standalone | pass | pass | Baseline above, 2026-08-10 |
| AUM | AUv3 | | | |
| Logic Pro | AU | | | |
| Bitwig Studio | CLAP | | | |
| Bitwig Studio | VST3 | | | |
