#pragma once
#include "UI/Widgets.h"
#include "Engine/Oscillators.h"
#include "Modulation/Modulation.h"
#include <array>
#include <complex>
#include <vector>

namespace xum
{
// Base: componente que se repinta solo cuando cambian los parámetros que muestra.
class LiveDisplay : public juce::Component, private juce::Timer
{
public:
    explicit LiveDisplay (APVTS& a) : apvts (a) { startTimerHz (20); }
    ~LiveDisplay() override { stopTimer(); }

protected:
    float v (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    virtual float signature() const = 0;
    void fillBackground (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (Theme::dispTop, 0.f, 0.f, Theme::dispBot, 0.f, r.getBottom(), false));
        g.fillRect (r);
    }
    APVTS& apvts;

private:
    void timerCallback() override
    {
        const float s = signature();
        if (s != last) { last = s; repaint(); }
    }
    float last = -1e9f;
};

// Nombre de cada tabla de ondas (indice del parametro osc*_wave), en el idioma activo
inline juce::String waveDisplayName (int i)
{
    switch (i)
    {
        case 0:  return tr ("Basic Morph", "Basic Morph");
        case 1:  return tr ("Seno", "Sine");
        case 2:  return tr ("Triangular", "Triangle");
        case 3:  return tr ("Sierra", "Saw");
        case 4:  return tr ("Cuadrada / PWM", "Square / PWM");
        default: return tr ("Formantes", "Formant");
    }
}

// ------------------------------------------------------------------ visor de oscilador
// Wavetable: tabla en 3D + recuadro con la forma de onda REAL (ya con los dos warps) + nombre.
// Granular: forma de onda del audio de origen, posicion de escaneo y granos.
class OscDisplay : public LiveDisplay
{
public:
    OscDisplay (APVTS& a, int index) : LiveDisplay (a), idx (index + 1)
    {
        rebuildFrames (0);
        GrainSource::instance().peaks (srcPeaks.data(), kBars);
    }

    void paint (juce::Graphics& g) override
    {
        fillBackground (g);
        auto r = getLocalBounds().toFloat();
        const int type = (int) v (pid ("osc", idx, "type"));
        if (type == 0)      { paintWavetable (g, r); paintWaveInset (g, r); }
        else if (type == 3) paintGranular (g, r);
        else
        {
            static const char* names[5] = { "WAVETABLE", "MULTISAMPLE", "SAMPLE", "GRANULAR", "SPECTRAL" };
            g.setColour (Theme::text);
            g.setFont (Theme::font (12.f));
            g.drawText (names[std::clamp (type, 0, 4)], r.withTrimmedBottom (14.f), juce::Justification::centred);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (9.5f));
            g.drawText (tr ("MOTOR EN DESARROLLO", "ENGINE IN DEVELOPMENT"), r.withTrimmedTop (r.getHeight() * 0.5f + 4.f), juce::Justification::centredTop);
        }
    }

private:
    static constexpr int kShown = 24, kPts = 128, kBars = 90;

    void rebuildFrames (int wave)
    {
        cachedWave = wave;
        const auto& bank = WavetableBank::instance();
        for (int f = 0; f < kShown; ++f)
            for (int i = 0; i < kPts; ++i)
                frames[(size_t) f][(size_t) i] = bank.read ((float) i / (float) kPts, (float) f / (float) (kShown - 1), 5, wave);
    }

    float signature() const override
    {
        return v (pid ("osc", idx, "type")) * 7.f + v (pid ("osc", idx, "pos")) * 13.f
             + v (pid ("osc", idx, "warp1")) * 3.f + v (pid ("osc", idx, "warp2")) * 2.f
             + v (pid ("osc", idx, "wave")) * 23.f + v (pid ("osc", idx, "warp1_mode")) * 17.f + v (pid ("osc", idx, "warp2_mode")) * 19.f
             + v (pid ("osc", idx, "grain_pos")) * 5.f + v (pid ("osc", idx, "grain_size")) * 0.01f
             + v (pid ("osc", idx, "grain_density")) * 0.1f + v (pid ("osc", idx, "grain_rand")) * 11.f;
    }

    static juce::String morphName (float pos)
    {
        const char* names[4] = { "SENO", "SIERRA", "CUADRADA", "FORMANTE" };
        const float seg = std::clamp (pos, 0.f, 1.f) * 3.f;
        const int   i   = std::min ((int) seg, 2);
        const float t   = seg - (float) i;
        if (t < 0.08f) return names[i];
        if (t > 0.92f) return names[i + 1];
        return juce::String (names[i]) + " > " + names[i + 1] + "  " + juce::String ((int) std::lround (t * 100.f)) + "%";
    }

    static juce::String warpName (int mode)
    {
        static const char* n[10] = { "OFF", "BEND+", "BEND-", "SYNC", "MIRROR", "FM", "RING", "FOLD", "DRIVE", "QUANT" };
        return n[std::clamp (mode, 0, 9)];
    }

    void paintWavetable (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const int wave = (int) v (pid ("osc", idx, "wave"));
        if (wave != cachedWave) rebuildFrames (wave);
        const float w = r.getWidth() * 0.60f, amp = r.getHeight() * 0.16f;
        const float depthX = r.getWidth() * 0.26f, depthY = r.getHeight() * 0.46f;
        const float baseX = r.getX() + r.getWidth() * 0.06f, baseY = r.getBottom() - r.getHeight() * 0.22f;
        auto buildPath = [&] (const float* pts, float t, juce::Path& line)
        {
            for (int i = 0; i < kPts; ++i)
            {
                const float x = baseX + t * depthX + (float) i / (float) kPts * w;
                const float y = baseY - t * depthY - pts[i] * amp;
                if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
            }
        };
        for (int f = kShown - 1; f >= 0; --f)
        {
            const float t = (float) f / (float) (kShown - 1);
            juce::Path line;
            buildPath (frames[(size_t) f].data(), t, line);
            juce::Path fill (line);
            fill.lineTo (baseX + t * depthX + w, baseY - t * depthY + amp);
            fill.lineTo (baseX + t * depthX, baseY - t * depthY + amp);
            fill.closeSubPath();
            g.setColour (Theme::dispTop.withAlpha (0.7f)); g.fillPath (fill);
            g.setColour (Theme::teal.withAlpha (0.55f));   g.strokePath (line, juce::PathStrokeType (1.f));
        }
        // frame actual (interpolado) en amarillo
        const auto& bank = WavetableBank::instance();
        const float pos = v (pid ("osc", idx, "pos"));
        std::array<float, kPts> cur{};
        for (int i = 0; i < kPts; ++i) cur[(size_t) i] = bank.read ((float) i / (float) kPts, pos, 5, wave);
        juce::Path line;
        buildPath (cur.data(), pos, line);
        g.setColour (Theme::yellow.withAlpha (0.25f)); g.strokePath (line, juce::PathStrokeType (5.f));
        g.setColour (Theme::yellow);                   g.strokePath (line, juce::PathStrokeType (1.8f));
    }

    // Recuadro con el ciclo real que se esta usando (tabla + warp 1 + warp 2) y su nombre
    void paintWaveInset (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float pos = v (pid ("osc", idx, "pos"));
        const int   wave = (int) v (pid ("osc", idx, "wave"));
        const int   m1 = (int) v (pid ("osc", idx, "warp1_mode")), m2 = (int) v (pid ("osc", idx, "warp2_mode"));
        const float a1 = v (pid ("osc", idx, "warp1")), a2 = v (pid ("osc", idx, "warp2"));

        // nombre de la onda (arriba a la izquierda)
        g.setColour (juce::Colours::white);
        g.setFont (Theme::font (10.5f));
        g.drawText (wave == 0 ? morphName (pos)
                              : waveDisplayName (wave).toUpperCase() + "  " + juce::String ((int) std::lround (pos * 100.f)) + "%",
                    r.withTrimmedLeft (6.f).withHeight (16.f), juce::Justification::centredLeft);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (8.5f));
        juce::String warps = "W1 " + warpName (m1) + " " + juce::String (a1, 2) + "   W2 " + warpName (m2) + " " + juce::String (a2, 2);
        g.drawText (warps, r.withTrimmedLeft (6.f).withTrimmedTop (14.f).withHeight (13.f), juce::Justification::centredLeft);

        // recuadro de la forma de onda
        const auto box = juce::Rectangle<float> (r.getRight() - r.getWidth() * 0.34f - 5.f, r.getY() + 5.f,
                                                 r.getWidth() * 0.34f, r.getHeight() * 0.36f);
        g.setColour (Theme::bg.withAlpha (0.65f));
        g.fillRoundedRectangle (box, 3.f);
        g.setColour (Theme::line);
        g.drawRoundedRectangle (box, 3.f, 1.f);
        g.setColour (Theme::line.brighter (0.2f));
        g.drawHorizontalLine ((int) box.getCentreY(), box.getX() + 2.f, box.getRight() - 2.f);

        const auto& bank = WavetableBank::instance();
        juce::Path p;
        const int n = 96;
        for (int i = 0; i < n; ++i)
        {
            const float ph = (float) i / (float) n;
            float q = WavetableEngine::warpPhase (WavetableEngine::warpPhase (ph, m1, a1), m2, a2);
            float y = bank.read (q, pos, 4, wave);
            y = WavetableEngine::warpOut (WavetableEngine::warpOut (y, ph, m1, a1), ph, m2, a2);
            const float x = box.getX() + 3.f + ph * (box.getWidth() - 6.f);
            const float yy = box.getCentreY() - std::clamp (y, -1.2f, 1.2f) * box.getHeight() * 0.38f;
            if (i == 0) p.startNewSubPath (x, yy); else p.lineTo (x, yy);
        }
        g.setColour (Theme::yellow.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (4.f));
        g.setColour (Theme::yellow);                   g.strokePath (p, juce::PathStrokeType (1.5f));
    }

    void paintGranular (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float mid = r.getCentreY(), h = r.getHeight() * 0.42f;
        const float bw = r.getWidth() / (float) kBars;
        for (int i = 0; i < kBars; ++i)
        {
            const float a = std::clamp (srcPeaks[(size_t) i], 0.05f, 1.f);
            g.setColour (Theme::green.withAlpha (0.85f));
            g.fillRect (r.getX() + (float) i * bw, mid - a * h, bw * 0.8f, a * h * 2.f);
        }
        const float pos = v (pid ("osc", idx, "grain_pos"));
        const float rnd = v (pid ("osc", idx, "grain_rand"));
        const float len = std::clamp (v (pid ("osc", idx, "grain_size")) / 500.f, 0.02f, 1.f) * r.getWidth() * 0.4f;
        const float x0 = r.getX() + pos * r.getWidth();
        // zona de aleatoriedad de la posicion
        g.setColour (Theme::yellow.withAlpha (0.12f));
        g.fillRect (std::max (r.getX(), x0 - rnd * r.getWidth() * 0.25f), r.getY(), rnd * r.getWidth() * 0.5f, r.getHeight());
        const int dens = std::min (40, (int) v (pid ("osc", idx, "grain_density")));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        for (int i = 0; i < dens; ++i)
            g.drawVerticalLine ((int) std::min (r.getRight() - 1.f, x0 + len * (float) i / (float) std::max (1, dens)), r.getY() + 6.f, r.getBottom() - 6.f);
        g.setColour (Theme::yellow);
        g.drawVerticalLine ((int) x0, r.getY(), r.getBottom());
        g.setColour (juce::Colours::white);
        g.setFont (Theme::font (10.5f));
        g.drawText (tr ("GRANULAR  -  ONDA DE ORIGEN", "GRANULAR  -  SOURCE WAVEFORM"), r.withTrimmedLeft (6.f).withHeight (16.f), juce::Justification::centredLeft);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (8.5f));
        g.drawText (tr ("fuente interna (carga de samples pr\xC3\xB3ximamente)", "internal source (sample loading coming soon)"), r.withTrimmedTop (r.getHeight() - 14.f), juce::Justification::centred);
    }

    int idx;
    int cachedWave = 0;
    std::array<std::array<float, kPts>, kShown> frames{};
    std::array<float, kBars> srcPeaks{};
};

// ------------------------------------------------------------------ visor de ruido
class NoiseDisplay : public LiveDisplay
{
public:
    explicit NoiseDisplay (APVTS& a) : LiveDisplay (a)
    {
        juce::Random rnd (777);
        for (auto& x : bars) x = rnd.nextFloat();
    }
    void paint (juce::Graphics& g) override
    {
        fillBackground (g);
        auto r = getLocalBounds().toFloat();
        const float lvl = 0.25f + 0.75f * v ("noise_level"), cut = v ("noise_cutoff");
        const float tone = std::clamp (std::log2 (cut / 20.f) / 10.f, 0.f, 1.f);   // 0 = grave, 1 = agudo
        const float cx = r.getCentreX(), n = (float) bars.size();
        for (int i = 0; i < (int) bars.size(); ++i)
        {
            const float t = (float) i / n;
            const float h = bars[(size_t) i] * lvl * r.getHeight() * 0.45f * (0.4f + 0.6f * std::sin (t * 3.1416f));
            const float x = r.getX() + t * r.getWidth();
            g.setColour (juce::Colours::white.withAlpha (0.25f + 0.5f * tone));
            g.drawVerticalLine ((int) x, r.getCentreY() - h, r.getCentreY() + h);
        }
        juce::ignoreUnused (cx);
    }
private:
    float signature() const override { return v ("noise_level") * 5.f + v ("noise_cutoff") * 0.001f; }
    std::array<float, 48> bars{};
};

// ------------------------------------------------------------------ visor de filtro: respuesta en frecuencia
class FilterDisplay : public LiveDisplay
{
public:
    explicit FilterDisplay (APVTS& a) : LiveDisplay (a) {}
    void setIndex (int i) { idx = i + 1; repaint(); }

    void paint (juce::Graphics& g) override
    {
        fillBackground (g);
        auto r = getLocalBounds().toFloat().reduced (0.f, 6.f);
        const auto type = (int) v (pid ("f", idx, "type"));
        const float cutoff = v (pid ("f", idx, "cutoff")), res = v (pid ("f", idx, "res"));
        const bool on = v (pid ("f", idx, "on")) > 0.5f;

        g.setColour (Theme::line);
        for (int k = 1; k < 4; ++k) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) k / 4.f), r.getX(), r.getRight());

        juce::Path p;
        const int n = 160;
        for (int i = 0; i <= n; ++i)
        {
            const float t = (float) i / (float) n;
            const float f = 20.f * std::pow (1000.f, t);
            const float db = std::clamp (response (type, f, cutoff, res), -42.f, 18.f);
            const float y = r.getBottom() - (db + 42.f) / 60.f * r.getHeight();
            const float x = r.getX() + t * r.getWidth();
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        juce::Path fill (p);
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        const auto col = on ? Theme::accent : Theme::textDim;
        g.setColour (col.withAlpha (0.35f)); g.fillPath (fill);
        g.setColour (col);                   g.strokePath (p, juce::PathStrokeType (1.6f));
    }

private:
    float signature() const override
    {
        return v (pid ("f", idx, "type")) * 11.f + v (pid ("f", idx, "cutoff")) * 0.01f
             + v (pid ("f", idx, "res")) * 7.f + v (pid ("f", idx, "on")) * 3.f + (float) idx * 100.f;
    }

    // Respuesta aproximada (dB) de cada tipo de filtro, solo para dibujar.
    static float response (int type, float f, float fc, float res)
    {
        using C = std::complex<float>;
        const float k = 2.f - 1.95f * res, x = f / fc;
        const float den2 = (1.f - x * x) * (1.f - x * x) + k * k * x * x;
        float mag = 1.f;
        switch (type)
        {
            case 0: { const C g = 1.f / (C (1.f, x)); const C g4 = g * g * g * g;
                      mag = std::abs (g4 / (C (1.f, 0.f) + res * 4.f * g4)); break; }
            case 1: mag = 1.f / std::sqrt (den2); break;
            case 2: mag = x * x / std::sqrt (den2); break;
            case 3: mag = k * x / std::sqrt (den2); break;
            case 4: { const float fb = res * 0.95f;
                      mag = 0.5f / std::sqrt (std::max (1e-3f, 1.f + fb * fb - 2.f * fb * std::cos (6.2831853f * x))); break; }
            default:
            {
                const float fl = 20.f * std::pow (1000.f, 0.5f);   // vocal central para el dibujo
                float acc = 0.f;
                static const float c[3] = { 1.f, 1.8f, 3.f };
                for (float m : c) { const float xx = f / (fl * m), kk = 2.f - 1.5f * res;
                                    acc += kk * xx / std::sqrt ((1.f - xx * xx) * (1.f - xx * xx) + kk * kk * xx * xx); }
                mag = acc; break;
            }
        }
        return 20.f * std::log10 (std::max (mag, 1e-4f));
    }

    int idx = 1;
};

// ------------------------------------------------------------------ visor de envolvente ADSR (con tensión)
class EnvDisplay : public LiveDisplay
{
public:
    explicit EnvDisplay (APVTS& a) : LiveDisplay (a) {}
    void setIndex (int i) { idx = i + 1; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::darkPanel);  g.fillRect (r);
        g.setColour (Theme::line);
        for (int k = 1; k < 8; ++k) g.drawVerticalLine ((int) (r.getWidth() * (float) k / 8.f), 0.f, r.getBottom());

        const float a = v (pid ("env", idx, "a")), d = v (pid ("env", idx, "d")), s = v (pid ("env", idx, "s"));
        const float rel = v (pid ("env", idx, "r")), t = v (pid ("env", idx, "tension"));
        const float hold = 0.25f * (a + d + rel + 0.05f);
        const float total = a + d + hold + rel;
        auto X = [&] (float sec) { return r.getX() + 4.f + sec / total * (r.getWidth() - 8.f); };
        auto Y = [&] (float lvl) { return r.getBottom() - 6.f - lvl * (r.getHeight() - 14.f); };
        auto shape = [&] (float x)
        {
            if (std::abs (t) < 0.01f) return x;
            const float kk = t * 5.f;
            return (std::exp (kk * x) - 1.f) / (std::exp (kk) - 1.f);
        };

        juce::Path p;
        p.startNewSubPath (X (0.f), Y (0.f));
        const int n = 40;
        for (int i = 1; i <= n; ++i) { const float x = (float) i / (float) n; p.lineTo (X (a * x), Y (shape (x))); }
        for (int i = 1; i <= n; ++i) { const float x = (float) i / (float) n; p.lineTo (X (a + d * x), Y (1.f + (s - 1.f) * shape (x))); }
        p.lineTo (X (a + d + hold), Y (s));
        for (int i = 1; i <= n; ++i) { const float x = (float) i / (float) n; p.lineTo (X (a + d + hold + rel * x), Y (s * (1.f - shape (x)))); }

        juce::Path fill (p);
        fill.lineTo (X (total), Y (0.f));
        fill.lineTo (X (0.f), Y (0.f));
        fill.closeSubPath();
        g.setColour (Theme::accent.withAlpha (0.22f)); g.fillPath (fill);
        g.setColour (Theme::accent);                   g.strokePath (p, juce::PathStrokeType (1.8f));

        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.f));
        g.drawText ("ENV " + juce::String (idx) + (idx == 1 ? "  (AMP)" : ""), getLocalBounds().reduced (8, 6), juce::Justification::topRight);
    }

private:
    float signature() const override
    {
        return v (pid ("env", idx, "a")) * 3.f + v (pid ("env", idx, "d")) * 5.f + v (pid ("env", idx, "s")) * 7.f
             + v (pid ("env", idx, "r")) * 11.f + v (pid ("env", idx, "tension")) * 13.f + (float) idx * 100.f;
    }
    int idx = 1;
};

// ------------------------------------------------------------------ visor de LFO (usa el LFO real, incluidos los modos de caos)
class LfoDisplay : public LiveDisplay
{
public:
    explicit LfoDisplay (APVTS& a) : LiveDisplay (a) {}
    void setIndex (int i) { idx = i + 1; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::darkPanel);  g.fillRect (r);
        g.setColour (Theme::line);
        for (int k = 1; k < 8; ++k) g.drawVerticalLine ((int) (r.getWidth() * (float) k / 8.f), 0.f, r.getBottom());
        g.drawHorizontalLine ((int) r.getCentreY(), 0.f, r.getRight());

        const int shape = (int) v (pid ("lfo", idx, "shape"));
        const bool bipolar = v (pid ("lfo", idx, "bipolar")) > 0.5f;
        Lfo lfo;
        lfo.setSampleRate (300.0);       // 300 muestras por ciclo a 1 Hz
        lfo.retrigger();
        juce::Path p;
        const int n = 600;               // 2 ciclos
        for (int i = 0; i < n; ++i)
        {
            float y = lfo.advance (1, 1.f, shape);
            if (! bipolar) y = 0.5f + 0.5f * y;
            const float px = r.getX() + (float) i / (float) (n - 1) * r.getWidth();
            const float py = bipolar ? r.getCentreY() - y * (r.getHeight() * 0.42f)
                                     : r.getBottom() - 8.f - y * (r.getHeight() - 16.f);
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (Theme::accent.withAlpha (0.18f)); g.strokePath (p, juce::PathStrokeType (6.f));
        g.setColour (Theme::accent);                   g.strokePath (p, juce::PathStrokeType (1.8f));
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.f));
        g.drawText ("LFO " + juce::String (idx), getLocalBounds().reduced (8, 6), juce::Justification::topRight);
    }

private:
    float signature() const override
    {
        return v (pid ("lfo", idx, "shape")) * 3.f + v (pid ("lfo", idx, "bipolar")) * 5.f + (float) idx * 100.f;
    }
    int idx = 1;
};

// ------------------------------------------------------------------ mapeo de velocidad / nota (lineal)
class MapDisplay : public juce::Component
{
public:
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Theme::darkPanel); g.fillRect (r);
        g.setColour (Theme::line);
        for (int k = 1; k < 4; ++k)
        {
            g.drawVerticalLine ((int) (r.getWidth() * (float) k / 4.f), 0.f, r.getBottom());
            g.drawHorizontalLine ((int) (r.getHeight() * (float) k / 4.f), 0.f, r.getRight());
        }
        g.setColour (Theme::accent);
        g.drawLine (r.getX() + 2.f, r.getBottom() - 2.f, r.getRight() - 2.f, r.getY() + 2.f, 1.8f);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.f));
        g.drawText (title, getLocalBounds().reduced (8, 6), juce::Justification::topLeft);
    }
private:
    juce::String title = "VELO";
};
}
