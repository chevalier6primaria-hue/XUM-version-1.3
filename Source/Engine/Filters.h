#pragma once
#include "Parameters.h"
#include "Engine/DspUtil.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace xum
{
// ---------------------------------------------------------------- SVF TPT (Cytomic)
struct SvfCoeffs
{
    float a1 = 0, a2 = 0, a3 = 0, k = 2;
    void set (float fc, float res, double sr)
    {
        fc = std::clamp (fc, 20.f, (float) (sr * 0.45));
        const float g = std::tan (kPi * fc / (float) sr);
        k  = 2.f - 1.95f * std::clamp (res, 0.f, 1.f);
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};
struct SvfState { float ic1 = 0, ic2 = 0; };

inline void svfTick (const SvfCoeffs& c, SvfState& s, float x, float& lp, float& bp, float& hp)
{
    const float v3 = x - s.ic2;
    const float v1 = c.a1 * s.ic1 + c.a2 * v3;
    const float v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
    s.ic1 = 2.f * v1 - s.ic1;
    s.ic2 = 2.f * v2 - s.ic2;
    lp = v2; bp = v1; hp = x - c.k * v1 - v2;
}

// ---------------------------------------------------------------- Unidad de filtro
// Tipos: Ladder LP (24 dB, saturado), SVF LP/HP/BP, Comb (feedback) y Formantes (A-E-I-O-U).
class FilterUnit
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        maxDelay = (int) (sr / 20.0) + 4;
        for (auto& d : delay) d.assign ((size_t) maxDelay, 0.f);
        reset();
    }

    void reset()
    {
        for (auto& l : lad) l = Ladder{};
        for (auto& ch : svf) for (auto& s : ch) s = SvfState{};
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.f);
        wpos = 0;
        coeffsValid = false;
    }

    void setParams (FilterType t, float cutoff, float resonance, float drv)
    {
        const bool sameType = (t == type);
        type = t;
        res  = std::clamp (resonance, 0.f, 1.f);
        driveGain = 1.f + std::clamp (drv, 0.f, 1.f) * 6.f;
        driveComp = 1.f / (1.f + std::clamp (drv, 0.f, 1.f) * 2.f);
        cutoff = std::clamp (cutoff, 20.f, 20000.f);

        // sin cambios apreciables no se recalculan tan(), exp()...
        if (sameType && coeffsValid && std::abs (cutoff - lastCutoff) <= lastCutoff * 1e-3f && std::abs (res - lastRes) <= 1e-3f)
            return;
        coeffsValid = true; lastCutoff = cutoff; lastRes = res;

        switch (t)
        {
            case FilterType::LadderLP:
                ladF  = std::min (cutoff / (0.5f * (float) sampleRate), 0.85f) * 1.16f;
                ladFb = res * 4.f * (1.f - 0.15f * ladF * ladF);
                break;
            case FilterType::SvfLP: case FilterType::SvfHP: case FilterType::SvfBP:
                svfC[0].set (cutoff, res, sampleRate);
                break;
            case FilterType::Comb:
                combDelay = std::clamp ((float) sampleRate / cutoff, 2.f, (float) (maxDelay - 2));
                combFb    = res * 0.95f;
                break;
            case FilterType::Formant:
            {
                static const float v[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 1700, 2700 },
                                               { 450,  800, 2830 }, { 325,  700, 2530 } };
                const float t01 = std::clamp (std::log2 (cutoff / 20.f) / std::log2 (1000.f), 0.f, 1.f) * 4.f;
                const int   i0  = std::min ((int) t01, 3);
                const float fr  = t01 - (float) i0;
                for (int f = 0; f < 3; ++f)
                    svfC[f].set (v[i0][f] + (v[i0 + 1][f] - v[i0][f]) * fr, 0.4f + res * 0.55f, sampleRate);
                break;
            }
            default: break;
        }
    }

    void process (float& l, float& r)
    {
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            float x = *io[c];
            float lp, bp, hp;
            switch (type)
            {
                case FilterType::LadderLP: x = ladder (lad[c], fastTanh (x * driveGain) * driveComp); break;
                case FilterType::SvfLP:    svfTick (svfC[0], svf[c][0], fastTanh (x * driveGain) * driveComp, lp, bp, hp); x = lp; break;
                case FilterType::SvfHP:    svfTick (svfC[0], svf[c][0], fastTanh (x * driveGain) * driveComp, lp, bp, hp); x = hp; break;
                case FilterType::SvfBP:    svfTick (svfC[0], svf[c][0], fastTanh (x * driveGain) * driveComp, lp, bp, hp); x = bp * svfC[0].k; break;
                case FilterType::Comb:     x = comb (c, fastTanh (x * driveGain) * driveComp); break;
                case FilterType::Formant:
                {
                    static const float w[3] = { 1.f, 0.6f, 0.35f };
                    const float in = fastTanh (x * driveGain) * driveComp;
                    float acc = 0.f;
                    for (int f = 0; f < 3; ++f)
                    {
                        svfTick (svfC[f], svf[c][f], in, lp, bp, hp);
                        acc += bp * svfC[f].k * w[f];
                    }
                    x = acc;
                    break;
                }
                default: break;
            }
            *io[c] = x;
        }
        if (type == FilterType::Comb) wpos = (wpos + 1) % maxDelay;
    }

private:
    struct Ladder { float in1 = 0, in2 = 0, in3 = 0, in4 = 0, o1 = 0, o2 = 0, o3 = 0, o4 = 0; };

    float ladder (Ladder& s, float x) const
    {
        const float f = ladF, f2 = f * f, f4 = f2 * f2;
        x -= s.o4 * ladFb;
        x *= 0.35013f * f4;
        s.o1 = x    + 0.3f * s.in1 + (1.f - f) * s.o1; s.in1 = x;
        s.o2 = s.o1 + 0.3f * s.in2 + (1.f - f) * s.o2; s.in2 = s.o1;
        s.o3 = s.o2 + 0.3f * s.in3 + (1.f - f) * s.o3; s.in3 = s.o2;
        s.o4 = s.o3 + 0.3f * s.in4 + (1.f - f) * s.o4; s.in4 = s.o3;
        return s.o4;
    }

    float comb (int c, float x)
    {
        auto& d = delay[c];
        float rp = (float) wpos - combDelay;
        while (rp < 0.f) rp += (float) maxDelay;
        const int   i0 = (int) rp % maxDelay, i1 = (i0 + 1) % maxDelay;
        const float fr = rp - std::floor (rp);
        const float delayed = d[(size_t) i0] + (d[(size_t) i1] - d[(size_t) i0]) * fr;
        const float y = x + combFb * delayed;
        d[(size_t) wpos] = std::clamp (y, -4.f, 4.f);
        return y * 0.5f;
    }

    double sampleRate = 44100.0;
    FilterType type = FilterType::LadderLP;
    float res = 0, driveGain = 1, driveComp = 1;
    bool  coeffsValid = false;
    float lastCutoff = 1000.f, lastRes = 0.f;
    float ladF = 0.1f, ladFb = 0;
    float combDelay = 100.f, combFb = 0;
    int   maxDelay = 2205, wpos = 0;
    Ladder lad[2];
    SvfCoeffs svfC[3];
    SvfState  svf[2][3];
    std::vector<float> delay[2];
};

// ---------------------------------------------------------------- Módulo doble (serie / paralelo)
class DualFilter
{
public:
    FilterUnit unit[2];
    bool on[2] = { true, false };
    bool parallel = false;

    void prepare (double sr) { for (auto& u : unit) u.prepare (sr); }
    void reset()             { for (auto& u : unit) u.reset(); }

    void process (float& l, float& r)
    {
        if (parallel)
        {
            if (on[0] && on[1])
            {
                float l1 = l, r1 = r, l2 = l, r2 = r;
                unit[0].process (l1, r1);
                unit[1].process (l2, r2);
                l = 0.5f * (l1 + l2);
                r = 0.5f * (r1 + r2);
            }
            else if (on[0]) unit[0].process (l, r);
            else if (on[1]) unit[1].process (l, r);
        }
        else
        {
            if (on[0]) unit[0].process (l, r);
            if (on[1]) unit[1].process (l, r);
        }
    }
};
}
