#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

namespace xum
{
// ------------------------------------------------------------------ constantes
constexpr int kNumOsc    = 3;
constexpr int kNumLfo    = 10;
constexpr int kNumEnv    = 4;
constexpr int kNumMacro  = 8;
constexpr int kNumSlots  = 16;   // filas de la matriz de modulación
constexpr int kMaxUnison = 16;
constexpr int kMaxVoices = 16;   // polifonía
constexpr int kMaxGrains = 256;

enum class OscType    { Wavetable = 0, Multisample, Sample, Granular, Spectral, Count };
// Modos de warp (se pueden elegir dos por oscilador)
enum class WarpMode { Off = 0, BendPlus, BendMinus, Sync, Mirror, FM, Ring, Fold, Drive, Quantize, Count };

enum class FilterType { LadderLP = 0, SvfLP, SvfHP, SvfBP, Comb, Formant, Count };

// Fuentes de modulación (índices en el array de fuentes y en el combo de la matriz)
namespace Src
{
    constexpr int none     = 0;
    constexpr int lfo      = 1;                   // 10 LFOs
    constexpr int env      = lfo + kNumLfo;       // 4 envolventes (ENV 1 = amplitud)
    constexpr int macro    = env + kNumEnv;       // 8 macros
    constexpr int velocity = macro + kNumMacro;
    constexpr int key      = velocity + 1;
    constexpr int count    = key + 1;
}

// Destinos de modulación
namespace Dst
{
    constexpr int none = 0;
    constexpr int oscBase = 1;
    constexpr int perOsc  = 6;
    constexpr int oLevel = 0, oPitch = 1, oPos = 2, oWarp1 = 3, oWarp2 = 4, oDetune = 5;
    constexpr int subLevel   = oscBase + kNumOsc * perOsc;
    constexpr int noiseLevel = subLevel + 1;
    constexpr int fBase = noiseLevel + 1;
    constexpr int fPer  = 3;
    constexpr int fCutoff = 0, fRes = 1, fDrive = 2;
    constexpr int count = fBase + 2 * fPer;
}

// ------------------------------------------------------------------ referencias atómicas
using AP = std::atomic<float>*;

struct OscRefs    { AP on{}, wave{}, type{}, level{}, pan{}, octave{}, semi{}, fine{}, voices{}, detune{}, blend{}, spread{},
                       pos{}, warp1{}, warp2{}, warp1Mode{}, warp2Mode{},
                       grainSize{}, grainDensity{}, grainPos{}, grainRand{}; };
struct FilterRefs { AP on{}, type{}, cutoff{}, res{}, drive{}; };
struct EnvRefs    { AP a{}, d{}, s{}, r{}, tension{}; };
struct LfoRefs    { AP shape{}, rate{}, sync{}, div{}, retrig{}, bipolar{}; };
struct SlotRefs   { AP src{}, dst{}, amount{}, invert{}; };

struct ParamRefs
{
    ParamRefs() = default;
    explicit ParamRefs (juce::AudioProcessorValueTreeState& apvts);

    OscRefs    osc[kNumOsc];
    FilterRefs filter[2];
    EnvRefs    env[kNumEnv];
    LfoRefs    lfo[kNumLfo];
    SlotRefs   slot[kNumSlots];
    AP         macro[kNumMacro] {};
    AP subOn{}, subWave{}, subOctave{}, subSat{}, subLevel{};
    AP noiseOn{}, noiseLevel{}, noiseCutoff{};
    EnvRefs noiseEnv;
    AP filterRouting{}, masterGain{}, ecoMode{};
};

// Valores de modulación publicados por el hilo de audio para animar la GUI (anillos, pestañas).
struct ModBus
{
    ModBus() { for (auto& a : dst) a.store (0.f); for (auto& a : src) a.store (0.f); }
    std::atomic<float> dst[Dst::count];   // suma de modulación por destino (voz más reciente)
    std::atomic<float> src[Src::count];   // valor actual de cada fuente (voz más reciente)
};

juce::String pid (const char* group, int oneBasedIndex, const char* name);
juce::StringArray sourceNames();
juce::StringArray destNames();
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
