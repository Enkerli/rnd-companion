#pragma once

// A MIDI-effect plugin whose only job is to answer one question per host:
// does SysEx survive the trip in and out of a plugin?
//
// Seeds only move over SysEx, so if a host strips or mangles it, the whole
// plugin plan for that host is dead and we want to know before writing a UI.
// The probe sends a burst of known frames on demand and reports every frame it
// receives, byte for byte, so both directions can be checked independently.

#include "../Protocol/RndProtocol.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <vector>

class ProbeProcessor : public juce::AudioProcessor
{
public:
    ProbeProcessor();
    ~ProbeProcessor() override = default;

    //==============================================================================
    /// The frames the probe sends. Chosen so a host that clamps, reorders, or
    /// truncates data bytes cannot produce these by accident: all-zero and
    /// all-ones septets, the seed from the hardware capture, and a
    /// walking-bit pattern.
    static const std::vector<std::uint32_t>& testSeeds();

    /// Base for the per-burst seed. Every burst ends on a value nobody has sent
    /// before, which is what makes the device's echo *proof* that this burst
    /// got out rather than a leftover from an earlier one.
    static constexpr std::uint32_t burstSeedBase = 0x5E5D0000u;

    //==============================================================================
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RND SysEx Probe"; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    //==============================================================================
    /// Queues the test burst. Safe to call from the message thread; the frames
    /// go out on the next processBlock.
    void requestSend();

    /// The unique seed the most recent burst ended on. Seeing this come back
    /// means the plugin's output reached the device and the device answered --
    /// OUT and IN both, in one observation.
    std::uint32_t currentBurstSeed() const { return burstSeed.load(); }

    /// How many times this burst's unique seed has come back.
    int burstEchoes() const { return burstEchoCount.load(); }

    void resetCounters();

    int framesSent()     const { return sentCount.load(); }
    int framesReceived() const { return receivedCount.load(); }
    int blocksSeen()     const { return blockCount.load(); }

    /// The verdict signal. An RND on the same port broadcasts its own status
    /// constantly, so a raw received count says nothing about passthrough --
    /// only frames carrying one of our test seeds do.
    int testSeedsReceived() const { return testSeedCount.load(); }

    /// Frames carrying our tag that would not parse: the signature of a host
    /// that truncates or rewrites SysEx rather than dropping it outright.
    int damagedReceived() const { return damagedCount.load(); }

    /// Well-formed SysEx belonging to somebody else -- Logic's tuning dumps,
    /// say. Not our data, but positive evidence: a host that hands over a
    /// 400-byte frame with its checksum intact is not stripping SysEx.
    int foreignReceived() const { return foreignCount.load(); }

    /// True once processBlock has run at all -- distinguishes "the host never
    /// calls us" from "the host calls us but drops SysEx".
    bool isBeingProcessed() const { return blockCount.load() > 0; }

    /// What a received frame turned out to be. Every one of these except
    /// `undecodable` means the host delivered intact bytes -- reporting a
    /// globals or engine frame as a failure would make the probe lie about the
    /// very thing it exists to measure.
    enum class Kind
    {
        damaged,        ///< Carries our tag but will not parse. The bad case.
        foreign,        ///< Valid SysEx from someone else. Proof the host passes SysEx.
        seed,
        dumpBegin,
        globals,
        trackEngine
    };

    struct ReceivedFrame
    {
        std::vector<std::uint8_t> bytes;
        Kind                      kind { Kind::damaged };
        std::uint32_t             seed {};
        bool                      matchesATestSeed {};
        bool                      matchesThisBurst {};
        std::string               foreignLabel;
    };

    static const char* describe (Kind);

    /// Drains the frames received since the last call.
    std::vector<ReceivedFrame> takeReceived();

private:
    std::atomic<bool> sendRequested { false };
    std::atomic<int>  sentCount { 0 }, receivedCount { 0 }, blockCount { 0 };
    std::atomic<int>  testSeedCount { 0 }, damagedCount { 0 }, foreignCount { 0 };
    std::atomic<int>  burstCounter { 0 }, burstEchoCount { 0 };
    std::atomic<std::uint32_t> burstSeed { 0 };

    juce::CriticalSection          receivedLock;
    std::vector<ReceivedFrame>     received;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProbeProcessor)
};
