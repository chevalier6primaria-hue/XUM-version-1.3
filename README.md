# XUM — sintetizador híbrido VST3 (by Tapelab)

Plugin VST3 (JUCE 8 + C++17) para FL Studio en Windows x64.

## Compilar con GitHub
1. Sube esta carpeta a un repositorio (rama `main`).
2. Pestaña **Actions** → "Build XUM (VST3)" (se lanza solo con cada push).
3. Al terminar, descarga el artefacto **XUM-VST3-win64** y copia `XUM.vst3`
   a `C:\Program Files\Common Files\VST3\`.
4. En FL Studio: Options → Manage plugins → *Find installed plugins*.

## Arquitectura
```
Source/
  Parameters.h/.cpp        IDs, layout (APVTS), enums de fuentes/destinos de modulación
  PluginProcessor.*        MIDI sample-accurate, asignación/robo de voces, master, scope
  PluginEditor.*           GUI: lienzo 1190x740 escalable, pestañas OSC / MATRIX, piano y osciloscopio
  UI/                      Theme, LookAndFeel, Widgets (Knob, NumBox...), Displays (wavetable 3D, filtro, ENV, LFO), Panels
  Engine/Voice.h           Flujo: 3 osc + sub + ruido -> DualFilter -> ENV1 (amp)
  Engine/Oscillators.h     IOscEngine, unísono (16), WavetableBank/Engine, motores pendientes
  Engine/Filters.h         Ladder, SVF, Comb, Formantes; DualFilter serie/paralelo
  Fx/                      FxParams (tabla de parametros), Effects (12 efectos + cadena), ConvolveFx (juce::dsp)
  Engine/SubNoise.h        Sub-oscilador (4 ondas, octava, saturación) y ruido filtrado
  Modulation/Modulation.h  Envelope (tensión), Lfo (geométrico/caos), matriz de modulación
  UI/Theme.h, LookAndFeel.h   paleta, tipografía, knobs/combos/sliders propios
  UI/Widgets.h                Knob, NumBox, LedToggle, WaveSelector, SegSelector, TabButton
  UI/Displays.h               visores: wavetable 3D, granular, ruido, filtro, ENV, LFO
  UI/Panels.h                 SUB, OSC A/B/C, NOISE, FILTER 1/2, MACROS, ENV, LFO, MATRIX
  UI/Scope.h, Oscilloscope.h  buffer lock-free y osciloscopio en tiempo real
```

## Estado
- Hecho: wavetable con interpolación entre frames + 2 warps, unísono 16, sub, ruido con
  envolvente, filtros duales, 10 LFOs (sync al DAW, S&H, caos logístico y Lorenz),
  4 ENV con tensión, 8 macros, matriz de 16 filas (fila 1 de ejemplo: ENV2 → F1 Cutoff).
- Pendiente (ya enrutados, solo falta `render()`): Multisample/SFZ, Sample, Spectral. LFO Path, mixer, FX, presets, arpegiador.
- 1.3: rack de efectos (pestana FX, 12 modulos con presets): Hyper/Dimension, Distortion (10 tipos, filtro pre/post, asimetria),
  Bode (frequency shifter), Chorus, Flanger, Phaser (4-32 etapas), Delay (sync/ms, ping-pong, filtros, saturacion),
  Reverb (Plate, Room, Hall, Nitrous, Vintage, Basin), Convolve (6 IR internas + archivo propio), Compressor (multibanda),
  EQ de 4 bandas con grafico arrastrable y Utility. Selector de onda con flechas en los 3 osciladores (6 tablas),
  encendido/apagado de osciladores, sub, ruido y filtros (apagados al iniciar; el panel se opaca).
- 1.2: modulación arrastrando (LFO/ENV/macros/VELO/NOTE sobre cualquier knob) con anillos animados, motor Granular real,
  10 modos de warp (Bend+/-, Sync, Mirror, FM, Ring, Fold, Drive, Quant) x2 por oscilador, LFO caos Rossler,
  visor con la forma de onda real de cada oscilador.
- 1.2 (extra): pantalla de bienvenida animada (solo la primera vez: nombre + idioma ES/EN), menu al pulsar el logo
  (tamano 50-200 %, proporcion Original / 16:9 / 4:3 / Libre, idioma, modo ECO), preferencias guardadas en
  %APPDATA%\Tapelab\XUM.settings, motor optimizado (tanh rapido, cache de filtros, LFOs solo si se usan, modo ECO).
- UI: matriz de modulación editable (pestaña MATRIX), visores en vivo, 4 ENV y 10 LFOs editables.
- Pendiente de UI: pestañas MIX / FX / GLOBAL, selector de wavetables, presets.

## Interfaz
Lienzo base 1190x740, redimensionable con proporción fija. Pestañas OSC y MATRIX activas
(MIX / FX / GLOBAL atenuadas hasta que existan). ENV 1-4 y LFO 1-10 se editan con las pestañas
inferiores; VELO/NOTE y VOICING son informativos por ahora.
