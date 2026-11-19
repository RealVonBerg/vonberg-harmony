// Offline checks of the harmony engine: note detection, key, diatonic maths, PSOLA pitch accuracy.
#include "../Source/HarmonyEngine.h"
#include <cstdio>
#include <cmath>

static int fails = 0;
#define CHECK(c, ...) do { if (! (c)) { ++fails; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

// vowel-like test vocal: glottal pulse train -> 3 formant resonators, vibrato, consonant noise between notes
static juce::AudioBuffer<float> makeVocal (double sr, const std::vector<int>& notes, double noteSec, double gapSec, double detune = 0.0)
{
    const int total = (int) (sr * (notes.size() * (noteSec + gapSec) + 0.2));
    juce::AudioBuffer<float> b (2, total);
    b.clear();
    juce::Random rnd (3);
    struct Res { double f, bw, y1 = 0, y2 = 0; };
    std::vector<Res> res { { 700, 90 }, { 1200, 110 }, { 2600, 160 } };
    double phase = 0.0;
    int pos = (int) (0.1 * sr);
    for (int m : notes)
    {
        const int len = (int) (noteSec * sr);
        for (int i = 0; i < len; ++i)
        {
            const double t = i / sr;
            const double vib = 0.25 * std::sin (2 * M_PI * 5.5 * t) * juce::jmin (1.0, t / 0.2);
            const double hz = 440.0 * std::pow (2.0, (m + detune + vib - 69) / 12.0);
            phase += hz / sr;
            double ex = 0.0;
            if (phase >= 1.0) { phase -= 1.0; ex = 1.0; }
            double y = 0;
            for (auto& r : res)
            {
                const double R = std::exp (-M_PI * r.bw / sr);
                const double a1 = 2 * R * std::cos (2 * M_PI * r.f / sr), a2 = -R * R;
                const double v = ex + a1 * r.y1 + a2 * r.y2;
                r.y2 = r.y1; r.y1 = v;
                y += v * (1 - R);
            }
            const double env = juce::jmin (1.0, juce::jmin (i / (0.02 * sr), (len - i) / (0.02 * sr)));
            const float s = (float) (y * env * 0.9);
            b.setSample (0, pos + i, s); b.setSample (1, pos + i, s);
        }
        pos += len;
        for (int i = 0; i < (int) (gapSec * sr * 0.5); ++i)   // "s" consonant
        {
            const float s = (rnd.nextFloat() * 2 - 1) * 0.02f;
            b.setSample (0, pos + i, s); b.setSample (1, pos + i, s);
        }
        pos += (int) (gapSec * sr);
    }
    const float pk = b.getMagnitude (0, total);
    b.applyGain (0.7f / pk);
    return b;
}

static float midiOf (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }

int main()
{
    // diatonic maths
    CHECK (vh::diatonicShift (60, 2, 0, vh::major) == 64, "C +3rd in C major");
    CHECK (vh::diatonicShift (64, 2, 0, vh::major) == 67, "E +3rd");
    CHECK (vh::diatonicShift (71, 2, 0, vh::major) == 74, "B +3rd");
    CHECK (vh::diatonicShift (60, -2, 0, vh::major) == 57, "C -3rd");
    CHECK (vh::diatonicShift (60, 7, 0, vh::major) == 72, "octave");
    CHECK (vh::diatonicShift (57, 2, 9, vh::minor) == 60, "A +3rd in A minor");
    CHECK (vh::diatonicShift (62, 2, 2, vh::hicaz) == 66, "D hicaz +3rd = F#");
    CHECK (vh::snapToScale (61.2f, 0, vh::major) == 60 || vh::snapToScale (61.2f, 0, vh::major) == 62, "snap");

    for (double sr : { 44100.0, 48000.0 })
    {
        const std::vector<int> mel { 60, 64, 67, 69, 65, 62, 59, 60 };
        auto clip = makeVocal (sr, mel, 0.5, 0.12);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto A = vh::analyse (clip, sr);
        std::printf ("sr %.0f: analysis %.0f ms, %d notes, key %s %s, %d marks\n", sr, juce::Time::getMillisecondCounterHiRes() - t0,
                     (int) A->notes.size(), vh::keyNames()[A->keyRoot].toRawUTF8(), vh::scaleNames()[A->keyScale].toRawUTF8(), (int) A->marks.size());
        CHECK (A->notes.size() == mel.size(), "note count %d (expected %d)", (int) A->notes.size(), (int) mel.size());
        for (size_t i = 0; i < juce::jmin (A->notes.size(), mel.size()); ++i)
            CHECK (A->notes[i].midi == mel[i], "note %d: %d expected %d", (int) i, A->notes[i].midi, mel[i]);
        CHECK ((A->keyRoot == 0 && A->keyScale == vh::major) || (A->keyRoot == 9 && A->keyScale == vh::minor), "key");

        vh::Settings S;
        S.keyRoot = 0; S.scale = vh::major; S.breath = 0.3f;
        struct Case { int mode, interval; float formant; };
        for (Case c : { Case { vh::scaleMode, 2, 0.0f }, Case { vh::scaleMode, -2, 0.0f }, Case { vh::scaleMode, 4, 0.0f },
                        Case { vh::semitoneMode, 12, 0.0f }, Case { vh::semitoneMode, -12, -3.0f }, Case { vh::semitoneMode, 7, 2.0f } })
        {
            S.voices[0] = {};
            S.voices[0].on = true; S.voices[0].mode = c.mode; S.voices[0].interval = c.interval; S.voices[0].formant = c.formant;
            vh::VoiceEdits e (A->notes.size());
            if (e.size() > 3) e[3].offset = 1;      // edited note
            if (e.size() > 5) e[5].mute = true;     // muted note
            std::vector<float> mono ((size_t) A->length), out;
            for (int i = 0; i < A->length; ++i) mono[(size_t) i] = clip.getSample (0, i);
            vh::renderVoice (mono.data(), *A, S, 0, e, out);
            float pk = 0; bool nan = false;
            for (float v : out) { pk = std::max (pk, std::abs (v)); nan |= ! std::isfinite (v); }
            CHECK (! nan && pk < 4.0f, "output sane (peak %.2f)", pk);
            int bad = 0;
            for (size_t i = 0; i < A->notes.size(); ++i)
            {
                const auto& nt = A->notes[i];
                const int mid = (nt.start + nt.end) / 2, w = (int) (0.2 * sr);
                const int expect = vh::finalTarget (*A, (int) i, S.voices[0], S, e);
                float rms = 0;
                for (int k = mid - w / 2; k < mid + w / 2; ++k) rms += out[(size_t) k] * out[(size_t) k];
                rms = std::sqrt (rms / w);
                if (expect < 0) { CHECK (rms < 0.01f, "muted note %d silent (rms %.3f)", (int) i, rms); continue; }
                const float hz = vh::detectPitchHz (out.data() + mid - w / 2, w, sr);
                const float got = midiOf (hz);
                if (std::abs (got - (float) expect) > 0.35f) { ++bad; std::printf ("   mode %d int %d note %d: got %.2f expected %d\n", c.mode, c.interval, (int) i, got, expect); }
                CHECK (rms > 0.02f, "voice audible note %d (rms %.3f)", (int) i, rms);
            }
            CHECK (bad == 0, "pitch accuracy mode %d interval %d", c.mode, c.interval);
        }

        // full render
        S.voices[0] = {}; S.voices[0].on = true; S.voices[0].interval = 2; S.voices[0].pan = -0.6f;
        S.voices[1].on = true; S.voices[1].interval = -3; S.voices[1].pan = 0.6f; S.voices[1].delayMs = 20;
        S.voices[2].on = true; S.voices[2].mode = vh::semitoneMode; S.voices[2].interval = 0; S.voices[2].detune = 10; S.voices[2].humanize = 1;
        vh::Edits E;
        vh::Output O;
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        CHECK (vh::render (clip, *A, S, E, O), "render");
        std::printf ("   render %.0f ms\n", juce::Time::getMillisecondCounterHiRes() - t1);
        CHECK (O.mix.getMagnitude (0, O.mix.getNumSamples()) <= 0.99f, "no clipping");
        CHECK (O.active[0] && O.active[1] && O.active[2] && ! O.active[3], "stems");
        CHECK (O.stems[1].getNumSamples() == O.mix.getNumSamples(), "stem len");
        CHECK (O.stems[0].getRMSLevel (0, 0, O.mix.getNumSamples()) > O.stems[0].getRMSLevel (1, 0, O.mix.getNumSamples()), "pan left");
    }

    // off-grid singer (almost a quarter tone sharp / flat) and legato (no gaps) must not get chopped up
    for (double det : { 0.47, -0.45, 0.3 })
    {
        const std::vector<int> mel { 62, 64, 65, 67, 69, 67, 65, 64 };
        auto clip = makeVocal (44100.0, mel, 0.45, det == 0.3 ? 0.0 : 0.1, det);
        auto A = vh::analyse (clip, 44100.0);
        CHECK (A->notes.size() == mel.size(), "detune %.2f: %d notes (expected %d)", det, (int) A->notes.size(), (int) mel.size());
    }

    // silence / tiny input must not crash
    {
        juce::AudioBuffer<float> z (2, 100); z.clear();
        auto A = vh::analyse (z, 44100.0);
        vh::Settings S; S.voices[0].on = true; vh::Edits E; vh::Output O;
        CHECK (vh::render (z, *A, S, E, O), "silence render");
        juce::AudioBuffer<float> s (1, 44100); s.clear();
        auto B = vh::analyse (s, 44100.0);
        CHECK (B->notes.empty(), "silence has no notes");
    }

    std::printf (fails == 0 ? "ALL ENGINE TESTS PASSED\n" : "%d FAILURES\n", fails);
    return fails == 0 ? 0 : 1;
}
