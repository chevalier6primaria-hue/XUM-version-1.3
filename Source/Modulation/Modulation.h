#pragma once
#include "Parameters.h"
#include "Engine/DspUtil.h"
#include <algorithm>
#include <cmath>

namespace xum
{
// =============================================================================
// Envolvente ADSR con tensión de curva ajustable.
//   tension = 0   -> segmentos lineales
//   tension > 0   -> curva convexa (arranca lento, termina rápido)
//   tension < 0   -> curva logarítmica (arranca rápido, termina lento)
// Se avanza en bloques cortos (sub-bloques de 16 muestras ≈ 0.36 ms @ 44.1 kHz),
// lo que la hace "ultrarrápida": el ataque mínimo es de 0.5 ms.
// =============================================================================
class Envelope
{
public:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double sr) { sampleRate = sr; }
    void set (float a, float d, float s, float r, float t) { att = a; dec = d; sus = s; rel = r; tension = t; }
    void reset() { stage = Stage::Idle; level = 0.f; pos = 0.f; from = 0.f; }

    void noteOn()  { stage = Stage::Attack; pos = 0.f; from = level; }   // re-disparo sin clic
    void noteOff() { if (stage != Stage::Idle) { stage = Stage::Release; pos = 0.f; from = level; } }

    bool  isActive() const { return stage != Stage::Idle; }
    float getLevel() const { return level; }

    float advance (int numSamples)
    {
        const float dt = (float) numSamples / (float) sampleRate;
        switch (stage)
        {
            case Stage::Idle:
                level = 0.f;
                break;
            case Stage::Attack:
                pos += (att > 1e-5f) ? dt / att : 1.f;
                if (pos >= 1.f) { level = 1.f; stage = Stage::Decay; pos = 0.f; from = 1.f; }
                else            level = from + (1.f - from) * shape (pos);
                break;
            case Stage::Decay:
                pos += (dec > 1e-5f) ? dt / dec : 1.f;
                if (pos >= 1.f) { level = sus; stage = Stage::Sustain; }
                else            level = from + (sus - from) * shape (pos);
                break;
            case Stage::Sustain:
                level = sus;
                break;
            case Stage::Release:
                pos += (rel > 1e-5f) ? dt / rel : 1.f;
                if (pos >= 1.f) { level = 0.f; stage = Stage::Idle; }
                else            level = from * (1.f - shape (pos));
                break;
        }
        return level;
    }

private:
    float shape (float x) const
    {
        if (std::abs (tension) < 0.01f) return x;
        const float k = tension * 5.f;
        return (std::exp (k * x) - 1.f) / (std::exp (k) - 1.f);
    }

    double sampleRate = 44100.0;
    Stage  stage = Stage::Idle;
    float  att = 0.005f, dec = 0.3f, sus = 0.7f, rel = 0.3f, tension = 0.f;
    float  level = 0.f, pos = 0.f, from = 0.f;
};

// =============================================================================
// LFO: formas geométricas (seno, triángulo, sierra, cuadrada), sample&hold y
// dos modos de caos matemático (mapa logístico suavizado y atractor de Lorenz).
// Salida bipolar [-1, 1].
// =============================================================================
class Lfo
{
public:
    enum Shape { Sine = 0, Triangle, Saw, Square, SampleHold, ChaosLogistic, ChaosLorenz, ChaosRossler, NumShapes };

    void setSampleRate (double sr) { sampleRate = sr; }
    void seed (uint32_t s) { rng.seed (s); }
    void retrigger() { phase = 0.f; }

    float advance (int n, float rateHz, int shape)
    {
        const float inc = rateHz * (float) n / (float) sampleRate;
        phase += inc;
        bool wrapped = false;
        while (phase >= 1.f) { phase -= 1.f; wrapped = true; }

        switch (shape)
        {
            case Sine:     return std::sin (kTwoPi * phase);
            case Triangle: return 1.f - 4.f * std::abs (phase - 0.5f);
            case Saw:      return 2.f * phase - 1.f;
            case Square:   return phase < 0.5f ? 1.f : -1.f;
            case SampleHold:
                if (wrapped) held = rng.next();
                return held;
            case ChaosLogistic:
                if (wrapped) { prev = cur; lx = 3.9f * lx * (1.f - lx); cur = lx * 2.f - 1.f; }
                return prev + (cur - prev) * phase;
            case ChaosLorenz:
            {
                const float tau   = inc * 1.5f;
                const int   steps = std::min (200, (int) std::ceil (tau / 0.01f) + 1);
                const float h     = tau / (float) steps;
                for (int i = 0; i < steps; ++i)
                {
                    const float dx = 10.f * (ly - zx);
                    const float dy = zx * (28.f - lz) - ly;
                    const float dz = zx * ly - (8.f / 3.f) * lz;
                    zx += dx * h; ly += dy * h; lz += dz * h;
                }
                return std::clamp (zx / 20.f, -1.f, 1.f);
            }
            case ChaosRossler:
            {
                // Atractor de Rossler (a = b = 0.2, c = 5.7), integrado con Euler en pasos pequeños
                const float tau   = inc * 6.f;
                const int   steps = std::min (200, (int) std::ceil (tau / 0.02f) + 1);
                const float h     = tau / (float) steps;
                for (int i = 0; i < steps; ++i)
                {
                    const float dx = -ry - rz;
                    const float dy = rx + 0.2f * ry;
                    const float dz = 0.2f + rz * (rx - 5.7f);
                    rx += dx * h; ry += dy * h; rz += dz * h;
                }
                return std::clamp (rx / 10.f, -1.f, 1.f);
            }
            default: return 0.f;
        }
    }

private:
    double sampleRate = 44100.0;
    float  phase = 0.f, held = 0.f;
    float  lx = 0.37f, prev = 0.f, cur = 0.f;           // logístico
    float  zx = 1.f, ly = 1.f, lz = 1.f;                // Lorenz (zx = x)
    float  rx = 0.1f, ry = 0.f, rz = 0.f;               // Rossler
    FastRand rng;
};

// Ciclos por pulso (negra) para las divisiones 1/1, 1/2, 1/4, 1/8, 1/16, 1/32
constexpr float kCyclesPerBeat[6] = { 0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f };

// =============================================================================
// Matriz de modulación: acumula en dst[] la suma (fuente * cantidad * inversión)
// de todas las filas activas. Los valores resultantes están normalizados y cada
// destino los escala a su rango (ver Voice::renderSub).
// =============================================================================
inline void applyMatrix (const ParamRefs& p, const float* src, float* dst)
{
    std::fill (dst, dst + Dst::count, 0.f);
    for (int i = 0; i < kNumSlots; ++i)
    {
        const auto& s = p.slot[i];
        const int si = (int) s.src->load();
        const int di = (int) s.dst->load();
        if (si <= 0 || di <= 0 || si >= Src::count || di >= Dst::count) continue;
        const float sign = s.invert->load() > 0.5f ? -1.f : 1.f;
        dst[di] += src[si] * s.amount->load() * sign;
    }
}
}
