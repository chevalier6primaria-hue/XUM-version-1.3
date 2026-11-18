#include "Parameters.h"
#include "Fx/FxParams.h"

namespace xum
{
juce::String pid (const char* group, int oneBasedIndex, const char* name)
{
    return juce::String (group) + juce::String (oneBasedIndex) + "_" + name;
}

juce::StringArray sourceNames()
{
    juce::StringArray s { "None" };
    for (int i = 1; i <= kNumLfo; ++i)   s.add ("LFO " + juce::String (i));
    for (int i = 1; i <= kNumEnv; ++i)   s.add ("ENV " + juce::String (i));
    for (int i = 1; i <= kNumMacro; ++i) s.add ("Macro " + juce::String (i));
    s.add ("Velocity");
    s.add ("Key");
    jassert (s.size() == Src::count);
    return s;
}

juce::StringArray destNames()
{
    juce::StringArray s { "None" };
    for (int o = 1; o <= kNumOsc; ++o)
        for (auto n : { "Level", "Pitch", "Pos", "Warp 1", "Warp 2", "Detune" })
            s.add ("Osc" + juce::String (o) + " " + n);
    s.add ("Sub Level");
    s.add ("Noise Level");
    for (int f = 1; f <= 2; ++f)
        for (auto n : { "Cutoff", "Res", "Drive" })
            s.add ("F" + juce::String (f) + " " + n);
    jassert (s.size() == Dst::count);
    return s;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto F = [&] (const String& id, const String& name, float lo, float hi, float def, float skew = 1.f)
    { layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.f, skew), def)); };
    auto I = [&] (const String& id, const String& name, int lo, int hi, int def)
    { layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto B = [&] (const String& id, const String& name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto C = [&] (const String& id, const String& name, const StringArray& choices, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def)); };

    // ---- global
    F ("master_gain", "Master", 0.f, 1.f, 0.5f);
    B ("eco_mode", "Eco Mode", true);          // menos CPU: 8 voces, unison <= 8, granos <= 96
    C ("filter_routing", "Filter Routing", { "Serie", "Paralelo" }, 0);
    for (int i = 1; i <= kNumMacro; ++i)
        F ("macro" + String (i), "Macro " + String (i), 0.f, 1.f, 0.f);

    // ---- osciladores principales
    const StringArray warpModes { "Off", "Bend +", "Bend -", "Sync", "Mirror", "FM", "Ring", "Fold", "Drive", "Quant" };
    for (int i = 1; i <= kNumOsc; ++i)
    {
        const String n = "Osc" + String (i) + " ";
        B (pid ("osc", i, "on"),     n + "On", false);                       // apagado al iniciar
        C (pid ("osc", i, "wave"),   n + "Wave", { "Basic Morph", "Sine", "Triangle", "Saw", "Square PWM", "Formant" }, 0);
        C (pid ("osc", i, "type"),   n + "Type", { "Wavetable", "Multisample", "Sample", "Granular", "Spectral" }, 0);
        F (pid ("osc", i, "level"),  n + "Level",  0.f, 1.f, i == 1 ? 0.8f : 0.f);
        F (pid ("osc", i, "pan"),    n + "Pan",   -1.f, 1.f, 0.f);
        I (pid ("osc", i, "octave"), n + "Octave", -3, 3, 0);
        I (pid ("osc", i, "semi"),   n + "Semi",  -12, 12, 0);
        I (pid ("osc", i, "fine"),   n + "Fine (cents)", -100, 100, 0);
        I (pid ("osc", i, "voices"), n + "Unison Voices", 1, kMaxUnison, i == 1 ? 3 : 1);
        F (pid ("osc", i, "detune"), n + "Detune", 0.f, 1.f, 0.25f);
        F (pid ("osc", i, "blend"),  n + "Blend",  0.f, 1.f, 0.75f);
        F (pid ("osc", i, "spread"), n + "Spread", 0.f, 1.f, 0.5f);
        F (pid ("osc", i, "pos"),    n + "WT Pos", 0.f, 1.f, 0.f);
        F (pid ("osc", i, "warp1"),  n + "Warp 1", 0.f, 1.f, 0.f);
        F (pid ("osc", i, "warp2"),  n + "Warp 2", 0.f, 1.f, 0.f);
        C (pid ("osc", i, "warp1_mode"), n + "Warp 1 Mode", warpModes, 1);
        C (pid ("osc", i, "warp2_mode"), n + "Warp 2 Mode", warpModes, 3);
        F (pid ("osc", i, "grain_size"),    n + "Grain Size (ms)", 5.f, 500.f, 80.f, 0.4f);
        I (pid ("osc", i, "grain_density"), n + "Grain Density", 1, kMaxGrains, 32);
        F (pid ("osc", i, "grain_pos"),     n + "Grain Pos", 0.f, 1.f, 0.f);
        F (pid ("osc", i, "grain_rand"),    n + "Grain Random", 0.f, 1.f, 0.1f);
    }

    // ---- sub y ruido
    B ("sub_on", "Sub On", false);
    B ("noise_on", "Noise On", false);
    C ("sub_wave", "Sub Wave", { "Seno", "Cuadrada", "Triangulo", "Sierra" }, 0);
    I ("sub_octave", "Sub Octave", -3, 0, -1);
    F ("sub_sat",    "Sub Saturation", 0.f, 1.f, 0.f);
    F ("sub_level",  "Sub Level",      0.f, 1.f, 0.f);
    F ("noise_level",  "Noise Level",  0.f, 1.f, 0.f);
    F ("noise_cutoff", "Noise Cutoff", 20.f, 20000.f, 12000.f, 0.3f);
    F ("noise_a", "Noise Attack",  0.0005f, 10.f, 0.001f, 0.3f);
    F ("noise_d", "Noise Decay",   0.0005f, 10.f, 0.3f,   0.3f);
    F ("noise_s", "Noise Sustain", 0.f, 1.f, 1.f);
    F ("noise_r", "Noise Release", 0.0005f, 10.f, 0.2f,   0.3f);

    // ---- filtros
    const StringArray filterTypes { "Ladder LP", "SVF LP", "SVF HP", "SVF BP", "Comb", "Formant" };
    for (int i = 1; i <= 2; ++i)
    {
        const String n = "F" + String (i) + " ";
        B (pid ("f", i, "on"),     n + "On", false);
        C (pid ("f", i, "type"),   n + "Type", filterTypes, i == 1 ? 0 : 2);
        F (pid ("f", i, "cutoff"), n + "Cutoff", 20.f, 20000.f, i == 1 ? 600.f : 80.f, 0.3f);
        F (pid ("f", i, "res"),    n + "Res",   0.f, 1.f, 0.15f);
        F (pid ("f", i, "drive"),  n + "Drive", 0.f, 1.f, 0.f);
    }

    // ---- envolventes (ENV 1 = amplitud)
    for (int i = 1; i <= kNumEnv; ++i)
    {
        const String n = "Env" + String (i) + " ";
        F (pid ("env", i, "a"), n + "A", 0.0005f, 10.f, 0.003f, 0.3f);
        F (pid ("env", i, "d"), n + "D", 0.0005f, 10.f, i == 1 ? 0.4f : 0.5f, 0.3f);
        F (pid ("env", i, "s"), n + "S", 0.f, 1.f, i == 1 ? 0.7f : 0.f);
        F (pid ("env", i, "r"), n + "R", 0.0005f, 10.f, 0.3f, 0.3f);
        F (pid ("env", i, "tension"), n + "Tension", -1.f, 1.f, 0.f);
    }

    // ---- LFOs
    for (int i = 1; i <= kNumLfo; ++i)
    {
        const String n = "LFO" + String (i) + " ";
        C (pid ("lfo", i, "shape"), n + "Shape",
           { "Seno", "Triangulo", "Sierra", "Cuadrada", "Sample&Hold", "Caos Logistico", "Caos Lorenz", "Caos Rossler" }, 0);
        F (pid ("lfo", i, "rate"), n + "Rate (Hz)", 0.01f, 50.f, 2.f, 0.3f);
        B (pid ("lfo", i, "sync"), n + "Sync", false);
        C (pid ("lfo", i, "div"),  n + "Division", { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 2);
        B (pid ("lfo", i, "retrig"),  n + "Retrigger", true);
        B (pid ("lfo", i, "bipolar"), n + "Bipolar", true);
    }

    // ---- matriz de modulación (fila 1 de ejemplo: ENV 2 -> F1 Cutoff)
    const auto srcNames = sourceNames();
    const auto dstNames = destNames();
    for (int i = 1; i <= kNumSlots; ++i)
    {
        const String n = "Mod" + String (i) + " ";
        C (pid ("mod", i, "src"), n + "Source", srcNames, i == 1 ? Src::env + 1 : 0);
        C (pid ("mod", i, "dst"), n + "Dest",   dstNames, i == 1 ? Dst::fBase + Dst::fCutoff : 0);
        F (pid ("mod", i, "amount"), n + "Amount", -1.f, 1.f, i == 1 ? 0.5f : 0.f);
        B (pid ("mod", i, "invert"), n + "Invert", false);
    }
    // ---- efectos (ids y rangos definidos en Fx/FxParams.h)
    for (const auto& fp : fxParamTable())
    {
        const String id (fp.id), name (fp.name);
        switch (fp.kind)
        {
            case FxFloat: F (id, name, fp.lo, fp.hi, fp.def, fp.skew); break;
            case FxInt: I (id, name, (int) fp.lo, (int) fp.hi, (int) fp.def); break;
            case FxBool: B (id, name, fp.def > 0.5f); break;
            default: C (id, name, StringArray::fromTokens (fp.choices, "|", ""), (int) fp.def); break;
        }
    }
    return layout;
}

ParamRefs::ParamRefs (juce::AudioProcessorValueTreeState& apvts)
{
    auto get = [&] (const juce::String& id) -> AP
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    for (int i = 0; i < kNumOsc; ++i)
    {
        auto& o = osc[i]; const int k = i + 1;
        o.on = get (pid ("osc", k, "on"));         o.wave = get (pid ("osc", k, "wave"));
        o.type = get (pid ("osc", k, "type"));     o.level = get (pid ("osc", k, "level"));
        o.pan = get (pid ("osc", k, "pan"));       o.octave = get (pid ("osc", k, "octave"));
        o.semi = get (pid ("osc", k, "semi"));     o.fine = get (pid ("osc", k, "fine"));     o.voices = get (pid ("osc", k, "voices"));
        o.detune = get (pid ("osc", k, "detune")); o.blend = get (pid ("osc", k, "blend"));
        o.spread = get (pid ("osc", k, "spread")); o.pos = get (pid ("osc", k, "pos"));
        o.warp1 = get (pid ("osc", k, "warp1"));   o.warp2 = get (pid ("osc", k, "warp2"));
        o.grainSize = get (pid ("osc", k, "grain_size"));
        o.grainDensity = get (pid ("osc", k, "grain_density"));
        o.grainPos = get (pid ("osc", k, "grain_pos"));
        o.grainRand = get (pid ("osc", k, "grain_rand"));
        o.warp1Mode = get (pid ("osc", k, "warp1_mode"));
        o.warp2Mode = get (pid ("osc", k, "warp2_mode"));
    }
    for (int i = 0; i < 2; ++i)
    {
        auto& f = filter[i]; const int k = i + 1;
        f.on = get (pid ("f", k, "on"));         f.type = get (pid ("f", k, "type"));
        f.cutoff = get (pid ("f", k, "cutoff")); f.res = get (pid ("f", k, "res"));
        f.drive = get (pid ("f", k, "drive"));
    }
    for (int i = 0; i < kNumEnv; ++i)
    {
        auto& e = env[i]; const int k = i + 1;
        e.a = get (pid ("env", k, "a")); e.d = get (pid ("env", k, "d")); e.s = get (pid ("env", k, "s"));
        e.r = get (pid ("env", k, "r")); e.tension = get (pid ("env", k, "tension"));
    }
    for (int i = 0; i < kNumLfo; ++i)
    {
        auto& l = lfo[i]; const int k = i + 1;
        l.shape = get (pid ("lfo", k, "shape")); l.rate = get (pid ("lfo", k, "rate"));
        l.sync = get (pid ("lfo", k, "sync"));   l.div = get (pid ("lfo", k, "div"));
        l.retrig = get (pid ("lfo", k, "retrig")); l.bipolar = get (pid ("lfo", k, "bipolar"));
    }
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto& s = slot[i]; const int k = i + 1;
        s.src = get (pid ("mod", k, "src")); s.dst = get (pid ("mod", k, "dst"));
        s.amount = get (pid ("mod", k, "amount")); s.invert = get (pid ("mod", k, "invert"));
    }
    for (int i = 0; i < kNumMacro; ++i) macro[i] = get ("macro" + juce::String (i + 1));

    subOn = get ("sub_on"); noiseOn = get ("noise_on");
    subWave = get ("sub_wave"); subOctave = get ("sub_octave"); subSat = get ("sub_sat"); subLevel = get ("sub_level");
    noiseLevel = get ("noise_level"); noiseCutoff = get ("noise_cutoff");
    noiseEnv.a = get ("noise_a"); noiseEnv.d = get ("noise_d"); noiseEnv.s = get ("noise_s"); noiseEnv.r = get ("noise_r");
    filterRouting = get ("filter_routing"); masterGain = get ("master_gain"); ecoMode = get ("eco_mode");
}
}
