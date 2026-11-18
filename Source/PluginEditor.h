#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Panels.h"
#include "UI/Oscilloscope.h"
#include "UI/Settings.h"
#include "UI/Welcome.h"
#include "UI/FxPage.h"

// Interfaz de XUM (Tapelab). Se diseña sobre un lienzo base de 1190 x 740 y se
// escala proporcionalmente al redimensionar la ventana.
class XumEditor : public juce::AudioProcessorEditor,
                  public juce::DragAndDropContainer,      // arrastrar LFO / ENV / macros sobre los knobs
                  private juce::Timer
{
public:
    explicit XumEditor (XumAudioProcessor&);
    ~XumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Canvas : public juce::Component { void paint (juce::Graphics&) override; };

    void timerCallback() override;      // anima los anillos de las pestañas
    void showSizeMenu();               // menu del logo: tamano, proporcion, idioma, eco
    void handleMenu (int result);
    void applySize();                  // aplica proporcion + tamano guardados
    void saveUserSettings();
    bool ecoOn() const;
    void showPage (int page);
    void selectEnv (int i);
    void selectLfo (int i);
    void selectMap (int i);

    XumAudioProcessor& proc;
    xum::UserSettings settings;
    xum::XumLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    Canvas content;

    xum::Knob mainKnob;
    xum::SubPanel sub;
    xum::OscPanel oscA, oscB, oscC;
    xum::NoisePanel noise;
    xum::FilterPanel filters;
    xum::MacroPanel macros;
    xum::EnvSection envSec;
    xum::LfoSection lfoSec;
    xum::MapDisplay mapDisp;
    xum::MatrixPage matrix;
    std::unique_ptr<xum::FxPage> fxPage;       // se crea al abrir la pestana FX (ahorra memoria)
    xum::Oscilloscope scope;
    xum::LogoButton logoBtn;
    std::unique_ptr<xum::WelcomeScreen> welcome;
    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<xum::TabButton>> pageTabs, envTabs, lfoTabs, mapTabs;
    int page = 0, envSel = 0, lfoSel = 0, mapSel = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XumEditor)
};
