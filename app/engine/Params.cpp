#include "engine/Params.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

const char* const kWarpModes[]   = { "Off", "Bend", "PWM", "Sync", "Mirror", "Fold", nullptr };
const char* const kSubShapes[]   = { "Sine", "Triangle", "Saw", "Square", nullptr };
const char* const kNoiseTypes[]  = { "White", "Pink", "Brown", nullptr };
const char* const kFilterTypes[] = { "LP 12", "LP 24", "BP 12", "BP 24", "HP 12", "HP 24", "Notch", "Peak", nullptr };
const char* const kLfoShapes[]   = { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Drift", "Custom", nullptr };
const char* const kLfoModes[]    = { "Free", "Retrig", "Envelope", nullptr };
const char* const kSyncDivs[]    = { "4/1", "2/1", "1/1", "1/2", "1/2 D", "1/2 T", "1/4", "1/4 D", "1/4 T",
                                     "1/8", "1/8 D", "1/8 T", "1/16", "1/16 T", "1/32", nullptr };
const float       kSyncDivBeats[]= { 16, 8, 4, 2, 3, 4.0f / 3, 1, 1.5f, 2.0f / 3,
                                     0.5f, 0.75f, 1.0f / 3, 0.25f, 1.0f / 6, 0.125f };
const char* const kDistModes[]   = { "Soft", "Hard", "Tube", "Fold", "Crush", nullptr };
const char* const kVoiceModes[]  = { "Poly", "Mono", "Legato", nullptr };
const char* const kFilterRouting[] = { "Serial", "Parallel", nullptr };
const char* const kArpModes[]    = { "Up", "Down", "Up/Down", "Random", "As Played", "Chord", nullptr };

static const char* const kFxNames[NUM_FX] = { "Distortion", "Phaser", "Flanger", "Chorus", "Delay", "Reverb", "EQ", "Compressor" };
const char* fxName(FxId id) { return kFxNames[(int)id]; }
P fxPowerParam(FxId id)
{
    switch (id) {
        case FxId::Distortion: return P::DIST_ON;
        case FxId::Phaser:     return P::PHA_ON;
        case FxId::Flanger:    return P::FLG_ON;
        case FxId::Chorus:     return P::CHO_ON;
        case FxId::Delay:      return P::DLY_ON;
        case FxId::Reverb:     return P::REV_ON;
        case FxId::Eq:         return P::EQ_ON;
        default:               return P::COMP_ON;
    }
}

static const ParamInfo kParams[NUM_PARAMS] = {
#define X(id, key, label, mn, mx, def, map, unit, scope, choices, skew) \
    { P::id, key, label, (float)(mn), (float)(mx), (float)(def), map, unit, scope, choices, (float)(skew) },
    HC_PARAM_LIST
#undef X
};

const ParamInfo& paramInfo(P id) { return kParams[(int)id]; }

int findParam(const char* key)
{
    for (int i = 0; i < NUM_PARAMS; ++i)
        if (std::strcmp(kParams[i].key, key) == 0)
            return i;
    return -1;
}

float ParamInfo::toValue(float n) const
{
    n = std::clamp(n, 0.0f, 1.0f);
    switch (map) {
        case Map::Lin:  return min + (max - min) * n;
        case Map::Log:  return min * std::pow(max / min, n);
        case Map::Pow:  return min + (max - min) * std::pow(n, skew);
        case Map::Int:
        case Map::Choice:
        case Map::Bool: return std::round(min + (max - min) * n);
    }
    return min;
}

float ParamInfo::toNorm(float value) const
{
    value = std::clamp(value, min, max);
    float n = 0.0f;
    switch (map) {
        case Map::Lin:  n = (value - min) / (max - min); break;
        case Map::Log:  n = std::log(value / min) / std::log(max / min); break;
        case Map::Pow:  n = std::pow((value - min) / (max - min), 1.0f / skew); break;
        case Map::Int:
        case Map::Choice:
        case Map::Bool: n = (std::round(value) - min) / (max - min); break;
    }
    return std::clamp(n, 0.0f, 1.0f);
}

int ParamInfo::numSteps() const
{
    if (map == Map::Int || map == Map::Choice || map == Map::Bool)
        return (int)(max - min);
    return 0;
}

std::string ParamInfo::format(float v) const
{
    char buf[64];
    if (map == Map::Choice && choices) {
        int i = std::clamp((int)std::round(v - min), 0, (int)(max - min));
        return choices[i];
    }
    if (map == Map::Bool)
        return v >= 0.5f ? "On" : "Off";

    switch (unit) {
        case Unit::Pct:    std::snprintf(buf, sizeof buf, "%.0f%%", v * 100.0f); break;
        case Unit::Hz:     if (v >= 1000.0f) std::snprintf(buf, sizeof buf, "%.2f kHz", v / 1000.0f);
                           else if (v < 10.0f) std::snprintf(buf, sizeof buf, "%.2f Hz", v);
                           else std::snprintf(buf, sizeof buf, "%.0f Hz", v);
                           break;
        case Unit::Sec:    if (v < 1.0f) std::snprintf(buf, sizeof buf, "%.0f ms", v * 1000.0f);
                           else std::snprintf(buf, sizeof buf, "%.2f s", v);
                           break;
        case Unit::Ms:     std::snprintf(buf, sizeof buf, "%.0f ms", v); break;
        case Unit::Db:     std::snprintf(buf, sizeof buf, "%+.1f dB", v); break;
        case Unit::Semi:   std::snprintf(buf, sizeof buf, "%+.0f st", v); break;
        case Unit::Cent:   std::snprintf(buf, sizeof buf, "%+.0f ct", v); break;
        case Unit::Oct:    std::snprintf(buf, sizeof buf, "%+.0f oct", v); break;
        case Unit::Bpm:    std::snprintf(buf, sizeof buf, "%.0f BPM", v); break;
        case Unit::Voices: std::snprintf(buf, sizeof buf, "%.0f", v); break;
        case Unit::Ratio:  std::snprintf(buf, sizeof buf, v < 10.0f ? "%.1f:1" : "%.0f:1", v); break;
        case Unit::Pan:    if (std::fabs(v) < 0.005f) std::snprintf(buf, sizeof buf, "C");
                           else std::snprintf(buf, sizeof buf, "%.0f%c", std::fabs(v) * 100.0f, v < 0 ? 'L' : 'R');
                           break;
        default:           std::snprintf(buf, sizeof buf, "%.2f", v); break;
    }
    return buf;
}

void ParamStore::resetToDefaults()
{
    for (int i = 0; i < NUM_PARAMS; ++i)
        v[i].store(kParams[i].toNorm(kParams[i].def), std::memory_order_relaxed);
}

void ParamStore::setNorm(P id, float n)
{
    const ParamInfo& info = paramInfo(id);
    n = std::clamp(n, 0.0f, 1.0f);
    if (int steps = info.numSteps())
        n = std::round(n * steps) / steps;   // snap stepped params
    v[(int)id].store(n, std::memory_order_relaxed);
}
