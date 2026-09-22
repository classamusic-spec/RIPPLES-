#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Utilities/VisualizationState.h"
#include "UI/Views/WaterSurface.h"
#include "Utilities/RandomGenerator.h"

#include <array>
#include <cstdint>
#include <memory>

namespace ripples
{

/**
    FLUID FIELD — the visual centrepiece of RIPPLES.

    A rectangular XY field: a pool of bioluminescent water that fills the whole
    card, seen from above. The water is a live damped-wave height field (see
    WaterSurface), lit per pixel into a flat top-down pool -- ripples spread
    edge to edge, interfere, reflect off the walls and decay. The tank walls,
    meniscus and floor caustic are cached; only the water and the node move.
    There is no bitmap art anywhere in this component.

      X axis:  CALM (left)    <-> CHAOS (right)   -> pid::fluidX
      Y axis:  SURFACE (top)  <-> DEPTH (bottom)  -> pid::fluidY

    Interaction
      - drag the luminous centre node (grab it, or click anywhere in the pool)
      - hold Shift for fine adjustment
      - double-click to return to the parameter defaults
      Host automation is recorded properly: every drag is wrapped in a
      begin/end gesture pair through juce::ParameterAttachment.

    Audio response is read only from the lock-free VisualizationState:
    note events spawn ripples, droplets spawn surface impacts, and the
    depth / glow / RMS / current values shape the light. All of it is
    deliberately restrained — the field is meant to be calm enough to stare
    at for hours.
*/
class FluidField final : public juce::Component,
                         private juce::Timer
{
public:
    FluidField (juce::AudioProcessorValueTreeState& apvts, VisualizationState& vis);
    ~FluidField() override;

    //==========================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    /** The node position, normalised 0..1 on each axis.
        x: 0 = CALM, 1 = CHAOS.  y: 0 = SURFACE, 1 = DEPTH. */
    juce::Point<float> getNodePosition() const noexcept { return { nodeX, nodeY }; }

private:
    //==========================================================================
    // Fixed pool sizes. Nothing here is ever resized at runtime.
    static constexpr int kRingCount     = 16;    // concentric undulating ellipses
    static constexpr int kAngleSteps    = 128;   // power of two: harmonic tables use a mask
    static constexpr int kAngleMask     = kAngleSteps - 1;
    static constexpr int kHarmonics     = 3;     // sine terms summed into each ring radius
    static constexpr int kParticleCount = 64;    // suspended motes
    static constexpr int kRippleCount   = 20;    // note + droplet pulses
    static constexpr int kBloomSteps    = 9;     // stacked passes that fake the node bloom
    static constexpr int kWellSteps     = 7;     // stacked passes that fake the depth well
    static constexpr int kGuideSegs     = 8;     // crosshair fade segments per arm

    /** One suspended mote. Stored in polar field coordinates so the slow
        orbital drift is a single add per frame. */
    struct Particle
    {
        float angle = 0.0f;        // radians around the field centre
        float radius = 0.0f;       // 0..1 of the field radius
        float angVel = 0.0f;       // radians / second (very slow)
        float breathePhase = 0.0f; // radial breathing
        float breatheRate = 0.0f;
        float depth = 0.0f;        // 0 = just under the surface, 1 = far down
        float size = 1.0f;         // relative dot size
        float twinklePhase = 0.0f;
        float twinkleRate = 0.0f;
        float brightness = 0.5f;
    };

    /** One expanding ripple. Recycled from a fixed pool, never allocated. */
    struct RipplePulse
    {
        bool  active = false;
        bool  isDroplet = false;
        float age = 0.0f;
        float life = 1.0f;
        float originX = 0.0f;   // field-normalised, -1..1 across the field radius
        float originY = 0.0f;
        float intensity = 0.0f;
        float spread = 1.0f;    // final radius as a fraction of the field radius
    };

    //==========================================================================
    void timerCallback() override;
    void updateTimerState();
    void advance (float dt);
    void pollParameters();
    void consumeAudioEvents();
    void spawnRipple (float ox, float oy, float intensity, bool droplet);
    void seedParticle (Particle& p, bool anywhere);

    void recomputeGeometry();
    void rebuildBackdrop (float deviceScale);
    void renderWater();
    void applyWaterParams();
    void strikeWater (float ox, float oy, float intensity, bool droplet);
    void paintWater (juce::Graphics&);

    void paintParticles (juce::Graphics& g) const;
    void paintGuides (juce::Graphics& g) const;
    void paintNode (juce::Graphics& g) const;
    void paintLabels (juce::Graphics& g) const;
    void paintVessel (juce::Graphics&);
    void rebuildVessel (float deviceScale);

    juce::Point<float> nodeToPixels (float nx, float ny) const noexcept;
    void setNormalisedParameters (float nx, float ny, bool partOfGesture);

    //==========================================================================
    // Bindings.
    juce::AudioProcessorValueTreeState& state;
    VisualizationState& visual;

    juce::RangedAudioParameter* paramX = nullptr;
    juce::RangedAudioParameter* paramY = nullptr;

    // Read, never written: the RIPPLE macro sets how hard the surface is driven
    // into its standing pattern. The visualised modulator value alone does not
    // carry the macro, so the field has to read the knob itself.
    juce::RangedAudioParameter* paramRipple = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachX, attachY;

    // Node state, all normalised 0..1.
    float targetX = 0.5f, targetY = 0.5f;
    float nodeX   = 0.5f, nodeY   = 0.5f;
    float defaultX = 0.5f, defaultY = 0.5f;

    // Interaction.
    bool  dragging = false;
    bool  gestureOpen = false;   // a begin/endGesture pair is currently open
    bool  fineDrag = false;      // Shift was held when the drag was last anchored
    bool  hoveringNode = false;
    float hoverAmount = 0.0f;
    juce::Point<float> grabMouse;
    float grabX = 0.5f, grabY = 0.5f;
    float dragVecX = 0.0f, dragVecY = 0.0f;   // smoothed drag direction, decays to zero
    float lastNodeX = 0.5f, lastNodeY = 0.5f;

    // Audio-derived, all smoothed towards their VisualizationState targets.
    float glowSmoothed    = 0.3f;
    float depthSmoothed   = 0.5f;
    float motionSmoothed  = 0.3f;
    float rmsSmoothed     = 0.0f;
    float currentSmoothed = 0.0f;
    float energy          = 0.0f;
    uint32_t lastDropletCount = 0;

    // Time. Every animated quantity runs on a wrapped phase rather than a
    // free-running clock, so nothing drifts out of precision or jogs after
    // hours of being open.
    double lastTickMs   = 0.0;
    float  driftPhaseA  = 0.0f;
    float  driftPhaseB  = 0.0f;
    std::array<float, kHarmonics> harmonicPhase {};

    // Geometry in logical pixels, recomputed on resize.
    juce::Rectangle<float> fieldArea;       // interaction frame (node, labels)
    juce::Rectangle<float> poolArea;        // the water fills this, edge to edge
    float centreX = 0.0f, centreY = 0.0f;   // static centre of the field
    float fieldCx = 0.0f, fieldCy = 0.0f;   // live centre, including the slow current drift
    float fieldRx = 1.0f, fieldRy = 1.0f;
    float uiScale = 1.0f;

    // Cached static layer: deep gradient + pool + vignette.
    juce::Image backdrop;
    WaterSurface   water;
    juce::Image    waterImage;
    RandomGenerator waterRng { 0x51A7E3u };
    float          standingPhase = 0.0f;   // slow drift of the pattern's centre
    float          standingOsc = 0.0f;     // the resonant drive itself
    float          rippleSmoothed = 0.0f;
    float          rippleMacro = 0.0f;
    float          rippleEnvelope = 0.0f;   // for the rising edge
    float          breathTimer = 0.0f;
    float          breathAngle = 0.0f;
    juce::Image vessel;
    float       vesselScale = 0.0f;
    float backdropScale = 0.0f;

    // Reusable scratch. Cleared and rebuilt, never reallocated after warm-up.
    juce::Path ringPath;

    // Lookup tables for the ring harmonics.
    std::array<float, kAngleSteps> cosTable {};
    std::array<float, kAngleSteps> sinTable {};

    // Fixed pools.
    std::array<Particle, kParticleCount> particles {};
    std::array<RipplePulse, kRippleCount> ripples {};
    int nextRippleSlot = 0;

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluidField)
};

} // namespace ripples
