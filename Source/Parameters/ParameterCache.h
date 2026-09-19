#pragma once

/*
    RIPPLES — lock-free parameter cache.

    prepare() runs on the message thread and resolves every parameter ID from
    ParameterIDs.h into a raw std::atomic<float>* exactly once.

    update() runs on the audio thread, once per block. It copies every atomic
    into a plain member in NATURAL UNITS (Hz, seconds, linear gain, enum values)
    so that the DSP never converts per sample and never touches the APVTS.
    It allocates nothing, locks nothing, builds no juce::String and logs nothing.

    The public members are grouped to mirror the DSP modules' own Params structs,
    so the synth can copy them across field by field.
*/

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterEnums.h"

#include <atomic>

namespace ripples
{

class ParameterCache
{
public:
    ParameterCache() = default;

    /** Message thread. Caches one raw atomic pointer per parameter. */
    void prepare (juce::AudioProcessorValueTreeState& apvts);

    /** Audio thread, once per block. Realtime safe. */
    void update() noexcept;

    //==========================================================================
    // OSCILLATORS A (TIDE) and B (CURRENT)
    //==========================================================================
    struct OscParams
    {
        OscWave wave     = OscWave::Saw;
        float   shape    = 0.5f;    // 0..1
        int     octave   = 0;       // -3..3
        int     semitone = 0;       // -12..12
        float   fine     = 0.0f;    // cents, -100..100
        float   phase    = -1.0f / 360.0f;   // 0..1 start phase; negative = free-running
        int     unison   = 1;       // 1..dsp::kMaxUnison
        float   detune   = 0.0f;    // 0..1
        float   stereo   = 0.5f;    // 0..1
        float   pan      = 0.0f;    // -1..1
        float   level    = 1.0f;    // LINEAR gain
        float   levelDb  = 0.0f;    // dB, as shown in the UI

        /** octave * 12 + semitone + fine / 100 — ready to add to a MIDI note. */
        float   pitchOffsetSemitones = 0.0f;
    };

    OscParams oscA, oscB;

    /** How CURRENT (osc B) interacts with TIDE (osc A). */
    struct InteractionParams
    {
        InteractionMode mode   = InteractionMode::Normal;
        float           amount = 0.5f;    // 0..1
    };

    InteractionParams interaction;

    //==========================================================================
    struct SubParams
    {
        SubWave wave    = SubWave::Sine;
        int     octave  = -1;      // -1 or -2
        float   level   = 0.0f;    // LINEAR gain
        float   levelDb = -12.0f;
    };

    SubParams sub;

    //==========================================================================
    struct NoiseParams
    {
        NoiseType type    = NoiseType::White;
        float     level   = 0.0f;   // LINEAR gain
        float     levelDb = -60.0f;
        float     tone    = 0.5f;   // 0..1
    };

    NoiseParams noise;

    //==========================================================================
    struct FilterParams
    {
        FilterMode mode      = FilterMode::LP24;
        float      cutoffHz  = 1200.0f;
        float      resonance = 0.15f;   // 0..1
        float      drive     = 0.12f;   // 0..1
        float      keyTrack  = 0.5f;    // 0..1
        float      envAmount = 0.35f;   // -1..1
        float      movement  = 0.5f;    // 0..1
        float      pressure  = 0.0f;    // 0..1
    };

    FilterParams filter;

    //==========================================================================
    struct EnvParams
    {
        float attack   = 0.005f;   // seconds
        float decay    = 1.0f;     // seconds
        float sustain  = 0.8f;     // 0..1
        float release  = 0.35f;    // seconds
        float velocity = 0.5f;     // 0..1 velocity sensitivity
    };

    EnvParams ampEnv, filterEnv;

    //==========================================================================
    struct MacroParams
    {
        float depth = 0.0f, wet = 0.0f, ripple = 0.0f, current = 0.0f;
        float drops = 0.0f, pressure = 0.0f, space = 0.0f, glow = 0.0f;   // all 0..1
    };

    MacroParams macros;

    //==========================================================================
    struct FluidParams
    {
        float x = 0.0f, y = 0.0f;              // -1..1 (calm<->chaos, surface<->depth)
        float xAmount = 0.5f, yAmount = 0.5f;  // 0..1 per-preset depth
    };

    FluidParams fluid;

    //==========================================================================
    struct TideParams
    {
        TideShape    shape        = TideShape::Sine;
        float        rateHz       = 1.0f;    // free-run rate
        bool         sync         = false;
        SyncDivision syncDivision = SyncDivision::Quarter;
        float        phase        = 0.0f;    // 0..1
        float        depth        = 0.5f;    // 0..1
        float        stereo       = 0.0f;    // 0..1
    };

    TideParams tide;

    //==========================================================================
    struct CurrentParams
    {
        float rateHz     = 0.5f;
        float amount     = 0.35f;   // 0..1
        float smoothness = 0.5f;    // 0..1
        float drift      = 0.2f;    // 0..1
        float stereo     = 0.5f;    // 0..1
    };

    CurrentParams current;

    //==========================================================================
    struct DriftParams
    {
        float rateHz = 0.05f;
        float amount = 0.3f;    // 0..1
        float stereo = 0.5f;    // 0..1
    };

    DriftParams drift;

    //==========================================================================
    struct RippleParams
    {
        float         rateHz  = 4.0f;
        float         decay   = 0.5f;    // 0..1
        float         depth   = 0.5f;    // 0..1
        float         cycles  = 4.0f;
        float         spread  = 0.0f;    // 0..1
        bool          invert  = false;
        RippleTrigger trigger = RippleTrigger::NoteOn;
    };

    RippleParams rippleMod;

    //==========================================================================
    struct DropletParams
    {
        float       amount = 0.0f, density = 0.3f, size = 0.5f, tone = 0.5f,
                    splash = 0.3f, gravity = 0.5f, bounce = 0.3f,
                    random = 0.5f, spread = 0.5f;          // all 0..1
        DropletMode mode = DropletMode::Atmospheric;
    };

    DropletParams droplets;

    //==========================================================================
    struct ResonatorParams
    {
        float amount = 0.0f, size = 0.5f, decay = 0.5f,
              damping = 0.5f, scatter = 0.3f, motion = 0.2f;   // all 0..1
    };

    ResonatorParams resonator;

    //==========================================================================
    struct StereoCurrentParams
    {
        bool  enabled = false;
        float amount = 0.3f, rateHz = 0.1f, width = 0.5f;
    };

    StereoCurrentParams stereoCurrent;

    //==========================================================================
    struct ChorusParams
    {
        bool  enabled = false;
        float rateHz = 0.4f, depth = 0.4f, delayMs = 12.0f,
              feedback = 0.15f, width = 0.6f, mix = 0.35f;
    };

    ChorusParams chorus;

    //==========================================================================
    struct DelayParams
    {
        bool         enabled      = false;
        float        timeSeconds  = 0.4f;    // free-run time
        bool         sync         = false;
        SyncDivision syncDivision = SyncDivision::Eighth;
        float        feedback = 0.4f, motion = 0.3f, spread = 0.4f,
                     damping = 0.5f, diffusion = 0.3f, mix = 0.3f;
    };

    DelayParams delay;

    //==========================================================================
    struct DiffusionParams
    {
        bool  enabled = false;
        float amount = 0.4f, size = 0.5f, damping = 0.4f, mix = 0.3f;
    };

    DiffusionParams diffusion;

    //==========================================================================
    struct ReverbParams
    {
        bool  enabled = false;
        float size = 0.6f, decay = 0.6f, predelayMs = 20.0f, damping = 0.5f,
              lowCutHz = 120.0f, highCutHz = 9000.0f, modulation = 0.3f, mix = 0.3f;
    };

    ReverbParams reverb;

    //==========================================================================
    struct MasterParams
    {
        float lowGainDb = 0.0f, midGainDb = 0.0f, highGainDb = 0.0f;
        float drive     = 0.0f;      // 0..1
        float ceilingDb = -0.3f;
        float outputDb  = 0.0f;
        float ceilingGain = 1.0f;    // LINEAR
        float outputGain  = 1.0f;    // LINEAR
    };

    MasterParams master;

    //==========================================================================
    struct GlobalParams
    {
        float     glideSeconds    = 0.0f;
        GlideMode glideMode       = GlideMode::Off;
        int       voiceCount      = 16;
        int       bendRange       = 2;     // semitones
        float     masterTuneCents = 0.0f;
        float     masterTuneRatio = 1.0f;  // frequency multiplier
    };

    GlobalParams global;

    //==========================================================================
    /** One modulation-matrix slot. source/dest index ModSource / ModDest. */
    struct ModSlot
    {
        int   source  = 0;       // (int) ModSource
        int   dest    = 0;       // (int) ModDest
        float amount  = 0.0f;    // -1..1
        bool  bipolar = false;
    };

    ModSlot modSlots[pid::kNumModSlots];

    //==========================================================================
    // Tempo-sync helpers. The synth passes the host BPM in.
    //==========================================================================

    /** Cycles per second for one sync division at the given tempo. */
    float syncedRateHz (SyncDivision div, double bpm) const noexcept;

    /** Length of one sync division in seconds at the given tempo. */
    float syncedTimeSeconds (SyncDivision div, double bpm) const noexcept;

    /** TIDE rate, resolving the free/sync switch. */
    float tideRateHz (double bpm) const noexcept;

    /** LIQUID DELAY time in seconds, resolving the free/sync switch and clamped
        to the delay line length. */
    float delayTimeSeconds (double bpm) const noexcept;

private:
    //==========================================================================
    struct OscPtrs
    {
        std::atomic<float>* wave     = nullptr;
        std::atomic<float>* shape    = nullptr;
        std::atomic<float>* octave   = nullptr;
        std::atomic<float>* semitone = nullptr;
        std::atomic<float>* fine     = nullptr;
        std::atomic<float>* phase    = nullptr;
        std::atomic<float>* unison   = nullptr;
        std::atomic<float>* detune   = nullptr;
        std::atomic<float>* stereo   = nullptr;
        std::atomic<float>* pan      = nullptr;
        std::atomic<float>* level    = nullptr;
    };

    struct Ptrs
    {
        OscPtrs oscA, oscB;
        std::atomic<float> *interMode = nullptr, *interAmt = nullptr;

        std::atomic<float> *subWave = nullptr, *subOctave = nullptr, *subLevel = nullptr;
        std::atomic<float> *noiseType = nullptr, *noiseLevel = nullptr, *noiseTone = nullptr;

        std::atomic<float> *filtMode = nullptr, *filtCutoff = nullptr, *filtReso = nullptr,
                           *filtDrive = nullptr, *filtKeyTrack = nullptr, *filtEnvAmt = nullptr,
                           *filtMovement = nullptr, *filtPressure = nullptr;

        std::atomic<float> *fenvA = nullptr, *fenvD = nullptr, *fenvS = nullptr,
                           *fenvR = nullptr, *fenvVel = nullptr;
        std::atomic<float> *aenvA = nullptr, *aenvD = nullptr, *aenvS = nullptr,
                           *aenvR = nullptr, *aenvVel = nullptr;

        std::atomic<float> *macroDepth = nullptr, *macroWet = nullptr, *macroRipple = nullptr,
                           *macroCurrent = nullptr, *macroDrops = nullptr, *macroPressure = nullptr,
                           *macroSpace = nullptr, *macroGlow = nullptr;

        std::atomic<float> *fluidX = nullptr, *fluidY = nullptr,
                           *fluidXAmount = nullptr, *fluidYAmount = nullptr;

        std::atomic<float> *tideShape = nullptr, *tideRate = nullptr, *tideSync = nullptr,
                           *tideSyncRate = nullptr, *tidePhase = nullptr, *tideDepth = nullptr,
                           *tideStereo = nullptr;

        std::atomic<float> *currRate = nullptr, *currAmount = nullptr, *currSmooth = nullptr,
                           *currDrift = nullptr, *currStereo = nullptr;

        std::atomic<float> *driftRate = nullptr, *driftAmount = nullptr, *driftStereo = nullptr;

        std::atomic<float> *rippleRate = nullptr, *rippleDecay = nullptr, *rippleDepth = nullptr,
                           *rippleCycles = nullptr, *rippleSpread = nullptr,
                           *ripplePolarity = nullptr, *rippleTrigger = nullptr;

        std::atomic<float> *dropAmount = nullptr, *dropDensity = nullptr, *dropSize = nullptr,
                           *dropTone = nullptr, *dropSplash = nullptr, *dropGravity = nullptr,
                           *dropBounce = nullptr, *dropRandom = nullptr, *dropSpread = nullptr,
                           *dropMode = nullptr;

        std::atomic<float> *resoAmount = nullptr, *resoSize = nullptr, *resoDecay = nullptr,
                           *resoDamping = nullptr, *resoScatter = nullptr, *resoMotion = nullptr;

        std::atomic<float> *scurEnable = nullptr, *scurAmount = nullptr,
                           *scurRate = nullptr, *scurWidth = nullptr;

        std::atomic<float> *chorEnable = nullptr, *chorRate = nullptr, *chorDepth = nullptr,
                           *chorDelay = nullptr, *chorFeedback = nullptr, *chorWidth = nullptr,
                           *chorMix = nullptr;

        std::atomic<float> *dlyEnable = nullptr, *dlyTime = nullptr, *dlySync = nullptr,
                           *dlySyncTime = nullptr, *dlyFeedback = nullptr, *dlyMotion = nullptr,
                           *dlySpread = nullptr, *dlyDamping = nullptr, *dlyDiffusion = nullptr,
                           *dlyMix = nullptr;

        std::atomic<float> *diffEnable = nullptr, *diffAmount = nullptr, *diffSize = nullptr,
                           *diffDamping = nullptr, *diffMix = nullptr;

        std::atomic<float> *verbEnable = nullptr, *verbSize = nullptr, *verbDecay = nullptr,
                           *verbPredelay = nullptr, *verbDamping = nullptr, *verbLowCut = nullptr,
                           *verbHighCut = nullptr, *verbMod = nullptr, *verbMix = nullptr;

        std::atomic<float> *mastLow = nullptr, *mastMid = nullptr, *mastHigh = nullptr,
                           *mastDrive = nullptr, *mastCeiling = nullptr, *mastOutput = nullptr;

        std::atomic<float> *glideTime = nullptr, *glideMode = nullptr, *voiceCount = nullptr,
                           *bendRange = nullptr, *masterTune = nullptr;

        struct ModPtrs
        {
            std::atomic<float> *source = nullptr, *dest = nullptr,
                               *amount = nullptr, *bipolar = nullptr;
        };

        ModPtrs mod[pid::kNumModSlots];
    };

    Ptrs p;

    /** Returned instead of nullptr so that update() never has to branch. */
    std::atomic<float> fallback { 0.0f };

    std::atomic<float>* resolve (juce::AudioProcessorValueTreeState& apvts, juce::StringRef id);
    void cacheOsc (const OscPtrs& src, OscParams& dest) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterCache)
};

} // namespace ripples
