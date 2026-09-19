#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

#include "DSP/RippleVoice.h"
#include "Utilities/VisualizationState.h"

namespace ripples
{

/**
    The polyphonic engine: owns the voices, allocates and steals them, and
    dispatches MIDI. Global effects live in the processor, not here — this class
    is purely "notes in, voice sum out".
*/
class RippleSynth
{
public:
    RippleSynth() = default;

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /** Per-block parameters, broadcast to every voice. */
    void setParams (const VoiceParams& p) noexcept;

    /** Limits how many of the voices may sound (1..dsp::kMaxVoices). */
    void setVoiceLimit (int limit) noexcept;

    /** Renders the whole block, interleaving MIDI at sample-accurate positions.
        The buffer is cleared first, then every active voice is summed into it. */
    void renderNextBlock (juce::AudioBuffer<float>& output,
                          const juce::MidiBuffer& midi,
                          int startSample, int numSamples) noexcept;

    /** Publishes voice/modulator state for the editor. Realtime safe. */
    void publishVisualisation (VisualizationState& vis) noexcept;

    int getActiveVoiceCount() const noexcept;

private:
    void handleMidiEvent (const juce::MidiMessage& m) noexcept;
    void startNote (int note, float velocity) noexcept;
    void stopNote (int note, bool allowTailOff) noexcept;
    void allNotesOff (bool allowTailOff) noexcept;
    RippleVoice* findVoiceToUse (int note) noexcept;

    std::array<RippleVoice, dsp::kMaxVoices> voices;
    int voiceLimit = dsp::kMaxVoices;

    VoiceParams params;
    uint64_t noteCounter = 0;

    // Live controller state, folded into VoiceParams before each block.
    float modWheel = 0.0f, aftertouch = 0.0f, pitchBend = 0.0f;

    // Sustain pedal: notes released while held are deferred.
    bool sustainPedalDown = false;
    std::array<bool, 128> sustainedNotes {};

    double sampleRate = 44100.0;
    int lastTriggeredVoice = 0;

    JUCE_LEAK_DETECTOR (RippleSynth)
};

} // namespace ripples
