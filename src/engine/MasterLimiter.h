#pragma once
#include <cmath>
#include "core/Sample.h"

struct MasterLimiter {
    float drive = 1.2f;

    inline float soft(float x) const
    {
        x *= drive;
        return x / (1.0f + fabsf(x));
    }

    Sample process(const Sample& in)
    {
        return { soft(in.left), soft(in.right) };
    }
};
