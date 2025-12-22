#pragma once
#include <cmath>
#include <algorithm>

struct Filter {
    // State (per channel)
    float ic1eqL = 0.0f, ic2eqL = 0.0f;
    float ic1eqR = 0.0f, ic2eqR = 0.0f;

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
        return std::clamp(res, 0.0f, 0.99f); // critical
    }

    inline float processSample(
        float in,
        float cutoff,
        float res,
        float sr,
        float& ic1eq,
        float& ic2eq
    ) {
        // TPT SVF (zero-delay feedback)
        float g = std::tan(float(M_PI) * cutoff / sr);
        float k = 2.0f - 2.0f * res; // Serum-style mapping

        float v0 = in;
        float v1 = (v0 - ic2eq) * g / (1.0f + g * (g + k));
        float v2 = ic2eq + g * v1;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        // Lowpass output
        return v2;
    }

    inline void process(
        float inL,
        float inR,
        float cutoff,
        float res,
        float sr,
        float& outL,
        float& outR
    ) {
        if (sr <= 0.0f) {
            outL = outR = 0.0f;
            reset();
            return;
        }

        cutoff = sanitizeCutoff(cutoff, sr);
        res    = sanitizeRes(res);

        outL = processSample(inL, cutoff, res, sr, ic1eqL, ic2eqL);
        outR = processSample(inR, cutoff, res, sr, ic1eqR, ic2eqR);

        // FINAL NaN GUARD — Serum does this
        if (!std::isfinite(outL) || !std::isfinite(outR)) {
            outL = outR = 0.0f;
            reset();
        }
    }
};
