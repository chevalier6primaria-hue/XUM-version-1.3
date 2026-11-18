#pragma once
#include "Parameters.h"
#include "Modulation/Modulation.h"
#include "Engine/Oscillators.h"
#include "Engine/SubNoise.h"
#include "Engine/Filters.h"

namespace xum
{
// Una voz polifónica. Flujo de señal:
//   3 osciladores (+ unísono) + sub + ruido -> DualFilter -> ENV 1 (amplitud) x velocidad
// La modulación se evalúa cada kSub muestras (control-rate) y se interpola en la amplitud.
class Voice
{
public:
    static constexpr int kSub = 32;

    Voice()
    {
        for (int i = 0; i < kNumOsc; ++i)
            for (int t = 0; t < (int) OscType::Count; ++t)
                engines[i][t] = makeEngine (t);
        for (int i = 0; i < kNumLfo; ++i) lfo[i].seed (1234u + 7919u * (uint32_t) i);
        noise.seed (99991u);
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& slot : engines) for (auto& e : slot) e->prepare (sr);
        for (auto& e : env) { e.setSampleRate (sr); e.reset(); }
        noiseEnv.setSampleRate (sr); noiseEnv.reset();
        for (auto& l : lfo) l.setSampleRate (sr);
        sub.prepare (sr); noise.prepare (sr); filters.prepare (sr);
        lastAmp = 0.f; held = false;
    }

    void noteOn (int note, float vel, const ParamRefs& p)
    {
        if (! isActive()) { filters.reset(); lastAmp = 0.f; }
        noteNumber = note; velocity = vel; held = true;
        for (auto& e : env) e.noteOn();
        noiseEnv.noteOn();
        for (int i = 0; i < kNumLfo; ++i)
            if (p.lfo[i].retrig->load() > 0.5f) lfo[i].retrigger();
        sub.noteOn();
        for (int i = 0; i < kNumOsc; ++i)
        {
            lastType[i] = std::clamp ((int) p.osc[i].type->load(), 0, (int) OscType::Count - 1);
            engines[i][lastType[i]]->noteOn();
        }
    }

    void noteOff()
    {
        held = false;
        for (auto& e : env) e.noteOff();
        noiseEnv.noteOff();
    }

    bool isActive()  const { return env[0].isActive(); }
    bool isHeld()    const { return held; }
    int  getNote()   const { return noteNumber; }
    uint64_t age = 0;       // para robo de voces
    const float* getLastMod() const { return lastMod; }
    const float* getLastSrc() const { return lastSrc; }

    // SUMA el resultado en outL/outR
    void render (float* outL, float* outR, int n, const ParamRefs& p, double bpm)
    {
        // Solo se calculan los LFOs que alguna fila de la matriz usa de verdad (ahorra CPU)
        lfoMask = 0;
        for (int k = 0; k < kNumSlots; ++k)
        {
            const int s = (int) p.slot[k].src->load();
            if (s >= Src::lfo && s < Src::lfo + kNumLfo && (int) p.slot[k].dst->load() > 0 && p.slot[k].amount->load() != 0.f)
                lfoMask |= 1u << (s - Src::lfo);
        }
        int done = 0;
        while (done < n && isActive())
        {
            const int m = std::min (kSub, n - done);
            renderSub (outL + done, outR + done, m, p, bpm);
            done += m;
        }
    }

private:
    static float c01 (float v) { return std::clamp (v, 0.f, 1.f); }

    void renderSub (float* outL, float* outR, int n, const ParamRefs& p, double bpm)
    {
        const bool eco = p.ecoMode->load() > 0.5f;

        // ---- 1) fuentes de modulación
        float src[Src::count] = {};
        for (int i = 0; i < kNumEnv; ++i)
        {
            const auto& r = p.env[i];
            env[i].set (r.a->load(), r.d->load(), r.s->load(), r.r->load(), r.tension->load());
            src[Src::env + i] = env[i].advance (n);
        }
        for (int i = 0; i < kNumLfo; ++i)
        {
            if (! (lfoMask & (1u << i))) continue;
            const auto& r = p.lfo[i];
            float rate = r.rate->load();
            if (r.sync->load() > 0.5f)
                rate = (float) (bpm / 60.0) * kCyclesPerBeat[std::clamp ((int) r.div->load(), 0, 5)];
            float v = lfo[i].advance (n, rate, (int) r.shape->load());
            if (r.bipolar->load() < 0.5f) v = 0.5f + 0.5f * v;
            src[Src::lfo + i] = v;
        }
        for (int i = 0; i < kNumMacro; ++i) src[Src::macro + i] = p.macro[i]->load();
        src[Src::velocity] = velocity;
        src[Src::key]      = (float) noteNumber / 127.f;

        float mod[Dst::count];
        applyMatrix (p, src, mod);
        std::copy (src, src + Src::count, lastSrc);      // para animar la GUI
        std::copy (mod, mod + Dst::count, lastMod);

        // ---- 2) osciladores principales
        float mixL[kSub] = {}, mixR[kSub] = {};
        const float freq = 440.f * std::exp2 (((float) noteNumber - 69.f) / 12.f);

        for (int i = 0; i < kNumOsc; ++i)
        {
            const auto& r = p.osc[i];
            if (r.on->load() < 0.5f) continue;                 // oscilador apagado
            const int base = Dst::oscBase + i * Dst::perOsc;
            const float level = c01 (r.level->load() + mod[base + Dst::oLevel]);
            if (level < 1e-4f) continue;

            const int type = std::clamp ((int) r.type->load(), 0, (int) OscType::Count - 1);
            if (type != lastType[i]) { engines[i][type]->noteOn(); lastType[i] = type; }

            OscFrame f;
            f.baseFreq   = freq;
            f.pitchSemis = r.octave->load() * 12.f + r.semi->load() + r.fine->load() * 0.01f
                         + mod[base + Dst::oPitch] * 24.f;
            f.pan        = r.pan->load();
            f.voices     = eco ? std::min ((int) r.voices->load(), 8) : (int) r.voices->load();
            f.detune     = c01 (r.detune->load() + mod[base + Dst::oDetune]);
            f.blend      = r.blend->load();
            f.spread     = r.spread->load();
            f.pos        = c01 (r.pos->load()   + mod[base + Dst::oPos]);
            f.warp1      = c01 (r.warp1->load() + mod[base + Dst::oWarp1]);
            f.warp2      = c01 (r.warp2->load() + mod[base + Dst::oWarp2]);
            f.wave       = (int) r.wave->load();
            f.warp1Mode  = (int) r.warp1Mode->load();
            f.warp2Mode  = (int) r.warp2Mode->load();
            f.grainRand  = r.grainRand->load();
            f.grainSize    = r.grainSize->load();
            f.grainDensity = eco ? std::min (r.grainDensity->load(), 96.f) : r.grainDensity->load();
            f.grainPos     = r.grainPos->load();

            float tl[kSub] = {}, tr[kSub] = {};
            engines[i][type]->render (tl, tr, n, f);
            for (int s = 0; s < n; ++s) { mixL[s] += tl[s] * level; mixR[s] += tr[s] * level; }
        }

        // ---- 3) sub-oscilador
        const float subLvl = c01 (p.subLevel->load() + mod[Dst::subLevel]);
        if (subLvl > 1e-4f && p.subOn->load() > 0.5f)
        {
            sub.setup (freq, (int) p.subOctave->load(), (int) p.subWave->load(), p.subSat->load());
            for (int s = 0; s < n; ++s) { const float y = sub.tick() * subLvl * 0.5f; mixL[s] += y; mixR[s] += y; }
        }

        // ---- 4) ruido con filtro y envolvente dedicada
        noiseEnv.set (p.noiseEnv.a->load(), p.noiseEnv.d->load(), p.noiseEnv.s->load(), p.noiseEnv.r->load(), 0.f);
        const float nLvl = (p.noiseOn->load() > 0.5f ? c01 (p.noiseLevel->load() + mod[Dst::noiseLevel]) : 0.f) * noiseEnv.advance (n);
        if (nLvl > 1e-5f)
        {
            noise.setCutoff (p.noiseCutoff->load());
            for (int s = 0; s < n; ++s) { const float y = noise.tick() * nLvl * 0.5f; mixL[s] += y; mixR[s] += y; }
        }

        // ---- 5) filtros duales
        for (int k = 0; k < 2; ++k)
        {
            const auto& r = p.filter[k];
            const int fb = Dst::fBase + k * Dst::fPer;
            const float cutoff = r.cutoff->load() * std::exp2 (mod[fb + Dst::fCutoff] * 5.f);   // ±5 octavas
            filters.unit[k].setParams ((FilterType) std::clamp ((int) r.type->load(), 0, (int) FilterType::Count - 1),
                                       cutoff,
                                       c01 (r.res->load()   + mod[fb + Dst::fRes]),
                                       c01 (r.drive->load() + mod[fb + Dst::fDrive]));
            filters.on[k] = r.on->load() > 0.5f;
        }
        filters.parallel = p.filterRouting->load() > 0.5f;
        for (int s = 0; s < n; ++s) filters.process (mixL[s], mixR[s]);

        // ---- 6) amplitud (ENV 1) con rampa lineal dentro del sub-bloque
        const float a0 = lastAmp, a1 = src[Src::env];
        const float vg = 0.25f + 0.75f * velocity;
        for (int s = 0; s < n; ++s)
        {
            const float g = (a0 + (a1 - a0) * (float) (s + 1) / (float) n) * vg;
            outL[s] += mixL[s] * g;
            outR[s] += mixR[s] * g;
        }
        lastAmp = a1;
    }

    double sampleRate = 44100.0;
    std::unique_ptr<IOscEngine> engines[kNumOsc][(int) OscType::Count];
    int lastType[kNumOsc] = {};
    Envelope env[kNumEnv], noiseEnv;
    Lfo lfo[kNumLfo];
    SubOscillator sub;
    NoiseGenerator noise;
    DualFilter filters;
    float lastMod[Dst::count] = {}, lastSrc[Src::count] = {};
    unsigned lfoMask = 0;
    int   noteNumber = 60;
    float velocity = 1.f, lastAmp = 0.f;
    bool  held = false;
};
}
