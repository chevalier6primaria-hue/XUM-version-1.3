#pragma once
#include "Fx/FxParams.h"
#include <utility>
#include <vector>

// Presets de cada efecto. Al elegir uno se restablecen primero todos los parametros del modulo a su
// valor por defecto y despues se aplican los valores de la lista (ids de Fx/FxParams.h).
namespace xum
{
struct FxPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> v;
};

inline const std::vector<FxPreset>& fxPresets (int fx)
{
    static const std::vector<std::vector<FxPreset>> all = {
        // ---- Hyper / Dimension
        { { "Hyper Wide",   { { "hyp_mode", 0 }, { "hyp_rate", 0.5f }, { "hyp_depth", 0.6f }, { "hyp_voices", 5 }, { "hyp_detune", 0.6f }, { "hyp_mix", 0.5f } } },
          { "Hyper Lush",   { { "hyp_mode", 0 }, { "hyp_rate", 0.3f }, { "hyp_depth", 0.8f }, { "hyp_voices", 7 }, { "hyp_detune", 0.8f }, { "hyp_mix", 0.6f } } },
          { "Hyper Subtle", { { "hyp_mode", 0 }, { "hyp_rate", 0.8f }, { "hyp_depth", 0.3f }, { "hyp_voices", 3 }, { "hyp_detune", 0.3f }, { "hyp_mix", 0.3f } } },
          { "Dimension Classic", { { "hyp_mode", 1 }, { "hyp_rate", 0.4f }, { "hyp_depth", 0.3f }, { "hyp_size", 0.5f }, { "hyp_mix", 0.5f } } },
          { "Dimension Huge",    { { "hyp_mode", 1 }, { "hyp_rate", 0.25f }, { "hyp_depth", 0.6f }, { "hyp_size", 0.9f }, { "hyp_mix", 0.6f } } } },
        // ---- Distortion
        { { "Warm Tube",      { { "dist_type", 0 }, { "dist_drive", 0.35f }, { "dist_asym", 0.3f } } },
          { "Crunch Overdrive", { { "dist_type", 7 }, { "dist_drive", 0.5f }, { "dist_asym", 0.1f }, { "dist_fpos", 2 }, { "dist_ftype", 0 }, { "dist_ffreq", 6000 } } },
          { "Fuzz Wall",      { { "dist_type", 9 }, { "dist_drive", 0.8f }, { "dist_fpos", 1 }, { "dist_ftype", 1 }, { "dist_ffreq", 300 } } },
          { "Foldback Metal", { { "dist_type", 5 }, { "dist_drive", 0.7f }, { "dist_mix", 0.7f } } },
          { "Rectified Grit", { { "dist_type", 6 }, { "dist_drive", 0.5f }, { "dist_mix", 0.5f } } },
          { "Bass Saturator", { { "dist_type", 1 }, { "dist_drive", 0.45f }, { "dist_fpos", 1 }, { "dist_ftype", 0 }, { "dist_ffreq", 400 }, { "dist_mix", 0.6f } } } },
        // ---- Bode
        { { "Dark Bell",   { { "bode_shift", 70 }, { "bode_dir", 2 }, { "bode_feed", 0 }, { "bode_mix", 0.5f } } },
          { "Sci-Fi Rise", { { "bode_shift", 400 }, { "bode_dir", 0 }, { "bode_feed", 0.5f }, { "bode_mix", 0.6f } } },
          { "Metal Hit",   { { "bode_shift", 180 }, { "bode_dir", 2 }, { "bode_feed", 0.3f }, { "bode_mix", 0.7f } } },
          { "Slow Detune", { { "bode_shift", 3 }, { "bode_dir", 0 }, { "bode_mix", 0.5f } } },
          { "Barberpole",  { { "bode_shift", 40 }, { "bode_dir", 1 }, { "bode_feed", 0.7f }, { "bode_mix", 0.6f } } } },
        // ---- Chorus
        { { "Classic",     { { "cho_voices", 2 }, { "cho_rate", 0.5f }, { "cho_depth", 0.5f }, { "cho_delay", 12 }, { "cho_fb", 0.1f } } },
          { "Wide 4 Voices", { { "cho_voices", 4 }, { "cho_rate", 0.3f }, { "cho_depth", 0.6f }, { "cho_delay", 15 }, { "cho_fb", 0.05f } } },
          { "Dark Chorus", { { "cho_voices", 3 }, { "cho_rate", 0.25f }, { "cho_depth", 0.4f }, { "cho_delay", 18 }, { "cho_lpf", 2500 }, { "cho_fb", 0.3f } } },
          { "Vibrato",     { { "cho_voices", 1 }, { "cho_rate", 5 }, { "cho_depth", 0.3f }, { "cho_delay", 6 }, { "cho_fb", 0 }, { "cho_mix", 1.0f } } } },
        // ---- Flanger
        { { "Jet",           { { "fla_delay", 2 }, { "fla_depth", 0.8f }, { "fla_rate", 0.15f }, { "fla_fb", 0.75f } } },
          { "Metallic Negative", { { "fla_delay", 1.2f }, { "fla_depth", 0.5f }, { "fla_rate", 0.3f }, { "fla_fb", 0.8f }, { "fla_inv", 1 } } },
          { "Slow Sweep",    { { "fla_delay", 4 }, { "fla_depth", 0.9f }, { "fla_rate", 0.08f }, { "fla_fb", 0.5f } } },
          { "Tempo Sweep",   { { "fla_sync", 1 }, { "fla_div", 0 }, { "fla_delay", 3 }, { "fla_depth", 0.8f }, { "fla_fb", 0.6f } } } },
        // ---- Phaser
        { { "Classic 4",  { { "pha_stages", 4 }, { "pha_freq", 700 }, { "pha_fb", 0.3f }, { "pha_rate", 0.4f }, { "pha_depth", 0.6f }, { "pha_offset", 0.3f } } },
          { "Deep 12",    { { "pha_stages", 12 }, { "pha_freq", 600 }, { "pha_fb", 0.6f }, { "pha_rate", 0.2f }, { "pha_depth", 0.8f }, { "pha_offset", 0.5f } } },
          { "Swirl 32",   { { "pha_stages", 32 }, { "pha_freq", 900 }, { "pha_fb", 0.7f }, { "pha_rate", 0.15f }, { "pha_depth", 0.9f }, { "pha_offset", 0.7f } } },
          { "Fast Throb", { { "pha_stages", 8 }, { "pha_rate", 3 }, { "pha_depth", 0.5f }, { "pha_fb", 0.4f } } } },
        // ---- Delay
        { { "Eighth Echo",    { { "del_sync", 1 }, { "del_divL", 3 }, { "del_divR", 3 }, { "del_fb", 0.4f }, { "del_hp", 100 }, { "del_lp", 9000 } } },
          { "Triplet Ping-Pong", { { "del_sync", 1 }, { "del_divL", 7 }, { "del_divR", 7 }, { "del_pp", 1 }, { "del_fb", 0.5f } } },
          { "Slapback",       { { "del_sync", 0 }, { "del_timeL", 110 }, { "del_timeR", 110 }, { "del_fb", 0.15f }, { "del_hp", 150 }, { "del_lp", 6000 }, { "del_mix", 0.25f } } },
          { "Tape Dub",       { { "del_sync", 0 }, { "del_timeL", 420 }, { "del_timeR", 560 }, { "del_fb", 0.6f }, { "del_hp", 200 }, { "del_lp", 3500 }, { "del_sat", 0.5f }, { "del_mix", 0.35f } } },
          { "Long Quarter",   { { "del_sync", 1 }, { "del_divL", 2 }, { "del_divR", 2 }, { "del_fb", 0.55f }, { "del_lp", 7000 } } } },
        // ---- Reverb
        { { "Small Room",   { { "rev_algo", 1 }, { "rev_size", 0.3f }, { "rev_decay", 0.8f }, { "rev_pre", 5 }, { "rev_damp", 0.5f }, { "rev_mod", 0.1f }, { "rev_lo", 80 }, { "rev_mix", 0.25f } } },
          { "Bright Plate", { { "rev_algo", 0 }, { "rev_size", 0.5f }, { "rev_decay", 1.8f }, { "rev_pre", 10 }, { "rev_damp", 0.25f }, { "rev_mod", 0.25f }, { "rev_lo", 120 }, { "rev_mix", 0.3f } } },
          { "Big Hall",     { { "rev_algo", 2 }, { "rev_size", 0.8f }, { "rev_decay", 4.5f }, { "rev_pre", 25 }, { "rev_damp", 0.4f }, { "rev_mod", 0.3f }, { "rev_lo", 60 }, { "rev_mix", 0.35f } } },
          { "Nitrous",      { { "rev_algo", 3 }, { "rev_size", 0.7f }, { "rev_decay", 6 }, { "rev_pre", 15 }, { "rev_damp", 0.3f }, { "rev_mod", 0.6f }, { "rev_lo", 100 }, { "rev_mix", 0.4f } } },
          { "Vintage Chamber", { { "rev_algo", 4 }, { "rev_size", 0.5f }, { "rev_decay", 2.2f }, { "rev_pre", 12 }, { "rev_damp", 0.7f }, { "rev_mod", 0.1f }, { "rev_lo", 150 }, { "rev_mix", 0.3f } } },
          { "Deep Basin",   { { "rev_algo", 5 }, { "rev_size", 0.9f }, { "rev_decay", 9 }, { "rev_pre", 30 }, { "rev_damp", 0.55f }, { "rev_mod", 0.15f }, { "rev_lo", 70 }, { "rev_mix", 0.4f } } } },
        // ---- Convolve
        { { "Pencil Tap",    { { "con_ir", 0 }, { "con_mix", 0.35f } } },
          { "Plate IR",      { { "con_ir", 1 }, { "con_mix", 0.3f } } },
          { "Hall IR",       { { "con_ir", 2 }, { "con_pre", 20 }, { "con_mix", 0.3f } } },
          { "Spring Boing",  { { "con_ir", 3 }, { "con_mix", 0.4f } } },
          { "Metal Pipe",    { { "con_ir", 4 }, { "con_mix", 0.4f } } },
          { "Glass Shimmer", { { "con_ir", 5 }, { "con_tone", 0.8f }, { "con_mix", 0.35f } } } },
        // ---- Compressor
        { { "Gentle Glue",   { { "comp_thr", -14 }, { "comp_ratio", 2 }, { "comp_att", 30 }, { "comp_rel", 200 }, { "comp_gain", 2 } } },
          { "Punchy",        { { "comp_thr", -20 }, { "comp_ratio", 4 }, { "comp_att", 15 }, { "comp_rel", 100 }, { "comp_gain", 4 } } },
          { "Limiter-ish",   { { "comp_thr", -8 }, { "comp_ratio", 20 }, { "comp_att", 0.5f }, { "comp_rel", 80 }, { "comp_gain", 3 } } },
          { "Multiband Master", { { "comp_multi", 1 }, { "comp_thr", -16 }, { "comp_ratio", 3 }, { "comp_att", 20 }, { "comp_rel", 150 }, { "comp_gain", 2 } } },
          { "Parallel Smash", { { "comp_thr", -30 }, { "comp_ratio", 10 }, { "comp_att", 5 }, { "comp_rel", 80 }, { "comp_gain", 8 }, { "comp_mix", 0.5f } } } },
        // ---- EQ
        { { "Bass Boost",   { { "eq_b1_type", 1 }, { "eq_b1_freq", 90 }, { "eq_b1_gain", 6 } } },
          { "Air",          { { "eq_b4_type", 2 }, { "eq_b4_freq", 9000 }, { "eq_b4_gain", 5 } } },
          { "Vocal Presence", { { "eq_b1_type", 1 }, { "eq_b1_freq", 120 }, { "eq_b1_gain", -4 }, { "eq_b2_type", 3 }, { "eq_b2_freq", 3000 }, { "eq_b2_gain", 4 }, { "eq_b2_q", 1 } } },
          { "Telephone",    { { "eq_b1_type", 0 }, { "eq_b2_type", 5 }, { "eq_b2_freq", 1500 }, { "eq_b2_q", 1.2f }, { "eq_b3_type", 0 }, { "eq_b4_type", 0 } } },
          { "Mid Scoop",    { { "eq_b2_type", 3 }, { "eq_b2_freq", 400 }, { "eq_b2_gain", -6 }, { "eq_b2_q", 0.8f }, { "eq_b3_type", 3 }, { "eq_b3_freq", 3500 }, { "eq_b3_gain", 3 } } } },
        // ---- Utility
        { { "Mono Bass",    { { "util_mbass", 1 }, { "util_mfreq", 150 }, { "util_width", 1.2f } } },
          { "Wide",         { { "util_width", 1.6f } } },
          { "Mono",         { { "util_mono", 1 } } },
          { "Telephone Cut", { { "util_hpf", 400 }, { "util_lpf", 3500 } } },
          { "Flip Left",    { { "util_polL", 1 } } } },
    };
    return all[(size_t) fx];
}
}
