#pragma once

#include <cmath>

#include "engine/Dsp.h"
#include "engine/LfoCurve.h"

enum class LfoShape { Sine, Triangle, SawUp, SawDown, Square, SampleHold, Drift, Custom };
enum class LfoMode  { Free, Retrig, Envelope };

// One LFO's running state. Free-mode LFOs share a global state so all voices
// stay in phase; Retrig/Envelope LFOs keep one per voice.
struct LfoState {
    float phase = 0.0f;       // 0..1
    float prev = 0.0f, next = 0.0f;
    float smoothed = 0.0f;
    bool  done = false;
    dsp::Rng rng;

    void reset(float startPhase, unsigned seed)
    {
        phase = startPhase;
        rng = dsp::Rng(seed);
        prev = rng.bi();
        next = rng.bi();
        done = false;
    }

    // advance by inc (cycles); envelope mode stops at the end of one cycle
    void advance(float inc, LfoMode mode)
    {
        if (done) return;
        phase += inc;
        if (phase >= 1.0f) {
            if (mode == LfoMode::Envelope) { phase = 1.0f; done = true; return; }
            phase -= std::floor(phase);
            prev = next;
            next = rng.bi();
        }
    }
};

// Bipolar (-1..1) shape value at phase p. curve: table for the Custom shape.
inline float lfoShapeValue(LfoShape shape, float p, const LfoState& st, const LfoCurveTable* curve = nullptr)
{
    switch (shape) {
        case LfoShape::Sine:       return dsp::fastSin(dsp::TWO_PI * p);
        case LfoShape::Triangle:   return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
        case LfoShape::SawUp:      return 2.0f * p - 1.0f;
        case LfoShape::SawDown:    return 1.0f - 2.0f * p;
        case LfoShape::Square:     return p < 0.5f ? 1.0f : -1.0f;
        case LfoShape::SampleHold: return st.prev;
        case LfoShape::Drift: {
            float t = p * p * (3.0f - 2.0f * p);
            return dsp::lerp(st.prev, st.next, t);
        }
        case LfoShape::Custom:
            return curve ? lookupLfoCurve(*curve, p) : 0.0f;
    }
    return 0.0f;
}

// For drawing (no random state): S&H/Drift get a fixed pseudo-random pattern.
inline float lfoShapePreview(LfoShape shape, float p)
{
    if (shape == LfoShape::SampleHold || shape == LfoShape::Drift) {
        static const float pts[9] = { 0.2f, -0.7f, 0.9f, -0.2f, 0.5f, -0.9f, 0.1f, 0.7f, 0.2f };
        float x = p * 8.0f;
        int i = (int)x;
        if (i > 7) i = 7;
        if (shape == LfoShape::SampleHold) return pts[i];
        float t = x - i;
        t = t * t * (3.0f - 2.0f * t);
        return dsp::lerp(pts[i], pts[i + 1], t);
    }
    LfoState dummy;
    return lfoShapeValue(shape, p, dummy);
}
