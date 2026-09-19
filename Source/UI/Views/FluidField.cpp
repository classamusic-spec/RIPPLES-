#include "UI/Views/FluidField.h"

#include "Parameters/ParameterIDs.h"
#include "UI/Theme/RippleTheme.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace ripples
{

//==============================================================================
// Proportions and time constants.
//
// Nothing in this file is an arbitrary pixel value: every dimension is either
// a fraction of the field radius (the constants below), a RippleTheme spacing
// grid step, or a RippleTheme geometry token. Every colour comes from
// RippleTheme::get().
//==============================================================================
namespace
{
    //--- Field geometry, as fractions --------------------------------------
    constexpr float kFieldAspectMax   = 0.92f;   // pool height / width, at most
    constexpr float kFieldAspectMin   = 0.46f;   // ...and at least
    constexpr float kNodeTravel       = 0.66f;   // node roams this much of the field radius
    constexpr int   kLabelGutterSteps = 5;       // grid steps reserved above / below
    constexpr int   kLabelSideSteps   = 15;      // grid steps reserved left / right
    constexpr int   kScaleRefSteps    = 100;     // field diameter that maps to uiScale 1.0
    constexpr float kUiScaleMin       = 0.62f;
    constexpr float kUiScaleMax       = 2.30f;

    //--- Concentric ring construction --------------------------------------
    constexpr float kRingInnerRadius  = 0.17f;   // radius of the innermost ring
    // The ring family is drawn inside this fraction of the field radius so that
    // kRingFit * kRingScaleSurface * (1 + kRingPerturbLimit) stays <= 1.0.
    // That makes it impossible for an undulating ring to reach the component
    // edge and be hard-clipped, at any size and any parameter combination.
    constexpr float kRingFit          = 0.80f;
    constexpr float kRingPerturbLimit = 0.19f;
    constexpr int   kHarmonicN[3]     = { 2, 3, 5 };           // integer angular harmonics
    constexpr float kHarmonicAmp[3]   = { 0.0190f, 0.0115f, 0.0062f };
    constexpr float kHarmonicRate[3]  = { 0.041f, 0.067f, 0.094f };   // Hz — mutually irrational-ish
    constexpr float kHarmonicSpatial[3] = { 2.35f, -3.70f, 5.15f };   // radians of phase per unit radius
    constexpr float kWobbleBase       = 1.0f;
    constexpr float kChaosWobbleMul   = 1.50f;   // extra undulation at full CHAOS
    constexpr float kMotionWobbleMul  = 0.55f;
    constexpr float kMotionRateMul    = 0.85f;   // MOTION speeds the undulation up
    constexpr float kRingAmpNear      = 0.38f;   // inner rings wobble less...
    constexpr float kRingAmpFar       = 1.05f;   // ...than outer rings
    constexpr float kRingFollow       = 0.86f;   // inner rings re-centre on the node
    constexpr float kRingFollowFall   = 1.55f;
    constexpr float kRingScaleSurface = 1.05f;   // rings open up near the SURFACE...
    constexpr float kRingScaleDepth   = 0.85f;   // ...and recede with DEPTH
    constexpr float kRingSquashSurface = 1.00f;
    constexpr float kRingSquashDepth   = 0.84f;
    constexpr float kDragBulge        = 0.085f;  // liquid bulge toward the drag direction
    constexpr float kRingAlphaNear    = 0.115f;
    constexpr float kRingAlphaFar     = 0.022f;
    constexpr float kRingWidthNear    = 1.35f;
    constexpr float kRingWidthFar     = 0.62f;
    constexpr float kRingDepthDim     = 0.32f;
    constexpr float kRingEnergyLift   = 0.40f;
    constexpr float kRingVioletTint   = 0.22f;
    constexpr float kRingGlowTint     = 0.30f;
    constexpr float kSoftPassLimit    = 0.55f;   // only the inner rings get the soft pass
    constexpr float kSoftPassWidth    = 3.4f;
    constexpr float kSoftPassAlpha    = 0.30f;
    constexpr float kRingAlphaCeiling = 0.42f;

    //--- Depth well ---------------------------------------------------------
    constexpr float kWellRadiusBase   = 0.12f;
    constexpr float kWellRadiusSpan   = 0.50f;
    constexpr float kWellAlpha        = 0.058f;

    //--- Particles ----------------------------------------------------------
    constexpr float kParticleRise     = 0.014f;  // outward drift, field radii / second
    constexpr float kParticleAngMin   = 0.008f;  // radians / second
    constexpr float kParticleAngMax   = 0.052f;
    constexpr float kParticleSizeMin  = 0.55f;
    constexpr float kParticleSizeMax  = 2.05f;
    constexpr float kParticleBreathe  = 0.035f;
    constexpr float kBreatheRateMin   = 0.10f;
    constexpr float kBreatheRateMax   = 0.42f;
    constexpr float kTwinkleRateMin   = 0.55f;
    constexpr float kTwinkleRateMax   = 2.10f;
    constexpr float kTwinkleBase      = 0.62f;
    constexpr float kTwinkleDepth     = 0.38f;
    constexpr float kParticleBrightMin = 0.16f;
    constexpr float kParticleBrightMax = 0.66f;
    constexpr float kParallaxNear     = 1.02f;   // shallow motes sit wider...
    constexpr float kParallaxFar      = 0.72f;   // ...deep ones are pulled in
    constexpr float kParticleFlatten  = 0.94f;
    constexpr float kParticleDepthFade = 0.38f;
    constexpr float kParticleDepthDim  = 0.30f;
    constexpr float kParticleDepthShrink = 0.55f;
    constexpr float kParticleFadeStart = 0.68f;  // rim fade-out begins here
    constexpr float kParticleHaloMul   = 2.6f;
    constexpr float kParticleHaloAlpha = 0.18f;
    constexpr float kParticleDragShift = 0.035f;
    constexpr float kChaosSwirl        = 1.30f;
    constexpr float kMotionSwirl       = 0.55f;
    constexpr float kParticleSeedInner = 0.14f;

    //--- Ripple pulses ------------------------------------------------------
    constexpr float kNoteLifeBase     = 2.45f;   // seconds
    constexpr float kNoteLifeVel      = 1.30f;
    constexpr float kNoteSpreadBase   = 0.52f;
    constexpr float kNoteSpreadVel    = 0.46f;
    constexpr float kNoteAlpha        = 0.46f;
    constexpr float kDropletLife      = 0.95f;
    constexpr float kDropletSpreadBase = 0.085f;
    constexpr float kDropletSpreadInt  = 0.130f;
    constexpr float kDropletAlpha      = 0.36f;
    constexpr float kDropletFlashPow   = 3.2f;
    constexpr float kDropletFlashR     = 1.5f;   // logical px at uiScale 1
    constexpr float kRippleFadePow     = 1.7f;
    constexpr float kRippleWidthWide   = 3.2f;
    constexpr float kRippleWidthMid    = 1.6f;
    constexpr float kRippleWidthThin   = 0.8f;
    constexpr float kRippleNodeBias    = 0.55f;
    constexpr float kPitchSpreadX      = 0.55f;
    constexpr float kPitchLiftY        = 0.34f;
    constexpr float kRippleOriginLimit = 0.84f;
    constexpr float kRippleRimFadeStart = 0.70f; // a pulse is fully gone by the time
    constexpr float kRippleRimFadeEnd   = 1.00f; // it reaches the rim -- never clipped
    constexpr float kDropletPanSpread  = 0.80f;
    constexpr float kDropletJitter     = 0.14f;
    constexpr float kDropletYSpread    = 1.20f;
    constexpr uint32_t kMaxDropletsPerFrame = 3;

    //--- Node ---------------------------------------------------------------
    constexpr int   kNodeRadiusSteps  = 2;       // grid steps -> 8 logical px at uiScale 1
    constexpr float kNodeHoverGrow    = 0.16f;
    constexpr float kNodeGrabMul      = 2.6f;    // generous grab radius
    constexpr float kNodeBloomMul     = 8.5f;
    constexpr float kNodeBloomEnergy  = 0.28f;
    constexpr float kNodeBloomAlpha   = 0.135f;
    constexpr float kNodeBloomFlatten = 0.88f;
    constexpr float kNodeHaloSpread   = 1.15f;
    constexpr float kNodeHaloAlpha    = 0.22f;
    constexpr float kNodeShadeOffset  = 0.34f;
    constexpr float kNodeSphereGradR  = 2.05f;
    constexpr float kNodeCoreRatio    = 0.34f;
    constexpr float kNodeCoreAlpha    = 0.78f;
    constexpr float kNodeCoreEnergy   = 0.20f;
    constexpr float kNodeRimAlpha     = 0.55f;
    constexpr float kNodeSpecRatio    = 0.17f;
    constexpr float kNodeSpecAlpha    = 0.55f;

    //--- Guides -------------------------------------------------------------
    constexpr float kGuideAlpha       = 0.26f;
    constexpr float kGuideIdle        = 0.34f;   // alpha multiplier when not hovered
    constexpr int   kGuideTickSteps   = 2;       // grid steps
    constexpr float kGuideTickAlpha   = 2.0f;

    //--- Backdrop -----------------------------------------------------------
    constexpr float kPoolRadiusMul    = 1.16f;
    constexpr float kPoolWellAlpha    = 0.88f;   // the dark well you look down into
    constexpr double kPoolStop1       = 0.24;
    constexpr float kPoolStop1Alpha   = 0.34f;
    constexpr double kPoolStop2       = 0.53;
    constexpr float kPoolLiftAlpha    = 0.62f;   // the band of scattered light
    constexpr double kPoolStop3       = 0.79;
    constexpr float kPoolStop3Alpha   = 0.52f;
    constexpr float kPoolRimAlpha     = 0.94f;
    constexpr float kVignetteRadiusMul = 0.64f;
    constexpr double kVignetteStop    = 0.58;
    constexpr float kVignetteMidAlpha = 0.16f;
    constexpr float kVignetteEdgeAlpha = 0.93f;
    constexpr float kCoverExpand      = 3.0f;
    constexpr float kRimAlpha         = 1.0f;
    constexpr float kSheenRadius      = 0.985f;
    constexpr float kSheenSweep       = 0.85f;   // radians either side of twelve o'clock
    constexpr float kSheenAlpha       = 1.3f;
    constexpr float kSheenWidth       = 1.6f;
    constexpr float kMinDeviceScale   = 0.5f;
    constexpr float kMaxDeviceScale   = 4.0f;
    constexpr float kScaleEpsilon     = 0.01f;

    //--- Drift and smoothing ------------------------------------------------
    constexpr float kCurrentDriftX    = 0.011f;  // fractions of the field radius
    constexpr float kCurrentDriftY    = 0.005f;
    constexpr float kAmbientDriftX    = 0.007f;
    constexpr float kAmbientDriftY    = 0.004f;
    constexpr float kDriftRateRatio   = 0.63f;   // the two drift axes never re-phase
    constexpr float kMoodTau          = 0.38f;   // seconds
    constexpr float kRmsTau           = 0.13f;
    constexpr float kCurrentTau       = 0.55f;
    constexpr float kNodeDragTau      = 0.030f;
    constexpr float kNodeGlideTau     = 0.115f;
    constexpr float kDragDecayTau     = 0.22f;
    constexpr float kRmsGain          = 3.2f;
    constexpr float kDragGain         = 0.55f;
    constexpr float kFineDragScale    = 0.22f;
    constexpr float kMaxFrameDelta    = 0.10f;   // seconds
    constexpr int   kMinFrameRate     = 30;
    constexpr int   kMaxFrameRate     = 60;
    constexpr float kAlphaFloor       = 0.004f;  // below this, skip the draw entirely

    /** One-pole smoothing coefficient for a given time constant. */
    inline float onePole (float dt, float tau) noexcept
    {
        if (tau <= 0.0f) return 1.0f;
        return 1.0f - std::exp (-dt / tau);
    }

    /** The device scale factor of the display this component is on. Used only
        as the initial guess; paint() corrects it from the real graphics context. */
    inline float displayScaleFor (juce::Component& c) noexcept
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();

        if (auto* d = displays.getDisplayForRect (c.getScreenBounds()))
            return (float) d->scale;

        if (auto* d = displays.getPrimaryDisplay())
            return (float) d->scale;

        return 1.0f;
    }
}

//==============================================================================
FluidField::FluidField (juce::AudioProcessorValueTreeState& apvts, VisualizationState& vis)
    : state (apvts),
      visual (vis),
      rng (juce::Random::getSystemRandom().nextInt64())
{
    setOpaque (true);
    setInterceptsMouseClicks (true, false);
    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    for (int i = 0; i < kAngleSteps; ++i)
    {
        const float a = math::twoPi * (float) i / (float) kAngleSteps;
        cosTable[(size_t) i] = std::cos (a);
        sinTable[(size_t) i] = std::sin (a);
    }

    paramX = state.getParameter (pid::fluidX);
    paramY = state.getParameter (pid::fluidY);

    if (paramX != nullptr)
    {
        defaultX = math::clamp (paramX->getDefaultValue(), 0.0f, 1.0f);
        targetX  = math::clamp (paramX->getValue(), 0.0f, 1.0f);
        attachX  = std::make_unique<juce::ParameterAttachment> (*paramX, [this] (float v)
        {
            if (paramX != nullptr)
                targetX = math::clamp (paramX->convertTo0to1 (v), 0.0f, 1.0f);
        });
    }

    if (paramY != nullptr)
    {
        defaultY = math::clamp (paramY->getDefaultValue(), 0.0f, 1.0f);
        targetY  = math::clamp (paramY->getValue(), 0.0f, 1.0f);
        attachY  = std::make_unique<juce::ParameterAttachment> (*paramY, [this] (float v)
        {
            if (paramY != nullptr)
                targetY = math::clamp (paramY->convertTo0to1 (v), 0.0f, 1.0f);
        });
    }

    nodeX = lastNodeX = targetX;
    nodeY = lastNodeY = targetY;

    for (auto& p : particles)
        seedParticle (p, true);

    // One allocation, here on the message thread: the ring path is cleared and
    // rebuilt every frame but never grows past this.
    ringPath.preallocateSpace ((kAngleSteps + 4) * 3);

    lastDropletCount = visual.getDropletCount();
    lastTickMs = juce::Time::getMillisecondCounterHiRes();

    updateTimerState();
}

FluidField::~FluidField()
{
    stopTimer();
}

//==============================================================================
void FluidField::seedParticle (Particle& p, bool anywhere)
{
    p.angle        = rng.nextFloat() * math::twoPi;
    p.radius       = anywhere ? rng.nextFloat()
                              : rng.nextFloat() * kParticleSeedInner;
    p.angVel       = math::lerp (kParticleAngMin, kParticleAngMax, rng.nextFloat())
                        * (rng.nextBool() ? 1.0f : -1.0f);
    p.breathePhase = rng.nextFloat() * math::twoPi;
    p.breatheRate  = math::lerp (kBreatheRateMin, kBreatheRateMax, rng.nextFloat());
    p.depth        = rng.nextFloat();
    p.size         = math::lerp (kParticleSizeMin, kParticleSizeMax, rng.nextFloat() * rng.nextFloat());
    p.twinklePhase = rng.nextFloat() * math::twoPi;
    p.twinkleRate  = math::lerp (kTwinkleRateMin, kTwinkleRateMax, rng.nextFloat());
    p.brightness   = math::lerp (kParticleBrightMin, kParticleBrightMax, rng.nextFloat());
}

//==============================================================================
void FluidField::resized()
{
    recomputeGeometry();
    rebuildBackdrop (backdropScale > 0.0f ? backdropScale : displayScaleFor (*this));
}

void FluidField::recomputeGeometry()
{
    const auto b = getLocalBounds().toFloat();

    if (b.getWidth() < 1.0f || b.getHeight() < 1.0f)
    {
        fieldArea = b;
        centreX = fieldCx = b.getCentreX();
        centreY = fieldCy = b.getCentreY();
        fieldRx = fieldRy = 1.0f;
        uiScale = 1.0f;
        return;
    }

    const auto gutterX = (float) RippleTheme::grid (kLabelSideSteps);
    const auto gutterY = (float) RippleTheme::grid (kLabelGutterSteps);

    fieldArea = b.reduced (juce::jmin (gutterX, b.getWidth() * 0.25f),
                           juce::jmin (gutterY, b.getHeight() * 0.25f));

    centreX = fieldArea.getCentreX();
    centreY = fieldArea.getCentreY();
    fieldCx = centreX;
    fieldCy = centreY;

    fieldRx = juce::jmax (1.0f, fieldArea.getWidth() * 0.5f);
    fieldRy = juce::jmax (1.0f, fieldArea.getHeight() * 0.5f);

    // Keep the pool reading as an ellipse seen slightly from above whatever
    // aspect ratio the host hands us.
    fieldRy = juce::jlimit (fieldRx * kFieldAspectMin, fieldRx * kFieldAspectMax, fieldRy);

    uiScale = juce::jlimit (kUiScaleMin, kUiScaleMax,
                            juce::jmin (fieldRx, fieldRy) * 2.0f
                                / (float) RippleTheme::grid (kScaleRefSteps));
}

void FluidField::rebuildBackdrop (float deviceScale)
{
    const auto& t = RippleTheme::get();

    backdropScale = math::clamp (deviceScale, kMinDeviceScale, kMaxDeviceScale);

    const int w = getWidth();
    const int h = getHeight();

    if (w <= 0 || h <= 0)
    {
        backdrop = juce::Image();
        return;
    }

    // Allocated at real device pixels so the cached layer stays crisp at
    // 125 / 150 / 200% scaling.
    const int pw = juce::jmax (1, juce::roundToInt ((float) w * backdropScale));
    const int ph = juce::jmax (1, juce::roundToInt ((float) h * backdropScale));

    // RGB rather than ARGB: the layer is opaque by construction, so the
    // per-frame blit is a straight copy with no alpha blending.
    backdrop = juce::Image (juce::Image::RGB, pw, ph, true);

    juce::Graphics g (backdrop);
    g.addTransform (juce::AffineTransform::scale ((float) pw / (float) w,
                                                  (float) ph / (float) h));

    const juce::Rectangle<float> bounds (0.0f, 0.0f, (float) w, (float) h);
    const auto clearCol = t.background.withAlpha (0.0f);

    g.setColour (t.background);
    g.fillRect (bounds);

    // ---- The pool ------------------------------------------------------
    // A single elliptical radial gradient carries the whole depth story:
    // a dark well at the centre, a band of scattered light around it, and a
    // darkening rim. Drawing it through a squashing transform makes the
    // circular gradient elliptical.
    {
        const float squash = juce::jlimit (kFieldAspectMin, kFieldAspectMax, fieldRy / fieldRx);
        const float radius = fieldRx * kPoolRadiusMul;

        juce::ColourGradient pool (t.backgroundDeep.withAlpha (kPoolWellAlpha), centreX, centreY,
                                   t.backgroundDeep.withAlpha (kPoolRimAlpha), centreX + radius, centreY,
                                   true);
        pool.addColour (kPoolStop1, t.background.withAlpha (kPoolStop1Alpha));
        pool.addColour (kPoolStop2, t.backgroundLift.withAlpha (kPoolLiftAlpha));
        pool.addColour (kPoolStop3, t.background.withAlpha (kPoolStop3Alpha));

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (1.0f, squash, centreX, centreY));
        g.setGradientFill (pool);
        g.fillRect (bounds.expanded (bounds.getWidth(), bounds.getHeight() * kCoverExpand));
        g.restoreState();
    }

    // ---- Vignette -------------------------------------------------------
    // Sits the field INTO the interface instead of pasting it on top.
    {
        const float squash = juce::jmax (kFieldAspectMin, bounds.getHeight() / bounds.getWidth());
        const float radius = bounds.getWidth() * kVignetteRadiusMul;
        const float vx = bounds.getCentreX();
        const float vy = bounds.getCentreY();

        juce::ColourGradient vig (clearCol, vx, vy,
                                  t.backgroundDeep.withAlpha (kVignetteEdgeAlpha), vx + radius, vy,
                                  true);
        vig.addColour (kVignetteStop, t.backgroundDeep.withAlpha (kVignetteMidAlpha));

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (1.0f, squash, vx, vy));
        g.setGradientFill (vig);
        g.fillRect (bounds.expanded (bounds.getWidth(), bounds.getHeight() * kCoverExpand));
        g.restoreState();
    }

    // ---- Rim and surface sheen -----------------------------------------
    {
        g.setColour (t.panelBorderSoft.withMultipliedAlpha (kRimAlpha));
        g.drawEllipse (centreX - fieldRx, centreY - fieldRy,
                       fieldRx * 2.0f, fieldRy * 2.0f, t.borderWidth);

        juce::Path sheen;
        sheen.addCentredArc (centreX, centreY, fieldRx * kSheenRadius, fieldRy * kSheenRadius,
                             0.0f, -kSheenSweep, kSheenSweep, true);

        g.setColour (t.panelHighlight.withMultipliedAlpha (kSheenAlpha));
        g.strokePath (sheen, juce::PathStrokeType (t.borderWidth * kSheenWidth,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }
}

//==============================================================================
void FluidField::visibilityChanged()      { updateTimerState(); }
void FluidField::parentHierarchyChanged() { updateTimerState(); }

void FluidField::updateTimerState()
{
    const bool shouldRun = isVisible() && (getParentComponent() == nullptr || isShowing());

    if (shouldRun)
    {
        if (! isTimerRunning())
        {
            lastTickMs = juce::Time::getMillisecondCounterHiRes();
            startTimerHz (juce::jlimit (kMinFrameRate, kMaxFrameRate,
                                        RippleTheme::get().targetFrameRate));
        }
    }
    else if (isTimerRunning())
    {
        stopTimer();   // hidden: idle CPU drops to zero
    }
}

void FluidField::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    float dt = (float) ((now - lastTickMs) * 0.001);
    lastTickMs = now;

    if (! std::isfinite (dt) || dt <= 0.0f)
        dt = 1.0f / (float) juce::jmax (1, RippleTheme::get().targetFrameRate);

    advance (juce::jmin (dt, kMaxFrameDelta));
    repaint();
}

//==============================================================================
void FluidField::pollParameters()
{
    // Polling as well as listening means a fast automation ramp is never missed
    // and the field always agrees with the host.
    if (paramX != nullptr) targetX = math::clamp (paramX->getValue(), 0.0f, 1.0f);
    if (paramY != nullptr) targetY = math::clamp (paramY->getValue(), 0.0f, 1.0f);
}

void FluidField::advance (float dt)
{
    const auto& t = RippleTheme::get();

    // Two independent wrapped phases rather than a free-running clock: the
    // drift stays continuous for as long as the plug-in is open.
    const float driftRate = math::twoPi * t.ambientMotionRate * dt;

    driftPhaseA += driftRate;
    if (driftPhaseA >= math::twoPi) driftPhaseA -= math::twoPi;

    driftPhaseB += driftRate * kDriftRateRatio;
    if (driftPhaseB >= math::twoPi) driftPhaseB -= math::twoPi;

    if (! dragging)
        pollParameters();

    // ---- Mood, all smoothed so nothing in the picture can snap ----------
    const float moodC = onePole (dt, kMoodTau);
    glowSmoothed   += (math::clamp (visual.getGlow(),   0.0f, 1.0f) - glowSmoothed)   * moodC;
    depthSmoothed  += (math::clamp (visual.getDepth(),  0.0f, 1.0f) - depthSmoothed)  * moodC;
    motionSmoothed += (math::clamp (visual.getMotion(), 0.0f, 1.0f) - motionSmoothed) * moodC;

    const float rmsTarget = math::clamp (visual.getOutputRMS() * kRmsGain, 0.0f, 1.0f);
    rmsSmoothed += (rmsTarget - rmsSmoothed) * onePole (dt, kRmsTau);
    energy = math::smoothstep (rmsSmoothed);

    currentSmoothed += (math::clamp (visual.getCurrentValue(), -1.0f, 1.0f) - currentSmoothed)
                           * onePole (dt, kCurrentTau);

    // ---- Ring phases ----------------------------------------------------
    const float rateScale = 1.0f + kMotionRateMul * motionSmoothed;
    for (int k = 0; k < kHarmonics; ++k)
    {
        float& ph = harmonicPhase[(size_t) k];
        ph += math::twoPi * kHarmonicRate[k] * rateScale * dt;
        if (ph > math::twoPi) ph -= math::twoPi;
    }

    // ---- Slow lateral movement of the whole field -----------------------
    fieldCx = centreX + fieldRx * (kCurrentDriftX * currentSmoothed
                                       + kAmbientDriftX * std::sin (driftPhaseA));
    fieldCy = centreY + fieldRy * (kCurrentDriftY * currentSmoothed
                                       + kAmbientDriftY * std::cos (driftPhaseB));

    // ---- Node ------------------------------------------------------------
    const float nodeC = onePole (dt, dragging ? kNodeDragTau : kNodeGlideTau);
    nodeX += (targetX - nodeX) * nodeC;
    nodeY += (targetY - nodeY) * nodeC;

    const float invDt = 1.0f / juce::jmax (dt, 1.0e-3f);
    const float velX = math::clamp ((nodeX - lastNodeX) * invDt * kDragGain, -1.0f, 1.0f);
    const float velY = math::clamp ((nodeY - lastNodeY) * invDt * kDragGain, -1.0f, 1.0f);
    lastNodeX = nodeX;
    lastNodeY = nodeY;

    const float dragC = onePole (dt, kDragDecayTau);
    dragVecX += (velX - dragVecX) * dragC;
    dragVecY += (velY - dragVecY) * dragC;

    hoverAmount += (((hoveringNode || dragging) ? 1.0f : 0.0f) - hoverAmount)
                       * onePole (dt, t.hoverFadeSeconds);

    // ---- Audio events ----------------------------------------------------
    consumeAudioEvents();

    // ---- Particles -------------------------------------------------------
    const float swirl = (1.0f + kChaosSwirl * nodeX) * (1.0f + kMotionSwirl * motionSmoothed);

    for (auto& p : particles)
    {
        p.angle += p.angVel * swirl * dt;
        if (p.angle >= math::twoPi)     p.angle -= math::twoPi;
        else if (p.angle < 0.0f)        p.angle += math::twoPi;

        p.breathePhase += p.breatheRate * dt;
        if (p.breathePhase >= math::twoPi) p.breathePhase -= math::twoPi;

        p.twinklePhase += p.twinkleRate * dt;
        if (p.twinklePhase >= math::twoPi) p.twinklePhase -= math::twoPi;

        // A very slow upwelling: motes rise out of the well and are recycled
        // back into it, so the pool always has a current running through it.
        p.radius += kParticleRise * dt * (0.45f + 0.55f * (1.0f - p.depth));

        if (p.radius > 1.0f)
            seedParticle (p, false);
    }

    // ---- Ripple pool -----------------------------------------------------
    for (auto& r : ripples)
    {
        if (! r.active)
            continue;

        r.age += dt;
        if (r.age >= r.life)
            r.active = false;
    }
}

void FluidField::consumeAudioEvents()
{
    // Stack storage, drained once per frame — nothing is allocated here.
    VisualizationState::NoteEvent events[VisualizationState::kEventCapacity];
    const int numEvents = visual.drainNoteEvents (events, VisualizationState::kEventCapacity);

    const float nodeOx = (nodeX - 0.5f) * 2.0f * kNodeTravel;
    const float nodeOy = (nodeY - 0.5f) * 2.0f * kNodeTravel;

    for (int i = 0; i < numEvents; ++i)
    {
        if (! events[i].isNoteOn)
            continue;

        const float vel   = math::clamp (events[i].velocity, 0.0f, 1.0f);
        const float pitch = math::clamp (events[i].pitch, 0.0f, 1.0f);

        // Origin follows the node but is offset by pitch: low notes spawn to
        // the left and deeper, high notes to the right and nearer the surface.
        const float ox = math::clamp (nodeOx * kRippleNodeBias + (pitch - 0.5f) * 2.0f * kPitchSpreadX,
                                      -kRippleOriginLimit, kRippleOriginLimit);
        const float oy = math::clamp (nodeOy * kRippleNodeBias - (pitch - 0.5f) * kPitchLiftY,
                                      -kRippleOriginLimit, kRippleOriginLimit);

        spawnRipple (ox, oy, vel, false);
    }

    const uint32_t drops = visual.getDropletCount();

    if (drops != lastDropletCount)
    {
        const uint32_t pending = juce::jmin (kMaxDropletsPerFrame, drops - lastDropletCount);
        lastDropletCount = drops;

        const float intensity = math::clamp (visual.getLastDropletIntensity(), 0.0f, 1.0f);
        const float pan       = math::clamp (visual.getLastDropletPan(), -1.0f, 1.0f);

        for (uint32_t i = 0; i < pending; ++i)
        {
            const float jx = (rng.nextFloat() - 0.5f) * kDropletJitter;
            const float jy = (rng.nextFloat() - 0.5f) * kDropletYSpread;

            spawnRipple (math::clamp (pan * kDropletPanSpread + jx, -kRippleOriginLimit, kRippleOriginLimit),
                         math::clamp (jy, -kRippleOriginLimit, kRippleOriginLimit),
                         intensity, true);
        }
    }
}

void FluidField::spawnRipple (float ox, float oy, float intensity, bool droplet)
{
    // Fixed pool: take the first free slot, otherwise recycle round-robin.
    int slot = nextRippleSlot;

    for (int i = 0; i < kRippleCount; ++i)
    {
        const int idx = (nextRippleSlot + i) % kRippleCount;

        if (! ripples[(size_t) idx].active)
        {
            slot = idx;
            break;
        }
    }

    nextRippleSlot = (slot + 1) % kRippleCount;

    auto& r = ripples[(size_t) slot];
    r.active    = true;
    r.isDroplet = droplet;
    r.age       = 0.0f;
    r.originX   = ox;
    r.originY   = oy;
    r.intensity = math::clamp (intensity, 0.0f, 1.0f);
    r.life      = droplet ? kDropletLife : (kNoteLifeBase + kNoteLifeVel * r.intensity);
    r.spread    = droplet ? (kDropletSpreadBase + kDropletSpreadInt * r.intensity)
                          : (kNoteSpreadBase + kNoteSpreadVel * r.intensity);
}

//==============================================================================
juce::Point<float> FluidField::nodeToPixels (float nx, float ny) const noexcept
{
    return { fieldCx + (nx - 0.5f) * 2.0f * fieldRx * kNodeTravel,
             fieldCy + (ny - 0.5f) * 2.0f * fieldRy * kNodeTravel };
}

void FluidField::setNormalisedParameters (float nx, float ny, bool partOfGesture)
{
    nx = math::clamp (nx, 0.0f, 1.0f);
    ny = math::clamp (ny, 0.0f, 1.0f);

    targetX = nx;
    targetY = ny;

    if (paramX != nullptr && attachX != nullptr)
    {
        const float denorm = paramX->convertFrom0to1 (nx);

        if (partOfGesture) attachX->setValueAsPartOfGesture (denorm);
        else               attachX->setValueAsCompleteGesture (denorm);
    }

    if (paramY != nullptr && attachY != nullptr)
    {
        const float denorm = paramY->convertFrom0to1 (ny);

        if (partOfGesture) attachY->setValueAsPartOfGesture (denorm);
        else               attachY->setValueAsCompleteGesture (denorm);
    }
}

//==============================================================================
void FluidField::mouseDown (const juce::MouseEvent& e)
{
    const auto node = nodeToPixels (nodeX, nodeY);
    const float grabR = (float) RippleTheme::grid (kNodeRadiusSteps) * uiScale * kNodeGrabMul;

    float startX = targetX;
    float startY = targetY;

    // Grabbing the node keeps its offset; clicking open water pulls it over.
    if (! e.mods.isShiftDown() && e.position.getDistanceFrom (node) > grabR)
    {
        startX = math::clamp (0.5f + (e.position.x - fieldCx) / (2.0f * fieldRx * kNodeTravel), 0.0f, 1.0f);
        startY = math::clamp (0.5f + (e.position.y - fieldCy) / (2.0f * fieldRy * kNodeTravel), 0.0f, 1.0f);
    }

    grabMouse = e.position;
    grabX = startX;
    grabY = startY;
    dragging = true;
    fineDrag = e.mods.isShiftDown();
    hoveringNode = true;

    if (! gestureOpen)
    {
        gestureOpen = true;
        if (attachX != nullptr) attachX->beginGesture();
        if (attachY != nullptr) attachY->beginGesture();
    }

    setNormalisedParameters (startX, startY, true);
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void FluidField::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    // Re-anchor when Shift is pressed or released so the node never jumps.
    if (e.mods.isShiftDown() != fineDrag)
    {
        fineDrag = e.mods.isShiftDown();
        grabMouse = e.position;
        grabX = targetX;
        grabY = targetY;
    }

    const float fine = fineDrag ? kFineDragScale : 1.0f;
    const float dx = (e.position.x - grabMouse.x) * fine / (2.0f * fieldRx * kNodeTravel);
    const float dy = (e.position.y - grabMouse.y) * fine / (2.0f * fieldRy * kNodeTravel);

    setNormalisedParameters (grabX + dx, grabY + dy, true);
}

void FluidField::mouseUp (const juce::MouseEvent& e)
{
    dragging = false;

    if (gestureOpen)
    {
        gestureOpen = false;
        if (attachX != nullptr) attachX->endGesture();
        if (attachY != nullptr) attachY->endGesture();
    }

    const auto node = nodeToPixels (nodeX, nodeY);
    const float grabR = (float) RippleTheme::grid (kNodeRadiusSteps) * uiScale * kNodeGrabMul;
    hoveringNode = e.position.getDistanceFrom (node) <= grabR;

    setMouseCursor (hoveringNode ? juce::MouseCursor::DraggingHandCursor
                                 : juce::MouseCursor::PointingHandCursor);
}

void FluidField::mouseMove (const juce::MouseEvent& e)
{
    const auto node = nodeToPixels (nodeX, nodeY);
    const float grabR = (float) RippleTheme::grid (kNodeRadiusSteps) * uiScale * kNodeGrabMul;
    const bool over = e.position.getDistanceFrom (node) <= grabR;

    if (over != hoveringNode)
    {
        hoveringNode = over;
        setMouseCursor (over ? juce::MouseCursor::DraggingHandCursor
                             : juce::MouseCursor::PointingHandCursor);
    }
}

void FluidField::mouseExit (const juce::MouseEvent&)
{
    hoveringNode = false;
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FluidField::mouseDoubleClick (const juce::MouseEvent&)
{
    // The gesture opened by the preceding mouseDown is still open, so this is
    // sent as part of it and closed by the matching mouseUp — never nested.
    dragging = false;
    setNormalisedParameters (defaultX, defaultY, gestureOpen);

    // A soft acknowledgement ripple from the node.
    spawnRipple ((defaultX - 0.5f) * 2.0f * kNodeTravel,
                 (defaultY - 0.5f) * 2.0f * kNodeTravel,
                 0.45f, false);
}

//==============================================================================
void FluidField::paint (juce::Graphics& g)
{
    const auto& t = RippleTheme::get();

    // The cached layer is rebuilt only when the size or the device scale
    // actually changes — never on an ordinary frame.
    const float deviceScale = math::clamp (g.getInternalContext().getPhysicalPixelScaleFactor(),
                                           kMinDeviceScale, kMaxDeviceScale);

    const int wantedW = juce::jmax (1, juce::roundToInt ((float) getWidth() * deviceScale));

    if (backdrop.isNull()
         || std::abs (deviceScale - backdropScale) > kScaleEpsilon
         || backdrop.getWidth() != wantedW)
    {
        rebuildBackdrop (deviceScale);
    }

    if (backdrop.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (backdrop, getLocalBounds().toFloat());
    }
    else
    {
        g.fillAll (t.background);
    }

    paintDepthWell (g);
    paintRings (g);
    paintRipples (g);
    paintParticles (g);
    paintGuides (g);
    paintNode (g);
    paintLabels (g);
}

void FluidField::paintDepthWell (juce::Graphics& g) const
{
    if (depthSmoothed <= 0.01f)
        return;

    const auto& t = RippleTheme::get();
    const auto node = nodeToPixels (nodeX, nodeY);
    const float aspect = fieldRy / fieldRx;

    // Stacked soft ellipses instead of a per-frame gradient: the water under
    // the node gets darker as DEPTH rises.
    for (int i = 0; i < kWellSteps; ++i)
    {
        const float f = (float) (i + 1) / (float) kWellSteps;
        const float r = fieldRx * (kWellRadiusBase + kWellRadiusSpan * f);
        const float a = kWellAlpha * depthSmoothed * (1.0f - f);

        if (a < kAlphaFloor)
            continue;

        g.setColour (t.backgroundDeep.withAlpha (a));
        g.fillEllipse (node.x - r, node.y - r * aspect, r * 2.0f, r * 2.0f * aspect);
    }
}

void FluidField::paintRings (juce::Graphics& g)
{
    const auto& t = RippleTheme::get();

    const float chaos = nodeX;
    const float depth = nodeY;

    const float wobble = kWobbleBase * (1.0f + kChaosWobbleMul * chaos)
                                     * (1.0f + kMotionWobbleMul * motionSmoothed);

    const float ringScale  = math::lerp (kRingScaleSurface, kRingScaleDepth, depth);
    const float ringSquash = math::lerp (kRingSquashSurface, kRingSquashDepth, depth);

    const auto node = nodeToPixels (nodeX, nodeY);
    const float nodeDx = node.x - fieldCx;
    const float nodeDy = node.y - fieldCy;

    const float edgeGlow = 0.42f + 0.95f * glowSmoothed;
    const float depthDim = 1.0f - kRingDepthDim * depthSmoothed;
    const float energyLift = 0.78f + kRingEnergyLift * energy;

    float amp[kHarmonics];
    float cosPhi[kHarmonics];
    float sinPhi[kHarmonics];

    for (int i = 0; i < kRingCount; ++i)
    {
        const float tt = (float) (i + 1) / (float) kRingCount;      // 0 (inner) .. 1 (outer)
        const float rNorm = math::lerp (kRingInnerRadius, 1.0f, tt);

        // Inner rings sit on the node, outer rings stay with the field: the
        // whole surface compresses on one side and stretches on the other.
        const float follow = std::pow (1.0f - tt, kRingFollowFall) * kRingFollow;
        const float cx = fieldCx + nodeDx * follow;
        const float cy = fieldCy + nodeDy * follow;

        const float rx = fieldRx * kRingFit * rNorm * ringScale;
        const float ry = fieldRy * kRingFit * rNorm * ringScale * ringSquash;

        const float ampScale = wobble * math::lerp (kRingAmpNear, kRingAmpFar, tt);

        for (int k = 0; k < kHarmonics; ++k)
        {
            const float phase = harmonicPhase[(size_t) k] + tt * kHarmonicSpatial[k];
            cosPhi[k] = std::cos (phase);
            sinPhi[k] = std::sin (phase);
            amp[k]    = kHarmonicAmp[k] * ampScale;
        }

        const float bulge = kDragBulge * (1.0f - 0.5f * tt);

        ringPath.clear();

        for (int s = 0; s <= kAngleSteps; ++s)
        {
            const int idx = s & kAngleMask;
            const float cA = cosTable[(size_t) idx];
            const float sA = sinTable[(size_t) idx];

            // Sum of slow sine terms. sin(n*a + phi) is expanded so the angular
            // part is a table lookup and only the phase needs real trig,
            // once per ring rather than once per point.
            float u = 0.0f;

            for (int k = 0; k < kHarmonics; ++k)
            {
                const int hi = (kHarmonicN[k] * s) & kAngleMask;
                u += amp[k] * (sinTable[(size_t) hi] * cosPhi[k]
                             + cosTable[(size_t) hi] * sinPhi[k]);
            }

            // Liquid bulge toward the direction the node is being dragged.
            u += bulge * (cA * dragVecX + sA * dragVecY);

            // Hard guarantee that the ring stays inside the field.
            const float rr = 1.0f + math::clamp (u, -kRingPerturbLimit, kRingPerturbLimit);
            const float x = cx + rx * rr * cA;
            const float y = cy + ry * rr * sA;

            if (s == 0) ringPath.startNewSubPath (x, y);
            else        ringPath.lineTo (x, y);
        }

        ringPath.closeSubPath();

        float alpha = math::lerp (kRingAlphaNear, kRingAlphaFar, tt) * edgeGlow * depthDim * energyLift;
        alpha = math::clamp (alpha, 0.0f, kRingAlphaCeiling);

        if (alpha < kAlphaFloor)
            continue;

        juce::Colour base = t.cyan.interpolatedWith (t.aqua, 0.20f + 0.55f * tt);
        base = base.interpolatedWith (t.violet,     kRingVioletTint * depthSmoothed);
        base = base.interpolatedWith (t.cyanBright, kRingGlowTint * glowSmoothed);

        const float width = math::lerp (kRingWidthNear, kRingWidthFar, tt) * uiScale;

        // Fake bloom: a wide, faint pass under a crisp one. No image filters.
        if (tt <= kSoftPassLimit)
        {
            g.setColour (base.withAlpha (alpha * kSoftPassAlpha));
            g.strokePath (ringPath, juce::PathStrokeType (width * kSoftPassWidth));
        }

        g.setColour (base.withAlpha (alpha));
        g.strokePath (ringPath, juce::PathStrokeType (width));
    }
}

void FluidField::paintRipples (juce::Graphics& g) const
{
    const auto& t = RippleTheme::get();
    const float glowLift = 0.5f + 0.8f * glowSmoothed;

    for (const auto& r : ripples)
    {
        if (! r.active)
            continue;

        const float x = math::clamp (r.age / r.life, 0.0f, 1.0f);
        const float ease = 1.0f - (1.0f - x) * (1.0f - x);     // quick out, slow settle
        const float fade = std::pow (1.0f - x, kRippleFadePow);

        float alpha = fade * r.intensity * (r.isDroplet ? kDropletAlpha : kNoteAlpha) * glowLift;

        // Fade a pulse out as it approaches the rim, so it dissolves into the
        // vignette instead of being cut off by the component edge.
        const float reach = ease * r.spread
                              + juce::jmax (std::abs (r.originX), std::abs (r.originY));
        alpha *= 1.0f - math::smoothstep ((reach - kRippleRimFadeStart)
                                              / (kRippleRimFadeEnd - kRippleRimFadeStart));
        alpha = math::clamp (alpha, 0.0f, 0.6f);

        if (alpha < kAlphaFloor)
            continue;

        const float rx = fieldRx * r.spread * ease;
        const float ry = fieldRy * r.spread * ease;
        const float cx = fieldCx + fieldRx * r.originX;
        const float cy = fieldCy + fieldRy * r.originY;

        const juce::Rectangle<float> area (cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
        const juce::Colour c = r.isDroplet ? t.cyanBright
                                           : t.cyan.interpolatedWith (t.aqua, 0.35f);

        g.setColour (c.withAlpha (alpha * 0.16f));
        g.drawEllipse (area, kRippleWidthWide * uiScale);
        g.setColour (c.withAlpha (alpha * 0.42f));
        g.drawEllipse (area, kRippleWidthMid * uiScale);
        g.setColour (c.withAlpha (alpha));
        g.drawEllipse (area, kRippleWidthThin * uiScale);

        if (r.isDroplet)
        {
            // The impact point itself: a tiny flash that snaps out fast.
            const float flash = std::pow (1.0f - x, kDropletFlashPow) * r.intensity;
            const float pr = kDropletFlashR * uiScale * (0.6f + 0.8f * r.intensity);

            if (flash > kAlphaFloor)
            {
                g.setColour (t.cyanBright.withAlpha (math::clamp (flash, 0.0f, 0.85f)));
                g.fillEllipse (cx - pr, cy - pr, pr * 2.0f, pr * 2.0f);
            }
        }
    }
}

void FluidField::paintParticles (juce::Graphics& g) const
{
    const auto& t = RippleTheme::get();

    const float glowLift = 0.55f + 0.70f * glowSmoothed;
    const float depthDim = 1.0f - kParticleDepthDim * depthSmoothed;
    const float shiftX = dragVecX * kParticleDragShift * fieldRx;
    const float shiftY = dragVecY * kParticleDragShift * fieldRy;

    for (const auto& p : particles)
    {
        const float rad = p.radius * (1.0f + kParticleBreathe * std::sin (p.breathePhase));

        if (rad > 1.02f)
            continue;

        // Parallax: shallow motes swing wider and are pushed further by a drag.
        const float parallax = math::lerp (kParallaxNear, kParallaxFar, p.depth);
        const float shallowness = 1.0f - p.depth;

        const float x = fieldCx + fieldRx * rad * std::cos (p.angle) * parallax + shiftX * shallowness;
        const float y = fieldCy + fieldRy * rad * std::sin (p.angle) * parallax * kParticleFlatten
                                + shiftY * shallowness;

        const float twinkle = kTwinkleBase + kTwinkleDepth * std::sin (p.twinklePhase);

        float alpha = p.brightness * twinkle * math::lerp (1.0f, kParticleDepthFade, p.depth)
                        * glowLift * depthDim;

        // Fade out toward the rim so nothing ever pops in or out.
        alpha *= 1.0f - math::smoothstep ((rad - kParticleFadeStart) / (1.0f - kParticleFadeStart));
        alpha = math::clamp (alpha, 0.0f, 0.8f);

        if (alpha < kAlphaFloor)
            continue;

        const float sz = p.size * uiScale * math::lerp (1.0f, kParticleDepthShrink, p.depth);
        const juce::Colour c = t.cyanBright.interpolatedWith (t.aqua, p.depth);

        const float hr = sz * kParticleHaloMul;
        g.setColour (c.withAlpha (alpha * kParticleHaloAlpha));
        g.fillEllipse (x - hr, y - hr, hr * 2.0f, hr * 2.0f);

        g.setColour (c.withAlpha (alpha));
        g.fillEllipse (x - sz, y - sz, sz * 2.0f, sz * 2.0f);
    }
}

void FluidField::paintGuides (juce::Graphics& g) const
{
    const auto& t = RippleTheme::get();

    const float baseAlpha = kGuideAlpha * (kGuideIdle + (1.0f - kGuideIdle) * hoverAmount);

    if (baseAlpha < kAlphaFloor)
        return;

    const auto node = nodeToPixels (nodeX, nodeY);
    const float thick = t.borderWidth;
    const float half = thick * 0.5f;

    // Four arms, each fading out in steps — no gradient objects per frame.
    struct Arm { float dx, dy; };
    const Arm arms[4] = { { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } };

    for (const auto& arm : arms)
    {
        const float len = (arm.dx != 0.0f) ? fieldRx : fieldRy;

        for (int s = 0; s < kGuideSegs; ++s)
        {
            const float f0 = (float) s / (float) kGuideSegs;
            const float f1 = (float) (s + 1) / (float) kGuideSegs;
            const float a = baseAlpha * (1.0f - f1) * (1.0f - f1);

            if (a < kAlphaFloor)
                continue;

            g.setColour (t.cyanDim.withAlpha (a));

            if (arm.dx != 0.0f)
            {
                const float x0 = node.x + arm.dx * len * f0;
                const float x1 = node.x + arm.dx * len * f1;
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (x0, x1), node.y - half,
                                                                        juce::jmax (x0, x1), node.y + half));
            }
            else
            {
                const float y0 = node.y + arm.dy * len * f0;
                const float y1 = node.y + arm.dy * len * f1;
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (node.x - half, juce::jmin (y0, y1),
                                                                        node.x + half, juce::jmax (y0, y1)));
            }
        }
    }

    // Axis ticks on the rim of the field: a quiet readout of the node position.
    const float tick = (float) RippleTheme::grid (kGuideTickSteps) * uiScale;
    const float tickAlpha = math::clamp (baseAlpha * kGuideTickAlpha, 0.0f, 0.75f);

    g.setColour (t.cyan.withAlpha (tickAlpha));
    g.fillRect (node.x - half, fieldCy - fieldRy,        thick, tick);
    g.fillRect (node.x - half, fieldCy + fieldRy - tick, thick, tick);
    g.fillRect (fieldCx - fieldRx,        node.y - half, tick, thick);
    g.fillRect (fieldCx + fieldRx - tick, node.y - half, tick, thick);
}

void FluidField::paintNode (juce::Graphics& g) const
{
    const auto& t = RippleTheme::get();

    const auto node = nodeToPixels (nodeX, nodeY);
    const float r = (float) RippleTheme::grid (kNodeRadiusSteps) * uiScale
                        * (1.0f + kNodeHoverGrow * hoverAmount);

    // ---- Soft radial bloom, faked with stacked fills --------------------
    const float bloomR = r * kNodeBloomMul
                            * (1.0f + kNodeBloomEnergy * energy)
                            * (0.80f + 0.55f * glowSmoothed);

    for (int i = kBloomSteps; i >= 1; --i)
    {
        const float f = (float) i / (float) kBloomSteps;
        const float rr = bloomR * f;
        const float a = math::clamp (kNodeBloomAlpha * (1.0f - f) * (1.0f - f)
                                        * (0.55f + t.glowAmount + 0.75f * glowSmoothed),
                                     0.0f, 1.0f);

        if (a < kAlphaFloor)
            continue;

        g.setColour (t.glowCore.withMultipliedAlpha (a));
        g.fillEllipse (node.x - rr, node.y - rr * kNodeBloomFlatten,
                       rr * 2.0f, rr * 2.0f * kNodeBloomFlatten);
    }

    // ---- Halo: a few stroked passes of decreasing alpha, increasing width
    const int layers = juce::jmax (1, t.shadowLayers);

    for (int i = 0; i < layers; ++i)
    {
        const float f = (float) (i + 1) / (float) layers;
        const float rr = r * (1.0f + kNodeHaloSpread * f);
        const float a = math::clamp (kNodeHaloAlpha * (1.0f - f) * (0.55f + 0.70f * glowSmoothed),
                                     0.0f, 1.0f);

        if (a < kAlphaFloor)
            continue;

        g.setColour (t.cyan.withAlpha (a));
        g.drawEllipse (node.x - rr, node.y - rr, rr * 2.0f, rr * 2.0f,
                       t.borderWidth * (1.0f + f * 2.0f) * uiScale);
    }

    // ---- The sphere itself ---------------------------------------------
    {
        const float ox = node.x - r * kNodeShadeOffset;
        const float oy = node.y - r * kNodeShadeOffset;

        juce::ColourGradient body (t.cyanBright, ox, oy,
                                   t.cyanDim.darker (0.45f), ox + r * kNodeSphereGradR, oy,
                                   true);
        body.addColour (0.42, t.cyan);

        g.setGradientFill (body);
        g.fillEllipse (node.x - r, node.y - r, r * 2.0f, r * 2.0f);
    }

    g.setColour (t.cyanBright.withAlpha (kNodeRimAlpha));
    g.drawEllipse (node.x - r, node.y - r, r * 2.0f, r * 2.0f, t.borderWidth * uiScale);

    // ---- Crisp core and a single specular point ------------------------
    const float cr = r * kNodeCoreRatio;
    g.setColour (t.knobIndicator.withAlpha (math::clamp (kNodeCoreAlpha + kNodeCoreEnergy * energy,
                                                         0.0f, 1.0f)));
    g.fillEllipse (node.x - cr, node.y - cr, cr * 2.0f, cr * 2.0f);

    const float sr = r * kNodeSpecRatio;
    const float sx = node.x - r * 0.38f;
    const float sy = node.y - r * 0.42f;
    g.setColour (t.knobIndicator.withAlpha (kNodeSpecAlpha));
    g.fillEllipse (sx - sr, sy - sr, sr * 2.0f, sr * 2.0f);
}

void FluidField::paintLabels (juce::Graphics& g) const
{
    const auto& t = RippleTheme::get();

    const auto b = getLocalBounds();
    const int gy = RippleTheme::grid (kLabelGutterSteps);
    const int gx = RippleTheme::grid (kLabelSideSteps);
    const int pad = RippleTheme::xs;

    // The label nearest the node lifts from tertiary toward secondary text —
    // just enough to read the position without drawing the eye.
    const auto lit = [&t] (float proximity)
    {
        const float p = math::smoothstep (math::clamp (proximity, 0.0f, 1.0f));
        return t.tertiaryText.interpolatedWith (t.secondaryText, p * p);
    };

    g.setFont (t.smallFont());

    const auto topRow    = juce::Rectangle<int> (b.getX(), b.getY(), b.getWidth(), gy);
    const auto bottomRow = juce::Rectangle<int> (b.getX(), b.getBottom() - gy, b.getWidth(), gy);
    const auto leftCol   = juce::Rectangle<int> (b.getX() + pad, b.getCentreY() - gy / 2, gx, gy);
    const auto rightCol  = juce::Rectangle<int> (b.getRight() - gx - pad, b.getCentreY() - gy / 2, gx, gy);

    g.setColour (lit (1.0f - nodeY));
    g.drawText ("SURFACE", topRow, juce::Justification::centred, false);

    g.setColour (lit (nodeY));
    g.drawText ("DEPTH", bottomRow, juce::Justification::centred, false);

    g.setColour (lit (1.0f - nodeX));
    g.drawText ("CALM", leftCol, juce::Justification::centredLeft, false);

    g.setColour (lit (nodeX));
    g.drawText ("CHAOS", rightCol, juce::Justification::centredRight, false);
}

} // namespace ripples
