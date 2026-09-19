#include "Presets/PresetAuthoring.h"
#include "Presets/Banks.h"

/*
    RIPPLES — the THE SURFACE bank.

    See Presets/PresetAuthoring.h for the unit helpers and the make() form, and
    docs/PRESET_GUIDE.md for what makes a preset in this bank good.
*/

namespace ripples::factory
{

//==============================================================================
//  THE SURFACE — light on water. Bright, airy, the air/water boundary.
//==============================================================================
void addSurfaceBank (std::vector<Preset>& out)
{
    out.push_back (make (kSurface, "INIT DEEP SAW", "Lead", { "Deep", "Calm" }, Dry::No,
        "The honest starting point, and identical to the plug-in's own defaults: "
        "two saws seven cents apart, a sine sub twelve down, a 24 dB ladder at "
        "1.2 kHz opening under the mod envelope. Every water extra is at zero so "
        "the raw voice can be judged on its own.",
    {
        // Deliberately empty: INIT DEEP SAW *is* the baseline. Changing it means
        // changing initValues() above, which mirrors ParameterLayout.cpp.
    }));

    // --- DRY AQUATIC #1 ----------------------------------------------------
    out.push_back (make (kSurface, "SURFACE", "Key", { "Bright", "Surface", "Calm" }, Dry::Yes,
        "Sunlight broken on a moving surface. Glass partials, a thread of air "
        "noise and a bright scattered resonator. No effects at all.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.62f },
        { pid::oscAUnison,    uni (3) },
        { pid::oscADetune,    0.10f },
        { pid::oscAStereo,    0.75f },
        { pid::oscALevel,     lvl (-3.0f) },

        { pid::oscBWave,      wv (OscWave::Triangle) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBFine,      cents (4.0f) },
        { pid::oscBShape,     0.40f },
        { pid::oscBUnison,    uni (2) },
        { pid::oscBDetune,    0.08f },
        { pid::oscBStereo,    0.65f },
        { pid::oscBLevel,     lvl (-11.0f) },

        { pid::subLevel,      lvl (-24.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-27.0f) },
        { pid::noiseTone,     0.78f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (5200.0f) },
        { pid::filtReso,      0.30f },
        { pid::filtDrive,     0.08f },
        { pid::filtKeyTrack,  0.80f },
        { pid::filtEnvAmt,    bip (0.22f) },
        { pid::filtPressure,  0.15f },

        { pid::fenvAttack,    att (0.020f) },
        { pid::fenvDecay,     dec (1.60f) },
        { pid::fenvSustain,   0.40f },
        { pid::fenvRelease,   rel (1.80f) },
        { pid::fenvVelocity,  0.45f },

        { pid::aenvAttack,    att (0.030f) },
        { pid::aenvDecay,     dec (2.20f) },
        { pid::aenvSustain,   0.62f },
        { pid::aenvRelease,   rel (2.40f) },
        { pid::aenvVelocity,  0.55f },

        { pid::macroDepth,    0.20f },
        { pid::macroWet,      0.45f },
        { pid::macroRipple,   0.35f },
        { pid::macroCurrent,  0.40f },
        { pid::macroDrops,    0.30f },
        { pid::macroGlow,     0.70f },
        { pid::fluidX,        bip (-0.25f) },
        { pid::fluidY,        bip (-0.55f) },
        { pid::fluidXAmount,  0.45f },
        { pid::fluidYAmount,  0.60f },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.55f) },
        { pid::tideDepth,     0.34f },
        { pid::tideStereo,    0.60f },
        { pid::currRate,      currHz (0.70f) },
        { pid::currAmount,    0.42f },
        { pid::currSmooth,    0.55f },
        { pid::currDrift,     0.25f },
        { pid::currStereo,    0.80f },
        { pid::driftRate,     driftHz (0.06f) },
        { pid::driftAmount,   0.22f },
        { pid::rippleRate,    rippleHz (6.0f) },
        { pid::rippleDecay,   0.62f },
        { pid::rippleDepth,   0.30f },
        { pid::rippleCycles,  cycles (3.0f) },
        { pid::rippleSpread,  0.55f },

        { pid::dropAmount,    0.22f },
        { pid::dropDensity,   0.26f },
        { pid::dropSize,      0.28f },
        { pid::dropTone,      0.80f },
        { pid::dropSplash,    0.40f },
        { pid::dropGravity,   0.35f },
        { pid::dropBounce,    0.25f },
        { pid::dropRandom,    0.65f },
        { pid::dropSpread,    0.85f },

        { pid::resoAmount,    0.38f },
        { pid::resoSize,      0.70f },
        { pid::resoDecay,     0.44f },
        { pid::resoDamping,   0.34f },
        { pid::resoScatter,   0.45f },
        { pid::resoMotion,    0.38f },

        { pid::mastHigh,      eqdb (1.5f) },
        { pid::mastDrive,     0.06f },
    },
    {
        { ModSource::Tide,     ModDest::ShapeA,           0.30f },
        { ModSource::Current,  ModDest::FilterCutoff,     0.28f },
        { ModSource::Current,  ModDest::ResonatorScatter, 0.22f },
        { ModSource::Ripple,   ModDest::FilterCutoff,     0.26f },
        { ModSource::Drift,    ModDest::PanA,             0.35f },
        { ModSource::Velocity, ModDest::DropletAmount,    0.30f },
        { ModSource::KeyTrack, ModDest::ResonatorSize,   -0.25f },
    }));

    out.push_back (make (kSurface, "SUN ON WATER", "Key", { "Bright", "Glassy", "Surface" }, Dry::No,
        "Glints. A double-crested tide runs the amplitude so every held note "
        "flickers the way light does on a swell.",
    {
        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.70f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.09f },
        { pid::oscAStereo,    0.70f },
        { pid::oscALevel,     lvl (-3.5f) },

        { pid::oscBWave,      wv (OscWave::Sine) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBSemitone,  semi (7) },
        { pid::oscBLevel,     lvl (-13.0f) },
        { pid::oscBPan,       bip (0.25f) },

        { pid::subLevel,      lvl (-26.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-30.0f) },
        { pid::noiseTone,     0.85f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (6800.0f) },
        { pid::filtReso,      0.24f },
        { pid::filtKeyTrack,  0.85f },
        { pid::filtEnvAmt,    bip (0.18f) },

        { pid::fenvAttack,    att (0.006f) },
        { pid::fenvDecay,     dec (0.70f) },
        { pid::fenvSustain,   0.35f },
        { pid::fenvRelease,   rel (1.20f) },

        { pid::aenvAttack,    att (0.012f) },
        { pid::aenvDecay,     dec (1.40f) },
        { pid::aenvSustain,   0.58f },
        { pid::aenvRelease,   rel (1.60f) },
        { pid::aenvVelocity,  0.60f },

        { pid::macroWet,      0.40f },
        { pid::macroGlow,     0.80f },
        { pid::macroSpace,    0.42f },
        { pid::macroRipple,   0.30f },
        { pid::fluidX,        bip (-0.20f) },
        { pid::fluidY,        bip (-0.64f) },

        { pid::tideShape,     ts (TideShape::DoubleWave) },
        { pid::tideRate,      tideHz (2.6f) },
        { pid::tideDepth,     0.30f },
        { pid::tideStereo,    0.55f },
        { pid::currRate,      currHz (0.5f) },
        { pid::currAmount,    0.25f },

        { pid::resoAmount,    0.26f },
        { pid::resoSize,      0.74f },
        { pid::resoDecay,     0.38f },
        { pid::resoDamping,   0.30f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.28f },
        { pid::scurRate,      scurHz (0.12f) },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.8f) },
        { pid::chorDepth,     0.40f },
        { pid::chorMix,       0.30f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::EighthD) },
        { pid::dlyFeedback,   0.30f },
        { pid::dlyDamping,    0.62f },
        { pid::dlyMix,        0.22f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.50f },
        { pid::verbHighCut,   vHighHz (12000.0f) },
        { pid::verbMix,       0.28f },
        { pid::mastHigh,      eqdb (2.0f) },
    },
    {
        { ModSource::Tide,     ModDest::Amplitude,    0.30f },
        { ModSource::Tide,     ModDest::FilterCutoff, 0.20f },
        { ModSource::Velocity, ModDest::ShapeA,       0.30f },
        { ModSource::Drift,    ModDest::PanB,         0.40f },
        { ModSource::ModWheel, ModDest::DelayMix,     0.35f },
    }));

    // --- DRY AQUATIC #2 ----------------------------------------------------
    out.push_back (make (kSurface, "FOAM", "Texture", { "Surface", "Organic", "Bright" }, Dry::Yes,
        "Surf noise through a moving band-pass, with a fine spray of droplets. "
        "The wash is the instrument; the oscillators are only a thread of pitch.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.55f },
        { pid::oscAUnison,    uni (4) },
        { pid::oscADetune,    0.30f },
        { pid::oscAStereo,    0.85f },
        { pid::oscALevel,     lvl (-16.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBLevel,     lvl (-22.0f) },
        { pid::oscBStereo,    0.80f },

        { pid::subLevel,      lvl (kSilentDb) },
        { pid::noiseType,     nz (NoiseType::Surf) },
        { pid::noiseLevel,    lvl (-4.0f) },
        { pid::noiseTone,     0.62f },

        { pid::filtMode,      fm (FilterMode::BP12) },
        { pid::filtCutoff,    hz (1800.0f) },
        { pid::filtReso,      0.42f },
        { pid::filtDrive,     0.14f },
        { pid::filtKeyTrack,  0.25f },
        { pid::filtEnvAmt,    bip (0.20f) },
        { pid::filtMovement,  0.60f },
        { pid::filtPressure,  0.30f },

        { pid::fenvAttack,    att (0.90f) },
        { pid::fenvDecay,     dec (3.00f) },
        { pid::fenvSustain,   0.55f },
        { pid::fenvRelease,   rel (3.50f) },

        { pid::aenvAttack,    att (1.50f) },
        { pid::aenvDecay,     dec (3.00f) },
        { pid::aenvSustain,   0.80f },
        { pid::aenvRelease,   rel (4.00f) },
        { pid::aenvVelocity,  0.25f },

        { pid::macroWet,      0.65f },
        { pid::macroCurrent,  0.70f },
        { pid::macroDrops,    0.55f },
        { pid::macroGlow,     0.55f },
        { pid::fluidX,        bip (0.25f) },
        { pid::fluidY,        bip (-0.60f) },
        { pid::fluidXAmount,  0.70f },

        { pid::tideShape,     ts (TideShape::Swell) },
        { pid::tideRate,      tideHz (0.22f) },
        { pid::tideDepth,     0.55f },
        { pid::tideStereo,    0.70f },
        { pid::currRate,      currHz (1.30f) },
        { pid::currAmount,    0.72f },
        { pid::currSmooth,    0.40f },
        { pid::currDrift,     0.35f },
        { pid::currStereo,    0.90f },
        { pid::driftRate,     driftHz (0.08f) },
        { pid::driftAmount,   0.30f },

        { pid::dropAmount,    0.42f },
        { pid::dropDensity,   0.72f },
        { pid::dropSize,      0.18f },
        { pid::dropTone,      0.86f },
        { pid::dropSplash,    0.62f },
        { pid::dropGravity,   0.30f },
        { pid::dropBounce,    0.20f },
        { pid::dropRandom,    0.85f },
        { pid::dropSpread,    0.95f },

        { pid::resoAmount,    0.20f },
        { pid::resoSize,      0.55f },
        { pid::resoDecay,     0.30f },
        { pid::resoDamping,   0.55f },
        { pid::resoScatter,   0.70f },
        { pid::resoMotion,    0.55f },

        { pid::mastLow,       eqdb (-2.0f) },
        { pid::mastHigh,      eqdb (1.0f) },
    },
    {
        { ModSource::Current,     ModDest::FilterCutoff,   0.60f },
        { ModSource::Current,     ModDest::NoiseTone,      0.40f },
        { ModSource::Tide,        ModDest::NoiseLevel,     0.35f },
        { ModSource::Tide,        ModDest::FilterCutoff,   0.25f },
        { ModSource::Drift,       ModDest::DropletDensity, 0.40f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,  0.45f },
        { ModSource::Velocity,    ModDest::NoiseLevel,     0.25f },
    }));

    // --- DRY AQUATIC #3 ----------------------------------------------------
    out.push_back (make (kSurface, "SKIPPING STONE", "Pluck", { "Surface", "Bright", "Wet" }, Dry::Yes,
        "A stone thrown flat. The RIPPLE modulator bends the pitch in fast "
        "damped bounces and the droplet engine's gravity does the rest.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Triangle) },
        { pid::oscAShape,     0.35f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.06f },
        { pid::oscALevel,     lvl (-2.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBShape,     0.55f },
        { pid::oscBLevel,     lvl (-9.0f) },
        { pid::oscBStereo,    0.60f },

        { pid::subLevel,      lvl (-20.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-22.0f) },
        { pid::noiseTone,     0.70f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (2400.0f) },
        { pid::filtReso,      0.38f },
        { pid::filtDrive,     0.16f },
        { pid::filtKeyTrack,  0.70f },
        { pid::filtEnvAmt,    bip (0.55f) },
        { pid::filtPressure,  0.18f },

        { pid::fenvAttack,    att (0.002f) },
        { pid::fenvDecay,     dec (0.18f) },
        { pid::fenvSustain,   0.05f },
        { pid::fenvRelease,   rel (0.35f) },
        { pid::fenvVelocity,  0.65f },

        { pid::aenvAttack,    att (0.002f) },
        { pid::aenvDecay,     dec (0.55f) },
        { pid::aenvSustain,   0.00f },
        { pid::aenvRelease,   rel (0.45f) },
        { pid::aenvVelocity,  0.70f },

        { pid::macroRipple,   0.85f },
        { pid::macroDrops,    0.60f },
        { pid::macroWet,      0.40f },
        { pid::macroGlow,     0.55f },
        { pid::fluidX,        bip (0.10f) },
        { pid::fluidY,        bip (-0.50f) },

        { pid::rippleRate,    rippleHz (9.0f) },
        { pid::rippleDecay,   0.72f },
        { pid::rippleDepth,   0.70f },
        { pid::rippleCycles,  cycles (8.0f) },
        { pid::rippleSpread,  0.45f },
        { pid::rippleTrigger, rtg (RippleTrigger::NoteOn) },
        { pid::tideRate,      tideHz (0.8f) },
        { pid::tideDepth,     0.15f },
        { pid::currRate,      currHz (0.9f) },
        { pid::currAmount,    0.25f },

        { pid::dropAmount,    0.55f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.45f },
        { pid::dropSize,      0.24f },
        { pid::dropTone,      0.72f },
        { pid::dropSplash,    0.35f },
        { pid::dropGravity,   0.88f },
        { pid::dropBounce,    0.82f },
        { pid::dropRandom,    0.30f },
        { pid::dropSpread,    0.70f },

        { pid::resoAmount,    0.30f },
        { pid::resoSize,      0.38f },
        { pid::resoDecay,     0.34f },
        { pid::resoDamping,   0.40f },
        { pid::resoScatter,   0.30f },
        { pid::resoMotion,    0.20f },

        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.04f) },
    },
    {
        { ModSource::Ripple,      ModDest::PitchAll,      0.30f },
        { ModSource::Ripple,      ModDest::FilterCutoff,  0.45f },
        { ModSource::Ripple,      ModDest::DropletSize,  -0.30f },
        { ModSource::Velocity,    ModDest::DropletAmount, 0.50f },
        { ModSource::Velocity,    ModDest::FilterCutoff,  0.35f },
        { ModSource::ModEnvelope, ModDest::ShapeB,        0.35f },
        { ModSource::KeyTrack,    ModDest::DropletTone,   0.30f },
    }));

    out.push_back (make (kSurface, "HORIZON LINE", "Pad", { "Calm", "Dreamy", "Surface" }, Dry::No,
        "A flat, patient pad for the top of an arrangement. Five-voice unison "
        "drifting slowly out of tune with itself and back again.",
    {
        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.45f },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.30f },
        { pid::oscAStereo,    0.90f },
        { pid::oscALevel,     lvl (-4.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBFine,      cents (7.0f) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.22f },
        { pid::oscBStereo,    0.85f },
        { pid::oscBLevel,     lvl (-9.0f) },

        { pid::subLevel,      lvl (-20.0f) },
        { pid::noiseType,     nz (NoiseType::Air) },
        { pid::noiseLevel,    lvl (-32.0f) },
        { pid::noiseTone,     0.72f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (3000.0f) },
        { pid::filtReso,      0.16f },
        { pid::filtKeyTrack,  0.45f },
        { pid::filtEnvAmt,    bip (0.25f) },
        { pid::filtPressure,  0.28f },

        { pid::fenvAttack,    att (2.20f) },
        { pid::fenvDecay,     dec (4.00f) },
        { pid::fenvSustain,   0.60f },
        { pid::fenvRelease,   rel (5.00f) },

        { pid::aenvAttack,    att (2.50f) },
        { pid::aenvDecay,     dec (3.00f) },
        { pid::aenvSustain,   0.85f },
        { pid::aenvRelease,   rel (5.50f) },
        { pid::aenvVelocity,  0.20f },

        { pid::macroDepth,    0.30f },
        { pid::macroWet,      0.50f },
        { pid::macroCurrent,  0.45f },
        { pid::macroSpace,    0.65f },
        { pid::macroGlow,     0.50f },
        { pid::fluidX,        bip (-0.56f) },
        { pid::fluidY,        bip (-0.40f) },

        { pid::tideShape,     ts (TideShape::Flow) },
        { pid::tideRate,      tideHz (0.12f) },
        { pid::tideDepth,     0.35f },
        { pid::tideStereo,    0.75f },
        { pid::driftRate,     driftHz (0.025f) },
        { pid::driftAmount,   0.45f },
        { pid::driftStereo,   0.85f },
        { pid::currRate,      currHz (0.18f) },
        { pid::currAmount,    0.35f },
        { pid::currSmooth,    0.80f },

        { pid::resoAmount,    0.18f },
        { pid::resoSize,      0.65f },
        { pid::resoDecay,     0.50f },
        { pid::resoDamping,   0.50f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.35f },
        { pid::scurRate,      scurHz (0.08f) },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.25f) },
        { pid::chorDepth,     0.45f },
        { pid::chorDelay,     chdel (18.0f) },
        { pid::chorMix,       0.32f },
        { pid::dlyEnable,     on },
        { pid::dlyTime,       dtime (0.62f) },
        { pid::dlyFeedback,   0.30f },
        { pid::dlyDamping,    0.65f },
        { pid::dlyMix,        0.18f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.28f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.72f },
        { pid::verbDecay,     0.68f },
        { pid::verbPredelay,  pre (35.0f) },
        { pid::verbMix,       0.40f },
        { pid::mastLow,       eqdb (-1.0f) },
    },
    {
        { ModSource::Drift,   ModDest::FineAll,      0.25f },
        { ModSource::Drift,   ModDest::FilterCutoff, 0.28f },
        { ModSource::Tide,    ModDest::ShapeA,       0.35f },
        { ModSource::Current, ModDest::PanB,         0.40f },
        { ModSource::Current, ModDest::OscMix,       0.25f },
        { ModSource::Note,    ModDest::Amplitude,   -0.15f },
    }));
}

} // namespace ripples::factory
