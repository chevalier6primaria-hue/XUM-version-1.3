#pragma once
#include "PluginProcessor.h"
#include "UI/Widgets.h"
#include "UI/Displays.h"
#include "UI/Panels.h"
#include "UI/Welcome.h"
#include "UI/FxPresets.h"
#include "Fx/Effects.h"
#include <functional>
#include <memory>
#include <vector>

namespace xum
{
// ------------------------------------------------------------------ boton de encendido (triangulo)
class PowerTri : public juce::ToggleButton
{
public:
    PowerTri() { setClickingTogglesState (true); }
    void attach (APVTS& apvts, const juce::String& paramId)
    {
        att.reset();
        att = std::make_unique<ButtonAtt> (apvts, paramId, *this);
    }
    void setAccent (juce::Colour c) { accent = c; }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const auto b = getLocalBounds().toFloat().reduced (3.f);
        juce::Path tri;
        tri.addTriangle (b.getX(), b.getY() + 3.f, b.getRight(), b.getY() + 3.f, b.getCentreX(), b.getBottom() - 2.f);
        const bool on = getToggleState();
        if (on) { g.setColour (accent.withAlpha (0.3f)); g.strokePath (tri, juce::PathStrokeType (5.f)); }
        g.setColour (on ? accent : (over ? juce::Colour (0xff6d7f8b) : juce::Colour (0xff3a4852)));
        g.fillPath (tri);
    }

private:
    juce::Colour accent { 0xff7ad94a };
    std::unique_ptr<ButtonAtt> att;
};

// ------------------------------------------------------------------ visor del EQ (nodos arrastrables)
class EqDisplay : public LiveDisplay
{
public:
    explicit EqDisplay (APVTS& a) : LiveDisplay (a) {}

    void paint (juce::Graphics& g) override
    {
        fillBackground (g);
        const auto r = area();
        // rejilla
        g.setColour (juce::Colour (0x22ffffff));
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
            g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
        for (int db = -12; db <= 12; db += 6)
            g.drawHorizontalLine ((int) yOf ((float) db), r.getX(), r.getRight());
        g.setColour (juce::Colour (0x44ffffff));
        g.drawHorizontalLine ((int) yOf (0.f), r.getX(), r.getRight());
        g.setFont (Theme::font (8.f, false));
        g.setColour (Theme::textDim);
        for (float f : { 100.f, 1000.f, 10000.f })
            g.drawText (f >= 1000.f ? juce::String ((int) (f / 1000.f)) + "k" : juce::String ((int) f),
                        (int) xOf (f) + 2, (int) r.getBottom() - 11, 26, 10, juce::Justification::centredLeft);

        // curva total
        Biquad bq[4];
        for (int b = 0; b < 4; ++b) bq[b] = Biquad::make (type (b), freq (b), gainDb (b), qv (b), kSr);
        juce::Path curve;
        for (int i = 0; i <= 240; ++i)
        {
            const float f = 20.f * std::pow (1000.f, (float) i / 240.f);
            float db = 0.f;
            for (int b = 0; b < 4; ++b)
                if (type (b) > 0) db += 20.f * std::log10 (std::max (1e-4f, bq[b].magnitude (f, kSr)));
            const float x = xOf (f), y = yOf (std::clamp (db, -24.f, 24.f));
            if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        juce::Path fill (curve);
        fill.lineTo (r.getRight(), yOf (0.f)); fill.lineTo (r.getX(), yOf (0.f)); fill.closeSubPath();
        g.setColour (Theme::accent.withAlpha (0.18f)); g.fillPath (fill);
        g.setColour (Theme::accent.withAlpha (0.25f)); g.strokePath (curve, juce::PathStrokeType (5.f));
        g.setColour (Theme::accent);                   g.strokePath (curve, juce::PathStrokeType (1.6f));

        // nodos de las bandas
        for (int b = 0; b < 4; ++b)
        {
            if (type (b) <= 0) continue;
            const auto c = node (b);
            g.setColour (juce::Colours::white.withAlpha (b == sel ? 1.f : 0.8f));
            g.fillEllipse (c.x - 7.f, c.y - 7.f, 14.f, 14.f);
            g.setColour (Theme::bg);
            g.setFont (Theme::font (9.f));
            g.drawText (juce::String (b + 1), (int) c.x - 7, (int) c.y - 7, 14, 14, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        sel = hit (e.position);
        if (sel < 0) return;
        for (auto* p : gestureParams (sel)) if (p != nullptr) p->beginChangeGesture();
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (sel < 0) return;
        const auto r = area();
        const float f = 20.f * std::pow (1000.f, std::clamp ((e.position.x - r.getX()) / r.getWidth(), 0.f, 1.f));
        setReal (idOf (sel, "freq"), f);
        const int t = type (sel);
        if (t >= 1 && t <= 3)
        {
            const float db = (r.getCentreY() - e.position.y) / (r.getHeight() * 0.5f) * 18.f;
            setReal (idOf (sel, "gain"), std::clamp (db, -18.f, 18.f));
        }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (sel >= 0) for (auto* p : gestureParams (sel)) if (p != nullptr) p->endChangeGesture();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int b = hit (e.position);
        if (b >= 0) setReal (idOf (b, "q"), std::clamp (qv (b) * std::exp (w.deltaY * 0.6f), 0.2f, 10.f));
    }

private:
    static constexpr float kSr = 44100.f;

    float signature() const override
    {
        float s = 0.f;
        for (int b = 0; b < 4; ++b) s += (float) type (b) * 3.f + freq (b) * 0.001f + gainDb (b) * 1.7f + qv (b) * 2.3f * (float) (b + 1);
        return s + (float) sel;
    }

    juce::String idOf (int b, const char* what) const { return "eq_b" + juce::String (b + 1) + "_" + what; }
    int   type (int b) const    { return (int) v (idOf (b, "type")); }
    float freq (int b) const    { return v (idOf (b, "freq")); }
    float gainDb (int b) const  { return v (idOf (b, "gain")); }
    float qv (int b) const      { return v (idOf (b, "q")); }

    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (8.f, 8.f); }
    float xOf (float f) const { const auto r = area(); return r.getX() + std::log (f / 20.f) / std::log (1000.f) * r.getWidth(); }
    float yOf (float db) const { const auto r = area(); return r.getCentreY() - db / 24.f * r.getHeight() * 0.5f; }
    juce::Point<float> node (int b) const
    {
        const int t = type (b);
        return { xOf (freq (b)), yOf ((t >= 1 && t <= 3) ? gainDb (b) : 0.f) };
    }
    int hit (juce::Point<float> p) const
    {
        int best = -1; float bd = 15.f;
        for (int b = 0; b < 4; ++b)
        {
            if (type (b) <= 0) continue;
            const float d = node (b).getDistanceFrom (p);
            if (d < bd) { bd = d; best = b; }
        }
        return best;
    }
    std::vector<juce::RangedAudioParameter*> gestureParams (int b)
    {
        return { apvts.getParameter (idOf (b, "freq")), apvts.getParameter (idOf (b, "gain")) };
    }
    void setReal (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    int sel = -1;
};

// ------------------------------------------------------------------ curva de transferencia de la distorsion
class DistDisplay : public LiveDisplay
{
public:
    explicit DistDisplay (APVTS& a) : LiveDisplay (a) {}
    void paint (juce::Graphics& g) override
    {
        fillBackground (g);
        const auto r = getLocalBounds().toFloat().reduced (6.f);
        g.setColour (juce::Colour (0x33ffffff));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        g.drawVerticalLine ((int) r.getCentreX(), r.getY(), r.getBottom());
        const int   t  = (int) v ("dist_type");
        const float dr = v ("dist_drive");
        const float gn = 1.f + dr * dr * 40.f, comp = 1.f / (1.f + dr * dr * 1.5f);
        const float bias = v ("dist_asym") * 0.5f, b0 = DistortionFx::shape (bias, t);
        juce::Path p;
        for (int i = 0; i <= 120; ++i)
        {
            const float x = (float) i / 120.f * 2.f - 1.f;
            const float y = std::clamp ((DistortionFx::shape (x * gn + bias, t) - b0) * comp, -1.2f, 1.2f);
            const float px = r.getX() + (x * 0.5f + 0.5f) * r.getWidth();
            const float py = r.getCentreY() - y * r.getHeight() * 0.42f;
            if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
        }
        g.setColour (Theme::accent.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (5.f));
        g.setColour (Theme::accent);                   g.strokePath (p, juce::PathStrokeType (1.6f));
    }
private:
    float signature() const override { return v ("dist_type") * 7.f + v ("dist_drive") * 13.f + v ("dist_asym") * 5.f; }
};

// ------------------------------------------------------------------ forma de la respuesta al impulso (Convolve)
class IrDisplay : public LiveDisplay
{
public:
    IrDisplay (APVTS& a, XumAudioProcessor& p) : LiveDisplay (a), proc (p)
    {
        for (int k = 0; k < 6; ++k)
        {
            auto buf = makeInternalIR (k, 22050.0);
            const int n = buf.getNumSamples(), per = std::max (1, n / kBars);
            const float* d = buf.getReadPointer (0);
            for (int b = 0; b < kBars; ++b)
            {
                float pk = 0.f;
                for (int i = b * per; i < (b + 1) * per && i < n; ++i) pk = std::max (pk, std::abs (d[i]));
                peaks[(size_t) k][(size_t) b] = pk;
            }
            float mx = 1e-6f;
            for (float pv : peaks[(size_t) k]) mx = std::max (mx, pv);
            for (auto& pv : peaks[(size_t) k]) pv /= mx;
        }
    }

    void paint (juce::Graphics& g) override
    {
        fillBackground (g);
        const auto r = getLocalBounds().toFloat();
        const int ir = (int) v ("con_ir");
        const float mid = r.getCentreY(), h = r.getHeight() * 0.42f, bw = r.getWidth() / (float) kBars;
        if (ir < 6)
            for (int i = 0; i < kBars; ++i)
            {
                const float a = std::max (0.03f, peaks[(size_t) ir][(size_t) i]);
                g.setColour (Theme::green.withAlpha (0.9f));
                g.fillRect (r.getX() + (float) i * bw, mid - a * h, bw * 0.8f, a * h * 2.f);
            }
        g.setColour (juce::Colours::white);
        g.setFont (Theme::font (10.f));
        const juce::String name = ir == 6 ? (proc.getCustomIRName().isEmpty() ? juce::String ("CUSTOM") : proc.getCustomIRName())
                                          : juce::String::fromUTF8 ("IR");
        g.drawText (ir == 6 ? name : (tr ("RESPUESTA AL IMPULSO", "IMPULSE RESPONSE")), r.withTrimmedLeft (6.f).withHeight (16.f),
                    juce::Justification::centredLeft);
    }
private:
    static constexpr int kBars = 80;
    float signature() const override { return v ("con_ir") * 11.f + (float) proc.getCustomIRName().hashCode() * 1e-9f; }
    XumAudioProcessor& proc;
    std::array<std::array<float, kBars>, 6> peaks{};
};

// ------------------------------------------------------------------ descripcion de cada modulo
struct FxCtl { char kind; const char* id; const char* label; int x, y, w; };   // K knob, C combo, T toggle, N numero
struct FxDesc { const char* title; juce::uint32 colour; int height; bool hasMix; int display; std::vector<FxCtl> ctls; };

inline FxCtl fxK (const char* id, const char* label, int x, int y = -1) { return { 'K', id, label, x, y, 56 }; }
inline FxCtl fxC (const char* id, int x, int y, int w)                  { return { 'C', id, "", x, y, w }; }
inline FxCtl fxT (const char* id, const char* label, int x, int y, int w = 90) { return { 'T', id, label, x, y, w }; }
inline FxCtl fxN (const char* id, const char* label, int x)             { return { 'N', id, label, x, -1, 52 }; }

inline const std::vector<FxDesc>& fxDescs()
{
    static const std::vector<FxDesc> d = {
        { "HYPER / DIMENSION", 0xff52d8ff, 92, true, 0,
          { fxC ("hyp_mode", 0, 12, 104), fxK ("hyp_rate", "RATE", 116), fxK ("hyp_depth", "DEPTH", 176), fxN ("hyp_voices", "UNISON", 236),
            fxK ("hyp_detune", "DETUNE", 296), fxK ("hyp_size", "SIZE", 356) } },
        { "DISTORTION", 0xffff7b3a, 104, true, 1,
          { fxC ("dist_type", 0, 10, 112), fxC ("dist_fpos", 0, 36, 54), fxC ("dist_ftype", 58, 36, 54), fxK ("dist_ffreq", "FREQ", 126),
            fxK ("dist_fres", "Q", 186), fxK ("dist_drive", "DRIVE", 258), fxK ("dist_asym", "ASYM", 318) } },
        { "BODE", 0xffff5c7a, 92, true, 0,
          { fxK ("bode_shift", "SHIFT Hz", 0), fxC ("bode_dir", 70, 36, 70), fxK ("bode_feed", "FEED", 160) } },
        { "CHORUS", 0xff4fd6ff, 92, true, 0,
          { fxN ("cho_voices", "VOICES", 0), fxK ("cho_rate", "RATE", 70), fxK ("cho_depth", "DEPTH", 130), fxK ("cho_delay", "DELAY", 190),
            fxK ("cho_fb", "FEED", 250), fxK ("cho_lpf", "LPF", 310), fxK ("cho_width", "WIDTH", 370) } },
        { "FLANGER", 0xffb07cff, 92, true, 0,
          { fxK ("fla_delay", "DELAY", 0), fxK ("fla_depth", "DEPTH", 60), fxK ("fla_rate", "RATE", 120), fxK ("fla_fb", "FEED", 180),
            fxT ("fla_inv", "INVERT", 256, 18), fxT ("fla_sync", "SYNC", 256, 50), fxC ("fla_div", 340, 36, 70) } },
        { "PHASER", 0xffff6ad5, 92, true, 0,
          { fxN ("pha_stages", "STAGES", 0), fxK ("pha_freq", "FREQ", 70), fxK ("pha_fb", "FEED", 130), fxK ("pha_rate", "RATE", 190),
            fxK ("pha_depth", "DEPTH", 250), fxK ("pha_offset", "STEREO", 310) } },
        { "DELAY", 0xffffd24a, 104, true, 0,
          { fxT ("del_sync", "SYNC", 0, 18, 70), fxT ("del_pp", "PING-PONG", 0, 50, 96),
            fxK ("del_timeL", "TIME L", 104, 8), fxC ("del_divL", 104, 74, 56), fxK ("del_timeR", "TIME R", 170, 8), fxC ("del_divR", 170, 74, 56),
            fxK ("del_fb", "FEED", 250), fxK ("del_hp", "LOW CUT", 310), fxK ("del_lp", "HIGH CUT", 370), fxK ("del_sat", "SAT", 430) } },
        { "REVERB", 0xff7ad94a, 92, true, 0,
          { fxC ("rev_algo", 0, 12, 104), fxK ("rev_size", "SIZE", 116), fxK ("rev_decay", "DECAY", 176), fxK ("rev_pre", "PRE-DLY", 236),
            fxK ("rev_damp", "DAMP", 296), fxK ("rev_mod", "MOD", 356), fxK ("rev_lo", "LOW", 416) } },
        { "CONVOLVE", 0xff89e04a, 104, true, 3,
          { fxC ("con_ir", 0, 10, 112), fxK ("con_tone", "TONE", 356), fxK ("con_pre", "PRE-DLY", 416), fxK ("con_gain", "GAIN", 476) } },
        { "COMPRESSOR", 0xff3ee0b0, 92, true, 0,
          { fxK ("comp_thr", "THRESH", 0), fxK ("comp_ratio", "RATIO", 60), fxK ("comp_att", "ATTACK", 120), fxK ("comp_rel", "RELEASE", 180),
            fxK ("comp_gain", "GAIN", 240), fxT ("comp_multi", "MULTIBAND", 320, 36, 100) } },
        { "EQ", 0xff5aa0ff, 232, false, 2,
          { fxC ("eq_b1_type", 370, 10, 88), fxK ("eq_b1_freq", "FREQ", 386, 36), fxK ("eq_b1_gain", "GAIN", 386, 92), fxK ("eq_b1_q", "Q", 386, 148),
            fxC ("eq_b2_type", 462, 10, 88), fxK ("eq_b2_freq", "FREQ", 478, 36), fxK ("eq_b2_gain", "GAIN", 478, 92), fxK ("eq_b2_q", "Q", 478, 148),
            fxC ("eq_b3_type", 554, 10, 88), fxK ("eq_b3_freq", "FREQ", 570, 36), fxK ("eq_b3_gain", "GAIN", 570, 92), fxK ("eq_b3_q", "Q", 570, 148),
            fxC ("eq_b4_type", 646, 10, 88), fxK ("eq_b4_freq", "FREQ", 662, 36), fxK ("eq_b4_gain", "GAIN", 662, 92), fxK ("eq_b4_q", "Q", 662, 148) } },
        { "UTILITY", 0xff9db4c4, 104, true, 0,
          { fxT ("util_polL", "L INV", 0, 16, 70), fxT ("util_polR", "R INV", 0, 48, 70), fxK ("util_lpf", "LPF", 80), fxK ("util_hpf", "HPF", 140),
            fxT ("util_mono", "MONO", 210, 16, 70), fxT ("util_mbass", "MONO BASS", 210, 48, 100), fxK ("util_mfreq", "FREQ", 312),
            fxK ("util_width", "WIDTH", 372), fxK ("util_pan", "PAN", 432), fxK ("util_gain", "GAIN", 492) } },
    };
    return d;
}

// ------------------------------------------------------------------ una fila del rack
class FxRow : public juce::Component
{
public:
    FxRow (XumAudioProcessor& p, int index, std::function<void()> loadIrCallback)
        : proc (p), apvts (p.apvts), idx (index), desc (fxDescs()[(size_t) index]), prefix (kFxPrefix[index]),
          accent (desc.colour)
    {
        power.setAccent (accent);
        power.attach (apvts, prefix + "_on");
        addAndMakeVisible (power);

        for (const auto& c : desc.ctls)
        {
            const juce::String id (c.id);
            switch (c.kind)
            {
                case 'K':
                {
                    auto k = std::make_unique<Knob> (apvts, id, juce::String (c.label), accent, Theme::text);
                    k->slider.setScrollWheelEnabled (false);          // la rueda desplaza el rack
                    addAndMakeVisible (*k);
                    knobs.push_back (std::move (k));
                    break;
                }
                case 'C':
                {
                    auto b = std::make_unique<juce::ComboBox>();
                    if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id))) styleCombo (*b, ch->choices);
                    addAndMakeVisible (*b);
                    combos.push_back (std::move (b));
                    comboAtts.push_back (std::make_unique<ComboAtt> (apvts, id, *combos.back()));
                    break;
                }
                case 'T':
                {
                    auto t = std::make_unique<LedToggle> (juce::String (c.label));
                    t->attach (apvts, id);
                    addAndMakeVisible (*t);
                    toggles.push_back (std::move (t));
                    break;
                }
                default:
                {
                    auto n = std::make_unique<NumBox> (paramRef (apvts, id), "", false, juce::String (c.label));
                    addAndMakeVisible (*n);
                    nums.push_back (std::move (n));
                    break;
                }
            }
        }

        if (desc.hasMix)
        {
            mixKnob = std::make_unique<Knob> (apvts, prefix + "_mix", "MIX", accent, Theme::text);
            mixKnob->slider.setScrollWheelEnabled (false);
            addAndMakeVisible (*mixKnob);
        }

        if (desc.display == 1) extra = std::make_unique<DistDisplay> (apvts);
        if (desc.display == 2) extra = std::make_unique<EqDisplay> (apvts);
        if (desc.display == 3)
        {
            extra = std::make_unique<IrDisplay> (apvts, proc);
            loadBtn.setText ("LOAD IR...");
            loadBtn.onClick = loadIrCallback;
            addAndMakeVisible (loadBtn);
        }
        if (extra != nullptr) addAndMakeVisible (*extra);

        // presets del efecto
        presetBox.addItem ("Init", 1);
        const auto& list = fxPresets (idx);
        for (size_t i = 0; i < list.size(); ++i) presetBox.addItem (list[i].name, (int) i + 2);
        presetBox.setTextWhenNothingSelected ("PRESETS");
        presetBox.onChange = [this] { applyPreset (presetBox.getSelectedId() - 1); };
        addAndMakeVisible (presetBox);

        onAtt = std::make_unique<juce::ParameterAttachment> (paramRef (apvts, prefix + "_on"),
                                                             [this] (float v) { isOn = v > 0.5f; repaint(); });
        onAtt->sendInitialUpdate();
    }

    int getRowHeight() const { return desc.height; }
    juce::String getTitle() const { return desc.title; }

    // niveles de salida del modulo (0..1+), con caida suave
    void setMeter (float l, float r)
    {
        meterL = std::max (l, meterL * 0.82f);
        meterR = std::max (r, meterR * 0.82f);
        repaint (getWidth() - 40, 0, 40, getHeight());
    }

    void resized() override
    {
        const int h = getHeight(), ox = 150;
        power.setBounds (12, 12, 26, 24);
        presetBox.setBounds (10, h - 30, 122, 20);
        size_t ik = 0, ic = 0, it = 0, in = 0;
        for (const auto& c : desc.ctls)
        {
            const int y = c.y >= 0 ? c.y : (h - 54) / 2;
            switch (c.kind)
            {
                case 'K': knobs[ik++]->setBounds (ox + c.x, y, 56, 54); break;
                case 'C': combos[ic++]->setBounds (ox + c.x, y, c.w, 20); break;
                case 'T': toggles[it++]->setBounds (ox + c.x, y, c.w, 22); break;
                default:  nums[in++]->setBounds (ox + c.x, (h - 38) / 2, 52, 38); break;
            }
        }
        if (mixKnob != nullptr) mixKnob->setBounds (getWidth() - 118, (h - 54) / 2, 60, 54);
        if (extra != nullptr)
        {
            if (desc.display == 1) extra->setBounds (ox + 392, 10, 150, h - 20);
            else if (desc.display == 2) extra->setBounds (ox, 10, 360, h - 20);
            else extra->setBounds (ox + 124, 10, 220, h - 20);
        }
        if (desc.display == 3) loadBtn.setBounds (ox, h - 36, 112, 24);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (Theme::darkPanel2.brighter (0.08f), 0.f, 0.f, Theme::darkPanel, 0.f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 4.f);
        g.setColour (juce::Colour (0xff222c35));
        g.fillRoundedRectangle (b.withWidth (142.f), 4.f);
        g.setColour (accent);
        g.fillRect (b.withWidth (3.f));
        g.setColour (isOn ? accent : accent.withAlpha (0.45f));
        g.setFont (Theme::font (15.f));
        g.drawText (desc.title, 46, 10, 94, 28, juce::Justification::centredLeft);
        g.setColour (Theme::line);
        g.drawVerticalLine (142, 4.f, b.getBottom() - 4.f);
        g.drawVerticalLine (getWidth() - 128, 4.f, b.getBottom() - 4.f);

        // medidor de salida
        const float mx = (float) getWidth() - 40.f, top = 8.f, hh = b.getHeight() - 16.f;
        for (int c = 0; c < 2; ++c)
        {
            const float lv = c == 0 ? meterL : meterR;
            const float db = 20.f * std::log10 (std::max (1e-4f, lv));
            const float fill = std::clamp ((db + 60.f) / 60.f, 0.f, 1.f);
            const float x = mx + (float) c * 11.f;
            g.setColour (juce::Colour (0xff10171d));
            g.fillRect (x, top, 8.f, hh);
            g.setColour (db > -1.f ? juce::Colour (0xffff4d4d) : (db > -9.f ? Theme::yellow : Theme::green));
            g.fillRect (x, top + hh * (1.f - fill), 8.f, hh * fill);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (isOn) return;
        juce::Graphics::ScopedSaveState save (g);
        g.excludeClipRegion (power.getBounds());
        g.setColour (juce::Colour (0x9906090d));                // modulo apagado: se opaca
        g.fillRect (getLocalBounds());
    }

private:
    void setReal (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    // 1) todos los parametros del modulo vuelven a su valor por defecto; 2) se aplican los del preset
    void applyPreset (int which)
    {
        for (const auto& fp : fxParamTable())
        {
            const juce::String id (fp.id);
            if (! id.startsWith (prefix + "_") || id.endsWith ("_on")) continue;
            setReal (id, fp.def);
        }
        if (which >= 1)
        {
            const auto& list = fxPresets (idx);
            if ((size_t) (which - 1) < list.size())
                for (const auto& kv : list[(size_t) (which - 1)].v) setReal (kv.first, kv.second);
        }
    }

    XumAudioProcessor& proc;
    APVTS& apvts;
    int idx;
    const FxDesc& desc;
    juce::String prefix;
    juce::Colour accent;
    bool isOn = false;
    float meterL = 0.f, meterR = 0.f;

    PowerTri power;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<ComboAtt>> comboAtts;
    std::vector<std::unique_ptr<LedToggle>> toggles;
    std::vector<std::unique_ptr<NumBox>> nums;
    std::unique_ptr<Knob> mixKnob;
    std::unique_ptr<juce::Component> extra;
    PillButton loadBtn;
    juce::ComboBox presetBox;
    std::unique_ptr<juce::ParameterAttachment> onAtt;
};

// ------------------------------------------------------------------ pagina FX
class FxPage : public juce::Component, private juce::Timer
{
public:
    explicit FxPage (XumAudioProcessor& p) : proc (p)
    {
        for (int i = 0; i < kNumFx; ++i)
        {
            rows.push_back (std::make_unique<FxRow> (p, i, [this] { chooseIR(); }));
            rack.addAndMakeVisible (*rows.back());
            leds[i].attach (p.apvts, juce::String (kFxPrefix[i]) + "_on");
            addAndMakeVisible (leds[i]);
        }
        viewport.setViewedComponent (&rack, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (10);
        addAndMakeVisible (viewport);
        startTimerHz (20);
    }
    ~FxPage() override { stopTimer(); }

    void resized() override
    {
        const int listW = 250;
        viewport.setBounds (listW + 6, 0, getWidth() - listW - 8, getHeight());
        const int rackW = viewport.getWidth() - viewport.getScrollBarThickness();
        int y = 0;
        for (auto& r : rows)
        {
            r->setBounds (0, y, rackW, r->getRowHeight());
            y += r->getRowHeight() + 6;
        }
        rack.setSize (rackW, y);
        for (int i = 0; i < kNumFx; ++i) leds[i].setBounds (18, 96 + i * 36, 22, 24);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::darkPanel);
        g.fillRect (getLocalBounds());
        g.setColour (Theme::bg);
        g.fillRect (0, 0, 250, getHeight());
        // pestanas MAIN / BUS 1 / BUS 2 (solo MAIN esta disponible)
        const char* tabs[3] = { "MAIN", "BUS 1", "BUS 2" };
        for (int i = 0; i < 3; ++i)
        {
            const juce::Rectangle<int> t (4 + i * 82, 6, 80, 28);
            g.setColour (i == 0 ? juce::Colour (0xff6b4a2a) : juce::Colour (0xff161e26));
            g.fillRoundedRectangle (t.toFloat(), 3.f);
            g.setColour (i == 0 ? juce::Colours::white : Theme::textDim);
            g.setFont (Theme::font (11.f));
            g.drawText (tabs[i], t, juce::Justification::centred);
        }
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.f, false));
        g.drawText (tr ("ORDEN DE PROCESO (de arriba a abajo)", "PROCESSING ORDER (top to bottom)"), 10, 58, 232, 22, juce::Justification::centredLeft);
        for (int i = 0; i < kNumFx; ++i)
        {
            g.setColour (juce::Colour (fxDescs()[(size_t) i].colour));
            g.fillRect (4, 96 + i * 36, 3, 24);
            g.setColour (Theme::text);
            g.setFont (Theme::font (12.f));
            g.drawText (fxDescs()[(size_t) i].title, 50, 96 + i * 36, 190, 24, juce::Justification::centredLeft);
        }
        g.setColour (Theme::line);
        g.drawVerticalLine (250, 0.f, (float) getHeight());
    }

    // un clic en el nombre del efecto desplaza el rack hasta ese modulo
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.x < 46 || e.x > 246 || e.y < 96) return;
        const int i = (e.y - 96) / 36;
        if (i < 0 || i >= kNumFx) return;
        int y = 0;
        for (int k = 0; k < i; ++k) y += rows[(size_t) k]->getRowHeight() + 6;
        viewport.setViewPosition (0, y);
    }

private:
    void timerCallback() override
    {
        if (! isShowing()) return;                      // oculto: no se anima (ahorra CPU)
        for (int i = 0; i < kNumFx; ++i)
            rows[(size_t) i]->setMeter (proc.fx.meter[i][0].load(), proc.fx.meter[i][1].load());
    }

    void chooseIR()
    {
        chooser = std::make_unique<juce::FileChooser> (tr ("Elige una respuesta al impulso (WAV, AIFF, FLAC)", "Choose an impulse response (WAV, AIFF, FLAC)"),
                                                       juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        juce::Component::SafePointer<FxPage> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (safe != nullptr && f.existsAsFile()) safe->proc.loadCustomIR (f);
                              });
    }

    struct Rack : public juce::Component {};

    XumAudioProcessor& proc;
    Rack rack;
    juce::Viewport viewport;
    std::vector<std::unique_ptr<FxRow>> rows;
    LedToggle leds[kNumFx];
    std::unique_ptr<juce::FileChooser> chooser;
};
}
