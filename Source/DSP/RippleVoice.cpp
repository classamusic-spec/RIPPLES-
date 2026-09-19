#include "DSP/RippleVoice.h"
#include "Utilities/MathUtils.h"

namespace ripples
{

//==============================================================================
void RippleVoice::prepare (double sr, int maxBlockSize, uint32_t seed)
{
    sampleRate = sr;
    rng.seed (seed);

    oscA.prepare (sr);
    oscB.prepare (sr);
    subOsc.prepare (sr);
    noise.prepare (sr, seed * 2654435761u + 1u);
    droplets.prepare (sr, seed * 40503u + 7u);

    filter.prepare (sr);
    resonator.prepare (sr, seed * 2246822519u + 13u);

    ampEnv.prepare (sr);
    modEnv.prepare (sr);

    tide.prepare (sr);
    current.prepare (sr, seed * 3266489917u + 19u);
    drift.prepare (sr, seed * 668265263u + 23u);
    rippleMod.prepare (sr);

    juce::ignoreUnused (maxBlockSize);

    // A 3 ms fade is short enough to free a voice promptly and long enough to
    // avoid a click when stealing.
    fadeCoeff = math::decayCoeff (0.003f, sr);

    reset();
}

void RippleVoice::reset() noexcept
{
    oscA.reset (rng);
    oscB.reset (rng);
    subOsc.reset();
    noise.reset();
    droplets.reset();
    filter.reset();
    resonator.reset();
    ampEnv.reset();
    modEnv.reset();
    tide.reset();
    current.reset();
    drift.reset();
    rippleMod.reset();

    active = releasing = fadingOut = false;
    currentNote = -1;
    velocity = 0.0f;
    fadeGain = 1.0f;
    modCounter = 0;
    lastAmpEnv = lastModEnv = 0.0f;
    lastTide = lastCurrent = lastDrift = lastRipple = 0.0f;

    for (auto& d : destOffsets)
        d = 0.0f;
}

//==============================================================================
void RippleVoice::noteOn (int midiNote, float vel, bool legato) noexcept
{
    currentNote = midiNote;
    velocity = math::clamp (vel, 0.0f, 1.0f);
    randomPerNote = rng.nextBipolar();

    noteHz = math::noteToHz ((float) midiNote);
    targetHz = noteHz;

    const bool shouldGlide = params.glideMode == GlideMode::Always
                          || (params.glideMode == GlideMode::Legato && legato && active);

    if (! shouldGlide || ! active)
        currentHz = targetHz;

    glideCoeff = params.glideTime > 0.0f
               ? math::decayCoeff (params.glideTime, sampleRate)
               : 0.0f;

    // A legato note into a sounding voice keeps the envelopes and oscillator
    // phase running — that is what makes legato feel connected.
    if (! legato || ! active)
    {
        oscA.reset (rng);
        oscB.reset (rng);
        subOsc.reset();
        filter.reset();
        resonator.reset();
    }

    resonator.setBaseFrequency (noteHz);

    ampEnv.noteOn();
    modEnv.noteOn();

    if (params.rippleTrigger == RippleTrigger::NoteOn)
        rippleMod.trigger (velocity);

    if (params.droplets.mode == DropletMode::Note)
        droplets.trigger (noteHz, velocity);

    active = true;
    releasing = false;
    fadingOut = false;
    fadeGain = 1.0f;
}

void RippleVoice::noteOff() noexcept
{
    ampEnv.noteOff();
    modEnv.noteOff();
    releasing = true;

    if (params.rippleTrigger == RippleTrigger::NoteOff)
        rippleMod.trigger (velocity);
}

void RippleVoice::kill() noexcept
{
    reset();
}

void RippleVoice::steal() noexcept
{
    fadingOut = true;
    releasing = true;
}

//==============================================================================
float RippleVoice::computeOscFrequency (float baseHz, int octave, int semitone,
                                        float fineCents, float pitchOffsetSemis) const noexcept
{
    const float semis = (float) (octave * 12 + semitone)
                      + fineCents * 0.01f
                      + params.masterTuneCents * 0.01f
                      + params.pitchBend * params.bendRangeSemis
                      + pitchOffsetSemis;

    return baseHz * math::semitonesToRatio (semis);
}

//==============================================================================
void RippleVoice::updateControlBlock() noexcept
{
    constexpr int n = dsp::kModBlockSize;

    // --- advance the per-voice modulators -----------------------------------
    const float tideV    = tide.advance (n);
    const float currentV = current.advance (n);
    const float driftV   = drift.advance (n);
    const float rippleV  = rippleMod.advance (n);
    const float ampV     = ampEnv.getCurrentValue();
    const float modV     = modEnv.getCurrentValue();

    lastTide = tideV; lastCurrent = currentV; lastDrift = driftV; lastRipple = rippleV;
    lastAmpEnv = ampV; lastModEnv = modV;

    // --- gather the matrix sources ------------------------------------------
    sourceValues.set (ModSource::None,          0.0f);
    sourceValues.set (ModSource::AmpEnvelope,   ampV);
    sourceValues.set (ModSource::ModEnvelope,   modV);
    sourceValues.set (ModSource::Tide,          tideV * params.tideDepth);
    sourceValues.set (ModSource::Current,       currentV * params.currentAmount);
    sourceValues.set (ModSource::Drift,         driftV * params.driftAmount);
    sourceValues.set (ModSource::Ripple,        rippleV * params.rippleDepth);
    sourceValues.set (ModSource::Velocity,      velocity);
    sourceValues.set (ModSource::Note,          math::clamp ((float) currentNote / 127.0f, 0.0f, 1.0f));
    sourceValues.set (ModSource::KeyTrack,      math::clamp (((float) currentNote - 60.0f) / 48.0f, -1.0f, 1.0f));
    sourceValues.set (ModSource::ModWheel,      params.modWheel);
    sourceValues.set (ModSource::Aftertouch,    params.aftertouch);
    sourceValues.set (ModSource::RandomPerNote, randomPerNote);
    sourceValues.set (ModSource::FluidFieldX,   params.fluidX * 2.0f - 1.0f);
    sourceValues.set (ModSource::FluidFieldY,   params.fluidY * 2.0f - 1.0f);

    matrix.process (sourceValues, destOffsets);
    applyModulation (destOffsets);
}

//==============================================================================
void RippleVoice::applyModulation (const float* d) noexcept
{
    const auto at = [d] (ModDest dest) noexcept { return d[(size_t) dest]; };

    // --- pitch ---------------------------------------------------------------
    modPitchOffsetSemis = at (ModDest::PitchAll) * 24.0f
                        + at (ModDest::FineAll) * 1.0f;

    // --- filter --------------------------------------------------------------
    // Key tracking pivots on middle C so the filter opens as you play up.
    const float keyTrackSemis = params.filterKeyTrack * (float) (currentNote - 60);

    // The envelope amount is bipolar and velocity scaled.
    const float envVel = math::lerp (1.0f, velocity, params.filterEnvVelocity);
    const float envSemis = params.filterEnvAmount * modEnv.getCurrentValue() * envVel * 84.0f;

    const float cutoffSemis = keyTrackSemis + envSemis
                            + at (ModDest::FilterCutoff) * 84.0f;

    modCutoffHz = math::clamp (params.filter.cutoffHz * math::semitonesToRatio (cutoffSemis),
                               dsp::kMinCutoffHz, dsp::kMaxCutoffHz);
    lastCutoffHz = modCutoffHz;

    modResonance = math::clamp (params.filter.resonance + at (ModDest::FilterResonance), 0.0f, 1.0f);

    auto fp = params.filter;
    fp.cutoffHz  = modCutoffHz;
    fp.resonance = modResonance;
    fp.drive     = math::clamp (fp.drive + at (ModDest::FilterDrive), 0.0f, 1.0f);
    fp.movement  = math::clamp (fp.movement + at (ModDest::FilterMovement), 0.0f, 1.0f);
    filter.setParams (fp);

    // --- oscillator levels and pan ------------------------------------------
    // OscMix pushes A and B in opposite directions around their set levels.
    const float mix = at (ModDest::OscMix);
    modOscALevel  = math::clamp (params.oscALevel  + at (ModDest::LevelA) - mix, 0.0f, 2.0f);
    modOscBLevel  = math::clamp (params.oscBLevel  + at (ModDest::LevelB) + mix, 0.0f, 2.0f);
    modSubLevel   = math::clamp (params.subLevel   + at (ModDest::SubLevel), 0.0f, 2.0f);
    modNoiseLevel = math::clamp (params.noiseLevel + at (ModDest::NoiseLevel), 0.0f, 2.0f);

    math::equalPowerPan (math::clamp (params.oscAPan + at (ModDest::PanA), -1.0f, 1.0f),
                         modOscAPanL, modOscAPanR);
    math::equalPowerPan (math::clamp (params.oscBPan + at (ModDest::PanB), -1.0f, 1.0f),
                         modOscBPanL, modOscBPanR);

    modAmplitude = math::clamp (1.0f + at (ModDest::Amplitude), 0.0f, 2.0f);
    modInterAmount = math::clamp (params.interAmount + at (ModDest::InteractionAmount), 0.0f, 1.0f);

    // --- oscillator shape ----------------------------------------------------
    auto aParams = params.oscA;
    aParams.shape = math::clamp (aParams.shape + at (ModDest::ShapeA), 0.0f, 1.0f);
    oscA.setParams (aParams);

    auto bParams = params.oscB;
    bParams.shape = math::clamp (bParams.shape + at (ModDest::ShapeB), 0.0f, 1.0f);
    oscB.setParams (bParams);

    // --- noise ---------------------------------------------------------------
    auto nParams = params.noise;
    nParams.tone = math::clamp (nParams.tone + at (ModDest::NoiseTone), 0.0f, 1.0f);
    noise.setParams (nParams);

    // --- resonator -----------------------------------------------------------
    auto rParams = params.resonator;
    rParams.amount  = math::clamp (rParams.amount  + at (ModDest::ResonatorAmount), 0.0f, 1.0f);
    rParams.size    = math::clamp (rParams.size    + at (ModDest::ResonatorSize), 0.0f, 1.0f);
    rParams.decay   = math::clamp (rParams.decay   + at (ModDest::ResonatorDecay), 0.0f, 1.0f);
    rParams.damping = math::clamp (rParams.damping + at (ModDest::ResonatorDamping), 0.0f, 1.0f);
    rParams.scatter = math::clamp (rParams.scatter + at (ModDest::ResonatorScatter), 0.0f, 1.0f);
    resonator.setParams (rParams);

    // --- droplets ------------------------------------------------------------
    auto dParams = params.droplets;
    dParams.amount  = math::clamp (dParams.amount  + at (ModDest::DropletAmount), 0.0f, 1.0f);
    dParams.density = math::clamp (dParams.density + at (ModDest::DropletDensity), 0.0f, 1.0f);
    dParams.size    = math::clamp (dParams.size    + at (ModDest::DropletSize), 0.0f, 1.0f);
    dParams.tone    = math::clamp (dParams.tone    + at (ModDest::DropletTone), 0.0f, 1.0f);
    droplets.setParams (dParams);
}

//==============================================================================
void RippleVoice::renderNextBlock (juce::AudioBuffer<float>& output,
                                   int startSample, int numSamples) noexcept
{
    if (! active)
        return;

    if (paramsDirty)
    {
        // Push the block's parameters into the modules that do not need
        // per-sample modulation.
        subOsc.setParams (params.sub);
        ampEnv.setParams (params.ampEnv);
        modEnv.setParams (params.modEnv);
        tide.setParams (params.tide);
        current.setParams (params.current);
        drift.setParams (params.drift);
        rippleMod.setParams (params.ripple);

        for (int i = 0; i < pid::kNumModSlots; ++i)
            matrix.setSlot (i, params.modSlots[i]);

        glideCoeff = params.glideTime > 0.0f
                   ? math::decayCoeff (params.glideTime, sampleRate)
                   : 0.0f;

        paramsDirty = false;
    }

    auto* left  = output.getWritePointer (0, startSample);
    auto* right = output.getNumChannels() > 1 ? output.getWritePointer (1, startSample) : left;

    // Velocity shapes the amp: at zero sensitivity every note is full level.
    const float velGain = math::lerp (1.0f, velocity, params.ampVelocity);

    for (int i = 0; i < numSamples; ++i)
    {
        if (modCounter == 0)
            updateControlBlock();

        modCounter = (modCounter + 1) % dsp::kModBlockSize;

        // --- pitch, with glide ----------------------------------------------
        if (glideCoeff > 0.0f)
            currentHz = targetHz + (currentHz - targetHz) * glideCoeff;
        else
            currentHz = targetHz;

        const float aHz = computeOscFrequency (currentHz, params.oscAOctave, params.oscASemitone,
                                               params.oscAFine,
                                               modPitchOffsetSemis + destOffsets[(size_t) ModDest::PitchA] * 24.0f);
        const float bHz = computeOscFrequency (currentHz, params.oscBOctave, params.oscBSemitone,
                                               params.oscBFine,
                                               modPitchOffsetSemis + destOffsets[(size_t) ModDest::PitchB] * 24.0f);

        oscA.setFrequency (aHz);
        oscB.setFrequency (bHz);
        subOsc.setFrequency (currentHz);

        // --- oscillator A ----------------------------------------------------
        float aL = 0.0f, aR = 0.0f;
        const bool aWrapped = oscA.processSample (aL, aR);
        const float aMono = (aL + aR) * 0.5f;

        // --- oscillator B, shaped by the interaction mode --------------------
        float bL = 0.0f, bR = 0.0f;
        float phaseMod = 0.0f, fmRatio = 1.0f;

        switch (params.interMode)
        {
            case InteractionMode::HardSync:
                if (aWrapped)
                    oscB.hardSync();
                break;

            case InteractionMode::FM:
                // Musically useful range: keep the index modest so RIPPLES stays
                // a subtractive synth rather than becoming an FM workstation.
                fmRatio = 1.0f + aMono * modInterAmount * 4.0f;
                break;

            case InteractionMode::PhaseMod:
                phaseMod = aMono * modInterAmount * 0.5f;
                break;

            case InteractionMode::Normal:
            case InteractionMode::RingMod:
            case InteractionMode::Crossfade:
            case InteractionMode::NumModes:
            default:
                break;
        }

        oscB.processSample (bL, bR, phaseMod, fmRatio);

        // --- mix the two oscillators ----------------------------------------
        float mixL = 0.0f, mixR = 0.0f;

        if (params.interMode == InteractionMode::RingMod)
        {
            const float ringL = aL * bL, ringR = aR * bR;
            mixL = math::lerp (aL * modOscALevel, ringL, modInterAmount) * modOscAPanL
                 + bL * modOscBLevel * modOscBPanL * (1.0f - modInterAmount);
            mixR = math::lerp (aR * modOscALevel, ringR, modInterAmount) * modOscAPanR
                 + bR * modOscBLevel * modOscBPanR * (1.0f - modInterAmount);
        }
        else if (params.interMode == InteractionMode::Crossfade)
        {
            const float xf = modInterAmount;
            mixL = aL * modOscALevel * (1.0f - xf) * modOscAPanL + bL * modOscBLevel * xf * modOscBPanL;
            mixR = aR * modOscALevel * (1.0f - xf) * modOscAPanR + bR * modOscBLevel * xf * modOscBPanR;
        }
        else
        {
            mixL = aL * modOscALevel * modOscAPanL + bL * modOscBLevel * modOscBPanL;
            mixR = aR * modOscALevel * modOscAPanR + bR * modOscBLevel * modOscBPanR;
        }

        // --- sub and noise ---------------------------------------------------
        const float subV = subOsc.processSample() * modSubLevel;
        mixL += subV;
        mixR += subV;

        float nL = 0.0f, nR = 0.0f;
        noise.processSample (nL, nR);
        mixL += nL * modNoiseLevel;
        mixR += nR * modNoiseLevel;

        // --- droplet exciter -------------------------------------------------
        float dL = 0.0f, dR = 0.0f;
        droplets.processSample (dL, dR);
        mixL += dL;
        mixR += dR;

        // --- filter ----------------------------------------------------------
        filter.setCutoff (modCutoffHz);
        filter.processSample (mixL, mixR);

        // --- resonator -------------------------------------------------------
        resonator.processSample (mixL, mixR);

        // --- amplitude -------------------------------------------------------
        const float env = ampEnv.processSample();
        float gain = env * velGain * modAmplitude;

        if (fadingOut)
        {
            fadeGain *= fadeCoeff;
            gain *= fadeGain;
        }

        left[i]  += math::sanitise (mixL * gain);
        right[i] += math::sanitise (mixR * gain);

        // --- voice lifetime --------------------------------------------------
        if ((! ampEnv.isActive()) || (fadingOut && fadeGain < 1.0e-4f))
        {
            reset();
            return;
        }
    }
}

} // namespace ripples
