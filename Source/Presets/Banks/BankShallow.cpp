#include "Presets/PresetAuthoring.h"
#include "Presets/Banks.h"

/*
    RIPPLES — the SHALLOW WATER bank.

    See Presets/PresetAuthoring.h for the unit helpers and the make() form, and
    docs/PRESET_GUIDE.md for what makes a preset in this bank good.
*/

namespace ripples::factory
{

//==============================================================================
//  SHALLOW WATER — clear, playful, close to the bottom. Plucks and small keys.
//==============================================================================
void addShallowBank (std::vector<Preset>& out)
{
    // --- DRY AQUATIC #4 ----------------------------------------------------
    out.push_back (make (kShallow, "LIQUID PLUCK", "Pluck", { "Wet", "Glassy", "Bright" }, Dry::Yes,
        "The reference pluck. A hard mod-envelope sweep for the attack, the "
        "resonator for the bubble that follows it, note-mode droplets for the "
        "splash. Nothing after the voice at all.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.48f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.09f },
        { pid::oscAStereo,    0.55f },
        { pid::oscALevel,     lvl (-2.5f) },
        { pid::oscAPhase,     startPhase (0.0f) },   // consistent attack transient

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBShape,     0.62f },
        { pid::oscBFine,      cents (-5.0f) },
        { pid::oscBLevel,     lvl (-10.0f) },
        { pid::oscBStereo,    0.70f },
        { pid::oscBPhase,     startPhase (0.0f) },

        { pid::subWave,       sw (SubWave::Sine) },
        { pid::subLevel,      lvl (-17.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-21.0f) },
        { pid::noiseTone,     0.66f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (700.0f) },
        { pid::filtReso,      0.46f },
        { pid::filtDrive,     0.20f },
        { pid::filtKeyTrack,  0.75f },
        { pid::filtEnvAmt,    bip (0.72f) },
        { pid::filtPressure,  0.22f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.30f) },
        { pid::fenvSustain,   0.08f },
        { pid::fenvRelease,   rel (0.40f) },
        { pid::fenvVelocity,  0.70f },

        { pid::aenvAttack,    att (0.002f) },
        { pid::aenvDecay,     dec (1.10f) },
        { pid::aenvSustain,   0.10f },
        { pid::aenvRelease,   rel (0.70f) },
        { pid::aenvVelocity,  0.65f },

        { pid::macroDepth,    0.35f },
        { pid::macroWet,      0.60f },
        { pid::macroRipple,   0.45f },
        { pid::macroDrops,    0.50f },
        { pid::macroGlow,     0.55f },
        { pid::fluidX,        bip (0.05f) },
        { pid::fluidY,        bip (-0.25f) },

        { pid::tideRate,      tideHz (0.9f) },
        { pid::tideDepth,     0.18f },
        { pid::tideStereo,    0.40f },
        { pid::currRate,      currHz (1.4f) },
        { pid::currAmount,    0.30f },
        { pid::currSmooth,    0.45f },
        { pid::rippleRate,    rippleHz (7.5f) },
        { pid::rippleDecay,   0.68f },
        { pid::rippleDepth,   0.42f },
        { pid::rippleCycles,  cycles (5.0f) },
        { pid::rippleSpread,  0.40f },

        { pid::dropAmount,    0.45f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.35f },
        { pid::dropSize,      0.32f },
        { pid::dropTone,      0.68f },
        { pid::dropSplash,    0.55f },
        { pid::dropGravity,   0.60f },
        { pid::dropBounce,    0.45f },
        { pid::dropRandom,    0.40f },
        { pid::dropSpread,    0.75f },

        { pid::resoAmount,    0.52f },
        { pid::resoSize,      0.44f },
        { pid::resoDecay,     0.56f },
        { pid::resoDamping,   0.38f },
        { pid::resoScatter,   0.28f },
        { pid::resoMotion,    0.30f },

        { pid::mastDrive,     0.10f },
        { pid::mastHigh,      eqdb (1.0f) },
    },
    {
        { ModSource::ModEnvelope, ModDest::ResonatorSize,   -0.30f },
        { ModSource::Ripple,      ModDest::FilterCutoff,     0.40f },
        { ModSource::Ripple,      ModDest::ResonatorScatter, 0.25f },
        { ModSource::Velocity,    ModDest::DropletAmount,    0.45f },
        { ModSource::Velocity,    ModDest::ResonatorAmount,  0.30f },
        { ModSource::Current,     ModDest::ShapeA,           0.30f },
        { ModSource::KeyTrack,    ModDest::DropletSize,     -0.35f },
        { ModSource::RandomPerNote, ModDest::PanA,           0.30f },
    }));

    out.push_back (make (kShallow, "TIDE POOL", "Key", { "Organic", "Calm", "Wet" }, Dry::No,
        "A small warm keyboard. Hollow odd harmonics, a bubble bed under it, "
        "and just enough room around it to sit in a mix.",
    {
        { pid::oscAWave,      wv (OscWave::Hollow) },
        { pid::oscAShape,     0.42f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.12f },
        { pid::oscAStereo,    0.50f },
        { pid::oscALevel,     lvl (-3.0f) },

        { pid::oscBWave,      wv (OscWave::Sine) },
        { pid::oscBOctave,    oct (-1) },
        { pid::oscBLevel,     lvl (-8.0f) },

        { pid::subLevel,      lvl (-15.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-25.0f) },
        { pid::noiseTone,     0.45f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (1500.0f) },
        { pid::filtReso,      0.22f },
        { pid::filtDrive,     0.18f },
        { pid::filtKeyTrack,  0.65f },
        { pid::filtEnvAmt,    bip (0.40f) },
        { pid::filtPressure,  0.35f },

        { pid::fenvAttack,    att (0.004f) },
        { pid::fenvDecay,     dec (0.80f) },
        { pid::fenvSustain,   0.25f },
        { pid::fenvRelease,   rel (0.90f) },
        { pid::fenvVelocity,  0.55f },

        { pid::aenvAttack,    att (0.006f) },
        { pid::aenvDecay,     dec (2.00f) },
        { pid::aenvSustain,   0.35f },
        { pid::aenvRelease,   rel (1.30f) },
        { pid::aenvVelocity,  0.60f },

        { pid::macroDepth,    0.35f },
        { pid::macroWet,      0.45f },
        { pid::macroDrops,    0.30f },
        { pid::macroSpace,    0.40f },
        { pid::macroPressure, 0.35f },
        { pid::fluidX,        bip (-0.15f) },
        { pid::fluidY,        bip (-0.20f) },

        { pid::tideShape,     ts (TideShape::Triangle) },
        { pid::tideRate,      tideHz (0.7f) },
        { pid::tideDepth,     0.22f },
        { pid::tideStereo,    0.45f },
        { pid::currRate,      currHz (0.6f) },
        { pid::currAmount,    0.38f },
        { pid::currSmooth,    0.65f },

        { pid::dropAmount,    0.28f },
        { pid::dropDensity,   0.30f },
        { pid::dropSize,      0.45f },
        { pid::dropTone,      0.52f },
        { pid::dropSpread,    0.70f },

        { pid::resoAmount,    0.30f },
        { pid::resoSize,      0.50f },
        { pid::resoDecay,     0.46f },
        { pid::resoDamping,   0.52f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.25f },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (0.45f) },
        { pid::chorDepth,     0.32f },
        { pid::chorMix,       0.24f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::Eighth) },
        { pid::dlyFeedback,   0.28f },
        { pid::dlyDamping,    0.70f },
        { pid::dlyMix,        0.18f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.45f },
        { pid::verbDecay,     0.42f },
        { pid::verbMix,       0.24f },
    },
    {
        { ModSource::Velocity, ModDest::FilterCutoff,  0.40f },
        { ModSource::Current,  ModDest::FilterCutoff,  0.25f },
        { ModSource::Tide,     ModDest::DropletDensity, 0.25f },
        { ModSource::ModWheel, ModDest::ResonatorAmount, 0.40f },
        { ModSource::Drift,    ModDest::FineAll,       0.10f },
    }));

    out.push_back (make (kShallow, "WADING", "Bass", { "Organic", "Wet", "Surface" }, Dry::No,
        "A mid bass that stays out of the sub region — for tracks that already "
        "have a low end. Key-tracked LP12, short mod envelope, mono legato.",
    {
        { pid::oscAWave,      wv (OscWave::Triangle) },
        { pid::oscAShape,     0.55f },
        { pid::oscAUnison,    uni (1) },
        { pid::oscALevel,     lvl (-2.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Hollow) },
        { pid::oscBOctave,    oct (0) },
        { pid::oscBFine,      cents (-9.0f) },
        { pid::oscBLevel,     lvl (-7.0f) },

        { pid::subWave,       sw (SubWave::Triangle) },
        { pid::subOctave,     subOct (-1) },
        { pid::subLevel,      lvl (-10.0f) },
        { pid::noiseType,     nz (NoiseType::Deep) },
        { pid::noiseLevel,    lvl (-30.0f) },
        { pid::noiseTone,     0.30f },

        { pid::filtMode,      fm (FilterMode::LP12) },
        { pid::filtCutoff,    hz (420.0f) },
        { pid::filtReso,      0.34f },
        { pid::filtDrive,     0.30f },
        { pid::filtKeyTrack,  0.85f },
        { pid::filtEnvAmt,    bip (0.55f) },
        { pid::filtPressure,  0.45f },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.22f) },
        { pid::fenvSustain,   0.18f },
        { pid::fenvRelease,   rel (0.25f) },
        { pid::fenvVelocity,  0.60f },

        { pid::aenvAttack,    att (0.003f) },
        { pid::aenvDecay,     dec (0.60f) },
        { pid::aenvSustain,   0.72f },
        { pid::aenvRelease,   rel (0.24f) },
        { pid::aenvVelocity,  0.45f },

        { pid::macroDepth,    0.55f },
        { pid::macroPressure, 0.50f },
        { pid::macroWet,      0.25f },
        { pid::fluidX,        bip (-0.30f) },
        { pid::fluidY,        bip (0.15f) },

        { pid::tideRate,      tideHz (0.5f) },
        { pid::tideDepth,     0.12f },
        { pid::currRate,      currHz (0.8f) },
        { pid::currAmount,    0.22f },
        { pid::currSmooth,    0.60f },

        { pid::resoAmount,    0.16f },
        { pid::resoSize,      0.30f },
        { pid::resoDecay,     0.30f },
        { pid::resoDamping,   0.70f },

        { pid::scurEnable,    on },
        { pid::scurAmount,    0.15f },
        { pid::scurWidth,     0.30f },
        { pid::diffEnable,    on },
        { pid::diffAmount,    0.25f },
        { pid::diffMix,       0.14f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.35f },
        { pid::verbDecay,     0.30f },
        { pid::verbLowCut,    vLowHz (320.0f) },
        { pid::verbMix,       0.12f },

        { pid::mastLow,       eqdb (1.5f) },
        { pid::mastDrive,     0.18f },
        { pid::voiceCount,    vox (1) },
        { pid::glideMode,     gmd (GlideMode::Legato) },
        { pid::glideTime,     gtime (0.06f) },
    },
    {
        { ModSource::Velocity,    ModDest::FilterCutoff, 0.45f },
        { ModSource::Velocity,    ModDest::FilterDrive,  0.25f },
        { ModSource::ModEnvelope, ModDest::ShapeA,       0.30f },
        { ModSource::Current,     ModDest::FilterCutoff, 0.18f },
        { ModSource::ModWheel,    ModDest::FilterResonance, 0.30f },
    }));

    out.push_back (make (kShallow, "RIPPLE RINGS", "Arp", { "Wet", "Bright", "Glassy" }, Dry::No,
        "Written for sixteenths. Tempo-synced tide on the shape, a dotted-eighth "
        "delay behind it, and a short resonator so every note leaves a ring.",
    {
        { pid::oscAWave,      wv (OscWave::Glass) },
        { pid::oscAShape,     0.58f },
        { pid::oscAUnison,    uni (2) },
        { pid::oscADetune,    0.07f },
        { pid::oscAStereo,    0.60f },
        { pid::oscALevel,     lvl (-3.0f) },
        { pid::oscAPhase,     startPhase (0.0f) },

        { pid::oscBWave,      wv (OscWave::Pulse) },
        { pid::oscBShape,     0.30f },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBLevel,     lvl (-14.0f) },
        { pid::oscBInterMode, im (InteractionMode::RingMod) },
        { pid::oscBInterAmt,  0.25f },

        { pid::subLevel,      lvl (-22.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-26.0f) },
        { pid::noiseTone,     0.72f },

        { pid::filtMode,      fm (FilterMode::LP24) },
        { pid::filtCutoff,    hz (1400.0f) },
        { pid::filtReso,      0.40f },
        { pid::filtDrive,     0.14f },
        { pid::filtKeyTrack,  0.70f },
        { pid::filtEnvAmt,    bip (0.60f) },

        { pid::fenvAttack,    att (0.001f) },
        { pid::fenvDecay,     dec (0.16f) },
        { pid::fenvSustain,   0.00f },
        { pid::fenvRelease,   rel (0.20f) },
        { pid::fenvVelocity,  0.75f },

        { pid::aenvAttack,    att (0.001f) },
        { pid::aenvDecay,     dec (0.40f) },
        { pid::aenvSustain,   0.00f },
        { pid::aenvRelease,   rel (0.30f) },
        { pid::aenvVelocity,  0.80f },

        { pid::macroRipple,   0.60f },
        { pid::macroWet,      0.50f },
        { pid::macroGlow,     0.60f },
        { pid::macroSpace,    0.45f },
        { pid::fluidX,        bip (0.30f) },
        { pid::fluidY,        bip (-0.30f) },

        { pid::tideShape,     ts (TideShape::Triangle) },
        { pid::tideSync,      on },
        { pid::tideSyncRate,  sd (SyncDivision::Half) },
        { pid::tideDepth,     0.45f },
        { pid::tideStereo,    0.50f },
        { pid::rippleRate,    rippleHz (12.0f) },
        { pid::rippleDecay,   0.80f },
        { pid::rippleDepth,   0.35f },
        { pid::rippleCycles,  cycles (3.0f) },

        { pid::dropAmount,    0.20f },
        { pid::dropMode,      dm (DropletMode::Note) },
        { pid::dropDensity,   0.22f },
        { pid::dropSize,      0.25f },
        { pid::dropTone,      0.80f },

        { pid::resoAmount,    0.34f },
        { pid::resoSize,      0.42f },
        { pid::resoDecay,     0.40f },
        { pid::resoDamping,   0.35f },
        { pid::resoScatter,   0.40f },

        { pid::scurEnable,    on },
        { pid::chorEnable,    on },
        { pid::chorRate,      chorHz (1.1f) },
        { pid::chorDepth,     0.30f },
        { pid::chorMix,       0.22f },
        { pid::dlyEnable,     on },
        { pid::dlySync,       on },
        { pid::dlySyncTime,   sd (SyncDivision::EighthD) },
        { pid::dlyFeedback,   0.46f },
        { pid::dlyMotion,     0.35f },
        { pid::dlySpread,     0.60f },
        { pid::dlyDamping,    0.55f },
        { pid::dlyMix,        0.30f },
        { pid::diffEnable,    on },
        { pid::diffMix,       0.20f },
        { pid::verbEnable,    on },
        { pid::verbSize,      0.50f },
        { pid::verbDecay,     0.45f },
        { pid::verbMix,       0.24f },
        { pid::mastHigh,      eqdb (1.5f) },
    },
    {
        { ModSource::Tide,          ModDest::ShapeA,            0.40f },
        { ModSource::Tide,          ModDest::FilterCutoff,      0.30f },
        { ModSource::Ripple,        ModDest::InteractionAmount, 0.35f },
        { ModSource::Velocity,      ModDest::FilterCutoff,      0.40f },
        { ModSource::RandomPerNote, ModDest::PanA,              0.45f },
        { ModSource::RandomPerNote, ModDest::FineAll,           0.06f },
        { ModSource::KeyTrack,      ModDest::DelayMix,         -0.20f },
    }));

    // --- DRY AQUATIC #5 ----------------------------------------------------
    out.push_back (make (kShallow, "SHOALS", "Texture", { "Organic", "Chaotic", "Wet" }, Dry::Yes,
        "A thousand small movements. Fast Current on shape, pan and resonator "
        "scatter, with dense atmospheric droplets — a shoal turning at once.",
    {
        RIPPLES_FX_BYPASSED,

        { pid::oscAWave,      wv (OscWave::Water) },
        { pid::oscAShape,     0.66f },
        { pid::oscAUnison,    uni (5) },
        { pid::oscADetune,    0.24f },
        { pid::oscAStereo,    0.95f },
        { pid::oscALevel,     lvl (-6.0f) },

        { pid::oscBWave,      wv (OscWave::Glass) },
        { pid::oscBOctave,    oct (1) },
        { pid::oscBSemitone,  semi (5) },
        { pid::oscBUnison,    uni (3) },
        { pid::oscBDetune,    0.20f },
        { pid::oscBStereo,    0.90f },
        { pid::oscBLevel,     lvl (-13.0f) },

        { pid::subLevel,      lvl (-26.0f) },
        { pid::noiseType,     nz (NoiseType::Bubble) },
        { pid::noiseLevel,    lvl (-19.0f) },
        { pid::noiseTone,     0.60f },

        { pid::filtMode,      fm (FilterMode::Morph) },
        { pid::filtCutoff,    hz (1600.0f) },
        { pid::filtReso,      0.44f },
        { pid::filtDrive,     0.12f },
        { pid::filtKeyTrack,  0.40f },
        { pid::filtEnvAmt,    bip (0.25f) },
        { pid::filtMovement,  0.35f },
        { pid::filtPressure,  0.25f },

        { pid::fenvAttack,    att (0.25f) },
        { pid::fenvDecay,     dec (1.50f) },
        { pid::fenvSustain,   0.45f },
        { pid::fenvRelease,   rel (2.00f) },

        { pid::aenvAttack,    att (0.40f) },
        { pid::aenvDecay,     dec (2.00f) },
        { pid::aenvSustain,   0.70f },
        { pid::aenvRelease,   rel (2.20f) },
        { pid::aenvVelocity,  0.40f },

        { pid::macroCurrent,  0.85f },
        { pid::macroWet,      0.55f },
        { pid::macroDrops,    0.65f },
        { pid::macroRipple,   0.30f },
        { pid::macroGlow,     0.45f },
        { pid::fluidX,        bip (0.55f) },
        { pid::fluidY,        bip (-0.20f) },
        { pid::fluidXAmount,  0.80f },

        { pid::tideShape,     ts (TideShape::DoubleWave) },
        { pid::tideRate,      tideHz (1.6f) },
        { pid::tideDepth,     0.30f },
        { pid::tideStereo,    0.85f },
        { pid::currRate,      currHz (3.2f) },
        { pid::currAmount,    0.80f },
        { pid::currSmooth,    0.30f },
        { pid::currDrift,     0.45f },
        { pid::currStereo,    0.95f },
        { pid::driftRate,     driftHz (0.12f) },
        { pid::driftAmount,   0.40f },

        { pid::dropAmount,    0.50f },
        { pid::dropDensity,   0.80f },
        { pid::dropSize,      0.22f },
        { pid::dropTone,      0.70f },
        { pid::dropSplash,    0.45f },
        { pid::dropGravity,   0.40f },
        { pid::dropBounce,    0.35f },
        { pid::dropRandom,    0.90f },
        { pid::dropSpread,    1.00f },

        { pid::resoAmount,    0.40f },
        { pid::resoSize,      0.48f },
        { pid::resoDecay,     0.42f },
        { pid::resoDamping,   0.45f },
        { pid::resoScatter,   0.75f },
        { pid::resoMotion,    0.70f },

        { pid::mastLow,       eqdb (-1.5f) },
        { pid::mastDrive,     0.08f },
    },
    {
        { ModSource::Current,     ModDest::ShapeA,           0.55f },
        { ModSource::Current,     ModDest::PanA,             0.50f },
        { ModSource::Current,     ModDest::ResonatorScatter, 0.45f },
        { ModSource::Tide,        ModDest::FilterMovement,   0.40f },
        { ModSource::Tide,        ModDest::PanB,            -0.45f },
        { ModSource::Drift,       ModDest::FilterCutoff,     0.30f },
        { ModSource::Drift,       ModDest::DropletDensity,   0.35f },
        { ModSource::AmpEnvelope, ModDest::DropletAmount,    0.40f },
        { ModSource::ModWheel,    ModDest::DropletDensity,   0.40f },
    }));
}

} // namespace ripples::factory
