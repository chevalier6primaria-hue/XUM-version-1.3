#pragma once
#include "Parameters.h"
#include "Fx/FxParams.h"
#include "Engine/DspUtil.h"
#include "Engine/Filters.h"
#include "Modulation/Modulation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

// Motor de efectos de XUM: cada efecto transforma el bloque "in place" en senal 100 % wet;
// la mezcla dry/wet, el encendido y los medidores los gestiona FxChain.
namespace xum
{
constexpr int kFxChunk = 256;

using ParamGetter = std::function<AP (const char*)>;

// ------------------------------------------------------------------ utilidades
struct DelayLine
{
    std::vector<float> buf;
    int size = 4, w = 0;

    void init (int maxSamples) { size = std::max (4, maxSamples); buf.assign ((size_t) size, 0.f); w = 0; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); w = 0; }
    // leer ANTES de escribir: d = 1 devuelve la muestra anterior
    float read (float d) const
    {
        d = std::clamp (d, 1.f, (float) (size - 2));
        float rp = (float) w - d;
        if (rp < 0.f) rp += (float) size;
        int i0 = (int) rp;
        const float fr = rp - (float) i0;
        if (i0 >= size) i0 -= size;
        const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * fr;
    }
    void write (float x) { buf[(size_t) w] = x; if (++w >= size) w = 0; }
};

struct OnePole
{
    float z = 0.f;
    static float coef (float fc, double sr) { return 1.f - std::exp (-kTwoPi * std::clamp (fc, 5.f, (float) (sr * 0.45)) / (float) sr); }
    float lp (float x, float a) { z += a * (x - z); return z; }
};

struct DcBlock
{
    float x1 = 0.f, y1 = 0.f;
    float process (float x) { const float y = x - x1 + 0.995f * y1; x1 = x; y1 = y; return y; }
};

inline float dbToGain (float db) { return std::exp2 (db * 0.16609640f); }   // 10^(db/20)

class FxBase
{
public:
    virtual ~FxBase() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void process (float* L, float* R, int n, double bpm) = 0;
};

// ------------------------------------------------------------------ Hyper / Dimension
class HyperFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { mode = g ("hyp_mode"); rate = g ("hyp_rate"); depth = g ("hyp_depth");
                                        voices = g ("hyp_voices"); detune = g ("hyp_detune"); size = g ("hyp_size"); }
    void prepare (double s) override { sr = (float) s; for (auto& d : dl) d.init ((int) (s * 0.06) + 8); reset(); }
    void reset() override
    {
        for (auto& d : dl) d.clear();
        for (int v = 0; v < 7; ++v) ph[v] = (float) v / 7.f;
        dimPhase = 0.f;
    }

    void process (float* L, float* R, int n, double) override
    {
        const int   nv  = std::clamp ((int) voices->load(), 2, 7);
        const float r   = rate->load(), dp = depth->load(), dt = detune->load(), sz = size->load();
        const bool  dim = mode->load() > 0.5f;
        const float inc = r / sr;
        const float norm = 1.2f / std::sqrt ((float) nv * 0.5f);

        for (int s = 0; s < n; ++s)
        {
            const float inL = L[s], inR = R[s];
            float outL = 0.f, outR = 0.f;
            if (! dim)
            {
                for (int v = 0; v < nv; ++v)
                {
                    const float o = (float) v / (float) (nv - 1) * 2.f - 1.f;          // -1..1
                    ph[v] += inc * (1.f + dt * 0.25f * o);                              // separacion de unisono
                    if (ph[v] >= 1.f) ph[v] -= 1.f;
                    const float lfo = 0.5f + 0.5f * std::sin (kTwoPi * ph[v]);
                    const float dMs = 7.f + dp * 6.f * lfo + dt * 3.f * o;
                    const float d   = dMs * 0.001f * sr;
                    const float ang = (o * 0.8f + 1.f) * 0.25f * kPi;
                    outL += dl[0].read (d) * std::cos (ang);
                    outR += dl[1].read (d) * std::sin (ang);
                }
                outL *= norm; outR *= norm;
                dl[0].write (inL); dl[1].write (inR);
            }
            else
            {
                dimPhase += inc * 0.5f; if (dimPhase >= 1.f) dimPhase -= 1.f;
                const float m = std::sin (kTwoPi * dimPhase) * (0.2f + dp * 0.8f);
                const float wl = dl[0].read ((7.f + 3.f * sz + m) * 0.001f * sr);
                const float wr = dl[1].read ((11.f + 4.f * sz - m) * 0.001f * sr);
                dl[0].write (inL * 0.8f + wr * 0.3f * sz);
                dl[1].write (inR * 0.8f + wl * 0.3f * sz);
                outL = (wl - 0.4f * wr) * 1.3f;
                outR = (wr - 0.4f * wl) * 1.3f;
            }
            L[s] = outL; R[s] = outR;
        }
    }

private:
    AP mode{}, rate{}, depth{}, voices{}, detune{}, size{};
    float sr = 44100.f, ph[7] = {}, dimPhase = 0.f;
    DelayLine dl[2];
};

// ------------------------------------------------------------------ Distortion
class DistortionFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { type = g ("dist_type"); drive = g ("dist_drive"); asym = g ("dist_asym"); fpos = g ("dist_fpos");
                                        ftype = g ("dist_ftype"); ffreq = g ("dist_ffreq"); fres = g ("dist_fres"); }
    void prepare (double s) override { sr = s; reset(); }
    void reset() override { for (auto& a : st) for (auto& b : a) b = SvfState{}; dc[0] = dc[1] = DcBlock{}; }

    static float shape (float x, int t)
    {
        switch (t)
        {
            case 0:  return fastTanh (x + 0.25f * x * x);                                   // Tube (armonicos pares)
            case 1:  return x / (1.f + std::abs (x));                                       // Solid State
            case 2:  return std::copysign (1.f - std::exp (-3.f * std::abs (x)), x);        // Diaper (saturador exponencial)
            case 3:  return x > 1.f ? 1.f : (x < -1.f ? -1.f : 1.5f * x - 0.5f * x * x * x);// Soft Clip
            case 4:  return std::clamp (x, -1.f, 1.f);                                      // Hard Clip
            case 5:  { float m = std::fmod (x + 1.f, 4.f); if (m < 0.f) m += 4.f; return 1.f - std::abs (m - 2.f); }  // Foldback
            case 6:  return std::abs (x);                                                   // Rectify
            case 7:  return x >= 0.f ? fastTanh (x) : 1.1f * fastTanh (0.8f * x);          // Overdrive A (calido, asimetrico)
            case 8:  { const float a = fastTanh (1.6f * x); return fastTanh (1.3f * a) ; }  // Overdrive B (dos etapas)
            default: return fastTanh (x + 0.35f * x * x * x);                               // Overdrive C (agresivo)
        }
    }

    void process (float* L, float* R, int n, double) override
    {
        const int   t  = std::clamp ((int) type->load(), 0, 9);
        const float dr = drive->load();
        const float g  = 1.f + dr * dr * 40.f;
        const float comp = 1.f / (1.f + dr * dr * 1.5f);
        const float bias = asym->load() * 0.5f;
        const float b0 = shape (bias, t);
        const int   fp = (int) fpos->load();
        const int   ft = (int) ftype->load();
        if (fp != 0) coef.set (ffreq->load(), fres->load(), sr);

        float* io[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < n; ++s)
            {
                float x = io[c][s];
                if (fp == 1) x = filt (c, x, ft);
                float y = (shape (x * g + bias, t) - b0) * comp;
                y = dc[c].process (y);
                if (fp == 2) y = filt (c, y, ft);
                io[c][s] = y;
            }
    }

private:
    float filt (int c, float x, int ft)
    {
        float lp, bp, hp;
        svfTick (coef, st[c][0], x, lp, bp, hp);
        return ft == 0 ? lp : (ft == 1 ? hp : bp * coef.k);
    }
    AP type{}, drive{}, asym{}, fpos{}, ftype{}, ffreq{}, fres{};
    double sr = 44100.0;
    SvfCoeffs coef;
    SvfState st[2][1];
    DcBlock dc[2];
};

// ------------------------------------------------------------------ Bode (frequency shifter)
// Desplazamiento en Hz mediante una red Hilbert de allpass (senal analitica) y modulacion en banda lateral unica.
class BodeFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { shift = g ("bode_shift"); dir = g ("bode_dir"); feed = g ("bode_feed"); }
    void prepare (double s) override { sr = (float) s; reset(); }
    void reset() override { for (auto& h : hil) h.init(); phase = 0.f; fbL = fbR = 0.f; }

    void process (float* L, float* R, int n, double) override
    {
        const float hz = shift->load();
        const int   d  = (int) dir->load();
        const float fb = feed->load();
        const float inc = kTwoPi * hz / sr;
        for (int s = 0; s < n; ++s)
        {
            phase += inc; if (phase > kTwoPi) phase -= kTwoPi;
            const float c = std::cos (phase), sn = std::sin (phase);
            float il, ql, ir, qr;
            hil[0].process (L[s] + fb * fbL, il, ql);
            hil[1].process (R[s] + fb * fbR, ir, qr);
            float oL, oR;
            if (d == 0)      { oL = il * c + ql * sn; oR = ir * c + qr * sn; }      // hacia arriba
            else if (d == 1) { oL = il * c - ql * sn; oR = ir * c - qr * sn; }      // hacia abajo
            else             { oL = il * c;           oR = ir * c; }                // ambos (modulacion en anillo)
            fbL = fastTanh (oL); fbR = fastTanh (oR);
            L[s] = oL; R[s] = oR;
        }
    }

    // Red Hilbert: dos cadenas de 4 allpass de 2.o orden con diferencia de fase de 90 grados
    struct Hilbert
    {
        struct Stage
        {
            float a = 0.f, x1 = 0.f, x2 = 0.f, y1 = 0.f, y2 = 0.f;
            float process (float x)
            {
                const float y = a * (x + y2) - x2;
                x2 = x1; x1 = x; y2 = y1; y1 = y;
                return y;
            }
        };
        Stage p1[4], p2[4];
        float delayed = 0.f;
        void init()
        {
            static const float c1[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            static const float c2[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            for (int i = 0; i < 4; ++i) { p1[i] = Stage{}; p2[i] = Stage{}; p1[i].a = c1[i] * c1[i]; p2[i].a = c2[i] * c2[i]; }
            delayed = 0.f;
        }
        void process (float x, float& i, float& q)
        {
            float a = x, b = x;
            for (int k = 0; k < 4; ++k) { a = p1[k].process (a); b = p2[k].process (b); }
            i = delayed;       // la rama 1 lleva un retardo de una muestra
            delayed = a;
            q = b;
        }
    };

private:
    AP shift{}, dir{}, feed{};
    float sr = 44100.f, phase = 0.f, fbL = 0.f, fbR = 0.f;
    Hilbert hil[2];
};

// ------------------------------------------------------------------ Chorus
class ChorusFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { voices = g ("cho_voices"); rate = g ("cho_rate"); depth = g ("cho_depth"); delay = g ("cho_delay");
                                        fb = g ("cho_fb"); lpf = g ("cho_lpf"); width = g ("cho_width"); }
    void prepare (double s) override { sr = (float) s; for (auto& d : dl) d.init ((int) (s * 0.08) + 8); reset(); }
    void reset() override { for (auto& d : dl) d.clear(); phase = 0.f; lpS[0] = lpS[1] = OnePole{}; fbS[0] = fbS[1] = 0.f; }

    void process (float* L, float* R, int n, double) override
    {
        const int   nv = std::clamp ((int) voices->load(), 1, 4);
        const float inc = rate->load() / sr, dp = depth->load(), base = delay->load();
        const float f = fb->load(), w = width->load();
        const float a = OnePole::coef (lpf->load(), sr);
        const float norm = 1.f / std::sqrt ((float) nv);
        for (int s = 0; s < n; ++s)
        {
            phase += inc; if (phase >= 1.f) phase -= 1.f;
            float wl = 0.f, wr = 0.f;
            for (int v = 0; v < nv; ++v)
            {
                const float pv = phase + (float) v / (float) nv;
                const float lfoL = std::sin (kTwoPi * pv), lfoR = std::sin (kTwoPi * (pv + 0.25f));
                wl += dl[0].read ((base + dp * base * 0.5f * lfoL) * 0.001f * sr);
                wr += dl[1].read ((base + dp * base * 0.5f * lfoR) * 0.001f * sr);
            }
            wl *= norm; wr *= norm;
            const float inL = L[s], inR = R[s];
            fbS[0] = lpS[0].lp (wl, a); fbS[1] = lpS[1].lp (wr, a);        // filtro paso bajo en la retroalimentacion
            dl[0].write (inL + f * fbS[0]); dl[1].write (inR + f * fbS[1]);
            const float m = 0.5f * (wl + wr), sd = 0.5f * (wl - wr) * w;   // ancho estereo
            L[s] = m + sd; R[s] = m - sd;
        }
    }

private:
    AP voices{}, rate{}, depth{}, delay{}, fb{}, lpf{}, width{};
    float sr = 44100.f, phase = 0.f, fbS[2] = {};
    DelayLine dl[2];
    OnePole lpS[2];
};

// ------------------------------------------------------------------ Flanger
class FlangerFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { delay = g ("fla_delay"); depth = g ("fla_depth"); rate = g ("fla_rate"); fb = g ("fla_fb");
                                        inv = g ("fla_inv"); sync = g ("fla_sync"); div = g ("fla_div"); }
    void prepare (double s) override { sr = (float) s; for (auto& d : dl) d.init ((int) (s * 0.02) + 8); reset(); }
    void reset() override { for (auto& d : dl) d.clear(); phase = 0.f; last[0] = last[1] = 0.f; }

    void process (float* L, float* R, int n, double bpm) override
    {
        float hz = rate->load();
        if (sync->load() > 0.5f) hz = (float) (bpm / 60.0) * kCyclesPerBeat[std::clamp ((int) div->load(), 0, 5)];
        const float inc = hz / sr, base = delay->load(), dp = depth->load();
        const float f = fb->load() * (inv->load() > 0.5f ? -1.f : 1.f);   // inversion de fase de la retroalimentacion
        for (int s = 0; s < n; ++s)
        {
            phase += inc; if (phase >= 1.f) phase -= 1.f;
            const float lL = 0.5f + 0.5f * std::sin (kTwoPi * phase);
            const float lR = 0.5f + 0.5f * std::sin (kTwoPi * (phase + 0.25f));
            const float dL = std::max (1.f, base * (1.f - dp * 0.85f * lL) * 0.001f * sr);
            const float dR = std::max (1.f, base * (1.f - dp * 0.85f * lR) * 0.001f * sr);
            const float wl = dl[0].read (dL), wr = dl[1].read (dR);
            dl[0].write (L[s] + f * last[0]); dl[1].write (R[s] + f * last[1]);
            last[0] = fastTanh (wl); last[1] = fastTanh (wr);
            L[s] = wl; R[s] = wr;
        }
    }

private:
    AP delay{}, depth{}, rate{}, fb{}, inv{}, sync{}, div{};
    float sr = 44100.f, phase = 0.f, last[2] = {};
    DelayLine dl[2];
};

// ------------------------------------------------------------------ Phaser
class PhaserFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { stages = g ("pha_stages"); freq = g ("pha_freq"); fb = g ("pha_fb"); rate = g ("pha_rate");
                                        depth = g ("pha_depth"); offset = g ("pha_offset"); }
    void prepare (double s) override { sr = (float) s; reset(); }
    void reset() override { for (auto& c : z) for (auto& v : c) v = 0.f; last[0] = last[1] = 0.f; phase = 0.f; a[0] = a[1] = 0.f; }

    void process (float* L, float* R, int n, double) override
    {
        const int   ns  = std::clamp (((int) stages->load() / 2) * 2, 4, 32);
        const float inc = rate->load() / sr, f0 = freq->load(), dp = depth->load(), f = fb->load(), off = offset->load() * 0.5f;
        float* io[2] = { L, R };
        for (int s = 0; s < n; ++s)
        {
            phase += inc; if (phase >= 1.f) phase -= 1.f;
            if ((s & 7) == 0)
                for (int c = 0; c < 2; ++c)
                {
                    const float lfo = std::sin (kTwoPi * (phase + (c == 1 ? off : 0.f)));
                    const float fc = std::clamp (f0 * std::exp2 (dp * 2.f * lfo), 20.f, 0.45f * sr);
                    const float t = std::tan (kPi * fc / sr);
                    a[c] = (t - 1.f) / (t + 1.f);
                }
            for (int c = 0; c < 2; ++c)
            {
                float x = io[c][s] + f * last[c];
                for (int k = 0; k < ns; ++k)
                {
                    const float y = a[c] * x + z[c][k];
                    z[c][k] = x - a[c] * y;
                    x = y;
                }
                last[c] = fastTanh (x);
                io[c][s] = x;
            }
        }
    }

private:
    AP stages{}, freq{}, fb{}, rate{}, depth{}, offset{};
    float sr = 44100.f, phase = 0.f, a[2] = {}, last[2] = {}, z[2][32] = {};
};

// ------------------------------------------------------------------ Delay
class DelayFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { sync = g ("del_sync"); tL = g ("del_timeL"); tR = g ("del_timeR"); dL = g ("del_divL"); dR = g ("del_divR");
                                        pp = g ("del_pp"); fb = g ("del_fb"); hp = g ("del_hp"); lp = g ("del_lp"); sat = g ("del_sat"); }
    void prepare (double s) override { sr = (float) s; for (auto& d : dl) d.init ((int) (s * 2.5)); reset(); }
    void reset() override { for (auto& d : dl) d.clear(); cur[0] = cur[1] = -1.f; for (auto& f : hpS) f = OnePole{}; for (auto& f : lpS) f = OnePole{}; }

    static float beats (int div)
    {
        static const float b[9] = { 4.f, 2.f, 1.f, 0.5f, 0.25f, 0.125f, 2.f / 3.f, 1.f / 3.f, 1.f / 6.f };
        return b[std::clamp (div, 0, 8)];
    }

    void process (float* L, float* R, int n, double bpm) override
    {
        const bool  sy = sync->load() > 0.5f;
        const float b  = (float) std::max (20.0, bpm);
        float target[2];
        target[0] = sy ? beats ((int) dL->load()) * 60.f / b * sr : tL->load() * 0.001f * sr;
        target[1] = sy ? beats ((int) dR->load()) * 60.f / b * sr : tR->load() * 0.001f * sr;
        for (auto& t : target) t = std::clamp (t, 2.f, (float) dl[0].size - 4.f);
        if (cur[0] < 0.f) { cur[0] = target[0]; cur[1] = target[1]; }
        const float f = fb->load(), sa = sat->load();
        const float ahp = OnePole::coef (hp->load(), sr), alp = OnePole::coef (lp->load(), sr);
        const bool ping = pp->load() > 0.5f;
        for (int s = 0; s < n; ++s)
        {
            cur[0] += (target[0] - cur[0]) * 0.0008f;           // el tiempo cambia suavemente
            cur[1] += (target[1] - cur[1]) * 0.0008f;
            const float oL = dl[0].read (cur[0]), oR = dl[1].read (cur[1]);
            const float pL = proc (0, oL, ahp, alp, sa), pR = proc (1, oR, ahp, alp, sa);
            if (ping) { dl[0].write ((L[s] + R[s]) * 0.5f + f * pR); dl[1].write (f * pL); }
            else      { dl[0].write (L[s] + f * pL);                  dl[1].write (R[s] + f * pR); }
            L[s] = oL; R[s] = oR;
        }
    }

private:
    // filtros de graves/agudos y saturacion en la linea de retroalimentacion
    float proc (int c, float x, float ahp, float alp, float sa)
    {
        x -= hpS[c].lp (x, ahp);
        x = lpS[c].lp (x, alp);
        if (sa > 0.001f) x = fastTanh (x * (1.f + sa * 4.f)) / (1.f + sa * 1.5f);
        return x;
    }
    AP sync{}, tL{}, tR{}, dL{}, dR{}, pp{}, fb{}, hp{}, lp{}, sat{};
    float sr = 44100.f, cur[2] = { -1.f, -1.f };
    DelayLine dl[2];
    OnePole hpS[2], lpS[2];
};

// ------------------------------------------------------------------ Reverb (FDN de 8 lineas, 6 algoritmos)
class ReverbFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { algo = g ("rev_algo"); size = g ("rev_size"); decay = g ("rev_decay"); pre = g ("rev_pre");
                                        damp = g ("rev_damp"); mod = g ("rev_mod"); lo = g ("rev_lo"); }
    void prepare (double s) override
    {
        sr = (float) s;
        for (auto& l : line) l.init ((int) (s * 0.45));
        pdl.init ((int) (s * 0.25) + 8);
        for (int i = 0; i < 2; ++i) ap[i].init ((int) (s * 0.02) + 8);
        reset();
    }
    void reset() override
    {
        for (auto& l : line) l.clear();
        pdl.clear(); for (auto& a : ap) a.clear();
        for (auto& f : lpS) f = OnePole{};
        lowS[0] = lowS[1] = OnePole{};
        phase = 0.f;
    }

    void process (float* L, float* R, int n, double) override
    {
        static const float setA[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.9f };
        static const float setB[8] = { 23.1f, 31.9f, 38.3f, 47.9f, 55.7f, 61.3f, 71.9f, 79.1f };
        struct Algo { const float* lens; float scale, mod, rate, dampMul, diff; };
        static const Algo algos[6] = {
            { setB, 0.55f, 0.35f, 0.8f, 0.6f, 0.75f },   // Plate
            { setA, 0.45f, 0.12f, 0.5f, 1.2f, 0.50f },   // Room
            { setA, 1.10f, 0.25f, 0.6f, 1.0f, 0.70f },   // Hall
            { setB, 1.50f, 0.60f, 1.3f, 0.8f, 0.85f },   // Nitrous
            { setA, 0.85f, 0.08f, 0.4f, 1.8f, 0.45f },   // Vintage
            { setB, 2.00f, 0.15f, 0.3f, 1.5f, 0.60f },   // Basin
        };
        const Algo& al = algos[std::clamp ((int) algo->load(), 0, 5)];
        const float sz = 0.5f + size->load();
        const float dec = std::max (0.1f, decay->load());
        const float dmp = damp->load();
        float len[8], gain[8];
        for (int i = 0; i < 8; ++i)
        {
            len[i]  = std::min (al.lens[i] * al.scale * sz * 0.001f * sr, (float) line[i].size - 40.f);
            gain[i] = std::pow (10.f, -3.f * (len[i] / sr) / dec);        // RT60
        }
        const float cutoff = 18000.f * (1.f - std::min (0.95f, dmp * al.dampMul * 0.9f));
        const float aDamp = OnePole::coef (cutoff, sr);
        const float aLow  = OnePole::coef (lo->load(), sr);
        const float preS  = pre->load() * 0.001f * sr;
        const float md    = mod->load() * al.mod * 0.0004f * sr;            // profundidad de modulacion de la cola
        const float inc   = al.rate / sr;
        const float dg    = al.diff * 0.7f;
        const float apLen[2] = { 0.0032f * sr, 0.0024f * sr };

        for (int s = 0; s < n; ++s)
        {
            phase += inc; if (phase >= 1.f) phase -= 1.f;
            // pre-delay + difusion de entrada
            float x = (L[s] + R[s]) * 0.5f;
            const float xd = pre->load() > 0.5f ? pdl.read (std::max (1.f, preS)) : x;
            pdl.write (x);
            float u = xd;
            for (int k = 0; k < 2; ++k)
            {
                const float d = ap[k].read (apLen[k]);
                const float v = u + dg * d;
                ap[k].write (v);
                u = d - dg * v;
            }
            // lectura de las 8 lineas (las pares con modulacion)
            float y[8];
            for (int i = 0; i < 8; ++i)
            {
                const float m = (i & 1) ? 0.f : md * std::sin (kTwoPi * (phase + (float) i * 0.125f));
                y[i] = line[i].read (len[i] + m);
            }
            float z[8];
            for (int i = 0; i < 8; ++i) z[i] = lpS[i].lp (y[i], aDamp) * gain[i];
            // mezcla Hadamard (ortogonal)
            for (int step = 1; step < 8; step <<= 1)
                for (int i = 0; i < 8; i += step * 2)
                    for (int j = i; j < i + step; ++j)
                    {
                        const float a = z[j], b = z[j + step];
                        z[j] = a + b; z[j + step] = a - b;
                    }
            for (int i = 0; i < 8; ++i) line[i].write (u * 0.3f + z[i] * 0.35355339f);
            float oL = (y[0] + y[2] + y[4] + y[6]) * 0.5f;
            float oR = (y[1] + y[3] + y[5] + y[7]) * 0.5f;
            oL -= lowS[0].lp (oL, aLow);                                    // control de graves (low cut)
            oR -= lowS[1].lp (oR, aLow);
            L[s] = oL; R[s] = oR;
        }
    }

private:
    AP algo{}, size{}, decay{}, pre{}, damp{}, mod{}, lo{};
    float sr = 44100.f, phase = 0.f;
    DelayLine line[8], pdl, ap[2];
    OnePole lpS[8], lowS[2];
};

// ------------------------------------------------------------------ Compresor (con modo multibanda)
class CompressorFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { thr = g ("comp_thr"); ratio = g ("comp_ratio"); att = g ("comp_att"); rel = g ("comp_rel");
                                        gain = g ("comp_gain"); multi = g ("comp_multi"); }
    void prepare (double s) override { sr = (float) s; reset(); }
    void reset() override { for (auto& e : env) e = 0.f; for (auto& x : xo) x = Xover{}; }

    void process (float* L, float* R, int n, double) override
    {
        const float th = thr->load(), ra = ratio->load();
        const float ca = std::exp (-1.f / (std::max (0.1f, att->load()) * 0.001f * sr));
        const float cr = std::exp (-1.f / (std::max (5.f, rel->load()) * 0.001f * sr));
        const float mk = dbToGain (gain->load());
        const bool  mb = multi->load() > 0.5f;
        if (mb) { xo[0].setup (200.f, sr); xo[1].setup (2500.f, sr); }
        for (int s = 0; s < n; ++s)
        {
            if (! mb)
            {
                const float g = gr (0, std::max (std::abs (L[s]), std::abs (R[s])), th, ra, ca, cr);
                L[s] *= g * mk; R[s] *= g * mk;
            }
            else
            {
                float lo[2], hi[2], mid[2], x[2] = { L[s], R[s] };
                for (int c = 0; c < 2; ++c)
                {
                    lo[c]  = xo[0].low (c, x[c]);
                    hi[c]  = xo[1].high (c, x[c]);
                    mid[c] = x[c] - lo[c] - hi[c];
                }
                const float g0 = gr (0, std::max (std::abs (lo[0]),  std::abs (lo[1])),  th, ra, ca, cr);
                const float g1 = gr (1, std::max (std::abs (mid[0]), std::abs (mid[1])), th, ra, ca, cr);
                const float g2 = gr (2, std::max (std::abs (hi[0]),  std::abs (hi[1])),  th, ra, ca, cr);
                L[s] = (lo[0] * g0 + mid[0] * g1 + hi[0] * g2) * mk;
                R[s] = (lo[1] * g0 + mid[1] * g1 + hi[1] * g2) * mk;
            }
        }
    }

    // reduccion de ganancia: rodilla suave de 6 dB
    float gr (int band, float level, float th, float ra, float ca, float cr)
    {
        float& e = env[band];
        e = level > e ? ca * e + (1.f - ca) * level : cr * e + (1.f - cr) * level;
        const float db = 6.0206f * std::log2 (std::max (e, 1e-6f));
        const float over = db - th;
        float red = 0.f;
        if (2.f * over > 6.f)       red = (1.f / ra - 1.f) * over;
        else if (2.f * over > -6.f) red = (1.f / ra - 1.f) * (over + 3.f) * (over + 3.f) / 12.f;
        return dbToGain (red);
    }

private:
    // Cruces Linkwitz-Riley de 4.o orden
    struct Xover
    {
        SvfCoeffs c;
        SvfState lp1[2], lp2[2], hp1[2], hp2[2];
        void setup (float f, float sr) { c.set (f, 0.3004f, sr); }          // k = 1.414 (Butterworth)
        float low (int ch, float x)
        {
            float lp, bp, hp;
            svfTick (c, lp1[ch], x, lp, bp, hp);  const float a = lp;
            svfTick (c, lp2[ch], a, lp, bp, hp);  return lp;
        }
        float high (int ch, float x)
        {
            float lp, bp, hp;
            svfTick (c, hp1[ch], x, lp, bp, hp);  const float a = hp;
            svfTick (c, hp2[ch], a, lp, bp, hp);  return hp;
        }
    };
    AP thr{}, ratio{}, att{}, rel{}, gain{}, multi{};
    float sr = 44100.f, env[3] = {};
    Xover xo[2];
};

// ------------------------------------------------------------------ EQ de 4 bandas
struct Biquad
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
    // tipo: 0 off, 1 low shelf, 2 high shelf, 3 peaking, 4 notch, 5 band pass
    static Biquad make (int type, float f, float gainDb, float q, float sr)
    {
        Biquad r;
        if (type <= 0) return r;
        f = std::clamp (f, 20.f, sr * 0.45f);
        q = std::max (0.1f, q);
        const float A = std::pow (10.f, gainDb / 40.f);
        const float w0 = kTwoPi * f / sr, cw = std::cos (w0), sw = std::sin (w0);
        const float alpha = sw / (2.f * q), beta = 2.f * std::sqrt (A) * alpha;
        float B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;
        switch (type)
        {
            case 1: B0 = A * ((A + 1) - (A - 1) * cw + beta); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - beta);
                    A0 = (A + 1) + (A - 1) * cw + beta; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - beta; break;
            case 2: B0 = A * ((A + 1) + (A - 1) * cw + beta); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - beta);
                    A0 = (A + 1) - (A - 1) * cw + beta; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - beta; break;
            case 3: B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break;
            case 4: B0 = 1; B1 = -2 * cw; B2 = 1; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
            default: B0 = alpha; B1 = 0; B2 = -alpha; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;
        }
        r.b0 = B0 / A0; r.b1 = B1 / A0; r.b2 = B2 / A0; r.a1 = A1 / A0; r.a2 = A2 / A0;
        return r;
    }
    // respuesta en magnitud (lineal) a la frecuencia f
    float magnitude (float f, float sr) const
    {
        const float w = kTwoPi * f / sr;
        const std::complex<float> z1 = std::polar (1.f, -w), z2 = std::polar (1.f, -2.f * w);
        const std::complex<float> num = b0 + b1 * z1 + b2 * z2, den = 1.f + a1 * z1 + a2 * z2;
        return std::abs (num / den);
    }
};

class EqFx : public FxBase
{
public:
    void bind (const ParamGetter& g)
    {
        for (int b = 0; b < 4; ++b)
        {
            const std::string p = "eq_b" + std::to_string (b + 1) + "_";
            type[b] = g ((p + "type").c_str()); freq[b] = g ((p + "freq").c_str());
            gain[b] = g ((p + "gain").c_str()); q[b]    = g ((p + "q").c_str());
        }
    }
    void prepare (double s) override { sr = (float) s; reset(); }
    void reset() override { std::fill (&z[0][0][0], &z[0][0][0] + 16, 0.f); }

    void process (float* L, float* R, int n, double) override
    {
        Biquad bq[4];
        for (int b = 0; b < 4; ++b) bq[b] = Biquad::make ((int) type[b]->load(), freq[b]->load(), gain[b]->load(), q[b]->load(), sr);
        float* io[2] = { L, R };
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < n; ++s)
            {
                float x = io[c][s];
                for (int b = 0; b < 4; ++b)
                {
                    if ((int) type[b]->load() <= 0) continue;
                    const float y = bq[b].b0 * x + z[c][b][0];
                    z[c][b][0] = bq[b].b1 * x - bq[b].a1 * y + z[c][b][1];
                    z[c][b][1] = bq[b].b2 * x - bq[b].a2 * y;
                    x = y;
                }
                io[c][s] = x;
            }
    }

private:
    AP type[4] = {}, freq[4] = {}, gain[4] = {}, q[4] = {};
    float sr = 44100.f, z[2][4][2] = {};
};

// ------------------------------------------------------------------ Utility
class UtilityFx : public FxBase
{
public:
    void bind (const ParamGetter& g) { polL = g ("util_polL"); polR = g ("util_polR"); lpf = g ("util_lpf"); hpf = g ("util_hpf");
                                        mono = g ("util_mono"); mbass = g ("util_mbass"); mfreq = g ("util_mfreq");
                                        width = g ("util_width"); pan = g ("util_pan"); gain = g ("util_gain"); }
    void prepare (double s) override { sr = (float) s; reset(); }
    void reset() override { for (auto& c : st) for (auto& v : c) v = SvfState{}; for (auto& o : lowS) o = OnePole{}; }

    void process (float* L, float* R, int n, double) override
    {
        const float sl = polL->load() > 0.5f ? -1.f : 1.f, sr_ = polR->load() > 0.5f ? -1.f : 1.f;
        const bool useLp = lpf->load() < 19000.f, useHp = hpf->load() > 22.f;
        if (useLp) cl.set (lpf->load(), 0.3f, sr);
        if (useHp) ch.set (hpf->load(), 0.3f, sr);
        const bool mn = mono->load() > 0.5f, mb = mbass->load() > 0.5f;
        const float am = OnePole::coef (mfreq->load(), sr);
        const float w = width->load();
        const float p = pan->load();
        const float ang = (p + 1.f) * 0.25f * kPi;
        const float pl = std::cos (ang) * 1.41421356f, pr = std::sin (ang) * 1.41421356f;
        const float g = dbToGain (gain->load());
        for (int s = 0; s < n; ++s)
        {
            float l = L[s] * sl, r = R[s] * sr_;
            float lp, bp, hp;
            if (useLp) { svfTick (cl, st[0][0], l, lp, bp, hp); l = lp; svfTick (cl, st[1][0], r, lp, bp, hp); r = lp; }
            if (useHp) { svfTick (ch, st[0][1], l, lp, bp, hp); l = hp; svfTick (ch, st[1][1], r, lp, bp, hp); r = hp; }
            float m = 0.5f * (l + r), sd = 0.5f * (l - r);
            if (mn) sd = 0.f;
            if (mb) { const float lowS_ = lowS[0].lp (sd, am); sd -= lowS_; }   // graves en mono
            sd *= w;
            l = (m + sd) * pl * g; r = (m - sd) * pr * g;
            L[s] = l; R[s] = r;
        }
    }

private:
    AP polL{}, polR{}, lpf{}, hpf{}, mono{}, mbass{}, mfreq{}, width{}, pan{}, gain{};
    float sr = 44100.f;
    SvfCoeffs cl, ch;
    SvfState st[2][2];
    OnePole lowS[2];
};

// ------------------------------------------------------------------ Cadena de efectos
class FxChain
{
public:
    // convolve: efecto externo (usa juce::dsp) que crea el procesador; puede ser nullptr
    void bind (const ParamGetter& g, FxBase* convolveFx)
    {
        hyper.bind (g); dist.bind (g); bode.bind (g); chorus.bind (g); flanger.bind (g); phaser.bind (g);
        delay.bind (g); reverb.bind (g); comp.bind (g); eq.bind (g); util.bind (g);
        fx[Fx::Hyper] = &hyper; fx[Fx::Distortion] = &dist; fx[Fx::Bode] = &bode; fx[Fx::Chorus] = &chorus; fx[Fx::Flanger] = &flanger;
        fx[Fx::Phaser] = &phaser; fx[Fx::Delay] = &delay; fx[Fx::Reverb] = &reverb; fx[Fx::Convolve] = convolveFx;
        fx[Fx::Compressor] = &comp; fx[Fx::Eq] = &eq; fx[Fx::Utility] = &util;
        for (int i = 0; i < kNumFx; ++i)
        {
            on[i]  = g ((std::string (kFxPrefix[i]) + "_on").c_str());
            mix[i] = g ((std::string (kFxPrefix[i]) + "_mix").c_str());     // el EQ no tiene mezcla (nullptr)
        }
    }

    void prepare (double sr)
    {
        for (auto* f : fx) if (f != nullptr) f->prepare (sr);
        for (int i = 0; i < kNumFx; ++i) { wasOn[i] = false; lastMix[i] = 1.f; }
    }

    void process (float* L, float* R, int n, double bpm)
    {
        for (int pos = 0; pos < n; pos += kFxChunk)
        {
            const int m = std::min (kFxChunk, n - pos);
            for (int i = 0; i < kNumFx; ++i)
            {
                const bool isOn = on[i] != nullptr && on[i]->load() > 0.5f && fx[i] != nullptr;
                if (! isOn)
                {
                    wasOn[i] = false;
                    meter[i][0].store (0.f, std::memory_order_relaxed); meter[i][1].store (0.f, std::memory_order_relaxed);
                    continue;
                }
                float* l = L + pos; float* r = R + pos;
                const float target = mix[i] != nullptr ? mix[i]->load() : 1.f;
                if (! wasOn[i]) { fx[i]->reset(); lastMix[i] = target; wasOn[i] = true; }
                std::copy (l, l + m, dryL); std::copy (r, r + m, dryR);
                fx[i]->process (l, r, m, bpm);
                const float m0 = lastMix[i];
                float pkL = 0.f, pkR = 0.f;
                for (int s = 0; s < m; ++s)
                {
                    const float mm = m0 + (target - m0) * (float) (s + 1) / (float) m;
                    l[s] = dryL[s] + (l[s] - dryL[s]) * mm;
                    r[s] = dryR[s] + (r[s] - dryR[s]) * mm;
                    pkL = std::max (pkL, std::abs (l[s])); pkR = std::max (pkR, std::abs (r[s]));
                }
                lastMix[i] = target;
                meter[i][0].store (pkL, std::memory_order_relaxed);
                meter[i][1].store (pkR, std::memory_order_relaxed);
            }
        }
    }

    bool anyOn() const
    {
        for (int i = 0; i < kNumFx; ++i) if (on[i] != nullptr && on[i]->load() > 0.5f) return true;
        return false;
    }

    std::atomic<float> meter[kNumFx][2];

    FxChain() { for (auto& m : meter) { m[0].store (0.f); m[1].store (0.f); } }

private:
    HyperFx hyper; DistortionFx dist; BodeFx bode; ChorusFx chorus; FlangerFx flanger; PhaserFx phaser;
    DelayFx delay; ReverbFx reverb; CompressorFx comp; EqFx eq; UtilityFx util;
    FxBase* fx[kNumFx] = {};
    AP on[kNumFx] = {}, mix[kNumFx] = {};
    bool wasOn[kNumFx] = {};
    float lastMix[kNumFx] = {};
    float dryL[kFxChunk] = {}, dryR[kFxChunk] = {};
};
}
