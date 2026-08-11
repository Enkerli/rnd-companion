#pragma once

// Owns the MIDI connection to the RND and turns it into decoded protocol
// events on the message thread.
//
// The standalone opens the device directly rather than going through a host.
// That is the point of building it first: it takes host SysEx handling out of
// the picture entirely, so anything that misbehaves here is the device or us.

#include "../Protocol/RndProtocol.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <functional>
#include <vector>

class DeviceLink : private juce::MidiInputCallback,
                   private juce::AsyncUpdater
{
public:
    DeviceLink();
    ~DeviceLink() override;

    //==============================================================================
    /// Called on the message thread whenever the device tells us something.
    std::function<void (const rnd::Message&)> onMessage;

    /// Called on the message thread for anything worth showing the user.
    std::function<void (const juce::String&)> onLog;

    /// Called when ports open, close, or the device list changes.
    std::function<void()> onConnectionChanged;

    //==============================================================================
    juce::Array<juce::MidiDeviceInfo> availableInputs() const  { return juce::MidiInput::getAvailableDevices(); }
    juce::Array<juce::MidiDeviceInfo> availableOutputs() const { return juce::MidiOutput::getAvailableDevices(); }

    /// Opens the first input and output whose name looks like an RND.
    /// Returns true when both ends are open.
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
    void sendSeed (std::uint32_t seed);

    /// Clears the play-lock and asks for a status dump. Audible: the device
    /// mutes briefly. Only ever call this from an explicit user action.
    void requestStatusDump();

    /// Selects a scale band on CC9. Locks on the hardware -- see the header of
    /// RndProtocol.h.
    void sendScale (int scaleIndex);

    /// Pulses a note on channel 1 to set the tonic. Also locks.
    void sendTonic (int pitchClass);

    /// Length of that pulse. Seed Lab uses 80-100 ms; the device only needs to
    /// see the note-on, so this is comfort margin rather than a requirement.
    static constexpr int tonicPulseMs = 100;

    void sendVolume (int value);
    void sendReverb (int value);

    /// Escape hatch for probing undocumented commands by hand.
    void sendRawSysex (const std::vector<std::uint8_t>& frame, const juce::String& description);

private:
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void handleAsyncUpdate() override;

    void sendCcToMixChannels (std::uint8_t controller, int value, const juce::String& description);
    void log (const juce::String& text);

    std::unique_ptr<juce::MidiInput>  midiInput;
    std::unique_ptr<juce::MidiOutput> midiOutput;
    juce::String openInputIdentifier, openOutputIdentifier;

    // Incoming SysEx arrives on a MIDI thread; everything else in this class
    // runs on the message thread.
    juce::CriticalSection incomingLock;
    std::vector<std::vector<std::uint8_t>> incoming;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeviceLink)
};
