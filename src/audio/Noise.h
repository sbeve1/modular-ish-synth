#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

#include "core/Sample.h"
#include "engine/NoiseParams.h"
#include "core/CONSTANTS.h"

class Noise {
public:
    // ============================
    // DSP STATE ONLY
    // ============================
    uint32_t seed = 1u;

    // ============================
    // PROCESS (PURE SOURCE)
    // ============================
    Sample process(float /*sr*/, const NoiseParams& p)
    {
        // Deterministic per-voice RNG
        seed = seed * 1664525u + 1013904223u;

        float n = ((seed >> 16) & 0x7FFF) / 16384.0f - 1.0f;

        // ----------------------------
        // PITCH (spectral tilt)
        // ----------------------------
        float pitchRatio = powf(2.0f, p.pitchMod / 12.0f);
        n *= pitchRatio;

        // ----------------------------
        // LEVEL
        // ----------------------------
        n *= p.levelMod;

        // ----------------------------
        // PAN (-1 .. +1), equal-power
        // ----------------------------
        float pan = std::clamp(p.panMod, -1.0f, 1.0f);

        float angle = (pan + 1.0f) * 0.25f * PI;
        float l = cosf(angle);
        float r = sinf(angle);

        return { n * l, n * r };
    }

    void reset(uint32_t voiceSeed)
    {
        seed = voiceSeed;
    }
};
