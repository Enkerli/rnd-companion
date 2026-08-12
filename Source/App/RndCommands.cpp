#include "RndCommands.h"

namespace rndcmd
{

juce::MidiMessage sysexFrom (const std::vector<std::uint8_t>& frame)
{
    // createSysExMessage wants the body without the F0/F7 wrapper.
    if (frame.size() < 3)
        return {};

    return juce::MidiMessage::createSysExMessage (frame.data() + 1,
                                                  static_cast<int> (frame.size()) - 2);
}

Commands seed (std::uint32_t value)
{
    return { { sysexFrom (rnd::makeSeedMessage (value)), 0 } };
}

Commands unlockAndDump()
{
    return { { sysexFrom (rnd::makeUnlockAndDump()), 0 } };
}

Commands scale (int scaleIndex)
{
    return { { juce::MidiMessage::controllerEvent (1, rnd::cc::scale, rnd::scaleCcValue (scaleIndex)), 0 } };
}

Commands tonic (int pitchClass, int pulseMs)
{
    const int note = rnd::tonicNoteNumber (pitchClass);

    return { { juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0 },
             { juce::MidiMessage::noteOff (1, note), pulseMs } };
}

static Commands mixCc (std::uint8_t controller, int value)
{
    const int clamped = juce::jlimit (0, 127, value);

    Commands commands;
    commands.reserve (rnd::mixChannels.size());

    for (int channel : rnd::mixChannels)
        commands.push_back ({ juce::MidiMessage::controllerEvent (channel, controller, clamped), 0 });

    return commands;
}

Commands volume (int value) { return mixCc (rnd::cc::volume, value); }

Commands trackVolume (int trackIndex, int value)
{
    if (trackIndex < 0 || trackIndex >= rnd::maxTracks)
        return {};

    return { { juce::MidiMessage::controllerEvent (rnd::trackChannel (trackIndex),
                                                   rnd::cc::volume,
                                                   juce::jlimit (0, 127, value)), 0 } };
}
Commands reverb (int value) { return mixCc (rnd::cc::reverb, value); }

}  // namespace rndcmd
