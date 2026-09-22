/*
    Instantiates the processor and its editor, resizes it through the range the
    UI claims to support, and paints each size into an offscreen image. This is
    what catches null dereferences and layout divide-by-zeros in the views, none
    of which the audio tests can reach.
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/PluginProcessor.h"

#if JUCE_LINUX
 #include <X11/Xlib.h>

 // Xvfb without a window manager is missing some of the atoms JUCE sets on a
 // new window, and Xlib's default error handler answers a BadAtom by calling
 // exit(1) -- which killed this test before a single animated frame was drawn.
 // Nothing here depends on those properties, so swallow X errors.
 static int ignoreXErrors (Display*, XErrorEvent*)  { return 0; }
#endif

using namespace ripples;

int main (int argc, char** argv)
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

    // --- animated pass -----------------------------------------------------
    // The Fluid Field's water is a running simulation, so a single static paint
    // shows a flat surface and proves nothing. This pass puts the editor on a
    // real peer (so its timer runs), plays chords through the processor, and
    // writes a frame every so often — which is the only way to actually look at
    // the ripples, the wake and the caustics.
    bool animate = false;
    for (int i = 1; i < argc; ++i)
        if (juce::String (argv[i]) == "--animate")
            animate = true;

    if (animate)
    {
       #if JUCE_LINUX
        XSetErrorHandler (ignoreXErrors);
       #endif

        editor->setSize (1600, 950);
        editor->setVisible (true);

        // A real peer is what makes isShowing() true, which is what starts the
        // Fluid Field's timer. Plain flags: the temporary-window hint asks the
        // X server for atoms a bare Xvfb without a window manager does not have.
        editor->addToDesktop (0);

        juce::AudioBuffer<float> audio (2, 512);
        int sampleClock = 0;

        // A few notes spread across the keyboard: low ones should push broad
        // slow swells, high ones tight fast ripples.
        const int notes[] = { 36, 52, 67, 79, 45, 60 };
        int nextNote = 0;

        constexpr int kFrames = 150;          // ~2.5 seconds at 60 Hz
        constexpr int kShotEvery = 25;

        for (int frame = 0; frame < kFrames; ++frame)
        {
            juce::MidiBuffer midi;

            if (frame % 18 == 0)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, notes[nextNote % 6], 0.85f), 0);
                ++nextNote;
            }
            if (frame % 18 == 12 && nextNote > 0)
                midi.addEvent (juce::MidiMessage::noteOff (1, notes[(nextNote - 1) % 6]), 0);

            // One video frame is about 735 samples at 44.1k; two blocks is close
            // enough and keeps the audio ahead of the picture.
            for (int b = 0; b < 2; ++b)
            {
                audio.clear();
                processor.processBlock (audio, midi);
                midi.clear();
                sampleClock += 512;
            }

            // One frame's worth of message loop, which is what lets the Fluid
            // Field's timer fire and step the water.
            juce::MessageManager::getInstance()->runDispatchLoopUntil (16);

            if (frame % kShotEvery == kShotEvery - 1)
            {
                juce::Image img (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
                {
                    juce::Graphics g (img);
                    editor->paintEntireComponent (g, true);
                }

                auto out = juce::File::getCurrentWorkingDirectory()
                               .getChildFile ("ripples_anim_" + juce::String (frame + 1) + ".png");
                out.deleteFile();

                if (auto stream = out.createOutputStream())
                {
                    juce::PNGImageFormat png;
                    png.writeImageToStream (img, *stream);
                    std::printf ("  animated frame %3d -> %s\n", frame + 1,
                                 out.getFileName().toRawUTF8());
                }
            }
        }

        // Cost with the simulation actually running, which is the number that
        // matters — the static figure above never touches the water.
        {
            juce::Image frame (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);

            const auto start = juce::Time::getHighResolutionTicks();
            constexpr int n = 90;

            for (int i = 0; i < n; ++i)
            {
                juce::Graphics g (frame);
                editor->paintEntireComponent (g, true);
            }

            const auto sec = juce::Time::highResolutionTicksToSeconds (
                                 juce::Time::getHighResolutionTicks() - start);

            std::printf ("paint cost with water running: %.2f ms/frame\n", sec * 1000.0 / n);
        }

        editor->removeFromDesktop();
        juce::ignoreUnused (sampleClock);
    }

    editor.reset();
    std::puts("editor destroyed cleanly");
    std::puts("EDITOR SMOKE TEST PASSED");
    return 0;
}
