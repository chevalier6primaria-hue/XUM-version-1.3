#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace xum
{
// Idioma activo de la interfaz (0 = espanol, 1 = ingles) y nombre del usuario.
inline int gLanguage = 0;
inline juce::String gUserName;

// Texto en el idioma activo. Los literales pueden llevar bytes UTF-8 escritos como \x..
inline juce::String tr (const char* es, const char* en)
{
    return juce::String::fromUTF8 (gLanguage == 0 ? es : en);
}

// Preferencias del usuario: se guardan en un archivo (Tapelab/XUM.settings) para que la
// pantalla de bienvenida solo salga la primera vez y se recuerden idioma y tamano de ventana.
struct UserSettings
{
    bool welcomed = false;
    juce::String userName;
    int language = 0;        // 0 = es, 1 = en
    int aspect = 0;          // 0 = original, 1 = 16:9, 2 = 4:3, 3 = libre
    int sizePercent = 100;
};

inline juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "XUM";
    o.folderName          = "Tapelab";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    return o;
}

inline UserSettings loadSettings()
{
    juce::PropertiesFile props (settingsOptions());
    UserSettings s;
    s.welcomed    = props.getValue ("welcomed") == "1";
    s.userName    = props.getValue ("userName");
    s.language    = props.getValue ("language", "0").getIntValue() == 1 ? 1 : 0;
    s.aspect      = std::clamp (props.getValue ("aspect", "0").getIntValue(), 0, 3);
    s.sizePercent = std::clamp (props.getValue ("sizePercent", "100").getIntValue(), 50, 200);
    return s;
}

inline void saveSettings (const UserSettings& s)
{
    juce::PropertiesFile props (settingsOptions());
    props.setValue ("welcomed", juce::String (s.welcomed ? "1" : "0"));
    props.setValue ("userName", s.userName);
    props.setValue ("language", juce::String (s.language));
    props.setValue ("aspect", juce::String (s.aspect));
    props.setValue ("sizePercent", juce::String (s.sizePercent));
    props.saveIfNeeded();
}
}
