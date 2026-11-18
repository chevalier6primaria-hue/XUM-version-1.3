#pragma once
#include "UI/Theme.h"
#include "UI/Settings.h"
#include "Engine/DspUtil.h"
#include <functional>
#include <cmath>

namespace xum
{
// Boton tipo "pastilla" (idioma, continuar...)
class PillButton : public juce::Component
{
public:
    explicit PillButton (const juce::String& t = {}) : text (t) {}
    void setText (const juce::String& t) { text = t; repaint(); }
    void setSelected (bool s) { selected = s; repaint(); }
    void setActive (bool a) { active = a; repaint(); }
    void setPrimary (bool p) { primary = p; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.f);
        const float alpha = active ? 1.f : 0.35f;
        juce::Colour fill = primary ? Theme::accent : (selected ? Theme::accent.withAlpha (0.28f) : juce::Colour (0xff131c24));
        if (hover && active) fill = fill.brighter (0.15f);
        g.setColour (fill.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour ((selected || primary ? Theme::accent : Theme::line.brighter (0.3f)).withMultipliedAlpha (alpha));
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.4f);
        g.setColour ((primary ? Theme::bg : juce::Colours::white).withMultipliedAlpha (alpha));
        g.setFont (Theme::font (13.f));
        g.drawText (text, b, juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (active && contains (e.getPosition()) && onClick) onClick();
    }

    std::function<void()> onClick;

private:
    juce::String text;
    bool selected = false, active = true, primary = false, hover = false;
};

// Logo clicable (abre el menu de tamano / proporcion / idioma / modo eco)
class LogoButton : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        if (hover)
        {
            g.setColour (Theme::accent.withAlpha (0.12f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.f);
        }
        // pequeno triangulo que indica que hay menu
        juce::Path arrow;
        const float cx = (float) getWidth() - 14.f, cy = 16.f;
        arrow.addTriangle (cx - 4.f, cy - 2.f, cx + 4.f, cy - 2.f, cx, cy + 3.f);
        g.setColour (hover ? Theme::accent : Theme::textDim);
        g.fillPath (arrow);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (contains (e.getPosition()) && onClick) onClick();
    }
    std::function<void()> onClick;

private:
    bool hover = false;
};

// Pantalla de bienvenida (solo la primera vez): fondo de osciloscopio animado, nombre del
// plugin, autor, campo para el nombre del usuario y selector de idioma.
class WelcomeScreen : public juce::Component, private juce::Timer
{
public:
    WelcomeScreen()
    {
        startTime = juce::Time::getMillisecondCounterHiRes();

        nameBox.setMultiLine (false);
        nameBox.setInputRestrictions (24);
        nameBox.setJustification (juce::Justification::centred);
        nameBox.setFont (Theme::font (20.f, false));
        nameBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b1218));
        nameBox.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        nameBox.setColour (juce::TextEditor::outlineColourId, Theme::line.brighter (0.3f));
        nameBox.setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent);
        nameBox.setColour (juce::TextEditor::highlightColourId, Theme::accent.withAlpha (0.4f));
        nameBox.onTextChange = [this] { updateState(); };
        nameBox.onReturnKey  = [this] { confirm(); };
        addAndMakeVisible (nameBox);

        esBtn.onClick = [this] { setLanguage (0); };
        enBtn.onClick = [this] { setLanguage (1); };
        esBtn.setText (juce::String::fromUTF8 ("ESPA\xC3\xB1" "OL"));
        enBtn.setText ("ENGLISH");
        okBtn.setPrimary (true);
        okBtn.onClick = [this] { confirm(); };
        addAndMakeVisible (esBtn);
        addAndMakeVisible (enBtn);
        addAndMakeVisible (okBtn);

        setLanguage (gLanguage);
        startTimerHz (30);
    }

    ~WelcomeScreen() override { stopTimer(); }

    // se llama al pulsar "Continuar": (nombre, idioma)
    std::function<void (const juce::String&, int)> onFinished;

    void focusName() { nameBox.grabKeyboardFocus(); }

    void resized() override
    {
        const int cx = getWidth() / 2;
        card = juce::Rectangle<int> (cx - 270, (getHeight() - 480) / 2, 540, 480);
        nameBox.setBounds (card.getX() + 70, card.getY() + 252, card.getWidth() - 140, 44);
        esBtn.setBounds (cx - 160, card.getY() + 340, 150, 34);
        enBtn.setBounds (cx + 10, card.getY() + 340, 150, 34);
        okBtn.setBounds (cx - 130, card.getY() + 408, 260, 46);
    }

    void paint (juce::Graphics& g) override
    {
        const float w = (float) getWidth(), h = (float) getHeight();
        const float t = (float) ((juce::Time::getMillisecondCounterHiRes() - startTime) * 0.001);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff03060a), 0.f, 0.f, juce::Colour (0xff0a1a24), 0.f, h, false));
        g.fillRect (0.f, 0.f, w, h);

        // retícula de osciloscopio
        g.setColour (juce::Colour (0x12ffffff));
        for (int i = 1; i < 20; ++i) g.drawVerticalLine ((int) (w * (float) i / 20.f), 0.f, h);
        for (int i = 1; i < 12; ++i) g.drawHorizontalLine ((int) (h * (float) i / 12.f), 0.f, w);
        g.setColour (juce::Colour (0x24ffffff));
        g.drawHorizontalLine ((int) (h * 0.5f), 0.f, w);
        g.drawVerticalLine ((int) (w * 0.5f), 0.f, h);

        drawWaves (g, w, h, t);

        // barrido (como el haz del osciloscopio)
        const float sx = std::fmod (t * 0.22f, 1.2f) * w - 0.1f * w;
        g.setGradientFill (juce::ColourGradient (Theme::accent.withAlpha (0.f), sx - 220.f, 0.f,
                                                 Theme::accent.withAlpha (0.16f), sx, 0.f, false));
        g.fillRect (sx - 220.f, 0.f, 220.f, h);
        g.setColour (Theme::accent.withAlpha (0.55f));
        g.drawVerticalLine ((int) sx, 0.f, h);

        // tarjeta central
        const auto c = card.toFloat();
        g.setColour (juce::Colour (0xe0060a0f));
        g.fillRoundedRectangle (c, 14.f);
        g.setColour (Theme::accent.withAlpha (0.45f));
        g.drawRoundedRectangle (c, 14.f, 1.5f);

        g.setColour (juce::Colours::white);
        g.setFont (Theme::font (70.f));
        g.drawText ("X  U  M", card.withTrimmedTop (22).withHeight (84), juce::Justification::centred);
        g.setColour (Theme::accent);
        g.fillRect (c.getCentreX() - 70.f, c.getY() + 112.f, 140.f, 2.f);
        g.setColour (Theme::text);
        g.setFont (Theme::font (14.f));
        g.drawText (tr ("hecho por ", "made by ") + juce::String (kAuthorText).toUpperCase(),
                    card.withTrimmedTop (122).withHeight (22), juce::Justification::centred);

        g.setColour (juce::Colours::white);
        g.setFont (Theme::font (28.f));
        g.drawText (tr ("Bienvenido", "Welcome"), card.withTrimmedTop (164).withHeight (40), juce::Justification::centred);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.f, false));
        g.drawText (tr ("Escribe tu nombre y elige el idioma para empezar", "Type your name and choose a language to start"),
                    card.withTrimmedTop (206).withHeight (24), juce::Justification::centred);

        g.setColour (Theme::text);
        g.setFont (Theme::font (11.f));
        g.drawText (tr ("IDIOMA", "LANGUAGE"), card.withTrimmedTop (316).withHeight (20), juce::Justification::centred);
    }

private:
    struct WaveDef { float y, amp, freq, speed; int kind; juce::Colour col; };

    void drawWaves (juce::Graphics& g, float w, float h, float t)
    {
        static const WaveDef defs[] = {
            { 0.16f, 0.07f, 3.0f,  1.4f, 0, juce::Colour (0xff10a8ff) },
            { 0.30f, 0.08f, 2.0f, -1.1f, 1, juce::Colour (0xff2bc9b4) },
            { 0.50f, 0.15f, 1.5f,  0.8f, 2, juce::Colour (0xff56e25c) },
            { 0.70f, 0.08f, 2.5f, -1.6f, 3, juce::Colour (0xff10a8ff) },
            { 0.84f, 0.07f, 3.5f,  1.9f, 4, juce::Colour (0xff2bc9b4) },
        };
        for (const auto& d : defs)
        {
            juce::Path p;
            const float cy = h * d.y, amp = h * d.amp;
            for (int xi = 0; xi <= (int) w; xi += 6)
            {
                const float x = (float) xi;
                const float u = x / w * d.freq * kTwoPi + t * d.speed * 3.f;
                float y = 0.f;
                switch (d.kind)
                {
                    case 0:  y = std::sin (u); break;
                    case 1:  y = 1.f - 4.f * std::abs (std::fmod (u / kTwoPi, 1.f) - 0.5f); break;
                    case 2:  for (int k = 1; k <= 6; ++k) y += std::sin ((float) k * u) / (float) k;
                             y *= 0.6f; break;
                    case 3:  y = fastTanh (3.f * std::sin (u)); break;
                    default: y = 0.6f * std::sin (u) + 0.4f * std::sin (u * 2.7f + t); break;
                }
                const float env = (0.55f + 0.45f * std::sin (kPi * x / w)) * (0.85f + 0.15f * std::sin (t * 0.9f + d.speed));
                const float yy = cy - y * amp * env;
                if (xi == 0) p.startNewSubPath (x, yy); else p.lineTo (x, yy);
            }
            g.setColour (d.col.withAlpha (0.07f)); g.strokePath (p, juce::PathStrokeType (9.f));
            g.setColour (d.col.withAlpha (0.20f)); g.strokePath (p, juce::PathStrokeType (4.f));
            g.setColour (d.col.withAlpha (0.90f)); g.strokePath (p, juce::PathStrokeType (1.4f));
        }
    }

    void setLanguage (int lang)
    {
        gLanguage = lang;
        esBtn.setSelected (lang == 0);
        enBtn.setSelected (lang == 1);
        okBtn.setText (tr ("Continuar", "Continue"));
        nameBox.setTextToShowWhenEmpty (tr ("Tu nombre", "Your name"), juce::Colour (0xff5d717d));
        updateState();
        repaint();
    }

    void updateState() { okBtn.setActive (nameBox.getText().trim().isNotEmpty()); }

    void confirm()
    {
        const auto name = nameBox.getText().trim();
        if (name.isEmpty() || fading) return;
        gUserName = name;
        fading = true;
        if (onFinished) onFinished (name, gLanguage);
    }

    void timerCallback() override
    {
        if (fading)
        {
            fade -= 0.08f;
            if (fade <= 0.f) { stopTimer(); setVisible (false); return; }
            setAlpha (fade);
        }
        repaint();
    }

    juce::TextEditor nameBox;
    PillButton esBtn, enBtn, okBtn;
    juce::Rectangle<int> card;
    double startTime = 0.0;
    bool fading = false;
    float fade = 1.f;
};
}
