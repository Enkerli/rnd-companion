#pragma once

// The suite's Shared Frame, in JUCE.
//
// Spec: @enkerli/ui `components/global-cluster.js` and the "Shared Frame —
// Consistency Pass" document. The constant is the ORDER, which is the whole
// point of the thing: theme · MIDI · density · Library · build. A person who
// learns where the theme control lives in one suite app should find it in the
// same place here.
//
// The web cluster's MIDI slot has a `native: true` mode for JUCE apps, whose
// chip reads "MIDI · native" because routing lives in the host. We are the
// native app and we *do* own our ports, so this shows them for real — the same
// slot, doing the job the web version delegates.

#include "SuiteTheme.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <functional>

class SharedFrame : public juce::Component
{
public:
    SharedFrame();
    ~SharedFrame() override;

    enum class ThemeChoice { automatic, light, dark };

    //==============================================================================
    // Slot 1 · theme. The suite has ONE theme control and it is a toggle whose
    // label names the mode you would GET (global-cluster.js slot 1). The v2
    // design pass called the Auto/Light/Dark dropdown out by name: theme is one
    // tap everywhere in the suite, so it is one tap here.
    std::function<void (ThemeChoice)> onThemeChange;
    void setThemeChoice (ThemeChoice, juce::NotificationType = juce::dontSendNotification);

    /// Which theme is actually showing, so the toggle can name the other one.
    void setResolvedDark (bool);

    // Slot 2 · MIDI
    struct MidiState
    {
        juce::Array<juce::MidiDeviceInfo> inputs, outputs;
        juce::String selectedInput, selectedOutput;   ///< identifiers
        bool connected {};
    };

    void setMidiState (const MidiState&);
    std::function<void (const juce::String&)> onSelectInput, onSelectOutput;
    std::function<void()> onFindDevice, onRescan;

    // Slot 3 · density
    std::function<void (bool dense)> onDensityChange;
    bool isDense() const noexcept { return dense; }
    void setDense (bool, juce::NotificationType = juce::dontSendNotification);

    // Slot 4 · library
    std::function<void (bool shown)> onLibraryToggle;
    void setLibraryState (bool shown, int count);

    // Slot 5 · build (non-interactive, rightmost)
    void setBuildId (const juce::String&);

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void lookAndFeelChanged() override;

    /// The cluster's natural height at the current density.
    int preferredHeight() const;

private:
    void showMidiPanel();
    void refreshMidiChip();

    /// The open MIDI panel, if any. It reads the port list once, and pressing
    /// Find RND from inside it changes that list underneath it -- so the frame
    /// refreshes it rather than leaving it saying "none" after a connect.
    juce::Component::SafePointer<juce::Component> openPanel;

    juce::TextButton themeToggle;
    bool             resolvedDark = false;
    ThemeChoice      themeChoice { ThemeChoice::automatic };

    juce::TextButton midiChip;
    MidiState        midi;

    juce::TextButton densityButton;
    bool             dense = false;

    juce::TextButton libraryButton;
    bool             libraryShown = true;
    int              libraryCount = 0;

    juce::Label      buildLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SharedFrame)
};
