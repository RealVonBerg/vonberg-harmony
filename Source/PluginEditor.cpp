#include "PluginEditor.h"

using namespace hui;

static constexpr int baseW = 1100;
static constexpr int baseH = 740;

//==============================================================================
juce::Colour hui::voiceColour (int v)
{
    static const juce::Colour c[] { juce::Colour (0xffe2c27e), juce::Colour (0xff5fd3c4), juce::Colour (0xffef8a7a), juce::Colour (0xffa98bf0) };
    return c[juce::jlimit (0, 3, v)];
}

juce::Font hui::uiFont (float size, bool bold, float tracking)
{
    juce::FontOptions opts (size);
   #if JUCE_WINDOWS
    opts = opts.withName ("Segoe UI");
   #endif
    opts = opts.withStyle (bold ? "Bold" : "Regular").withKerningFactor (tracking);
    return juce::Font (opts);
}

juce::String hui::T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

static juce::Font logoFont (float size)
{
   #if JUCE_WINDOWS
    return juce::Font (juce::FontOptions ("Segoe Script", size, juce::Font::plain));
   #elif JUCE_MAC
    return juce::Font (juce::FontOptions ("Snell Roundhand", size, juce::Font::bold));
   #else
    return juce::Font (juce::FontOptions (size).withStyle ("Italic"));
   #endif
}

static juce::String voiceLetter (int v) { return juce::String::charToString ((juce::juce_wchar) ('A' + v)); }

static void drawIcon (juce::Graphics& g, VbButton::Icon icon, juce::Rectangle<float> r)
{
    juce::Path p;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    bool fill = false;
    switch (icon)
    {
        case VbButton::Icon::play:  p.addTriangle (x + w * 0.2f, y + h * 0.12f, x + w * 0.2f, y + h * 0.88f, x + w * 0.86f, y + h * 0.5f); fill = true; break;
        case VbButton::Icon::stop:  p.addRoundedRectangle (r.reduced (w * 0.16f), 1.5f); fill = true; break;
        case VbButton::Icon::rec:   p.addEllipse (r.reduced (w * 0.16f)); fill = true; break;
        case VbButton::Icon::open:
            p.startNewSubPath (x + w * 0.08f, y + h * 0.8f); p.lineTo (x + w * 0.08f, y + h * 0.2f); p.lineTo (x + w * 0.38f, y + h * 0.2f);
            p.lineTo (x + w * 0.48f, y + h * 0.32f); p.lineTo (x + w * 0.92f, y + h * 0.32f); p.lineTo (x + w * 0.92f, y + h * 0.8f); p.closeSubPath();
            break;
        case VbButton::Icon::clear:
            p.startNewSubPath (x + w * 0.22f, y + h * 0.22f); p.lineTo (x + w * 0.78f, y + h * 0.78f);
            p.startNewSubPath (x + w * 0.78f, y + h * 0.22f); p.lineTo (x + w * 0.22f, y + h * 0.78f);
            break;
        case VbButton::Icon::dice:  p.addRoundedRectangle (r.reduced (w * 0.06f), w * 0.2f); break;
        case VbButton::Icon::left:  p.startNewSubPath (x + w * 0.62f, y + h * 0.2f); p.lineTo (x + w * 0.33f, y + h * 0.5f); p.lineTo (x + w * 0.62f, y + h * 0.8f); break;
        case VbButton::Icon::right: p.startNewSubPath (x + w * 0.38f, y + h * 0.2f); p.lineTo (x + w * 0.67f, y + h * 0.5f); p.lineTo (x + w * 0.38f, y + h * 0.8f); break;
        case VbButton::Icon::none:  break;
    }
    if (fill) g.fillPath (p);
    else g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (icon == VbButton::Icon::dice)
    {
        const auto c = r.getCentre();
        const float d = w * 0.22f, s = w * 0.16f;
        for (auto pt : { c, c.translated (-d, -d), c.translated (d, d) }) g.fillEllipse (pt.x - s * 0.5f, pt.y - s * 0.5f, s, s);
    }
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, col::ivory);
    setColour (juce::ComboBox::arrowColourId, col::gold);
    setColour (juce::PopupMenu::backgroundColourId, col::bgMid);
    setColour (juce::PopupMenu::textColourId, col::ivory);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::gold.withAlpha (0.2f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::gold);
    setColour (juce::PopupMenu::headerTextColourId, col::gold);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    const juce::Rectangle<float> track ((float) x, cy - 2.0f, (float) w, 4.0f);
    g.setColour (juce::Colour (0xff33363d));
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (col::gold.withAlpha (0.8f));
    g.fillRoundedRectangle (track.withRight (pos), 2.0f);
    const juce::Rectangle<float> thumb (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
    g.setColour (col::gold.withAlpha (0.3f));
    g.fillEllipse (thumb.expanded (3.0f));
    g.setColour (col::gold);
    g.fillEllipse (thumb);
    g.setColour (col::bgMid);
    g.drawEllipse (thumb, 2.0f);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const juce::Colour accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const bool bipolar = (bool) s.getProperties()["bipolar"];

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, a0, a1, true);
    g.setColour (juce::Colour (0xff33363d));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float ang = a0 + pos * (a1 - a0);
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (ang - from) > 0.01f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float kr = r - 7.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d44), c.x, c.y - kr, juce::Colour (0xff1d1e22), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colour (0xff4a4e57));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);
    juce::Path ptr;
    ptr.addRoundedRectangle (-1.2f, -kr + 2.0f, 2.4f, kr * 0.55f, 1.2f);
    g.setColour (col::ivory);
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (col::well);
    g.fillRoundedRectangle (r, h * 0.5f);
    g.setColour (box.isMouseOver (true) ? col::borderHi : col::border);
    g.drawRoundedRectangle (r.reduced (0.5f), h * 0.5f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (col::gold);
    g.strokePath (arrow, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (12.0f, true); }
juce::Font LookAndFeel::getPopupMenuFont() { return uiFont (13.0f); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 0, box.getWidth() - 34, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
VbButton::VbButton (const juce::String& text, Style s, Icon i) : juce::Button (text), style (s), icon (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void VbButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    const float radius = r.getHeight() * 0.5f;
    juce::Colour fg = col::muted;
    switch (style)
    {
        case Style::pill:
            if (on) { g.setColour (accent.withAlpha (0.16f)); g.fillRoundedRectangle (r.reduced (0.5f), radius); fg = accent; }
            else if (icon == Icon::rec) fg = col::rec.withAlpha (0.9f);
            g.setColour (on ? accent : col::border);
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            break;
        case Style::gold:
            g.setGradientFill (juce::ColourGradient (col::gold.brighter (over ? 0.15f : 0.05f), 0, r.getY(), col::gold.darker (0.12f), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, radius);
            fg = col::bgMid;
            break;
        case Style::segment:
            if (on) { g.setColour (juce::Colour (0xff35373e)); g.fillRoundedRectangle (r, radius); fg = accent; }
            else fg = col::muted2;
            break;
        case Style::outline:
            g.setColour (col::border);
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            break;
        case Style::ghost: break;
    }
    if (over && ! on && style != Style::gold) fg = fg.brighter (0.35f);
    if (down) fg = fg.darker (0.2f);
    if (! isEnabled()) fg = fg.withAlpha (0.35f);
    g.setColour (fg);

    const auto label = getButtonText();
    if (icon != Icon::none && label.isEmpty())
    {
        const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.44f;
        drawIcon (g, icon, r.withSizeKeepingCentre (s, s));
        return;
    }
    auto font = uiFont (10.5f, true, 0.12f);
    g.setFont (font);
    const float textW = juce::GlyphArrangement::getStringWidth (font, label);
    const float iconW = icon != Icon::none ? 12.0f : 0.0f, gap = icon != Icon::none ? 7.0f : 0.0f;
    float x0 = r.getCentreX() - (textW + iconW + gap) * 0.5f;
    if (icon != Icon::none) { drawIcon (g, icon, juce::Rectangle<float> (x0, r.getCentreY() - 6.0f, 12.0f, 12.0f)); x0 += iconW + gap; }
    g.drawText (label, juce::Rectangle<float> (x0, r.getY(), textW + 2.0f, r.getHeight()), juce::Justification::centredLeft);
}

//==============================================================================
// NOTE EDITOR
//==============================================================================
static constexpr float keyW = 46.0f, rulerH = 20.0f;

NoteEditor::NoteEditor (HarmonyProcessor& p) : proc (p) {}

juce::Rectangle<float> NoteEditor::plotArea() const
{
    return getLocalBounds().toFloat().reduced (2.0f).withTrimmedLeft (keyW).withTrimmedTop (rulerH);
}

juce::Rectangle<float> NoteEditor::rulerArea() const
{
    auto b = getLocalBounds().toFloat().reduced (2.0f).withTrimmedLeft (keyW);
    return b.withHeight (rulerH);
}

float NoteEditor::xOf (double s) const
{
    const auto p = plotArea();
    return p.getX() + (float) ((s - t0) / juce::jmax (1.0, t1 - t0)) * p.getWidth();
}

double NoteEditor::sampleOf (float x) const
{
    const auto p = plotArea();
    return t0 + (double) ((x - p.getX()) / p.getWidth()) * (t1 - t0);
}

float NoteEditor::yOf (float m) const
{
    const auto p = plotArea();
    return p.getBottom() - (m - pLo) / (pHi - pLo) * p.getHeight();
}

float NoteEditor::midiOf (float y) const
{
    const auto p = plotArea();
    return pLo + (p.getBottom() - y) / p.getHeight() * (pHi - pLo);
}

juce::Rectangle<float> NoteEditor::noteRect (int note, int target) const
{
    const auto& nt = an->notes[(size_t) note];
    const float x0 = xOf (nt.start), x1 = xOf (nt.end);
    const float rowH = plotArea().getHeight() / (pHi - pLo);
    const float h = juce::jmax (4.0f, rowH * 0.82f);
    return { x0 + 0.5f, yOf ((float) target) - h * 0.5f, juce::jmax (2.0f, x1 - x0 - 1.0f), h };
}

void NoteEditor::fitView()
{
    if (an == nullptr || an->notes.empty()) return;
    t0 = 0.0;
    t1 = juce::jmax (1.0, (double) an->length);
    float lo = 200.0f, hi = -200.0f;
    for (size_t i = 0; i < an->notes.size(); ++i)
    {
        lo = juce::jmin (lo, (float) an->notes[i].midi);
        hi = juce::jmax (hi, (float) an->notes[i].midi);
        for (int v = 0; v < vh::numVoices; ++v)
            if (proc.getParam (HarmonyProcessor::vid ("on", v)) > 0.5f)
            {
                const int t = proc.getBaseTarget (v, (int) i);
                lo = juce::jmin (lo, (float) t);
                hi = juce::jmax (hi, (float) t);
            }
    }
    lo -= 3.0f; hi += 3.0f;
    if (hi - lo < 20.0f) { const float c = (lo + hi) * 0.5f; lo = c - 10.0f; hi = c + 10.0f; }
    pLo = lo - 0.5f; pHi = hi + 0.5f;
    repaint();
}

void NoteEditor::refresh()
{
    bool dirty = false;
    if (proc.getAnalysisVersion() != seenAnalysis)
    {
        seenAnalysis = proc.getAnalysisVersion();
        an = proc.getAnalysis();
        hover = dragNote = -1;
        fitView();
        dirty = true;
    }
    if (proc.getChangeCount() != seenChange)
    {
        seenChange = proc.getChangeCount();
        dirty = true;
        // keep every active harmony note in view
        if (an != nullptr && dragNote < 0)
            for (size_t i = 0; i < an->notes.size(); ++i)
                for (int v = 0; v < vh::numVoices; ++v)
                    if (proc.getParam (HarmonyProcessor::vid ("on", v)) > 0.5f)
                    {
                        const float t = (float) (proc.getBaseTarget (v, (int) i) + proc.getEdits()[(size_t) v][(size_t) i].offset);
                        if (t - 1.0f < pLo) pLo = t - 2.5f;
                        if (t + 1.0f > pHi) pHi = t + 2.5f;
                    }
    }
    if (proc.getRenderVersion() != seenRender) { seenRender = proc.getRenderVersion(); dirty = true; }
    const auto st = proc.getClipState();
    if (st != seenState) { seenState = st; dirty = true; }
    if (proc.isPreviewing() || st == HarmonyProcessor::ClipState::recording || st == HarmonyProcessor::ClipState::waitingForBar
        || st == HarmonyProcessor::ClipState::analysing)
        dirty = true;
    if (dirty) repaint();
}

int NoteEditor::hitNote (int v, juce::Point<float> pt) const
{
    if (an == nullptr || ! plotArea().contains (pt)) return -1;
    if (proc.getParam (HarmonyProcessor::vid ("on", v)) < 0.5f) return -1;
    for (int i = 0; i < (int) an->notes.size(); ++i)
    {
        int t = proc.getTarget (v, i);
        if (t < 0) t = proc.getBaseTarget (v, i) + proc.getEdits()[(size_t) v][(size_t) i].offset;
        if (noteRect (i, t).expanded (0.0f, 2.0f).contains (pt)) return i;
    }
    return -1;
}

void NoteEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto st = proc.getClipState();
    g.setColour (col::display);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    auto centreText = [&] (const juce::String& big, const juce::String& small, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (uiFont (15.0f, true, 0.2f));
        g.drawText (big, b.withTrimmedBottom (26.0f), juce::Justification::centred);
        g.setColour (col::muted2);
        g.setFont (uiFont (12.0f));
        g.drawText (small, b.withTrimmedTop (34.0f), juce::Justification::centred);
    };

    if (st == HarmonyProcessor::ClipState::waitingForBar)
        centreText ("REC ARMED - STARTS ON THE NEXT BAR", T ("FL'de çal; vokal bir sonraki bardan itibaren kaydedilir"), col::rec);
    else if (st == HarmonyProcessor::ClipState::recording)
    {
        const bool blink = (juce::Time::getMillisecondCounter() / 400) % 2 == 0;
        centreText ("RECORDING  " + juce::String (proc.getRecordSeconds(), 1) + " s", T ("Durdurmak için REC'e tekrar bas (en fazla 60 sn)"),
                    col::rec.withAlpha (blink ? 1.0f : 0.5f));
    }
    else if (st == HarmonyProcessor::ClipState::analysing)
        centreText ("ANALYSING THE VOCAL...", T ("Notalar ve ton bulunuyor"), col::ivory);
    else if (an == nullptr)
    {
        juce::Path border, dashed;
        border.addRoundedRectangle (b.reduced (8.0f), 8.0f);
        const float dashes[] { 6.0f, 5.0f };
        juce::PathStrokeType (1.1f).createDashedStroke (dashed, border, dashes, 2);
        g.setColour (col::borderHi);
        g.fillPath (dashed);
        if (st == HarmonyProcessor::ClipState::failed) centreText (proc.getClipMessage(), T ("Başka bir dosya dene (tek ses, kuru vokal en iyi sonucu verir)"), col::rec);
        else centreText ("DROP A VOCAL HERE", T ("FL Browser (Current project → Audio clips) / Gezgin'den sürükle  ·  tıkla = dosya seç  ·  ya da REC ile kaydet"), col::ivory);
    }

    if (an != nullptr && st != HarmonyProcessor::ClipState::recording && st != HarmonyProcessor::ClipState::waitingForBar
        && st != HarmonyProcessor::ClipState::analysing)
    {
        const auto p = plotArea();
        const int key = (int) proc.getParam ("key"), scale = (int) proc.getParam ("scale");
        const float rowH = p.getHeight() / (pHi - pLo);

        // rows + keyboard
        g.saveState();
        g.reduceClipRegion (b.reduced (1.0f).withTrimmedTop (rulerH + 2.0f).toNearestInt());
        for (int m = (int) std::floor (pLo); m <= (int) std::ceil (pHi); ++m)
        {
            const float y0 = yOf ((float) m + 0.5f), y1 = yOf ((float) m - 0.5f);
            const juce::Rectangle<float> row (p.getX(), y0, p.getWidth(), y1 - y0);
            const bool in = vh::inScale (m, key, scale);
            if (in) { g.setColour (juce::Colours::white.withAlpha (0.035f)); g.fillRect (row); }
            if (((m - key) % 12 + 12) % 12 == 0) { g.setColour (col::gold.withAlpha (0.06f)); g.fillRect (row); }
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRect (p.getX(), y1 - 0.5f, p.getWidth(), 0.5f);

            const bool black = juce::String ("010100101010").substring ((m % 12 + 12) % 12, (m % 12 + 12) % 12 + 1) == "1";
            const juce::Rectangle<float> k (b.getX() + 4.0f, y0 + 0.5f, keyW - 6.0f, juce::jmax (1.0f, y1 - y0 - 1.0f));
            g.setColour (black ? juce::Colour (0xff202226) : juce::Colour (0xffc9c4b8).withAlpha (in ? 0.9f : 0.55f));
            g.fillRect (k);
            if (rowH >= 9.0f && (((m - key) % 12 + 12) % 12 == 0 || m % 12 == 0))
            {
                g.setColour (black ? col::ivory : col::bgBottom);
                g.setFont (uiFont (juce::jmin (10.0f, rowH - 1.0f), true));
                g.drawText (vh::noteName (m), k.withTrimmedLeft (3.0f), juce::Justification::centredLeft);
            }
        }
        g.restoreState();

        g.saveState();
        g.reduceClipRegion (p.toNearestInt());

        // waveform (faint)
        if (! an->peaks.empty())
        {
            const int cols = (int) an->peaks.size();
            const float mid = p.getBottom() - p.getHeight() * 0.16f, amp = p.getHeight() * 0.14f;
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            for (float x = p.getX(); x < p.getRight(); x += 2.0f)
            {
                const double s = sampleOf (x);
                if (s < 0 || s >= an->length) continue;
                const float v = an->peaks[(size_t) juce::jlimit (0, cols - 1, (int) (s / an->length * cols))];
                g.fillRect (x, mid - v * amp, 1.5f, juce::jmax (1.0f, v * amp * 2.0f));
            }
        }

        // lead pitch curve
        {
            juce::Path curve;
            bool open = false;
            const int F = an->numFrames();
            const int f0 = juce::jmax (0, (int) (t0 / an->hop) - 1), f1 = juce::jmin (F - 1, (int) (t1 / an->hop) + 1);
            const int stride = juce::jmax (1, (int) ((f1 - f0) / (p.getWidth() * 1.5f)));
            for (int f = f0; f <= f1; f += stride)
            {
                const float m = an->pitch[(size_t) f];
                if (m <= 0.0f) { open = false; continue; }
                const float x = xOf ((double) f * an->hop), y = yOf (m);
                if (! open) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
                open = true;
            }
            g.setColour (col::ivory.withAlpha (0.35f));
            g.strokePath (curve, juce::PathStrokeType (1.2f));
        }

        // lead notes
        for (int i = 0; i < (int) an->notes.size(); ++i)
        {
            const auto r = noteRect (i, an->notes[(size_t) i].midi);
            if (r.getRight() < p.getX() || r.getX() > p.getRight()) continue;
            g.setColour (col::ivory.withAlpha (0.09f));
            g.fillRoundedRectangle (r, 2.5f);
            g.setColour (col::ivory.withAlpha (0.5f));
            g.drawRoundedRectangle (r, 2.5f, 1.0f);
        }

        // harmony notes (other voices first, the edited voice on top)
        std::vector<int> order;
        for (int v = 0; v < vh::numVoices; ++v) if (v != voice) order.push_back (v);
        order.push_back (voice);
        for (int v : order)
        {
            if (proc.getParam (HarmonyProcessor::vid ("on", v)) < 0.5f) continue;
            const bool sel = v == voice;
            const auto c = voiceColour (v);
            for (int i = 0; i < (int) an->notes.size(); ++i)
            {
                const int t = proc.getTarget (v, i);
                const bool muted = t < 0;
                if (muted && ! sel) continue;
                const int shown = muted ? proc.getBaseTarget (v, i) + proc.getEdits()[(size_t) v][(size_t) i].offset : t;
                auto r = noteRect (i, shown);
                if (r.getRight() < p.getX() || r.getX() > p.getRight()) continue;
                if (! sel) r = r.reduced (0.0f, r.getHeight() * 0.18f);
                if (muted)
                {
                    juce::Path outline, dashed;
                    outline.addRoundedRectangle (r, 2.5f);
                    const float d[] { 3.0f, 3.0f };
                    juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, d, 2);
                    g.setColour (c.withAlpha (0.6f));
                    g.fillPath (dashed);
                    continue;
                }
                const bool hot = sel && (i == hover || i == dragNote);
                g.setColour (c.withAlpha (sel ? (hot ? 1.0f : 0.85f) : 0.3f));
                g.fillRoundedRectangle (r, 2.5f);
                if (sel)
                {
                    g.setColour (hot ? col::ivory : c.brighter (0.4f));
                    g.drawRoundedRectangle (r, 2.5f, hot ? 1.5f : 0.8f);
                    if (proc.isEdited (v, i))
                    {
                        g.setColour (col::bgBottom);
                        g.fillEllipse (r.getX() + 3.0f, r.getCentreY() - 2.0f, 4.0f, 4.0f);
                    }
                    if (r.getWidth() > 34.0f && r.getHeight() >= 9.0f)
                    {
                        g.setColour (col::bgBottom.withAlpha (0.85f));
                        g.setFont (uiFont (juce::jmin (10.0f, r.getHeight() - 1.0f), true));
                        g.drawText (vh::noteName (t), r.withTrimmedLeft (proc.isEdited (v, i) ? 9.0f : 4.0f), juce::Justification::centredLeft);
                    }
                }
            }
        }

        // drag label
        if (dragNote >= 0)
        {
            const int t = proc.getTarget (voice, dragNote);
            if (t >= 0)
            {
                const auto r = noteRect (dragNote, t);
                const int off = proc.getEdits()[(size_t) voice][(size_t) dragNote].offset;
                const juce::String txt = vh::noteName (t) + (off != 0 ? "  (" + juce::String (off > 0 ? "+" : "") + juce::String (off) + ")" : "");
                const juce::Rectangle<float> chip (r.getX(), r.getY() - 20.0f, 92.0f, 17.0f);
                g.setColour (col::bgBottom.withAlpha (0.9f));
                g.fillRoundedRectangle (chip, 8.0f);
                g.setColour (voiceColour (voice));
                g.setFont (uiFont (10.5f, true));
                g.drawText (txt, chip, juce::Justification::centred);
            }
        }

        // preview playhead
        if (proc.isPreviewing())
        {
            g.setColour (col::ivory);
            g.fillRect (xOf (proc.getPreviewSample()), p.getY(), 1.5f, p.getHeight());
        }
        g.restoreState();

        // ruler
        const auto ru = rulerArea();
        g.setColour (juce::Colours::white.withAlpha (0.03f));
        g.fillRect (ru);
        const double secs = (t1 - t0) / an->sampleRate;
        double stepS = 0.5;
        for (double c : { 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0 }) { stepS = c; if (secs / c < 14.0) break; }
        g.setFont (uiFont (9.5f, true));
        for (double s = std::ceil (t0 / an->sampleRate / stepS) * stepS; s * an->sampleRate <= t1; s += stepS)
        {
            const float x = xOf (s * an->sampleRate);
            g.setColour (col::muted2.withAlpha (0.6f));
            g.fillRect (x, ru.getBottom() - 6.0f, 1.0f, 6.0f);
            g.drawText (juce::String (s, stepS < 1.0 ? 2 : 0) + "s", juce::Rectangle<float> (x + 3.0f, ru.getY(), 50.0f, ru.getHeight() - 4.0f),
                        juce::Justification::centredLeft);
        }

        if (st == HarmonyProcessor::ClipState::rendering)
        {
            const juce::Rectangle<float> chip (p.getRight() - 120.0f, p.getY() + 6.0f, 112.0f, 20.0f);
            g.setColour (col::bgBottom.withAlpha (0.85f));
            g.fillRoundedRectangle (chip, 10.0f);
            g.setColour (col::gold);
            g.setFont (uiFont (9.5f, true, 0.18f));
            g.drawText ("RENDERING...", chip, juce::Justification::centred);
        }
    }

    if (fileDragHover)
    {
        g.setColour (col::gold.withAlpha (0.12f));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (col::gold);
        g.drawRoundedRectangle (b.reduced (1.0f), 10.0f, 1.5f);
        g.setFont (uiFont (16.0f, true, 0.25f));
        g.drawText ("DROP THE VOCAL", b, juce::Justification::centred);
    }
}

void NoteEditor::mouseMove (const juce::MouseEvent& e)
{
    if (an == nullptr) { setMouseCursor (juce::MouseCursor::PointingHandCursor); return; }
    const int h = hitNote (voice, e.position);
    if (h != hover) { hover = h; repaint(); }
    if (rulerArea().contains (e.position)) setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else setMouseCursor (h >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void NoteEditor::mouseExit (const juce::MouseEvent&) { if (hover >= 0) { hover = -1; repaint(); } }

void NoteEditor::mouseDown (const juce::MouseEvent& e)
{
    clickInRuler = false;
    dragNote = -1;
    if (an == nullptr) return;
    if (rulerArea().contains (e.position))
    {
        clickInRuler = true;
        proc.setPreview (true, (int) juce::jlimit (0.0, (double) an->length, sampleOf (e.position.x)));
        return;
    }
    const int h = hitNote (voice, e.position);
    if (h >= 0)
    {
        if (e.mods.isPopupMenu()) { showNoteMenu (h); return; }
        dragNote = h;
        dragStartY = e.position.y;
        dragStartTarget = proc.getBaseTarget (voice, h) + proc.getEdits()[(size_t) voice][(size_t) h].offset;
        repaint();
        return;
    }
    for (int v = 0; v < vh::numVoices; ++v)
        if (v != voice && hitNote (v, e.position) >= 0)
        {
            if (onSelectVoice) onSelectVoice (v);
            return;
        }
}

void NoteEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (an == nullptr) return;
    if (clickInRuler)
    {
        proc.setPreview (true, (int) juce::jlimit (0.0, (double) an->length, sampleOf (e.position.x)));
        return;
    }
    if (dragNote < 0) return;
    const float rowH = plotArea().getHeight() / (pHi - pLo);
    const float want = (float) dragStartTarget + (dragStartY - e.position.y) / rowH;
    const int key = (int) proc.getParam ("key"), scale = (int) proc.getParam ("scale");
    const int snapped = e.mods.isAltDown() ? (int) std::round (want) : vh::snapToScale (want, key, scale);
    proc.setNoteOffset (voice, dragNote, snapped - proc.getBaseTarget (voice, dragNote));
    repaint();
}

void NoteEditor::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDrag = e.getDistanceFromDragStart() > 3;
    if (an == nullptr)
    {
        if (onBrowse && proc.getClipState() != HarmonyProcessor::ClipState::recording
            && proc.getClipState() != HarmonyProcessor::ClipState::analysing) onBrowse();
        return;
    }
    juce::ignoreUnused (wasDrag);
    dragNote = -1;
    clickInRuler = false;
    repaint();
}

void NoteEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitNote (voice, e.position);
    if (h >= 0) { proc.toggleMute (voice, h); repaint(); }
}

void NoteEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (an == nullptr) return;
    const double len = t1 - t0;
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        const double at = sampleOf (e.position.x);
        const double z = w.deltaY > 0 ? 0.8 : 1.25;
        const double newLen = juce::jlimit (an->sampleRate * 0.5, (double) an->length, len * z);
        t0 = at - (at - t0) * newLen / len;
        t1 = t0 + newLen;
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        const double d = (std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : -w.deltaY) * len * 0.5;
        t0 += d; t1 += d;
    }
    else
    {
        const float d = w.deltaY * 6.0f;
        pLo += d; pHi += d;
        pLo = juce::jlimit (12.0f, 110.0f - (pHi - pLo), pLo);
        pHi = juce::jmax (pHi, pLo + 12.0f);
    }
    if (t0 < 0.0) { t1 -= t0; t0 = 0.0; }
    if (t1 > an->length) { t0 = juce::jmax (0.0, t0 - (t1 - an->length)); t1 = an->length; }
    repaint();
}

void NoteEditor::showNoteMenu (int note)
{
    juce::PopupMenu m;
    const bool muted = proc.getTarget (voice, note) < 0;
    m.addSectionHeader ("Voice " + voiceLetter (voice) + "  -  note " + juce::String (note + 1));
    m.addItem (1, muted ? T ("Sesi aç (unmute)") : T ("Sustur (mute)"));
    m.addItem (2, T ("Bu notayı sıfırla"), proc.isEdited (voice, note));
    m.addSeparator();
    m.addItem (3, T ("+1 yarım ses"));
    m.addItem (4, T ("-1 yarım ses"));
    m.addItem (5, T ("+1 oktav"));
    m.addItem (6, T ("-1 oktav"));
    m.addSeparator();
    m.addItem (7, T ("Bu sesin tüm düzenlemelerini sıfırla"), proc.getEditCount (voice) > 0);
    juce::Component::SafePointer<NoteEditor> safe (this);
    const int v = voice;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, v, note] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc;
        const int off = p.getEdits()[(size_t) v][(size_t) note].offset;
        switch (r)
        {
            case 1: p.toggleMute (v, note); break;
            case 2: p.resetNote (v, note); break;
            case 3: p.setNoteOffset (v, note, off + 1); break;
            case 4: p.setNoteOffset (v, note, off - 1); break;
            case 5: p.setNoteOffset (v, note, off + 12); break;
            case 6: p.setNoteOffset (v, note, off - 12); break;
            case 7: p.resetEdits (v); break;
            default: break;
        }
        safe->repaint();
    });
}

//==============================================================================
// VOICE STRIP
//==============================================================================
namespace
{
    struct KnobDef { const char* id; const char* label; bool bipolar; };
    const KnobDef knobDefs[] { { "lvl", "LEVEL", false }, { "pan", "PAN", true }, { "fmt", "FORMANT", true },
                               { "dly", "DELAY", false }, { "det", "DETUNE", true }, { "hum", "HUMAN", false } };

    int scaleStepsToSemis (int steps)
    {
        static const int maj[] { 0, 2, 4, 5, 7, 9, 11 };
        const int oct = steps >= 0 ? steps / 7 : -((-steps + 6) / 7);
        return oct * 12 + maj[((steps % 7) + 7) % 7];
    }
    int semisToScaleSteps (int semis)
    {
        int best = 0, bestD = 99;
        for (int s = -14; s <= 14; ++s)
            if (std::abs (scaleStepsToSemis (s) - semis) < bestD) { bestD = std::abs (scaleStepsToSemis (s) - semis); best = s; }
        return best;
    }
}

VoiceStrip::VoiceStrip (HarmonyProcessor& p, int voice) : proc (p), v (voice), colour (voiceColour (voice))
{
    onBtn.setClickingTogglesState (true);
    onBtn.accent = colour;
    onAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, HarmonyProcessor::vid ("on", v), onBtn);
    addAndMakeVisible (onBtn);

    scaleBtn.accent = semiBtn.accent = colour;
    modeAtt = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (HarmonyProcessor::vid ("mode", v)), [this] (float m)
    {
        scaleBtn.setToggleState ((int) m == 0, juce::dontSendNotification);
        semiBtn.setToggleState ((int) m == 1, juce::dontSendNotification);
        refresh();
    });
    modeAtt->sendInitialUpdate();
    scaleBtn.onClick = [this] { setMode (vh::scaleMode); };
    semiBtn.onClick = [this] { setMode (vh::semitoneMode); };
    prevBtn.onClick = [this] { stepInterval (-1); };
    nextBtn.onClick = [this] { stepInterval (1); };
    for (auto* b : { &scaleBtn, &semiBtn, &prevBtn, &nextBtn }) addAndMakeVisible (b);

    for (auto& d : knobDefs)
    {
        auto* s = knobs.add (new juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox));
        s->setColour (juce::Slider::rotarySliderFillColourId, colour);
        s->getProperties().set ("bipolar", d.bipolar);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        addAndMakeVisible (s);
        knobAtts.add (new juce::AudioProcessorValueTreeState::SliderAttachment (p.apvts, HarmonyProcessor::vid (d.id, v), *s));
        auto* param = p.apvts.getParameter (HarmonyProcessor::vid (d.id, v));
        s->setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
        s->onValueChange = [this] { repaint(); };
        s->onDragStart = [this] { if (onSelect) onSelect (v); };
    }
    refresh();
}

VoiceStrip::~VoiceStrip() = default;

void VoiceStrip::refresh()
{
    const auto txt = vh::intervalName ((int) proc.getParam (HarmonyProcessor::vid ("mode", v)),
                                       (int) proc.getParam (HarmonyProcessor::vid ("int", v)));
    if (txt != intervalText) { intervalText = txt; repaint(); }
    const bool on = proc.getParam (HarmonyProcessor::vid ("on", v)) > 0.5f;
    for (auto* k : knobs) k->setAlpha (on ? 1.0f : 0.45f);
}

void VoiceStrip::setMode (int mode)
{
    const int cur = (int) proc.getParam (HarmonyProcessor::vid ("mode", v));
    if (cur == mode) return;
    const int iv = (int) proc.getParam (HarmonyProcessor::vid ("int", v));
    const int converted = mode == vh::semitoneMode ? scaleStepsToSemis (juce::jlimit (-14, 14, iv)) : semisToScaleSteps (iv);
    modeAtt->setValueAsCompleteGesture ((float) mode);
    proc.setParam (HarmonyProcessor::vid ("int", v), (float) converted);
    if (onSelect) onSelect (v);
}

void VoiceStrip::stepInterval (int dir)
{
    const int mode = (int) proc.getParam (HarmonyProcessor::vid ("mode", v));
    const int lim = mode == vh::scaleMode ? 14 : 24;
    const int iv = juce::jlimit (-lim, lim, (int) proc.getParam (HarmonyProcessor::vid ("int", v)) + dir);
    proc.setParam (HarmonyProcessor::vid ("int", v), (float) iv);
    proc.setParam (HarmonyProcessor::vid ("on", v), 1.0f);
    if (onSelect) onSelect (v);
}

void VoiceStrip::showIntervalMenu()
{
    const int mode = (int) proc.getParam (HarmonyProcessor::vid ("mode", v));
    const int cur = (int) proc.getParam (HarmonyProcessor::vid ("int", v));
    juce::PopupMenu m;
    const int lim = mode == vh::scaleMode ? 14 : 24;
    for (int i = lim; i >= -lim; --i)
    {
        if (mode == vh::semitoneMode && std::abs (i) > 12 && std::abs (i) != 19 && std::abs (i) != 24) continue;
        if (i == 0) m.addSeparator();
        m.addItem (i + 100, vh::intervalName (mode, i), true, i == cur);
        if (i == 0) m.addSeparator();
    }
    juce::Component::SafePointer<VoiceStrip> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMaximumNumColumns (2), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        safe->proc.setParam (HarmonyProcessor::vid ("int", safe->v), (float) (r - 100));
        safe->proc.setParam (HarmonyProcessor::vid ("on", safe->v), 1.0f);
        if (safe->onSelect) safe->onSelect (safe->v);
    });
}

juce::Rectangle<int> VoiceStrip::intervalBox() const { return { 40, 38, getWidth() - 80, 26 }; }

void VoiceStrip::mouseUp (const juce::MouseEvent& e)
{
    if (intervalBox().contains (e.getPosition())) { showIntervalMenu(); return; }
    if (onSelect) onSelect (v);
}

void VoiceStrip::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const bool on = proc.getParam (HarmonyProcessor::vid ("on", v)) > 0.5f;
    g.setColour (juce::Colours::white.withAlpha (selected ? 0.045f : 0.022f));
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (selected ? colour.withAlpha (0.8f) : col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, selected ? 1.5f : 1.0f);

    g.setColour (on ? colour : colour.withAlpha (0.35f));
    g.fillEllipse (14.0f, 12.0f, 10.0f, 10.0f);
    g.setColour (on ? col::ivory : col::muted2);
    g.setFont (uiFont (11.5f, true, 0.12f));
    g.drawText ("VOICE " + voiceLetter (v), juce::Rectangle<float> (30.0f, 6.0f, 70.0f, 22.0f), juce::Justification::centredLeft);

    // mode segment well
    auto seg = scaleBtn.getBounds().getUnion (semiBtn.getBounds()).toFloat().expanded (2.0f);
    g.setColour (col::bgBottom);
    g.fillRoundedRectangle (seg, seg.getHeight() * 0.5f);

    // interval box
    const auto ib = intervalBox().toFloat();
    g.setColour (col::well);
    g.fillRoundedRectangle (ib, 13.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (ib.reduced (0.5f), 13.0f, 1.0f);
    g.setColour (on ? colour : col::muted2);
    g.setFont (uiFont (12.0f, true));
    g.drawText (intervalText, ib, juce::Justification::centred);

    // knob labels / values
    for (int i = 0; i < knobs.size(); ++i)
    {
        const auto kb = knobs[i]->getBounds().toFloat();
        const juce::Rectangle<float> txt (kb.getRight() + 2.0f, kb.getY() + 2.0f, 42.0f, kb.getHeight() - 4.0f);
        g.setColour (col::muted2);
        g.setFont (uiFont (8.0f, true, 0.04f));
        g.drawText (knobDefs[i].label, txt.withHeight (txt.getHeight() * 0.5f), juce::Justification::bottomLeft);
        const float val = (float) knobs[i]->getValue();
        juce::String s;
        switch (i)
        {
            case 0: s = juce::String ((int) val) + "%"; break;
            case 1: s = std::abs (val) < 0.5f ? "C" : (val < 0 ? "L" : "R") + juce::String ((int) std::abs (val)); break;
            case 2: s = (val > 0 ? "+" : "") + juce::String (val, val == std::floor (val) ? 0 : 1); break;
            case 3: s = juce::String ((int) val) + " ms"; break;
            case 4: s = (val > 0 ? "+" : "") + juce::String ((int) val) + " ct"; break;
            default: s = juce::String ((int) val) + "%"; break;
        }
        g.setColour (on ? col::ivory : col::muted2);
        g.setFont (uiFont (10.5f, true));
        g.drawText (s, txt.withTrimmedTop (txt.getHeight() * 0.5f), juce::Justification::topLeft);
    }
}

void VoiceStrip::resized()
{
    onBtn.setBounds (getWidth() - 56, 7, 46, 22);
    scaleBtn.setBounds (getWidth() - 152, 8, 46, 20);
    semiBtn.setBounds (getWidth() - 106, 8, 46, 20);
    const auto ib = intervalBox();
    prevBtn.setBounds (ib.getX() - 30, ib.getY() + 1, 26, 24);
    nextBtn.setBounds (ib.getRight() + 4, ib.getY() + 1, 26, 24);
    const int cw = (getWidth() - 16) / 3;
    for (int i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (6 + (i % 3) * cw, 74 + (i / 3) * 52, 38, 38);
}

//==============================================================================
// DRAG TILE
//==============================================================================
DragTile::DragTile (HarmonyProcessor& p, HarmonyProcessor::Export k, juce::String t, juce::String s)
    : proc (p), kind (k), title (std::move (t)), sub (std::move (s)) {}

void DragTile::refresh()
{
    if (proc.getRenderVersion() != seen) { seen = proc.getRenderVersion(); r = proc.getRendered(); repaint(); }
    setMouseCursor (r != nullptr ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    if (flashUntil != 0 && juce::Time::getMillisecondCounter() > flashUntil) { flashUntil = 0; flash = {}; repaint(); }
}

void DragTile::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const bool ok = r != nullptr;
    g.setColour (juce::Colours::white.withAlpha (ok ? 0.04f : 0.02f));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (ok ? col::gold.withAlpha (isMouseOver() ? 1.0f : 0.6f) : col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, isMouseOver() && ok ? 1.5f : 1.0f);

    // mini waveform
    if (ok)
    {
        const juce::AudioBuffer<float>* src = &r->mix;
        if (kind == HarmonyProcessor::Export::harmonies) src = &r->harmonies;
        const auto wa = b.reduced (10.0f, 6.0f).withTrimmedTop (19.0f).withTrimmedBottom (15.0f);
        const int n = src->getNumSamples();
        if (kind == HarmonyProcessor::Export::stems)
        {
            int k = 0;
            for (int v = 0; v < vh::numVoices; ++v) if (r->active[(size_t) v]) ++k;
            int row = 0;
            for (int v = 0; v < vh::numVoices; ++v)
            {
                if (! r->active[(size_t) v]) continue;
                const float rh = wa.getHeight() / (float) juce::jmax (1, k);
                g.setColour (voiceColour (v).withAlpha (0.8f));
                g.fillRoundedRectangle (wa.getX(), wa.getY() + rh * row + 1.0f, wa.getWidth(), juce::jmax (2.0f, rh - 3.0f), 2.0f);
                ++row;
            }
        }
        else if (n > 0)
        {
            const int cols = (int) wa.getWidth() / 2;
            g.setColour (col::gold.withAlpha (0.75f));
            for (int c = 0; c < cols; ++c)
            {
                const int s0 = (int) ((juce::int64) c * n / cols), s1 = (int) ((juce::int64) (c + 1) * n / cols);
                const float m = src->getMagnitude (s0, juce::jmax (1, s1 - s0));
                const float h = juce::jmax (1.0f, m * wa.getHeight() * 0.5f);
                g.fillRect (wa.getX() + c * 2.0f, wa.getCentreY() - h, 1.5f, h * 2.0f);
            }
        }
    }
    g.setColour (ok ? col::ivory : col::muted2);
    g.setFont (uiFont (11.0f, true, 0.25f));
    g.drawText (title, juce::Rectangle<float> (10.0f, 5.0f, b.getWidth() - 20.0f, 18.0f), juce::Justification::centredLeft);
    g.setColour (flash.isNotEmpty() ? col::gold : col::muted2);
    g.setFont (uiFont (9.0f, true, 0.12f));
    g.drawText (flash.isNotEmpty() ? flash : (ok ? sub : "-"), juce::Rectangle<float> (10.0f, b.getHeight() - 17.0f, b.getWidth() - 20.0f, 14.0f),
                juce::Justification::centredLeft);
}

void DragTile::mouseDown (const juce::MouseEvent&) { dragStarted = false; }

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || r == nullptr || e.getDistanceFromDragStart() < 6) return;
    dragStarted = true;
    const auto files = proc.exportFiles (kind);
    if (! files.isEmpty()) juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
}

void DragTile::mouseUp (const juce::MouseEvent&)
{
    if (dragStarted || r == nullptr) { dragStarted = false; return; }
    // click: save the files and show them
    const auto files = proc.exportFiles (kind);
    if (! files.isEmpty())
    {
        juce::File (files[0]).revealToUser();
        flash = T ("Belgeler\\VonBerg Harmony'ye kaydedildi");
        flashUntil = juce::Time::getMillisecondCounter() + 2500;
        repaint();
    }
}

//==============================================================================
// PANEL
//==============================================================================
class HarmonyEditor::Panel : public juce::Component
{
public:
    Panel (HarmonyProcessor& p, std::function<void()> onScale, std::function<void()> onBrowse)
        : proc (p), editor (p),
          mixTile (p, HarmonyProcessor::Export::mix, "MIX", T ("vokal + harmoniler")),
          harmTile (p, HarmonyProcessor::Export::harmonies, "HARMONIES", T ("sadece harmoniler")),
          stemTile (p, HarmonyProcessor::Export::stems, "STEMS", T ("her ses ayrı dosya"))
    {
        addAndMakeVisible (editor);
        editor.onBrowse = onBrowse;
        editor.onSelectVoice = [this] (int v) { selectVoice (v); };

        for (int v = 0; v < vh::numVoices; ++v)
        {
            auto* s = strips.add (new VoiceStrip (p, v));
            s->onSelect = [this] (int x) { selectVoice (x); };
            addAndMakeVisible (s);
            auto* t = tabs.add (new VbButton (voiceLetter (v), VbButton::Style::segment));
            t->accent = voiceColour (v);
            t->onClick = [this, v] { selectVoice (v); };
            addAndMakeVisible (t);
        }

        keyBox.addItemList (vh::keyNames(), 1);
        scaleBox.addItemList (vh::scaleNames(), 1);
        keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "key", keyBox);
        scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "scale", scaleBox);
        addAndMakeVisible (keyBox);
        addAndMakeVisible (scaleBox);

        struct G { const char* id; const char* label; };
        const G gl[] { { "lead", "LEAD VOCAL" }, { "harmony", "HARMONIES" }, { "breath", "BREATH" }, { "tune", "PITCH LOCK" }, { "width", "WIDTH" } };
        for (auto& x : gl)
        {
            auto* s = globals.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
            s->onValueChange = [this] { repaint (globalsArea); };
            addAndMakeVisible (s);
            globalAtts.add (new juce::AudioProcessorValueTreeState::SliderAttachment (p.apvts, x.id, *s));
            globalLabels.add (x.label);
        }

        for (auto* t : { &mixTile, &harmTile, &stemTile }) addAndMakeVisible (t);

        recBtn.accent = col::rec;
        recBtn.onClick = [this] { proc.toggleRecord(); };
        openBtn.onClick = onBrowse;
        clearBtn.onClick = [this] { proc.clearClip(); };
        playBtn.onClick = [this] { proc.setPreview (! proc.isPreviewing(), 0); };
        prevPreset.onClick = [this] { stepPreset (-1); };
        nextPreset.onClick = [this] { stepPreset (1); };
        randomBtn.onClick = [this] { proc.randomize(); repaint(); };
        detectBtn.onClick = [this] { proc.applyDetectedKey(); };
        resetBtn.onClick = [this] { proc.resetEdits (voice); };
        fitBtn.onClick = [this] { editor.fitView(); };
        scaleBtn.onClick = std::move (onScale);
        for (auto* c : { &recBtn, &openBtn, &clearBtn, &playBtn, &prevPreset, &nextPreset, &randomBtn, &detectBtn, &resetBtn, &fitBtn, &scaleBtn })
            addAndMakeVisible (c);

        selectVoice (0);
        refresh();
    }

    void stepPreset (int d)
    {
        proc.applyPreset ((proc.currentPresetIndex() + d + HarmonyProcessor::numPresets()) % HarmonyProcessor::numPresets());
        repaint();
    }

    void selectVoice (int v)
    {
        voice = v;
        editor.setVoice (v);
        for (int i = 0; i < strips.size(); ++i)
        {
            strips[i]->selected = i == v;
            strips[i]->repaint();
            tabs[i]->setToggleState (i == v, juce::dontSendNotification);
        }
        repaint();
    }

    void setScaleText (int percent) { scaleBtn.setButtonText (juce::String (percent) + "%"); }
    NoteEditor& getEditor() { return editor; }

    void refresh()
    {
        editor.refresh();
        for (auto* s : strips) s->refresh();
        for (auto* t : { &mixTile, &harmTile, &stemTile }) t->refresh();

        const auto st = proc.getClipState();
        const bool recOn = st == HarmonyProcessor::ClipState::recording || st == HarmonyProcessor::ClipState::waitingForBar;
        recBtn.setToggleState (recOn, juce::dontSendNotification);
        recBtn.setButtonText (recOn ? "STOP" : "REC");
        const bool hasRender = proc.getRendered() != nullptr;
        playBtn.setEnabled (hasRender);
        playBtn.setToggleState (proc.isPreviewing(), juce::dontSendNotification);
        playBtn.setButtonText (proc.isPreviewing() ? "STOP" : "PREVIEW");
        playBtn.setIcon (proc.isPreviewing() ? VbButton::Icon::stop : VbButton::Icon::play);
        const auto det = proc.detectedKeyText();
        detectBtn.setEnabled (det.isNotEmpty());
        detectBtn.setButtonText (det.isNotEmpty() ? "AUTO: " + det.toUpperCase() : "AUTO KEY");
        resetBtn.setEnabled (proc.getEditCount (voice) > 0);
        juce::String info;
        if (auto a = proc.getAnalysis())
            info = proc.getClipName() + "   ·   " + juce::String ((int) a->notes.size()) + " notes   ·   "
                   + juce::String ((double) a->length / a->sampleRate, 1) + " s";
        if (info != infoText) { infoText = info; repaint(); }
        if (proc.currentPresetName() != presetText) { presetText = proc.currentPresetName(); repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        juce::ColourGradient bg (col::bgTop, b.getCentreX(), -60.0f, col::bgBottom, b.getCentreX(), b.getHeight() + 120.0f, true);
        bg.addColour (0.55, col::bgMid);
        g.setGradientFill (bg);
        g.fillRect (b);

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.035f), 0, 0, juce::Colours::transparentWhite, 0, 64, false));
        g.fillRect (0.0f, 0.0f, b.getWidth(), 64.0f);
        g.setColour (col::line);
        g.fillRect (0.0f, 63.0f, b.getWidth(), 1.0f);
        const auto lf = logoFont (26.0f);
        const float vonW = juce::GlyphArrangement::getStringWidth (lf, "Von");
        g.setFont (lf);
        g.setColour (juce::Colours::white);
        g.drawText ("Von", juce::Rectangle<float> (22.0f, 14.0f, 60.0f, 38.0f), juce::Justification::centredLeft);
        g.setColour (col::gold);
        g.drawText ("Berg", juce::Rectangle<float> (22.0f + vonW, 14.0f, 80.0f, 38.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff3a3d44));
        g.fillRect (132.0f, 21.0f, 1.0f, 22.0f);
        g.setColour (col::gold);
        g.setFont (uiFont (12.0f, true, 0.4f));
        g.drawText ("HARMONY", juce::Rectangle<float> (146.0f, 18.0f, 110.0f, 28.0f), juce::Justification::centredLeft);
        g.setColour (col::muted2);
        g.setFont (uiFont (11.0f, false, 0.05f));
        g.drawText (T ("Vokali bırak → harmoniyi seç / düzenle → playlist'e sürükle"), juce::Rectangle<float> (262.0f, 18.0f, 380.0f, 28.0f),
                    juce::Justification::centredLeft);
        g.setColour (col::muted2);
        g.setFont (uiFont (9.5f, true, 0.3f));
        g.drawText ("KEY", juce::Rectangle<float> (640.0f, 18.0f, 30.0f, 28.0f), juce::Justification::centredLeft);

        // toolbar
        g.setColour (col::muted);
        g.setFont (uiFont (11.5f, true));
        g.drawText (infoText, juce::Rectangle<float> (322.0f, 76.0f, 214.0f, 30.0f), juce::Justification::centredLeft, true);
        const juce::Rectangle<float> pr (628.0f, 77.0f, 190.0f, 28.0f);
        g.setColour (col::well);
        g.fillRoundedRectangle (pr, 14.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (pr.reduced (0.5f), 14.0f, 1.0f);
        g.setColour (col::ivory);
        g.setFont (uiFont (11.5f, true));
        g.drawText (presetText, pr, juce::Justification::centred);
        g.setColour (col::muted2);
        g.setFont (uiFont (9.5f, true, 0.3f));
        g.drawText ("PRESET", juce::Rectangle<float> (530.0f, 77.0f, 66.0f, 28.0f), juce::Justification::centredRight);

        // edit row
        g.setColour (col::muted2);
        g.setFont (uiFont (9.5f, true, 0.3f));
        g.drawText ("EDIT VOICE", juce::Rectangle<float> (22.0f, 118.0f, 80.0f, 24.0f), juce::Justification::centredLeft);
        g.setFont (uiFont (11.0f));
        g.drawText (T ("Notayı yukarı/aşağı sürükle (Alt = kromatik) · çift tık = sustur · sağ tık = menü · üst şeride tıkla = oradan çal · Ctrl+tekerlek = zoom"),
                    juce::Rectangle<float> (232.0f, 118.0f, 660.0f, 24.0f), juce::Justification::centredLeft);
        auto seg = tabs[0]->getBounds().getUnion (tabs[tabs.size() - 1]->getBounds()).toFloat().expanded (2.0f);
        g.setColour (col::bgBottom);
        g.fillRoundedRectangle (seg, seg.getHeight() * 0.5f);

        // globals
        for (int i = 0; i < globals.size(); ++i)
        {
            const auto sb = globals[i]->getBounds().toFloat();
            g.setColour (col::muted2);
            g.setFont (uiFont (9.5f, true, 0.25f));
            g.drawText (globalLabels[i], juce::Rectangle<float> (sb.getX(), sb.getY() - 18.0f, sb.getWidth(), 16.0f), juce::Justification::centredLeft);
            g.setColour (col::gold);
            g.setFont (uiFont (11.0f, true));
            g.drawText (juce::String ((int) globals[i]->getValue()) + "%", juce::Rectangle<float> (sb.getX(), sb.getY() - 18.0f, sb.getWidth(), 16.0f),
                        juce::Justification::centredRight);
        }
        g.setColour (col::line);
        g.fillRect (20.0f, 646.0f, 1060.0f, 1.0f);
        g.setColour (col::muted2);
        g.setFont (uiFont (9.5f, true, 0.3f));
        g.drawText (T ("PLAYLIST'E SÜRÜKLE"), juce::Rectangle<float> (676.0f, 650.0f, 200.0f, 16.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        keyBox.setBounds (670, 18, 70, 28);
        scaleBox.setBounds (746, 18, 160, 28);
        detectBtn.setBounds (912, 18, 116, 28);
        scaleBtn.setBounds (1034, 18, 48, 28);

        openBtn.setBounds (20, 76, 92, 30);
        recBtn.setBounds (118, 76, 92, 30);
        clearBtn.setBounds (216, 76, 92, 30);
        prevPreset.setBounds (600, 77, 28, 28);
        nextPreset.setBounds (818, 77, 28, 28);
        randomBtn.setBounds (852, 76, 104, 30);
        playBtn.setBounds (962, 76, 118, 30);

        for (int i = 0; i < tabs.size(); ++i) tabs[i]->setBounds (106 + i * 30, 119, 30, 22);
        fitBtn.setBounds (898, 117, 58, 26);
        resetBtn.setBounds (962, 117, 118, 26);
        editor.setBounds (20, 150, 1060, 290);

        for (int i = 0; i < strips.size(); ++i) strips[i]->setBounds (20 + i * 268, 452, 256, 184);

        globalsArea = { 20, 664, 640, 64 };
        for (int i = 0; i < globals.size(); ++i) globals[i]->setBounds (20 + i * 128, 690, 116, 24);
        mixTile.setBounds (676, 668, 130, 66);
        harmTile.setBounds (814, 668, 130, 66);
        stemTile.setBounds (952, 668, 128, 66);
    }

private:
    HarmonyProcessor& proc;
    NoteEditor editor;
    juce::OwnedArray<VoiceStrip> strips;
    juce::OwnedArray<VbButton> tabs;
    juce::OwnedArray<juce::Slider> globals;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> globalAtts;
    juce::StringArray globalLabels;
    juce::Rectangle<int> globalsArea;
    DragTile mixTile, harmTile, stemTile;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;

    VbButton recBtn     { "REC",     VbButton::Style::pill, VbButton::Icon::rec };
    VbButton openBtn    { "OPEN",    VbButton::Style::pill, VbButton::Icon::open };
    VbButton clearBtn   { "CLEAR",   VbButton::Style::pill, VbButton::Icon::clear };
    VbButton playBtn    { "PREVIEW", VbButton::Style::pill, VbButton::Icon::play };
    VbButton prevPreset { "", VbButton::Style::ghost, VbButton::Icon::left };
    VbButton nextPreset { "", VbButton::Style::ghost, VbButton::Icon::right };
    VbButton randomBtn  { "RANDOM",  VbButton::Style::gold, VbButton::Icon::dice };
    VbButton detectBtn  { "AUTO KEY", VbButton::Style::outline };
    VbButton resetBtn   { "RESET EDITS", VbButton::Style::pill, VbButton::Icon::clear };
    VbButton fitBtn     { "FIT",     VbButton::Style::pill };
    VbButton scaleBtn   { "100%",    VbButton::Style::outline };

    int voice = 0;
    juce::String presetText, infoText;
};

//==============================================================================
HarmonyEditor::HarmonyEditor (HarmonyProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    panel = std::make_unique<Panel> (p,
        [this]
        {
            static const int steps[] { 100, 125, 150, 75 };
            int next = 100;
            for (int i = 0; i < 4; ++i) if (steps[i] == scalePercent) next = steps[(i + 1) % 4];
            applyScale (next);
        },
        [this] { browse(); });
    addAndMakeVisible (*panel);
    panel->setBounds (0, 0, baseW, baseH);
    applyScale ((int) proc.apvts.state.getProperty ("uiScale", 100));
    startTimerHz (30);
}

HarmonyEditor::~HarmonyEditor() { setLookAndFeel (nullptr); }

void HarmonyEditor::applyScale (int percent)
{
    scalePercent = juce::jlimit (50, 300, percent);
    proc.apvts.state.setProperty ("uiScale", scalePercent, nullptr);
    const float s = (float) scalePercent / 100.0f;
    panel->setTransform (juce::AffineTransform::scale (s));
    panel->setScaleText (scalePercent);
    setSize (juce::roundToInt (baseW * s), juce::roundToInt (baseH * s));
}

void HarmonyEditor::resized() {}
void HarmonyEditor::timerCallback() { panel->refresh(); }

void HarmonyEditor::browse()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a vocal", juce::File(), "*.wav;*.wave;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.wma;*.aac");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadFile (fc.getResult()); });
}

bool HarmonyEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (HarmonyProcessor::isSupportedFile (f)) return true;
    return false;
}
void HarmonyEditor::fileDragEnter (const juce::StringArray&, int, int) { panel->getEditor().fileDragHover = true; panel->getEditor().repaint(); }
void HarmonyEditor::fileDragExit (const juce::StringArray&)           { panel->getEditor().fileDragHover = false; panel->getEditor().repaint(); }
void HarmonyEditor::filesDropped (const juce::StringArray& files, int, int)
{
    panel->getEditor().fileDragHover = false;
    panel->getEditor().repaint();
    for (auto& f : files) if (HarmonyProcessor::isSupportedFile (f)) { proc.loadFile (juce::File (f)); break; }
}
