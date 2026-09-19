#include "Core/PluginProcessor.h"
#include "Core/PluginEditor.h"
#include "Parameters/ParameterLayout.h"
#include "Utilities/MathUtils.h"

namespace ripples
{

//==============================================================================
RipplesAudioProcessor::RipplesAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RIPPLES", createParameterLayout()),
      presetManager (apvts)
{
    cache.prepare (apvts);
}

RipplesAudioProcessor::~RipplesAudioProcessor() = default;

//==============================================================================
void RipplesAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    synth.prepare (sampleRate, samplesPerBlock);

    stereoCurrent.prepare (sampleRate, samplesPerBlock);
    chorus.prepare (sampleRate, samplesPerBlock);
    delay.prepare (sampleRate, samplesPerBlock);
    diffusion.prepare (sampleRate, samplesPerBlock);
    reverb.prepare (sampleRate, samplesPerBlock);
    master.prepare (sampleRate, samplesPerBlock);
}

void RipplesAudioProcessor::releaseResources()
{
    synth.reset();
    stereoCurrent.reset();
    chorus.reset();
    delay.reset();
    diffusion.reset();
    reverb.reset();
    master.reset();
}

bool RipplesAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
VoiceParams RipplesAudioProcessor::buildVoiceParams (const MacroModulation& m,
                                                     double bpm) const noexcept
{
    VoiceParams v;

    // --- oscillators ---------------------------------------------------------
    const auto fillOsc = [] (BandLimitedOscillator::Params& dest,
                             const ParameterCache::OscParams& src)
    {
        dest.wave       = src.wave;
        dest.shape      = src.shape;
        dest.unison     = src.unison;
        dest.detune     = src.detune;
        dest.stereo     = src.stereo;
        dest.startPhase = src.phase;
    };

    fillOsc (v.oscA, cache.oscA);
    v.oscAOctave = cache.oscA.octave;  v.oscASemitone = cache.oscA.semitone;
    v.oscAFine   = cache.oscA.fine;    v.oscALevel    = cache.oscA.level;
    v.oscAPan    = cache.oscA.pan;

    fillOsc (v.oscB, cache.oscB);
    v.oscBOctave = cache.oscB.octave;  v.oscBSemitone = cache.oscB.semitone;
    v.oscBFine   = cache.oscB.fine;    v.oscBLevel    = cache.oscB.level;
    v.oscBPan    = cache.oscB.pan;
    v.interMode  = cache.interaction.mode;
    v.interAmount = cache.interaction.amount;

    // --- sub / noise ---------------------------------------------------------
    v.sub.wave   = cache.sub.wave;
    v.sub.octave = cache.sub.octave;
    v.subLevel   = cache.sub.level * m.subLevelMultiplier;

    v.noise.type = cache.noise.type;
    v.noise.tone = math::clamp (cache.noise.tone + m.noiseToneOffset, 0.0f, 1.0f);
    v.noiseLevel = cache.noise.level;

    // --- filter --------------------------------------------------------------
    v.filter.mode      = cache.filter.mode;
    v.filter.cutoffHz  = math::clamp (cache.filter.cutoffHz * m.cutoffMultiplier,
                                      dsp::kMinCutoffHz, dsp::kMaxCutoffHz);
    v.filter.resonance = math::clamp (cache.filter.resonance + m.resonanceOffset, 0.0f, 1.0f);
    v.filter.drive     = math::clamp (cache.filter.drive + m.driveOffset, 0.0f, 1.0f);
    v.filter.movement  = cache.filter.movement;
    v.filter.pressure  = math::clamp (cache.filter.pressure + m.pressureOffset, 0.0f, 1.0f);
    v.filterKeyTrack   = cache.filter.keyTrack;
    v.filterEnvAmount  = cache.filter.envAmount;

    // --- envelopes -----------------------------------------------------------
    v.ampEnv = { cache.ampEnv.attack, cache.ampEnv.decay,
                 cache.ampEnv.sustain, cache.ampEnv.release };
    v.modEnv = { cache.filterEnv.attack, cache.filterEnv.decay,
                 cache.filterEnv.sustain, cache.filterEnv.release };
    v.ampVelocity       = cache.ampEnv.velocity;
    v.filterEnvVelocity = cache.filterEnv.velocity;

    // --- modulators ----------------------------------------------------------
    v.tide.shape       = cache.tide.shape;
    v.tide.rateHz      = cache.tideRateHz (bpm);
    v.tide.phaseOffset = cache.tide.phase;
    v.tide.stereoPhase = cache.tide.stereo;
    v.tideDepth        = cache.tide.depth;

    v.current.rate       = cache.current.rateHz * m.currentRateMultiplier;
    v.current.smoothness = cache.current.smoothness;
    v.current.drift      = cache.current.drift;
    v.current.stereo     = cache.current.stereo;
    v.currentAmount      = math::clamp (cache.current.amount + m.currentAmountOffset, 0.0f, 1.0f);

    v.drift.rate   = cache.drift.rateHz;
    v.drift.stereo = cache.drift.stereo;
    v.driftAmount  = math::clamp (cache.drift.amount + m.driftAmountOffset, 0.0f, 1.0f);

    v.ripple.rateHz = cache.rippleMod.rateHz;
    v.ripple.decay  = cache.rippleMod.decay;
    v.ripple.cycles = cache.rippleMod.cycles;
    v.ripple.spread = cache.rippleMod.spread;
    v.ripple.invert = cache.rippleMod.invert;
    v.rippleDepth   = math::clamp (cache.rippleMod.depth + m.rippleDepthOffset, 0.0f, 1.0f);
    v.rippleTrigger = cache.rippleMod.trigger;

    // --- aquatic engines -----------------------------------------------------
    v.droplets.amount  = math::clamp (cache.droplets.amount + m.dropletAmountOffset, 0.0f, 1.0f);
    v.droplets.density = cache.droplets.density;
    v.droplets.size    = cache.droplets.size;
    v.droplets.tone    = cache.droplets.tone;
    v.droplets.splash  = cache.droplets.splash;
    v.droplets.gravity = cache.droplets.gravity;
    v.droplets.bounce  = cache.droplets.bounce;
    v.droplets.random  = math::clamp (cache.droplets.random + m.dropletRandomOffset, 0.0f, 1.0f);
    v.droplets.spread  = cache.droplets.spread;
    v.droplets.mode    = cache.droplets.mode;

    v.resonator.amount  = math::clamp (cache.resonator.amount + m.resonatorAmountOffset, 0.0f, 1.0f);
    v.resonator.size    = cache.resonator.size;
    v.resonator.decay   = cache.resonator.decay;
    v.resonator.damping = cache.resonator.damping;
    v.resonator.scatter = math::clamp (cache.resonator.scatter + m.resonatorScatterOffset, 0.0f, 1.0f);
    v.resonator.motion  = math::clamp (cache.resonator.motion + m.resonatorMotionOffset, 0.0f, 1.0f);

    // --- performance ---------------------------------------------------------
    v.glideTime       = cache.global.glideSeconds;
    v.glideMode       = cache.global.glideMode;
    v.bendRangeSemis  = (float) cache.global.bendRange;
    v.masterTuneCents = cache.global.masterTuneCents;

    // The cache stores the Fluid Field as -1..1; the voice expects 0..1.
    v.fluidX = cache.fluid.x * 0.5f + 0.5f;
    v.fluidY = cache.fluid.y * 0.5f + 0.5f;

    // --- modulation matrix ---------------------------------------------------
    for (int i = 0; i < pid::kNumModSlots; ++i)
    {
        const auto& s = cache.modSlots[i];
        v.modSlots[i].source  = (ModSource) math::clamp (s.source, 0, (int) ModSource::NumSources - 1);
        v.modSlots[i].dest    = (ModDest)   math::clamp (s.dest,   0, (int) ModDest::NumDests - 1);
        v.modSlots[i].amount  = s.amount;
        v.modSlots[i].bipolar = s.bipolar;
    }

    return v;
}

//==============================================================================
void RipplesAudioProcessor::updateEffectParams (const MacroModulation& m, double bpm) noexcept
{
    const auto clamp01 = [] (float x) { return math::clamp (x, 0.0f, 1.0f); };

    StereoCurrent::Params sc;
    sc.enabled = cache.stereoCurrent.enabled;
    sc.amount  = cache.stereoCurrent.amount;
    sc.rate    = cache.stereoCurrent.rateHz * m.stereoRateMultiplier;
    sc.width   = clamp01 (cache.stereoCurrent.width * m.stereoWidthMultiplier);
    stereoCurrent.setParams (sc);

    LiquidChorus::Params ch;
    ch.enabled  = cache.chorus.enabled;
    ch.rate     = cache.chorus.rateHz;
    ch.depth    = cache.chorus.depth;
    ch.delayMs  = cache.chorus.delayMs;
    ch.feedback = cache.chorus.feedback;
    ch.width    = cache.chorus.width;
    ch.mix      = clamp01 (cache.chorus.mix + m.chorusMixOffset);
    chorus.setParams (ch);

    LiquidDelay::Params dl;
    dl.enabled      = cache.delay.enabled;
    dl.timeSeconds  = cache.delayTimeSeconds (bpm);
    dl.feedback     = cache.delay.feedback;
    dl.motion       = clamp01 (cache.delay.motion + m.delayMotionOffset);
    dl.spread       = cache.delay.spread;
    dl.damping      = cache.delay.damping;
    dl.diffusion    = cache.delay.diffusion;
    dl.mix          = clamp01 (cache.delay.mix + m.delayMixOffset);
    delay.setParams (dl);

    DiffusionNetwork::Params df;
    df.enabled = cache.diffusion.enabled;
    df.amount  = clamp01 (cache.diffusion.amount + m.diffusionAmountOffset);
    df.size    = cache.diffusion.size;
    df.damping = cache.diffusion.damping;
    df.mix     = cache.diffusion.mix;
    diffusion.setParams (df);

    AbyssReverb::Params rv;
    rv.enabled    = cache.reverb.enabled;
    rv.size       = clamp01 (cache.reverb.size + m.reverbSizeOffset);
    rv.decay      = cache.reverb.decay;
    rv.predelayMs = cache.reverb.predelayMs;
    rv.damping    = clamp01 (cache.reverb.damping + m.reverbDampingOffset);
    rv.lowCutHz   = cache.reverb.lowCutHz;
    rv.highCutHz  = cache.reverb.highCutHz;
    rv.modulation = cache.reverb.modulation;
    rv.mix        = clamp01 (cache.reverb.mix + m.reverbMixOffset);
    reverb.setParams (rv);

    MasterShaper::Params ms;
    ms.lowGainDb  = cache.master.lowGainDb + m.lowMidGainDb;
    ms.midGainDb  = cache.master.midGainDb;
    ms.highGainDb = cache.master.highGainDb + m.highShelfGainDb;
    ms.drive      = cache.master.drive;
    ms.ceilingDb  = cache.master.ceilingDb;
    ms.outputDb   = cache.master.outputDb;
    master.setParams (ms);
}

//==============================================================================
void RipplesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // Any output channels beyond our stereo pair must still be cleared.
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0)
        return;

    // --- tempo ---------------------------------------------------------------
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                lastKnownBpm = *bpm;
    }

    // --- parameters ----------------------------------------------------------
    cache.update();

    MacroEngine::Input macroIn;
    macroIn.depth    = cache.macros.depth;
    macroIn.wet      = cache.macros.wet;
    macroIn.ripple   = cache.macros.ripple;
    macroIn.current  = cache.macros.current;
    macroIn.drops    = cache.macros.drops;
    macroIn.pressure = cache.macros.pressure;
    macroIn.space    = cache.macros.space;
    macroIn.glow     = cache.macros.glow;
    macroIn.fluidX   = cache.fluid.x * 0.5f + 0.5f;   // cache is -1..1
    macroIn.fluidY   = cache.fluid.y * 0.5f + 0.5f;
    macroIn.fluidXAmount = cache.fluid.xAmount;
    macroIn.fluidYAmount = cache.fluid.yAmount;

    const auto macro = MacroEngine::compute (macroIn);

    synth.setVoiceLimit (cache.global.voiceCount);
    synth.setParams (buildVoiceParams (macro, lastKnownBpm));
    updateEffectParams (macro, lastKnownBpm);

    // --- synthesis -----------------------------------------------------------
    synth.renderNextBlock (buffer, midiMessages, 0, numSamples);

    // --- global effect chain -------------------------------------------------
    stereoCurrent.process (buffer);
    chorus.process (buffer);
    delay.process (buffer);
    diffusion.process (buffer);
    reverb.process (buffer);
    master.process (buffer);

    // --- visualisation -------------------------------------------------------
    synth.publishVisualisation (visualisation);
    visualisation.setOutputPeak (master.getPeakL(), master.getPeakR());
    visualisation.setOutputRMS (buffer.getRMSLevel (0, 0, numSamples));
    visualisation.setDepth (macro.visualDepth);
    visualisation.setGlow (macro.visualGlow);
    visualisation.setMotion (macro.visualMotion);

    // Note events drive the Fluid Field's expanding ripples.
    for (const auto meta : midiMessages)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOnOrOff())
            visualisation.pushNoteEvent (msg.getFloatVelocity(),
                                         (float) msg.getNoteNumber() / 127.0f,
                                         msg.isNoteOn());
    }
}

//==============================================================================
juce::AudioProcessorEditor* RipplesAudioProcessor::createEditor()
{
    return new RipplesAudioProcessorEditor (*this);
}

//==============================================================================
int RipplesAudioProcessor::getNumPrograms()
{
    return juce::jmax (1, presetManager.getNumPresets());
}

int RipplesAudioProcessor::getCurrentProgram()
{
    return juce::jmax (0, presetManager.getCurrentPresetIndex());
}

void RipplesAudioProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, presetManager.getNumPresets()))
        presetManager.loadFactoryPreset (index);
}

const juce::String RipplesAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, presetManager.getNumPresets()))
        return presetManager.getPresetInfo (index).name;

    return "Submerged Dreams";
}

//==============================================================================
void RipplesAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetManager.getCurrentPresetName(), nullptr);
    state.setProperty ("parameterVersion", pid::kParameterVersion, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RipplesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);

        if (tree.isValid() && tree.hasType (apvts.state.getType()))
            apvts.replaceState (tree);
    }
}

} // namespace ripples

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ripples::RipplesAudioProcessor();
}
