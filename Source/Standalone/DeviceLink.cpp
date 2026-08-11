#include "DeviceLink.h"

DeviceLink::DeviceLink() = default;

DeviceLink::~DeviceLink()
{
    cancelPendingUpdate();
    closeInput();
    closeOutput();
}

//==============================================================================
bool DeviceLink::looksLikeRnd (const juce::String& deviceName)
{
    return deviceName.containsIgnoreCase ("RND");
}

bool DeviceLink::connectToRnd()
{
    juce::String foundInput, foundOutput;

    for (const auto& device : availableInputs())
        if (looksLikeRnd (device.name))
        {
            foundInput = device.identifier;
            break;
        }

    for (const auto& device : availableOutputs())
        if (looksLikeRnd (device.name))
        {
            foundOutput = device.identifier;
            break;
        }

    if (foundInput.isEmpty() && foundOutput.isEmpty())
    {
        log ("No MIDI port with 'RND' in its name. Pick ports by hand if yours is named differently.");
        return false;
    }

    if (foundInput.isNotEmpty())
        openInput (foundInput);

    if (foundOutput.isNotEmpty())
        openOutput (foundOutput);

    return isConnected();
}

bool DeviceLink::openInput (const juce::String& identifier)
{
    closeInput();

    midiInput = juce::MidiInput::openDevice (identifier, this);
    if (midiInput == nullptr)
    {
        log ("Could not open MIDI input.");
        if (onConnectionChanged != nullptr)
            onConnectionChanged();
        return false;
    }

    openInputIdentifier = identifier;
    midiInput->start();
    log ("Listening on " + midiInput->getName());

    if (onConnectionChanged != nullptr)
        onConnectionChanged();

    return true;
}

bool DeviceLink::openOutput (const juce::String& identifier)
{
    closeOutput();

    midiOutput = juce::MidiOutput::openDevice (identifier);
    if (midiOutput == nullptr)
    {
        log ("Could not open MIDI output.");
        if (onConnectionChanged != nullptr)
            onConnectionChanged();
        return false;
    }

    openOutputIdentifier = identifier;
    log ("Sending to " + midiOutput->getName());

    if (onConnectionChanged != nullptr)
        onConnectionChanged();

    return true;
}

void DeviceLink::closeInput()
{
    if (midiInput != nullptr)
        midiInput->stop();

    midiInput.reset();
    openInputIdentifier.clear();
}

void DeviceLink::closeOutput()
{
    midiOutput.reset();
    openOutputIdentifier.clear();
}

//==============================================================================
void DeviceLink::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (! message.isSysEx())
        return;

    // getSysExData() skips the leading F0 and the size excludes the trailing
    // F7. The parser accepts the frame with or without them.
    const auto* data = message.getSysExData();
    const auto  size = message.getSysExDataSize();

    if (data == nullptr || size <= 0)
        return;

    {
        const juce::ScopedLock lock (incomingLock);
        incoming.emplace_back (data, data + size);
    }

    triggerAsyncUpdate();
}

void DeviceLink::handleAsyncUpdate()
{
    std::vector<std::vector<std::uint8_t>> batch;

    {
        const juce::ScopedLock lock (incomingLock);
        batch.swap (incoming);
    }

    for (const auto& frame : batch)
    {
        const auto parsed = rnd::parseSysex (frame);

        if (! parsed.has_value())
        {
            // Worth surfacing rather than dropping: an unknown command is how a
            // firmware change would first announce itself.
            juce::String hex;
            for (auto byte : frame)
                hex << juce::String::toHexString (byte).paddedLeft ('0', 2).toUpperCase() << " ";

            log ("Unrecognised SysEx: " + hex.trim());
            continue;
        }

        if (onMessage != nullptr)
            onMessage (*parsed);
    }
}

//==============================================================================
void DeviceLink::sendSeed (std::uint32_t seed)
{
    sendRawSysex (rnd::makeSeedMessage (seed),
                  "Sent seed " + juce::String (rnd::formatSeed (seed)));
}

void DeviceLink::requestStatusDump()
{
    sendRawSysex (rnd::makeUnlockAndDump(), "Requested status dump (device mutes briefly)");
}

void DeviceLink::sendRawSysex (const std::vector<std::uint8_t>& frame, const juce::String& description)
{
    if (midiOutput == nullptr)
    {
        log ("No MIDI output open.");
        return;
    }

    if (frame.size() < 3)
        return;

    // createSysExMessage wants the body without the F0/F7 wrapper.
    midiOutput->sendMessageNow (juce::MidiMessage::createSysExMessage (frame.data() + 1,
                                                                       static_cast<int> (frame.size()) - 2));
    log (description);
}

void DeviceLink::sendScale (int scaleIndex)
{
    if (midiOutput == nullptr)
    {
        log ("No MIDI output open.");
        return;
    }

    const auto value = rnd::scaleCcValue (scaleIndex);
    midiOutput->sendMessageNow (juce::MidiMessage::controllerEvent (1, rnd::cc::scale, value));
    log ("Scale -> " + juce::String (rnd::scaleName (scaleIndex))
         + " (CC9 = " + juce::String (value) + "). Locks on the device until power cycle.");
}

void DeviceLink::sendTonic (int pitchClass)
{
    if (midiOutput == nullptr)
    {
        log ("No MIDI output open.");
        return;
    }

    const int note = rnd::tonicNoteNumber (pitchClass);

    // A short pulse, scheduled rather than slept, so the note-off never blocks
    // the message thread. At 1000 "samples" per second a sample position is a
    // millisecond, which makes the pulse length readable.
    juce::MidiBuffer pulse;
    pulse.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
    pulse.addEvent (juce::MidiMessage::noteOff (1, note), tonicPulseMs);

    midiOutput->sendBlockOfMessages (pulse,
                                     juce::Time::getMillisecondCounterHiRes() + 1.0,
                                     1000.0);

    log ("Tonic -> " + juce::String (rnd::tonicName (pitchClass))
         + " (note " + juce::String (note) + "). Locks on the device until power cycle.");
}

void DeviceLink::sendVolume (int value)
{
    sendCcToMixChannels (rnd::cc::volume, value, "Volume " + juce::String (value));
}

void DeviceLink::sendReverb (int value)
{
    sendCcToMixChannels (rnd::cc::reverb, value,
                         "Reverb " + juce::String (value) + " (analog mix out only, USB stems stay dry)");
}

void DeviceLink::sendCcToMixChannels (std::uint8_t controller, int value, const juce::String& description)
{
    if (midiOutput == nullptr)
    {
        log ("No MIDI output open.");
        return;
    }

    const int clamped = juce::jlimit (0, 127, value);

    for (int channel : rnd::mixChannels)
        midiOutput->sendMessageNow (juce::MidiMessage::controllerEvent (channel, controller, clamped));

    log (description);
}

void DeviceLink::log (const juce::String& text)
{
    if (onLog != nullptr)
        onLog (text);
}
