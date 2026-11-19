#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace hui
{
    namespace col
    {
        const juce::Colour bgTop    { 0xff2a2c31 };
        const juce::Colour bgMid    { 0xff1b1c20 };
        const juce::Colour bgBottom { 0xff121316 };
        const juce::Colour line     { 0xff2c2e34 };
        const juce::Colour border   { 0xff34373e };
        const juce::Colour borderHi { 0xff4a4e57 };
        const juce::Colour well     { 0xff141518 };
        const juce::Colour display  { 0xff0f1012 };
        const juce::Colour gold     { 0xffe2c27e };
        const juce::Colour ivory    { 0xffede6d6 };
        const juce::Colour muted    { 0xffa3a6ad };
        const juce::Colour muted2   { 0xff8a8d95 };
        const juce::Colour rec      { 0xffe5484d };
    }

    juce::Colour voiceColour (int v);
    juce::Font uiFont (float size, bool bold = false, float tracking = 0.0f);
    juce::String T (const char* utf8);

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
        void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
        void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        juce::Font getPopupMenuFont() override;
    };

    class VbButton : public juce::Button
    {
    public:
        enum class Style { pill, gold, segment, outline, ghost };
        enum class Icon  { none, play, stop, rec, open, clear, dice, left, right };
        VbButton (const juce::String& text, Style, Icon = Icon::none);
        void paintButton (juce::Graphics&, bool over, bool down) override;
        void setIcon (Icon i) { icon = i; repaint(); }
        juce::Colour accent = col::gold;
    private:
        Style style;
        Icon icon;
    };

    //==========================================================================
    class NoteEditor : public juce::Component
    {
    public:
        explicit NoteEditor (HarmonyProcessor&);
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        void refresh();
        void setVoice (int v) { voice = v; hover = -1; repaint(); }
        void fitView();

        bool fileDragHover = false;
        std::function<void()> onBrowse;
        std::function<void (int)> onSelectVoice;

    private:
        juce::Rectangle<float> plotArea() const;
        juce::Rectangle<float> rulerArea() const;
        float xOf (double sample) const;
        double sampleOf (float x) const;
        float yOf (float midi) const;
        float midiOf (float y) const;
        juce::Rectangle<float> noteRect (int note, int target) const;
        int hitNote (int v, juce::Point<float>) const;
        void showNoteMenu (int note);

        HarmonyProcessor& proc;
        std::shared_ptr<const vh::Analysis> an;
        int seenAnalysis = -1, seenChange = -1, seenRender = -1;
        HarmonyProcessor::ClipState seenState = HarmonyProcessor::ClipState::empty;
        int voice = 0, hover = -1;
        double t0 = 0.0, t1 = 1.0;
        float pLo = 48.0f, pHi = 72.0f;
        int dragNote = -1, dragStartTarget = 0;
        float dragStartY = 0.0f;
        bool clickInRuler = false;
    };

    //==========================================================================
    class VoiceStrip : public juce::Component
    {
    public:
        VoiceStrip (HarmonyProcessor&, int voice);
        ~VoiceStrip() override;
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent&) override;
        void refresh();
        bool selected = false;
        std::function<void (int)> onSelect;

    private:
        void stepInterval (int dir);
        void setMode (int mode);
        void showIntervalMenu();
        juce::Rectangle<int> intervalBox() const;

        HarmonyProcessor& proc;
        int v;
        juce::Colour colour;
        VbButton onBtn { "ON", VbButton::Style::pill };
        VbButton scaleBtn { "SCALE", VbButton::Style::segment };
        VbButton semiBtn { "SEMI", VbButton::Style::segment };
        VbButton prevBtn { "", VbButton::Style::ghost, VbButton::Icon::left };
        VbButton nextBtn { "", VbButton::Style::ghost, VbButton::Icon::right };
        juce::OwnedArray<juce::Slider> knobs;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> knobAtts;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAtt;
        std::unique_ptr<juce::ParameterAttachment> modeAtt;
        juce::String intervalText;
    };

    //==========================================================================
    class DragTile : public juce::Component
    {
    public:
        DragTile (HarmonyProcessor&, HarmonyProcessor::Export, juce::String title, juce::String sub);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void refresh();
    private:
        HarmonyProcessor& proc;
        HarmonyProcessor::Export kind;
        juce::String title, sub;
        std::shared_ptr<const vh::Output> r;
        int seen = -1;
        bool dragStarted = false;
        juce::String flash;
        juce::uint32 flashUntil = 0;
    };
}

//==============================================================================
class HarmonyEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    explicit HarmonyEditor (HarmonyProcessor&);
    ~HarmonyEditor() override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    class Panel;
    void timerCallback() override;
    void applyScale (int percent);
    void browse();

    HarmonyProcessor& proc;
    hui::LookAndFeel lnf;
    std::unique_ptr<Panel> panel;
    std::unique_ptr<juce::FileChooser> chooser;
    int scalePercent = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonyEditor)
};
