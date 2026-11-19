#include "HarmonyEngine.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace vh
{
//==============================================================================
// music helpers
//==============================================================================
juce::StringArray scaleNames()
{
    return { "Major", "Minor", "Harmonic Minor", "Dorian", "Phrygian / Kurdi", "Mixolydian", "Lydian", "Hicaz",
             "Pentatonic Major", "Pentatonic Minor" };
}

const std::vector<int>& scaleSteps (int scale)
{
    static const std::vector<int> s[numScales]
    {
        { 0, 2, 4, 5, 7, 9, 11 },   // major
        { 0, 2, 3, 5, 7, 8, 10 },   // natural minor
        { 0, 2, 3, 5, 7, 8, 11 },   // harmonic minor
        { 0, 2, 3, 5, 7, 9, 10 },   // dorian
        { 0, 1, 3, 5, 7, 8, 10 },   // phrygian / kurdi
        { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian
        { 0, 2, 4, 6, 7, 9, 11 },   // lydian
        { 0, 1, 4, 5, 7, 8, 10 },   // hicaz (phrygian dominant)
        { 0, 2, 4, 7, 9 },          // pentatonic major
        { 0, 3, 5, 7, 10 },         // pentatonic minor
    };
    return s[juce::jlimit (0, numScales - 1, scale)];
}

juce::StringArray keyNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

juce::String noteName (int midi)
{
    return keyNames()[((midi % 12) + 12) % 12] + juce::String (midi / 12 - 1);
}

static int floorDiv (int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
static int posMod (int a, int b) { return ((a % b) + b) % b; }

bool inScale (int midi, int root, int scale)
{
    const auto& s = scaleSteps (scale);
    return std::find (s.begin(), s.end(), posMod (midi - root, 12)) != s.end();
}

int diatonicShift (int midi, int steps, int root, int scale)
{
    const auto& s = scaleSteps (scale);
    const int n = (int) s.size();
    const int rel = midi - root;
    const int oct = floorDiv (rel, 12);
    const int pc = posMod (rel, 12);
    int deg = 0;
    for (int i = 0; i < n; ++i) if (s[(size_t) i] <= pc) deg = i;
    const int chroma = pc - s[(size_t) deg];      // out-of-scale notes keep their offset
    const int total = deg + steps;
    return root + (oct + floorDiv (total, n)) * 12 + s[(size_t) posMod (total, n)] + chroma;
}

int snapToScale (float midi, int root, int scale)
{
    const int c = (int) std::round (midi);
    int best = c;
    float bestD = 1.0e9f;
    for (int m = c - 3; m <= c + 3; ++m)
        if (inScale (m, root, scale) && std::abs ((float) m - midi) < bestD) { bestD = std::abs ((float) m - midi); best = m; }
    return best;
}

juce::String intervalName (int mode, int value)
{
    if (mode == semitoneMode)
    {
        if (value == 0) return "Unison";
        static const char* names[] { "", "m2", "M2", "m3", "M3", "4th", "Tritone", "5th", "m6", "M6", "m7", "M7", "Octave" };
        const int a = std::abs (value);
        juce::String s = (value > 0 ? "+" : "-") + juce::String (a) + " st";
        if (a <= 12) s << "  (" << names[a] << ")";
        else if (a == 24) s << "  (2 Oct)";
        else if (a == 19) s << "  (Oct+5th)";
        return s;
    }
    if (value == 0) return "Unison";
    static const char* ord[] { "", "2nd", "3rd", "4th", "5th", "6th", "7th", "Octave", "9th", "10th", "11th", "12th", "13th", "14th", "2 Oct" };
    const int a = juce::jlimit (1, 14, std::abs (value));
    return juce::String (ord[a]) + (value > 0 ? " Up" : " Down");
}

//==============================================================================
// analysis
//==============================================================================
static void decimate (const float* x, int n, int D, std::vector<float>& y)
{
    if (D <= 1) { y.assign (x, x + n); return; }
    const int half = 8 * D;
    std::vector<float> h ((size_t) (2 * half + 1));
    const double fc = 0.45 / D;
    double sum = 0.0;
    for (int k = -half; k <= half; ++k)
    {
        const double sinc = k == 0 ? 2.0 * fc : std::sin (2.0 * juce::MathConstants<double>::pi * fc * k) / (juce::MathConstants<double>::pi * k);
        const double t = (double) (k + half) / (2.0 * half);
        const double w = 0.42 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * t) + 0.08 * std::cos (4.0 * juce::MathConstants<double>::pi * t);
        h[(size_t) (k + half)] = (float) (sinc * w);
        sum += sinc * w;
    }
    for (auto& v : h) v = (float) (v / sum);
    y.assign ((size_t) (n / D), 0.0f);
    for (int i = 0; i < (int) y.size(); ++i)
    {
        const int c = i * D;
        float acc = 0.0f;
        const int k0 = juce::jmax (-half, -c), k1 = juce::jmin (half, n - 1 - c);
        for (int k = k0; k <= k1; ++k) acc += h[(size_t) (k + half)] * x[c + k];
        y[(size_t) i] = acc;
    }
}

namespace
{
    struct Yin
    {
        int W, tauMin, tauMax;
        std::vector<float> d, cm;
        Yin (double sr) : W (juce::jmax (32, (int) std::round (sr * 0.025))),
                          tauMin (juce::jmax (2, (int) std::floor (sr / 1100.0))),
                          tauMax ((int) std::ceil (sr / 65.0))
        {
            d.resize ((size_t) tauMax + 2);
            cm.resize ((size_t) tauMax + 2);
        }

        // returns period in samples (0 = none), ap = aperiodicity
        float run (const float* y, int ny, int start, float& ap)
        {
            auto at = [&] (int i) { return i >= 0 && i < ny ? y[i] : 0.0f; };
            const bool inside = start >= 0 && start + W + tauMax + 1 < ny;
            for (int tau = 1; tau <= tauMax + 1; ++tau)
            {
                float s = 0.0f;
                if (inside)
                {
                    const float* a = y + start;
                    const float* b = y + start + tau;
                    for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; s += df * df; }
                }
                else
                    for (int j = 0; j < W; ++j) { const float df = at (start + j) - at (start + j + tau); s += df * df; }
                d[(size_t) tau] = s;
            }
            float run = 0.0f;
            cm[0] = 1.0f;
            for (int tau = 1; tau <= tauMax + 1; ++tau)
            {
                run += d[(size_t) tau];
                cm[(size_t) tau] = run > 1.0e-12f ? d[(size_t) tau] * (float) tau / run : 1.0f;
            }
            int best = -1;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (cm[(size_t) tau] < 0.15f)
                {
                    while (tau + 1 <= tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                    best = tau;
                    break;
                }
            if (best < 0)
            {
                best = tauMin;
                for (int tau = tauMin; tau <= tauMax; ++tau) if (cm[(size_t) tau] < cm[(size_t) best]) best = tau;
            }
            ap = cm[(size_t) best];
            const float a = cm[(size_t) best - 1], b = cm[(size_t) best], c = cm[(size_t) best + 1];
            const float den = a - 2.0f * b + c;
            float off = std::abs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.0f;
            off = juce::jlimit (-1.0f, 1.0f, off);
            return (float) best + off;
        }
    };

    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        const size_t m = v.size() / 2;
        std::nth_element (v.begin(), v.begin() + (long) m, v.end());
        return v[m];
    }

    float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
}

float detectPitchHz (const float* x, int n, double sr)
{
    const int D = juce::jmax (1, (int) std::round (sr / 11025.0));
    std::vector<float> y;
    decimate (x, n, D, y);
    const double srd = sr / D;
    Yin yin (srd);
    std::vector<float> per;
    for (int s = 0; s + yin.W + yin.tauMax + 2 < (int) y.size(); s += yin.W / 2)
    {
        float ap = 1.0f;
        const float t = yin.run (y.data(), (int) y.size(), s, ap);
        if (ap < 0.3f && t > 0.0f) per.push_back (t);
    }
    if (per.empty()) return 0.0f;
    return (float) (srd / median (per));
}

std::shared_ptr<Analysis> analyse (const juce::AudioBuffer<float>& clip, double sr, const std::atomic<bool>* abort)
{
    auto A = std::make_shared<Analysis>();
    const int n = clip.getNumSamples();
    A->sampleRate = sr;
    A->length = n;
    A->hop = juce::jmax (64, (int) std::round (sr / 172.0));
    const int H = A->hop;
    if (n < H * 4) return A;

    std::vector<float> mono ((size_t) n);
    const int chans = juce::jmax (1, clip.getNumChannels());
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < chans; ++c) s += clip.getSample (c, i);
        mono[(size_t) i] = s / (float) chans;
    }

    // overview
    {
        const int cols = 1600;
        A->peaks.assign ((size_t) cols, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            auto& p = A->peaks[(size_t) ((juce::int64) i * cols / n)];
            p = juce::jmax (p, std::abs (mono[(size_t) i]));
        }
    }

    // ---- pitch track ------------------------------------------------------------------
    const int D = juce::jmax (1, (int) std::round (sr / 11025.0));
    std::vector<float> y;
    decimate (mono.data(), n, D, y);
    const double srd = sr / D;
    Yin yin (srd);

    const int F = n / H + 1;
    A->pitch.assign ((size_t) F, 0.0f);
    A->level.assign ((size_t) F, 0.0f);
    std::vector<float> ap ((size_t) F, 1.0f), hz ((size_t) F, 0.0f);
    float maxLevel = 0.0f;
    for (int f = 0; f < F; ++f)
    {
        const int c = f * H;
        double e = 0.0;
        int cnt = 0;
        for (int i = juce::jmax (0, c - H); i < juce::jmin (n, c + H); ++i) { e += mono[(size_t) i] * mono[(size_t) i]; ++cnt; }
        A->level[(size_t) f] = (float) std::sqrt (e / juce::jmax (1, cnt));
        maxLevel = juce::jmax (maxLevel, A->level[(size_t) f]);
    }
    for (int f = 0; f < F; ++f)
    {
        if ((f & 63) == 0 && abort != nullptr && abort->load()) return nullptr;
        if (A->level[(size_t) f] < 1.0e-4f || A->level[(size_t) f] < maxLevel * 0.01f) continue;
        const int cd = (int) ((juce::int64) f * H / D);
        float a = 1.0f;
        const float tau = yin.run (y.data(), (int) y.size(), cd - yin.W / 2, a);
        ap[(size_t) f] = a;
        if (a < 0.25f && tau > 0.0f) hz[(size_t) f] = (float) (srd / tau);
    }

    std::vector<float> raw ((size_t) F, 0.0f);
    for (int f = 0; f < F; ++f) if (hz[(size_t) f] > 0.0f) raw[(size_t) f] = hzToMidi (hz[(size_t) f]);

    // median filter (voiced neighbours only)
    std::vector<float> pm ((size_t) F, 0.0f);
    for (int f = 0; f < F; ++f)
    {
        if (raw[(size_t) f] <= 0.0f) continue;
        std::vector<float> w;
        for (int k = f - 2; k <= f + 2; ++k) if (k >= 0 && k < F && raw[(size_t) k] > 0.0f) w.push_back (raw[(size_t) k]);
        pm[(size_t) f] = median (w);
    }

    // voiced runs: bridge tiny gaps, drop tiny islands, fix octave jumps
    auto runs = [&] (const std::vector<float>& p)
    {
        std::vector<std::pair<int, int>> r;
        for (int f = 0; f < F;)
        {
            if (p[(size_t) f] <= 0.0f) { ++f; continue; }
            int e = f;
            while (e + 1 < F && p[(size_t) e + 1] > 0.0f) ++e;
            r.push_back ({ f, e });
            f = e + 1;
        }
        return r;
    };
    {
        auto r = runs (pm);
        for (size_t i = 0; i + 1 < r.size(); ++i)
        {
            const int gap = r[i + 1].first - r[i].second - 1;
            const float a = pm[(size_t) r[i].second], b = pm[(size_t) r[i + 1].first];
            if (gap <= 3 && std::abs (a - b) < 3.0f)
                for (int k = 1; k <= gap; ++k) pm[(size_t) (r[i].second + k)] = a + (b - a) * (float) k / (float) (gap + 1);
        }
    }
    for (auto [a, b] : runs (pm))
    {
        if (b - a + 1 < 5) { for (int f = a; f <= b; ++f) pm[(size_t) f] = 0.0f; continue; }
        std::vector<float> v (pm.begin() + a, pm.begin() + b + 1);
        const float med = median (v);
        for (int f = a; f <= b; ++f)
        {
            float& p = pm[(size_t) f];
            if (p - med > 9.0f) p -= 12.0f;
            else if (med - p > 9.0f) p += 12.0f;
            if (std::abs (p - med) > 9.0f) p = med;
        }
    }
    A->pitch = pm;

    // ---- notes ------------------------------------------------------------------------
    const int persist = juce::jmax (3, (int) std::round (0.045 * sr / H));
    const int minNote = juce::jmax (4, (int) std::round (0.07 * sr / H));
    struct Seg { int a, b; };
    std::vector<Seg> segs;
    for (auto [a, b] : runs (pm))
    {
        std::vector<float> sm ((size_t) (b - a + 1));
        for (int f = a; f <= b; ++f)
        {
            std::vector<float> w;
            for (int k = juce::jmax (a, f - 4); k <= juce::jmin (b, f + 4); ++k) w.push_back (pm[(size_t) k]);
            sm[(size_t) (f - a)] = median (w);
        }
        // split where the pitch leaves the running centre of the current note
        // (relative, so a singer who is off the 12-TET grid doesn't get chopped up)
        double sum = sm[0];
        int cnt = 1;
        int start = a, devStart = -1;
        for (int f = a + 1; f <= b; ++f)
        {
            const float s = sm[(size_t) (f - a)];
            const float cur = (float) (sum / cnt);
            if (std::abs (s - cur) > 0.7f)
            {
                if (devStart < 0) devStart = f;
                if (f - devStart + 1 >= persist)
                {
                    if (devStart > start) segs.push_back ({ start, devStart - 1 });
                    start = devStart;
                    sum = 0.0; cnt = 0;
                    for (int k = devStart; k <= f; ++k) { sum += sm[(size_t) (k - a)]; ++cnt; }
                    devStart = -1;
                }
            }
            else
            {
                devStart = -1;
                sum += s; ++cnt;
            }
        }
        segs.push_back ({ start, b });
    }

    auto segPitch = [&] (const Seg& s)
    {
        std::vector<float> v (pm.begin() + s.a, pm.begin() + s.b + 1);
        return median (v);
    };

    // merge short notes into a neighbour that touches them
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (size_t i = 0; i < segs.size(); ++i)
        {
            if (segs[i].b - segs[i].a + 1 >= minNote) continue;
            const bool prevTouch = i > 0 && segs[i - 1].b + 1 >= segs[i].a;
            const bool nextTouch = i + 1 < segs.size() && segs[i].b + 1 >= segs[i + 1].a;
            if (! prevTouch && ! nextTouch)
            {
                if (segs[i].b - segs[i].a + 1 < minNote / 2 + 2) { segs.erase (segs.begin() + (long) i); changed = true; break; }
                continue;
            }
            const float p = segPitch (segs[i]);
            size_t into = prevTouch ? i - 1 : i + 1;
            if (prevTouch && nextTouch && std::abs (segPitch (segs[i + 1]) - p) < std::abs (segPitch (segs[i - 1]) - p)) into = i + 1;
            segs[into].a = juce::jmin (segs[into].a, segs[i].a);
            segs[into].b = juce::jmax (segs[into].b, segs[i].b);
            segs.erase (segs.begin() + (long) i);
            changed = true;
            break;
        }
    }
    // join same-pitch neighbours with a tiny gap
    for (size_t i = 0; i + 1 < segs.size();)
    {
        if (segs[i + 1].a - segs[i].b <= 3 && (int) std::round (segPitch (segs[i])) == (int) std::round (segPitch (segs[i + 1])))
        {
            segs[i].b = segs[i + 1].b;
            segs.erase (segs.begin() + (long) i + 1);
        }
        else ++i;
    }

    for (auto& s : segs)
    {
        Note nt;
        nt.pitch = segPitch (s);
        nt.midi = (int) std::round (nt.pitch);
        nt.start = juce::jmax (0, s.a * H - H / 2);
        nt.end = juce::jmin (n, s.b * H + H / 2);
        A->notes.push_back (nt);
    }

    // ---- key (Krumhansl-Schmuckler) ---------------------------------------------------
    {
        double hist[12] {};
        for (auto& nt : A->notes) hist[posMod (nt.midi, 12)] += (double) (nt.end - nt.start);
        static const double maj[12] { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto corr = [&] (const double* prof, int root)
        {
            double mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += hist[(i + root) % 12]; my += prof[i]; }
            mx /= 12; my /= 12;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double a = hist[(i + root) % 12] - mx, b = prof[i] - my;
                sxy += a * b; sxx += a * a; syy += b * b;
            }
            return sxx > 0 ? sxy / std::sqrt (sxx * syy) : 0.0;
        };
        double best = -2.0;
        for (int r = 0; r < 12; ++r)
        {
            const double cM = corr (maj, r), cm = corr (mnr, r);
            if (cM > best) { best = cM; A->keyRoot = r; A->keyScale = major; }
            if (cm > best) { best = cm; A->keyRoot = r; A->keyScale = minor; }
        }
    }

    // ---- pitch marks ------------------------------------------------------------------
    std::vector<float> lp ((size_t) n);
    {
        const float a = std::exp (-2.0f * juce::MathConstants<float>::pi * 900.0f / (float) sr);
        float s1 = 0.0f, s2 = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            s1 = mono[(size_t) i] + a * (s1 - mono[(size_t) i]);
            s2 = s1 + a * (s2 - s1);
            lp[(size_t) i] = s2;
        }
    }
    auto periodAtFrame = [&] (float fr, int ra, int rb)
    {
        const int f0 = juce::jlimit (ra, rb, (int) std::floor (fr));
        const int f1 = juce::jlimit (ra, rb, f0 + 1);
        const float t = juce::jlimit (0.0f, 1.0f, fr - (float) f0);
        const float m = pm[(size_t) f0] + (pm[(size_t) f1] - pm[(size_t) f0]) * t;
        return (float) sr / (440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f));
    };
    for (auto [ra, rb] : runs (pm))
    {
        const int s0 = juce::jmax (0, ra * H - H / 2), s1 = juce::jmin (n - 1, rb * H + H / 2);
        float T = periodAtFrame ((float) s0 / H, ra, rb);
        auto argmax = [&] (int lo, int hi)
        {
            lo = juce::jlimit (0, n - 1, lo); hi = juce::jlimit (0, n - 1, hi);
            int b = lo;
            for (int i = lo + 1; i <= hi; ++i) if (lp[(size_t) i] > lp[(size_t) b]) b = i;
            return b;
        };
        int m = argmax (s0, s0 + (int) T);
        const size_t firstMark = A->marks.size();
        while (m <= s1)
        {
            A->marks.push_back (m);
            T = periodAtFrame ((float) m / H, ra, rb);
            A->markPeriod.push_back (T);
            const float g = (float) m + T;
            int m2 = argmax ((int) (g - 0.2f * T), (int) (g + 0.2f * T));
            if (m2 <= m + (int) (0.5f * T)) m2 = (int) std::round (g);
            m = m2;
        }
        // period = distance to the next mark when it is plausible
        for (size_t k = firstMark; k + 1 < A->marks.size(); ++k)
        {
            const float dist = (float) (A->marks[k + 1] - A->marks[k]);
            if (dist > 0.7f * A->markPeriod[k] && dist < 1.3f * A->markPeriod[k]) A->markPeriod[k] = dist;
        }
    }
    juce::ignoreUnused (ap);
    return A;
}

//==============================================================================
// harmony targets
//==============================================================================
int baseTarget (const Analysis& A, int note, const Voice& v, const Settings& s)
{
    const int m = A.notes[(size_t) note].midi;
    if (v.mode == semitoneMode) return m + v.interval;
    return diatonicShift (m, v.interval, s.keyRoot, s.scale);
}

int finalTarget (const Analysis& A, int note, const Voice& v, const Settings& s, const VoiceEdits& e)
{
    int off = 0;
    if (note < (int) e.size())
    {
        if (e[(size_t) note].mute) return -1;
        off = e[(size_t) note].offset;
    }
    return baseTarget (A, note, v, s) + off;
}

//==============================================================================
// render (TD-PSOLA)
//==============================================================================
void renderVoice (const float* mono, const Analysis& A, const Settings& S, int vi, const VoiceEdits& edits,
                  std::vector<float>& out, juce::uint32 seed)
{
    const int n = A.length;
    out.assign ((size_t) n, 0.0f);
    if (A.notes.empty() || n == 0) return;
    const auto& V = S.voices[(size_t) vi];
    const int H = A.hop, F = A.numFrames();
    const double sr = A.sampleRate;

    // owning note per frame (nearest note)
    std::vector<int> own ((size_t) F, 0);
    {
        size_t k = 0;
        for (int f = 0; f < F; ++f)
        {
            const int s = f * H;
            while (k + 1 < A.notes.size() && A.notes[k + 1].start <= s) ++k;
            int best = (int) k;
            if (s > A.notes[k].end && k + 1 < A.notes.size()
                && (A.notes[k + 1].start - s) < (s - A.notes[k].end)) best = (int) k + 1;
            own[(size_t) f] = best;
        }
    }

    std::vector<float> noteShift (A.notes.size()), noteMute (A.notes.size());
    for (size_t i = 0; i < A.notes.size(); ++i)
    {
        const int t = finalTarget (A, (int) i, V, S, edits);
        noteMute[i] = t < 0 ? 1.0f : 0.0f;
        noteShift[i] = t < 0 ? 0.0f : (float) (t - A.notes[i].midi);
    }

    // shift curve (semitones) + gain per frame
    std::vector<float> shift ((size_t) F), gain ((size_t) F);
    juce::Random rng ((juce::int64) seed * 7919 + vi * 104729);
    const float glideA = std::exp (-(float) H / (float) (0.022 * sr));
    const float gainA = std::exp (-(float) H / (float) (0.012 * sr));
    const float driftA = std::exp (-(float) H / (float) (0.18 * sr));
    float sCur = noteShift[(size_t) own[0]], gCur = 1.0f - noteMute[(size_t) own[0]], drift = 0.0f;
    for (int f = 0; f < F; ++f)
    {
        const int i = own[(size_t) f];
        const auto& nt = A.notes[(size_t) i];
        float target = noteShift[(size_t) i];
        const float p = A.pitch[(size_t) f];
        const int s = f * H;
        if (S.tune > 0.0f && p > 0.0f && s >= nt.start && s <= nt.end)
            target += S.tune * juce::jlimit (-2.0f, 2.0f, (float) nt.midi - p);
        sCur = target + (sCur - target) * glideA;
        drift = (rng.nextFloat() * 2.0f - 1.0f) + (drift - (rng.nextFloat() * 2.0f - 1.0f)) * driftA;
        const float d = V.humanize * 0.9f * drift;   // up to ~ +-25 cents of slow drift
        shift[(size_t) f] = sCur + V.detune / 100.0f + d;
        const float gt = 1.0f - noteMute[(size_t) i];
        gCur = gt + (gCur - gt) * gainA;
        gain[(size_t) f] = gCur;
    }
    auto frameVal = [&] (const std::vector<float>& v, double pos)
    {
        const double fr = pos / H;
        const int f0 = juce::jlimit (0, F - 1, (int) fr);
        const int f1 = juce::jmin (F - 1, f0 + 1);
        const float t = (float) juce::jlimit (0.0, 1.0, fr - f0);
        return v[(size_t) f0] + (v[(size_t) f1] - v[(size_t) f0]) * t;
    };
    auto src = [&] (double pos)
    {
        const int i = (int) std::floor (pos);
        if (i < 0 || i + 1 >= n) return 0.0f;
        const float t = (float) (pos - i);
        return mono[i] + (mono[i + 1] - mono[i]) * t;
    };

    const float fmt = std::pow (2.0f, V.formant / 12.0f);
    const double Tu = sr * 0.004;
    const auto& marks = A.marks;
    double ts = 0.0;
    while (ts < n)
    {
        // nearest analysis mark
        double ta = ts, T = Tu, S2 = Tu, Lh = Tu, f = 1.0;
        float g = S.breath;
        bool voiced = false;
        if (! marks.empty())
        {
            auto it = std::lower_bound (marks.begin(), marks.end(), (int) ts);
            size_t k = (size_t) (it - marks.begin());
            if (k == marks.size()) k = marks.size() - 1;
            else if (k > 0 && std::abs (marks[k - 1] - ts) < std::abs (marks[k] - ts)) k = k - 1;
            const double per = A.markPeriod[k];
            if (std::abs (marks[k] - ts) <= per * 1.01)
            {
                voiced = true;
                ta = marks[k];
                T = per;
                const double r = std::pow (2.0, frameVal (shift, ts) / 12.0);
                f = fmt;
                S2 = T / r;
                Lh = T / f;
                g = 1.0f;
            }
        }
        g *= frameVal (gain, ts) * (float) juce::jmin (1.0, S2 / Lh);
        if (g > 1.0e-5f)
        {
            const int o0 = (int) std::ceil (ts - Lh), o1 = (int) std::floor (ts + Lh);
            for (int o = juce::jmax (0, o0); o <= juce::jmin (n - 1, o1); ++o)
            {
                const double d = o - ts;
                const float w = 0.5f + 0.5f * std::cos ((float) (juce::MathConstants<double>::pi * d / Lh));
                out[(size_t) o] += g * w * (voiced ? src (ta + d * f) : src ((double) o));
            }
        }
        ts += juce::jmax (8.0, S2);
    }
}

bool render (const juce::AudioBuffer<float>& clip, const Analysis& A, const Settings& S, const Edits& E, Output& O,
             const std::atomic<bool>* abort)
{
    const int n = juce::jmin (clip.getNumSamples(), A.length);
    const double sr = A.sampleRate;
    O.sampleRate = sr;
    std::vector<float> mono ((size_t) n);
    const int chans = juce::jmax (1, clip.getNumChannels());
    for (int i = 0; i < n; ++i)
    {
        float s = 0.0f;
        for (int c = 0; c < chans; ++c) s += clip.getSample (c, i);
        mono[(size_t) i] = s / (float) chans;
    }

    int maxDelay = 0;
    for (auto& v : S.voices) if (v.on) maxDelay = juce::jmax (maxDelay, (int) std::round (v.delayMs * 0.001 * sr));
    const int len = n + maxDelay;

    O.harmonies.setSize (2, len);
    O.harmonies.clear();
    std::vector<float> vo;
    for (int v = 0; v < numVoices; ++v)
    {
        auto& st = O.stems[(size_t) v];
        const auto& V = S.voices[(size_t) v];
        O.active[(size_t) v] = V.on && V.level > 0.0f;
        if (! O.active[(size_t) v]) { st.setSize (2, 0); continue; }
        if (abort != nullptr && abort->load()) return false;
        renderVoice (mono.data(), A, S, v, E[(size_t) v], vo, (juce::uint32) (v + 1));
        st.setSize (2, len);
        st.clear();
        const int dl = (int) std::round (V.delayMs * 0.001 * sr);
        const float p = juce::jlimit (-1.0f, 1.0f, V.pan * S.width);
        const float ang = (p + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float lvl = V.level * S.harmony * juce::MathConstants<float>::sqrt2;
        const float gl = std::cos (ang) * lvl, gr = std::sin (ang) * lvl;
        for (int i = 0; i < n; ++i)
        {
            st.setSample (0, i + dl, vo[(size_t) i] * gl);
            st.setSample (1, i + dl, vo[(size_t) i] * gr);
        }
        for (int c = 0; c < 2; ++c) O.harmonies.addFrom (c, 0, st, c, 0, len);
    }

    O.mix.makeCopyOf (O.harmonies);
    for (int c = 0; c < 2; ++c)
        O.mix.addFrom (c, 0, clip, juce::jmin (c, clip.getNumChannels() - 1), 0, n, S.lead);

    // keep everything out of clipping with one common gain (stems stay in balance)
    const float pk = juce::jmax (O.mix.getMagnitude (0, len), O.harmonies.getMagnitude (0, len));
    if (pk > 0.98f)
    {
        const float g = 0.98f / pk;
        O.mix.applyGain (g);
        O.harmonies.applyGain (g);
        for (auto& st : O.stems) if (st.getNumSamples() > 0) st.applyGain (g);
    }

    const int cols = 1600;
    O.peaks.assign ((size_t) cols, 0.0f);
    for (int i = 0; i < len; ++i)
    {
        auto& pp = O.peaks[(size_t) ((juce::int64) i * cols / juce::jmax (1, len))];
        pp = juce::jmax (pp, juce::jmax (std::abs (O.mix.getSample (0, i)), std::abs (O.mix.getSample (1, i))));
    }
    return true;
}
} // namespace vh
