#include "SuiteTheme.h"

namespace suite
{

Theme Theme::light()
{
    Theme t;
    t.bg           = juce::Colour (0xfff5f2eb);  // paper
    t.bgRaised     = juce::Colour (0xfffcfbf7);  // panel
    t.bgSunken     = juce::Colour (0xffefebe2);  // field / well
    t.fg           = juce::Colour (0xff2d2b27);  // ink
    t.fg2          = juce::Colour (0xff4b463e);
    t.fgMuted      = juce::Colour (0xff6b665b);
    t.fgFaint      = juce::Colour (0xffb3ac9e);  // disabled only
    t.border       = juce::Colour (0xffddd6ca);
    t.borderSoft   = juce::Colour (0xffeae3d4);
    t.borderStrong = juce::Colour (0xff9d8967);
    t.accent       = juce::Colour (0xff2f66a5);
    t.accentFg     = juce::Colour (0xffffffff);
    t.danger       = juce::Colour (0xffb3403a);
    t.caution      = juce::Colour (0xffd28330);  // dim-pressure
    t.affirm       = juce::Colour (0xff2d9d8a);  // dim-expr
    return t;
}

Theme Theme::dark()
{
    Theme t;
    t.bg           = juce::Colour (0xff1a1814);
    t.bgRaised     = juce::Colour (0xff221f1a);
    t.bgSunken     = juce::Colour (0xff14130f);
    t.fg           = juce::Colour (0xffe8e1d2);
    t.fg2          = juce::Colour (0xffcfc7b5);
    t.fgMuted      = juce::Colour (0xff908672);
    t.fgFaint      = juce::Colour (0xff5f584a);
    t.border       = juce::Colour (0xff38332b);
    t.borderSoft   = juce::Colour (0xff2b2620);
    t.borderStrong = juce::Colour (0xff736958);
    t.accent       = juce::Colour (0xff6da3df);
    t.accentFg     = juce::Colour (0xff14130f);
    t.danger       = juce::Colour (0xffcf6a5e);
    t.caution      = juce::Colour (0xffd28330);
    t.affirm       = juce::Colour (0xff40b8a2);  // lifted to clear the near-black
    return t;
}

namespace metrics
{
    int controlHeight()
    {
       #if JUCE_IOS || JUCE_ANDROID
        return 44;   // touch target
       #else
        return 32;
       #endif
    }
}

//==============================================================================
SuiteLookAndFeel::SuiteLookAndFeel()
{
    applyColours();
}

void SuiteLookAndFeel::setTheme (const Theme& t)
{
    current = t;
    applyColours();
}

juce::Font SuiteLookAndFeel::sansFont (float height, bool bold)
{
    // The token stack is "Inter Tight, Inter, -apple-system, …" — we do not
    // embed the webfonts, so this resolves to the platform's system sans, which
    // is the next entry in that same stack.
    auto options = juce::FontOptions (height);
    return bold ? juce::Font (options).boldened() : juce::Font (options);
}

juce::Font SuiteLookAndFeel::monoFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font SuiteLookAndFeel::eyebrowFont()
{
    return sansFont (static_cast<float> (metrics::textXs), true);
}

void SuiteLookAndFeel::applyColours()
{
    setColour (juce::ResizableWindow::backgroundColourId, current.bg);
    setColour (juce::DocumentWindow::textColourId,        current.fg);

    setColour (juce::Label::textColourId,                 current.fg);
    setColour (juce::Label::backgroundColourId,           juce::Colours::transparentBlack);

    setColour (juce::TextButton::buttonColourId,          current.bgRaised);
    setColour (juce::TextButton::buttonOnColourId,        current.accent);
    setColour (juce::TextButton::textColourOffId,         current.fg);
    setColour (juce::TextButton::textColourOnId,          current.accentFg);

    setColour (juce::ComboBox::backgroundColourId,        current.bgRaised);
    setColour (juce::ComboBox::textColourId,              current.fg);
    setColour (juce::ComboBox::outlineColourId,           current.borderStrong);
    setColour (juce::ComboBox::arrowColourId,             current.fg2);
    setColour (juce::ComboBox::focusedOutlineColourId,    current.accent);

    setColour (juce::PopupMenu::backgroundColourId,       current.bgRaised);
    setColour (juce::PopupMenu::textColourId,             current.fg);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, current.accent);
    setColour (juce::PopupMenu::highlightedTextColourId,  current.accentFg);

    setColour (juce::TextEditor::backgroundColourId,      current.bgSunken);
    setColour (juce::TextEditor::textColourId,            current.fg);
    setColour (juce::TextEditor::outlineColourId,         current.borderStrong);
    setColour (juce::TextEditor::focusedOutlineColourId,  current.accent);
    setColour (juce::TextEditor::highlightColourId,       current.accent.withAlpha (0.3f));
    setColour (juce::TextEditor::shadowColourId,          juce::Colours::transparentBlack);

    setColour (juce::ListBox::backgroundColourId,         current.bgSunken);
    setColour (juce::ListBox::textColourId,               current.fg);

    setColour (juce::ScrollBar::thumbColourId,            current.borderStrong);

    setColour (juce::Slider::backgroundColourId,          current.bgSunken);
    setColour (juce::Slider::trackColourId,               current.accent);
    setColour (juce::Slider::thumbColourId,               current.accent);
    setColour (juce::Slider::textBoxTextColourId,         current.fg);
    setColour (juce::Slider::textBoxBackgroundColourId,   current.bgSunken);
    setColour (juce::Slider::textBoxOutlineColourId,      current.border);

    setColour (juce::ToggleButton::textColourId,          current.fg);
    setColour (juce::ToggleButton::tickColourId,          current.accent);
    setColour (juce::ToggleButton::tickDisabledColourId,  current.borderStrong);

    setColour (juce::TooltipWindow::backgroundColourId,   current.fg);
    setColour (juce::TooltipWindow::textColourId,         current.bg);
}

//==============================================================================
void SuiteLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = static_cast<float> (metrics::radiusSm);

    auto fill = backgroundColour;
    if (down)             fill = fill.overlaidWith (current.fg.withAlpha (0.12f));
    else if (highlighted) fill = fill.overlaidWith (current.fg.withAlpha (0.06f));

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, radius);

    // border-strong: this boundary identifies a control, so it is the ≥3:1 one.
    g.setColour (button.hasKeyboardFocus (false) ? current.accent : current.borderStrong);
    g.drawRoundedRectangle (bounds, radius, button.hasKeyboardFocus (false) ? 2.0f : 1.0f);
}

void SuiteLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    const float radius = static_cast<float> (metrics::radiusSm);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, radius);

    const bool focused = box.hasKeyboardFocus (false);
    g.setColour (focused ? current.accent : current.borderStrong);
    g.drawRoundedRectangle (bounds, radius, focused ? 2.0f : 1.0f);

    // Chevron rather than a filled triangle: quieter, and it reads at 11px.
    const float cx = static_cast<float> (width) - 14.0f;
    const float cy = static_cast<float> (height) * 0.5f;

    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void SuiteLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                 juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<int> (0, 0, width, height).toFloat(),
                            static_cast<float> (metrics::radiusSm));
}

void SuiteLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                              juce::TextEditor& editor)
{
    if (! editor.isEnabled())
        return;

    const bool focused = editor.hasKeyboardFocus (false);
    g.setColour (focused ? current.accent : current.borderStrong);
    g.drawRoundedRectangle (juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f),
                            static_cast<float> (metrics::radiusSm),
                            focused ? 2.0f : 1.0f);
}

void SuiteLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float,
                                         juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float trackHeight = 6.0f;
    const float cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;

    juce::Rectangle<float> track (static_cast<float> (x), cy - trackHeight * 0.5f,
                                  static_cast<float> (width), trackHeight);

    g.setColour (current.bgSunken);
    g.fillRoundedRectangle (track, trackHeight * 0.5f);
    g.setColour (current.border);
    g.drawRoundedRectangle (track.reduced (0.5f), trackHeight * 0.5f, 1.0f);

    juce::Rectangle<float> filled (track.getX(), track.getY(), sliderPos - track.getX(), trackHeight);
    g.setColour (current.accent);
    g.fillRoundedRectangle (filled, trackHeight * 0.5f);

    // A thumb big enough to hit with a finger, per the touch-target rule.
    const float thumbRadius = metrics::controlHeight() >= 44 ? 11.0f : 8.0f;
    g.setColour (current.bgRaised);
    g.fillEllipse (sliderPos - thumbRadius, cy - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
    g.setColour (slider.hasKeyboardFocus (false) ? current.accent : current.borderStrong);
    g.drawEllipse (sliderPos - thumbRadius, cy - thumbRadius,
                   thumbRadius * 2.0f, thumbRadius * 2.0f,
                   slider.hasKeyboardFocus (false) ? 2.0f : 1.5f);
}

void SuiteLookAndFeel::drawTickBox (juce::Graphics& g, juce::Component& component,
                                    float x, float y, float w, float h,
                                    bool ticked, bool enabled, bool, bool)
{
    juce::Rectangle<float> box (x, y, w, h);

    g.setColour (ticked ? current.accent : current.bgRaised);
    g.fillRoundedRectangle (box, 5.0f);

    g.setColour (component.hasKeyboardFocus (false) ? current.accent
                                                    : (enabled ? current.borderStrong : current.fgFaint));
    g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, 1.0f);

    if (! ticked)
        return;

    juce::Path tick;
    tick.startNewSubPath (x + w * 0.26f, y + h * 0.52f);
    tick.lineTo (x + w * 0.44f, y + h * 0.70f);
    tick.lineTo (x + w * 0.76f, y + h * 0.30f);

    g.setColour (current.accentFg);
    g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

}  // namespace suite
