/*
    Instantiates the processor and its editor, resizes it through the range the
    UI claims to support, and paints each size into an offscreen image. This is
    what catches null dereferences and layout divide-by-zeros in the views, none
    of which the audio tests can reach.
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/PluginProcessor.h"
#include "Parameters/ParameterIDs.h"

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

        auto& apvts = processor.getAPVTS();

        auto setParam = [&apvts] (const char* id, float value)
        {
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        };

        // Each of these is a behaviour the Fluid Field is supposed to show. A
        // single generic run proves only that something moves; running them
        // apart is the only way to tell whether CALM really rings longer than
        // CHAOS, or whether RIPPLE actually produces a standing pattern.
        struct Scene
        {
            const char* name;
            float fluidX, fluidY;     // CALM<->CHAOS, SURFACE<->DEPTH
            float ripple, drops;
            int   note;               // -1 for none
        };

        const Scene scenes[] = {
            { "calm",      0.06f, 0.30f, 0.10f, 0.00f, 45 },
            { "chaos",     0.95f, 0.45f, 0.10f, 0.00f, 60 },
            { "deep",      0.35f, 0.95f, 0.10f, 0.00f, 31 },
            { "drops",     0.25f, 0.40f, 0.00f, 0.95f, 55 },
            { "vibration", 0.40f, 0.40f, 0.95f, 0.00f, 67 },
            { "chord",     0.45f, 0.50f, 0.35f, 0.30f, -2 },
        };

        juce::AudioBuffer<float> audio (2, 512);

        for (const auto& scene : scenes)
        {
            setParam (pid::fluidX, scene.fluidX);
            setParam (pid::fluidY, scene.fluidY);
            setParam (pid::macroRipple, scene.ripple);
            setParam (pid::macroDrops, scene.drops);

            // Let the smoothers and the field settle on the new position, and
            // let whatever the previous scene left in the water die away.
            for (int i = 0; i < 90; ++i)
            {
                juce::MidiBuffer none;
                audio.clear();
                processor.processBlock (audio, none);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (4);
            }

            constexpr int kFrames = 120;

            // What the picture is actually being driven by. Guessing at this
            // from the screenshots alone wasted a round of tuning.
            float rippleMin = 1.0f, rippleMax = -1.0f, rmsMax = 0.0f;
            uint32_t drops0 = processor.getVisualisation().getDropletCount();

            for (int frame = 0; frame < kFrames; ++frame)
            {
                juce::MidiBuffer midi;

                if (frame == 4)
                {
                    if (scene.note == -2)
                    {
                        for (int semis : { 0, 7, 12, 19 })
                            midi.addEvent (juce::MidiMessage::noteOn (1, 40 + semis, 0.8f), 0);
                    }
                    else if (scene.note >= 0)
                    {
                        midi.addEvent (juce::MidiMessage::noteOn (1, scene.note, 0.9f), 0);
                    }
                }

                if (frame == 50)
                {
                    if (scene.note == -2)
                    {
                        for (int semis : { 0, 7, 12, 19 })
                            midi.addEvent (juce::MidiMessage::noteOff (1, 40 + semis), 0);
                    }
                    else if (scene.note >= 0)
                    {
                        midi.addEvent (juce::MidiMessage::noteOff (1, scene.note), 0);
                    }
                }

                // One video frame is about 735 samples at 44.1k; two blocks is
                // close enough and keeps the audio ahead of the picture.
                for (int b = 0; b < 2; ++b)
                {
                    audio.clear();
                    processor.processBlock (audio, midi);
                    midi.clear();
                }

                juce::MessageManager::getInstance()->runDispatchLoopUntil (16);

                {
                    auto& vis = processor.getVisualisation();
                    const float rv = vis.getRippleValue();
                    rippleMin = juce::jmin (rippleMin, rv);
                    rippleMax = juce::jmax (rippleMax, rv);
                    rmsMax    = juce::jmax (rmsMax, vis.getOutputRMS());
                }

                if (frame == 10 || frame == 20 || frame == 60 || frame == 110)
                {
                    juce::Image img (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
                    {
                        juce::Graphics g (img);
                        editor->paintEntireComponent (g, true);
                    }

                    auto out = juce::File::getCurrentWorkingDirectory()
                                   .getChildFile (juce::String ("ripples_anim_") + scene.name
                                                  + "_" + juce::String (frame) + ".png");
                    out.deleteFile();

                    if (auto stream = out.createOutputStream())
                    {
                        juce::PNGImageFormat png;
                        png.writeImageToStream (img, *stream);
                        std::printf ("  %-10s frame %3d -> %s\n", scene.name, frame,
                                     out.getFileName().toRawUTF8());
                    }
                }
            }

            std::printf ("  %-10s ripple %+.3f..%+.3f   peak rms %.3f   droplets %u\n",
                         scene.name, rippleMin, rippleMax, rmsMax,
                         processor.getVisualisation().getDropletCount() - drops0);
        }

        // Cost with the simulation actually running, which is the number that
        // matters -- the static figure above never touches the water.
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
    }

    editor.reset();
    std::puts("editor destroyed cleanly");
    std::puts("EDITOR SMOKE TEST PASSED");
    return 0;
}
