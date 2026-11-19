#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// VonBerg Harmony - offline vocal harmony engine
//  1. analyse(): pitch track (YIN), note segmentation, key detection, pitch marks
//  2. render():  every harmony voice is a TD-PSOLA copy of the lead (formants kept),
//                note by note shifted to its target pitch, then mixed / exported.
namespace vh
{
    constexpr int numVoices = 4;

    struct Note
    {
        int start = 0, end = 0;     // samples
        int midi = 60;              // detected note (rounded)
        float pitch = 60.0f;        // median pitch (midi, with cents)
    };

    struct Analysis
    {
        double sampleRate = 44100.0;
        int hop = 256;
        int length = 0;
        std::vector<float> pitch;       // continuous midi per frame, 0 = unvoiced
        std::vector<float> level;       // frame rms
        std::vector<Note> notes;
        std::vector<int> marks;         // pitch marks (one per glottal period)
        std::vector<float> markPeriod;  // period at each mark (samples)
        std::vector<float> peaks;       // waveform overview (abs peak per column)
        int keyRoot = 0, keyScale = 0;  // detected key (major / minor)

        int numFrames() const noexcept { return (int) pitch.size(); }
    };

    enum ScaleId { major, minor, harmonicMinor, dorian, phrygian, mixolydian, lydian, hicaz, pentMajor, pentMinor, numScales };
    juce::StringArray scaleNames();
    const std::vector<int>& scaleSteps (int scale);
    juce::StringArray keyNames();
    juce::String noteName (int midi);
    bool inScale (int midi, int root, int scale);
    int diatonicShift (int midi, int steps, int root, int scale);
    int snapToScale (float midi, int root, int scale);

    enum IntervalMode { scaleMode = 0, semitoneMode = 1 };
    juce::String intervalName (int mode, int value);

    struct Voice
    {
        bool on = false;
        int mode = scaleMode;
        int interval = 2;          // scale steps (scaleMode) or semitones
        float level = 0.8f;        // 0..1
        float pan = 0.0f;          // -1..1
        float formant = 0.0f;      // semitones
        float delayMs = 0.0f;
        float detune = 0.0f;       // cents
        float humanize = 0.0f;     // 0..1
    };

    struct NoteEdit { int offset = 0; bool mute = false; };
    using VoiceEdits = std::vector<NoteEdit>;   // indexed by note
    using Edits = std::array<VoiceEdits, numVoices>;

    struct Settings
    {
        int keyRoot = 0, scale = major;
        float lead = 1.0f;        // dry vocal in the mix
        float harmony = 0.8f;     // harmony bus
        float breath = 0.35f;     // consonants / breaths in the harmony voices
        float tune = 0.0f;        // pitch lock of the harmony voices (0..1)
        float width = 1.0f;       // stereo spread
        std::array<Voice, numVoices> voices;
    };

    // -------- analysis
    std::shared_ptr<Analysis> analyse (const juce::AudioBuffer<float>& clip, double sampleRate,
                                       const std::atomic<bool>* abort = nullptr);
    float detectPitchHz (const float* x, int n, double sampleRate);   // single-shot YIN (tests)

    // -------- harmony notes
    int baseTarget (const Analysis&, int note, const Voice&, const Settings&);
    int finalTarget (const Analysis&, int note, const Voice&, const Settings&, const VoiceEdits&);   // -1 = muted

    // -------- render
    struct Output
    {
        juce::AudioBuffer<float> mix, harmonies;
        std::array<juce::AudioBuffer<float>, numVoices> stems;
        std::array<bool, numVoices> active {};
        std::vector<float> peaks;      // overview of the mix
        double sampleRate = 44100.0;
    };

    void renderVoice (const float* mono, const Analysis&, const Settings&, int voice, const VoiceEdits&,
                      std::vector<float>& out, juce::uint32 seed = 1);
    bool render (const juce::AudioBuffer<float>& clip, const Analysis&, const Settings&, const Edits&, Output&,
                 const std::atomic<bool>* abort = nullptr);
}
