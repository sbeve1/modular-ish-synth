#pragma once

#include <cmath>
#include <algorithm>

#include "core/Sample.h"
#include "core/CONSTANTS.h"
#include "engine/FilterParams.h"

class Filter {
public:
    // DSP state
    float zL = 0.0f;
    float zR = 0.0f;

    Sample process(float sr, const Sample& in, const FilterParams& p)
    {
        // ---------- cutoff ----------
        float cutoff = std::clamp(p.cutoffMod, 20.0f, sr * 0.45f);
        float a = expf(-2.0f * PI * cutoff / sr);

        // ---------- one-pole LP ----------
        zL = (1.0f - a) * in.left  + a * zL;
        zR = (1.0f - a) * in.right + a * zR;

        Sample wet{ zL, zR };

        // ---------- MIX (wet/dry) ----------
        float mix = std::clamp(p.mixMod, 0.0f, 1.0f);
        Sample out;
        out.left  = in.left  * (1.0f - mix) + wet.left  * mix;
        out.right = in.right * (1.0f - mix) + wet.right * mix;

        // ---------- LEVEL ----------
        out.left  *= p.levelMod;
        out.right *= p.levelMod;

        // ---------- PAN ----------
        float pan = std::clamp(p.panMod, -1.0f, 1.0f);
        float angle = (pan + 1.0f) * 0.25f * PI;
        float l = cosf(angle);
        float r = sinf(angle);

        return { out.left * l, out.right * r };
    }
};
