#pragma once

#include "../App/CompanionView.h"
#include "CompanionProcessor.h"

class CompanionEditor : public juce::AudioProcessorEditor
{
public:
    explicit CompanionEditor (CompanionProcessor&);

    void resized() override;

private:
    CompanionView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionEditor)
};
