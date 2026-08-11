#pragma once

// The direct transport: our own MIDI port to the RND, no host in between.
//
// This is a pure transport now -- it opens ports, sends messages somebody else
// built, and reports decoded frames. What those frames mean is CompanionModel's
// business.

#include "../Protocol/RndProtocol.h"
#include "RndCommands.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <functional>
#include <vector>

class DeviceLink : private juce::MidiInputCallback,
                   private juce::AsyncUpdater
{
public:
    DeviceLink();
    ~DeviceLink() override;

    /// Message thread.
    std::function<void (const rnd::Message&)> onMessage;
    std::function<void (const juce::String&)> onLog;
    std::function<void()>                     onConnectionChanged;

    //==============================================================================
    juce::Array<juce::MidiDeviceInfo> availableInputs() const  { return juce::MidiInput::getAvailableDevices(); }
    juce::Array<juce::MidiDeviceInfo> availableOutputs() const { return juce::MidiOutput::getAvailableDevices(); }

    /// Opens the first input and output whose name looks like an RND.
    bool connectToRnd();

    bool openInput  (const juce::String& identifier);
    bool openOutput (const juce::String& identifier);
    void closeInput();
    void closeOutput();

    bool hasInput()  const noexcept { return midiInput != nullptr; }
    bool hasOutput() const noexcept { return midiOutput != nullptr; }
    bool isConnected() const noexcept { return hasInput() && hasOutput(); }

    juce::String inputName()  const { return midiInput  != nullptr ? midiInput->getName()  : juce::String(); }
    juce::String outputName() const { return midiOutput != nullptr ? midiOutput->getName() : juce::String(); }
    juce::String inputIdentifier()  const { return openInputIdentifier; }
    juce::String outputIdentifier() const { return openOutputIdentifier; }

    /// The port-name test. The manufacturer tag alone is not proof of an RND
    /// (0x6F is not a non-commercial ID), so we match the name too.
    static bool looksLikeRnd (const juce::String& deviceName);

    //==============================================================================
    /// Sends built commands. Delayed ones are scheduled, never slept on.
    void send (const rndcmd::Commands&);

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;
    void log (const juce::String& text);

    std::unique_ptr<juce::MidiInput>  midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    juce::String openInputIdentifier, openOutputIdentifier;

    // Incoming SysEx arrives on a MIDI thread; everything else runs on the
    // message thread.
    juce::CriticalSection incomingLock;
    std::vector<std::vector<std::uint8_t>> incoming;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceLink)
};
