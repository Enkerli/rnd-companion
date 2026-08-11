#pragma once

// The editor is now a WebView over the suite's shared UI
// (music-suite/apps/rnd-companion), not a hand-built JUCE surface.
//
// Everything below the view is unchanged: CompanionModel still owns the state,
// DeviceLink still owns the port, SeedLibrary still writes the suite envelope.
// This file is the wire between them and the page -- nothing more, which is the
// point. The design system lives in one place and no longer has a second
// implementation here to drift from it.
//
// The event contract, both directions, is the whole interface:
//
//   JS -> C++   uiReady sendSeed readDevice sendScale sendRoot sendVolume
//               sendReverb setTransport openInput openOutput findDevice
//               rescan capture rate setNote remove exportLibrary importLibrary
//   C++ -> JS   status library ports transport log

#include "../App/CompanionModel.h"
#include "CompanionProcessor.h"

#include <EnkerliWebView.h>

#include <juce_audio_processors/juce_audio_processors.h>

class CompanionEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit CompanionEditor (CompanionProcessor&);
    ~CompanionEditor() override;

    void resized() override;

private:
    void timerCallback() override;

    enkerli::BridgedWebView::EventMap makeEvents();

    enum class OnlyIfChanged { no, yes };

    void pushStatus();
    void pushLibrary();
    void pushPorts (OnlyIfChanged = OnlyIfChanged::no);
    void pushTransport();
    void log (const juce::String&);

    static SeedEntry::Rating ratingFromVar (const juce::var&);

    CompanionModel&     model;
    enkerli::BridgedWebView web;

    /// Nothing is emitted before the page says it is listening: an event sent
    /// to a page that has not loaded is simply dropped, and the UI would open
    /// blank with no clue why.
    bool pageReady = false;

    /// Guards the poll: re-emitting an unchanged port list would re-render the
    /// cluster, and a re-render closes whatever popover is open.
    juce::String lastPortSignature;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionEditor)
};
