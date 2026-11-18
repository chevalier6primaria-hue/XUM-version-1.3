#pragma once
#include <vector>

// Tabla unica de parametros de los efectos (ids, rangos, valores por defecto). La usan:
// el layout del plugin (Parameters.cpp), el motor de efectos y la interfaz.
namespace xum
{
constexpr int kNumFx = 12;
namespace Fx { enum { Hyper = 0, Distortion, Bode, Chorus, Flanger, Phaser, Delay, Reverb, Convolve, Compressor, Eq, Utility }; }

inline const char* const kFxPrefix[kNumFx] = { "hyp", "dist", "bode", "cho", "fla", "pha", "del", "rev", "con", "comp", "eq", "util" };

enum FxKind { FxFloat = 0, FxInt, FxBool, FxChoice };

struct FxP
{
    const char* id;
    const char* name;
    int kind;
    float lo, hi, def, skew;
    const char* choices;      // para FxChoice: opciones separadas por '|'
};

inline const std::vector<FxP>& fxParamTable()
{
    static const std::vector<FxP> t = {
        // ---- Hyper / Dimension
        { "hyp_on",    "Hyper On",       FxBool, 0, 1, 0, 1, "" },
        { "hyp_mix",   "Hyper Mix",      FxFloat, 0, 1, 0.5f, 1, "" },
        { "hyp_mode",  "Hyper Mode",     FxChoice, 0, 1, 0, 1, "Hyper|Dimension" },
        { "hyp_rate",  "Hyper Rate",     FxFloat, 0.05f, 10, 0.6f, 0.4f, "" },
        { "hyp_depth", "Hyper Depth",    FxFloat, 0, 1, 0.5f, 1, "" },
        { "hyp_voices","Hyper Unison",   FxInt, 2, 7, 4, 1, "" },
        { "hyp_detune","Hyper Detune",   FxFloat, 0, 1, 0.5f, 1, "" },
        { "hyp_size",  "Dimension Size", FxFloat, 0, 1, 0.5f, 1, "" },
        // ---- Distortion
        { "dist_on",   "Dist On",        FxBool, 0, 1, 0, 1, "" },
        { "dist_mix",  "Dist Mix",       FxFloat, 0, 1, 1.0f, 1, "" },
        { "dist_type", "Dist Type",      FxChoice, 0, 9, 0, 1, "Tube|Solid State|Diaper|Soft Clip|Hard Clip|Foldback|Rectify|Overdrive A|Overdrive B|Overdrive C" },
        { "dist_drive","Dist Drive",     FxFloat, 0, 1, 0.4f, 1, "" },
        { "dist_asym", "Dist Asymmetry", FxFloat, 0, 1, 0, 1, "" },
        { "dist_fpos", "Dist Filter Pos",FxChoice, 0, 2, 0, 1, "Off|Pre|Post" },
        { "dist_ftype","Dist Filter Type",FxChoice,0, 2, 0, 1, "LP|HP|BP" },
        { "dist_ffreq","Dist Filter Freq",FxFloat,20, 20000, 2000, 0.3f, "" },
        { "dist_fres", "Dist Filter Q",  FxFloat, 0, 1, 0.3f, 1, "" },
        // ---- Bode (frequency shifter)
        { "bode_on",   "Bode On",        FxBool, 0, 1, 0, 1, "" },
        { "bode_mix",  "Bode Mix",       FxFloat, 0, 1, 0.5f, 1, "" },
        { "bode_shift","Bode Shift (Hz)",FxFloat, 0, 2000, 50, 0.4f, "" },
        { "bode_dir",  "Bode Direction", FxChoice, 0, 2, 0, 1, "Up|Down|Both" },
        { "bode_feed", "Bode Feedback",  FxFloat, 0, 0.9f, 0, 1, "" },
        // ---- Chorus
        { "cho_on",    "Chorus On",      FxBool, 0, 1, 0, 1, "" },
        { "cho_mix",   "Chorus Mix",     FxFloat, 0, 1, 0.5f, 1, "" },
        { "cho_voices","Chorus Voices",  FxInt, 1, 4, 2, 1, "" },
        { "cho_rate",  "Chorus Rate",    FxFloat, 0.05f, 8, 0.4f, 0.4f, "" },
        { "cho_depth", "Chorus Depth",   FxFloat, 0, 1, 0.5f, 1, "" },
        { "cho_delay", "Chorus Delay (ms)",FxFloat,5, 30, 12, 1, "" },
        { "cho_fb",    "Chorus Feedback",FxFloat, 0, 0.8f, 0.1f, 1, "" },
        { "cho_lpf",   "Chorus LPF",     FxFloat, 200, 20000, 8000, 0.3f, "" },
        { "cho_width", "Chorus Width",   FxFloat, 0, 1, 1, 1, "" },
        // ---- Flanger
        { "fla_on",    "Flanger On",     FxBool, 0, 1, 0, 1, "" },
        { "fla_mix",   "Flanger Mix",    FxFloat, 0, 1, 0.5f, 1, "" },
        { "fla_delay", "Flanger Delay (ms)",FxFloat,0.1f, 10, 2, 0.5f, "" },
        { "fla_depth", "Flanger Depth",  FxFloat, 0, 1, 0.6f, 1, "" },
        { "fla_rate",  "Flanger Rate",   FxFloat, 0.05f, 5, 0.3f, 0.4f, "" },
        { "fla_fb",    "Flanger Feedback",FxFloat,0, 0.95f, 0.5f, 1, "" },
        { "fla_inv",   "Flanger Invert", FxBool, 0, 1, 0, 1, "" },
        { "fla_sync",  "Flanger Sync",   FxBool, 0, 1, 0, 1, "" },
        { "fla_div",   "Flanger Division",FxChoice,0, 5, 2, 1, "1/1|1/2|1/4|1/8|1/16|1/32" },
        // ---- Phaser
        { "pha_on",    "Phaser On",      FxBool, 0, 1, 0, 1, "" },
        { "pha_mix",   "Phaser Mix",     FxFloat, 0, 1, 0.5f, 1, "" },
        { "pha_stages","Phaser Stages",  FxInt, 4, 32, 8, 1, "" },
        { "pha_freq",  "Phaser Center",  FxFloat, 100, 8000, 800, 0.35f, "" },
        { "pha_fb",    "Phaser Feedback",FxFloat, 0, 0.95f, 0.5f, 1, "" },
        { "pha_rate",  "Phaser Rate",    FxFloat, 0.02f, 10, 0.3f, 0.4f, "" },
        { "pha_depth", "Phaser Depth",   FxFloat, 0, 1, 0.7f, 1, "" },
        { "pha_offset","Phaser Stereo",  FxFloat, 0, 1, 0.5f, 1, "" },
        // ---- Delay
        { "del_on",    "Delay On",       FxBool, 0, 1, 0, 1, "" },
        { "del_mix",   "Delay Mix",      FxFloat, 0, 1, 0.3f, 1, "" },
        { "del_sync",  "Delay Sync",     FxBool, 0, 1, 1, 1, "" },
        { "del_timeL", "Delay Time L (ms)",FxFloat,1, 2000, 350, 0.4f, "" },
        { "del_timeR", "Delay Time R (ms)",FxFloat,1, 2000, 350, 0.4f, "" },
        { "del_divL",  "Delay Div L",    FxChoice, 0, 8, 3, 1, "1/1|1/2|1/4|1/8|1/16|1/32|1/4T|1/8T|1/16T" },
        { "del_divR",  "Delay Div R",    FxChoice, 0, 8, 3, 1, "1/1|1/2|1/4|1/8|1/16|1/32|1/4T|1/8T|1/16T" },
        { "del_pp",    "Delay Ping-Pong",FxBool, 0, 1, 0, 1, "" },
        { "del_fb",    "Delay Feedback", FxFloat, 0, 0.95f, 0.4f, 1, "" },
        { "del_hp",    "Delay Low Cut",  FxFloat, 20, 2000, 20, 0.4f, "" },
        { "del_lp",    "Delay High Cut", FxFloat, 500, 20000, 12000, 0.3f, "" },
        { "del_sat",   "Delay Saturation",FxFloat,0, 1, 0, 1, "" },
        // ---- Reverb
        { "rev_on",    "Reverb On",      FxBool, 0, 1, 0, 1, "" },
        { "rev_mix",   "Reverb Mix",     FxFloat, 0, 1, 0.3f, 1, "" },
        { "rev_algo",  "Reverb Algorithm",FxChoice,0, 5, 0, 1, "Plate|Room|Hall|Nitrous|Vintage|Basin" },
        { "rev_size",  "Reverb Size",    FxFloat, 0, 1, 0.5f, 1, "" },
        { "rev_decay", "Reverb Decay (s)",FxFloat,0.2f, 15, 2.5f, 0.4f, "" },
        { "rev_pre",   "Reverb Pre-Delay (ms)",FxFloat,0, 200, 10, 1, "" },
        { "rev_damp",  "Reverb Damp",    FxFloat, 0, 1, 0.4f, 1, "" },
        { "rev_mod",   "Reverb Mod",     FxFloat, 0, 1, 0.2f, 1, "" },
        { "rev_lo",    "Reverb Low Cut", FxFloat, 20, 1000, 40, 0.4f, "" },
        // ---- Convolve
        { "con_on",    "Convolve On",    FxBool, 0, 1, 0, 1, "" },
        { "con_mix",   "Convolve Mix",   FxFloat, 0, 1, 0.3f, 1, "" },
        { "con_ir",    "Convolve IR",    FxChoice, 0, 6, 0, 1, "Pencil|Plate|Hall|Spring|Metal Pipe|Glass|Custom" },
        { "con_tone",  "Convolve Tone",  FxFloat, 0, 1, 1, 1, "" },
        { "con_pre",   "Convolve Pre-Delay",FxFloat,0, 200, 0, 1, "" },
        { "con_gain",  "Convolve Gain",  FxFloat, -12, 12, 0, 1, "" },
        // ---- Compressor
        { "comp_on",   "Comp On",        FxBool, 0, 1, 0, 1, "" },
        { "comp_mix",  "Comp Mix",       FxFloat, 0, 1, 1, 1, "" },
        { "comp_thr",  "Comp Threshold", FxFloat, -60, 0, -18, 1, "" },
        { "comp_ratio","Comp Ratio",     FxFloat, 1, 20, 4, 0.5f, "" },
        { "comp_att",  "Comp Attack (ms)",FxFloat,0.1f, 200, 10, 0.4f, "" },
        { "comp_rel",  "Comp Release (ms)",FxFloat,5, 1000, 120, 0.4f, "" },
        { "comp_gain", "Comp Gain",      FxFloat, 0, 24, 0, 1, "" },
        { "comp_multi","Comp Multiband", FxBool, 0, 1, 0, 1, "" },
        // ---- EQ (4 bandas)
        { "eq_on",     "EQ On",          FxBool, 0, 1, 0, 1, "" },
        { "eq_b1_type","EQ1 Type", FxChoice, 0, 5, 1, 1, "Off|Low Shelf|High Shelf|Peaking|Notch|Band Pass" },
        { "eq_b1_freq","EQ1 Freq", FxFloat, 20, 20000, 100, 0.3f, "" },
        { "eq_b1_gain","EQ1 Gain", FxFloat, -18, 18, 0, 1, "" },
        { "eq_b1_q",   "EQ1 Q",    FxFloat, 0.2f, 10, 0.7f, 0.4f, "" },
        { "eq_b2_type","EQ2 Type", FxChoice, 0, 5, 3, 1, "Off|Low Shelf|High Shelf|Peaking|Notch|Band Pass" },
        { "eq_b2_freq","EQ2 Freq", FxFloat, 20, 20000, 500, 0.3f, "" },
        { "eq_b2_gain","EQ2 Gain", FxFloat, -18, 18, 0, 1, "" },
        { "eq_b2_q",   "EQ2 Q",    FxFloat, 0.2f, 10, 0.7f, 0.4f, "" },
        { "eq_b3_type","EQ3 Type", FxChoice, 0, 5, 3, 1, "Off|Low Shelf|High Shelf|Peaking|Notch|Band Pass" },
        { "eq_b3_freq","EQ3 Freq", FxFloat, 20, 20000, 2500, 0.3f, "" },
        { "eq_b3_gain","EQ3 Gain", FxFloat, -18, 18, 0, 1, "" },
        { "eq_b3_q",   "EQ3 Q",    FxFloat, 0.2f, 10, 0.7f, 0.4f, "" },
        { "eq_b4_type","EQ4 Type", FxChoice, 0, 5, 2, 1, "Off|Low Shelf|High Shelf|Peaking|Notch|Band Pass" },
        { "eq_b4_freq","EQ4 Freq", FxFloat, 20, 20000, 8000, 0.3f, "" },
        { "eq_b4_gain","EQ4 Gain", FxFloat, -18, 18, 0, 1, "" },
        { "eq_b4_q",   "EQ4 Q",    FxFloat, 0.2f, 10, 0.7f, 0.4f, "" },
        // ---- Utility
        { "util_on",   "Utility On",     FxBool, 0, 1, 0, 1, "" },
        { "util_mix",  "Utility Mix",    FxFloat, 0, 1, 1, 1, "" },
        { "util_polL", "Utility Polarity L",FxBool,0, 1, 0, 1, "" },
        { "util_polR", "Utility Polarity R",FxBool,0, 1, 0, 1, "" },
        { "util_lpf",  "Utility LPF",    FxFloat, 20, 20000, 20000, 0.3f, "" },
        { "util_hpf",  "Utility HPF",    FxFloat, 20, 2000, 20, 0.4f, "" },
        { "util_mono", "Utility Mono",   FxBool, 0, 1, 0, 1, "" },
        { "util_mbass","Utility Mono Bass",FxBool,0, 1, 0, 1, "" },
        { "util_mfreq","Utility Mono Bass Freq",FxFloat,20, 500, 120, 1, "" },
        { "util_width","Utility Width",  FxFloat, 0, 2, 1, 1, "" },
        { "util_pan",  "Utility Pan",    FxFloat, -1, 1, 0, 1, "" },
        { "util_gain", "Utility Gain",   FxFloat, -24, 24, 0, 1, "" },
    };
    return t;
}
}
