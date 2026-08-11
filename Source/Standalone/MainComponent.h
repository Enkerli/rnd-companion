#pragma once

#include "DeviceLink.h"
#include "SeedLibrary.h"

#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRow) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void timerCallback() override;

    void refreshPortLists();
    void refreshConnectionLabel();
    void refreshStatusDisplay();
    void refreshLibrary();

    void handleDeviceMessage (const rnd::Message&);
    void appendLog (const juce::String&);

    void sendSeedFromEditor();
    void sendRandomSeed();
    void sendSeed (std::uint32_t seed, const juce::String& reason);
    void captureCurrentSeed();
    void rateSelected (SeedEntry::Rating);
    void removeSelected();
    void sendSelected();

    std::optional<std::uint32_t> selectedSeed() const;

    //==============================================================================
    DeviceLink  link;
    SeedLibrary library;

    rnd::DeviceStatus status;

    // Ports
    juce::Label     portsHeading { {}, "MIDI" };
    juce::ComboBox  inputCombo, outputCombo;
    juce::TextButton rescanButton { "Rescan" };
    juce::TextButton autoConnectButton { "Find RND" };
    juce::Label     connectionLabel;

    // Device
    juce::Label      deviceHeading { {}, "Device" };
    juce::Label      seedDisplay;
    juce::Label      statusDisplay;
    juce::TextEditor seedEditor;
    juce::TextButton sendButton   { "Send" };
    juce::TextButton randomButton { "Random" };
    juce::TextButton readButton   { "Read device" };
    juce::TextButton captureButton { "Add to library" };
    juce::Label      readCaveat;

    // Live controls
    juce::Label     liveHeading { {}, "Live" };
    juce::ComboBox  scaleCombo, tonicCombo;
    juce::Label     scaleLabel { {}, "Scale" }, tonicLabel { {}, "Tonic" };
    juce::Slider    volumeSlider, reverbSlider;
    juce::Label     volumeLabel { {}, "Volume" }, reverbLabel { {}, "Reverb" };
    juce::Label     lockWarning;

    // Library
    juce::Label      libraryHeading { {}, "Library" };
    juce::ToggleButton showUnrated { "New" }, showKeep { "Keep" }, showPass { "Pass" };
    juce::ListBox    libraryList { "seeds", this };
    juce::TextButton keepButton { "Keep" };
    juce::TextButton passButton { "Pass" };
    juce::TextButton sendSelectedButton { "Send" };
    juce::TextButton removeButton { "Remove" };
    juce::TextEditor noteEditor;
    juce::TextButton exportButton { "Export" };
    juce::TextButton importButton { "Import" };

    std::vector<SeedEntry> visibleEntries;

    // Log
    juce::TextEditor logView;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
