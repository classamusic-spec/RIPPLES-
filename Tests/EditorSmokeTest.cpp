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

    // --- paint cost -------------------------------------------------------
    // The glass passes run on every frame in the Fluid Field, so measure the
    // real cost rather than assuming it is negligible.
    {
        editor->setSize (1600, 950);

        juce::Image frame (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);

        constexpr int warmup = 10;
        constexpr int frames = 120;

        for (int i = 0; i < warmup; ++i)
        {
            juce::Graphics g (frame);
            editor->paintEntireComponent (g, true);
        }

        const auto start = juce::Time::getHighResolutionTicks();

        for (int i = 0; i < frames; ++i)
        {
            juce::Graphics g (frame);
            editor->paintEntireComponent (g, true);
        }

        const auto seconds = juce::Time::highResolutionTicksToSeconds (
                                 juce::Time::getHighResolutionTicks() - start);

        const double msPerFrame = seconds * 1000.0 / (double) frames;

        std::printf ("paint cost: %.2f ms/frame at 1600x950  (%.0f fps headroom, 60fps budget is 16.7 ms)\n",
                     msPerFrame, 1000.0 / msPerFrame);
    }

    editor.reset();
    std::puts("editor destroyed cleanly");
    std::puts("EDITOR SMOKE TEST PASSED");
    return 0;
}
