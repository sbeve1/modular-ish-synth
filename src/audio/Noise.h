#pragma once

#include "audio/Module.h"
#include <cstdint>
#include <cmath>

class Noise : public Module {
public:
    // ============================
    // BASE PARAMS (Serum-style)
    // ============================
    float pitchBase = 0.0f;   // fine pitch only (semitone-ish)

    // ============================
    // MOD SHADOW
    // ============================
    float pitchMod  = 0.0f;

    // ============================
    // INTERNAL RNG (per voice)
    // ============================
    uint32_t seed = 1u;

    void resetModValues()
    {
        pitchMod = pitchBase;
    }

    // ============================
    // MOD TARGET
    // ============================
    float* getPitchPtr()        { return &pitchMod; }
    float  getPitchBase() const { return pitchBase; }

    // ============================
    // PROCESS
    // ============================
    Sample process(float /*sr*/, const Sample&) override
    {
        // Per-voice deterministic RNG
        seed = seed * 1664525u + 1013904223u;

        float n = ((seed >> 16) & 0x7FFF) / 16384.0f - 1.0f;

        // Pitch = spectral scaling (simple approximation)
        float pitchRatio = powf(2.0f, pitchMod / 12.0f);
        n *= pitchRatio;

        // Raw mono noise (no pan, no level)
        return { n, n };
    }

    void reset(uint32_t voiceSeed)
    {
        seed = voiceSeed;
    }
};
