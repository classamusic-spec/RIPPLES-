/*
    Instantiates the processor and its editor, resizes it through the range the
    UI claims to support, and paints each size into an offscreen image. This is
    what catches null dereferences and layout divide-by-zeros in the views, none
    of which the audio tests can reach.
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/PluginProcessor.h"

using namespace ripples;

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    RipplesAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::puts("FAIL: createEditor returned nullptr");
        return 1;
    }

    std::printf ("editor created: %d x %d\n", editor->getWidth(), editor->getHeight());

    // Paint at the documented default, the minimum, a scaled-up size, and a
    // couple of awkward intermediates.
    const std::pair<int,int> sizes[] = {
        {1280, 760}, {1011, 600}, {1600, 950}, {2560, 1520}, {1120, 700}, {1024, 620}
    };

    for (auto [w, h] : sizes)
    {
        editor->setSize (w, h);

        juce::Image img (juce::Image::ARGB, juce::jmax (1, editor->getWidth()),
                         juce::jmax (1, editor->getHeight()), true);
        {
            juce::Graphics g (img);
            editor->paintEntireComponent (g, true);
        }

        // A completely blank frame would mean nothing actually drew.
        int nonBlank = 0;
        for (int y = 0; y < img.getHeight(); y += 7)
            for (int x = 0; x < img.getWidth(); x += 7)
                if (img.getPixelAt (x, y).getBrightness() > 0.02f)
                    ++nonBlank;

        std::printf ("  %4d x %4d  painted, %d sampled pixels above black\n",
                     editor->getWidth(), editor->getHeight(), nonBlank);

        if (nonBlank == 0)
        {
            std::puts("FAIL: frame painted completely black");
            return 1;
        }

        // Write the frame out so the interface can actually be looked at.
        juce::File out (juce::File::getCurrentWorkingDirectory()
                            .getChildFile ("ripples_" + juce::String (w) + "x"
                                           + juce::String (h) + ".png"));
        out.deleteFile();

        if (auto stream = out.createOutputStream())
        {
            juce::PNGImageFormat png;
            png.writeImageToStream (img, *stream);
            std::printf ("    wrote %s\n", out.getFullPathName().toRawUTF8());
        }
    }

    // Exercise a preset change with the editor open — this is where a stale
    // callback into a destroyed view would show up.
    for (int i = 0; i < juce::jmin (6, processor.getNumPrograms()); ++i)
    {
        processor.setCurrentProgram (i);
        juce::Image img (juce::Image::ARGB, 1280, 760, true);
        juce::Graphics g (img);
        editor->paintEntireComponent (g, true);
    }
    std::puts("preset changes with editor open: ok");

    editor.reset();
    std::puts("editor destroyed cleanly");
    std::puts("EDITOR SMOKE TEST PASSED");
    return 0;
}
