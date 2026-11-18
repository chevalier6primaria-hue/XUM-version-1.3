#pragma once
#include <cstdint>

namespace xum
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

// tanh barato (aproximacion racional, error < 2 %): mucho menos CPU que std::tanh en el bucle de audio
inline float fastTanh (float x)
{
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Generador pseudoaleatorio barato (xorshift32), seguro para el hilo de audio.
struct FastRand
{
    uint32_t state = 2463534242u;
    void  seed (uint32_t s) { state = s ? s : 1u; }
    float next()   // [-1, 1)
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (state & 0xFFFFFFu) * (1.0f / 8388608.0f) - 1.0f;
    }
    float next01() { return 0.5f * (next() + 1.0f); }
};
}
