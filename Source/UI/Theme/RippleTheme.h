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

    //==========================================================================
    // GLASS VESSEL
    //
    // A panel is a glass container with water in it, not a flat dark card. The
    // cues that actually sell that, in order of how much they matter:
    //
    //   1. WALL THICKNESS — two edge strokes a couple of pixels apart (a lit
    //      outer rim and a darker inner wall). Without this, glass reads as a
    //      sticker. With it, everything else becomes believable.
    //   2. A LIQUID COLUMN — denser and darker at the bottom than the top,
    //      because there is more water to look through down there.
    //   3. A MENISCUS — the bright line where liquid climbs the inside of the
    //      glass. It is what makes the fill read as a LIQUID rather than paint.
    //   4. EDGE REFRACTION — light bending through the curved wall makes the
    //      liquid brighter in a narrow band against each side.
    //   5. A SPECULAR STREAK — a vertical highlight, not a wash. Cylindrical
    //      glass catches light in a line down its length.
    //   6. A CAUSTIC FLOOR — light focused through the water pools at the base.
    //==========================================================================

    // The water column, top (aerated, lighter) to bottom (dense, dark).
    juce::Colour liquidTop        { 0x5c1a3350 };
    juce::Colour liquidMid        { 0x780e1d31 };
    juce::Colour liquidDeep       { 0x9e050b15 };

    // Glass itself. The rim is lit from above, so its top is far brighter than
    // its bottom; the inner wall is the refracted back edge seen through the
    // liquid.
    juce::Colour glassRimTop      { 0xbcc6f0ff };
    juce::Colour glassRimBottom   { 0x3a4a9ab4 };
    juce::Colour glassInnerWall   { 0x3a0a1420 };
    juce::Colour glassSpecular    { 0x30dff8ff };
    juce::Colour meniscus         { 0x6693e4f2 };
    juce::Colour causticFloor     { 0x264fd8ee };
    juce::Colour edgeRefraction   { 0x337fdcea };

    float glassWallThickness   = 1.8f;   // px between the outer rim and inner wall
    float glassRimWidth        = 1.5f;   // the wall reads as thin below about this
    float glassLipWidth        = 2.2f;   // the lit upper lip, brightest part of the vessel
    float meniscusInset        = 3.2f;   // px below the inner top edge
    float meniscusThickness    = 1.1f;
    float specularWidthRatio   = 0.085f; // of panel width
    float specularLeftRatio    = 0.10f;  // where the main streak sits
    float specularRightRatio   = 0.93f;  // the weaker opposite catch
    float refractionBandRatio  = 0.055f; // how far the edge band reaches inward
    float causticHeightRatio   = 0.15f;  // how tall the pool of light at the base is

    //==========================================================================
    // LUMINOUS PASS
    //
    // The reference art is markedly brighter than a first implementation tends
    // to be: the waves are thick glowing ribbons rather than hairlines, knob
    // arcs read from across a room, and the active tab is a filled pill. These
    // tokens exist so that character lives in one place instead of being
    // scattered as magic numbers.
    //==========================================================================

    // Waveform / graph traces. A trace is built from a wide, very faint bloom,
    // a mid halo, then the crisp core line on top.
    juce::Colour traceCore        { 0xffbdf2ff };
    juce::Colour traceGlow        { 0x8c4fd8ee };
    juce::Colour traceBloom       { 0x3322b8d8 };
    juce::Colour traceFill        { 0x2a1f9fc4 };   // wash under a curve
    juce::Colour traceGhost       { 0x33307a92 };   // the layer behind the main one

    float traceCoreWidth   = 1.8f;
    float traceGlowWidth   = 4.5f;
    float traceBloomWidth  = 11.0f;
    float traceGlowAlpha   = 0.55f;
    float traceBloomAlpha  = 0.28f;

    // Knob arc: the single loudest colour in the interface.
    juce::Colour arcCore          { 0xff5fe3f7 };
    juce::Colour arcBloom         { 0x664fd8ee };
    float arcBloomWidth    = 5.0f;
    float arcBloomAlpha    = 0.40f;

    // Active-tab pill and other filled chips.
    juce::Colour pillFill         { 0xff123044 };
    juce::Colour pillFillActive   { 0xff1d5f7a };
    juce::Colour pillBorder       { 0x665fe3f7 };
    juce::Colour pillText         { 0xffe8fbff };
    float pillRadius       = 16.0f;

    // Header preset capsule.
    juce::Colour capsuleFill      { 0x66091726 };
    juce::Colour capsuleBorder    { 0x4a4fd8ee };
    float capsuleRadius    = 22.0f;

    // Output meter bar and the header oscilloscope.
    juce::Colour meterTrack       { 0xff0c1a26 };
    juce::Colour meterFillLow     { 0xff2ad4e8 };
    juce::Colour meterFillHigh    { 0xffa8f0ff };
    juce::Colour scopeTrace       { 0xffdff6ff };
    float meterBarHeight   = 8.0f;
    float meterBarRadius   = 4.0f;

    // Section-title glyphs.
    juce::Colour glyphStroke      { 0xff7fe4f6 };
    float glyphSize        = 18.0f;
    float glyphStrokeWidth = 1.4f;

    // Footer strip.
    juce::Colour footerText       { 0xff3d5568 };
    float footerHeight     = 26.0f;

    /** The single shared instance. Components read tokens from here. */
    static const RippleTheme& get()
    {
        static const RippleTheme instance;
        return instance;
    }

    //==========================================================================
    // TACTILE CONTROL SET
    //
    // Tokens added for RippleLookAndFeel, GlassPanel, RippleKnob,
    // RippleSelector, RippleToggle, RippleButton and SectionHeader. Everything
    // below is additive — nothing above was changed.
    //==========================================================================

    // --- Control surfaces -----------------------------------------------------
    juce::Colour controlFill        { 0xff0b1420 };   // flat dark well (selector, button)
    juce::Colour controlFillHover   { 0xff101d2c };
    juce::Colour controlFillDown    { 0xff070e18 };
    juce::Colour controlBorder      { 0x3a41677d };   // thin neutral border
    juce::Colour controlBorderHover { 0x664fd8ee };   // border warms to cyan on hover
    juce::Colour focusRing          { 0xcc8cecff };   // keyboard focus indication

    // --- Knob extras ----------------------------------------------------------
    juce::Colour knobHighlight      { 0x1effffff };   // mild inner top highlight
    juce::Colour knobShadow         { 0x59000000 };   // soft drop shadow beneath the body
    juce::Colour knobRingHover      { 0x804fd8ee };   // ring lifts toward cyan on hover
    juce::Colour modRing            { 0xff36c3a8 };   // modulation ring (aqua)
    juce::Colour modRingTrack       { 0x2236c3a8 };

    // --- Toggle ---------------------------------------------------------------
    juce::Colour toggleTrackOff     { 0xff101b28 };
    juce::Colour toggleTrackBorder  { 0x44557a8e };
    juce::Colour toggleThumbOff     { 0xff7e93a8 };   // dim thumb, parked left
    juce::Colour toggleThumbOn      { 0xff05121c };   // dark thumb on the lit track

    // --- Popup menus, tooltips, scrollbars, editors ---------------------------
    juce::Colour menuBackground     { 0xf00a1322 };   // dark glass popup
    juce::Colour menuBorder         { 0x384fd8ee };
    juce::Colour menuHighlight      { 0x3a4fd8ee };
    juce::Colour menuSeparator      { 0x1f5fd8e8 };
    juce::Colour tooltipBackground  { 0xf4091220 };
    juce::Colour scrollbarThumb     { 0x662a7d90 };
    juce::Colour scrollbarThumbOver { 0xaa4fd8ee };
    juce::Colour selectionHighlight { 0x554fd8ee };   // text selection

    // --- Extra geometry, in device-independent pixels -------------------------
    float knobRingThickness      =  1.4f;
    float knobModArcThickness    =  2.0f;
    float knobIndicatorThickness =  2.0f;
    float knobShadowOffset       =  2.0f;
    float knobShadowSpread       =  6.0f;
    float panelShadowSpread      = 10.0f;
    float panelShadowOffset      =  3.0f;
    int   shadowLayers           =  5;      // layered strokes instead of a blurred image
    float focusRingWidth         =  1.6f;
    float focusRingPadding       =  2.0f;

    // --- Knob proportions, as fractions of the control radius ------------------
    float knobBodyRadiusRatio      = 0.72f;
    float knobRingRadiusRatio      = 0.755f;
    float knobArcRadiusRatio       = 0.885f;
    float knobModRadiusRatio       = 0.985f;
    float knobIndicatorInnerRatio  = 0.28f;
    float knobIndicatorOuterRatio  = 0.62f;
    float knobHighlightRadiusRatio = 0.58f;
    float knobShadeOffsetRatio     = 0.42f;   // radial shading centre, above the middle
    float knobShadeSpreadRatio     = 1.45f;

    // --- Rotary sweep, clockwise from twelve o'clock ---------------------------
    float knobStartAngle = juce::MathConstants<float>::pi * 1.25f;
    float knobEndAngle   = juce::MathConstants<float>::pi * 2.75f;

    // --- Control sizes ---------------------------------------------------------
    int knobLargeDiameter   = 62;
    int knobMediumDiameter  = 46;
    int knobSmallDiameter   = 34;
    int knobLabelHeight     = 13;
    int knobValueHeight     = 14;
    int selectorHeight      = 26;
    int buttonHeight        = 26;
    int toggleWidth         = 34;
    int toggleHeight        = 18;
    int sectionHeaderHeight = 20;
    int menuItemHeight      = 22;
    int menuBorderSize      = 6;
    int scrollbarWidth      = 10;
    int panelTitleHeight    = 24;
    int panelContentInset   = 12;

    // --- Fine detail sizes -----------------------------------------------------
    float chevronThickness  = 1.6f;
    int   chevronSize       = 5;
    float toggleThumbInset  = 2.5f;
    float sectionTickWidth  = 2.0f;
    int   sectionTickHeight = 10;
    int   tooltipPadding    = 8;
    float panelHighlightRatio = 0.34f;   // height of the internal top highlight

    // --- Detail levels: small controls drop ornament, never legibility ---------
    float knobDetailLarge     = 1.0f;
    float knobDetailMedium    = 0.7f;
    float knobDetailSmall     = 0.35f;
    float knobDetailThreshold = 0.5f;

    // --- Knob interaction ------------------------------------------------------
    int   knobDragPixels       = 220;    // pixels of vertical travel for the full range
    int   knobVelocityThreshold = 1;
    float knobFineVelocity     = 0.28f;  // sensitivity while Shift / Ctrl / Cmd is held
    float knobWheelFineScale   = 0.22f;  // wheel step scaling in fine mode

    // --- Intensities -----------------------------------------------------------
    float disabledAlpha    = 0.42f;   // disabled controls stay visible, never invisible
    float hoverBrighten    = 0.22f;
    float pressDarken      = 0.14f;
    float modRingAlpha     = 0.85f;
    float arcGlowAlpha     = 0.30f;
    float shadowLayerAlpha = 0.16f;
    float modRingEpsilon   = 0.002f;  // ignore modulation changes smaller than this
};

} // namespace ripples
