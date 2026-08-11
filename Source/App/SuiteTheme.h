#pragma once

// The suite's "paper & ink" design language, in JUCE.
//
// Values come straight from @enkerli/ui tokens.css in the music-suite monorepo
// (packages/ui/tokens/tokens.css, DESIGN.md). They are not re-picked here: if a
// token changes there, change it here and nowhere else.
//
// Two rules from DESIGN.md that shape this file:
//   * Light is the default design target; dark is a first-class variant, never
//     an afterthought. The OS preference applies until the person chooses.
//   * Controls are 32px with a pointer and 44px on touch, so the same layout is
//     usable on a desktop and under a fingertip.

#include <juce_gui_basics/juce_gui_basics.h>

namespace suite
{

struct Theme
{
    juce::Colour bg, bgRaised, bgSunken;
    juce::Colour fg, fg2, fgMuted, fgFaint;
    juce::Colour border, borderSoft, borderStrong;
    juce::Colour accent, accentFg, danger;

    // ── Local extensions, pending promotion ─────────────────────────────────
    // These two have no counterpart in tokens.css, which exposes only
    // --es-accent and --es-danger. They are derived from documented dimension
    // colours rather than invented, but a WASM or web build of this UI would
    // have no matching token, so the two surfaces could drift.
    //
    // Raised by the 2026-08-11 design audit (F3) as worth promoting to
    // first-class suite tokens (both themes, >=3:1). That is a design-system
    // decision for the monorepo, not one to make from inside a single plugin --
    // so it stays flagged here until it is taken.

    /// Caution, not destruction: the "pressure" dimension colour. Warnings that
    /// are about hardware state rather than about losing data.
    juce::Colour caution;

    /// Affirmative state -- connected, kept. The "expression" dimension colour.
    juce::Colour affirm;

    static Theme light();
    static Theme dark();
};

//==============================================================================
/// Shape and metric tokens.
namespace metrics
{
    inline constexpr int radiusLg = 20;
    inline constexpr int radiusMd = 14;
    inline constexpr int radiusSm = 10;

    inline constexpr int textXs = 11;
    inline constexpr int textSm = 13;
    inline constexpr int textMd = 16;
    inline constexpr int textLg = 20;

    inline constexpr int sectionHeader = 28;

    /// 32px with a pointer, 44px on touch. Touch wins on iOS, where every
    /// target is a fingertip. `dense` is the .es-dense equivalent: density is a
    /// setting, not a redesign (DESIGN.md), so it moves this one number.
    int controlHeight (bool dense = false);
}

//==============================================================================
/// Button roles, from components.css. Exactly one primary per group -- that is
/// what gives a row of actions a shape instead of being, as the design pass put
/// it, identical outlined boxes with no hierarchy.
enum class ButtonRole
{
    normal,    ///< .es-btn
    primary,   ///< .es-primary -- the one obvious action in its group
    danger     ///< destructive; keeps the --es-danger edge and sits apart
};

void setButtonRole (juce::Button&, ButtonRole);

/// Non-ASCII glyphs go through fromUTF8, always. A bare UTF-8 literal reaching
/// a juce::String trips an assertion on any byte > 127 -- see the project's
/// CLAUDE.md, which records this biting twice.
namespace glyph
{
    juce::String middot();   ///< U+00B7, the suite's chip separator
    juce::String sun();      ///< U+2600, "switch to light"
    juce::String moon();     ///< U+25CF, "switch to dark"
    juce::String density();  ///< U+2261
}

class SuiteLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SuiteLookAndFeel();

    void setTheme (const Theme&);
    const Theme& theme() const noexcept { return current; }

    /// Small-caps section label. JUCE has no small-caps, so this is the honest
    /// approximation: uppercase, bold, muted, one size down.
    static juce::Font eyebrowFont();
    static juce::Font monoFont (float height, bool bold = false);
    static juce::Font sansFont (float height, bool bold = false);

    //==============================================================================
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h,
                      bool ticked, bool enabled, bool highlighted, bool down) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&,
                                   const juce::Path&, juce::Image&) override;

private:
    void applyColours();

    Theme current { Theme::light() };
};

}  // namespace suite
