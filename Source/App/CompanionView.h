#pragma once

#include "CompanionModel.h"
#include "SuiteTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

class CompanionView : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::Timer,
                      private juce::DarkModeSettingListener
{
public:
    explicit CompanionView (CompanionModel&);
    ~CompanionView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /// Called by the scrolled content. Public so the nested component can
    /// forward to them; not part of the class's outward interface.
    void layoutContent (juce::Rectangle<int> bounds);
    void paintContent (juce::Graphics&);

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

    void refreshFromModel();
    void darkModeSettingChanged() override;
    void applyTheme();
    void appendLog (const juce::String&);

    void sendSeedFromEditor();
    void sendRandomSeed();
    void sendSeed (std::uint32_t seed, const juce::String& reason);
    void captureCurrentSeed();
    void rateSelected (SeedEntry::Rating);
    void removeSelected();
    void sendSelected();

    std::optional<std::uint32_t> selectedSeed() const;

    /// Everything lives inside a viewport. A host may hand an AUv3 a pane far
    /// shorter than the controls need -- without this the overflow simply gets
    /// laid out at zero or negative height and disappears, which is how the
    /// Live section went missing in a narrow window.
    struct Content : juce::Component
    {
        explicit Content (CompanionView& o) : owner (o) {}
        void resized() override { owner.layoutContent (getLocalBounds()); }
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
        CompanionView& owner;
    };

    int naturalContentHeight (int width) const;

    //==============================================================================
    CompanionModel& model;

    juce::Viewport viewport;
    Content        content { *this };

    juce::Label    transportLabel { {}, "Route" };
    juce::ComboBox transportCombo;

    suite::SuiteLookAndFeel lookAndFeel;
    juce::Label    themeLabel { {}, "Theme" };
    juce::ComboBox themeCombo;
    juce::TooltipWindow tooltips { this };

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

    /// Raised panels painted behind the two columns.
    std::vector<juce::Rectangle<int>> panelBounds;

    // Log
    juce::TextEditor logView;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanionView)
};
