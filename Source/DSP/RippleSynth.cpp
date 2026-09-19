#include "DSP/RippleSynth.h"
#include "Utilities/MathUtils.h"

namespace ripples
{

//==============================================================================
void RippleSynth::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;

    uint32_t seed = 0x1F2E3D4Cu;
    for (auto& v : voices)
    {
        // Each voice gets a distinct seed so their noise and random modulation
        // never correlate — correlated voices sound like one thick voice.
        v.prepare (sr, maxBlockSize, seed);
        seed = seed * 1664525u + 1013904223u;
    }

    reset();
}

void RippleSynth::reset() noexcept
{
    for (auto& v : voices)
        v.reset();

    sustainPedalDown = false;
    sustainedNotes.fill (false);
    modWheel = aftertouch = pitchBend = 0.0f;
    noteCounter = 0;
}

void RippleSynth::setParams (const VoiceParams& p) noexcept
{
    params = p;
}

void RippleSynth::setVoiceLimit (int limit) noexcept
{
    voiceLimit = math::clamp (limit, 1, (int) dsp::kMaxVoices);
}

//==============================================================================
void RippleSynth::renderNextBlock (juce::AudioBuffer<float>& output,
                                   const juce::MidiBuffer& midi,
                                   int startSample, int numSamples) noexcept
{
    output.clear();

    auto iterator = midi.findNextSamplePosition (0);
    int position = 0;

    while (position < numSamples)
    {
        // How far can we render before the next MIDI event?
        int samplesToNext = numSamples - position;

        if (iterator != midi.cend())
        {
            const auto nextEventTime = (*iterator).samplePosition;
            samplesToNext = math::clamp (nextEventTime - position, 0, numSamples - position);
        }

        if (samplesToNext > 0)
        {
            // Refresh the live controller state, then render this slice.
            params.modWheel   = modWheel;
            params.aftertouch = aftertouch;
            params.pitchBend  = pitchBend;

            for (auto& v : voices)
            {
                if (v.isActive())
                {
                    v.setParams (params);
                    v.renderNextBlock (output, startSample + position, samplesToNext);
                }
            }

            position += samplesToNext;
        }

        // Consume every event scheduled at this exact position.
        while (iterator != midi.cend() && (*iterator).samplePosition <= position)
        {
            handleMidiEvent ((*iterator).getMessage());
            ++iterator;
        }

        // Nothing left to render and no further events — done.
        if (samplesToNext == 0 && iterator == midi.cend())
            break;
    }
}

//==============================================================================
void RippleSynth::handleMidiEvent (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
    {
        startNote (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        if (sustainPedalDown)
            sustainedNotes[(size_t) m.getNoteNumber()] = true;
        else
            stopNote (m.getNoteNumber(), true);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        allNotesOff (! m.isAllSoundOff());
    }
    else if (m.isPitchWheel())
    {
        pitchBend = ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
    }
    else if (m.isChannelPressure())
    {
        aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    }
    else if (m.isAftertouch())
    {
        aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();

        if (cc == 1)                    // mod wheel
        {
            modWheel = (float) m.getControllerValue() / 127.0f;
        }
        else if (cc == 64)              // sustain pedal
        {
            const bool down = m.getControllerValue() >= 64;

            if (sustainPedalDown && ! down)
            {
                // Pedal released — let go of everything it was holding.
                for (int n = 0; n < 128; ++n)
                {
                    if (sustainedNotes[(size_t) n])
                    {
                        stopNote (n, true);
                        sustainedNotes[(size_t) n] = false;
                    }
                }
            }
            sustainPedalDown = down;
        }
    }
}

//==============================================================================
RippleVoice* RippleSynth::findVoiceToUse (int note) noexcept
{
    // 1. Reuse a voice already sounding this note (retrigger).
    for (int i = 0; i < voiceLimit; ++i)
        if (voices[(size_t) i].isActive() && voices[(size_t) i].getCurrentNote() == note)
            return &voices[(size_t) i];

    // 2. Any free voice.
    for (int i = 0; i < voiceLimit; ++i)
        if (! voices[(size_t) i].isActive())
            return &voices[(size_t) i];

    // 3. Steal: prefer the quietest released voice, then the oldest voice.
    RippleVoice* bestReleasing = nullptr;
    float quietest = 1.0e9f;
    RippleVoice* oldest = nullptr;
    uint64_t oldestOrder = UINT64_MAX;

    for (int i = 0; i < voiceLimit; ++i)
    {
        auto& v = voices[(size_t) i];

        if (v.isReleasing() && v.getCurrentLevel() < quietest)
        {
            quietest = v.getCurrentLevel();
            bestReleasing = &v;
        }

        if (v.getStartOrder() < oldestOrder)
        {
            oldestOrder = v.getStartOrder();
            oldest = &v;
        }
    }

    auto* victim = bestReleasing != nullptr ? bestReleasing : oldest;

    if (victim != nullptr)
        victim->steal();    // short fade rather than a click

    return victim;
}

void RippleSynth::startNote (int note, float velocity) noexcept
{
    if (velocity <= 0.0f)
    {
        stopNote (note, true);
        return;
    }

    auto* voice = findVoiceToUse (note);

    if (voice == nullptr)
        return;

    // Legato when another voice is already sounding — drives glide behaviour.
    bool anyActive = false;
    for (int i = 0; i < voiceLimit; ++i)
        if (voices[(size_t) i].isActive() && ! voices[(size_t) i].isReleasing())
            anyActive = true;

    voice->setParams (params);
    voice->setStartOrder (++noteCounter);
    voice->noteOn (note, velocity, anyActive);

    lastTriggeredVoice = (int) (voice - voices.data());
}

void RippleSynth::stopNote (int note, bool allowTailOff) noexcept
{
    for (int i = 0; i < voiceLimit; ++i)
    {
        auto& v = voices[(size_t) i];

        if (v.isActive() && ! v.isReleasing() && v.getCurrentNote() == note)
        {
            if (allowTailOff)
                v.noteOff();
            else
                v.kill();
        }
    }
}

void RippleSynth::allNotesOff (bool allowTailOff) noexcept
{
    for (auto& v : voices)
    {
        if (! v.isActive())
            continue;

        if (allowTailOff)
            v.noteOff();
        else
            v.kill();
    }

    sustainedNotes.fill (false);
}

//==============================================================================
int RippleSynth::getActiveVoiceCount() const noexcept
{
    int n = 0;
    for (const auto& v : voices)
        if (v.isActive())
            ++n;
    return n;
}

void RippleSynth::publishVisualisation (VisualizationState& vis) noexcept
{
    vis.setActiveVoices (getActiveVoiceCount());

    // Report from the most recently triggered voice if it is still sounding,
    // otherwise from whichever voice is loudest — that is the one the user
    // perceives, so it is the honest thing to visualise.
    const RippleVoice* source = &voices[(size_t) lastTriggeredVoice];

    if (! source->isActive())
    {
        float loudest = -1.0f;
        for (const auto& v : voices)
        {
            if (v.isActive() && v.getCurrentLevel() > loudest)
            {
                loudest = v.getCurrentLevel();
                source = &v;
            }
        }
    }

    if (source->isActive())
    {
        vis.setAmpEnvValue (source->getAmpEnvValue());
        vis.setModEnvValue (source->getModEnvValue());
        vis.setTideValue (source->getTideValue());
        vis.setCurrentValue (source->getCurrentValue());
        vis.setDriftValue (source->getDriftValue());
        vis.setRippleValue (source->getRippleValue());
        vis.setFilterCutoffHz (source->getFilterCutoffHz());
    }

    // Forward any droplet impacts so the Fluid Field can splash.
    for (auto& v : voices)
    {
        float intensity = 0.0f, pan = 0.0f;
        while (v.consumeDropletEvent (intensity, pan))
            vis.pushDropletEvent (intensity, pan);
    }
}

} // namespace ripples
