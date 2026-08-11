#include "ProbeEditor.h"

namespace
{
    juce::String describeFormat()
    {
        return juce::AudioProcessor::getWrapperTypeDescription (juce::PluginHostType::getPluginLoadedAs());
    }

    juce::String describeHost()
    {
        const juce::PluginHostType host;
        const juce::String described = host.getHostDescription();
        return described.isNotEmpty() ? described : juce::String ("unknown host");
    }

    juce::String hexOf (const std::vector<std::uint8_t>& bytes)
    {
        juce::String out;
        for (auto byte : bytes)
            out << juce::String::toHexString (byte).paddedLeft ('0', 2).toUpperCase() << " ";
        return out.trim();
    }
}

ProbeEditor::ProbeEditor (ProbeProcessor& p)
    : AudioProcessorEditor (&p), probe (p)
{
    identity.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
    identity.setText (describeFormat() + "  in  " + describeHost(), juce::dontSendNotification);
    addAndMakeVisible (identity);

    instructions.setFont (juce::Font (juce::FontOptions (11.5f)));
    instructions.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    instructions.setJustificationType (juce::Justification::topLeft);
    instructions.setText ("OUT: route this plugin's MIDI output to a port RND Companion is listening on, "
                          "then press Send test burst. Four seeds should appear there.\n"
                          "IN: point RND Companion's MIDI output at this track and send a seed. "
                          "It should appear below.",
                          juce::dontSendNotification);
    addAndMakeVisible (instructions);

    sendButton.onClick = [this]
    {
        probe.requestSend();
        appendLine ("Queued 4 frames for the next audio block.");
    };
    addAndMakeVisible (sendButton);

    resetButton.onClick = [this]
    {
        probe.resetCounters();
        loggedLines = 0;
        logView.clear();
    };
    addAndMakeVisible (resetButton);

    copyButton.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (
            describeFormat() + " in " + describeHost() + "\n"
            + outCounters.getText() + "\n" + inCounters.getText() + "\n"
            + verdict.getText() + "\n\n" + logView.getText());
    };
    addAndMakeVisible (copyButton);

    logEverything.setTooltip ("The RND broadcasts its status constantly; off by default so the log stays readable.");
    addAndMakeVisible (logEverything);

    for (auto* label : { &outCounters, &inCounters })
    {
        label->setFont (juce::Font (juce::FontOptions (13.0f)));
        addAndMakeVisible (*label);
    }

    verdict.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
    addAndMakeVisible (verdict);

    logView.setMultiLine (true);
    logView.setReadOnly (true);
    logView.setScrollbarsShown (true);
    logView.setCaretVisible (false);
    logView.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
    logView.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141417));
    addAndMakeVisible (logView);

    setSize (560, 420);
    startTimerHz (10);
}

ProbeEditor::~ProbeEditor()
{
    stopTimer();
}

void ProbeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff232328));
}

void ProbeEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    identity.setBounds (area.removeFromTop (24));
    instructions.setBounds (area.removeFromTop (52));
    area.removeFromTop (6);

    auto buttons = area.removeFromTop (26);
    sendButton.setBounds (buttons.removeFromLeft (130));
    buttons.removeFromLeft (8);
    resetButton.setBounds (buttons.removeFromLeft (70));
    buttons.removeFromLeft (8);
    copyButton.setBounds (buttons.removeFromLeft (100));
    buttons.removeFromLeft (12);
    logEverything.setBounds (buttons.removeFromLeft (130));

    area.removeFromTop (8);
    outCounters.setBounds (area.removeFromTop (20));
    inCounters.setBounds (area.removeFromTop (20));
    verdict.setBounds (area.removeFromTop (22));
    area.removeFromTop (6);

    logView.setBounds (area);
}

void ProbeEditor::timerCallback()
{
    const int sent = probe.framesSent();
    const int received = probe.framesReceived();
    const int ours = probe.testSeedsReceived();
    const int damaged = probe.damagedReceived();
    const int foreign = probe.foreignReceived();

    outCounters.setText ("Sent " + juce::String (sent) + " test frames", juce::dontSendNotification);
    inCounters.setText ("Received " + juce::String (received) + " frames: "
                        + juce::String (ours) + " of ours, "
                        + juce::String (foreign) + " other SysEx, "
                        + juce::String (damaged) + " damaged",
                        juce::dontSendNotification);

    // Order matters. Damage outranks everything -- a host that rewrites our
    // frames is disqualified no matter what else it delivers. Foreign SysEx is
    // genuine positive evidence and must never be reported as a failure.
    if (! probe.isBeingProcessed())
    {
        verdict.setColour (juce::Label::textColourId, juce::Colours::orange);
        verdict.setText ("Host is not calling processBlock -- start the transport or arm the track.",
                         juce::dontSendNotification);
    }
    else if (damaged > 0)
    {
        verdict.setColour (juce::Label::textColourId, juce::Colours::orangered);
        verdict.setText ("This host DAMAGES RND SysEx: " + juce::String (damaged) + " of our frames arrived broken.",
                         juce::dontSendNotification);
    }
    else if (ours > 0)
    {
        verdict.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
        verdict.setText ("SysEx IN works: " + juce::String (ours) + " test frames arrived intact.",
                         juce::dontSendNotification);
    }
    else if (foreign > 0)
    {
        verdict.setColour (juce::Label::textColourId, juce::Colours::yellowgreen);
        verdict.setText ("SysEx reaches this plugin (" + juce::String (foreign)
                         + " other frames, intact). Send an RND frame in to confirm.",
                         juce::dontSendNotification);
    }
    else if (received > 0)
    {
        verdict.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        verdict.setText ("Receiving traffic, but no SysEx yet.", juce::dontSendNotification);
    }
    else
    {
        verdict.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        verdict.setText ("No SysEx has reached this plugin yet.", juce::dontSendNotification);
    }

    for (const auto& frame : probe.takeReceived())
    {
        // An RND on the same port broadcasts its status continuously, so
        // logging everything buries the two lines that matter. Test seeds and
        // parse failures always show; the rest only on request.
        // Foreign frames are rare and are evidence, so they always show.
        const bool interesting = frame.matchesATestSeed
                              || frame.kind == ProbeProcessor::Kind::damaged
                              || frame.kind == ProbeProcessor::Kind::foreign;

        if (! interesting && ! logEverything.getToggleState())
            continue;

        if (++loggedLines > 500)
        {
            logView.clear();
            loggedLines = 0;
            appendLine ("(log trimmed)");
        }

        juce::String line;
        line << "IN  " << hexOf (frame.bytes) << "  -> " << ProbeProcessor::describe (frame.kind);

        if (frame.kind == ProbeProcessor::Kind::seed)
        {
            line << " " << juce::String (rnd::formatSeed (frame.seed));
            if (frame.matchesATestSeed)
                line << "  <-- one of ours, intact";
        }
        else if (frame.kind == ProbeProcessor::Kind::foreign && ! frame.foreignLabel.empty())
        {
            line << ": " << juce::String (frame.foreignLabel);
        }

        appendLine (line);
    }
}

void ProbeEditor::appendLine (const juce::String& text)
{
    logView.moveCaretToEnd();
    logView.insertTextAtCaret (text + "\n");
    logView.moveCaretToEnd();
}
