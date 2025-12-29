#pragma once
#include <cmath>
#include <cstdint>
#include "core/Sample.h"
#include "engine/voice/Resolved.h"

struct Noise
{
    uint32_t seed = 0x12345678u;
    float humPhase = 0.0f;

    inline float rand01()
    {
        seed = seed * 1664525u + 1013904223u;
        return ((seed >> 9) & 0x007FFFFF) * (1.0f / 8388607.0f);
    }

    inline Sample process(float sr, const NoiseResolved& r)
    {
        if (r.level <= 1e-6f)
            return {};

        float n = 0.0f;

        switch (r.mode) {
            case NoiseMode::WHITE:
                n = rand01() * 2.0f - 1.0f;
                break;

            case NoiseMode::AC_HUM: {
                float baseHz = 60.0f * r.pitch;
                float inc = TWO_PI * baseHz / sr;

                humPhase += inc;
                if (humPhase >= TWO_PI)
                    humPhase -= TWO_PI;

                n =
                    1.0f * std::sin(humPhase) +
                    0.5f * std::sin(2.0f * humPhase) +
                    0.25f * std::sin(3.0f * humPhase);
                break;
            }
        }

        n *= r.level;

        float angle = (r.pan + 1.0f) * 0.25f * PI;
        return {
            n * std::cos(angle),
            n * std::sin(angle)
        };
    }

    inline void reset()
    {
        seed = 0x12345678u;
        humPhase = 0.0f;
    }
};
