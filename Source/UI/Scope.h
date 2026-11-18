#pragma once
#include <array>
#include <atomic>

namespace xum
{
// Buffer circular sin bloqueos: el hilo de audio escribe, la GUI lee (con disparo por flanco).
class ScopeBuffer
{
public:
    static constexpr int size = 8192, mask = size - 1;

    ScopeBuffer() { for (auto& d : data) d.store (0.f, std::memory_order_relaxed); }

    void push (const float* l, const float* r, int n)
    {
        int w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            data[(size_t) w].store (0.5f * (l[i] + r[i]), std::memory_order_relaxed);
            w = (w + 1) & mask;
        }
        writePos.store (w, std::memory_order_release);
    }

    // Copia n muestras empezando en un cruce ascendente por cero (imagen estable)
    void readTriggered (float* dst, int n) const
    {
        const int w = writePos.load (std::memory_order_acquire);
        const int start = (w - n * 2) & mask;
        int offset = 0;
        for (int i = 0; i < n; ++i)
        {
            const float a = data[(size_t) ((start + i) & mask)].load (std::memory_order_relaxed);
            const float b = data[(size_t) ((start + i + 1) & mask)].load (std::memory_order_relaxed);
            if (a <= 0.f && b > 0.f) { offset = i; break; }
        }
        for (int i = 0; i < n; ++i)
            dst[i] = data[(size_t) ((start + offset + i) & mask)].load (std::memory_order_relaxed);
    }

private:
    std::array<std::atomic<float>, size> data;
    std::atomic<int> writePos { 0 };
};
}
