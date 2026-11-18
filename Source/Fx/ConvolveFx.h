#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Fx/Effects.h"

namespace xum
{
// Respuestas al impulso internas generadas por el propio plugin (no ocupan archivos):
// 0 Pencil, 1 Plate, 2 Hall, 3 Spring, 4 Metal Pipe, 5 Glass
inline juce::AudioBuffer<float> makeInternalIR (int idx, double sampleRate)
{
    const float srf = (float) sampleRate;
    static const float seconds[6] = { 0.35f, 2.2f, 3.0f, 1.2f, 1.6f, 1.4f };
    idx = std::clamp (idx, 0, 5);
    const int len = std::max (64, (int) (seconds[idx] * srf));
    juce::AudioBuffer<float> buf (2, len);

    static const float metalRatio[8] = { 1.f, 2.76f, 5.40f, 8.93f, 13.34f, 18.64f, 24.82f, 31.88f };
    static const float glassFreq[6]  = { 1830.f, 2750.f, 3890.f, 5120.f, 6480.f, 7900.f };

    for (int ch = 0; ch < 2; ++ch)
    {
        FastRand rng;
        rng.seed (1234u + 77u * (uint32_t) ch + 13u * (uint32_t) idx);
        float* d = buf.getWritePointer (ch);
        float lp = 0.f;
        for (int n = 0; n < len; ++n)
        {
            const float t = (float) n / srf;
            float v = 0.f;
            switch (idx)
            {
                case 0:   // Pencil: rafaga corta y brillante
                    v = rng.next() * std::exp (-t * 22.f);
                    break;
                case 1:   // Plate: densa, pierde brillo con el tiempo
                    lp += (0.9f * std::exp (-t * 1.2f) + 0.05f) * (rng.next() - lp);
                    v = lp * std::exp (-t * 3.0f);
                    break;
                case 2:   // Hall: pre-retraso y cola oscura
                    lp += (0.55f * std::exp (-t) + 0.04f) * (rng.next() - lp);
                    v = t < 0.025f ? 0.f : lp * std::exp (-(t - 0.025f) * 2.0f);
                    break;
                case 3:   // Spring: ecos dispersivos
                    lp += 0.5f * (rng.next() - lp);
                    v = lp * std::exp (-t * 3.5f) * (std::fmod (t, 0.037f) < 0.0015f ? 2.f : 0.5f);
                    break;
                case 4:   // Metal Pipe: modos inarmonicos
                    for (int k = 0; k < 8; ++k)
                        v += std::sin (kTwoPi * (210.f + 20.f * (float) ch) * metalRatio[k] * t + (float) k * 1.7f)
                             * std::exp (-t / (1.2f - 0.1f * (float) k)) / (float) (k + 1);
                    v += rng.next() * 0.02f * std::exp (-t * 30.f);
                    break;
                default:  // Glass: modos agudos largos
                    for (int k = 0; k < 6; ++k)
                        v += std::sin (kTwoPi * glassFreq[k] * (1.f + 0.003f * (float) ch) * t + (float) k * 2.3f)
                             * std::exp (-t / (1.0f - 0.1f * (float) k)) / (float) (k + 2);
                    v += rng.next() * 0.01f * std::exp (-t * 40.f);
                    break;
            }
            d[n] = v;
        }
    }
    return buf;
}

// Convolve: reverberacion por convolucion (juce::dsp::Convolution) con IR internas o un archivo del usuario.
class ConvolveFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { tone = g ("con_tone"); pre = g ("con_pre"); gain = g ("con_gain"); }

    void prepare (double s) override
    {
        sr = s;
        for (auto& d : pdl) d.init ((int) (s * 0.25) + 8);
        juce::dsp::ProcessSpec spec { s, (juce::uint32) kFxChunk, 2 };
        conv.prepare (spec);
        reset();
    }

    void reset() override
    {
        conv.reset();
        for (auto& d : pdl) d.clear();
        for (auto& f : lpS) f = OnePole{};
    }

    void process (float* L, float* R, int n, double) override
    {
        const float preS = pre->load() * 0.001f * (float) sr;
        if (preS > 1.f)
            for (int s = 0; s < n; ++s)
            {
                const float xl = L[s], xr = R[s];
                L[s] = pdl[0].read (preS); R[s] = pdl[1].read (preS);
                pdl[0].write (xl); pdl[1].write (xr);
            }

        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        conv.process (ctx);

        const float g = dbToGain (gain->load());
        const float tn = tone->load();
        if (tn < 0.99f)
        {
            const float a = OnePole::coef (1000.f * std::pow (20.f, tn), sr);     // tono: 1 kHz ... 20 kHz
            for (int s = 0; s < n; ++s) { L[s] = lpS[0].lp (L[s], a) * g; R[s] = lpS[1].lp (R[s], a) * g; }
        }
        else if (g != 1.f)
            for (int s = 0; s < n; ++s) { L[s] *= g; R[s] *= g; }
    }

    // se llaman desde el hilo de mensajes (la carga es asincrona y no bloquea el audio)
    void loadInternal (int idx)
    {
        conv.loadImpulseResponse (makeInternalIR (idx, sr > 0.0 ? sr : 44100.0), sr > 0.0 ? sr : 44100.0,
                                  juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no,
                                  juce::dsp::Convolution::Normalise::yes);
    }
    void loadFile (const juce::File& f)
    {
        conv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::yes,
                                  0, juce::dsp::Convolution::Normalise::yes);
    }

private:
    AP tone{}, pre{}, gain{};
    double sr = 44100.0;
    juce::dsp::Convolution conv;
    DelayLine pdl[2];
    OnePole lpS[2];
};
}
