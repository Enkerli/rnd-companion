#include "CompanionEditor.h"

CompanionEditor::CompanionEditor (CompanionProcessor& p)
    : AudioProcessorEditor (&p), view (p.model())
{
    addAndMakeVisible (view);

    // Wide enough for the two columns, and resizable because AUM presents
    // plugin UIs at whatever size the user has dragged the pane to.
    setResizable (true, true);
    setResizeLimits (720, 520, 4000, 3000);
    setSize (1040, 720);
}

void CompanionEditor::resized()
{
    view.setBounds (getLocalBounds());
}
