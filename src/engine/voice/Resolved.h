#pragma once
#include "core/CONSTANTS.h"

struct OscResolved {
    OscMode mode;
    float freq = 440.0f;        // Hz
    float wtPos = 0.0f;         // 0..1
    float level = 1.0f;         // linear
    float detune = 0.0f;        // cents
    float stereoSpread = 0.0f;  // amount unison spreads
    float pan = 0.0f;           // -1..1 explicit per-osc pan (center = 0)
    float phaseSpread = 0.0f;   // optional per-osc phase spread (0..1)
    int   unison = 1;           // 1..MAX_UNISON
};

struct NoiseResolved {
    NoiseMode mode;
    float pitch = 1.0f;
    float level = dbToLin(-24.0f);
    float pan = 0.0f;
};

struct FilterResolved {
    float cutoffHz;
    float resonance;
};
