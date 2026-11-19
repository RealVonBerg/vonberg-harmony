#include "PluginProcessor.h"
#include "PluginEditor.h"

static constexpr double maxRecSeconds = 60.0;
static constexpr double maxFileSeconds = 180.0;

//==============================================================================
static juce::File outputFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("VonBerg Harmony");
    d.createDirectory();
    return d;
}

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    auto os = f.createOutputStream();
    if (os == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned int) b.getNumChannels(), 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

//==============================================================================
class HarmonyProcessor::AnalyseThread : public juce::Thread
{
public:
    AnalyseThread (HarmonyProcessor& o, std::unique_ptr<juce::AudioBuffer<float>> b, juce::File f, double targetRate, bool keep)
        : juce::Thread ("Harmony analyse"), owner (o), buffer (std::move (b)), file (std::move (f)), rate (targetRate), keepEdits (keep) {}
    ~AnalyseThread() override { abort.store (true); stopThread (10000); }

    void run() override
    {
        if (buffer == nullptr)
        {
            std::unique_ptr<juce::AudioFormatReader> r (owner.formats.createReaderFor (file));
            if (r == nullptr || r->lengthInSamples <= 0 || r->sampleRate <= 0.0) return done (nullptr, nullptr, "Can't read this file.");
            if ((double) r->lengthInSamples / r->sampleRate > maxFileSeconds) return done (nullptr, nullptr, "File is too long (max 3 min).");

            juce::AudioBuffer<float> src ((int) juce::jmax (1u, r->numChannels), (int) r->lengthInSamples);
            r->read (&src, 0, (int) r->lengthInSamples, 0, true, true);
            if (threadShouldExit()) return;

            buffer = std::make_unique<juce::AudioBuffer<float>>();
            const double ratio = r->sampleRate / rate;
            const int outLen = (int) std::ceil (src.getNumSamples() / ratio);
            buffer->setSize (2, outLen);
            buffer->clear();
            for (int c = 0; c < 2; ++c)
            {
                const float* in = src.getReadPointer (juce::jmin (c, src.getNumChannels() - 1));
                if (std::abs (ratio - 1.0) < 1.0e-9) buffer->copyFrom (c, 0, in, juce::jmin (outLen, src.getNumSamples()));
                else
                {
                    juce::LagrangeInterpolator li;
                    li.process (ratio, in, buffer->getWritePointer (c), outLen, src.getNumSamples(), 0);
                }
            }
        }
        auto a = vh::analyse (*buffer, rate, &abort);
        if (a == nullptr || threadShouldExit()) return;
        if (a->notes.empty()) return done (nullptr, nullptr, "No sung notes found in this audio.");
        done (std::shared_ptr<const juce::AudioBuffer<float>> (std::move (buffer)), a, {});
    }

private:
    void done (std::shared_ptr<const juce::AudioBuffer<float>> b, std::shared_ptr<const vh::Analysis> a, const juce::String& err)
    {
        {
            const juce::ScopedLock sl (owner.pendingLock);
            owner.pendingAnalysis = { std::move (b), std::move (a), err, keepEdits, true };
        }
        owner.triggerAsyncUpdate();
    }
    HarmonyProcessor& owner;
    std::unique_ptr<juce::AudioBuffer<float>> buffer;
    juce::File file;
    double rate;
    bool keepEdits;
    std::atomic<bool> abort { false };
};

//==============================================================================
class HarmonyProcessor::RenderThread : public juce::Thread
{
public:
    RenderThread (HarmonyProcessor& o, std::shared_ptr<const juce::AudioBuffer<float>> c, std::shared_ptr<const vh::Analysis> a,
                  vh::Settings s, vh::Edits e)
        : juce::Thread ("Harmony render"), owner (o), clipAudio (std::move (c)), an (std::move (a)), settings (s), ed (std::move (e)) {}
    ~RenderThread() override { abort.store (true); stopThread (10000); }

    void run() override
    {
        auto out = std::make_shared<vh::Output>();
        if (! vh::render (*clipAudio, *an, settings, ed, *out, &abort) || threadShouldExit()) return;
        {
            const juce::ScopedLock sl (owner.pendingLock);
            owner.pendingRender = out;
            owner.pendingRenderDone = true;
        }
        owner.triggerAsyncUpdate();
    }

private:
    HarmonyProcessor& owner;
    std::shared_ptr<const juce::AudioBuffer<float>> clipAudio;
    std::shared_ptr<const vh::Analysis> an;
    vh::Settings settings;
    vh::Edits ed;
    std::atomic<bool> abort { false };
};

//==============================================================================
HarmonyProcessor::HarmonyProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VBHARMONY", createLayout())
{
    formats.registerBasicFormats();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->getParameterID(), this);
    startTimerHz (10);
}

HarmonyProcessor::~HarmonyProcessor()
{
    stopTimer();
    cancelPendingUpdate();
    analyser.reset();
    renderer.reset();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

juce::AudioProcessorValueTreeState::ParameterLayout HarmonyProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [&] (const String& id, const String& name, float lo, float hi, float def, const String& unit = "%", float step = 1.0f)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def,
                                                      AudioParameterFloatAttributes().withLabel (unit)));
    };

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", vh::keyNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", vh::scaleNames(), 0));
    pct ("lead", "Lead Vocal", 0.0f, 100.0f, 100.0f);
    pct ("harmony", "Harmony Level", 0.0f, 100.0f, 80.0f);
    pct ("breath", "Breath", 0.0f, 100.0f, 35.0f);
    pct ("tune", "Pitch Lock", 0.0f, 100.0f, 0.0f);
    pct ("width", "Width", 0.0f, 100.0f, 100.0f);

    static const int defInt[] { 2, -2, 4, -12 };
    static const int defMode[] { vh::scaleMode, vh::scaleMode, vh::scaleMode, vh::semitoneMode };
    static const float defPan[] { -30.0f, 30.0f, -60.0f, 60.0f };
    for (int v = 0; v < vh::numVoices; ++v)
    {
        const String n = String ("Voice ") + String::charToString ((juce_wchar) ('A' + v)) + " ";
        l.add (std::make_unique<AudioParameterBool> (ParameterID { vid ("on", v), 1 }, n + "On", v == 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { vid ("mode", v), 1 }, n + "Mode", StringArray { "Scale", "Semitones" }, defMode[v]));
        l.add (std::make_unique<AudioParameterInt> (ParameterID { vid ("int", v), 1 }, n + "Interval", -24, 24, defInt[v]));
        pct (vid ("lvl", v), n + "Level", 0.0f, 100.0f, 80.0f);
        pct (vid ("pan", v), n + "Pan", -100.0f, 100.0f, defPan[v], "");
        pct (vid ("fmt", v), n + "Formant", -12.0f, 12.0f, 0.0f, "st", 0.5f);
        pct (vid ("dly", v), n + "Delay", 0.0f, 80.0f, 0.0f, "ms");
        pct (vid ("det", v), n + "Detune", -50.0f, 50.0f, 0.0f, "ct");
        pct (vid ("hum", v), n + "Humanize", 0.0f, 100.0f, 15.0f);
    }
    return l;
}

void HarmonyProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

bool HarmonyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void HarmonyProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0.0 ? sr : 44100.0;
    recBuffer.setSize (2, (int) (sampleRate * maxRecSeconds) + 1);
}

//==============================================================================
vh::Settings HarmonyProcessor::currentSettings() const
{
    vh::Settings s;
    s.keyRoot = (int) getParam ("key");
    s.scale = (int) getParam ("scale");
    s.lead = getParam ("lead") / 100.0f;
    s.harmony = getParam ("harmony") / 100.0f;
    s.breath = getParam ("breath") / 100.0f;
    s.tune = getParam ("tune") / 100.0f;
    s.width = getParam ("width") / 100.0f;
    for (int v = 0; v < vh::numVoices; ++v)
    {
        auto& o = s.voices[(size_t) v];
        o.on = getParam (vid ("on", v)) > 0.5f;
        o.mode = (int) getParam (vid ("mode", v));
        o.interval = (int) getParam (vid ("int", v));
        if (o.mode == vh::scaleMode) o.interval = juce::jlimit (-14, 14, o.interval);
        o.level = getParam (vid ("lvl", v)) / 100.0f;
        o.pan = getParam (vid ("pan", v)) / 100.0f;
        o.formant = getParam (vid ("fmt", v));
        o.delayMs = getParam (vid ("dly", v));
        o.detune = getParam (vid ("det", v));
        o.humanize = getParam (vid ("hum", v)) / 100.0f;
    }
    return s;
}

int HarmonyProcessor::getBaseTarget (int voice, int note) const
{
    if (analysis == nullptr || note < 0 || note >= (int) analysis->notes.size()) return -1;
    const auto s = currentSettings();
    return vh::baseTarget (*analysis, note, s.voices[(size_t) voice], s);
}

int HarmonyProcessor::getTarget (int voice, int note) const
{
    if (analysis == nullptr || note < 0 || note >= (int) analysis->notes.size()) return -1;
    const auto s = currentSettings();
    return vh::finalTarget (*analysis, note, s.voices[(size_t) voice], s, edits[(size_t) voice]);
}

bool HarmonyProcessor::isEdited (int voice, int note) const
{
    const auto& e = edits[(size_t) voice];
    return note >= 0 && note < (int) e.size() && (e[(size_t) note].offset != 0 || e[(size_t) note].mute);
}

int HarmonyProcessor::getEditCount (int voice) const
{
    int c = 0;
    for (auto& e : edits[(size_t) voice]) if (e.offset != 0 || e.mute) ++c;
    return c;
}

void HarmonyProcessor::setNoteOffset (int voice, int note, int offset)
{
    auto& e = edits[(size_t) voice];
    if (note < 0 || note >= (int) e.size() || e[(size_t) note].offset == offset) return;
    e[(size_t) note].offset = offset;
    requestRender();
}

void HarmonyProcessor::toggleMute (int voice, int note)
{
    auto& e = edits[(size_t) voice];
    if (note < 0 || note >= (int) e.size()) return;
    e[(size_t) note].mute = ! e[(size_t) note].mute;
    requestRender();
}

void HarmonyProcessor::resetNote (int voice, int note)
{
    auto& e = edits[(size_t) voice];
    if (note < 0 || note >= (int) e.size()) return;
    e[(size_t) note] = {};
    requestRender();
}

void HarmonyProcessor::resetEdits (int voice)
{
    for (auto& e : edits[(size_t) voice]) e = {};
    requestRender();
}

juce::String HarmonyProcessor::editsToString() const
{
    juce::StringArray items;
    for (int v = 0; v < vh::numVoices; ++v)
        for (size_t i = 0; i < edits[(size_t) v].size(); ++i)
        {
            const auto& e = edits[(size_t) v][i];
            if (e.offset != 0 || e.mute) items.add (juce::String (v) + ":" + juce::String ((int) i) + ":" + juce::String (e.offset) + ":" + (e.mute ? "1" : "0"));
        }
    return items.joinIntoString (";");
}

void HarmonyProcessor::editsFromString (const juce::String& s)
{
    juce::StringArray items;
    items.addTokens (s, ";", "");
    for (auto& it : items)
    {
        juce::StringArray p;
        p.addTokens (it, ":", "");
        if (p.size() != 4) continue;
        const int v = p[0].getIntValue(), i = p[1].getIntValue();
        if (v < 0 || v >= vh::numVoices || i < 0 || i >= (int) edits[(size_t) v].size()) continue;
        edits[(size_t) v][(size_t) i] = { p[2].getIntValue(), p[3].getIntValue() != 0 };
    }
}

juce::String HarmonyProcessor::detectedKeyText() const
{
    if (analysis == nullptr) return {};
    return vh::keyNames()[analysis->keyRoot] + (analysis->keyScale == vh::minor ? " Minor" : " Major");
}

void HarmonyProcessor::applyDetectedKey()
{
    if (analysis == nullptr) return;
    setParam ("key", (float) analysis->keyRoot);
    setParam ("scale", (float) analysis->keyScale);
}

//==============================================================================
namespace
{
    struct VP { bool on; int mode; int iv; float lvl, pan, fmt, dly, det, hum; };
    constexpr int S = vh::scaleMode, T = vh::semitoneMode;
    const VP off { false, S, 2, 80, 0, 0, 0, 0, 15 };
    struct Preset { const char* name; std::array<VP, vh::numVoices> v; float tune; };

    const Preset presets[]
    {
        { "3rd Up",               { VP { true, S,  2, 85, -25, 0, 0, 0, 15 }, off, off, off }, 0 },
        { "3rd Down",             { VP { true, S, -2, 85,  25, 0, 0, 0, 15 }, off, off, off }, 0 },
        { "5th Up",               { VP { true, S,  4, 80,  25, 0, 0, 0, 15 }, off, off, off }, 0 },
        { "5th Down",             { VP { true, S, -4, 80, -25, 0, 0, 0, 15 }, off, off, off }, 0 },
        { "4th Up",               { VP { true, S,  3, 80,  20, 0, 0, 0, 15 }, off, off, off }, 0 },
        { "6th Down",             { VP { true, S, -5, 80, -20, 0, 0, 0, 15 }, off, off, off }, 0 },
        { "Octave Up",            { VP { true, T, 12, 65,   0, 0, 0, 0, 10 }, off, off, off }, 0 },
        { "Octave Down",          { VP { true, T, -12, 75,  0, 0, 0, 0, 10 }, off, off, off }, 0 },
        { "Triad (3rd + 5th)",    { VP { true, S,  2, 80, -40, 0, 0, 0, 20 }, VP { true, S, 4, 70, 40, 0, 8, 0, 20 }, off, off }, 0 },
        { "3rd Up + 3rd Down",    { VP { true, S,  2, 80, -45, 0, 0, 0, 20 }, VP { true, S, -2, 80, 45, 0, 6, 0, 20 }, off, off }, 0 },
        { "Pop Stack",            { VP { true, S,  2, 80, -50, 0, 6, 0, 20 }, VP { true, S, 4, 68, 50, 0, 12, 0, 20 }, VP { true, T, -12, 45, 0, 0, 0, 0, 10 }, off }, 0 },
        { "Four-Part Stack",      { VP { true, S,  2, 75, -45, 0, 4, 0, 25 }, VP { true, S, 4, 65, 45, 0, 10, 0, 25 }, VP { true, S, -3, 70, -15, 0, 7, 0, 25 }, VP { true, T, -12, 50, 15, 0, 0, 0, 15 } }, 0 },
        { "Gospel Choir",         { VP { true, S,  2, 75, -60, 0, 0, 0, 45 }, VP { true, S, 4, 70, 60, 0, 12, 0, 45 }, VP { true, S, 7, 50, -25, 0, 20, 0, 45 }, VP { true, S, -3, 65, 25, 0, 6, 0, 45 } }, 0 },
        { "Barbershop (Below)",   { VP { true, S, -2, 80, -40, 0, 0, 0, 20 }, VP { true, S, -4, 75, 40, 0, 5, 0, 20 }, VP { true, S, -7, 60, 0, 0, 0, 0, 15 }, off }, 0 },
        { "Open 4th + 5th",       { VP { true, S,  3, 75, -40, 0, 0, 0, 20 }, VP { true, S, -4, 75, 40, 0, 6, 0, 20 }, off, off }, 0 },
        { "Power 5ths",           { VP { true, T,  7, 75, -35, 0, 0, 0, 15 }, VP { true, T, -12, 70, 35, 0, 0, 0, 10 }, VP { true, T, 12, 45, 0, 0, 10, 0, 15 }, off }, 0 },
        { "Double Track",         { VP { true, T,  0, 75, -85, 0, 18, -9, 55 }, VP { true, T, 0, 75, 85, 0, 31, 8, 55 }, off, off }, 0 },
        { "Wide Double + 3rd",    { VP { true, T,  0, 70, -85, 0, 20, -8, 55 }, VP { true, T, 0, 70, 85, 0, 33, 8, 55 }, VP { true, S, 2, 70, 0, 0, 0, 0, 20 }, off }, 0 },
        { "Dark Choir",           { VP { true, S, -2, 75, -50, -1.5f, 0, 0, 30 }, VP { true, S, -4, 70, 50, -2, 8, 0, 30 }, VP { true, T, -12, 70, 0, -3, 0, 0, 20 }, off }, 0 },
        { "Angel Choir",          { VP { true, T, 12, 55, -50, 1, 25, 0, 40 }, VP { true, S, 9, 45, 50, 1.5f, 35, 0, 40 }, VP { true, S, 4, 50, 0, 0, 15, 0, 40 }, off }, 0 },
        { "Trap Octaves",         { VP { true, T, -12, 75, -30, 0, 0, 0, 5 }, VP { true, T, 12, 55, 30, 0, 0, 0, 5 }, off, off }, 0.6f },
        { "Demon Stack",          { VP { true, T, -12, 90, 0, -5, 0, 0, 10 }, VP { true, T, -7, 50, -30, -4, 0, 0, 10 }, VP { true, T, -24, 40, 30, -6, 0, 0, 10 }, off }, 0.5f },
        { "Chipmunk",             { VP { true, T, 12, 70, 0, 6, 0, 0, 10 }, off, off, off }, 0 },
        { "Robot Harmony",        { VP { true, S, 2, 80, -40, 0, 0, 0, 0 }, VP { true, S, -3, 80, 40, 0, 0, 0, 0 }, off, off }, 1.0f },
    };
}

int HarmonyProcessor::numPresets() { return (int) (sizeof (presets) / sizeof (presets[0])); }
juce::String HarmonyProcessor::presetName (int i) { return presets[juce::jlimit (0, numPresets() - 1, i)].name; }

void HarmonyProcessor::applyPreset (int i)
{
    const auto& p = presets[juce::jlimit (0, numPresets() - 1, i)];
    for (int v = 0; v < vh::numVoices; ++v)
    {
        const auto& x = p.v[(size_t) v];
        setParam (vid ("on", v), x.on ? 1.0f : 0.0f);
        if (! x.on) continue;
        setParam (vid ("mode", v), (float) x.mode);
        setParam (vid ("int", v), (float) x.iv);
        setParam (vid ("lvl", v), x.lvl);
        setParam (vid ("pan", v), x.pan);
        setParam (vid ("fmt", v), x.fmt);
        setParam (vid ("dly", v), x.dly);
        setParam (vid ("det", v), x.det);
        setParam (vid ("hum", v), x.hum);
    }
    setParam ("tune", p.tune * 100.0f);
    apvts.state.setProperty ("preset", p.name, nullptr);
    apvts.state.setProperty ("presetIndex", juce::jlimit (0, numPresets() - 1, i), nullptr);
    for (int v = 0; v < vh::numVoices; ++v) for (auto& e : edits[(size_t) v]) e = {};
    requestRender();
}

void HarmonyProcessor::randomize()
{
    juce::Random r;
    const int count = 1 + r.nextInt (3);
    struct Opt { int mode, iv; };
    const Opt pool[] { { S, 2 }, { S, -2 }, { S, 4 }, { S, -3 }, { S, -4 }, { S, 3 }, { S, -5 }, { S, 5 }, { S, 9 },
                       { T, 12 }, { T, -12 }, { T, 0 }, { S, 7 }, { S, -7 } };
    std::vector<int> used;
    for (int v = 0; v < vh::numVoices; ++v)
    {
        if (v >= count) { setParam (vid ("on", v), 0.0f); continue; }
        int k;
        do k = r.nextInt ((int) std::size (pool)); while (std::find (used.begin(), used.end(), k) != used.end());
        used.push_back (k);
        const float side = (v % 2 == 0 ? -1.0f : 1.0f);
        setParam (vid ("on", v), 1.0f);
        setParam (vid ("mode", v), (float) pool[k].mode);
        setParam (vid ("int", v), (float) pool[k].iv);
        setParam (vid ("lvl", v), 55.0f + (float) r.nextInt (30));
        setParam (vid ("pan", v), count == 1 ? side * 20.0f : side * (25.0f + (float) r.nextInt (50)));
        const bool octave = pool[k].mode == T && pool[k].iv != 0;
        setParam (vid ("fmt", v), octave && r.nextInt (3) == 0 ? (pool[k].iv > 0 ? 1.5f : -2.0f) : 0.0f);
        setParam (vid ("dly", v), pool[k].iv == 0 ? 15.0f + (float) r.nextInt (20) : (float) r.nextInt (12));
        setParam (vid ("det", v), pool[k].iv == 0 ? side * (6.0f + (float) r.nextInt (6)) : 0.0f);
        setParam (vid ("hum", v), 10.0f + (float) r.nextInt (40));
    }
    apvts.state.setProperty ("preset", "Random", nullptr);
    for (int v = 0; v < vh::numVoices; ++v) for (auto& e : edits[(size_t) v]) e = {};
    requestRender();
}

//==============================================================================
void HarmonyProcessor::toggleRecord()
{
    const auto st = clipState.load();
    if (st == ClipState::recording || st == ClipState::waitingForBar)
    {
        recStopRequested.store (true);
        return;
    }
    previewing.store (false);
    recPos.store (0);
    recSeconds.store (0.0f);
    recStopRequested.store (false);
    clipMessage = {};
    clipState.store (ClipState::waitingForBar);
}

bool HarmonyProcessor::isSupportedFile (const juce::String& path)
{
    const auto ext = juce::File (path).getFileExtension().toLowerCase();
    for (auto* e : { ".wav", ".wave", ".aif", ".aiff", ".flac", ".mp3", ".ogg", ".m4a", ".wma", ".aac" })
        if (ext == e) return true;
    return false;
}

void HarmonyProcessor::startAnalysis (std::unique_ptr<juce::AudioBuffer<float>> buffer, const juce::File& file, bool keepEdits)
{
    previewing.store (false);
    renderer.reset();
    analyser.reset();
    clipMessage = {};
    clipState.store (ClipState::analysing);
    analyser = std::make_unique<AnalyseThread> (*this, std::move (buffer), file, sampleRate, keepEdits);
    analyser->startThread();
}

void HarmonyProcessor::loadFile (const juce::File& f, bool keepEdits)
{
    clipName = f.getFileNameWithoutExtension();
    apvts.state.setProperty ("clipFile", f.getFullPathName(), nullptr);
    startAnalysis (nullptr, f, keepEdits);
}

void HarmonyProcessor::clearClip()
{
    previewing.store (false);
    analyser.reset();
    renderer.reset();
    clip.reset();
    analysis.reset();
    ++analysisVersion;
    for (auto& e : edits) e.clear();
    apvts.state.setProperty ("clipFile", "", nullptr);
    {
        const juce::SpinLock::ScopedLockType sl (renderLock);
        rendered.reset();
    }
    ++renderVersion;
    clipState.store (ClipState::empty);
}

void HarmonyProcessor::startRender()
{
    if (clip == nullptr || analysis == nullptr) return;
    renderer.reset();
    renderer = std::make_unique<RenderThread> (*this, clip, analysis, currentSettings(), edits);
    if (clipState.load() != ClipState::ready) clipState.store (ClipState::rendering);
    renderer->startThread();
}

void HarmonyProcessor::requestRender()
{
    ++changeCount;
    renderDirty.store (true);
    dirtyTicks = 0;
}

void HarmonyProcessor::parameterChanged (const juce::String&, float)
{
    ++changeCount;
    renderDirty.store (true);
    dirtyTicks = 0;
}

void HarmonyProcessor::timerCallback()
{
    if (renderDirty.load() && clip != nullptr && analysis != nullptr)
    {
        const auto st = clipState.load();
        if (st != ClipState::recording && st != ClipState::waitingForBar && st != ClipState::analysing && ++dirtyTicks >= 2)
        {
            renderDirty.store (false);
            dirtyTicks = 0;
            startRender();
        }
    }
}

void HarmonyProcessor::handleAsyncUpdate()
{
    // recording finished on the audio thread
    if (clipState.load() == ClipState::analysing && recStopRequested.exchange (false))
    {
        const int n = recPos.load();
        if (n < (int) (sampleRate * 0.3))
        {
            clipMessage = "Recording was empty.";
            clipState.store (clip != nullptr ? ClipState::ready : ClipState::empty);
        }
        else
        {
            auto b = std::make_unique<juce::AudioBuffer<float>> (2, n);
            for (int c = 0; c < 2; ++c) b->copyFrom (c, 0, recBuffer, c, 0, n);
            clipName = "Vocal " + juce::Time::getCurrentTime().formatted ("%H-%M-%S");
            auto f = outputFolder().getChildFile (clipName + " (dry).wav");
            if (writeWav (f, *b, sampleRate)) apvts.state.setProperty ("clipFile", f.getFullPathName(), nullptr);
            startAnalysis (std::move (b), {}, false);
        }
    }

    PendingAnalysis pa;
    bool renderDone = false;
    std::shared_ptr<const vh::Output> newRender;
    {
        const juce::ScopedLock sl (pendingLock);
        pa = pendingAnalysis;
        pendingAnalysis = {};
        renderDone = pendingRenderDone;
        pendingRenderDone = false;
        newRender = std::move (pendingRender);
    }

    if (pa.done)
    {
        analyser.reset();
        if (pa.analysis == nullptr)
        {
            clipMessage = pa.error;
            clipState.store (ClipState::failed);
        }
        else
        {
            clip = pa.clip;
            analysis = pa.analysis;
            ++analysisVersion;
            for (auto& e : edits) e.assign (analysis->notes.size(), {});
            if (pa.keepEdits) editsFromString (restoredEdits);
            else applyDetectedKey();     // new vocal: follow its key
            restoredEdits = {};
            renderDirty.store (false);
            clipState.store (ClipState::rendering);
            startRender();
        }
    }
    if (renderDone && newRender != nullptr)
    {
        renderer.reset();
        {
            const juce::SpinLock::ScopedLockType sl (renderLock);
            rendered = newRender;
        }
        ++renderVersion;
        clipState.store (ClipState::ready);
    }
}

std::shared_ptr<const vh::Output> HarmonyProcessor::getRendered() const
{
    const juce::SpinLock::ScopedLockType sl (renderLock);
    return rendered;
}

juce::StringArray HarmonyProcessor::exportFiles (Export what)
{
    juce::StringArray files;
    auto r = getRendered();
    if (r == nullptr) return files;
    auto folder = outputFolder();
    auto save = [&] (const juce::String& name, const juce::AudioBuffer<float>& b)
    {
        auto f = folder.getChildFile (juce::File::createLegalFileName (clipName + " - " + name + ".wav"));
        if (b.getNumSamples() > 0 && writeWav (f, b, r->sampleRate)) files.add (f.getFullPathName());
    };
    if (what == Export::mix) save ("Harmony Mix", r->mix);
    else if (what == Export::harmonies) save ("Harmonies", r->harmonies);
    else
    {
        const auto s = currentSettings();
        for (int v = 0; v < vh::numVoices; ++v)
            if (r->active[(size_t) v])
                save (juce::String ("Voice ") + juce::String::charToString ((juce::juce_wchar) ('A' + v)) + " ("
                          + vh::intervalName (s.voices[(size_t) v].mode, s.voices[(size_t) v].interval) + ")",
                      r->stems[(size_t) v]);
    }
    return files;
}

void HarmonyProcessor::setPreview (bool p, int startSample)
{
    if (p) previewSample.store ((double) juce::jmax (0, startSample));
    previewing.store (p && getRendered() != nullptr);
}

//==============================================================================
void HarmonyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    double ppq = 0.0;
    bool playingHost = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) if (*b > 20.0 && *b < 999.0) hostBpm.store (*b);
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) { ppq = *q; playingHost = true; }
        }
    const double beatsPerSample = hostBpm.load() / 60.0 / sampleRate;

    // ---- capture (dry input) ------------------------------------------------------------
    auto st = clipState.load();
    if (st == ClipState::waitingForBar)
    {
        int startAt = -1;
        if (! playingHost) startAt = 0;
        else
        {
            const double nextBar = std::ceil (ppq / 4.0 - 1.0e-9) * 4.0;
            const int off = (int) std::round ((nextBar - ppq) / beatsPerSample);
            if (off < n) startAt = juce::jmax (0, off);
        }
        if (recStopRequested.load())
        {
            recStopRequested.store (false);
            clipState.store (clip != nullptr ? ClipState::ready : ClipState::empty);
        }
        else if (startAt >= 0 && recBuffer.getNumSamples() > 0)
        {
            clipState.store (ClipState::recording);
            const int num = juce::jmin (n - startAt, recBuffer.getNumSamples());
            for (int c = 0; c < 2; ++c) recBuffer.copyFrom (c, 0, buffer, juce::jmin (c, buffer.getNumChannels() - 1), startAt, num);
            recPos.store (num);
        }
    }
    else if (st == ClipState::recording)
    {
        int pos = recPos.load();
        const int num = juce::jmin (n, recBuffer.getNumSamples() - pos);
        if (num > 0)
            for (int c = 0; c < 2; ++c) recBuffer.copyFrom (c, pos, buffer, juce::jmin (c, buffer.getNumChannels() - 1), 0, num);
        pos += juce::jmax (0, num);
        recPos.store (pos);
        recSeconds.store ((float) (pos / sampleRate));
        if (recStopRequested.load() || pos >= recBuffer.getNumSamples())
        {
            recStopRequested.store (true);
            clipState.store (ClipState::analysing);
            triggerAsyncUpdate();
        }
    }

    // ---- preview -------------------------------------------------------------------------
    if (previewing.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (renderLock);
        if (lock.isLocked() && rendered != nullptr)
        {
            const auto& a = rendered->mix;
            const double step = rendered->sampleRate / sampleRate;
            double ps = previewSample.load();
            for (int i = 0; i < n; ++i)
            {
                const int idx = (int) ps;
                if (idx >= a.getNumSamples()) { previewing.store (false); break; }
                for (int c = 0; c < buffer.getNumChannels(); ++c) buffer.addSample (c, i, a.getSample (juce::jmin (c, 1), idx));
                ps += step;
            }
            previewSample.store (ps);
        }
    }
}

//==============================================================================
void HarmonyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("edits", editsToString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void HarmonyProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);
    restoredEdits = state.getProperty ("edits").toString();

    const auto path = state.getProperty ("clipFile").toString();
    if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
    {
        juce::MessageManager::callAsync ([safe = juce::WeakReference<HarmonyProcessor> (this), path]
        {
            if (auto* p = safe.get())
                if (juce::File (path).existsAsFile()) p->loadFile (juce::File (path), true);
        });
    }
}

juce::AudioProcessorEditor* HarmonyProcessor::createEditor() { return new HarmonyEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HarmonyProcessor(); }
