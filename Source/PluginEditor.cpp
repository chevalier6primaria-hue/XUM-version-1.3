#include "PluginEditor.h"

namespace
{
constexpr int kBaseW = 1190, kBaseH = 740;
}

// ---------------------------------------------------------------- partes estáticas del lienzo
void XumEditor::Canvas::paint (juce::Graphics& g)
{
    using namespace xum;
    g.fillAll (Theme::bg);

    // logotipo
    g.setColour (Theme::topBar);
    g.fillRoundedRectangle (2.f, 2.f, 162.f, 64.f, 4.f);
    g.setColour (juce::Colours::white);
    g.setFont (Theme::font (34.f));
    g.drawText ("X  U  M", 14, 4, 148, 40, juce::Justification::centredLeft);
    g.setColour (Theme::accent);
    g.fillRect (18.f, 46.f, 128.f, 2.f);
    g.setColour (Theme::text);
    g.setFont (Theme::font (10.f));
    g.drawText (tr ("por ", "by ") + juce::String (kAuthorText).toUpperCase(),
                14, 49, 148, 14, juce::Justification::centredLeft);

    // barra de preset
    g.setColour (Theme::topBar);
    g.fillRoundedRectangle (508.f, 2.f, 578.f, 64.f, 4.f);
    g.setColour (Theme::accent);
    g.setFont (Theme::font (17.f));
    g.drawText ("XUM  -  Init", 526, 6, 400, 28, juce::Justification::centredLeft);
    if (gUserName.isNotEmpty())
    {
        g.setColour (Theme::text);
        g.setFont (Theme::font (13.f, false));
        g.drawText (tr ("Hola, ", "Hello, ") + gUserName, 830, 8, 242, 24, juce::Justification::centredRight);
    }
    g.setFont (Theme::font (11.f));
    g.setColour (Theme::textDim);
    g.drawText (tr ("ARTISTA:", "ARTIST:"), 526, 40, 60, 20, juce::Justification::centredLeft);
    g.drawText ("DESC:", 760, 40, 40, 20, juce::Justification::centredLeft);
    g.setColour (Theme::text);
    g.setFont (Theme::font (13.f, false));
    g.drawText (kAuthorText, 590, 40, 160, 20, juce::Justification::centredLeft);
    g.drawText (tr ("Sintetizador h\xC3\xAD" "brido", "Hybrid synthesizer"), 802, 40, 270, 20, juce::Justification::centredLeft);

    // panel del master
    g.setColour (Theme::topBar);
    g.fillRoundedRectangle (1090.f, 2.f, 98.f, 64.f, 4.f);

    // fila de modulación: fondo de las pestañas
    g.setColour (Theme::darkPanel);
    g.fillRect (166, 392, 1022, 26);

    // voicing (informativo)
    g.setColour (Theme::darkPanel);
    g.fillRect (1042, 562, 146, 90);
    g.setColour (Theme::text);
    g.setFont (Theme::font (12.f));
    g.drawText ("VOICING", 1054, 568, 120, 18, juce::Justification::centredLeft);
    g.setColour (Theme::line);
    g.drawHorizontalLine (588, 1054.f, 1176.f);
    g.setColour (Theme::textDim);
    g.setFont (Theme::font (11.f));
    g.drawText ("POLY", 1054, 596, 60, 18, juce::Justification::centredLeft);
    g.setColour (Theme::text);
    g.drawText (juce::String (kMaxVoices), 1120, 596, 56, 18, juce::Justification::centredRight);
    g.setColour (Theme::textDim);
    g.drawText ("MONO / LEGATO", 1054, 620, 122, 18, juce::Justification::centredLeft);
    g.setFont (Theme::font (9.f));
    g.drawText (tr ("PR\xC3\x93XIMAMENTE", "COMING SOON"), 1054, 636, 122, 12, juce::Justification::centredLeft);

    // créditos
    g.setColour (Theme::darkPanel);
    g.fillRect (1042, 656, 146, 82);
    g.setColour (juce::Colours::white);
    g.setFont (Theme::font (24.f));
    g.drawText ("XUM", 1042, 662, 146, 30, juce::Justification::centred);
    g.setColour (Theme::accent);
    g.setFont (Theme::font (12.f));
    g.drawText (tr ("SINTETIZADOR", "SYNTHESIZER"), 1042, 692, 146, 16, juce::Justification::centred);
    g.setColour (Theme::text);
    g.setFont (Theme::font (10.f));
    g.drawText (tr ("POR ", "BY ") + juce::String (kAuthorText).toUpperCase(), 1042, 710, 146, 16, juce::Justification::centred);
}

// ---------------------------------------------------------------- editor
XumEditor::XumEditor (XumAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      mainKnob (p.apvts, "master_gain", "MAIN", xum::Theme::accent, xum::Theme::text),
      sub (p.apvts, p.modBus),
      oscA (p.apvts, 0, "OSC A", p.modBus), oscB (p.apvts, 1, "OSC B", p.modBus), oscC (p.apvts, 2, "OSC C", p.modBus),
      noise (p.apvts, p.modBus), filters (p.apvts, p.modBus), macros (p.apvts), envSec (p.apvts), lfoSec (p.apvts),
      matrix (p.apvts), scope (p.scope),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    using namespace xum;
    settings = loadSettings();                 // idioma, nombre y tamano guardados
    gLanguage = settings.language;
    gUserName = settings.userName;
    setLookAndFeel (&laf);
    addAndMakeVisible (content);

    // el logo abre el menu de tamano / proporcion / idioma / eco
    logoBtn.setBounds (2, 2, 162, 64);
    logoBtn.onClick = [this] { showSizeMenu(); };
    content.addAndMakeVisible (logoBtn);

    // ---- fila superior
    content.addAndMakeVisible (mainKnob);
    mainKnob.setBounds (1112, 6, 54, 58);

    const char* pageNames[5] = { "OSC", "MIX", "FX", "MATRIX", "GLOBAL" };
    for (int i = 0; i < 5; ++i)
    {
        auto t = std::make_unique<TabButton> (pageNames[i]);
        t->setLedStyle (true);
        t->setBounds (166 + i * 68, 2, 66, 64);
        if (i == 0 || i == 2 || i == 3) t->onClick = [this, i] { showPage (i); };
        else                  t->setAlpha (0.4f);          // secciones aún no disponibles
        content.addAndMakeVisible (*t);
        pageTabs.push_back (std::move (t));
    }

    // ---- fila de osciladores
    sub.setBounds (2, 68, 73, 320);
    oscA.setBounds (76, 68, 268, 320);
    oscB.setBounds (346, 68, 268, 320);
    oscC.setBounds (616, 68, 268, 320);
    noise.setBounds (886, 68, 98, 320);
    filters.setBounds (986, 68, 202, 320);
    for (juce::Component* c : { (juce::Component*) &sub, (juce::Component*) &oscA, (juce::Component*) &oscB,
                                (juce::Component*) &oscC, (juce::Component*) &noise, (juce::Component*) &filters })
        content.addAndMakeVisible (c);

    // ---- fila de modulación
    macros.setBounds (2, 392, 162, 260);
    envSec.setBounds (166, 420, 350, 232);
    lfoSec.setBounds (518, 420, 522, 232);
    mapDisp.setBounds (1042, 420, 146, 140);
    for (juce::Component* c : { (juce::Component*) &macros, (juce::Component*) &envSec,
                                (juce::Component*) &lfoSec, (juce::Component*) &mapDisp })
        content.addAndMakeVisible (c);

    auto addTab = [this] (std::vector<std::unique_ptr<TabButton>>& list, const juce::String& text,
                          int x, int w, std::function<void()> onClick)
    {
        auto t = std::make_unique<TabButton> (text);
        t->setBounds (x, 392, w, 26);
        t->onClick = std::move (onClick);
        content.addAndMakeVisible (*t);
        list.push_back (std::move (t));
    };
    for (int i = 0; i < kNumEnv; ++i)
        addTab (envTabs, "ENV " + juce::String (i + 1), 166 + i * 88, 88, [this, i] { selectEnv (i); });
    for (int i = 0; i < kNumLfo; ++i)
        addTab (lfoTabs, juce::String (i + 1), 518 + i * 52, 52, [this, i] { selectLfo (i); });
    addTab (mapTabs, "VELO", 1040, 72,  [this] { selectMap (0); });
    addTab (mapTabs, "NOTE", 1112, 76,  [this] { selectMap (1); });

    // todas las fuentes se pueden arrastrar sobre un knob para modular
    for (int i = 0; i < kNumEnv; ++i) envTabs[(size_t) i]->setDragSource (Src::env + i);
    for (int i = 0; i < kNumLfo; ++i) lfoTabs[(size_t) i]->setDragSource (Src::lfo + i);
    mapTabs[0]->setDragSource (Src::velocity);
    mapTabs[1]->setDragSource (Src::key);

    // ---- matriz (pestaña MATRIX)
    matrix.setBounds (2, 68, 1186, 584);
    content.addAndMakeVisible (matrix);

    // ---- fila inferior: osciloscopio + piano
    scope.setBounds (2, 656, 328, 82);
    content.addAndMakeVisible (scope);

    keyboard.setAvailableRange (36, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setKeyWidth (704.f / 36.f);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffcfe0ec));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff10161c));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff7d93a1));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Theme::accent.withAlpha (0.18f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Theme::accent.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colours::transparentBlack);
    keyboard.setBounds (334, 656, 704, 82);
    content.addAndMakeVisible (keyboard);

    selectEnv (0);
    selectLfo (0);
    selectMap (0);
    showPage (0);

    // ---- ventana redimensionable con proporción fija
    constrainer.setSizeLimits (595, 370, 2380, 1480);
    setConstrainer (&constrainer);
    setResizable (true, true);
    applySize();

    // pantalla de bienvenida: solo la primera vez
    if (! settings.welcomed)
    {
        welcome = std::make_unique<WelcomeScreen>();
        welcome->setBounds (0, 0, kBaseW, kBaseH);
        welcome->onFinished = [this] (const juce::String& name, int lang)
        {
            settings.welcomed = true;
            settings.userName = name;
            settings.language = lang;
            xum::gUserName = name;
            xum::gLanguage = lang;
            saveUserSettings();
            content.repaint();
        };
        content.addAndMakeVisible (*welcome);
        welcome->toFront (false);
        juce::Component::SafePointer<WelcomeScreen> w (welcome.get());
        juce::MessageManager::callAsync ([w] { if (w != nullptr) w->focusName(); });
    }
    startTimerHz (20);
}

XumEditor::~XumEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void XumEditor::timerCallback()
{
    using namespace xum;
    for (int i = 0; i < kNumEnv; ++i)
        envTabs[(size_t) i]->setLive (proc.modBus.src[Src::env + i].load());
    for (int i = 0; i < kNumLfo; ++i)
    {
        const float v = proc.modBus.src[Src::lfo + i].load();
        const bool bipolar = proc.apvts.getRawParameterValue (pid ("lfo", i + 1, "bipolar"))->load() > 0.5f;
        lfoTabs[(size_t) i]->setLive (bipolar ? 0.5f + 0.5f * v : v);
    }
    mapTabs[0]->setLive (proc.modBus.src[Src::velocity].load());
    mapTabs[1]->setLive (proc.modBus.src[Src::key].load());
}

bool XumEditor::ecoOn() const
{
    return proc.apvts.getRawParameterValue ("eco_mode")->load() > 0.5f;
}

void XumEditor::saveUserSettings()
{
    settings.language = xum::gLanguage;
    settings.userName = xum::gUserName;
    xum::saveSettings (settings);
}

// Proporcion (original, 16:9, 4:3 o libre) y tamano en % guardados en las preferencias
void XumEditor::applySize()
{
    const double base = (double) kBaseW / (double) kBaseH;
    double ratio = base;
    if (settings.aspect == 1) ratio = 16.0 / 9.0;
    else if (settings.aspect == 2) ratio = 4.0 / 3.0;
    else if (settings.aspect == 3) ratio = 0.0;          // 0 = sin proporcion fija
    constrainer.setFixedAspectRatio (ratio);

    const int h = (int) std::lround ((double) kBaseH * settings.sizePercent / 100.0);
    const int w = ratio > 0.0 ? (int) std::lround ((double) h * ratio)
                              : (int) std::lround ((double) kBaseW * settings.sizePercent / 100.0);
    setSize (juce::jlimit (595, 2380, w), juce::jlimit (370, 1480, h));
}

void XumEditor::showSizeMenu()
{
    using namespace xum;
    static const int pcts[6] = { 50, 75, 100, 125, 150, 200 };
    juce::PopupMenu m;
    m.addSectionHeader (tr ("Tama\xC3\xB1o de la ventana", "Window size"));
    for (int i = 0; i < 6; ++i)
        m.addItem (1 + i, juce::String (pcts[i]) + " %", true, settings.sizePercent == pcts[i]);
    m.addSeparator();
    m.addSectionHeader (tr ("Relaci\xC3\xB3n de aspecto", "Aspect ratio"));
    m.addItem (11, "Original (16:10)", true, settings.aspect == 0);
    m.addItem (12, "16:9", true, settings.aspect == 1);
    m.addItem (13, "4:3", true, settings.aspect == 2);
    m.addItem (14, tr ("Libre (estirar)", "Free (stretch)"), true, settings.aspect == 3);
    m.addSeparator();
    m.addSectionHeader (tr ("Idioma", "Language"));
    m.addItem (21, juce::String::fromUTF8 ("Espa\xC3\xB1" "ol"), true, gLanguage == 0);
    m.addItem (22, "English", true, gLanguage == 1);
    m.addSeparator();
    m.addItem (31, tr ("Modo ECO (menos CPU)", "ECO mode (less CPU)"), true, ecoOn());

    juce::Component::SafePointer<XumEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&logoBtn),
                     [safe] (int r) { if (safe != nullptr && r != 0) safe->handleMenu (r); });
}

void XumEditor::handleMenu (int r)
{
    static const int pcts[6] = { 50, 75, 100, 125, 150, 200 };
    if (r >= 1 && r <= 6)        { settings.sizePercent = pcts[r - 1]; applySize(); }
    else if (r >= 11 && r <= 14) { settings.aspect = r - 11; applySize(); }
    else if (r == 21 || r == 22) { xum::gLanguage = r - 21; content.repaint(); }
    else if (r == 31)
    {
        if (auto* p = proc.apvts.getParameter ("eco_mode"))
        {
            const bool on = p->getValue() > 0.5f;
            p->beginChangeGesture();
            p->setValueNotifyingHost (on ? 0.f : 1.f);
            p->endChangeGesture();
        }
    }
    saveUserSettings();
}

void XumEditor::showPage (int newPage)
{
    page = newPage;
    const bool osc = page == 0;
    for (juce::Component* c : { (juce::Component*) &sub, (juce::Component*) &oscA, (juce::Component*) &oscB,
                                (juce::Component*) &oscC, (juce::Component*) &noise, (juce::Component*) &filters,
                                (juce::Component*) &macros, (juce::Component*) &envSec, (juce::Component*) &lfoSec,
                                (juce::Component*) &mapDisp })
        c->setVisible (osc);
    for (auto* list : { &envTabs, &lfoTabs, &mapTabs })
        for (auto& t : *list) t->setVisible (osc);
    matrix.setVisible (page == 3);
    if (page == 2 && fxPage == nullptr)                     // el rack de efectos se crea la primera vez que se abre
    {
        fxPage = std::make_unique<xum::FxPage> (proc);
        fxPage->setBounds (2, 68, 1186, 584);
        content.addAndMakeVisible (*fxPage);
        if (welcome != nullptr) welcome->toFront (false);
    }
    if (fxPage != nullptr) fxPage->setVisible (page == 2);
    for (int i = 0; i < (int) pageTabs.size(); ++i) pageTabs[(size_t) i]->setSelected (i == page);
    content.repaint();
}

void XumEditor::selectEnv (int i)
{
    envSel = i;
    envSec.select (i);
    for (int k = 0; k < (int) envTabs.size(); ++k) envTabs[(size_t) k]->setSelected (k == i);
}

void XumEditor::selectLfo (int i)
{
    lfoSel = i;
    lfoSec.select (i);
    for (int k = 0; k < (int) lfoTabs.size(); ++k) lfoTabs[(size_t) k]->setSelected (k == i);
}

void XumEditor::selectMap (int i)
{
    mapSel = i;
    mapDisp.setTitle (i == 0 ? "VELOCITY" : "NOTE");
    for (int k = 0; k < (int) mapTabs.size(); ++k) mapTabs[(size_t) k]->setSelected (k == i);
}

void XumEditor::paint (juce::Graphics& g) { g.fillAll (xum::Theme::bg); }

void XumEditor::resized()
{
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kBaseW,
                                                        (float) getHeight() / (float) kBaseH));
}
