#pragma once

#include <cmath>
#include <algorithm>

#include "core/Sample.h"
#include "core/CONSTANTS.h"
#include "engine/FilterParams.h"

// A modest, cheap resonant-ish filter with drive and safe clamping.
// Uses base+mod semantics: caller/storage keeps base in *.Base and mod in *.Mod (modifiable by ModMatrix).
class Filter {
public:
    // state for a 2-pole-ish integrator
    float ic1eqL = 0.0f, ic2eqL = 0.0f;
    float ic1eqR = 0.0f, ic2eqR = 0.0f;

    Sample process(float sr, const Sample& in, const FilterParams& p)
    {
        // -------- combine base + mod (explicit) --------
        float cutoff = std::clamp(p.cutoffBase + p.cutoffMod, 20.0f, sr * 0.45f);
        float res    = std::clamp(p.resBase + p.resMod, 0.0f, 1.0f);
        float drive  = std::max(p.driveBase + p.driveMod, 0.0f);
        float fat    = std::max(p.fatBase + p.fatMod, 0.0f);
        float mix    = std::clamp(p.mixBase + p.mixMod, 0.0f, 1.0f);
        float level  = std::max(p.levelBase + p.levelMod, 0.0f);
        float pan    = std::clamp(p.panBase + p.panMod, -1.0f, 1.0f);

        // small safety: if level is effectively zero, return silence quickly
        if (level <= 0.0000001f || std::isnan(level) || std::isinf(level)) {
            return { 0.0f, 0.0f };
        }

        // resonant-ish biquad approximation (cheap)
        float g = tanf(PI * cutoff / sr);

        // map res (0..1) to k. higher res -> stronger feedback
        float k = 2.0f * res;

        auto proc = [&](float x, float& ic1, float& ic2)
        {
            // pre-drive nonlinearity
            float xd = tanhf(x * (1.0f + drive));

            // two-integrator loop (TPT-style-ish)
            float v1 = (xd - ic2) * g / (1.0f + g * (g + k));
            float v2 = v1 * g;
            float lp = v2 + ic2;

            // state update (simple)
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            return lp;
        };

        Sample out;
        out.left  = proc(in.left,  ic1eqL, ic2eqL);
        out.right = proc(in.right, ic1eqR, ic2eqR);

        // mix wet/dry
        out.left  = in.left  * (1.0f - mix) + out.left  * mix;
        out.right = in.right * (1.0f - mix) + out.right * mix;

        // level and panning
        out.left  *= level;
        out.right *= level;

        float angle = (pan + 1.0f) * 0.25f * PI;
        float l = cosf(angle);
        float r = sinf(angle);

        return { out.left * l, out.right * r };
    }
};
