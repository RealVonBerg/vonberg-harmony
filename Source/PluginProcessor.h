#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "HarmonyEngine.h"

//==============================================================================
class HarmonyProcessor : public juce::AudioProcessor,
                         private juce::AsyncUpdater,
                         private juce::Timer,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class ClipState { empty, waitingForBar, recording, analysing, rendering, ready, failed };
    enum class Export { mix = 0, harmonies, stems };

    HarmonyProcessor();
    ~HarmonyProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VonBerg Harmony"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // clip
    void toggleRecord();
    static bool isSupportedFile (const juce::String& path);
    void loadFile (const juce::File&, bool keepEdits = false);
    void clearClip();
    ClipState getClipState() const noexcept { return clipState.load(); }
    float getRecordSeconds() const noexcept { return recSeconds.load(); }
    juce::String getClipMessage() const { return clipMessage; }
    juce::String getClipName() const { return clipName; }

    std::shared_ptr<const vh::Analysis> getAnalysis() const { return analysis; }
    int getAnalysisVersion() const noexcept { return analysisVersion; }
    std::shared_ptr<const vh::Output> getRendered() const;
    int getRenderVersion() const noexcept { return renderVersion.load(); }
    juce::StringArray exportFiles (Export);

    void setPreview (bool on, int startSample = 0);
    bool isPreviewing() const noexcept { return previewing.load(); }
    double getPreviewSample() const noexcept { return previewSample.load(); }

    // key
    juce::String detectedKeyText() const;
    void applyDetectedKey();

    // harmony settings + note edits (message thread)
    vh::Settings currentSettings() const;
    const vh::Edits& getEdits() const noexcept { return edits; }
    int getTarget (int voice, int note) const;        // -1 = muted
    int getBaseTarget (int voice, int note) const;
    bool isEdited (int voice, int note) const;
    void setNoteOffset (int voice, int note, int offset);
    void toggleMute (int voice, int note);
    void resetNote (int voice, int note);
    void resetEdits (int voice);
    int getEditCount (int voice) const;

    // presets
    static int numPresets();
    static juce::String presetName (int);
    void applyPreset (int);
    void randomize();
    juce::String currentPresetName() const { return apvts.state.getProperty ("preset", presetName (0)).toString(); }
    int currentPresetIndex() const { return (int) apvts.state.getProperty ("presetIndex", 0); }

    int getChangeCount() const noexcept { return changeCount.load(); }   // params / edits changed (UI redraw)
    double getBpm() const noexcept { return hostBpm.load(); }
    float getParam (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float value);
    static juce::String vid (const char* name, int voice) { return juce::String (name) + juce::String (voice + 1); }

    juce::AudioProcessorValueTreeState apvts;

private:
    class AnalyseThread;
    class RenderThread;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void requestRender();
    void startRender();
    void startAnalysis (std::unique_ptr<juce::AudioBuffer<float>> buffer, const juce::File& file, bool keepEdits);
    juce::String editsToString() const;
    void editsFromString (const juce::String&);

    double sampleRate = 44100.0;
    std::atomic<double> hostBpm { 120.0 };

    // capture
    std::atomic<ClipState> clipState { ClipState::empty };
    juce::AudioBuffer<float> recBuffer;
    std::atomic<int> recPos { 0 };
    std::atomic<float> recSeconds { 0.0f };
    std::atomic<bool> recStopRequested { false };

    std::shared_ptr<const juce::AudioBuffer<float>> clip;     // dry vocal
    std::shared_ptr<const vh::Analysis> analysis;
    int analysisVersion = 0;
    vh::Edits edits;
    juce::String restoredEdits;
    juce::String clipName = "Vocal";
    juce::String clipMessage;

    juce::AudioFormatManager formats;
    std::unique_ptr<AnalyseThread> analyser;
    std::unique_ptr<RenderThread> renderer;
    std::atomic<bool> renderDirty { false };
    std::atomic<int> changeCount { 0 };
    int dirtyTicks = 0;

    juce::CriticalSection pendingLock;
    struct PendingAnalysis
    {
        std::shared_ptr<const juce::AudioBuffer<float>> clip;
        std::shared_ptr<const vh::Analysis> analysis;
        juce::String error;
        bool keepEdits = false;
        bool done = false;
    } pendingAnalysis;
    std::shared_ptr<const vh::Output> pendingRender;
    bool pendingRenderDone = false;

    juce::SpinLock renderLock;
    std::shared_ptr<const vh::Output> rendered;
    std::atomic<int> renderVersion { 0 };

    std::atomic<bool> previewing { false };
    std::atomic<double> previewSample { 0.0 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (HarmonyProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonyProcessor)
};
