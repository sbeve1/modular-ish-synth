#pragma once
#include "core/Sample.h"
#include <cmath>

// very small soft clip limiter — cheap, safe
struct MasterLimiter {
    // simple soft clip scale: x / (1 + |x|)
    Sample process(const Sample& in) {
        return { soft(in.left), soft(in.right) };
    }

    inline float soft(float x) const {
        // small pre-gain, then soft saturate
        float y = x;
        return y / (1.0f + fabsf(y));
    }
};
