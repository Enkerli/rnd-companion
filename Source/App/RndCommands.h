#pragma once

// Every outbound action as plain MIDI messages, built once and delivered by
// whichever transport is in use.
//
// This split exists because there are two ways to reach the RND and they need
// identical bytes: straight out of our own CoreMIDI/ALSA port, or through the
// host's MIDI stream. Building the messages in one place is what keeps "send a
// seed" meaning the same thing in a plugin and in the standalone.

#include "../Protocol/RndProtocol.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace rndcmd
{

/// A message plus how long to wait before it goes out. Only the tonic pulse
/// needs a delay; everything else is immediate.
struct TimedMessage
{
    juce::MidiMessage message;
    int               delayMs { 0 };
};

using Commands = std::vector<TimedMessage>;

Commands seed (std::uint32_t value);

/// Clears the play-lock and asks for a dump. Audible: brief mute.
Commands unlockAndDump();

/// Selects a scale band on CC9. Locks on the hardware.
Commands scale (int scaleIndex);

/// Pulses a note on channel 1. Also locks.
Commands tonic (int pitchClass, int pulseMs = 100);

Commands volume (int value);
Commands reverb (int value);

/// Wraps a complete SysEx frame (F0…F7) as a JUCE message.
juce::MidiMessage sysexFrom (const std::vector<std::uint8_t>& frame);

}  // namespace rndcmd
