#pragma once
#include "control/ControlModule.h"
#include "core/CONSTANTS.h"
#include <cmath>
#include <algorithm>

/*
    Serum-style ADSR envelope with curve control.

    Concept:
    - Each stage runs on normalized time t ∈ [0,1]
    - A curve parameter c ∈ [-1, +1] shapes t:
        c < 0 : logarithmic (fast start, punchy)
        c = 0 : linear
        c > 0 : exponential (slow start, smooth)
    - process() returns the FINAL gain (use this as VCA)
*/

class Envelope : public ControlModule {
public:
    // =============================
    // ADSR PARAMETERS (seconds)
    // =============================
    float attack  = 0.001f;
    float hold    = 0.0f;
    float decay   = 1.00f;
    float sustain = 1.00f; // 0 to 1, 1 being 0db
    float release = 0.015f;

    // =============================
    // CURVE PARAMETERS [-1 .. +1]
    // =============================
    float attackCurve  = 0.0f;
    float decayCurve   = 0.0f;
    float releaseCurve = 0.0f;

    // =============================
    // OUTPUT SCALING
    // =============================
    float amount = 1.0f;

    // =============================
    // VELOCITY
    // =============================
    float velocitySensitivity = 0.0f; // 0 = off
    float velocity = 1.0f;

    // =============================
    // TIME SCALING / KEY TRACK
    // =============================
    float timeScale = 1.0f;
    int   noteNumber = 60;
    float noteTracking = 0.0f;

    // =============================
    // NOTE EVENTS
    // =============================
    void noteOn(int midiNote = 60, float vel = 1.0f)
    {
        velocity   = vel;
        noteNumber = midiNote;

        elapsed = 0.0f;
        startLevel = value; // retrigger from current value (no click)
        state = State::ATTACK;
    }

    void noteOff()
    {
        elapsed = 0.0f;
        releaseStart = value;
        state = State::RELEASE;
    }

    // =============================
    // PROCESS (RETURNS FINAL GAIN)
    // =============================
    float process(float sr) override
    {
        float dt = 1.0f / sr;

        const float ts = computeTimeScale();   // once per tick (was once per stage time)
        float a = clampTime(attack  * ts);
        float d = clampTime(decay   * ts);
        float r = clampTime(release * ts);

        elapsed += dt;

        switch (state)
        {
            case State::ATTACK:
            {
                float t = elapsed / a;

                if (t >= 1.0f) {
                    value = 1.0f;
                    elapsed = 0.0f;
                    state = (hold > 0.0f) ? State::HOLD : State::DECAY;
                } else {
                    float shaped = shapeCurve(t, attackCurve, attackExp);
                    value = lerp(startLevel, 1.0f, shaped);
                }
                break;
            }

            case State::HOLD:
                if (elapsed >= hold) {
                    elapsed = 0.0f;
                    state = State::DECAY;
                }
                break;

            case State::DECAY:
            {
                float t = elapsed / d;

                if (t >= 1.0f) {
                    value = sustain;
                    state = State::SUSTAIN;
                } else {
                    float shaped = shapeCurve(t, decayCurve, decayExp);
                    value = lerp(1.0f, sustain, shaped);
                }
                break;
            }

            case State::SUSTAIN:
                value = sustain;
                break;

            case State::RELEASE:
            {
                float t = elapsed / r;

                if (t >= 1.0f) {
                    value = 0.0f;
                    state = State::IDLE;
                } else {
                    float shaped = shapeCurve(t, releaseCurve, releaseExp);
                    value = lerp(releaseStart, 0.0f, shaped);
                }
                break;
            }

            case State::IDLE:
                value = 0.0f;
                break;
        }

        return value * amount * velocityScale();
    }

    bool isActive() const { return state != State::IDLE; }

    // for displays: 0 idle, 1 attack, 2 hold, 3 decay, 4 sustain, 5 release
    int   stageIndex() const { return (int)state; }
    float stageTime()  const { return elapsed; }
    float level()      const { return value; }

private:
    // =============================
    // INTERNAL STATE
    // =============================
    enum class State { IDLE, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE };
    State state = State::IDLE;

    float elapsed = 0.0f;
    float startLevel = 0.0f;
    float releaseStart = 0.0f;

    // =============================
    // CURVE SHAPER (CORE FUNCTION)
    // =============================
    // Exponent per curve value, cached: only recomputed when the curve changes.
    struct CurveExp { float curve = 0.0f, exponent = 1.0f; };
    CurveExp attackExp, decayExp, releaseExp;

    static inline float shapeCurve(float x, float curve, CurveExp& cache)
    {
        x = std::clamp(x, 0.0f, 1.0f);
        curve = std::clamp(curve, -1.0f, 1.0f);

        // Curve strength (Serum-ish)
        constexpr float k = 4.0f;

        // curve -> exponent
        // < 1 : log-like
        // = 1 : linear
        // > 1 : exp-like
        if (curve != cache.curve) {
            cache.curve = curve;
            cache.exponent = std::pow(2.0f, curve * k);
        }
        if (cache.exponent == 1.0f) return x;
        return std::pow(x, cache.exponent);
    }

    // =============================
    // HELPERS
    // =============================
    static inline float lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    float computeTimeScale() const
    {
        if (noteTracking == 0.0f) return timeScale;   // skip the pow in the common case
        float noteFactor = std::pow(2.0f, (noteNumber - 60) / 12.0f);
        return timeScale * (1.0f + noteTracking * (noteFactor - 1.0f));
    }

    float velocityScale() const
    {
        return 1.0f + velocitySensitivity * (velocity - 1.0f);
    }

    float clampTime(float t) const
    {
        return std::clamp(t, ENV_MIN_TIME, ENV_MAX_TIME);
    }
};
