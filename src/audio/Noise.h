#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

#include "core/Sample.h"
#include "engine/NoiseParams.h"
#include "core/CONSTANTS.h"

// Simple, efficient per-voice noise source with pitch tilt/pan/level
class Noise {
public:
    // ============================
    // DSP STATE ONLY
    // ============================
    uint32_t seed = 2166136261u; // non-zero default

    // ============================
    // PROCESS (PURE SOURCE)
    // ============================
    Sample process(float /*sr*/, const NoiseParams& p)
    {
        // LCG - deterministic per-voice RNG
        seed = seed * 1664525u + 1013904223u;

        // 15-bit random centered -1..+1
        float n = (float)((seed >> 16) & 0x7FFF) / 16384.0f - 1.0f;

        // ----------------------------
        // PITCH (spectral tilt)
        // treat pitch as semitones; small tilt
        // ----------------------------
        float pitchSemi = p.pitchBase + p.pitchMod;
        float pitchRatio = powf(2.0f, pitchSemi / 12.0f);
        n *= pitchRatio;

        // ----------------------------
        // LEVEL (base + mod) then clamp
        // ----------------------------
        float level = std::max(0.0f, p.levelBase + p.levelMod);
        n *= level;

        // ----------------------------
        // PAN (-1 .. +1), equal-power
        // ----------------------------
        float pan = std::clamp(p.panBase + p.panMod, -1.0f, 1.0f);
        float angle = (pan + 1.0f) * 0.25f * PI; // -1..1 -> 0..pi/2
        float l = cosf(angle);
        float r = sinf(angle);

        return { n * l, n * r };
    }

    // reset per-voice RNG seed (call from Voice::start)
    void reset(uint32_t voiceSeed)
    {
        // avoid zero seed for LCG
        seed = (voiceSeed == 0u) ? 2166136261u : voiceSeed;
    }
};
