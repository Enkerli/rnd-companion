#include "ProbeProcessor.h"
#include "ProbeEditor.h"

ProbeProcessor::ProbeProcessor()
    : AudioProcessor (BusesProperties())
{
}

const std::vector<std::uint32_t>& ProbeProcessor::testSeeds()
{
    // 0x00000000 -> every septet 0x00
    // 0xFFFFFFFF -> every septet 0x7F, last nibble 0x0F (the widest legal bytes)
    // 0xAA442CE7 -> the seed the real hardware sent, from the capture
    // 0x0FEDCBA9 -> a walking pattern that lands on distinct septets
    static const std::vector<std::uint32_t> seeds {
        0x00000000u, 0xFFFFFFFFu, 0xAA442CE7u, 0x0FEDCBA9u
    };
    return seeds;
}

void ProbeProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (audio);

    blockCount.fetch_add (1);

    // Collect first: adding to a MidiBuffer while iterating it is not safe.
    std::vector<ReceivedFrame> justArrived;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (! message.isSysEx())
            continue;

        ReceivedFrame frame;

        // getSysExData() skips the leading F0; the size excludes the trailing F7.
        const auto* data = message.getSysExData();
        const auto  size = message.getSysExDataSize();
        if (data != nullptr && size > 0)
            frame.bytes.assign (data, data + size);

        if (const auto parsed = rnd::parseSysex (frame.bytes))
        {
            if (const auto* seedMessage = std::get_if<rnd::SeedMessage> (&*parsed))
            {
                frame.kind = Kind::seed;
                frame.seed = seedMessage->seed;

                const auto& seeds = testSeeds();
                frame.matchesATestSeed = std::find (seeds.begin(), seeds.end(), frame.seed) != seeds.end();

                if (frame.matchesATestSeed)
                    testSeedCount.fetch_add (1);
            }
            else if (std::get_if<rnd::DumpBeginMessage> (&*parsed) != nullptr)
                frame.kind = Kind::dumpBegin;
            else if (std::get_if<rnd::GlobalsMessage> (&*parsed) != nullptr)
                frame.kind = Kind::globals;
            else if (std::get_if<rnd::TrackEngineMessage> (&*parsed) != nullptr)
                frame.kind = Kind::trackEngine;
        }
        else
        {
            undecodableCount.fetch_add (1);
        }

        justArrived.push_back (std::move (frame));
    }

    if (! justArrived.empty())
    {
        receivedCount.fetch_add (static_cast<int> (justArrived.size()));

        const juce::ScopedLock lock (receivedLock);
        for (auto& frame : justArrived)
        {
            // Bounded: the UI drains this, but a host with no editor open must
            // not grow it without limit.
            if (received.size() < 256)
                received.push_back (std::move (frame));
        }
    }

    if (sendRequested.exchange (false))
    {
        for (auto seed : testSeeds())
        {
            const auto frame = rnd::makeSeedMessage (seed);
            midi.addEvent (juce::MidiMessage::createSysExMessage (frame.data() + 1,
                                                                  static_cast<int> (frame.size()) - 2),
                           0);
        }

        sentCount.fetch_add (static_cast<int> (testSeeds().size()));
    }
}

std::vector<ProbeProcessor::ReceivedFrame> ProbeProcessor::takeReceived()
{
    std::vector<ReceivedFrame> out;

    const juce::ScopedLock lock (receivedLock);
    out.swap (received);
    return out;
}

const char* ProbeProcessor::describe (Kind kind)
{
    switch (kind)
    {
        case Kind::seed:        return "seed";
        case Kind::dumpBegin:   return "dump begin";
        case Kind::globals:     return "globals";
        case Kind::trackEngine: return "track engine";
        case Kind::undecodable: break;
    }
    return "did NOT parse";
}

void ProbeProcessor::resetCounters()
{
    sentCount.store (0);
    receivedCount.store (0);
    blockCount.store (0);
    testSeedCount.store (0);
    undecodableCount.store (0);

    const juce::ScopedLock lock (receivedLock);
    received.clear();
}

juce::AudioProcessorEditor* ProbeProcessor::createEditor()
{
    return new ProbeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProbeProcessor();
}
