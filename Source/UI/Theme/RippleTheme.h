#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*
    RIPPLES — Design tokens.

    Every colour, radius, spacing value and font in the interface comes from
    here. No Component may hard-code a juce::Colour literal. If a value is
    needed that this struct does not provide, add it here rather than inlining
    it locally — that is what keeps the instrument looking like one product.

    Visual balance target: 70% clean flat UI / 20% glass / 10% tactile depth.
*/

namespace ripples
{

struct RippleTheme
{
    //==========================================================================
    // PALETTE — near-black navy base, bioluminescent cyan accent.
    //==========================================================================

    // Backgrounds, darkest to lightest.
    juce::Colour background      { 0xff04070d };   // near-black navy, the abyss
    juce::Colour backgroundDeep  { 0xff02040a };   // vignette / far depth
    juce::Colour backgroundLift  { 0xff0a1220 };   // subtle upper-water lightening

    // Panels — translucent glass over the background.
    juce::Colour panel           { 0x9a0d1a2b };   // primary glass fill
    juce::Colour panelRaised     { 0xb2122135 };   // hover / focused glass
    juce::Colour panelSunken     { 0x8a070e18 };   // wells, graph backgrounds
    juce::Colour panelBorder     { 0x2a5fd8e8 };   // subtle cyan edge
    juce::Colour panelBorderSoft { 0x141f3a52 };   // quieter divider
    juce::Colour panelHighlight  { 0x14ffffff };   // internal top highlight

    // Accents.
    juce::Colour cyan            { 0xff4fd8ee };   // primary functional accent
    juce::Colour cyanBright      { 0xff8cecff };   // peak / focus
    juce::Colour cyanDim         { 0xff2a7d90 };   // inactive arc, tick marks
    juce::Colour aqua            { 0xff36c3a8 };   // modulation / motion
    juce::Colour aquaDim         { 0xff1d6b5f };
    juce::Colour violet          { 0xff8b7ae8 };   // pressure / glow accents, used sparingly
    juce::Colour violetDim       { 0xff4a4280 };
    juce::Colour coral           { 0xffe0705f };   // danger, only when necessary

    // Text.
    juce::Colour primaryText     { 0xfff0f6fa };   // off-white
    juce::Colour secondaryText   { 0xff8fa4b8 };   // muted blue-gray
    juce::Colour tertiaryText    { 0xff5d7086 };   // faint labels, units
    juce::Colour disabledText    { 0xff44566a };

    // Controls.
    juce::Colour knobBody        { 0xff121c2a };
    juce::Colour knobBodyTop     { 0xff1d2b3e };   // radial shading, upper
    juce::Colour knobBodyBottom  { 0xff080e17 };   // radial shading, lower
    juce::Colour knobRing        { 0xff1e2e42 };   // thin outer ring
    juce::Colour knobTrack       { 0xff16222f };   // unfilled value arc
    juce::Colour knobIndicator   { 0xffe8f6fb };   // pointer

    juce::Colour shadow          { 0x66000000 };
    juce::Colour glowCore        { 0x664fd8ee };

    //==========================================================================
    // GEOMETRY
    //==========================================================================
    float panelRadius     = 14.0f;
    float controlRadius   =  7.0f;
    float smallRadius     =  4.0f;
    float borderWidth     =  1.0f;
    float glowAmount      =  0.35f;   // 0..1, kept restrained on purpose
    float knobArcThickness = 3.0f;

    //==========================================================================
    // SPACING — a strict 4px grid.
    //==========================================================================
    static constexpr int unit  = 4;
    static constexpr int xs    = 4;
    static constexpr int sm    = 8;
    static constexpr int md    = 12;
    static constexpr int lg    = 16;
    static constexpr int xl    = 24;
    static constexpr int xxl   = 32;

    /** Snap any value to the spacing grid. */
    static constexpr int grid (int steps) noexcept { return steps * unit; }

    //==========================================================================
    // TYPOGRAPHY
    //==========================================================================
    juce::Font titleFont() const    { return withTracking (26.0f, juce::Font::plain, 0.38f); }
    juce::Font sectionFont() const  { return withTracking (13.0f, juce::Font::plain, 0.18f); }
    juce::Font bodyFont() const     { return withTracking (13.0f, juce::Font::plain, 0.0f);  }
    juce::Font labelFont() const    { return withTracking (10.5f, juce::Font::plain, 0.14f); }
    juce::Font valueFont() const    { return withTracking (11.5f, juce::Font::plain, 0.02f); }
    juce::Font smallFont() const    { return withTracking (9.5f,  juce::Font::plain, 0.12f); }

    /** Builds a sans-serif font with the given size and letter tracking.
        Tracking is expressed as a fraction of the font height. */
    static juce::Font withTracking (float height, int styleFlags, float tracking)
    {
        juce::Font f (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                         height, styleFlags));
        if (tracking != 0.0f)
            f.setExtraKerningFactor (tracking);
        return f;
    }

    //==========================================================================
    // ANIMATION
    //==========================================================================
    int   targetFrameRate     = 60;
    float hoverFadeSeconds    = 0.18f;
    float valueSmoothSeconds  = 0.08f;
    float ambientMotionRate   = 0.06f;   // very slow background drift, Hz

    //==========================================================================
    // HELPERS
    //==========================================================================

    /** The accent colour for a given semantic role. */
    juce::Colour accentFor (int roleIndex) const
    {
        switch (roleIndex % 3)
        {
            case 0:  return cyan;
            case 1:  return aqua;
            default: return violet;
        }
    }

    /** Vertical glass gradient for a panel of the given bounds. */
    juce::ColourGradient panelGradient (juce::Rectangle<float> b) const
    {
        juce::ColourGradient g (panel.brighter (0.06f), b.getCentreX(), b.getY(),
                                panel.darker (0.12f),   b.getCentreX(), b.getBottom(), false);
        return g;
    }

    /** The single shared instance. Components read tokens from here. */
    static const RippleTheme& get()
    {
        static const RippleTheme instance;
        return instance;
    }
};

} // namespace ripples
