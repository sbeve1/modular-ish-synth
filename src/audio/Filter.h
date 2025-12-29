#pragma once
#include <cmath>
#include <algorithm>
#include "engine/Params/FilterParams.h"

struct Filter {
    float ic1eqL = 0.0f, ic2eqL = 0.0f;
    float ic1eqR = 0.0f, ic2eqR = 0.0f;

    FilterMode lastMode = FilterMode::LP;

    inline void reset() {
        ic1eqL = ic2eqL = 0.0f;
        ic1eqR = ic2eqR = 0.0f;
    }

    inline float sanitizeCutoff(float cutoff, float sr) {
        if (!std::isfinite(cutoff)) return 1000.0f;
        return std::clamp(cutoff, 20.0f, sr * 0.45f);
    }

    inline float sanitizeRes(float res) {
        if (!std::isfinite(res)) return 0.0f;
        return std::clamp(res, 0.0f, 0.99f);
    }

    inline float processSample(
        float in,
        float cutoff,
        float res,
        FilterMode mode,
        float sr,
        float& ic1eq,
        float& ic2eq
    ) {
        float g = std::tan(float(M_PI) * cutoff / sr);
        float k = 2.0f - 2.0f * res;

        float v0 = in;
        float v1 = (v0 - ic2eq) * g / (1.0f + g * (g + k));
        float v2 = ic2eq + g * v1;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        float lp = v2;
        float bp = v1;
        float hp = v0 - k * v1 - v2;

        switch (mode) {
            case FilterMode::LP: return lp;
            case FilterMode::BP: return bp;
            case FilterMode::HP: return hp;
        }
        return lp;
    }

    inline void process(
        float inL,
        float inR,
        float cutoff,
        float res,
        FilterMode mode,
        float sr,
        float& outL,
        float& outR
    ) {
        if (mode != lastMode) {
            reset();
            lastMode = mode;
        }

        cutoff = sanitizeCutoff(cutoff, sr);
        res    = sanitizeRes(res);

        outL = processSample(inL, cutoff, res, mode, sr, ic1eqL, ic2eqL);
        outR = processSample(inR, cutoff, res, mode, sr, ic1eqR, ic2eqR);

        if (!std::isfinite(outL) || !std::isfinite(outR)) {
            reset();
            outL = outR = 0.0f;
        }
    }
};
