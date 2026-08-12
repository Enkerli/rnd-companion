#pragma once

// Everything the companion knows and does, independent of how it reaches the
// device. The view talks to this; the plugin processor and the standalone only
// supply transports.
//
// There are two transports because the host tests said so. AUM passes SysEx to
// an AUv3 in both directions, so the host stream is real and worth using. Logic
// and Bitwig would not route our frames to the hardware at all, so a plugin that
// depends on host routing is a plugin that does not work there. Owning the port
// directly works everywhere, which is why it is the default.

#include "DeviceLink.h"
#include "RndCommands.h"
#include "SeedLibrary.h"

#include <juce_events/juce_events.h>

#include <functional>
#include <vector>

class CompanionModel : private juce::Timer
{
public:
    CompanionModel();
    ~CompanionModel() override;

    /// Light is the suite's default design target; automatic follows the OS
    /// until the person chooses. Order matches the combo and the saved state.
    enum class ThemeMode
    {
        automatic,
        light,
        dark
    };

    void      setThemeMode (ThemeMode mode) { currentTheme = mode; }
    ThemeMode themeMode() const noexcept { return currentTheme; }

    enum class Transport
    {
        direct,   ///< Our own MIDI port. Works in every host, needs no routing.
        host,     ///< The host's MIDI stream. Proven in AUM; not in Logic/Bitwig.
        both
    };

    void      setTransport (Transport);
    Transport transport() const noexcept { return currentTransport; }
    static juce::String transportName (Transport);

    DeviceLink&  link()    { return deviceLink; }
    SeedLibrary& library() { return seedLibrary; }

    const rnd::DeviceStatus& status() const noexcept { return deviceStatus; }

    //==============================================================================
    void sendSeed (std::uint32_t seed);
    void requestStatusDump();
    void sendScale (int scaleIndex);
    void sendTonic (int pitchClass);
    void sendVolume (int value, bool announce);
    void sendReverb (int value, bool announce);

    /// Per-track volume, 0 = muted. The RND gives every track its own volume,
    /// which is what makes auditioning one track of a seed possible at all.
    void sendTrackVolume (int trackIndex, int value, bool announce);

    /// Applies a whole mute set at once: every track gets full volume except
    /// the muted ones. Sent as a set rather than per toggle so the device never
    /// sits in a half-applied state.
    void applyTrackMutes (const std::vector<int>& mutedTracks, int trackCount);

    /// Opens the RND if nothing is open yet and one is present. This app is
    /// for one device, so making the user hunt for it in a dropdown is asking
    /// a question with only one answer. Never steals a port the person chose:
    /// it only acts when BOTH ends are closed.
    /// Returns true when it connected something.
    bool autoConnectIfIdle();

    /// Called by whichever transport received it. Message thread only.
    void handleMessage (const rnd::Message&);

    void log (const juce::String&);

    //==============================================================================
    // Host transport, called from processBlock on the audio thread.

    /// Pulls any queued outbound commands into the host's buffer, and hands
    /// back SysEx the host delivered. Never allocates for the common empty case.
    void processHostMidi (juce::MidiBuffer&, double sampleRate, int numSamples);

    //==============================================================================
    std::function<void()>                     onStatusChanged;
    std::function<void (const juce::String&)> onLog;

private:
    void timerCallback() override;
    void dispatch (const rndcmd::Commands&);

    DeviceLink        deviceLink;
    SeedLibrary       seedLibrary;
    rnd::DeviceStatus deviceStatus;
    Transport         currentTransport { Transport::direct };
    ThemeMode         currentTheme { ThemeMode::automatic };

    /// Guards the auto-capture against the device's repeated status broadcasts.
    std::optional<std::uint32_t> lastAutoCapturedSeed;

    // Outbound queue for the host transport: written on the message thread,
    // drained on the audio thread.
    juce::CriticalSection    outboundLock;
    rndcmd::Commands         outbound;

    // Inbound from the audio thread, drained on the message thread.
    juce::CriticalSection                  inboundLock;
    std::vector<std::vector<std::uint8_t>> inbound;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionModel)
};
