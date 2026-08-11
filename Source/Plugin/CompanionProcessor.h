#pragma once

// The plugin shell. All the behaviour lives in CompanionModel; this only wires
// the host's MIDI stream to it as a second transport alongside the direct port.

#include "../App/CompanionModel.h"

#include <juce_audio_processors/juce_audio_processors.h>

class CompanionProcessor : public juce::AudioProcessor
{
public:
    CompanionProcessor();
    ~CompanionProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RND Companion"; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    CompanionModel& model() { return companionModel; }

private:
    CompanionModel companionModel;
    double currentSampleRate { 44100.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionProcessor)
};
