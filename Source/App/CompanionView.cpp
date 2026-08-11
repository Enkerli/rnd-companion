#include "CompanionView.h"

namespace
{
    constexpr int rowHeight = 24;
    constexpr int gap = 8;

    void styleHeading (juce::Label& label)
    {
        label.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
        label.setColour (juce::Label::textColourId, juce::Colours::white);
    }
}

CompanionView::CompanionView (CompanionModel& m)
    : model (m)
{
    // ── Ports ───────────────────────────────────────────────────────────────
    styleHeading (portsHeading);
    addAndMakeVisible (portsHeading);

    inputCombo.setTextWhenNoChoicesAvailable ("No MIDI inputs");
    inputCombo.onChange = [this]
    {
        const auto identifier = inputCombo.getSelectedId() > 0
                              ? model.link().availableInputs()[inputCombo.getSelectedId() - 1].identifier
                              : juce::String();
        if (identifier.isNotEmpty())
            model.link().openInput (identifier);
    };
    addAndMakeVisible (inputCombo);

    outputCombo.setTextWhenNoChoicesAvailable ("No MIDI outputs");
    outputCombo.onChange = [this]
    {
        const auto identifier = outputCombo.getSelectedId() > 0
                              ? model.link().availableOutputs()[outputCombo.getSelectedId() - 1].identifier
                              : juce::String();
        if (identifier.isNotEmpty())
            model.link().openOutput (identifier);
    };
    addAndMakeVisible (outputCombo);

    rescanButton.onClick = [this] { refreshPortLists(); };
    addAndMakeVisible (rescanButton);

    autoConnectButton.onClick = [this]
    {
        model.link().connectToRnd();
        refreshPortLists();
    };
    addAndMakeVisible (autoConnectButton);

    connectionLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible (connectionLabel);

    transportLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (transportLabel);

    transportCombo.addItem (CompanionModel::transportName (CompanionModel::Transport::direct), 1);
    transportCombo.addItem (CompanionModel::transportName (CompanionModel::Transport::host), 2);
    transportCombo.addItem (CompanionModel::transportName (CompanionModel::Transport::both), 3);
    transportCombo.setSelectedId (1, juce::dontSendNotification);
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
    addAndMakeVisible (transportCombo);

    // ── Device ──────────────────────────────────────────────────────────────
    styleHeading (deviceHeading);
    addAndMakeVisible (deviceHeading);

    seedDisplay.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 30.0f, juce::Font::bold)));
    seedDisplay.setText ("--", juce::dontSendNotification);
    addAndMakeVisible (seedDisplay);

    statusDisplay.setJustificationType (juce::Justification::topLeft);
    statusDisplay.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusDisplay);

    seedEditor.setTextToShowWhenEmpty ("0x00000000 or a decimal number", juce::Colours::grey);
    seedEditor.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain)));
    seedEditor.onReturnKey = [this] { sendSeedFromEditor(); };
    addAndMakeVisible (seedEditor);

    sendButton.onClick = [this] { sendSeedFromEditor(); };
    addAndMakeVisible (sendButton);

    randomButton.onClick = [this] { sendRandomSeed(); };
    addAndMakeVisible (randomButton);

    readButton.onClick = [this] { model.requestStatusDump(); };
    addAndMakeVisible (readButton);

    captureButton.onClick = [this] { captureCurrentSeed(); };
    addAndMakeVisible (captureButton);

    readCaveat.setFont (juce::Font (juce::FontOptions (11.0f)));
    readCaveat.setColour (juce::Label::textColourId, juce::Colours::grey);
    readCaveat.setText ("Reading mutes the device briefly. Seeds arrive on their own when you turn the knob.",
                        juce::dontSendNotification);
    addAndMakeVisible (readCaveat);

    // ── Live controls ───────────────────────────────────────────────────────
    styleHeading (liveHeading);
    addAndMakeVisible (liveHeading);

    for (auto* label : { &scaleLabel, &tonicLabel, &volumeLabel, &reverbLabel })
    {
        label->setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (*label);
    }

    for (int i = 0; i < rnd::numScales; ++i)
        scaleCombo.addItem (juce::String (rnd::scaleName (i)), i + 1);

    scaleCombo.onChange = [this]
    {
        if (scaleCombo.getSelectedId() > 0)
            model.sendScale (scaleCombo.getSelectedId() - 1);
    };
    addAndMakeVisible (scaleCombo);

    for (int i = 0; i < rnd::numTonics; ++i)
        tonicCombo.addItem (juce::String (rnd::tonicName (i)), i + 1);

    tonicCombo.onChange = [this]
    {
        if (tonicCombo.getSelectedId() > 0)
            model.sendTonic (tonicCombo.getSelectedId() - 1);
    };
    addAndMakeVisible (tonicCombo);

    volumeSlider.setRange (0.0, 127.0, 1.0);
    volumeSlider.setValue (100.0, juce::dontSendNotification);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, rowHeight - 4);
    volumeSlider.onValueChange = [this] { model.sendVolume ((int) volumeSlider.getValue(), ! volumeSlider.isMouseButtonDown()); };
    volumeSlider.onDragEnd     = [this] { appendLog ("Volume " + juce::String ((int) volumeSlider.getValue())); };
    addAndMakeVisible (volumeSlider);

    reverbSlider.setRange (0.0, 127.0, 1.0);
    reverbSlider.setValue (40.0, juce::dontSendNotification);
    reverbSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, rowHeight - 4);
    reverbSlider.onValueChange = [this] { model.sendReverb ((int) reverbSlider.getValue(), ! reverbSlider.isMouseButtonDown()); };
    reverbSlider.onDragEnd     = [this] { appendLog ("Reverb " + juce::String ((int) reverbSlider.getValue())
                                                     + " (analog mix out only, USB stems stay dry)"); };
    addAndMakeVisible (reverbSlider);

    lockWarning.setFont (juce::Font (juce::FontOptions (11.0f)));
    lockWarning.setColour (juce::Label::textColourId, juce::Colours::orange);
    lockWarning.setText ("Scale and tonic lock on the hardware and change what a seed produces. Power-cycle to clear.",
                         juce::dontSendNotification);
    addAndMakeVisible (lockWarning);

    // ── Library ─────────────────────────────────────────────────────────────
    styleHeading (libraryHeading);
    addAndMakeVisible (libraryHeading);

    for (auto* toggle : { &showUnrated, &showKeep, &showPass })
    {
        toggle->setToggleState (true, juce::dontSendNotification);
        toggle->onClick = [this] { refreshLibrary(); };
        addAndMakeVisible (*toggle);
    }
    showPass.setToggleState (false, juce::dontSendNotification);

    libraryList.setRowHeight (38);
    libraryList.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1b1b1f));
    addAndMakeVisible (libraryList);

    keepButton.onClick = [this] { rateSelected (SeedEntry::Rating::keep); };
    passButton.onClick = [this] { rateSelected (SeedEntry::Rating::pass); };
    sendSelectedButton.onClick = [this] { sendSelected(); };
    removeButton.onClick = [this] { removeSelected(); };

    for (auto* button : { &keepButton, &passButton, &sendSelectedButton, &removeButton })
        addAndMakeVisible (*button);

    noteEditor.setTextToShowWhenEmpty ("Note on the selected seed", juce::Colours::grey);
    noteEditor.onFocusLost = [this]
    {
        if (const auto seed = selectedSeed())
            model.library().setNote (*seed, noteEditor.getText());
    };
    addAndMakeVisible (noteEditor);

    exportButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export seed library",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                           .getChildFile ("rnd-seeds.json"),
                                                       "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file != juce::File())
                                      appendLog (model.library().exportTo (file) ? "Exported to " + file.getFullPathName()
                                                                         : "Export failed.");
                              });
    };
    addAndMakeVisible (exportButton);

    importButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import seed library",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       "*.json");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file != juce::File())
                                      appendLog (model.library().importFrom (file) ? "Imported " + file.getFullPathName()
                                                                           : "Import failed.");
                              });
    };
    addAndMakeVisible (importButton);

    // ── Log ─────────────────────────────────────────────────────────────────
    logView.setMultiLine (true);
    logView.setReadOnly (true);
    logView.setScrollbarsShown (true);
    logView.setCaretVisible (false);
    logView.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
    logView.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141417));
    addAndMakeVisible (logView);

    // ── Wiring ──────────────────────────────────────────────────────────────
    model.onStatusChanged = [this] { refreshFromModel(); };
    model.onLog           = [this] (const juce::String& text) { appendLog (text); };
    model.link().onConnectionChanged = [this] { refreshConnectionLabel(); };
    model.library().onChanged = [this] { refreshLibrary(); };

    refreshPortLists();
    refreshStatusDisplay();
    refreshLibrary();

    appendLog ("RND Companion ready. Connect the RND over USB, then press Find RND.");

    // Ports come and go while the app runs; a slow poll keeps the lists honest
    // without a platform-specific hotplug callback.
    startTimer (2000);

    setSize (1040, 720);
}

CompanionView::~CompanionView()
{
    model.library().flush();
}

//==============================================================================
void CompanionView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff232328));
}

void CompanionView::resized()
{
    auto area = getLocalBounds().reduced (gap * 2);

    // Log along the bottom.
    logView.setBounds (area.removeFromBottom (110));
    area.removeFromBottom (gap);

    // Ports across the top.
    auto ports = area.removeFromTop (rowHeight * 2 + gap);
    portsHeading.setBounds (ports.removeFromTop (rowHeight));

    auto portRow = ports.removeFromTop (rowHeight);
    inputCombo.setBounds (portRow.removeFromLeft (250));
    portRow.removeFromLeft (gap);
    outputCombo.setBounds (portRow.removeFromLeft (250));
    portRow.removeFromLeft (gap);
    autoConnectButton.setBounds (portRow.removeFromLeft (90));
    portRow.removeFromLeft (gap);
    rescanButton.setBounds (portRow.removeFromLeft (80));
    portRow.removeFromLeft (gap);
    transportLabel.setBounds (portRow.removeFromLeft (44));
    transportCombo.setBounds (portRow.removeFromLeft (150));
    portRow.removeFromLeft (gap);
    connectionLabel.setBounds (portRow);

    area.removeFromTop (gap * 2);

    auto left = area.removeFromLeft (area.getWidth() / 2 - gap);
    area.removeFromLeft (gap * 2);
    auto right = area;

    // ── Left: device + live ────────────────────────────────────────────────
    deviceHeading.setBounds (left.removeFromTop (rowHeight));
    seedDisplay.setBounds (left.removeFromTop (42));
    statusDisplay.setBounds (left.removeFromTop (76));
    left.removeFromTop (gap);

    auto seedRow = left.removeFromTop (rowHeight);
    seedEditor.setBounds (seedRow.removeFromLeft (200));
    seedRow.removeFromLeft (gap);
    sendButton.setBounds (seedRow.removeFromLeft (70));
    seedRow.removeFromLeft (gap);
    randomButton.setBounds (seedRow.removeFromLeft (80));

    left.removeFromTop (gap);
    auto actionRow = left.removeFromTop (rowHeight);
    readButton.setBounds (actionRow.removeFromLeft (110));
    actionRow.removeFromLeft (gap);
    captureButton.setBounds (actionRow.removeFromLeft (120));

    readCaveat.setBounds (left.removeFromTop (rowHeight));
    left.removeFromTop (gap * 2);

    liveHeading.setBounds (left.removeFromTop (rowHeight));

    auto scaleRow = left.removeFromTop (rowHeight);
    scaleLabel.setBounds (scaleRow.removeFromLeft (56));
    scaleCombo.setBounds (scaleRow.removeFromLeft (180));
    scaleRow.removeFromLeft (gap);
    tonicLabel.setBounds (scaleRow.removeFromLeft (44));
    tonicCombo.setBounds (scaleRow.removeFromLeft (70));

    left.removeFromTop (gap);
    auto volumeRow = left.removeFromTop (rowHeight);
    volumeLabel.setBounds (volumeRow.removeFromLeft (56));
    volumeSlider.setBounds (volumeRow);

    left.removeFromTop (gap / 2);
    auto reverbRow = left.removeFromTop (rowHeight);
    reverbLabel.setBounds (reverbRow.removeFromLeft (56));
    reverbSlider.setBounds (reverbRow);

    lockWarning.setBounds (left.removeFromTop (rowHeight * 2));

    // ── Right: library ─────────────────────────────────────────────────────
    libraryHeading.setBounds (right.removeFromTop (rowHeight));

    auto filterRow = right.removeFromTop (rowHeight);
    showUnrated.setBounds (filterRow.removeFromLeft (80));
    showKeep.setBounds (filterRow.removeFromLeft (80));
    showPass.setBounds (filterRow.removeFromLeft (80));
    filterRow.removeFromLeft (gap);
    exportButton.setBounds (filterRow.removeFromLeft (70));
    filterRow.removeFromLeft (gap);
    importButton.setBounds (filterRow.removeFromLeft (70));

    right.removeFromTop (gap);

    auto buttonRow = right.removeFromBottom (rowHeight);
    keepButton.setBounds (buttonRow.removeFromLeft (70));
    buttonRow.removeFromLeft (gap);
    passButton.setBounds (buttonRow.removeFromLeft (70));
    buttonRow.removeFromLeft (gap);
    sendSelectedButton.setBounds (buttonRow.removeFromLeft (70));
    buttonRow.removeFromLeft (gap);
    removeButton.setBounds (buttonRow.removeFromLeft (80));

    right.removeFromBottom (gap);
    noteEditor.setBounds (right.removeFromBottom (rowHeight));
    right.removeFromBottom (gap);

    libraryList.setBounds (right);
}

//==============================================================================
void CompanionView::timerCallback()
{
    const auto inputs = model.link().availableInputs();
    const auto outputs = model.link().availableOutputs();

    if (inputs.size() != inputCombo.getNumItems() || outputs.size() != outputCombo.getNumItems())
        refreshPortLists();
}

void CompanionView::refreshPortLists()
{
    const auto fill = [] (juce::ComboBox& combo, const juce::Array<juce::MidiDeviceInfo>& devices,
                          const juce::String& openIdentifier)
    {
        combo.clear (juce::dontSendNotification);

        for (int i = 0; i < devices.size(); ++i)
            combo.addItem (devices[i].name, i + 1);

        for (int i = 0; i < devices.size(); ++i)
            if (devices[i].identifier == openIdentifier)
                combo.setSelectedId (i + 1, juce::dontSendNotification);
    };

    fill (inputCombo, model.link().availableInputs(), model.link().inputIdentifier());
    fill (outputCombo, model.link().availableOutputs(), model.link().outputIdentifier());

    refreshConnectionLabel();
}

void CompanionView::refreshConnectionLabel()
{
    if (model.link().isConnected())
    {
        connectionLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        connectionLabel.setText (model.link().outputName(), juce::dontSendNotification);
    }
    else if (model.link().hasInput() || model.link().hasOutput())
    {
        connectionLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
        connectionLabel.setText (model.link().hasInput() ? "Input only" : "Output only", juce::dontSendNotification);
    }
    else
    {
        connectionLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
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
    if (const auto seed = selectedSeed())
        model.library().remove (*seed);
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

    if (selected)
        g.fillAll (juce::Colour (0xff35405a));

    juce::Colour accent = juce::Colours::grey;
    if (entry.rating == SeedEntry::Rating::keep) accent = juce::Colours::lightgreen;
    if (entry.rating == SeedEntry::Rating::pass) accent = juce::Colours::indianred;

    g.setColour (accent);
    g.fillRect (0, 0, 3, height);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    g.drawText (entry.displayName(), 10, 2, width - 14, height / 2, juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));

    juce::String detail = entry.summary();
    if (entry.note.isNotEmpty())
        detail += "  -  " + entry.note;

    g.drawText (detail, 10, height / 2, width - 14, height / 2 - 2, juce::Justification::centredLeft);
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
