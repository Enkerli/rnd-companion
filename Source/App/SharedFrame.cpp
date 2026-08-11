#include "SharedFrame.h"

namespace
{
    constexpr int gap = 10;   // --es-gap

    /// The MIDI panel behind the chip — the JUCE equivalent of the cluster's
    /// popover. Two endpoints plus the two actions that only a native app can
    /// offer, since it owns the ports rather than asking a host for them.
    class MidiPanel : public juce::Component
    {
    public:
        MidiPanel (const SharedFrame::MidiState& state,
                   std::function<void (const juce::String&)> selectIn,
                   std::function<void (const juce::String&)> selectOut,
                   std::function<void()> find,
                   std::function<void()> rescan)
            : onIn (std::move (selectIn)), onOut (std::move (selectOut))
        {
            heading.setFont (suite::SuiteLookAndFeel::eyebrowFont());
            heading.setText ("MIDI DEVICES", juce::dontSendNotification);
            addAndMakeVisible (heading);

            auto fill = [] (juce::ComboBox& box, juce::Label& label, const juce::String& text,
                            const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& selected)
            {
                label.setFont (suite::SuiteLookAndFeel::eyebrowFont());
                label.setText (text, juce::dontSendNotification);

                box.setTextWhenNoChoicesAvailable ("none found");
                box.setTextWhenNothingSelected ("none");

                for (int i = 0; i < devices.size(); ++i)
                    box.addItem (devices[i].name, i + 1);

                for (int i = 0; i < devices.size(); ++i)
                    if (devices[i].identifier == selected)
                        box.setSelectedId (i + 1, juce::dontSendNotification);
            };

            fill (inputBox, inputLabel, "IN", state.inputs, state.selectedInput);
            fill (outputBox, outputLabel, "OUT", state.outputs, state.selectedOutput);

            inputBox.onChange = [this, ids = state.inputs]
            {
                const int i = inputBox.getSelectedId() - 1;
                if (i >= 0 && i < ids.size() && onIn) onIn (ids[i].identifier);
            };
            outputBox.onChange = [this, ids = state.outputs]
            {
                const int i = outputBox.getSelectedId() - 1;
                if (i >= 0 && i < ids.size() && onOut) onOut (ids[i].identifier);
            };

            findButton.onClick = [find] { if (find) find(); };
            rescanButton.onClick = [rescan] { if (rescan) rescan(); };

            const std::initializer_list<juce::Component*> children {
                &inputLabel, &inputBox, &outputLabel, &outputBox, &findButton, &rescanButton
            };
            for (auto* c : children)
                addAndMakeVisible (*c);

            setSize (320, 168);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (12);
            const int row = suite::metrics::controlHeight();

            heading.setBounds (area.removeFromTop (suite::metrics::sectionHeader));
            inputLabel.setBounds (area.removeFromTop (16));
            inputBox.setBounds (area.removeFromTop (row));
            area.removeFromTop (gap);
            outputLabel.setBounds (area.removeFromTop (16));
            outputBox.setBounds (area.removeFromTop (row));
            area.removeFromTop (gap);

            auto buttons = area.removeFromTop (row);
            findButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - gap / 2));
            rescanButton.setBounds (buttons.removeFromRight (buttons.getWidth() - gap / 2));
        }

    private:
        juce::Label      heading, inputLabel, outputLabel;
        juce::ComboBox   inputBox, outputBox;
        juce::TextButton findButton { "Find RND" }, rescanButton { "Rescan" };
        std::function<void (const juce::String&)> onIn, onOut;
    };
}

//==============================================================================
SharedFrame::SharedFrame()
{
    // Slot 1 · theme. The web toggle flips light/dark and stores the choice;
    // "auto" there is the absence of a stored one. Exposing it as a third
    // option makes the OS-following state reachable again after a choice —
    // same mechanism, one more door.
    themeLabel.setFont (suite::SuiteLookAndFeel::eyebrowFont());
    addAndMakeVisible (themeLabel);

    themeCombo.addItem ("Auto", 1);
    themeCombo.addItem ("Light", 2);
    themeCombo.addItem ("Dark", 3);
    themeCombo.setSelectedId (1, juce::dontSendNotification);
    themeCombo.setTitle ("Theme");
    themeCombo.onChange = [this]
    {
        if (onThemeChange != nullptr)
            onThemeChange (static_cast<ThemeChoice> (themeCombo.getSelectedId() - 1));
    };
    addAndMakeVisible (themeCombo);

    // Slot 2 · MIDI
    midiChip.setTitle ("MIDI devices");
    midiChip.onClick = [this] { showMidiPanel(); };
    addAndMakeVisible (midiChip);
    refreshMidiChip();

    // Slot 3 · density
    densityButton.setTitle ("Density");
    densityButton.setTooltip ("Comfortable or compact controls. Compact fits more into a small plugin pane.");
    densityButton.onClick = [this] { setDense (! dense, juce::sendNotification); };
    addAndMakeVisible (densityButton);

    // Slot 4 · library
    libraryButton.setTitle ("Library");
    libraryButton.onClick = [this]
    {
        libraryShown = ! libraryShown;
        setLibraryState (libraryShown, libraryCount);

        if (onLibraryToggle != nullptr)
            onLibraryToggle (libraryShown);
    };
    addAndMakeVisible (libraryButton);

    // Slot 5 · build. Non-interactive by spec: it answers "which build am I
    // looking at?", nothing more.
    buildLabel.setJustificationType (juce::Justification::centredRight);
    buildLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (buildLabel);

    setDense (false);
    setLibraryState (true, 0);
    lookAndFeelChanged();
}

SharedFrame::~SharedFrame() = default;

//==============================================================================
void SharedFrame::setThemeChoice (ThemeChoice choice, juce::NotificationType notify)
{
    themeCombo.setSelectedId (static_cast<int> (choice) + 1, notify);
}

void SharedFrame::setMidiState (const MidiState& state)
{
    midi = state;
    refreshMidiChip();
}

void SharedFrame::refreshMidiChip()
{
    const int ports = midi.inputs.size() + midi.outputs.size();

    midiChip.setButtonText (midi.connected ? "MIDI - connected"
                                           : (ports > 0 ? "MIDI - " + juce::String (ports)
                                                        : "MIDI - none"));

    midiChip.setTooltip (midi.connected
                             ? "Connected. Click to change ports."
                             : "No RND connected. Click to choose ports or press Find RND.");
    repaint();
}

void SharedFrame::showMidiPanel()
{
    auto panel = std::make_unique<MidiPanel> (midi, onSelectInput, onSelectOutput, onFindDevice, onRescan);

    juce::CallOutBox::launchAsynchronously (std::move (panel),
                                            getScreenBounds().withPosition (midiChip.getScreenPosition()),
                                            nullptr);
}

void SharedFrame::setDense (bool shouldBeDense, juce::NotificationType notify)
{
    dense = shouldBeDense;
    densityButton.setButtonText (dense ? "Compact" : "Comfortable");
    densityButton.setToggleState (dense, juce::dontSendNotification);

    if (notify != juce::dontSendNotification && onDensityChange != nullptr)
        onDensityChange (dense);

    resized();
}

void SharedFrame::setLibraryState (bool shown, int count)
{
    libraryShown = shown;
    libraryCount = count;

    libraryButton.setButtonText ("Library " + juce::String (count));
    libraryButton.setToggleState (shown, juce::dontSendNotification);
    libraryButton.setTooltip (shown ? "Hide the seed library" : "Show the seed library");
}

void SharedFrame::setBuildId (const juce::String& id)
{
    buildLabel.setText ("build " + id, juce::dontSendNotification);
    buildLabel.setTooltip ("Which build this is. Quote it when reporting anything odd.");
}

//==============================================================================
void SharedFrame::lookAndFeelChanged()
{
    if (auto* lf = dynamic_cast<suite::SuiteLookAndFeel*> (&getLookAndFeel()))
    {
        const auto& t = lf->theme();
        themeLabel.setColour (juce::Label::textColourId, t.fgMuted);
        buildLabel.setColour (juce::Label::textColourId, t.fgFaint);
        buildLabel.setFont (suite::SuiteLookAndFeel::monoFont (static_cast<float> (suite::metrics::textXs)));
    }
}

int SharedFrame::preferredHeight() const
{
    return suite::metrics::controlHeight (dense);
}

void SharedFrame::paint (juce::Graphics& g)
{
    if (auto* lf = dynamic_cast<suite::SuiteLookAndFeel*> (&getLookAndFeel()))
    {
        const auto& t = lf->theme();

        // A connected device gets a lit dot on the chip: state as shape and
        // colour, never colour alone.
        if (midi.connected)
        {
            const auto b = midiChip.getBounds();
            g.setColour (t.affirm);
            g.fillEllipse (static_cast<float> (b.getX()) + 8.0f,
                           static_cast<float> (b.getCentreY()) - 3.0f, 6.0f, 6.0f);
        }
    }
}

void SharedFrame::resized()
{
    auto area = getLocalBounds();
    const int h = suite::metrics::controlHeight (dense);

    // The order IS the spec: theme · MIDI · density · Library, then build hard
    // right. Anything that has to give up width gives it up in the middle.
    buildLabel.setBounds (area.removeFromRight (juce::jmin (140, area.getWidth() / 4)));
    area.removeFromRight (gap);

    themeLabel.setBounds (area.removeFromLeft (juce::jmin (44, area.getWidth() / 6)));
    themeCombo.setBounds (area.removeFromLeft (juce::jmin (96, area.getWidth() / 3)).withHeight (h));
    area.removeFromLeft (gap);

    midiChip.setBounds (area.removeFromLeft (juce::jmin (140, area.getWidth() / 3)).withHeight (h));
    area.removeFromLeft (gap);

    densityButton.setBounds (area.removeFromLeft (juce::jmin (120, area.getWidth() / 2)).withHeight (h));
    area.removeFromLeft (gap);

    libraryButton.setBounds (area.removeFromLeft (juce::jmax (0, juce::jmin (110, area.getWidth()))).withHeight (h));
}
