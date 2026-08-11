#include "CompanionEditor.h"

CompanionEditor::CompanionEditor (CompanionProcessor& p)
    : AudioProcessorEditor (&p), view (p.model())
{
    addAndMakeVisible (view);

    // The minimum has to be genuinely small. A host is under no obligation to
    // honour our preferred size, and AUM in particular presents the view at
    // whatever the pane happens to be -- anything we lay out beyond that width
    // is simply invisible, which is how the transport selector came to look
    // broken. The view stacks its columns below 860px.
    setResizable (true, true);
    setResizeLimits (420, 460, 4000, 3000);
    setSize (1040, 720);
}

void CompanionEditor::resized()
{
    view.setBounds (getLocalBounds());
}
