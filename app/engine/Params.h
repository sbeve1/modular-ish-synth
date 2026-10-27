#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>

// =====================================================================
// Parameter table
//
// Every user-facing control is a parameter with metadata. The UI and the
// audio thread share a ParamStore of *normalized* (0..1) atomics: the UI
// writes, the audio thread snapshots once per block. Normalized storage
// lets the mod matrix work in knob space, like most modern synths.
// =====================================================================

enum class Map  : uint8_t { Lin, Log, Pow, Int, Choice, Bool };
enum class Unit : uint8_t { None, Pct, Hz, Sec, Ms, Db, Semi, Cent, Oct, Bpm, Pan, Voices, Ratio };
enum class Scope: uint8_t { Voice, Global };   // Voice params are modulated per voice

// Choice lists
extern const char* const kWarpModes[];
extern const char* const kSubShapes[];
extern const char* const kNoiseTypes[];
extern const char* const kFilterTypes[];
extern const char* const kLfoShapes[];
extern const char* const kLfoModes[];
extern const char* const kSyncDivs[];
extern const float       kSyncDivBeats[];
extern const char* const kDistModes[];
extern const char* const kVoiceModes[];
extern const char* const kFilterRouting[];
extern const char* const kArpModes[];

// X(ID, key, label, min, max, default, map, unit, scope, choices, skew)
#define HC_OSC_PARAMS(P, K, L, ON)                                                        \
    X(P##_ON,      K "_on",      L " On",       0, 1, ON, Map::Bool, Unit::None, Scope::Voice, nullptr, 1) \
    X(P##_POS,     K "_pos",     "WT Pos",      0, 1, 0,  Map::Lin,  Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_OCT,     K "_oct",     "Oct",        -4, 4, 0,  Map::Int,  Unit::Oct,  Scope::Voice, nullptr, 1) \
    X(P##_SEMI,    K "_semi",    "Semi",      -12,12, 0,  Map::Int,  Unit::Semi, Scope::Voice, nullptr, 1) \
    X(P##_FINE,    K "_fine",    "Fine",     -100,100,0,  Map::Lin,  Unit::Cent, Scope::Voice, nullptr, 1) \
    X(P##_LEVEL,   K "_level",   "Level",       0, 1, 0.7f,Map::Lin, Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_PAN,     K "_pan",     "Pan",        -1, 1, 0,  Map::Lin,  Unit::Pan,  Scope::Voice, nullptr, 1) \
    X(P##_UNISON,  K "_unison",  "Unison",      1,16, 1,  Map::Int,  Unit::Voices,Scope::Voice,nullptr, 1) \
    X(P##_DETUNE,  K "_detune",  "Detune",      0, 1, 0.25f,Map::Lin,Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_BLEND,   K "_blend",   "Blend",       0, 1, 0.75f,Map::Lin,Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_WIDTH,   K "_width",   "Width",       0, 1, 0.8f,Map::Lin, Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_PHASE,   K "_phase",   "Phase",       0, 1, 0,  Map::Lin,  Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_RAND,    K "_rand",    "Rand",        0, 1, 1,  Map::Lin,  Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_WARP,    K "_warp",    "Warp",        0, 5, 0,  Map::Choice,Unit::None,Scope::Voice, kWarpModes, 1) \
    X(P##_WARPAMT, K "_warpamt", "Warp Amt",    0, 1, 0,  Map::Lin,  Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_FILTER,  K "_filter",  "To Filter",   0, 1, 1,  Map::Bool, Unit::None, Scope::Voice, nullptr, 1)

#define HC_ENV_PARAMS(P, K)                                                               \
    X(P##_ATK,    K "_atk",    "Attack",  0, 12, 0.004f, Map::Pow, Unit::Sec, Scope::Voice, nullptr, 3) \
    X(P##_HOLD,   K "_hold",   "Hold",    0, 4,  0,      Map::Pow, Unit::Sec, Scope::Voice, nullptr, 3) \
    X(P##_DEC,    K "_dec",    "Decay",   0, 12, 0.6f,   Map::Pow, Unit::Sec, Scope::Voice, nullptr, 3) \
    X(P##_SUS,    K "_sus",    "Sustain", 0, 1,  0.8f,   Map::Lin, Unit::Pct, Scope::Voice, nullptr, 1) \
    X(P##_REL,    K "_rel",    "Release", 0, 12, 0.25f,  Map::Pow, Unit::Sec, Scope::Voice, nullptr, 3) \
    X(P##_ACRV,   K "_acrv",   "A Curve",-1, 1,  0,      Map::Lin, Unit::None,Scope::Voice, nullptr, 1) \
    X(P##_DCRV,   K "_dcrv",   "D Curve",-1, 1, -0.4f,   Map::Lin, Unit::None,Scope::Voice, nullptr, 1) \
    X(P##_RCRV,   K "_rcrv",   "R Curve",-1, 1, -0.4f,   Map::Lin, Unit::None,Scope::Voice, nullptr, 1)

#define HC_LFO_PARAMS(P, K)                                                               \
    X(P##_SHAPE,  K "_shape",  "Shape",   0, 7, 0,    Map::Choice, Unit::None, Scope::Voice, kLfoShapes, 1) \
    X(P##_RATE,   K "_rate",   "Rate", 0.02f, 40, 2,  Map::Log,    Unit::Hz,   Scope::Voice, nullptr, 1) \
    X(P##_SYNC,   K "_sync",   "Sync",    0, 1, 0,    Map::Bool,   Unit::None, Scope::Voice, nullptr, 1) \
    X(P##_DIV,    K "_div",    "Division",0, 14, 6,   Map::Choice, Unit::None, Scope::Voice, kSyncDivs, 1) \
    X(P##_MODE,   K "_mode",   "Mode",    0, 2, 0,    Map::Choice, Unit::None, Scope::Voice, kLfoModes, 1) \
    X(P##_PHASE,  K "_phase",  "Phase",   0, 1, 0,    Map::Lin,    Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_RISE,   K "_rise",   "Rise",    0, 5, 0,    Map::Pow,    Unit::Sec,  Scope::Voice, nullptr, 3) \
    X(P##_SMOOTH, K "_smooth", "Smooth",  0, 1, 0,    Map::Lin,    Unit::Pct,  Scope::Voice, nullptr, 1)

#define HC_FILTER_PARAMS(P, K, L, ON, TYPE, CUT)                                          \
    X(P##_ON,     K "_on",     L " On",   0, 1, ON,   Map::Bool,   Unit::None, Scope::Voice, nullptr, 1) \
    X(P##_TYPE,   K "_type",   "Type",    0, 7, TYPE, Map::Choice, Unit::None, Scope::Voice, kFilterTypes, 1) \
    X(P##_CUTOFF, K "_cutoff", "Cutoff", 20, 20000, CUT, Map::Log, Unit::Hz,   Scope::Voice, nullptr, 1) \
    X(P##_RES,    K "_res",    "Res",     0, 1, 0.1f, Map::Lin,    Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_DRIVE,  K "_drive",  "Drive",   0, 1, 0,    Map::Lin,    Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_KEYTRK, K "_keytrk", "Key Trk", 0, 1, 0,    Map::Lin,    Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(P##_MIX,    K "_mix",    "Mix",     0, 1, 1,    Map::Lin,    Unit::Pct,  Scope::Voice, nullptr, 1)

#define HC_PARAM_LIST                                                                     \
    /* ---- oscillators ---- */                                                           \
    HC_OSC_PARAMS(OSCA, "osca", "Osc A", 1)                                               \
    HC_OSC_PARAMS(OSCB, "oscb", "Osc B", 0)                                               \
    X(SUB_ON,     "sub_on",     "Sub On",   0, 1, 0, Map::Bool,   Unit::None, Scope::Voice, nullptr, 1) \
    X(SUB_SHAPE,  "sub_shape",  "Shape",    0, 3, 0, Map::Choice, Unit::None, Scope::Voice, kSubShapes, 1) \
    X(SUB_OCT,    "sub_oct",    "Octave",  -2, 0,-1, Map::Int,    Unit::Oct,  Scope::Voice, nullptr, 1) \
    X(SUB_LEVEL,  "sub_level",  "Level",    0, 1, 0.5f,Map::Lin,  Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(SUB_PAN,    "sub_pan",    "Pan",     -1, 1, 0, Map::Lin,    Unit::Pan,  Scope::Voice, nullptr, 1) \
    X(SUB_FILTER, "sub_filter", "To Filter",0, 1, 1, Map::Bool,   Unit::None, Scope::Voice, nullptr, 1) \
    X(NOISE_ON,   "noise_on",   "Noise On", 0, 1, 0, Map::Bool,   Unit::None, Scope::Voice, nullptr, 1) \
    X(NOISE_TYPE, "noise_type", "Type",     0, 2, 0, Map::Choice, Unit::None, Scope::Voice, kNoiseTypes, 1) \
    X(NOISE_LEVEL,"noise_level","Level",    0, 1, 0.25f,Map::Lin, Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(NOISE_PAN,  "noise_pan",  "Pan",     -1, 1, 0, Map::Lin,    Unit::Pan,  Scope::Voice, nullptr, 1) \
    X(NOISE_FILTER,"noise_filter","To Filter",0,1,1, Map::Bool,   Unit::None, Scope::Voice, nullptr, 1) \
    /* ---- filter ---- */                                                                \
    HC_FILTER_PARAMS(FLT,  "flt",  "Filter 1", 1, 1, 9000)                              \
    HC_FILTER_PARAMS(FLT2, "flt2", "Filter 2", 0, 4, 120)                               \
    X(FLT_ROUTING,"flt_routing","Routing",  0, 1, 0,    Map::Choice, Unit::None, Scope::Voice, kFilterRouting, 1) \
    /* ---- envelopes & LFOs ---- */                                                      \
    HC_ENV_PARAMS(ENV1, "env1")                                                           \
    HC_ENV_PARAMS(ENV2, "env2")                                                           \
    HC_ENV_PARAMS(ENV3, "env3")                                                           \
    HC_LFO_PARAMS(LFO1, "lfo1")                                                           \
    HC_LFO_PARAMS(LFO2, "lfo2")                                                           \
    HC_LFO_PARAMS(LFO3, "lfo3")                                                           \
    HC_LFO_PARAMS(LFO4, "lfo4")                                                           \
    /* ---- voicing ---- */                                                               \
    X(VEL_SENS,   "vel_sens",   "Velocity", 0, 1, 0.6f, Map::Lin, Unit::Pct,  Scope::Voice, nullptr, 1) \
    X(PB_RANGE,   "pb_range",   "Bend",     0, 24, 2,   Map::Int, Unit::Semi, Scope::Voice, nullptr, 1) \
    X(GLIDE,      "glide",      "Glide",    0, 2, 0,    Map::Pow, Unit::Sec,  Scope::Voice, nullptr, 3) \
    /* ---- global (not per-voice) ---- */                                                \
    X(MACRO1,     "macro1",     "Macro 1",  0, 1, 0,    Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(MACRO2,     "macro2",     "Macro 2",  0, 1, 0,    Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(MACRO3,     "macro3",     "Macro 3",  0, 1, 0,    Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(MACRO4,     "macro4",     "Macro 4",  0, 1, 0,    Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(VOICE_MODE, "voice_mode", "Mode",     0, 2, 0,    Map::Choice, Unit::None, Scope::Global, kVoiceModes, 1) \
    X(POLYPHONY,  "polyphony",  "Voices",   1, 16, 8,   Map::Int, Unit::Voices,Scope::Global, nullptr, 1) \
    X(TEMPO,      "tempo",      "Tempo",   40, 240, 120,Map::Lin, Unit::Bpm,  Scope::Global, nullptr, 1) \
    X(DIST_ON,    "dist_on",    "Distortion",0, 1, 0,   Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(DIST_MODE,  "dist_mode",  "Mode",     0, 4, 0,    Map::Choice, Unit::None, Scope::Global, kDistModes, 1) \
    X(DIST_DRIVE, "dist_drive", "Drive",    0, 1, 0.35f,Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(DIST_TONE,  "dist_tone",  "Tone",     0, 1, 0.7f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(DIST_MIX,   "dist_mix",   "Mix",      0, 1, 1,    Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(CHO_ON,     "cho_on",     "Chorus",   0, 1, 0,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(CHO_RATE,   "cho_rate",   "Rate",  0.05f, 5, 0.5f,Map::Log, Unit::Hz,   Scope::Global, nullptr, 1) \
    X(CHO_DEPTH,  "cho_depth",  "Depth",    0, 1, 0.5f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(CHO_FB,     "cho_fb",     "Feedback", 0, 0.9f, 0.15f,Map::Lin,Unit::Pct,Scope::Global, nullptr, 1) \
    X(CHO_MIX,    "cho_mix",    "Mix",      0, 1, 0.5f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(DLY_ON,     "dly_on",     "Delay",    0, 1, 0,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(DLY_SYNC,   "dly_sync",   "Sync",     0, 1, 1,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(DLY_DIV,    "dly_div",    "Division", 0, 14, 10,  Map::Choice, Unit::None, Scope::Global, kSyncDivs, 1) \
    X(DLY_TIME,   "dly_time",   "Time",     1, 2000, 375,Map::Log, Unit::Ms,  Scope::Global, nullptr, 1) \
    X(DLY_FB,     "dly_fb",     "Feedback", 0, 0.95f, 0.35f,Map::Lin,Unit::Pct,Scope::Global, nullptr, 1) \
    X(DLY_PING,   "dly_ping",   "Ping-Pong",0, 1, 1,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(DLY_TONE,   "dly_tone",   "Tone",     0, 1, 0.6f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(DLY_MIX,    "dly_mix",    "Mix",      0, 1, 0.25f,Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(REV_ON,     "rev_on",     "Reverb",   0, 1, 0,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(REV_SIZE,   "rev_size",   "Size",     0, 1, 0.6f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(REV_DECAY,  "rev_decay",  "Decay", 0.2f, 20, 2.5f,Map::Log, Unit::Sec,  Scope::Global, nullptr, 1) \
    X(REV_DAMP,   "rev_damp",   "Damping",  0, 1, 0.45f,Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(REV_PREDLY, "rev_predly", "Pre-Delay",0, 200, 12, Map::Lin, Unit::Ms,   Scope::Global, nullptr, 1) \
    X(REV_WIDTH,  "rev_width",  "Width",    0, 1, 1,    Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(REV_MIX,    "rev_mix",    "Mix",      0, 1, 0.25f,Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(ARP_ON,     "arp_on",     "Arpeggiator",0, 1, 0,  Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(ARP_MODE,   "arp_mode",   "Mode",     0, 5, 0,    Map::Choice, Unit::None, Scope::Global, kArpModes, 1) \
    X(ARP_DIV,    "arp_div",    "Rate",     0, 14, 12,  Map::Choice, Unit::None, Scope::Global, kSyncDivs, 1) \
    X(ARP_OCT,    "arp_oct",    "Octaves",  1, 4, 1,    Map::Int, Unit::Voices, Scope::Global, nullptr, 1) \
    X(ARP_GATE,   "arp_gate",   "Gate",  0.05f, 1, 0.6f,Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(ARP_SWING,  "arp_swing",  "Swing",    0, 0.6f, 0, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(ARP_HOLD,   "arp_hold",   "Hold",     0, 1, 0,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(EQ_ON,      "eq_on",      "EQ",       0, 1, 0,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(EQ_LOW,     "eq_low",     "Low",    -18, 18, 0,   Map::Lin, Unit::Db,   Scope::Global, nullptr, 1) \
    X(EQ_LOWFREQ, "eq_lowfreq", "Low Freq",30, 1000, 120,Map::Log, Unit::Hz,  Scope::Global, nullptr, 1) \
    X(EQ_MID,     "eq_mid",     "Mid",    -18, 18, 0,   Map::Lin, Unit::Db,   Scope::Global, nullptr, 1) \
    X(EQ_MIDFREQ, "eq_midfreq", "Mid Freq",150, 8000, 1000,Map::Log, Unit::Hz, Scope::Global, nullptr, 1) \
    X(EQ_MIDQ,    "eq_midq",    "Mid Q", 0.3f, 8, 1,    Map::Log, Unit::None, Scope::Global, nullptr, 1) \
    X(EQ_HIGH,    "eq_high",    "High",   -18, 18, 0,   Map::Lin, Unit::Db,   Scope::Global, nullptr, 1) \
    X(EQ_HIGHFREQ,"eq_highfreq","High Freq",1000, 16000, 6000,Map::Log, Unit::Hz, Scope::Global, nullptr, 1) \
    X(PHA_ON,     "pha_on",     "Phaser",   0, 1, 0,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(PHA_RATE,   "pha_rate",   "Rate",  0.02f, 10, 0.4f,Map::Log, Unit::Hz,  Scope::Global, nullptr, 1) \
    X(PHA_DEPTH,  "pha_depth",  "Depth",    0, 1, 0.7f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(PHA_FB,     "pha_fb",     "Feedback", 0, 0.9f, 0.5f,Map::Lin,Unit::Pct, Scope::Global, nullptr, 1) \
    X(PHA_MIX,    "pha_mix",    "Mix",      0, 1, 0.5f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(FLG_ON,     "flg_on",     "Flanger",  0, 1, 0,    Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(FLG_RATE,   "flg_rate",   "Rate",  0.02f, 10, 0.25f,Map::Log, Unit::Hz, Scope::Global, nullptr, 1) \
    X(FLG_DEPTH,  "flg_depth",  "Depth",    0, 1, 0.6f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(FLG_FB,     "flg_fb",     "Feedback",-0.95f, 0.95f, 0.5f,Map::Lin, Unit::Pct, Scope::Global, nullptr, 1) \
    X(FLG_MIX,    "flg_mix",    "Mix",      0, 1, 0.5f, Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(COMP_ON,    "comp_on",    "Compressor",0, 1, 0,   Map::Bool, Unit::None, Scope::Global, nullptr, 1) \
    X(COMP_THRESH,"comp_thresh","Threshold",-48, 0, -18,Map::Lin, Unit::Db,   Scope::Global, nullptr, 1) \
    X(COMP_RATIO, "comp_ratio", "Ratio",    1, 20, 4,   Map::Log, Unit::Ratio,Scope::Global, nullptr, 1) \
    X(COMP_ATK,   "comp_atk",   "Attack", 0.1f, 100, 10,Map::Log, Unit::Ms,   Scope::Global, nullptr, 1) \
    X(COMP_REL,   "comp_rel",   "Release", 10, 1000, 120,Map::Log, Unit::Ms,  Scope::Global, nullptr, 1) \
    X(COMP_MAKEUP,"comp_makeup","Makeup",   0, 24, 0,   Map::Lin, Unit::Db,   Scope::Global, nullptr, 1) \
    X(COMP_MIX,   "comp_mix",   "Mix",      0, 1, 1,    Map::Lin, Unit::Pct,  Scope::Global, nullptr, 1) \
    X(MASTER_VOL, "master",     "Master", -48, 6, -6,   Map::Lin, Unit::Db,   Scope::Global, nullptr, 1)

enum class P : int {
#define X(id, key, label, mn, mx, def, map, unit, scope, choices, skew) id,
    HC_PARAM_LIST
#undef X
    COUNT
};

constexpr int NUM_PARAMS = (int)P::COUNT;

struct ParamInfo {
    P id;
    const char* key;
    const char* label;
    float min, max, def;
    Map map;
    Unit unit;
    Scope scope;
    const char* const* choices;
    float skew;

    float toValue(float norm) const;   // normalized -> plain
    float toNorm(float value) const;   // plain -> normalized
    int   numSteps() const;            // 0 for continuous
    std::string format(float value) const;
};

const ParamInfo& paramInfo(P id);
inline const ParamInfo& paramInfo(int id) { return paramInfo((P)id); }
int findParam(const char* key);   // -1 if unknown

// Offsets let one code path serve Osc A/B, Env 1-3, LFO 1-4.
constexpr int OSC_STRIDE = (int)P::OSCB_ON - (int)P::OSCA_ON;
constexpr int ENV_STRIDE = (int)P::ENV2_ATK - (int)P::ENV1_ATK;
constexpr int LFO_STRIDE = (int)P::LFO2_SHAPE - (int)P::LFO1_SHAPE;
constexpr int FLT_STRIDE = (int)P::FLT2_ON - (int)P::FLT_ON;
inline P fltParam(int f, P f1) { return (P)((int)f1 + f * FLT_STRIDE); }

// Effects, in the order they can be arranged on the rack.
enum class FxId : int { Distortion, Phaser, Flanger, Chorus, Delay, Reverb, Eq, Compressor, COUNT };
constexpr int NUM_FX = (int)FxId::COUNT;
const char* fxName(FxId id);
P fxPowerParam(FxId id);
// order packed as one nibble per slot (slot 0 in the low bits)
constexpr uint32_t DEFAULT_FX_ORDER =
    (0u) | (6u << 4) | (1u << 8) | (2u << 12) | (3u << 16) | (4u << 20) | (5u << 24) | (7u << 28);
inline FxId fxAt(uint32_t order, int slot) { return (FxId)((order >> (slot * 4)) & 0xF); }
inline P oscParam(int osc, P a)  { return (P)((int)a + osc * OSC_STRIDE); }
inline P envParam(int env, P e1) { return (P)((int)e1 + env * ENV_STRIDE); }
inline P lfoParam(int lfo, P l1) { return (P)((int)l1 + lfo * LFO_STRIDE); }

// ---------------------------------------------------------------------
// Shared normalized parameter values (UI writes, audio reads)
// ---------------------------------------------------------------------
class ParamStore {
public:
    ParamStore() { resetToDefaults(); }

    void resetToDefaults();

    float norm(P id) const { return v[(int)id].load(std::memory_order_relaxed); }
    void  setNorm(P id, float n);

    float value(P id) const { return paramInfo(id).toValue(norm(id)); }
    void  setValue(P id, float value) { setNorm(id, paramInfo(id).toNorm(value)); }

private:
    std::atomic<float> v[NUM_PARAMS];
};
