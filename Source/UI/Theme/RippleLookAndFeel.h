#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

/*
    RIPPLES — LookAndFeel and the shared drawing primitives.

    Two things live here:

      * ripples::ui — small, allocation-free painting helpers shared by the
        custom components and by the LookAndFeel, so a stock juce widget and a
        RIPPLES widget are drawn by the same code.

      * RippleLookAndFeel — themes everything the interface relies on but does
        not custom-draw: popup menus, combo boxes, scrollbars, tooltips, text
        editors, labels, buttons and sliders.

    Nothing in here allocates an Image or rebuilds a large Path per frame.
    Shadows are layered strokes and gradients, so they stay crisp at 100%,
    125%, 150% and 200% scaling.
*/

namespace ripples::ui
{

//==============================================================================
/** Everything drawKnob() needs. Kept as a plain struct so both RippleKnob and
    LookAndFeel::drawRotarySlider can fill it in cheaply. */
struct KnobStyle
{
    juce::Colour accent;              // value arc / glow colour
    float value01    = 0.0f;          // 0..1 position along the rotary sweep
    float hover      = 0.0f;          // 0..1 animated hover amount
    float modAmount  = 0.0f;          // -1..1, secondary modulation ring
    float detail     = 1.0f;          // 0..1, ornament budget (small knobs drop layers)
    bool  bipolar    = false;         // arc grows from the centre rather than the start
    bool  enabled    = true;
    bool  focused    = false;         // keyboard focus indication
};

/** Draws the complete knob: shadow, shaded body, ring, track, value arc,
    modulation ring and indicator. `area` is the square the knob lives in. */
void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, const KnobStyle& style);

/** Soft outer shadow for a rounded rectangle — layered strokes, no blur image. */
void drawSoftShadow (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                     juce::Colour colour, float spread, float yOffset);

/** Soft circular shadow — a single radial gradient, no blur image. */
void drawSoftCircleShadow (juce::Graphics& g, juce::Point<float> centre, float radius,
                           juce::Colour colour, float spread, float yOffset);

/** The liquid inside a glass vessel: water column, edge refraction, caustic
    floor, specular streaks and the meniscus. Draw this BEFORE the contents. */
void drawGlassSurface (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                       juce::Colour accent, bool withHighlight);

/** The glass wall: a lit outer rim plus the refracted inner edge behind it.
    Call AFTER a panel's contents so the wall sits in front of the liquid — that
    separation is what gives the panel thickness. */
void drawGlassRim (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                   juce::Colour accent);

/** Flat dark well used by selectors, buttons and toggles. */
void drawControlWell (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                      juce::Colour border, bool hovered, bool down, bool enabled);

/** Track-and-thumb switch. `onAmount` is the animated 0..1 thumb position; the
    thumb changes shape as well as colour and position, so state never depends
    on colour alone. */
void drawSwitch (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent,
                 float onAmount, bool hovered, bool down, bool enabled, bool focused);

/** A crisp downward chevron centred in `area`. */
void drawChevron (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float thickness);

/** Keyboard focus indication — never colour alone, it is an extra outline. */
void drawFocusRing (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius);

/** Dims a colour for a disabled control without making it invisible. */
juce::Colour dimForState (juce::Colour c, bool enabled);

} // namespace ripples::ui

namespace ripples
{

//==============================================================================
/** The plug-in wide LookAndFeel. Set it once on the editor; every child
    inherits it. The custom components additionally attach it themselves so
    they also look correct in isolation (unit tests, previews). */
class RippleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RippleLookAndFeel();
    ~RippleLookAndFeel() override;

    //==========================================================================
    // Sliders
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    int getSliderThumbRadius (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    //==========================================================================
    // Buttons
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    //==========================================================================
    // Combo boxes
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    //==========================================================================
    // Popup menus
    void drawPopupMenuBackgroundWithOptions (juce::Graphics&, int width, int height,
                                             const juce::PopupMenu::Options&) override;

    void drawPopupMenuItemWithOptions (juce::Graphics&, const juce::Rectangle<int>& area,
                                       bool isHighlighted, const juce::PopupMenu::Item&,
                                       const juce::PopupMenu::Options&) override;

    void getIdealPopupMenuItemSizeWithOptions (const juce::String& text, bool isSeparator,
                                               int standardMenuItemHeight, int& idealWidth,
                                               int& idealHeight, const juce::PopupMenu::Options&) override;

    void drawPopupMenuSectionHeaderWithOptions (juce::Graphics&, const juce::Rectangle<int>& area,
                                                const juce::String& sectionName,
                                                const juce::PopupMenu::Options&) override;

    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override;

    //==========================================================================
    // Labels, editors, scrollbars, tooltips
    juce::Font getLabelFont (juce::Label&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    int getDefaultScrollbarWidth() override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

private:
    void applyColourScheme();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RippleLookAndFeel)
};

} // namespace ripples
