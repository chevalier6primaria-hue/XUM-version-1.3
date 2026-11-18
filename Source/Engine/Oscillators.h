#pragma once
#include "Parameters.h"
#include "Engine/DspUtil.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace xum
{
// Parámetros de un oscilador para un sub-bloque, ya con la modulación aplicada.
struct OscFrame
{
    float baseFreq = 440.f;     // Hz de la nota MIDI
    float pitchSemis = 0.f;     // octava + semitonos + modulación
    float pan = 0.f;
    int   voices = 1;           // unísono (1..16)
    float detune = 0.f, blend = 1.f, spread = 0.f;
    float pos = 0.f, warp1 = 0.f, warp2 = 0.f;
    int   wave = 0;                          // tabla: 0 morph, 1 seno, 2 triangulo, 3 sierra, 4 cuadrada/PWM, 5 formantes
    int   warp1Mode = 1, warp2Mode = 3;      // ver WarpMode
    float grainSize = 80.f, grainDensity = 32.f, grainPos = 0.f, grainRand = 0.f;
};

// Interfaz común de los 5 motores. render() SUMA su salida en L/R.
class IOscEngine
{
public:
    virtual ~IOscEngine() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void noteOn() = 0;
    virtual void render (float* L, float* R, int numSamples, const OscFrame& f) = 0;
};

// ---------------------------------------------------------------- Unísono
struct UnisonLayout
{
    int count = 1;
    std::array<float, kMaxUnison> ratio{}, gainL{}, gainR{};

    void compute (int voices, float detune, float blend, float spread, float pan)
    {
        count = std::clamp (voices, 1, kMaxUnison);
        const float maxCents = detune * detune * 100.f;
        std::array<float, kMaxUnison> w{}, pans{};
        float sumSq = 0.f;
        for (int i = 0; i < count; ++i)
        {
            const float o = count > 1 ? 2.f * (float) i / (float) (count - 1) - 1.f : 0.f;
            ratio[(size_t) i] = std::exp2 (o * maxCents / 1200.f);
            w[(size_t) i]     = 1.f - (1.f - blend) * std::abs (o);
            pans[(size_t) i]  = std::clamp (pan + o * spread, -1.f, 1.f);
            sumSq += w[(size_t) i] * w[(size_t) i];
        }
        const float norm = 1.f / std::sqrt (std::max (sumSq, 1e-6f));
        for (int i = 0; i < count; ++i)
        {
            const float ang = (pans[(size_t) i] + 1.f) * 0.25f * kPi;
            gainL[(size_t) i] = std::cos (ang) * 1.41421356f * w[(size_t) i] * norm;
            gainR[(size_t) i] = std::sin (ang) * 1.41421356f * w[(size_t) i] * norm;
        }
    }
};

// ---------------------------------------------------------------- Banco de wavetables
// 6 tablas x 24 frames x 2048 muestras x 10 niveles mipmap (limitados en banda por octavas):
//   0 Basic Morph (seno > sierra > cuadrada > formante)   1 Seno (+armonicos suaves)
//   2 Triangular (+pares)   3 Sierra (oscura > brillante)   4 Cuadrada/PWM (50 % > 5 %)   5 Formantes (A E I O U)
class WavetableBank
{
public:
    static constexpr int tableSize = 2048, numFrames = 24, numMips = 10, numBanks = 6, stride = tableSize + 1;

    // Llamar una vez fuera del hilo de audio (se hace en el constructor del procesador).
    static const WavetableBank& instance() { static WavetableBank bank; return bank; }

    int mipFor (float inc) const
    {
        const float allowed = 0.5f / std::max (inc, 1e-5f);
        int m = 0;
        while (m < numMips - 1 && (float) (tableSize / 2 >> m) > allowed) ++m;
        return m;
    }

    // Interpolacion lineal en fase y entre frames ("Smooth Interpolation")
    float read (float phase, float framePos, int mip, int bank = 0) const
    {
        bank = std::clamp (bank, 0, numBanks - 1);
        const float fp = std::clamp (framePos, 0.f, 1.f) * (float) (numFrames - 1);
        const int   f0 = std::min ((int) fp, numFrames - 1), f1 = std::min (f0 + 1, numFrames - 1);
        const float ft = fp - (float) f0;
        const float ph = phase * (float) tableSize;
        const int   i0 = std::min ((int) ph, tableSize - 1);
        const float t  = ph - (float) i0;
        const float* a = frame (bank, mip, f0);
        const float* b = frame (bank, mip, f1);
        const float va = a[i0] + t * (a[i0 + 1] - a[i0]);
        const float vb = b[i0] + t * (b[i0 + 1] - b[i0]);
        return va + ft * (vb - va);
    }

private:
    WavetableBank()
    {
        data.assign ((size_t) numBanks * numMips * numFrames * stride, 0.f);
        std::vector<float> sinT ((size_t) tableSize);
        for (int i = 0; i < tableSize; ++i) sinT[(size_t) i] = std::sin (kTwoPi * (float) i / (float) tableSize);

        for (int bank = 0; bank < numBanks; ++bank)
            for (int f = 0; f < numFrames; ++f)
            {
                const float morph = (float) f / (float) (numFrames - 1);
                // se construye desde el nivel mas "bajo" (pocos armonicos) hacia el completo: cada armonico se calcula una vez
                int prevMax = 0;
                for (int m = numMips - 1; m >= 0; --m)
                {
                    float* d = frame (bank, m, f);
                    if (m < numMips - 1) std::copy (frame (bank, m + 1, f), frame (bank, m + 1, f) + stride, d);
                    const int maxH = (tableSize / 2) >> m;
                    for (int h = prevMax + 1; h <= maxH; ++h)
                    {
                        float a = 0.f, b = 0.f;
                        harmonic (bank, h, morph, a, b);
                        const bool hasB = std::abs (b) > 1e-5f;
                        if (std::abs (a) < 1e-5f && ! hasB) continue;
                        int idx = 0;
                        for (int n = 0; n < tableSize; ++n, idx = (idx + h) & (tableSize - 1))
                        {
                            float v = a * sinT[(size_t) idx];
                            if (hasB) v += b * sinT[(size_t) ((idx + tableSize / 4) & (tableSize - 1))];
                            d[n] += v;
                        }
                    }
                    prevMax = maxH;
                }
                // normaliza todos los niveles con el pico del nivel completo
                const float* full = frame (bank, 0, f);
                float pk = 1e-6f;
                for (int n = 0; n < tableSize; ++n) pk = std::max (pk, std::abs (full[n]));
                const float gain = 1.f / pk;
                for (int m = 0; m < numMips; ++m)
                {
                    float* d = frame (bank, m, f);
                    for (int n = 0; n < tableSize; ++n) d[n] *= gain;
                    d[tableSize] = d[0];
                }
            }
    }

    static float harmonicAmp (int h, float m)
    {
        const float seg  = m * 3.f;
        const float sine = (h == 1) ? 1.f : 0.f;
        const float saw  = 1.f / (float) h;
        const float sq   = (h & 1) ? saw : 0.f;
        if (seg < 1.f) return sine + (saw - sine) * seg;
        if (seg < 2.f) return saw + (sq - saw) * (seg - 1.f);
        const float t = seg - 2.f;
        const float dh = (float) h - 9.f;
        return sq * (1.f + 6.f * t * std::exp (-0.5f * dh * dh / 9.f));
    }

    // Amplitud del armonico h: a * sin(hx) + b * cos(hx)
    static void harmonic (int bank, int h, float m, float& a, float& b)
    {
        const float fh = (float) h;
        a = 0.f; b = 0.f;
        switch (bank)
        {
            case 0: a = harmonicAmp (h, m); break;
            case 1: a = (h == 1) ? 1.f : m * 0.9f / (fh * fh); break;
            case 2:
                if (h & 1) a = ((((h - 1) / 2) & 1) ? -1.f : 1.f) / (fh * fh);
                else       a = m * 0.8f / (fh * fh);
                break;
            case 3: a = std::exp (-(fh - 1.f) * (1.f - m) * 0.35f) / fh; break;
            case 4:
            {
                const float phi = kPi * (1.f - 0.9f * m);       // ancho de pulso 50 % -> 5 %
                a = (1.f - std::cos (fh * phi)) / fh;
                b = -std::sin (fh * phi) / fh;
                break;
            }
            default:
            {
                static const float vf[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 },
                                                { 450,  800, 2830 }, { 325,  700, 2530 } };
                static const float gw[3] = { 1.f, 0.6f, 0.35f }, bw[3] = { 110.f, 160.f, 220.f };
                const float t  = m * 4.f;
                const int   i0 = std::min ((int) t, 3);
                const float fr = t - (float) i0;
                const float freq = fh * 110.f;
                float s = 0.25f / std::pow (fh, 0.8f);
                for (int k = 0; k < 3; ++k)
                {
                    const float F = vf[i0][k] + (vf[i0 + 1][k] - vf[i0][k]) * fr;
                    const float d = (freq - F) / bw[k];
                    s += gw[k] * std::exp (-0.5f * d * d);
                }
                a = s;
                break;
            }
        }
    }

    float*       frame (int bank, int mip, int f)       { return data.data() + (((size_t) bank * numMips + (size_t) mip) * numFrames + (size_t) f) * stride; }
    const float* frame (int bank, int mip, int f) const { return data.data() + (((size_t) bank * numMips + (size_t) mip) * numFrames + (size_t) f) * stride; }

    std::vector<float> data;
};

// ---------------------------------------------------------------- Motor Wavetable
class WavetableEngine : public IOscEngine
{
public:
    void prepare (double sr) override { sampleRate = sr; }

    void noteOn() override
    {
        for (auto& p : phase) p = rng.next01();     // fase inicial aleatoria por voz de unísono
        firstBlock = true;
    }

    void render (float* L, float* R, int n, const OscFrame& f) override
    {
        const auto& bank = WavetableBank::instance();
        unison.compute (f.voices, f.detune, f.blend, f.spread, f.pan);

        const float freq = f.baseFreq * std::exp2 (f.pitchSemis / 12.f);
        const float pos1 = std::clamp (f.pos, 0.f, 1.f);
        const float pos0 = firstBlock ? pos1 : lastPos;
        const float posStep = (pos1 - pos0) / (float) n;     // rampa suave de la posición
        lastPos = pos1; firstBlock = false;

        for (int v = 0; v < unison.count; ++v)
        {
            const float inc = std::min (0.49f, freq * unison.ratio[(size_t) v] / (float) sampleRate);
            const int   mip = bank.mipFor (inc);
            const float gl = unison.gainL[(size_t) v], gr = unison.gainR[(size_t) v];
            float ph = phase[(size_t) v], pos = pos0;

            for (int s = 0; s < n; ++s)
            {
                pos += posStep;
                float p = warpPhase (warpPhase (ph, f.warp1Mode, f.warp1), f.warp2Mode, f.warp2);   // warps duales (fase)
                float y = bank.read (p, pos, mip, f.wave);
                y = warpOut (warpOut (y, ph, f.warp1Mode, f.warp1), ph, f.warp2Mode, f.warp2);       // warps duales (salida)
                L[s] += y * gl;
                R[s] += y * gr;
                ph += inc;
                if (ph >= 1.f) ph -= 1.f;
            }
            phase[(size_t) v] = ph;
        }
    }

private:
public:
    // Modos de warp. Los de "fase" deforman el recorrido de la tabla; los de "salida" procesan la señal.
    // Con cantidad 0 todos son transparentes.
    static float warpPhase (float p, int mode, float a)
    {
        if (a < 1e-3f) return p;
        switch ((WarpMode) mode)
        {
            case WarpMode::BendPlus:
            {
                const float c = 0.5f + 0.45f * a;
                return p < c ? 0.5f * p / c : 0.5f + 0.5f * (p - c) / (1.f - c);
            }
            case WarpMode::BendMinus:
            {
                const float c = 0.5f - 0.45f * a;
                return p < c ? 0.5f * p / c : 0.5f + 0.5f * (p - c) / (1.f - c);
            }
            case WarpMode::Sync:
            {
                const float q = p * (1.f + 7.f * a);
                return q - std::floor (q);
            }
            case WarpMode::Mirror:
            {
                const float tri = p < 0.5f ? 2.f * p : 2.f - 2.f * p;
                return p + a * (tri - p);
            }
            case WarpMode::FM:
            {
                const float q = p + a * 0.4f * std::sin (kTwoPi * p);
                return q - std::floor (q);
            }
            default: return p;
        }
    }

    static float warpOut (float y, float ph, int mode, float a)
    {
        if (a < 1e-3f) return y;
        switch ((WarpMode) mode)
        {
            case WarpMode::Ring:
                return y * (1.f - a + a * std::sin (kTwoPi * 2.f * ph));
            case WarpMode::Fold:
            {
                const float folded = std::sin (y * (1.f + a * 5.f) * 1.5707963f);
                return y + a * (folded - y);
            }
            case WarpMode::Drive:
            {
                const float k = 1.f + a * 8.f;
                return fastTanh (y * k) / fastTanh (k);
            }
            case WarpMode::Quantize:
            {
                const float lv = 64.f - a * 60.f;
                return std::round (y * lv) / lv;
            }
            default: return y;
        }
    }

private:
    double sampleRate = 44100.0;
    std::array<float, kMaxUnison> phase{};
    UnisonLayout unison;
    FastRand rng;
    float lastPos = 0.f;
    bool  firstBlock = true;
};

// ---------------------------------------------------------------- Motores pendientes
// Misma interfaz y ya enrutados en la voz: rellenar render() y listo.
class UnimplementedEngine : public IOscEngine
{
public:
    void prepare (double) override {}
    void noteOn() override {}
    void render (float*, float*, int, const OscFrame&) override {}
};

// TODO: reproducción por capas de velocidad / mapeo de teclas, parser SFZ.
class MultisampleEngine : public UnimplementedEngine {};
// TODO: un sample con "snap loop" (loop automático en cruces por cero) y tape-stop (rampa de velocidad).
class SampleEngine : public UnimplementedEngine {};
// ---------------------------------------------------------------- Granular
// Fuente de audio interna (se genera una vez): 4 s de un tono armonico a 110 Hz cuyos parciales
// evolucionan con el tiempo, mas un soplo de ruido. Al "escanear" la posicion cambia el timbre.
// (Cuando exista carga de archivos, este buffer se sustituira por el sample del usuario.)
class GrainSource
{
public:
    static constexpr int   length  = 44100 * 4;
    static constexpr float refFreq = 110.f;
    static constexpr float refRate = 44100.f;

    static const GrainSource& instance() { static GrainSource s; return s; }

    float read (float pos) const
    {
        int i = (int) pos;
        const float t = pos - (float) i;
        if (i >= length) i -= length;
        if (i < 0) i = 0;
        const int j = i + 1 >= length ? 0 : i + 1;
        return data[(size_t) i] + (data[(size_t) j] - data[(size_t) i]) * t;
    }

    // Picos por segmento para dibujar la forma de onda en la GUI
    void peaks (float* out, int n) const
    {
        const int per = length / std::max (1, n);
        for (int b = 0; b < n; ++b)
        {
            float pk = 0.f;
            for (int i = b * per; i < (b + 1) * per && i < length; i += 8) pk = std::max (pk, std::abs (data[(size_t) i]));
            out[b] = pk;
        }
    }

private:
    GrainSource()
    {
        data.assign ((size_t) length, 0.f);
        FastRand rng; rng.seed (4242u);
        float noiseState = 0.f;
        float hAmp[19] = {};
        for (int h = 1; h <= 18; ++h) hAmp[h] = 1.f / std::pow ((float) h, 1.15f);
        for (int n = 0; n < length; ++n)
        {
            const float t = (float) n / refRate;
            const float cyc = refFreq * t;
            float y = 0.f;
            for (int h = 1; h <= 18; ++h)
            {
                const float evo = 0.5f + 0.5f * std::sin (kTwoPi * (0.11f * t * (float) (1 + h % 5) + 0.37f * (float) h));
                y += std::sin (kTwoPi * (float) h * (cyc - std::floor (cyc))) * evo * hAmp[h];
            }
            noiseState += 0.15f * (rng.next() - noiseState);
            y += noiseState * 0.25f * (0.5f + 0.5f * std::sin (kTwoPi * 0.2f * t));
            data[(size_t) n] = y;
        }
        float pk = 1e-6f;
        for (float v : data) pk = std::max (pk, std::abs (v));
        for (auto& v : data) v *= 0.9f / pk;
    }

    std::vector<float> data;
};

class GranularEngine : public IOscEngine
{
public:
    void prepare (double sr) override { sampleRate = sr; }

    void noteOn() override
    {
        numActive = 0;
        countdown = 0.f;
        rng.seed (1000u + (uint32_t) (seedCounter++ * 7919u));
    }

    void render (float* L, float* R, int n, const OscFrame& f) override
    {
        const auto& src  = GrainSource::instance();
        const auto& hann = hannTable();
        const float density  = std::clamp (f.grainDensity, 1.f, (float) kMaxGrains);   // granos por segundo
        const float interval = (float) sampleRate / density;
        const int   len      = std::max (32, (int) (f.grainSize * 0.001f * (float) sampleRate));
        const float freq     = f.baseFreq * std::exp2 (f.pitchSemis / 12.f);
        const float inc      = freq / GrainSource::refFreq * (GrainSource::refRate / (float) sampleRate);
        const float overlap  = std::max (1.f, density * f.grainSize * 0.001f);
        const float norm     = 1.4f / std::sqrt (overlap);

        for (int s = 0; s < n; ++s)
        {
            countdown -= 1.f;
            if (countdown <= 0.f)
            {
                spawn (f, len, inc, norm);
                countdown += interval * (0.9f + 0.2f * rng.next01());
            }
            float l = 0.f, r = 0.f;
            for (int g = 0; g < numActive; )
            {
                auto& gr = grains[(size_t) g];
                const float fi = (float) gr.age * gr.invLen * 1024.f;
                const int   wi = std::min ((int) fi, 1023);
                const float w  = hann[(size_t) wi] + (hann[(size_t) wi + 1] - hann[(size_t) wi]) * (fi - (float) wi);
                const float y = src.read (gr.pos) * w;
                l += y * gr.gainL; r += y * gr.gainR;
                gr.pos += gr.inc;
                if (gr.pos >= (float) GrainSource::length) gr.pos -= (float) GrainSource::length;
                if (++gr.age >= gr.len) grains[(size_t) g] = grains[(size_t) (--numActive)];   // quitar grano terminado
                else ++g;
            }
            L[s] += l; R[s] += r;
        }
    }

private:
    struct Grain { float pos = 0, inc = 1, gainL = 0, gainR = 0, invLen = 1; int age = 0, len = 1; };

    // ventana de Hann precalculada (1024 + 1 puntos)
    static const std::array<float, 1025>& hannTable()
    {
        static const std::array<float, 1025> t = []
        {
            std::array<float, 1025> a{};
            for (int i = 0; i <= 1024; ++i) a[(size_t) i] = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / 1024.f);
            return a;
        }();
        return t;
    }

    void spawn (const OscFrame& f, int len, float inc, float norm)
    {
        if (numActive >= kMaxGrains) return;
        auto& g = grains[(size_t) numActive++];
        float p = f.grainPos + rng.next() * f.grainRand * 0.5f;
        p -= std::floor (p);
        g.pos = p * (float) GrainSource::length;
        g.inc = inc;
        g.age = 0;
        g.len = len;
        g.invLen = 1.f / (float) len;
        const float pan = std::clamp (f.pan + rng.next() * f.spread, -1.f, 1.f);
        const float ang = (pan + 1.f) * 0.25f * kPi;
        g.gainL = std::cos (ang) * 1.41421356f * norm;
        g.gainR = std::sin (ang) * 1.41421356f * norm;
    }

    double sampleRate = 44100.0;
    std::array<Grain, kMaxGrains> grains{};
    int   numActive = 0;
    float countdown = 0.f;
    FastRand rng;
    static inline uint32_t seedCounter = 1;
};
// TODO: resíntesis FFT / aditiva por parciales en tiempo real.
class SpectralEngine : public UnimplementedEngine {};

inline std::unique_ptr<IOscEngine> makeEngine (int type)
{
    switch ((OscType) type)
    {
        case OscType::Wavetable:   return std::make_unique<WavetableEngine>();
        case OscType::Multisample: return std::make_unique<MultisampleEngine>();
        case OscType::Sample:      return std::make_unique<SampleEngine>();
        case OscType::Granular:    return std::make_unique<GranularEngine>();
        case OscType::Spectral:    return std::make_unique<SpectralEngine>();
        default:                   return std::make_unique<UnimplementedEngine>();
    }
}
}
