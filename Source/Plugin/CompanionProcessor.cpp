#include "CompanionProcessor.h"
#include "CompanionEditor.h"

CompanionProcessor::CompanionProcessor()
    : AudioProcessor (BusesProperties())
{
}

void CompanionProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void CompanionProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (audio);

    companionModel.processHostMidi (midi, currentSampleRate, audio.getNumSamples());
}

void CompanionProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    // The seed is 32 bits, which will not survive a float parameter -- so it
    // lives in state, not automation. The library is its own file.
    juce::ValueTree state ("RndCompanion");
    state.setProperty ("transport", static_cast<int> (companionModel.transport()), nullptr);

    if (const auto seed = companionModel.status().seed)
        state.setProperty ("seed", juce::String (rnd::formatSeed (*seed)), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void CompanionProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        const auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid())
            return;

        const int transport = state.getProperty ("transport", 0);
        companionModel.setTransport (static_cast<CompanionModel::Transport> (juce::jlimit (0, 2, transport)));
    }
}

juce::AudioProcessorEditor* CompanionProcessor::createEditor()
{
    return new CompanionEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompanionProcessor();
}
