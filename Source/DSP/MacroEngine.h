#pragma once

#include "Utilities/MathUtils.h"

namespace ripples
{

/**
    Turns the eight primary macros and the Fluid Field position into a coherent
    set of offsets applied across the whole instrument.

    The point of this class is that DEPTH and PRESSURE stay *different concepts*.
    Depth moves the listener from the surface toward the ocean floor: brightness
    falls, damping rises, the image narrows in the highs, movement slows, the sub
    grows. Pressure is density: drive, low-mid weight, resonance, saturation and
    resonator intensity all increase while transients soften. Mapping either one
    to "filter cutoff" alone would waste the idea, so neither is.

    Everything here is pure, stateless and realtime safe.
*/
struct MacroModulation
{
    // Voice-level
    float cutoffMultiplier      = 1.0f;
    float resonanceOffset       = 0.0f;
    float driveOffset           = 0.0f;
    float pressureOffset        = 0.0f;
    float noiseToneOffset       = 0.0f;
    float subLevelMultiplier    = 1.0f;
    float resonatorAmountOffset = 0.0f;
    float resonatorScatterOffset = 0.0f;
    float resonatorMotionOffset = 0.0f;
    float dropletAmountOffset   = 0.0f;
    float dropletRandomOffset   = 0.0f;
    float rippleDepthOffset     = 0.0f;
    float currentAmountOffset   = 0.0f;
    float currentRateMultiplier = 1.0f;
    float driftAmountOffset     = 0.0f;
    float pitchInstability      = 0.0f;   // 0..1, feeds fine-pitch wobble

    // Global effects
    float chorusMixOffset       = 0.0f;
    float delayMixOffset        = 0.0f;
    float delayMotionOffset     = 0.0f;
    float diffusionAmountOffset = 0.0f;
    float reverbMixOffset       = 0.0f;
    float reverbSizeOffset      = 0.0f;
    float reverbDampingOffset   = 0.0f;
    float stereoWidthMultiplier = 1.0f;
    float stereoRateMultiplier  = 1.0f;
    float lowMidGainDb          = 0.0f;
    float highShelfGainDb       = 0.0f;

    // Visual mood, published to the editor
    float visualDepth  = 0.5f;
    float visualGlow   = 0.3f;
    float visualMotion = 0.3f;
};

//==============================================================================
class MacroEngine
{
public:
    struct Input
    {
        // The eight primary macros, all 0..1.
        float depth = 0.5f, wet = 0.0f, ripple = 0.0f, current = 0.0f,
              drops = 0.0f, pressure = 0.0f, space = 0.0f, glow = 0.0f;

        // Fluid Field position, 0..1, and the preset's sensitivity to each axis.
        float fluidX = 0.5f, fluidY = 0.5f;
        float fluidXAmount = 1.0f, fluidYAmount = 1.0f;
    };

    static MacroModulation compute (const Input& in) noexcept
    {
        MacroModulation m;

        using namespace math;

        // The Fluid Field folds into the macros before anything else:
        // Y is another hand on DEPTH, X is another hand on CHAOS.
        const float fieldY = (in.fluidY - 0.5f) * 2.0f * in.fluidYAmount;   // -1..1
        const float fieldX = (in.fluidX - 0.5f) * 2.0f * in.fluidXAmount;   // -1..1

        const float depth = clamp (in.depth + fieldY * 0.5f, 0.0f, 1.0f);
        const float chaos = clamp (fieldX * 0.5f + 0.5f, 0.0f, 1.0f);       // 0 calm, 1 chaos

        // Smoothed curves keep the macros musical across their whole travel
        // rather than doing everything in the last 20%.
        const float d  = smootherstep (depth);
        const float p  = smootherstep (clamp (in.pressure, 0.0f, 1.0f));
        const float c  = clamp (in.current, 0.0f, 1.0f);
        const float sp = smoothstep (clamp (in.space, 0.0f, 1.0f));
        const float w  = clamp (in.wet, 0.0f, 1.0f);
        const float g  = clamp (in.glow, 0.0f, 1.0f);
        const float dr = clamp (in.drops, 0.0f, 1.0f);
        const float rp = clamp (in.ripple, 0.0f, 1.0f);
        const float ch = smoothstep (chaos);

        //----------------------------------------------------------------------
        // DEPTH — surface to ocean floor.
        //----------------------------------------------------------------------
        // Brightness falls by up to ~3.5 octaves, but never to nothing: even the
        // abyss keeps some definition or the instrument just sounds broken.
        m.cutoffMultiplier   = std::pow (2.0f, -3.5f * d);
        m.noiseToneOffset    = -0.55f * d;          // noise darkens with depth
        m.subLevelMultiplier = 1.0f + 0.85f * d;    // low end swells
        m.lowMidGainDb       = 3.5f * d;            // low-mid emphasis
        m.highShelfGainDb    = -7.0f * d;           // high-frequency damping
        m.reverbDampingOffset = 0.45f * d;          // the water absorbs the tail
        m.stereoWidthMultiplier = 1.0f - 0.3f * d;  // subtle narrowing when deep
        m.diffusionAmountOffset = 0.3f * d;         // more scattering at depth
        m.currentRateMultiplier = 1.0f - 0.45f * d; // movement slows under pressure

        //----------------------------------------------------------------------
        // PRESSURE — sonic density. A different axis from Depth by design.
        //----------------------------------------------------------------------
        m.driveOffset           = 0.65f * p;
        m.pressureOffset        = p;
        m.resonanceOffset       = 0.22f * p;
        m.lowMidGainDb         += 4.0f * p;         // body, on top of Depth's
        m.subLevelMultiplier   *= 1.0f + 0.4f * p;  // sub reinforcement
        m.resonatorAmountOffset = 0.3f * p;         // resonators bite harder

        //----------------------------------------------------------------------
        // CURRENT — smooth organic movement.
        //----------------------------------------------------------------------
        m.currentAmountOffset  = c;
        m.driftAmountOffset    = 0.6f * c;
        m.stereoRateMultiplier = 1.0f + 1.5f * c;
        m.delayMotionOffset    = 0.35f * c;

        //----------------------------------------------------------------------
        // RIPPLE — decaying wave modulation.
        //----------------------------------------------------------------------
        m.rippleDepthOffset = rp;

        //----------------------------------------------------------------------
        // DROPS — droplet activity.
        //----------------------------------------------------------------------
        m.dropletAmountOffset = dr;

        //----------------------------------------------------------------------
        // SPACE — the size of the room, distinct from WET (how much of it).
        //----------------------------------------------------------------------
        m.reverbSizeOffset = 0.5f * sp;
        m.delayMixOffset   = 0.25f * sp;
        m.diffusionAmountOffset += 0.35f * sp;

        //----------------------------------------------------------------------
        // WET — the global sense of submersion.
        //----------------------------------------------------------------------
        m.reverbMixOffset  = 0.55f * w;
        m.chorusMixOffset  = 0.3f * w;
        m.delayMixOffset  += 0.3f * w;

        //----------------------------------------------------------------------
        // GLOW — bioluminescent sheen. Opens the top and lights the resonators,
        // deliberately pulling against DEPTH so the two can be combined.
        //----------------------------------------------------------------------
        m.cutoffMultiplier      *= 1.0f + 1.6f * g;
        m.highShelfGainDb       += 4.0f * g;
        m.resonatorAmountOffset += 0.35f * g;
        m.resonatorMotionOffset  = 0.3f * g;

        //----------------------------------------------------------------------
        // FLUID FIELD X — calm to chaos. Instability across every random source.
        //----------------------------------------------------------------------
        m.currentAmountOffset     += 0.5f * ch;
        m.currentRateMultiplier   *= 1.0f + 2.0f * ch;
        m.resonatorScatterOffset   = 0.6f * ch;
        m.dropletRandomOffset      = 0.7f * ch;
        m.delayMotionOffset       += 0.4f * ch;
        m.stereoWidthMultiplier   *= 1.0f + 0.35f * ch;
        m.pitchInstability         = 0.35f * ch;
        m.driftAmountOffset       += 0.3f * ch;

        //----------------------------------------------------------------------
        // Visual mood for the Fluid Field.
        //----------------------------------------------------------------------
        m.visualDepth  = d;
        m.visualGlow   = clamp (g * 0.7f + (1.0f - d) * 0.3f, 0.0f, 1.0f);
        m.visualMotion = clamp (c * 0.5f + ch * 0.5f, 0.0f, 1.0f);

        //----------------------------------------------------------------------
        // Final safety: nothing downstream should ever receive a wild value.
        //----------------------------------------------------------------------
        m.cutoffMultiplier      = clamp (m.cutoffMultiplier, 0.03f, 8.0f);
        m.subLevelMultiplier    = clamp (m.subLevelMultiplier, 0.0f, 3.0f);
        m.stereoWidthMultiplier = clamp (m.stereoWidthMultiplier, 0.0f, 2.0f);
        m.currentRateMultiplier = clamp (m.currentRateMultiplier, 0.05f, 8.0f);
        m.stereoRateMultiplier  = clamp (m.stereoRateMultiplier, 0.05f, 8.0f);
        m.lowMidGainDb          = clamp (m.lowMidGainDb, -12.0f, 12.0f);
        m.highShelfGainDb       = clamp (m.highShelfGainDb, -18.0f, 12.0f);

        return m;
    }
};

} // namespace ripples
