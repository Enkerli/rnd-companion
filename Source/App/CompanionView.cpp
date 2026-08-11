#include "CompanionView.h"

#include "BuildStamp.h"

#include <FileExport.h>
#include <FileImport.h>

namespace
{
    constexpr int gap = 10;   // --es-gap

    inline int headerHeight() { return suite::metrics::sectionHeader; }

}

CompanionView::UndoToast::UndoToast()
{
    message.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (message);

    undoButton.onClick = [this]
    {
        if (onUndo != nullptr)
            onUndo();

        dismiss();
    };
    addAndMakeVisible (undoButton);

    setVisible (false);
    setInterceptsMouseClicks (false, true);
}

void CompanionView::UndoToast::show (const juce::String& text, std::function<void()> undoAction)
{
    message.setText (text, juce::dontSendNotification);
    onUndo = std::move (undoAction);
    setVisible (true);
    toFront (false);

    // Long enough to notice and act on, short enough not to linger.
    startTimer (7000);
}

void CompanionView::UndoToast::dismiss()
{
    stopTimer();
    onUndo = nullptr;
    setVisible (false);
}

void CompanionView::UndoToast::timerCallback()
{
    dismiss();
}

void CompanionView::UndoToast::paint (juce::Graphics& g)
{
    if (auto* lf = dynamic_cast<suite::SuiteLookAndFeel*> (&getLookAndFeel()))
    {
        const auto& t = lf->theme();
        g.setColour (t.fg.withAlpha (0.95f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), static_cast<float> (suite::metrics::radiusSm));
        message.setColour (juce::Label::textColourId, t.bg);
    }
}

void CompanionView::UndoToast::resized()
{
    auto area = getLocalBounds().reduced (10, 4);
    undoButton.setBounds (area.removeFromRight (70));
    area.removeFromRight (8);
    message.setBounds (area);
}

//==============================================================================
CompanionView::CompanionView (CompanionModel& m)
    : model (m)
{
    setLookAndFeel (&lookAndFeel);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    juce::Component::addAndMakeVisible (viewport);
    juce::Component::addChildComponent (undoToast);   // hidden until something is removed

    juce::Desktop::getInstance().addDarkModeSettingListener (this);

    // ── Shared Frame ────────────────────────────────────────────────────────
    // The suite's global cluster, in its fixed order. Everything that used to
    // be a loose header control lives here now, in the place a person who uses
    // another suite app already looks.
    frame.onThemeChange = [this] (SharedFrame::ThemeChoice choice)
    {
        model.setThemeMode (static_cast<CompanionModel::ThemeMode> (choice));
        applyTheme();
    };

    frame.onSelectInput  = [this] (const juce::String& id) { model.link().openInput (id); };
    frame.onSelectOutput = [this] (const juce::String& id) { model.link().openOutput (id); };
    frame.onFindDevice   = [this] { model.link().connectToRnd(); refreshFrameMidiState(); };
    frame.onRescan       = [this] { refreshFrameMidiState(); };

    frame.onDensityChange = [this] (bool) { relayout(); };

    frame.onLibraryToggle = [this] (bool shown)
    {
        libraryShown = shown;
        const std::initializer_list<juce::Component*> libraryParts {
            &libraryHeading, &showUnrated, &showKeep, &showPass, &exportButton, &importButton,
            &libraryList, &noteEditor, &keepButton, &passButton, &sendSelectedButton, &removeButton
        };
        for (auto* c : libraryParts)
            c->setVisible (shown);

        relayout();
    };

    frame.setBuildId (RND_BUILD_STAMP);
    frame.setThemeChoice (static_cast<SharedFrame::ThemeChoice> (model.themeMode()));
    content.addAndMakeVisible (frame);

    content.addAndMakeVisible (connectionLabel);

    // ── Route ───────────────────────────────────────────────────────────────
    // Not a Shared Frame slot: the cluster's five are fixed, and which
    // transport reaches the hardware is this app's own business.
    transportLabel.setFont (suite::SuiteLookAndFeel::eyebrowFont());
    content.addAndMakeVisible (transportLabel);

    transportCombo.addItem (CompanionModel::transportName (CompanionModel::Transport::direct), 1);
    transportCombo.addItem (CompanionModel::transportName (CompanionModel::Transport::host), 2);
    transportCombo.addItem (CompanionModel::transportName (CompanionModel::Transport::both), 3);
    transportCombo.setSelectedId (static_cast<int> (model.transport()) + 1, juce::dontSendNotification);
    transportCombo.setTitle ("Route");
    transportCombo.setTooltip ("Direct works in every host and needs no routing. Host uses the "
                               "plugin's MIDI stream -- proven in AUM, not in Logic or Bitwig.");
    transportCombo.onChange = [this]
    {
        switch (transportCombo.getSelectedId())
        {
            case 2:  model.setTransport (CompanionModel::Transport::host); break;
            case 3:  model.setTransport (CompanionModel::Transport::both); break;
            default: model.setTransport (CompanionModel::Transport::direct); break;
        }
    };
    content.addAndMakeVisible (transportCombo);

    // ── Device ──────────────────────────────────────────────────────────────
    content.addAndMakeVisible (deviceHeading);

    // Mono with tabular figures for anything numeric-musical -- DESIGN.md.
    seedDisplay.setFont (suite::SuiteLookAndFeel::monoFont (30.0f, true));
    seedDisplay.setText ("--", juce::dontSendNotification);
    content.addAndMakeVisible (seedDisplay);

    statusDisplay.setJustificationType (juce::Justification::topLeft);
    content.addAndMakeVisible (statusDisplay);

    seedEditor.setFont (suite::SuiteLookAndFeel::monoFont (static_cast<float> (suite::metrics::textSm) + 1.0f));
    seedEditor.onReturnKey = [this] { sendSeedFromEditor(); };
    content.addAndMakeVisible (seedEditor);

    sendButton.onClick = [this] { sendSeedFromEditor(); };
    content.addAndMakeVisible (sendButton);

    randomButton.onClick = [this] { sendRandomSeed(); };
    content.addAndMakeVisible (randomButton);

    readButton.onClick = [this] { model.requestStatusDump(); };
    content.addAndMakeVisible (readButton);

    captureButton.onClick = [this] { captureCurrentSeed(); };
    content.addAndMakeVisible (captureButton);

    readCaveat.setFont (suite::SuiteLookAndFeel::sansFont (static_cast<float> (suite::metrics::textXs)));
    readCaveat.setText ("Reading mutes the device briefly. Seeds arrive on their own when you turn the knob.",
                        juce::dontSendNotification);
    content.addAndMakeVisible (readCaveat);

    // ── Live controls ───────────────────────────────────────────────────────
    content.addAndMakeVisible (liveHeading);

    for (auto* label : { &scaleLabel, &tonicLabel, &volumeLabel, &reverbLabel })
    {
        label->setFont (suite::SuiteLookAndFeel::eyebrowFont());
        content.addAndMakeVisible (*label);
    }

    for (int i = 0; i < rnd::numScales; ++i)
        scaleCombo.addItem (juce::String (rnd::scaleName (i)), i + 1);

    // Blank until the device reports one: these show what the RND is doing, and
    // an arbitrary default would be a claim we cannot make.
    scaleCombo.setTextWhenNothingSelected ("from device");
    scaleCombo.onChange = [this]
    {
        if (scaleCombo.getSelectedId() > 0)
            model.sendScale (scaleCombo.getSelectedId() - 1);
    };
    content.addAndMakeVisible (scaleCombo);

    for (int i = 0; i < rnd::numTonics; ++i)
        tonicCombo.addItem (juce::String (rnd::tonicName (i)), i + 1);

    tonicCombo.setTextWhenNothingSelected ("--");
    tonicCombo.onChange = [this]
    {
        if (tonicCombo.getSelectedId() > 0)
            model.sendTonic (tonicCombo.getSelectedId() - 1);
    };
    content.addAndMakeVisible (tonicCombo);

    volumeSlider.setRange (0.0, 127.0, 1.0);
    volumeSlider.setValue (100.0, juce::dontSendNotification);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, rowHeight() - 6);
    volumeSlider.onValueChange = [this]
    {
        markMixTouched();
        model.sendVolume ((int) volumeSlider.getValue(), ! volumeSlider.isMouseButtonDown());
    };
    volumeSlider.onDragEnd     = [this] { appendLog ("Volume " + juce::String ((int) volumeSlider.getValue())); };
    content.addAndMakeVisible (volumeSlider);

    reverbSlider.setRange (0.0, 127.0, 1.0);
    reverbSlider.setValue (40.0, juce::dontSendNotification);
    reverbSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, rowHeight() - 6);
    reverbSlider.onValueChange = [this]
    {
        markMixTouched();
        model.sendReverb ((int) reverbSlider.getValue(), ! reverbSlider.isMouseButtonDown());
    };
    reverbSlider.onDragEnd     = [this] { appendLog ("Reverb " + juce::String ((int) reverbSlider.getValue())
                                                     + " (analog mix out only, USB stems stay dry)"); };
    content.addAndMakeVisible (reverbSlider);

    mixCaveat.setFont (suite::SuiteLookAndFeel::sansFont (static_cast<float> (suite::metrics::textXs)));
    mixCaveat.setText ("Volume and reverb are send-only: the RND never reports them, so these show "
                       "what will be sent, not where the hardware is.",
                       juce::dontSendNotification);
    content.addAndMakeVisible (mixCaveat);

    lockWarning.setFont (suite::SuiteLookAndFeel::sansFont (static_cast<float> (suite::metrics::textXs)));
    lockWarning.setText ("Scale and tonic lock on the hardware and change what a seed produces. Power-cycle to clear.",
                         juce::dontSendNotification);
    content.addAndMakeVisible (lockWarning);

    // ── Library ─────────────────────────────────────────────────────────────
    content.addAndMakeVisible (libraryHeading);

    for (auto* toggle : { &showUnrated, &showKeep, &showPass })
    {
        toggle->setToggleState (true, juce::dontSendNotification);
        toggle->onClick = [this] { refreshLibrary(); };
        content.addAndMakeVisible (*toggle);
    }
    showPass.setToggleState (false, juce::dontSendNotification);

    libraryList.setRowHeight (juce::jmax (44, suite::metrics::controlHeight() + 14));
    libraryList.setWantsKeyboardFocus (true);
    libraryList.setTitle ("Seed library");
    content.addAndMakeVisible (libraryList);

    keepButton.onClick = [this] { rateSelected (SeedEntry::Rating::keep); };
    passButton.onClick = [this] { rateSelected (SeedEntry::Rating::pass); };
    sendSelectedButton.onClick = [this] { sendSelected(); };
    removeButton.onClick = [this] { removeSelected(); };

    for (auto* button : { &keepButton, &passButton, &sendSelectedButton, &removeButton })
        content.addAndMakeVisible (*button);

    noteEditor.onFocusLost = [this]
    {
        if (const auto seed = selectedSeed())
            model.library().setNote (*seed, noteEditor.getText());
    };
    content.addAndMakeVisible (noteEditor);

    exportButton.onClick = [this]
    {
        // enkerli::exportBytes because a plain FileChooser does nothing inside
        // an AUv3 app extension -- on iOS this becomes a share sheet presented
        // from the responder chain. Same call on every platform.
        const auto json = model.library().toJsonString();
        juce::MemoryBlock bytes (json.toRawUTF8(), json.getNumBytesAsUTF8());

        enkerli::exportBytes (*this, SeedLibrary::timestampedExportName(), bytes);
        appendLog ("Exporting " + SeedLibrary::timestampedExportName());
    };
    content.addAndMakeVisible (exportButton);

    importButton.onClick = [this]
    {
        enkerli::importFile (*this, "*.json",
                             [this] (const juce::String& name, const juce::MemoryBlock& bytes)
                             {
                                 const juce::String text (juce::CharPointer_UTF8 (
                                     static_cast<const char*> (bytes.getData())), bytes.getSize());

                                 appendLog (model.library().importJsonString (text)
                                                ? "Imported " + name
                                                : "Could not read " + name);
                             });
    };
    content.addAndMakeVisible (importButton);

    // ── Log ─────────────────────────────────────────────────────────────────
    logView.setMultiLine (true);
    logView.setReadOnly (true);
    logView.setScrollbarsShown (true);
    logView.setCaretVisible (false);
    logView.setFont (suite::SuiteLookAndFeel::monoFont (static_cast<float> (suite::metrics::textXs)));
    content.addAndMakeVisible (logView);

    // ── Wiring ──────────────────────────────────────────────────────────────
    model.onStatusChanged = [this] { refreshFromModel(); };
    model.onLog           = [this] (const juce::String& text) { appendLog (text); };
    model.link().onConnectionChanged = [this] { refreshConnectionLabel(); };
    model.library().onChanged = [this] { refreshLibrary(); };

    refreshFrameMidiState();
    refreshStatusDisplay();
    refreshLibrary();

    applyTheme();

    appendLog ("RND Companion ready. Connect the RND over USB, then press Find RND.");

    // Ports come and go while the app runs; a slow poll keeps the lists honest
    // without a platform-specific hotplug callback.
    startTimer (2000);

    setSize (1040, 720);
}

CompanionView::~CompanionView()
{
    juce::Desktop::getInstance().removeDarkModeSettingListener (this);
    setLookAndFeel (nullptr);

    model.library().flush();
}

//==============================================================================
void CompanionView::paint (juce::Graphics& g)
{
    g.fillAll (lookAndFeel.theme().bg);
}

void CompanionView::paintContent (juce::Graphics& g)
{
    const auto& t = lookAndFeel.theme();
    g.fillAll (t.bg);

    // A raised panel behind each column, per the paper-and-ink surfaces.
    g.setColour (t.bgRaised);
    for (const auto& panel : panelBounds)
        g.fillRoundedRectangle (panel.toFloat(), static_cast<float> (suite::metrics::radiusMd));

    g.setColour (t.borderSoft);
    for (const auto& panel : panelBounds)
        g.drawRoundedRectangle (panel.toFloat().reduced (0.5f),
                                static_cast<float> (suite::metrics::radiusMd), 1.0f);
}

void CompanionView::resized()
{
    viewport.setBounds (getLocalBounds());

    auto toastArea = getLocalBounds().reduced (gap * 3, 0);
    undoToast.setBounds (toastArea.removeFromBottom (rowHeight() + 12)
                                  .withTrimmedBottom (gap * 2));

    const int width = viewport.getMaximumVisibleWidth();
    content.setSize (width, juce::jmax (naturalContentHeight (width),
                                        viewport.getMaximumVisibleHeight()));
}

/// Height the controls actually need. Below this the viewport scrolls rather
/// than silently cropping.
int CompanionView::naturalContentHeight (int width) const
{
    const int row = rowHeight();
    const int header = headerHeight();

    // ports block, then the device+live column, then library, then log
    const int ports  = header + row + gap / 2 + row;
    const int device = header + 42 + 76 + gap + row + gap + row + row
                     + gap + header + row + gap + row + gap / 2 + row + row * 2;
    const int lib    = header + row + gap + 200 + gap + row + gap + row;
    const int log    = 80;

    if (width >= 860)
        return gap * 4 + ports + gap * 2 + juce::jmax (device, lib) + gap + log;

    return gap * 4 + ports + gap * 2 + device + gap + lib + gap + log;
}

int CompanionView::rowHeight() const
{
    // --es-ctl-h: 32 with a pointer, 44 under a fingertip, less when compact.
    // Reading the frame's density here is what makes the toggle move the whole
    // surface rather than only the cluster that carries it.
    return suite::metrics::controlHeight (frame.isDense());
}

void CompanionView::relayout()
{
    // Toggling a child's visibility does not change the content's SIZE, so
    // Component never calls resized() and the layout silently keeps its old
    // geometry -- which is how showing the library again left it invisible.
    resized();
    layoutContent (content.getLocalBounds());
    repaint();
}

void CompanionView::layoutContent (juce::Rectangle<int> fullBounds)
{
    // Responsive on purpose. An AUv3 is presented at whatever size the host's
    // pane happens to be, not at the size we asked for -- the transport
    // selector used to sit ~700px along a single row, which put it off-screen
    // in AUM entirely and read as "the dropdown doesn't work".
    auto area = fullBounds.reduced (gap * 2);

    const bool narrow = area.getWidth() < 860;

    logView.setBounds (area.removeFromBottom (narrow ? 80 : 110));
    area.removeFromBottom (gap);

    // ── Shared Frame, then the app's own row ───────────────────────────────
    frame.setBounds (area.removeFromTop (frame.preferredHeight()));
    area.removeFromTop (gap / 2);

    auto transportRow = area.removeFromTop (rowHeight());
    transportLabel.setBounds (transportRow.removeFromLeft (44));
    transportCombo.setBounds (transportRow.removeFromLeft (juce::jmin (170, transportRow.getWidth() / 2)));
    transportRow.removeFromLeft (gap);
    connectionLabel.setBounds (transportRow);

    area.removeFromTop (gap * 2);

    // ── Below: two columns when there is room, stacked when there is not ───
    juce::Rectangle<int> left, right;

    if (! libraryShown)
    {
        left = area;
        right = {};
    }
    else if (narrow)
    {
        // The device column gets what it needs; the library takes the rest.
        // Splitting down the middle is what made Live disappear.
        const int deviceHeight = headerHeight() + 42 + 76 + gap + rowHeight() + gap
                               + rowHeight() + rowHeight() + gap + headerHeight()
                               + rowHeight() + gap + rowHeight() + gap / 2
                               + rowHeight() + rowHeight() * 2;

        left = area.removeFromTop (juce::jmin (deviceHeight, area.getHeight()));
        area.removeFromTop (gap);
        right = area;
    }
    else
    {
        left = area.removeFromLeft (area.getWidth() / 2 - gap);
        area.removeFromLeft (gap * 2);
        right = area;
    }

    panelBounds = right.isEmpty() ? std::vector<juce::Rectangle<int>> { left.expanded (gap) }
                                  : std::vector<juce::Rectangle<int>> { left.expanded (gap), right.expanded (gap) };
    left = left.reduced (gap / 2);
    right = right.reduced (gap / 2);

    // ── Left: device + live ────────────────────────────────────────────────
    deviceHeading.setBounds (left.removeFromTop (headerHeight()));
    seedDisplay.setBounds (left.removeFromTop (42));
    statusDisplay.setBounds (left.removeFromTop (76));
    left.removeFromTop (gap);

    auto seedRow = left.removeFromTop (rowHeight());
    seedEditor.setBounds (seedRow.removeFromLeft (juce::jmax (120, seedRow.getWidth() - 170)));
    seedRow.removeFromLeft (gap);
    sendButton.setBounds (seedRow.removeFromLeft (juce::jmin (70, seedRow.getWidth())));
    seedRow.removeFromLeft (gap);
    randomButton.setBounds (seedRow.removeFromLeft (juce::jmax (0, juce::jmin (80, seedRow.getWidth()))));

    left.removeFromTop (gap);
    auto actionRow = left.removeFromTop (rowHeight());
    readButton.setBounds (actionRow.removeFromLeft (110));
    actionRow.removeFromLeft (gap);
    captureButton.setBounds (actionRow.removeFromLeft (juce::jmax (0, juce::jmin (120, actionRow.getWidth()))));

    readCaveat.setBounds (left.removeFromTop (rowHeight()));
    left.removeFromTop (gap);

    liveHeading.setBounds (left.removeFromTop (headerHeight()));

    auto scaleRow = left.removeFromTop (rowHeight());
    scaleLabel.setBounds (scaleRow.removeFromLeft (50));
    scaleCombo.setBounds (scaleRow.removeFromLeft (juce::jmax (110, scaleRow.getWidth() - 130)));
    scaleRow.removeFromLeft (gap);
    tonicLabel.setBounds (scaleRow.removeFromLeft (juce::jmin (44, scaleRow.getWidth())));
    tonicCombo.setBounds (scaleRow.removeFromLeft (juce::jmax (0, juce::jmin (70, scaleRow.getWidth()))));

    left.removeFromTop (gap);
    auto volumeRow = left.removeFromTop (rowHeight());
    volumeLabel.setBounds (volumeRow.removeFromLeft (56));
    volumeSlider.setBounds (volumeRow);

    left.removeFromTop (gap / 2);
    auto reverbRow = left.removeFromTop (rowHeight());
    reverbLabel.setBounds (reverbRow.removeFromLeft (56));
    reverbSlider.setBounds (reverbRow);

    if (left.getHeight() > 0)
        mixCaveat.setBounds (left.removeFromTop (juce::jmin (rowHeight(), left.getHeight())));

    if (left.getHeight() > 0)
        lockWarning.setBounds (left.removeFromTop (juce::jmin (rowHeight() * 2, left.getHeight())));

    // ── Right: library ─────────────────────────────────────────────────────
    libraryHeading.setBounds (right.removeFromTop (headerHeight()));

    auto filterRow = right.removeFromTop (rowHeight());
    // Export/Import claimed from the right first, so a narrow pane eats into
    // the toggles rather than pushing the buttons off the edge.
    importButton.setBounds (filterRow.removeFromRight (juce::jmin (70, filterRow.getWidth() / 4)));
    filterRow.removeFromRight (gap / 2);
    exportButton.setBounds (filterRow.removeFromRight (juce::jmin (70, filterRow.getWidth() / 3)));
    filterRow.removeFromRight (gap);

    const int toggleWidth = juce::jmax (46, filterRow.getWidth() / 3);
    showUnrated.setBounds (filterRow.removeFromLeft (toggleWidth));
    showKeep.setBounds (filterRow.removeFromLeft (juce::jmin (toggleWidth, filterRow.getWidth())));
    showPass.setBounds (filterRow.removeFromLeft (juce::jmax (0, filterRow.getWidth())));

    right.removeFromTop (gap);

    auto buttonRow = right.removeFromBottom (rowHeight());
    // Remove is destructive and sits apart from the rating actions, hard right
    // with a gap wide enough that it is not a neighbouring tap target.
    removeButton.setBounds (buttonRow.removeFromRight (juce::jmin (80, buttonRow.getWidth() / 3)));
    buttonRow.removeFromRight (gap * 3);

    const int actionWidth = juce::jmax (46, (buttonRow.getWidth() - gap * 2) / 3);
    keepButton.setBounds (buttonRow.removeFromLeft (actionWidth));
    buttonRow.removeFromLeft (gap);
    passButton.setBounds (buttonRow.removeFromLeft (juce::jmin (actionWidth, buttonRow.getWidth())));
    buttonRow.removeFromLeft (gap);
    sendSelectedButton.setBounds (buttonRow.removeFromLeft (juce::jmax (0, buttonRow.getWidth())));

    right.removeFromBottom (gap);
    noteEditor.setBounds (right.removeFromBottom (rowHeight()));
    right.removeFromBottom (gap);

    libraryList.setBounds (right);
}

//==============================================================================
void CompanionView::timerCallback()
{
    // Ports come and go while the app runs; a slow poll keeps the frame honest
    // without a platform-specific hotplug callback.
    static int lastPortCount = -1;
    const int ports = model.link().availableInputs().size() + model.link().availableOutputs().size();

    if (ports != lastPortCount)
    {
        lastPortCount = ports;
        refreshFrameMidiState();
    }
}

void CompanionView::refreshFrameMidiState()
{
    SharedFrame::MidiState state;
    state.inputs         = model.link().availableInputs();
    state.outputs        = model.link().availableOutputs();
    state.selectedInput  = model.link().inputIdentifier();
    state.selectedOutput = model.link().outputIdentifier();
    state.connected      = model.link().isConnected();

    frame.setMidiState (state);
    refreshConnectionLabel();
}

void CompanionView::refreshConnectionLabel()
{
    const auto& t = lookAndFeel.theme();

    if (model.link().isConnected())
    {
        connectionLabel.setColour (juce::Label::textColourId, t.affirm);
        connectionLabel.setText (model.link().outputName(), juce::dontSendNotification);
    }
    else if (model.link().hasInput() || model.link().hasOutput())
    {
        connectionLabel.setColour (juce::Label::textColourId, t.caution);
        connectionLabel.setText (model.link().hasInput() ? "Input only" : "Output only", juce::dontSendNotification);
    }
    else
    {
        connectionLabel.setColour (juce::Label::textColourId, t.caution);
        connectionLabel.setText ("Not connected", juce::dontSendNotification);
    }
}

void CompanionView::refreshStatusDisplay()
{
    seedDisplay.setText (model.status().seed ? juce::String (rnd::formatSeed (*model.status().seed)) : juce::String ("--"),
                         juce::dontSendNotification);

    juce::StringArray lines;

    if (model.status().tonic && model.status().scaleIndex)
        lines.add (juce::String (rnd::tonicName (*model.status().tonic)) + " "
                   + juce::String (rnd::scaleName (*model.status().scaleIndex)));

    if (model.status().tempoBpm)
        lines.add (juce::String (*model.status().tempoBpm) + " BPM as reported");

    if (model.status().patchMode)
        lines.add ("Patch mode " + juce::String (*model.status().patchMode));

    if (! model.status().engines.empty())
    {
        juce::StringArray names;
        for (const auto& engine : model.status().engines)
            names.add (juce::String (engine.index + 1) + ": " + juce::String (engine.name));

        lines.add ("Engines " + names.joinIntoString ("  "));
    }

    if (lines.isEmpty())
        lines.add ("No status yet. Press Read device, or turn the RND's seed knob.");

    statusDisplay.setText (lines.joinIntoString ("\n"), juce::dontSendNotification);

    // Reflect what the device reports without echoing it straight back out.
    if (model.status().scaleIndex)
        scaleCombo.setSelectedId (*model.status().scaleIndex + 1, juce::dontSendNotification);

    if (model.status().tonic)
        tonicCombo.setSelectedId (*model.status().tonic + 1, juce::dontSendNotification);
}

void CompanionView::refreshLibrary()
{
    const auto previous = selectedSeed();

    visibleEntries = model.library().filtered (showUnrated.getToggleState(),
                                       showKeep.getToggleState(),
                                       showPass.getToggleState());
    libraryList.updateContent();
    frame.setLibraryState (libraryShown, static_cast<int> (visibleEntries.size()));

    if (previous)
    {
        for (int i = 0; i < static_cast<int> (visibleEntries.size()); ++i)
            if (visibleEntries[static_cast<std::size_t> (i)].seed == *previous)
            {
                libraryList.selectRow (i, false, true);
                break;
            }
    }

    libraryList.repaint();
}

//==============================================================================
void CompanionView::darkModeSettingChanged()
{
    if (model.themeMode() == CompanionModel::ThemeMode::automatic)
        applyTheme();
}

void CompanionView::applyTheme()
{
    const bool dark = model.themeMode() == CompanionModel::ThemeMode::dark
                   || (model.themeMode() == CompanionModel::ThemeMode::automatic
                       && juce::Desktop::getInstance().isDarkModeActive());

    lookAndFeel.setTheme (dark ? suite::Theme::dark() : suite::Theme::light());

    const auto& t = lookAndFeel.theme();

    for (auto* heading : { &deviceHeading, &liveHeading, &libraryHeading })
    {
        heading->setFont (suite::SuiteLookAndFeel::eyebrowFont());
        heading->setColour (juce::Label::textColourId, t.fgMuted);
    }

    seedDisplay.setColour (juce::Label::textColourId, t.fg);
    statusDisplay.setColour (juce::Label::textColourId, t.fg2);
    readCaveat.setColour (juce::Label::textColourId, t.fgMuted);
    lockWarning.setColour (juce::Label::textColourId, t.caution);

    for (auto* label : { &scaleLabel, &tonicLabel, &volumeLabel, &reverbLabel, &transportLabel })
        label->setColour (juce::Label::textColourId, t.fgMuted);

    logView.setColour (juce::TextEditor::backgroundColourId, t.bgSunken);
    logView.setColour (juce::TextEditor::textColourId, t.fgMuted);

    // Placeholder colours are baked in at the call, so they have to be re-set
    // per theme -- otherwise light's pale ink stays put and glares on dark.
    // fg-muted rather than fg-faint: DESIGN.md reserves faint for disabled
    // controls, and a hint is still something you are meant to read.
    seedEditor.setTextToShowWhenEmpty ("0x00000000 or a decimal number", t.fgMuted);
    noteEditor.setTextToShowWhenEmpty ("Note on the selected seed", t.fgMuted);

    // Components cache colours out of the LookAndFeel when it is *assigned*, so
    // mutating the same object leaves them on the old palette -- which is how
    // the slider value boxes ended up drawing near-invisible text. Push the
    // change through the tree explicitly.
    sendLookAndFeelChange();

    for (auto* caveat : { &readCaveat, &mixCaveat })
        caveat->setColour (juce::Label::textColourId, t.fgMuted);


    // Send-only controls start dimmed: 100 and 40 are our defaults, not a
    // reading of the hardware, and the panel should not imply otherwise.
    const float mixAlpha = mixTouched ? 1.0f : 0.55f;
    volumeSlider.setAlpha (mixAlpha);
    reverbSlider.setAlpha (mixAlpha);

    refreshConnectionLabel();
    repaint();
}

void CompanionView::markMixTouched()
{
    if (mixTouched)
        return;

    mixTouched = true;
    volumeSlider.setAlpha (1.0f);
    reverbSlider.setAlpha (1.0f);
}

void CompanionView::refreshFromModel()
{
    if (const auto seed = model.status().seed)
        seedEditor.setText (juce::String (rnd::formatSeed (*seed)), juce::dontSendNotification);

    refreshStatusDisplay();
}

void CompanionView::appendLog (const juce::String& text)
{
    logView.moveCaretToEnd();
    logView.insertTextAtCaret (juce::Time::getCurrentTime().toString (false, true, true, true)
                               + "  " + text + "\n");
    logView.moveCaretToEnd();
}

//==============================================================================
void CompanionView::sendSeedFromEditor()
{
    const auto parsed = rnd::parseSeed (seedEditor.getText().toStdString());

    if (! parsed)
    {
        appendLog ("Not a seed: " + seedEditor.getText());
        return;
    }

    sendSeed (*parsed, "typed");
}

void CompanionView::sendRandomSeed()
{
    const auto seed = static_cast<std::uint32_t> (random.nextInt64());
    seedEditor.setText (juce::String (rnd::formatSeed (seed)), juce::dontSendNotification);
    sendSeed (seed, "random");
}

void CompanionView::sendSeed (std::uint32_t seed, const juce::String& reason)
{
    juce::ignoreUnused (reason);
    model.sendSeed (seed);
}

void CompanionView::captureCurrentSeed()
{
    if (! model.status().seed)
    {
        appendLog ("Nothing to capture yet.");
        return;
    }

    model.library().captureSeed (*model.status().seed, model.status());
    appendLog ("Captured " + juce::String (rnd::formatSeed (*model.status().seed)));
}

//==============================================================================
std::optional<std::uint32_t> CompanionView::selectedSeed() const
{
    const int row = libraryList.getSelectedRow();

    if (row < 0 || row >= static_cast<int> (visibleEntries.size()))
        return std::nullopt;

    return visibleEntries[static_cast<std::size_t> (row)].seed;
}

void CompanionView::rateSelected (SeedEntry::Rating rating)
{
    if (const auto seed = selectedSeed())
        model.library().setRating (*seed, rating);
}

void CompanionView::removeSelected()
{
    const auto seed = selectedSeed();
    if (! seed)
        return;

    const auto removed = model.library().remove (*seed);
    if (! removed)
        return;

    undoToast.show ("Removed " + removed->displayName(),
                    [this, entry = *removed]
                    {
                        model.library().reinsert (entry);
                        appendLog ("Restored " + entry.displayName());
                    });

    appendLog ("Removed " + removed->displayName() + " -- undo available for a few seconds");
}

void CompanionView::sendSelected()
{
    if (const auto seed = selectedSeed())
        sendSeed (*seed, "library");
}

//==============================================================================
int CompanionView::getNumRows()
{
    return static_cast<int> (visibleEntries.size());
}

void CompanionView::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= static_cast<int> (visibleEntries.size()))
        return;

    const auto& entry = visibleEntries[static_cast<std::size_t> (row)];

    const auto& t = lookAndFeel.theme();

    if (selected)
        g.fillAll (t.accent.withAlpha (0.18f));

    // Rating is a colour AND a letter: no colour-only encoding (DESIGN.md).
    juce::Colour accent = t.fgFaint;
    juce::String mark = "-";
    if (entry.rating == SeedEntry::Rating::keep) { accent = t.affirm; mark = "K"; }
    if (entry.rating == SeedEntry::Rating::pass) { accent = t.danger; mark = "P"; }

    g.setColour (accent);
    g.fillRect (0, 0, 3, height);
    g.setFont (suite::SuiteLookAndFeel::eyebrowFont());
    g.drawText (mark, width - 20, 0, 16, height, juce::Justification::centredRight);

    g.setColour (t.fg);
    g.setFont (suite::SuiteLookAndFeel::monoFont (static_cast<float> (suite::metrics::textSm)));
    g.drawText (entry.displayName(), 10, 2, width - 34, height / 2, juce::Justification::centredLeft);

    g.setColour (t.fgMuted);
    g.setFont (suite::SuiteLookAndFeel::sansFont (static_cast<float> (suite::metrics::textXs)));

    juce::String detail = entry.summary();
    if (entry.note.isNotEmpty())
        detail += "  -  " + entry.note;

    g.drawText (detail, 10, height / 2, width - 34, height / 2 - 2, juce::Justification::centredLeft);
}

juce::String CompanionView::getNameForRow (int row)
{
    if (row < 0 || row >= static_cast<int> (visibleEntries.size()))
        return {};

    const auto& entry = visibleEntries[static_cast<std::size_t> (row)];

    juce::String rating = "unrated";
    if (entry.rating == SeedEntry::Rating::keep) rating = "kept";
    if (entry.rating == SeedEntry::Rating::pass) rating = "passed";

    // Spoken, not shown: colour and the K/P letter are the visual channels.
    return entry.displayName() + ", " + rating + ", " + entry.summary()
         + (entry.note.isNotEmpty() ? ", note: " + entry.note : juce::String());
}

void CompanionView::selectedRowsChanged (int)
{
    if (const auto seed = selectedSeed())
    {
        if (const auto* entry = model.library().find (*seed))
            noteEditor.setText (entry->note, juce::dontSendNotification);
    }
    else
    {
        noteEditor.clear();
    }
}

void CompanionView::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < static_cast<int> (visibleEntries.size()))
        sendSeed (visibleEntries[static_cast<std::size_t> (row)].seed, "library");
}
