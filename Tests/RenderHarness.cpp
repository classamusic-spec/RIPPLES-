/*
    RIPPLES — offline render harness.

    Instantiates the real plug-in, plays notes into it and writes the result to a
    WAV file so the sound can be measured rather than guessed at.

        RIPPLES_Render --preset 0 --out init_deep_saw.wav --seconds 4 --note 48
        RIPPLES_Render --preset 3 --out dry.wav --dry        # global FX bypassed

    Analyse the result with tools/analyse.py.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Core/PluginProcessor.h"
#include "Parameters/ParameterIDs.h"

using namespace ripples;

namespace
{

void setParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float normalised)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

/** Silences every global effect so a patch can be judged on its synthesis alone —
    this is the "dry aquatic test" from the product spec. */
void bypassGlobalEffects (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* id : { pid::chorEnable, pid::dlyEnable, pid::diffEnable,
                      pid::verbEnable, pid::scurEnable })
        setParam (apvts, id, 0.0f);

    for (auto* id : { pid::chorMix, pid::dlyMix, pid::diffMix, pid::verbMix })
        setParam (apvts, id, 0.0f);
}

/** Zeroes the eight primary macros, isolating a preset's own programming from
    whatever the macro layer adds on top. */
void zeroMacros (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* id : { pid::macroDepth, pid::macroWet, pid::macroRipple, pid::macroCurrent,
                      pid::macroDrops, pid::macroPressure, pid::macroSpace, pid::macroGlow })
        setParam (apvts, id, 0.0f);
}

/** Applies any --set <paramID>=<normalised> overrides, so a single parameter can
    be isolated without editing a preset. */
void applyOverrides (juce::AudioProcessorValueTreeState& apvts, const juce::StringArray& args)
{
    for (int i = 0; i + 1 < args.size(); ++i)
    {
        if (args[i] != "--set")
            continue;

        const auto pair = args[i + 1];
        const auto eq = pair.indexOfChar ('=');

        if (eq <= 0)
            continue;

        const auto id = pair.substring (0, eq);
        const auto value = pair.substring (eq + 1).getFloatValue();

        if (apvts.getParameter (id) == nullptr)
            std::fprintf (stderr, "warning: unknown parameter '%s'\n", id.toRawUTF8());
        else
            setParam (apvts, id, value);
    }
}

int getIntArg (const juce::StringArray& args, const juce::String& flag, int fallback)
{
    const int i = args.indexOf (flag);
    return (i >= 0 && i + 1 < args.size()) ? args[i + 1].getIntValue() : fallback;
}

double getDoubleArg (const juce::StringArray& args, const juce::String& flag, double fallback)
{
    const int i = args.indexOf (flag);
    return (i >= 0 && i + 1 < args.size()) ? args[i + 1].getDoubleValue() : fallback;
}

juce::String getStringArg (const juce::StringArray& args, const juce::String& flag,
                           const juce::String& fallback)
{
    const int i = args.indexOf (flag);
    return (i >= 0 && i + 1 < args.size()) ? args[i + 1] : fallback;
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (juce::String (argv[i]));

    const int    presetIndex = getIntArg    (args, "--preset", -1);
    const double seconds     = getDoubleArg (args, "--seconds", 4.0);
    const double sampleRate  = getDoubleArg (args, "--rate", 44100.0);
    const int    blockSize   = getIntArg    (args, "--block", 512);
    const int    rootNote    = getIntArg    (args, "--note", 48);
    const bool   chord       = args.contains ("--chord");
    const bool   dry         = args.contains ("--dry");
    const auto   outPath     = getStringArg (args, "--out", "render.wav");

    RipplesAudioProcessor processor;

    if (args.contains ("--list"))
    {
        for (int i = 0; i < processor.getNumPrograms(); ++i)
            std::printf ("%d\t%s\n", i, processor.getProgramName (i).toRawUTF8());

        return 0;
    }

    if (presetIndex >= 0)
        processor.loadFactoryPreset (presetIndex);

    if (dry)
        bypassGlobalEffects (processor.getAPVTS());

    if (args.contains ("--zero-macros"))
        zeroMacros (processor.getAPVTS());

    applyOverrides (processor.getAPVTS(), args);

    processor.setPlayConfigDetails (0, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    const int totalSamples = (int) (seconds * sampleRate);
    // Hold the note for 60% of the render so the release tail is captured too.
    const int noteOffSample = (int) (totalSamples * 0.6);

    juce::AudioBuffer<float> outputFile (2, totalSamples);
    outputFile.clear();

    juce::AudioBuffer<float> block (2, blockSize);

    juce::Array<int> notes;
    if (chord)
        notes = { rootNote, rootNote + 7, rootNote + 12, rootNote + 15 };   // min-ish voicing
    else
        notes = { rootNote };

    bool notesStarted = false, notesStopped = false;
    int position = 0;

    while (position < totalSamples)
    {
        const int numSamples = juce::jmin (blockSize, totalSamples - position);
        block.setSize (2, numSamples, false, false, true);
        block.clear();

        juce::MidiBuffer midi;

        if (! notesStarted)
        {
            for (auto n : notes)
                midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
            notesStarted = true;
        }

        if (! notesStopped && position + numSamples > noteOffSample)
        {
            const int offset = juce::jlimit (0, numSamples - 1, noteOffSample - position);
            for (auto n : notes)
                midi.addEvent (juce::MidiMessage::noteOff (1, n), offset);
            notesStopped = true;
        }

        processor.processBlock (block, midi);

        for (int ch = 0; ch < 2; ++ch)
            outputFile.copyFrom (ch, position, block, ch, 0, numSamples);

        position += numSamples;
    }

    // Report anything that would make the render meaningless before writing it.
    int nonFinite = 0;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = outputFile.getReadPointer (ch);
        for (int i = 0; i < totalSamples; ++i)
            if (! std::isfinite (d[i]))
                ++nonFinite;
    }

    if (nonFinite > 0)
        std::fprintf (stderr, "RENDER ERROR: %d non-finite samples\n", nonFinite);

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
    out.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());

    if (stream == nullptr)
    {
        std::fprintf (stderr, "RENDER ERROR: could not open %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.release(), sampleRate, 2, 24, {}, 0));

    if (writer == nullptr)
    {
        std::fprintf (stderr, "RENDER ERROR: could not create WAV writer\n");
        return 1;
    }

    writer->writeFromAudioSampleBuffer (outputFile, 0, totalSamples);
    writer.reset();

    std::printf ("wrote %s  (%.2f s, %.0f Hz, peak %.4f)\n",
                 out.getFullPathName().toRawUTF8(), seconds, sampleRate,
                 juce::jmax (outputFile.getMagnitude (0, 0, totalSamples),
                             outputFile.getMagnitude (1, 0, totalSamples)));

    return nonFinite > 0 ? 1 : 0;
}
