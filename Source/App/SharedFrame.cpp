#include "SharedFrame.h"

namespace
{
    constexpr int gap = 10;   // --es-gap

    /// Kept in the anonymous namespace; SharedFrame only needs it as a Component.
    struct PanelBase : juce::Component
    {
        virtual void refresh (const SharedFrame::MidiState&) = 0;
    };

    /// The MIDI panel behind the chip — the JUCE equivalent of the cluster's
    /// popover. Two endpoints plus the two actions that only a native app can
    /// offer, since it owns the ports rather than asking a host for them.
    class MidiPanel : public PanelBase
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

            refresh (state);

            findButton.onClick = [find] { if (find) find(); };
            rescanButton.onClick = [rescan] { if (rescan) rescan(); };

            const std::initializer_list<juce::Component*> children {
                &inputLabel, &inputBox, &outputLabel, &outputBox, &findButton, &rescanButton
            };
            for (auto* c : children)
                addAndMakeVisible (*c);

            // Tall enough for both endpoints AND the two actions. At 168 the
            // buttons were laid out below the bottom edge -- present, invisible.
            setSize (320, 214);
        }

        /// Re-reads the ports. Pressing Find RND from inside this panel changes
        /// the selection under it, and a panel still saying "none" after a
        /// successful connect is simply wrong.
        void refresh (const SharedFrame::MidiState& state) override
        {
            auto fill = [] (juce::ComboBox& box, juce::Label& label, const juce::String& text,
                            const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& selected)
            {
                label.setFont (suite::SuiteLookAndFeel::eyebrowFont());
                label.setText (text, juce::dontSendNotification);

                box.clear (juce::dontSendNotification);
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
    // Slot 1 · theme. One tap, and the label names the mode you would get --
    // the suite's single ThemeToggle rather than a bespoke dropdown.
    themeToggle.setTitle ("Theme");
    themeToggle.onClick = [this]
    {
        // Explicit light/dark, exactly like theme.js: "auto" there is the
        // absence of a stored choice, and toggling is how you make one.
        themeChoice = resolvedDark ? ThemeChoice::light : ThemeChoice::dark;

        if (onThemeChange != nullptr)
            onThemeChange (themeChoice);
    };
    addAndMakeVisible (themeToggle);

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
    themeChoice = choice;

    if (notify != juce::dontSendNotification && onThemeChange != nullptr)
        onThemeChange (themeChoice);
}

void SharedFrame::setResolvedDark (bool isDark)
{
    resolvedDark = isDark;

    // The label is the destination, not the current state.
    themeToggle.setButtonText ((isDark ? suite::glyph::sun() : suite::glyph::moon())
                               + juce::String (isDark ? "  Light" : "  Dark"));
    themeToggle.setTooltip (isDark ? "Switch to the light theme" : "Switch to the dark theme");
}

void SharedFrame::setMidiState (const MidiState& state)
{
    midi = state;
    refreshMidiChip();

    if (auto* panel = dynamic_cast<PanelBase*> (openPanel.getComponent()))
        panel->refresh (midi);
}

void SharedFrame::refreshMidiChip()
{
    const int ports = midi.inputs.size() + midi.outputs.size();

    // "MIDI · n", as the shared cluster writes it. The separator is U+00B7, so
    // it goes through fromUTF8 like every other non-ASCII character here.
    midiChip.setButtonText ("MIDI " + suite::glyph::middot() + " "
                            + (ports > 0 ? juce::String (ports) : juce::String ("none")));

    midiChip.setTooltip (midi.connected
                             ? "Connected. Click to change ports."
                             : "No RND connected. Click to choose ports or press Find RND.");
    repaint();
}

void SharedFrame::showMidiPanel()
{
    auto panel = std::make_unique<MidiPanel> (midi, onSelectInput, onSelectOutput, onFindDevice, onRescan);
    openPanel = panel.get();

    // A CallOutBox's content is not in our component tree, so it would render
    // with JUCE's default dark LookAndFeel -- a dark popover on paper.
    panel->setLookAndFeel (&getLookAndFeel());

    auto& box = juce::CallOutBox::launchAsynchronously (
        std::move (panel),
        midiChip.getScreenBounds(),   // the chip itself: a CallOutBox points at what it is given
        nullptr);

    // The box draws its own background, so it needs the theme too.
    box.setLookAndFeel (&getLookAndFeel());
}

void SharedFrame::setDense (bool shouldBeDense, juce::NotificationType notify)
{
    dense = shouldBeDense;
    densityButton.setButtonText (suite::glyph::density() + juce::String (dense ? "  Dense" : "  Cozy"));
    densityButton.setToggleState (dense, juce::dontSendNotification);

    if (notify != juce::dontSendNotification && onDensityChange != nullptr)
        onDensityChange (dense);

    resized();
}

void SharedFrame::setLibraryState (bool shown, int count)
{
    libraryShown = shown;
    libraryCount = count;

    libraryButton.setButtonText ("Library  " + juce::String (count));
    libraryButton.setToggleState (shown, juce::dontSendNotification);
    libraryButton.setTooltip (shown ? "Hide the seed library" : "Show the seed library");
}

void SharedFrame::setBuildId (const juce::String& id)
{
    // The cluster shows the id alone at 75% opacity; the word "build" lives in
    // the accessible name rather than taking width from it.
    buildLabel.setText (id, juce::dontSendNotification);
    buildLabel.setTitle ("Build " + id);
    buildLabel.setTooltip ("Build id. Quote it when reporting anything odd.");
}

//==============================================================================
void SharedFrame::lookAndFeelChanged()
{
    if (auto* lf = dynamic_cast<suite::SuiteLookAndFeel*> (&getLookAndFeel()))
    {
        const auto& t = lf->theme();
        buildLabel.setColour (juce::Label::textColourId, t.fgMuted.withAlpha (0.75f));
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

    themeToggle.setBounds (area.removeFromLeft (juce::jmin (104, area.getWidth() / 4)).withHeight (h));
    area.removeFromLeft (gap);

    midiChip.setBounds (area.removeFromLeft (juce::jmin (140, area.getWidth() / 3)).withHeight (h));
    area.removeFromLeft (gap);

    densityButton.setBounds (area.removeFromLeft (juce::jmin (120, area.getWidth() / 2)).withHeight (h));
    area.removeFromLeft (gap);

    libraryButton.setBounds (area.removeFromLeft (juce::jmax (0, juce::jmin (110, area.getWidth()))).withHeight (h));
}
