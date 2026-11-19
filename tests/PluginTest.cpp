// End-to-end: drop file -> analyse -> render, presets, note edits, export (mix / harmonies / stems), preview, REC, state, screenshots.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

static juce::AudioBuffer<float> vocal (double sr)
{
    const int notes[] { 57, 60, 64, 62, 60, 57, 55, 57, 59, 60 };
    const double noteSec = 0.42;
    juce::AudioBuffer<float> b (2, (int) (sr * (10 * noteSec + 0.3)));
    b.clear();
    struct Res { double f, bw, y1 = 0, y2 = 0; };
    std::vector<Res> res { { 650, 90 }, { 1100, 110 }, { 2500, 160 } };
    double phase = 0;
    for (int n = 0; n < 10; ++n)
    {
        const int start = (int) (sr * (0.1 + n * noteSec)), len = (int) (sr * noteSec * 0.85);
        for (int i = 0; i < len; ++i)
        {
            const double t = i / sr;
            const double hz = 440.0 * std::pow (2.0, (notes[n] + 0.2 * std::sin (2 * M_PI * 5.0 * t) - 69) / 12.0);
            phase += hz / sr;
            double ex = 0; if (phase >= 1) { phase -= 1; ex = 1; }
            double y = 0;
            for (auto& r : res)
            {
                const double R = std::exp (-M_PI * r.bw / sr);
                const double v = ex + 2 * R * std::cos (2 * M_PI * r.f / sr) * r.y1 - R * R * r.y2;
                r.y2 = r.y1; r.y1 = v; y += v * (1 - R);
            }
            const double env = juce::jmin (1.0, juce::jmin (i / (0.02 * sr), (len - i) / (0.02 * sr)));
            b.setSample (0, start + i, (float) (y * env)); b.setSample (1, start + i, (float) (y * env));
        }
    }
    b.applyGain (0.6f / b.getMagnitude (0, b.getNumSamples()));
    return b;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? argv[1] : "/tmp");
    int fails = 0;
    auto check = [&] (bool ok, const char* what, const juce::String& info = {})
    { std::printf ("%-36s %s %s\n", what, ok ? "OK " : "<-- FAIL", info.toRawUTF8()); if (! ok) ++fails; };

    const double sr = 48000.0;
    auto proc = std::make_unique<HarmonyProcessor>();
    proc->setPlayConfigDetails (2, 2, sr, 512);
    proc->prepareToPlay (sr, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
    auto shot = [&] (const char* name)
    {
        pump (200);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = out.getChildFile (name); f.deleteFile();
        juce::FileOutputStream fos (f);
        juce::PNGImageFormat().writeImageToStream (img, fos);
    };
    shot ("hm_empty.png");

    // write test vocal at 44.1k (forces resampling to 48k)
    auto v = vocal (44100.0);
    auto wavFile = out.getChildFile ("hm_test_vocal.wav");
    {
        wavFile.deleteFile();
        auto os = wavFile.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> w (juce::WavAudioFormat().createWriterFor (os.get(), 44100.0, 2, 24, {}, 0));
        os.release();
        w->writeFromAudioSampleBuffer (v, 0, v.getNumSamples());
    }
    auto waitReady = [&] (int ms = 8000)
    {
        for (int t = 0; t < ms / 20; ++t)
        {
            pump (20);
            if (proc->getClipState() == HarmonyProcessor::ClipState::ready && proc->getRendered() != nullptr) return true;
            if (proc->getClipState() == HarmonyProcessor::ClipState::failed) return false;
        }
        return false;
    };
    auto waitRender = [&] (int prevVersion)
    {
        for (int t = 0; t < 400; ++t) { pump (20); if (proc->getRenderVersion() > prevVersion && proc->getClipState() == HarmonyProcessor::ClipState::ready) return true; }
        return false;
    };

    check (HarmonyProcessor::isSupportedFile (wavFile.getFullPathName()), "wav supported");
    proc->loadFile (wavFile);
    check (waitReady(), "load + analyse + render", proc->getClipMessage());
    auto an = proc->getAnalysis();
    check (an != nullptr && an->notes.size() == 10, "10 notes found", juce::String (an ? (int) an->notes.size() : -1));
    check (proc->detectedKeyText().isNotEmpty(), "key detected", proc->detectedKeyText());
    check ((int) proc->getParam ("key") == an->keyRoot, "key param follows detection");
    std::printf ("   notes:");
    for (auto& n : an->notes) std::printf (" %s", vh::noteName (n.midi).toRawUTF8());
    std::printf ("\n");
    auto r = proc->getRendered();
    check (r->mix.getNumSamples() >= an->length && r->active[0] && ! r->active[1], "default render (voice A)");
    shot ("hm_loaded.png");

    // presets
    int ver = proc->getRenderVersion();
    proc->applyPreset (11);   // Four-Part Stack
    check (waitRender (ver), "preset render", HarmonyProcessor::presetName (11));
    r = proc->getRendered();
    int act = 0; for (bool a : r->active) act += a ? 1 : 0;
    check (act == 4, "four voices active", juce::String (act));
    for (int i = 0; i < HarmonyProcessor::numPresets(); ++i) proc->applyPreset (i);
    ver = proc->getRenderVersion();
    proc->applyPreset (8);    // Triad
    check (waitRender (ver), "triad render");

    // note edits
    const int base = proc->getBaseTarget (0, 2);
    proc->setNoteOffset (0, 2, 3);
    check (proc->getTarget (0, 2) == base + 3, "note offset");
    proc->toggleMute (1, 4);
    check (proc->getTarget (1, 4) == -1 && proc->getEditCount (1) == 1, "note mute");
    ver = proc->getRenderVersion();
    check (waitRender (ver), "edit re-render");
    shot ("hm_edited.png");

    // export
    auto mixF = proc->exportFiles (HarmonyProcessor::Export::mix);
    auto harmF = proc->exportFiles (HarmonyProcessor::Export::harmonies);
    auto stemF = proc->exportFiles (HarmonyProcessor::Export::stems);
    check (mixF.size() == 1 && juce::File (mixF[0]).getSize() > 100000, "export mix", mixF[0]);
    check (harmF.size() == 1, "export harmonies", harmF[0]);
    check (stemF.size() == 2, "export stems (2 voices)", stemF.joinIntoString (" | "));

    // preview adds the render to the output
    {
        proc->setPreview (true, (int) (0.5 * sr));
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        float e = 0;
        for (int k = 0; k < 40; ++k) { b.clear(); proc->processBlock (b, m); e += b.getRMSLevel (0, 0, 512); }
        check (e > 0.5f && proc->isPreviewing(), "preview audible", juce::String (e));
        proc->setPreview (false);
    }

    // state round-trip keeps edits
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    {
        auto p2 = std::make_unique<HarmonyProcessor>();
        p2->setPlayConfigDetails (2, 2, sr, 512);
        p2->prepareToPlay (sr, 512);
        p2->setStateInformation (state.getData(), (int) state.getSize());
        bool ok = false;
        for (int t = 0; t < 400 && ! ok; ++t) { pump (20); ok = p2->getClipState() == HarmonyProcessor::ClipState::ready; }
        check (ok, "state reload");
        check (p2->getTarget (0, 2) == base + 3 && p2->getTarget (1, 4) == -1, "edits restored");
        check ((int) p2->getParam ("on3") == 0 && p2->getParam ("on2") > 0.5f, "voice params restored");
    }

    // REC capture (transport stopped -> starts immediately)
    {
        proc->toggleRecord();
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
        for (int pos = 0; pos + 512 < v.getNumSamples(); pos += 512)
        {
            for (int c = 0; c < 2; ++c) b.copyFrom (c, 0, v, c, pos, 512);
            proc->processBlock (b, m);
        }
        proc->toggleRecord();
        b.clear(); proc->processBlock (b, m);
        check (waitReady(), "REC -> analyse -> render", proc->getClipMessage());
        check (proc->getAnalysis() != nullptr && proc->getAnalysis()->notes.size() >= 9, "REC notes",
               juce::String (proc->getAnalysis() ? (int) proc->getAnalysis()->notes.size() : -1));
    }

    // silence file -> friendly error
    {
        juce::AudioBuffer<float> z (2, 44100); z.clear();
        auto zf = out.getChildFile ("hm_silence.wav"); zf.deleteFile();
        auto os = zf.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> w (juce::WavAudioFormat().createWriterFor (os.get(), 44100.0, 2, 16, {}, 0));
        os.release(); w->writeFromAudioSampleBuffer (z, 0, z.getNumSamples()); w.reset();
        proc->loadFile (zf);
        for (int t = 0; t < 200 && proc->getClipState() != HarmonyProcessor::ClipState::failed; ++t) pump (20);
        check (proc->getClipState() == HarmonyProcessor::ClipState::failed, "silence -> error", proc->getClipMessage());
        proc->loadFile (wavFile);
        waitReady();
    }

    proc->applyPreset (12);   // Gospel
    pump (600);
    shot ("hm_gospel.png");
    ed.reset();
    proc.reset();
    std::printf (fails == 0 ? "ALL PLUGIN TESTS PASSED\n" : "%d FAILURES\n", fails);
    return fails == 0 ? 0 : 1;
}
