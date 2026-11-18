#pragma once
#include "Engine/DspUtil.h"
#include <algorithm>
#include <cmath>

namespace xum
{
inline float polyBlep (float t, float dt)
{
    if (t < dt)         { t /= dt;           return t + t - t * t - 1.f; }
    if (t > 1.f - dt)   { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
}

// Sub-oscilador: 0 = seno, 1 = cuadrada, 2 = triángulo, 3 = sierra (con PolyBLEP) + saturación
class SubOscillator
{
public:
    void prepare (double sr) { sampleRate = sr; }
    void noteOn() { phase = 0.f; }

    void setup (float noteFreq, int octave, int wave, float sat)
    {
        inc   = std::min (0.45f, noteFreq * std::exp2 ((float) octave) / (float) sampleRate);
        type  = wave;
        drive = 1.f + sat * 9.f;
        norm  = 1.f / fastTanh (drive);
    }

    float tick()
    {
        float y = 0.f;
        switch (type)
        {
            case 0: y = std::sin (kTwoPi * phase); break;
            case 1: { const float p2 = (phase + 0.5f) - std::floor (phase + 0.5f);
                      y = (phase < 0.5f ? 1.f : -1.f) + polyBlep (phase, inc) - polyBlep (p2, inc); } break;
            case 2: y = 1.f - 4.f * std::abs (phase - 0.5f); break;
            default: y = 2.f * phase - 1.f - polyBlep (phase, inc); break;
        }
        phase += inc;
        if (phase >= 1.f) phase -= 1.f;
        if (drive > 1.001f) y = fastTanh (y * drive) * norm;
        return y;
    }

private:
    double sampleRate = 44100.0;
    float  phase = 0.f, inc = 0.f, drive = 1.f, norm = 1.f;
    int    type = 0;
};

// Generador de ruido blanco con filtro paso-bajo de un polo
class NoiseGenerator
{
public:
    void prepare (double sr) { sampleRate = sr; }
    void seed (uint32_t s) { rng.seed (s); }
    void setCutoff (float hz)
    {
        hz = std::clamp (hz, 20.f, 20000.f);
        coef = 1.f - std::exp (-kTwoPi * hz / (float) sampleRate);
    }
    float tick() { z += coef * (rng.next() - z); return z; }

private:
    double sampleRate = 44100.0;
    float coef = 1.f, z = 0.f;
    FastRand rng;
};
}
