#include "CompanionModel.h"

CompanionModel::CompanionModel()
{
    deviceLink.onMessage = [this] (const rnd::Message& message) { handleMessage (message); };
    deviceLink.onLog     = [this] (const juce::String& text) { log (text); };

    seedLibrary.load();

    // Drains SysEx the host handed us on the audio thread. 20 ms is well inside
    // the device's broadcast rate without making a UI job of every frame.
    startTimer (20);
}

CompanionModel::~CompanionModel()
{
    stopTimer();
}

juce::String CompanionModel::transportName (Transport transport)
{
    switch (transport)
    {
        case Transport::direct: return "Direct MIDI port";
        case Transport::host:   return "Host MIDI stream";
        case Transport::both:   return "Both";
    }
    return {};
}

void CompanionModel::setTransport (Transport transport)
{
    if (currentTransport == transport)
        return;

    currentTransport = transport;
    log ("Transport: " + transportName (transport));
}

//==============================================================================
void CompanionModel::dispatch (const rndcmd::Commands& commands)
{
    if (commands.empty())
        return;

    if (currentTransport != Transport::host)
        deviceLink.send (commands);

    if (currentTransport != Transport::direct)
    {
        const juce::ScopedLock lock (outboundLock);
        outbound.insert (outbound.end(), commands.begin(), commands.end());
    }
}

void CompanionModel::sendSeed (std::uint32_t seed)
{
    dispatch (rndcmd::seed (seed));
    log ("Sent seed " + juce::String (rnd::formatSeed (seed)));

    // Show it now; the device echoes back in its own time. Drop the rest of the
    // status: it described the previous patch.
    deviceStatus.seed = seed;
    deviceStatus.patchMode.reset();
    deviceStatus.tempoBpm.reset();
    deviceStatus.tonic.reset();
    deviceStatus.scaleIndex.reset();
    deviceStatus.engines.clear();

    if (onStatusChanged != nullptr)
        onStatusChanged();
}

void CompanionModel::requestStatusDump()
{
    dispatch (rndcmd::unlockAndDump());
    log ("Requested status dump (device mutes briefly)");
}

void CompanionModel::sendScale (int scaleIndex)
{
    dispatch (rndcmd::scale (scaleIndex));
    log ("Scale -> " + juce::String (rnd::scaleName (scaleIndex))
         + ". Locks on the device until power cycle.");
}

void CompanionModel::sendTonic (int pitchClass)
{
    dispatch (rndcmd::tonic (pitchClass));
    log ("Tonic -> " + juce::String (rnd::tonicName (pitchClass))
         + ". Locks on the device until power cycle.");
}

void CompanionModel::sendVolume (int value, bool announce)
{
    dispatch (rndcmd::volume (value));

    if (announce)
        log ("Volume " + juce::String (value));
}

void CompanionModel::sendReverb (int value, bool announce)
{
    dispatch (rndcmd::reverb (value));

    if (announce)
        log ("Reverb " + juce::String (value) + " (analog mix out only, USB stems stay dry)");
}

//==============================================================================
void CompanionModel::handleMessage (const rnd::Message& message)
{
    const bool wasSeed = std::get_if<rnd::SeedMessage> (&message) != nullptr;
    const auto previousSeed = deviceStatus.seed;

    deviceStatus.apply (message);

    if (wasSeed && deviceStatus.seed != previousSeed)
        log ("Device is on " + juce::String (rnd::formatSeed (*deviceStatus.seed)));

    // A full dump is the only moment a seed's metadata is known -- but the
    // device re-broadcasts everything hundreds of times a second, so this fires
    // on a new seed, never on every globals frame.
    if (std::get_if<rnd::GlobalsMessage> (&message) != nullptr
        && deviceStatus.seed
        && deviceStatus.seed != lastAutoCapturedSeed)
    {
        lastAutoCapturedSeed = deviceStatus.seed;
        seedLibrary.captureSeed (*deviceStatus.seed, deviceStatus);
    }

    if (onStatusChanged != nullptr)
        onStatusChanged();
}

void CompanionModel::log (const juce::String& text)
{
    if (onLog != nullptr)
        onLog (text);
}

//==============================================================================
void CompanionModel::processHostMidi (juce::MidiBuffer& midi, double sampleRate, int numSamples)
{
    if (currentTransport != Transport::direct)
    {
        // We *consume* RND SysEx rather than passing it on. In AUM it is easy to
        // wire a plugin's output back to its own input, and forwarding the
        // device's constant status broadcast into such a loop turns a routing
        // mistake into a runaway. Everything that is not ours passes through
        // untouched -- notes, clock, other vendors' SysEx.
        std::vector<std::vector<std::uint8_t>> arrived;
        juce::MidiBuffer passedThrough;

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();

            if (message.isSysEx())
            {
                const auto* data = message.getSysExData();
                const auto  size = message.getSysExDataSize();

                if (data != nullptr && size > 0 && rnd::hasManufacturerTag (data, static_cast<std::size_t> (size)))
                {
                    arrived.emplace_back (data, data + size);
                    continue;   // ours: swallowed, never forwarded
                }
            }

            passedThrough.addEvent (message, metadata.samplePosition);
        }

        midi.swapWith (passedThrough);

        if (! arrived.empty())
        {
            const juce::ScopedLock lock (inboundLock);
            for (auto& frame : arrived)
                if (inbound.size() < 512)
                    inbound.push_back (std::move (frame));
        }

        rndcmd::Commands toSend;
        {
            const juce::ScopedLock lock (outboundLock);
            toSend.swap (outbound);
        }

        for (const auto& command : toSend)
        {
            // A delayed message (only the tonic note-off) is clamped into this
            // block rather than carried across blocks: the device needs to see
            // the note-off, not to see it at an exact time.
            const int offset = command.delayMs <= 0
                                 ? 0
                                 : juce::jmin (numSamples - 1,
                                               juce::roundToInt (sampleRate * command.delayMs * 0.001));

            midi.addEvent (command.message, juce::jmax (0, offset));
        }
    }
}

void CompanionModel::timerCallback()
{
    std::vector<std::vector<std::uint8_t>> batch;

    {
        const juce::ScopedLock lock (inboundLock);
        batch.swap (inbound);
    }

    for (const auto& frame : batch)
    {
        if (const auto parsed = rnd::parseSysex (frame))
        {
            handleMessage (*parsed);
        }
        else if (rnd::hasManufacturerTag (frame))
        {
            juce::String hex;
            for (auto byte : frame)
                hex << juce::String::toHexString (byte).paddedLeft ('0', 2).toUpperCase() << " ";

            log ("Damaged RND frame from host: " + hex.trim());
        }
        // Foreign SysEx is somebody else's business; ignore it silently.
    }
}
