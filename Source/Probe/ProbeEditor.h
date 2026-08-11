#pragma once

#include "ProbeProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

/// Deliberately self-documenting: it names the host and the plugin format it is
/// running as, so a screenshot of this window is a complete test result.
class ProbeEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit ProbeEditor (ProbeProcessor&);
    ~ProbeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void appendLine (const juce::String&);

    ProbeProcessor& probe;

    juce::Label      identity;
    juce::Label      instructions;
    juce::TextButton sendButton  { "Send test burst" };
    juce::TextButton resetButton { "Reset" };
    juce::TextButton copyButton  { "Copy report" };
    juce::ToggleButton logEverything { "Log all traffic" };
    juce::Label      outCounters, inCounters, verdict;
    juce::TextEditor logView;

    int loggedLines = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProbeEditor)
};
