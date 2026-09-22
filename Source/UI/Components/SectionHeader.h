#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/Theme/RippleTheme.h"

namespace ripples
{

//==============================================================================
/**
    A small uppercase, widely tracked title with an optional subtitle, in the
    style "TIDE / OSC A". A glyph sits at the left — a thin cyan line-art icon
    built from juce::Path, never an asset — then the title, then the dimmer
    subtitle, then a quiet rule out to the right edge, so a row of sections
    reads as one system.

    The glyph defaults to the plain accent tick the instrument has always used,
    so a caller that asks for nothing gets exactly what it got before.
*/
class SectionHeader : public juce::Component
{
public:
    /** The line-art icons a panel title can carry. Every one is drawn from a
        juce::Path built once and shared by all headers. */
    enum class Glyph
    {
        Tick = 0,   /**< The plain accent tick. The default. */
        Wave,       /**< One period of a sine — oscillators, LFOs, motion. */
        DualWave,   /**< Two layered waves — a second source, unison, mixing. */
        Filter,     /**< A lowpass curve with a resonant bump. */
        Envelope,   /**< An attack / decay / sustain / release outline. */
        Droplet,    /**< A falling drop — droplets and water sources. */
        Scatter     /**< A scatter of dots — diffusion, randomness, presets. */
    };

    explicit SectionHeader (const juce::String& title,
                            const juce::String& subtitle = {},
                            Glyph glyph = Glyph::Tick);

    void setAccent (juce::Colour accent);
    void setTitle (const juce::String& title);
    void setSubtitle (const juce::String& subtitle);

    /** Swaps the left-hand icon. Cheap — the artwork is shared, not rebuilt. */
    void setGlyph (Glyph glyph);
    Glyph getGlyph() const noexcept  { return glyphKind; }

    void paint (juce::Graphics&) override;

private:
    juce::String titleText;
    juce::String subtitleText;
    juce::Colour accentColour { RippleTheme::get().cyan };
    Glyph        glyphKind { Glyph::Tick };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionHeader)
};

} // namespace ripples
