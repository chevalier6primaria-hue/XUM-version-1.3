#pragma once
#include "UI/Widgets.h"
#include "UI/Displays.h"

namespace xum
{
// Dibuja el fondo común de un panel claro: cuerpo de pizarra y franja de cabecera.
inline void paintSlatePanel (juce::Graphics& g, juce::Rectangle<float> b, float headerH = 27.f)
{
    g.setColour (Theme::slateBody);
    g.fillRoundedRectangle (b, 3.f);
    g.setColour (Theme::slateHead);
    g.fillRoundedRectangle (b.withHeight (headerH), 3.f);
    g.setColour (Theme::slateDark.withAlpha (0.8f));
    g.drawHorizontalLine ((int) headerH, b.getX(), b.getRight());
}

// Atenua el panel cuando su modulo esta apagado (se deja sin atenuar la zona 'keepClear', p. ej. el LED)
inline void paintOffOverlay (juce::Graphics& g, juce::Rectangle<int> bounds, juce::Rectangle<int> keepClear)
{
    juce::Graphics::ScopedSaveState save (g);
    g.excludeClipRegion (keepClear);
    g.setColour (juce::Colour (0xb0090d11));
    g.fillRoundedRectangle (bounds.toFloat(), 3.f);
}

inline void styleCombo (juce::ComboBox& box, juce::StringArray items)
{
    box.addItemList (items, 1);
    box.setJustificationType (juce::Justification::centredLeft);
}

// ================================================================== SUB
class SubPanel : public juce::Component
{
public:
    SubPanel (APVTS& a, ModBus& bus)
        : octBox (paramRef (a, "sub_octave"), "OCT ", true),
          waves (paramRef (a, "sub_wave")),
          satK (a, "sub_sat", "SAT"), levelK (a, "sub_level", "LEVEL")
    {
        addAndMakeVisible (octBox); addAndMakeVisible (waves);
        addAndMakeVisible (satK);   addAndMakeVisible (levelK);
        levelK.setModTarget (Dst::subLevel, &bus);
        powerLed.attach (a, "sub_on");                       // apagado al iniciar
        addAndMakeVisible (powerLed);
        onAtt = std::make_unique<juce::ParameterAttachment> (paramRef (a, "sub_on"), [this] (float v) { isOn = v > 0.5f; repaint(); });
        onAtt->sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        paintSlatePanel (g, b);
        g.setColour (Theme::ink);   g.setFont (Theme::font (13.f));
        g.drawText ("SUB", 24, 2, 44, 25, juce::Justification::centredLeft);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! isOn) paintOffOverlay (g, getLocalBounds(), powerLed.getBounds());
    }

    void resized() override
    {
        powerLed.setBounds (4, 3, 22, 22);
        octBox.setBounds (4, 52, getWidth() - 8, 20);
        waves.setBounds (4, 76, getWidth() - 8, 116);
        satK.setBounds (getWidth() / 2 - 26, 205, 52, 54);
        levelK.setBounds (getWidth() / 2 - 26, 260, 52, 54);
    }

private:
    NumBox octBox;
    WaveSelector waves;
    Knob satK, levelK;
    LedToggle powerLed;
    std::unique_ptr<juce::ParameterAttachment> onAtt;
    bool isOn = false;
};

// ================================================================== OSC A / B / C
class OscPanel : public juce::Component
{
public:
    OscPanel (APVTS& a, int index, const juce::String& title, ModBus& bus)
        : name (title), idx (index + 1),
          octBox  (paramRef (a, pid ("osc", idx, "octave")), "OCT ", true),
          semBox  (paramRef (a, pid ("osc", idx, "semi")),   "SEM ", true),
          finBox  (paramRef (a, pid ("osc", idx, "fine")),   "FIN ", true),
          unisonBox (paramRef (a, pid ("osc", idx, "voices")), "", false, "UNISON"),
          display (a, index),
          posK    (a, pid ("osc", idx, "pos"),    "WT POS"),
          detuneK (a, pid ("osc", idx, "detune"), "DETUNE"),
          blendK  (a, pid ("osc", idx, "blend"),  "BLEND"),
          spreadK (a, pid ("osc", idx, "spread"), "SPREAD"),
          warp1K  (a, pid ("osc", idx, "warp1"),  "WARP 1"),
          warp2K  (a, pid ("osc", idx, "warp2"),  "WARP 2"),
          scanK   (a, pid ("osc", idx, "grain_pos"),     "SCAN",   Theme::green),
          densK   (a, pid ("osc", idx, "grain_density"), "DENS",   Theme::green),
          lengthK (a, pid ("osc", idx, "grain_size"),    "LENGTH", Theme::green),
          randK   (a, pid ("osc", idx, "grain_rand"),    "RAND",   Theme::green),
          panK    (a, pid ("osc", idx, "pan"),   "PAN"),
          levelK  (a, pid ("osc", idx, "level"), "LEVEL")
    {
        styleCombo (typeBox, { "WAVETABLE", "MULTISAMPLE", "SAMPLE", "GRANULAR", "SPECTRAL" });
        typeBox.setColour (juce::ComboBox::backgroundColourId, Theme::slateHead.darker (0.12f));
        typeBox.setColour (juce::ComboBox::textColourId, Theme::ink);
        typeBox.setColour (juce::ComboBox::arrowColourId, Theme::ink);
        typeBox.setColour (juce::ComboBox::outlineColourId, Theme::slateDark);
        typeAtt = std::make_unique<ComboAtt> (a, pid ("osc", idx, "type"), typeBox);
        typeBox.onChange = [this] { updateVisibility(); repaint(); };

        // encendido del oscilador (apagado al iniciar) y selector de onda con flechas
        powerLed.attach (a, pid ("osc", idx, "on"));
        addAndMakeVisible (powerLed);
        onAtt = std::make_unique<juce::ParameterAttachment> (paramRef (a, pid ("osc", idx, "on")),
                                                             [this] (float v) { isOn = v > 0.5f; repaint(); });
        onAtt->sendInitialUpdate();
        waveAtt = std::make_unique<juce::ParameterAttachment> (paramRef (a, pid ("osc", idx, "wave")),
                                                               [this] (float v) { waveIndex = (int) std::lround (v); repaint (0, 28, getWidth(), 23); });
        waveAtt->sendInitialUpdate();

        // modos de warp (dos por oscilador)
        const juce::StringArray warpNames { "Off", "Bend+", "Bend-", "Sync", "Mirror", "FM", "Ring", "Fold", "Drive", "Quant" };
        styleCombo (warp1ModeBox, warpNames);
        styleCombo (warp2ModeBox, warpNames);
        for (auto* box : { &warp1ModeBox, &warp2ModeBox })
        {
            box->setJustificationType (juce::Justification::centred);
            box->setColour (juce::ComboBox::backgroundColourId, Theme::slateHead.darker (0.12f));
            box->setColour (juce::ComboBox::textColourId, Theme::ink);
            box->setColour (juce::ComboBox::arrowColourId, Theme::ink);
            box->setColour (juce::ComboBox::outlineColourId, Theme::slateDark);
        }
        warp1ModeAtt = std::make_unique<ComboAtt> (a, pid ("osc", idx, "warp1_mode"), warp1ModeBox);
        warp2ModeAtt = std::make_unique<ComboAtt> (a, pid ("osc", idx, "warp2_mode"), warp2ModeBox);

        // destinos de modulación (arrastra LFO / ENV / macro encima)
        const int dstBase = Dst::oscBase + index * Dst::perOsc;
        posK.setModTarget    (dstBase + Dst::oPos, &bus);
        detuneK.setModTarget (dstBase + Dst::oDetune, &bus);
        warp1K.setModTarget  (dstBase + Dst::oWarp1, &bus);
        warp2K.setModTarget  (dstBase + Dst::oWarp2, &bus);
        levelK.setModTarget  (dstBase + Dst::oLevel, &bus);
        levelK.slider.onValueChange = [this] { repaint (0, 0, getWidth(), 27); };

        for (juce::Component* c : { (juce::Component*) &typeBox, (juce::Component*) &octBox, (juce::Component*) &semBox,
                                    (juce::Component*) &finBox, (juce::Component*) &unisonBox, (juce::Component*) &display,
                                    (juce::Component*) &posK, (juce::Component*) &detuneK, (juce::Component*) &blendK,
                                    (juce::Component*) &spreadK, (juce::Component*) &warp1K, (juce::Component*) &warp2K,
                                    (juce::Component*) &scanK, (juce::Component*) &densK, (juce::Component*) &lengthK, (juce::Component*) &randK,
                                    (juce::Component*) &warp1ModeBox, (juce::Component*) &warp2ModeBox,
                                    (juce::Component*) &panK, (juce::Component*) &levelK })
            addAndMakeVisible (c);
        warp1K.setLabel ("");
        warp2K.setLabel ("");
        updateVisibility();
    }

    void paint (juce::Graphics& g) override
    {
        paintSlatePanel (g, getLocalBounds().toFloat());
        g.setColour (Theme::ink);
        g.setFont (Theme::font (13.f));
        g.drawText (name, 24, 2, 50, 25, juce::Justification::centredLeft);

        // nombre de la onda con flechas a los lados (solo en el motor Wavetable)
        const int type = typeBox.getSelectedItemIndex();
        const juce::String src = type == 0 ? waveDisplayName (waveIndex)
                                           : (type == 3 ? tr ("Fuente interna", "Internal source") : juce::String ("---"));
        g.setFont (Theme::font (12.f));
        g.drawText (src, 26, 28, getWidth() - 52, 23, juce::Justification::centred);
        if (type == 0)
        {
            g.setColour (Theme::ink.withAlpha (0.75f));
            juce::Path left, right;
            const float cy = 39.5f, w = (float) getWidth();
            left.addTriangle (18.f, cy - 6.f, 18.f, cy + 6.f, 10.f, cy);
            right.addTriangle (w - 18.f, cy - 6.f, w - 18.f, cy + 6.f, w - 10.f, cy);
            g.fillPath (left);
            g.fillPath (right);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! isOn) paintOffOverlay (g, getLocalBounds(), powerLed.getBounds());
    }

    // flechas: onda anterior / siguiente; clic en el nombre: lista de ondas
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y < 28 || e.y > 51 || typeBox.getSelectedItemIndex() != 0) return;
        const int n = WavetableBank::numBanks;
        if (e.x < 30)                   setWave ((waveIndex + n - 1) % n);
        else if (e.x > getWidth() - 30) setWave ((waveIndex + 1) % n);
        else
        {
            juce::PopupMenu m;
            for (int i = 0; i < n; ++i) m.addItem (i + 1, waveDisplayName (i), true, i == waveIndex);
            juce::Component::SafePointer<OscPanel> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                             [safe] (int r) { if (safe != nullptr && r > 0) safe->setWave (r - 1); });
        }
    }

    void resized() override
    {
        const int w = getWidth();
        powerLed.setBounds (4, 3, 22, 22);
        typeBox.setBounds (76, 4, w - 80, 20);
        octBox.setBounds (4, 52, 84, 20);
        semBox.setBounds (92, 52, 84, 20);
        finBox.setBounds (180, 52, w - 184, 20);
        display.setBounds (4, 76, w - 8, 118);

        const int cx[5] = { 28, 84, 140, 192, 240 };
        auto place = [] (juce::Component& c, int centreX, int centreY)
        { c.setBounds (centreX - 26, centreY - 19, 52, 54); };
        place (posK, cx[0], 224);  place (unisonBox, cx[1], 224);  place (detuneK, cx[2], 224);  place (blendK, cx[3], 224);
        place (warp1K, cx[0], 279); place (warp2K, cx[1], 279);    place (spreadK, cx[2], 279);
        place (scanK, cx[0], 224);  place (densK, cx[1], 224);     place (lengthK, cx[2], 224);  place (randK, cx[3], 224);
        warp1ModeBox.setBounds (cx[0] - 27, 301, 54, 14);
        warp2ModeBox.setBounds (cx[1] - 27, 301, 54, 14);
        place (panK, cx[4], 224);   place (levelK, cx[4], 279);
        unisonBox.setBounds (cx[1] - 24, 206, 48, 38);
    }

private:
    void setWave (int i) { waveAtt->setValueAsCompleteGesture ((float) i); }

    void updateVisibility()
    {
        const bool granular = typeBox.getSelectedItemIndex() == 3;
        for (juce::Component* c : { (juce::Component*) &posK, (juce::Component*) &unisonBox, (juce::Component*) &detuneK,
                                    (juce::Component*) &blendK, (juce::Component*) &warp1K, (juce::Component*) &warp2K,
                                    (juce::Component*) &spreadK, (juce::Component*) &warp1ModeBox, (juce::Component*) &warp2ModeBox })
            c->setVisible (! granular);
        for (juce::Component* c : { (juce::Component*) &scanK, (juce::Component*) &densK, (juce::Component*) &lengthK, (juce::Component*) &randK })
            c->setVisible (granular);
    }

    juce::String name;
    int idx;
    bool isOn = false;
    int waveIndex = 0;
    LedToggle powerLed;
    std::unique_ptr<juce::ParameterAttachment> onAtt, waveAtt;
    juce::ComboBox typeBox;
    std::unique_ptr<ComboAtt> typeAtt;
    NumBox octBox, semBox, finBox, unisonBox;
    OscDisplay display;
    juce::ComboBox warp1ModeBox, warp2ModeBox;
    std::unique_ptr<ComboAtt> warp1ModeAtt, warp2ModeAtt;
    Knob posK, detuneK, blendK, spreadK, warp1K, warp2K, scanK, densK, lengthK, randK, panK, levelK;
};

// ================================================================== NOISE
class NoisePanel : public juce::Component
{
public:
    NoisePanel (APVTS& a, ModBus& bus)
        : display (a), filterK (a, "noise_cutoff", "FILTER"), levelK (a, "noise_level", "LEVEL")
    {
        addAndMakeVisible (display); addAndMakeVisible (filterK); addAndMakeVisible (levelK);
        levelK.setModTarget (Dst::noiseLevel, &bus);
        powerLed.attach (a, "noise_on");
        addAndMakeVisible (powerLed);
        onAtt = std::make_unique<juce::ParameterAttachment> (paramRef (a, "noise_on"), [this] (float v) { isOn = v > 0.5f; repaint(); });
        onAtt->sendInitialUpdate();
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! isOn) paintOffOverlay (g, getLocalBounds(), powerLed.getBounds());
    }
    void paint (juce::Graphics& g) override
    {
        paintSlatePanel (g, getLocalBounds().toFloat());
        g.setColour (Theme::ink);
        g.setFont (Theme::font (13.f));
        g.drawText ("NOISE", 24, 2, 60, 25, juce::Justification::centredLeft);
        g.setFont (Theme::font (12.f));
        g.drawText ("White", 4, 28, getWidth() - 8, 23, juce::Justification::centred);
    }
    void resized() override
    {
        powerLed.setBounds (4, 3, 22, 22);
        display.setBounds (4, 52, getWidth() - 8, 142);
        filterK.setBounds (getWidth() / 2 - 26, 205, 52, 54);
        levelK.setBounds (getWidth() / 2 - 26, 260, 52, 54);
    }
private:
    NoiseDisplay display;
    Knob filterK, levelK;
    LedToggle powerLed;
    std::unique_ptr<juce::ParameterAttachment> onAtt;
    bool isOn = false;
};

// ================================================================== FILTROS 1 y 2
class FilterPanel : public juce::Component
{
public:
    FilterPanel (APVTS& a, ModBus& modBus)
        : apvts (a), bus (modBus), display (a), routing (paramRef (a, "filter_routing"), { "Serie", "Paralelo" }),
          cutoffK (a, "f1_cutoff", "CUTOFF"), resK (a, "f1_res", "RES"), driveK (a, "f1_drive", "DRIVE")
    {
        styleCombo (typeBox, { "Ladder LP", "SVF LP", "SVF HP", "SVF BP", "Comb", "Formant" });
        for (int i = 0; i < 2; ++i) leds[i].attach (a, pid ("f", i + 1, "on"));
        for (auto& l : leds) addAndMakeVisible (l);
        for (int i = 0; i < 2; ++i)
            onAtt[i] = std::make_unique<juce::ParameterAttachment> (paramRef (a, pid ("f", i + 1, "on")),
                                                                    [this, i] (float v) { fOn[i] = v > 0.5f; repaint(); });
        for (auto& o : onAtt) o->sendInitialUpdate();
        addAndMakeVisible (typeBox); addAndMakeVisible (display); addAndMakeVisible (routing);
        addAndMakeVisible (cutoffK); addAndMakeVisible (resK); addAndMakeVisible (driveK);
        select (0);
    }

    void select (int i)
    {
        selected = i;
        const int k = i + 1;
        typeAtt.reset();
        typeAtt = std::make_unique<ComboAtt> (apvts, pid ("f", k, "type"), typeBox);
        cutoffK.attach (pid ("f", k, "cutoff"));
        resK.attach (pid ("f", k, "res"));
        driveK.attach (pid ("f", k, "drive"));
        const int base = Dst::fBase + i * Dst::fPer;
        cutoffK.setModTarget (base + Dst::fCutoff, &bus);
        resK.setModTarget (base + Dst::fRes, &bus);
        driveK.setModTarget (base + Dst::fDrive, &bus);
        display.setIndex (i);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (Theme::slateBody); g.fillRoundedRectangle (b, 3.f);
        for (int i = 0; i < 2; ++i)
        {
            auto h = headerRect (i).toFloat();
            g.setColour (i == selected ? Theme::slateHead.brighter (0.25f) : Theme::slateHead.darker (0.15f));
            g.fillRect (h);
            g.setColour (Theme::ink);
            g.setFont (Theme::font (12.f));
            g.drawText ("FILTER " + juce::String (i + 1), h.withTrimmedLeft (26.f), juce::Justification::centredLeft);
            if (i == selected) { g.setColour (Theme::accent); g.fillRect (h.removeFromBottom (2.f)); }
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (fOn[selected]) return;
        juce::Graphics::ScopedSaveState save (g);
        g.excludeClipRegion (juce::Rectangle<int> (0, 0, getWidth(), 27));      // las cabeceras (con sus LED) no se atenuan
        g.setColour (juce::Colour (0xb0090d11));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.f);
    }

    void resized() override
    {
        for (int i = 0; i < 2; ++i) leds[i].setBounds (headerRect (i).getX() + 4, 2, 20, 25);
        typeBox.setBounds (4, 30, getWidth() - 8, 22);
        display.setBounds (4, 56, getWidth() - 8, 122);
        routing.setBounds (4, 180, getWidth() - 8, 18);
        const int cx[3] = { 36, 101, 166 };
        cutoffK.setBounds (cx[0] - 26, 221, 52, 54);
        resK.setBounds (cx[1] - 26, 221, 52, 54);
        driveK.setBounds (cx[2] - 26, 221, 52, 54);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 2; ++i)
            if (headerRect (i).contains (e.getPosition()) && e.x > headerRect (i).getX() + 26) { select (i); return; }
    }

private:
    juce::Rectangle<int> headerRect (int i) const { return { i * getWidth() / 2, 0, getWidth() / 2, 27 }; }

    APVTS& apvts;
    ModBus& bus;
    int selected = 0;
    bool fOn[2] = { false, false };
    std::unique_ptr<juce::ParameterAttachment> onAtt[2];
    LedToggle leds[2];
    juce::ComboBox typeBox;
    std::unique_ptr<ComboAtt> typeAtt;
    FilterDisplay display;
    SegSelector routing;
    Knob cutoffK, resK, driveK;
};

// ================================================================== MACROS
class MacroPanel : public juce::Component
{
public:
    explicit MacroPanel (APVTS& a)
    {
        for (int i = 0; i < kNumMacro; ++i)
        {
            knobs.push_back (std::make_unique<Knob> (a, "macro" + juce::String (i + 1), "MACRO " + juce::String (i + 1),
                                                     Theme::accent, Theme::text));
            knobs.back()->setDragSource (Src::macro + i);   // se arrastra desde el nombre
            addAndMakeVisible (*knobs.back());
        }
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::darkPanel); g.fillRect (getLocalBounds());
        g.setColour (Theme::darkPanel2); g.fillRect (0, 0, getWidth(), 26);
        g.setColour (juce::Colours::white);
        g.setFont (Theme::font (13.f));
        g.drawText ("MACROS", getLocalBounds().removeFromTop (26), juce::Justification::centred);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (8.5f, false));
        g.drawText (tr ("arrastra el nombre a un knob", "drag the name onto a knob"), 0, getHeight() - 14, getWidth(), 12, juce::Justification::centred);
    }
    void resized() override
    {
        for (int i = 0; i < kNumMacro; ++i)
            knobs[(size_t) i]->setBounds ((i % 2) * 81 + 10, 28 + (i / 2) * 58, 62, 56);
    }
private:
    std::vector<std::unique_ptr<Knob>> knobs;
};

// ================================================================== ENVOLVENTE (ENV 1-4)
class EnvSection : public juce::Component
{
public:
    explicit EnvSection (APVTS& a)
        : display (a), atkK (a, "env1_a", "ATK", Theme::accent, Theme::text), decK (a, "env1_d", "DEC", Theme::accent, Theme::text),
          susK (a, "env1_s", "SUS", Theme::accent, Theme::text), relK (a, "env1_r", "REL", Theme::accent, Theme::text),
          tenK (a, "env1_tension", "CURVE", Theme::accent, Theme::text)
    {
        addAndMakeVisible (display);
        for (Knob* k : { &atkK, &decK, &susK, &relK, &tenK }) addAndMakeVisible (k);
    }
    void select (int i)
    {
        const int k = i + 1;
        display.setIndex (i);
        atkK.attach (pid ("env", k, "a"));  decK.attach (pid ("env", k, "d")); susK.attach (pid ("env", k, "s"));
        relK.attach (pid ("env", k, "r"));  tenK.attach (pid ("env", k, "tension"));
    }
    void paint (juce::Graphics& g) override { g.setColour (Theme::darkPanel2); g.fillRect (getLocalBounds()); }
    void resized() override
    {
        display.setBounds (2, 2, getWidth() - 4, 138);
        const int w = getWidth() / 5;
        Knob* ks[5] = { &atkK, &decK, &susK, &relK, &tenK };
        for (int i = 0; i < 5; ++i) ks[i]->setBounds (i * w + (w - 52) / 2, 168, 52, 54);
    }
private:
    EnvDisplay display;
    Knob atkK, decK, susK, relK, tenK;
};

// ================================================================== LFO (1-10)
class LfoSection : public juce::Component
{
public:
    explicit LfoSection (APVTS& a)
        : apvts (a), display (a), rateK (a, "lfo1_rate", "RATE", Theme::accent, Theme::text),
          retrig ("RETRIG"), bipolar ("BIPOLAR"), sync ("SYNC / HOST")
    {
        styleCombo (shapeBox, { "Seno", "Triangulo", "Sierra", "Cuadrada", "Sample&Hold", "Caos Logistico", "Caos Lorenz", "Caos Rossler" });
        styleCombo (divBox, { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" });
        for (juce::Component* c : { (juce::Component*) &display, (juce::Component*) &rateK, (juce::Component*) &retrig,
                                    (juce::Component*) &bipolar, (juce::Component*) &sync, (juce::Component*) &shapeBox,
                                    (juce::Component*) &divBox })
            addAndMakeVisible (c);
    }
    void select (int i)
    {
        const int k = i + 1;
        display.setIndex (i);
        rateK.attach (pid ("lfo", k, "rate"));
        retrig.attach (apvts, pid ("lfo", k, "retrig"));
        bipolar.attach (apvts, pid ("lfo", k, "bipolar"));
        sync.attach (apvts, pid ("lfo", k, "sync"));
        shapeAtt.reset(); divAtt.reset();
        shapeAtt = std::make_unique<ComboAtt> (apvts, pid ("lfo", k, "shape"), shapeBox);
        divAtt   = std::make_unique<ComboAtt> (apvts, pid ("lfo", k, "div"), divBox);
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::darkPanel2); g.fillRect (getLocalBounds());
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (9.f));
        g.drawText ("SHAPE", 6, 6, 90, 12, juce::Justification::centredLeft);
        g.drawText ("DIVISION", 190, 150, 70, 12, juce::Justification::centredLeft);
    }
    void resized() override
    {
        shapeBox.setBounds (4, 20, 98, 22);
        retrig.setBounds (6, 56, 96, 22);
        bipolar.setBounds (6, 86, 96, 22);
        sync.setBounds (6, 116, 100, 22);
        display.setBounds (110, 2, getWidth() - 112, 138);
        rateK.setBounds (110, 168, 52, 54);
        divBox.setBounds (190, 166, 90, 24);
    }
private:
    APVTS& apvts;
    LfoDisplay display;
    Knob rateK;
    LedToggle retrig, bipolar, sync;
    juce::ComboBox shapeBox, divBox;
    std::unique_ptr<ComboAtt> shapeAtt, divAtt;
};

// ================================================================== MATRIZ DE MODULACIÓN (pestaña MATRIX)
class MatrixPage : public juce::Component
{
public:
    explicit MatrixPage (APVTS& a)
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto& r = rows[(size_t) i];
            const int k = i + 1;
            styleCombo (r.src, sourceNames());
            styleCombo (r.dst, destNames());
            r.amount.setSliderStyle (juce::Slider::LinearHorizontal);
            r.amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
            r.amount.setColour (juce::Slider::textBoxTextColourId, Theme::text);
            r.invert.attach (a, pid ("mod", k, "invert"));
            r.srcA = std::make_unique<ComboAtt> (a, pid ("mod", k, "src"), r.src);
            r.dstA = std::make_unique<ComboAtt> (a, pid ("mod", k, "dst"), r.dst);
            r.amtA = std::make_unique<SliderAtt> (a, pid ("mod", k, "amount"), r.amount);
            r.amount.setDoubleClickReturnValue (true, 0.0);
            for (juce::Component* c : { (juce::Component*) &r.src, (juce::Component*) &r.dst,
                                        (juce::Component*) &r.amount, (juce::Component*) &r.invert })
                addAndMakeVisible (c);
        }
    }
    void paint (juce::Graphics& g) override
    {
        g.setColour (Theme::darkPanel); g.fillRect (getLocalBounds());
        g.setColour (Theme::text);
        g.setFont (Theme::font (11.f));
        g.drawText ("#", 12, 6, 30, 22, juce::Justification::centredLeft);
        g.drawText (tr ("FUENTE", "SOURCE"), 50, 6, 260, 22, juce::Justification::centredLeft);
        g.drawText (tr ("DESTINO", "DESTINATION"), 330, 6, 300, 22, juce::Justification::centredLeft);
        g.drawText (tr ("CANTIDAD", "AMOUNT"), 650, 6, 380, 22, juce::Justification::centredLeft);
        g.drawText (tr ("INVERTIR", "INVERT"), 1056, 6, 100, 22, juce::Justification::centredLeft);
        for (int i = 0; i < kNumSlots; ++i)
        {
            g.setColour (Theme::textDim);
            g.drawText (juce::String (i + 1), 12, 34 + i * 33, 30, 28, juce::Justification::centredLeft);
        }
    }
    void resized() override
    {
        for (int i = 0; i < kNumSlots; ++i)
        {
            auto& r = rows[(size_t) i];
            const int y = 34 + i * 33;
            r.src.setBounds (50, y + 3, 260, 24);
            r.dst.setBounds (330, y + 3, 300, 24);
            r.amount.setBounds (650, y + 3, 380, 24);
            r.invert.setBounds (1056, y + 3, 90, 24);
        }
    }
private:
    struct Row
    {
        juce::ComboBox src, dst;
        juce::Slider amount;
        LedToggle invert;
        std::unique_ptr<ComboAtt> srcA, dstA;
        std::unique_ptr<SliderAtt> amtA;
    };
    std::array<Row, kNumSlots> rows;
};
}
