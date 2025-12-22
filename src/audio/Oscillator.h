#pragma once

#include <cmath>
#include <array>
#include <algorithm>

#include "core/CONSTANTS.h"
#include "core/Sample.h"
#include "audio/Wavetable.h"

// ============================
// Resolved oscillator params
// (Voice owns modulation; osc consumes)
// ============================
// Resolved oscillator params
struct OscResolved {
    float freq = 440.0f;        // Hz
    float wtPos = 0.0f;         // 0..1
    float level = 1.0f;         // linear
    float detune = 0.0f;        // cents
    float stereoSpread = 0.0f;  // amount unison spreads
    float pan = 0.0f;           // -1..1 explicit per-osc pan (center = 0)
    float phaseSpread = 0.0f;   // optional per-osc phase spread (0..1)
    int   unison = 1;           // 1..MAX_UNISON
};


class Oscillator {
public:
    static constexpr int MAX_UNISON = 8;

    std::array<float, MAX_UNISON> phase{};
    const Wavetable* wavetable = nullptr;

    // ============================
    // PHASE RESET
    // ============================
    void resetPhase(bool randomize)
    {
        for (auto& p : phase) {
            p = randomize
                ? ((float)std::rand() / RAND_MAX) * TWO_PI
                : 0.0f;
        }
    }

    // ============================
    // PROCESS (Serum-style)
    // ============================
    inline Sample process(float sr, const OscResolved& r)
    {
        Sample out{};

        if (!wavetable || wavetable->numFrames == 0 || sr <= 0.0f)
            return out;

        int voices = std::clamp(r.unison, 1, MAX_UNISON);

        // ----------------------------
        // WAVETABLE POSITION
        // ----------------------------
        float wtPos = std::clamp(r.wtPos, 0.0f, 1.0f);
        float framePos = wtPos * (wavetable->numFrames - 1);
        int f0 = (int)framePos;
        int f1 = std::min(f0 + 1, wavetable->numFrames - 1);
        float frameFrac = framePos - f0;

        float outL = 0.0f;
        float outR = 0.0f;

        // ----------------------------
        // UNISON LOOP
        // ----------------------------
        for (int i = 0; i < voices; ++i) {

            float spread = (voices == 1)
                ? 0.0f
                : (float(i) / (voices - 1) * 2.0f - 1.0f);

            float detuneRatio = std::pow(2.0f, (spread * r.detune) / 1200.0f);
            float freq = r.freq * detuneRatio;

            // phase → table index
            float ph = phase[i] * INV_2PI;
            float idx = ph * wavetable->tableSize;
            int i0 = (int)idx & (wavetable->tableSize - 1);
            int i1 = (i0 + 1) & (wavetable->tableSize - 1);
            float frac = idx - i0;

            float s00 = wavetable->sample(f0, i0);
            float s01 = wavetable->sample(f0, i1);
            float s10 = wavetable->sample(f1, i0);
            float s11 = wavetable->sample(f1, i1);

            float s0 = s00 + (s01 - s00) * frac;
            float s1 = s10 + (s11 - s10) * frac;
            float s  = s0  + (s1  - s0)  * frameFrac;

            phase[i] += TWO_PI * freq / sr;
            if (phase[i] >= TWO_PI)
                phase[i] -= TWO_PI;

            // explicit per-osc pan + per-voice spread offset (combined)
            float explicitPan = std::clamp(r.pan, -1.0f, 1.0f);
            float spreadOffset = (r.unison == 1) ? 0.0f : (spread * r.stereoSpread);

            // combine and clamp
            float pan = std::clamp(explicitPan + spreadOffset, -1.0f, 1.0f);
            float ang = (pan + 1.0f) * 0.25f * PI;


            outL += s * std::cos(ang);
            outR += s * std::sin(ang);
        }

        float norm = 1.0f / std::sqrt((float)voices);

        out.left  = outL * r.level * norm;
        out.right = outR * r.level * norm;

        return out;
    }
};
