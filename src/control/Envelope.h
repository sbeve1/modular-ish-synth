#pragma once
#include "control/ControlModule.h"
#include <cmath>
#include <algorithm>

class Envelope : public ControlModule {
public:

    // -----------------------------
    // PUBLIC ADSR PARAMETERS
    // -----------------------------
    float attack  = 0.01f;   // seconds
    float hold    = 0.00f;   // seconds (NEW)
    float decay   = 0.20f;
    float sustain = 0.70f;
    float release = 0.30f;

    // -----------------------------
    // Serum-style curve controls
    // -----------------------------
    float attackCurve  = 0.0f;
    float decayCurve   = 0.0f;
    float releaseCurve = 0.0f;

    // -----------------------------
    // Envelope depth (NEW)
    // -----------------------------
    float amount = 1.0f; // Output multiplier (modulatable)

    // -----------------------------
    // Global time scaling
    // -----------------------------
    float timeScale = 1.0f;

    // -----------------------------
    // Velocity → amplitude scaling
    // -----------------------------
    float velocitySensitivity = 0.0f;
    float velocity = 1.0f;

    // -----------------------------
    // Note tracking
    // -----------------------------
    float noteTracking = 0.0f;
    int noteNumber = 60;

    // -----------------------------
    // Retrigger smoothing (NEW)
    // -----------------------------
    bool smoothRetrigger = true;

    void noteOn(int midiNote = 60, float vel = 1.0f)
    {
        velocity = vel;
        noteNumber = midiNote;

        if (smoothRetrigger && value > 0.3f)
            value = 0.3f;

        elapsed = 0.0f;
        state = State::ATTACK;
    }

    void noteOff()
    {
        state = State::RELEASE;
    }

    float process(float sr) override
    {
        float dt = 1.0f / sr;

        float scaledAttack  = clampTime(attack  * computeTimeScale());
        float scaledHold    = clampTime(hold); // simple hold
        float scaledDecay   = clampTime(decay   * computeTimeScale());
        float scaledRelease = clampTime(release * computeTimeScale());

        elapsed += dt;

        switch (state)
        {
            // --------------------------
            case State::ATTACK:
            {
                float curve = computeCurve(attackCurve);
                value += ((1.0f - value) * curve * dt) / scaledAttack;

                if (value >= 0.999999f) {
                    value = 1.0f;
                    elapsed = 0.0f;
                    state = (scaledHold > 0.0f) ? State::HOLD : State::DECAY;
                }
                break;
            }

            // --------------------------
            case State::HOLD:
                if (elapsed >= scaledHold) {
                    elapsed = 0.0f;
                    state = State::DECAY;
                }
                break;

            // --------------------------
            case State::DECAY:
            {
                float curve = computeCurve(decayCurve);
                value += ((sustain - value) * curve * dt) / scaledDecay;

                if (fabs(value - sustain) < 0.000001f) {
                    value = sustain;
                    state = State::SUSTAIN;
                }
                break;
            }

            // --------------------------
            case State::SUSTAIN:
                value = sustain * velocityScale();
                break;

            // --------------------------
            case State::RELEASE:
            {
                float curve = computeCurve(releaseCurve);
                value += (-value * curve * dt) / scaledRelease;

                if (value <= 0.000001f) {
                    value = 0.0f;
                    state = State::IDLE;
                }
                break;
            }

            // --------------------------
            case State::IDLE:
                value = 0.0f;
                break;
        }

        return value * amount; // apply envelope amount
    }

    bool isActive() const { return state != State::IDLE; }

private:

    enum class State { IDLE, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE };
    State state = State::IDLE;

    float value = 0.0f;
    float elapsed = 0.0f;

    // ------------------------------------
    float computeCurve(float c) const
    {
        return (c >= 0.f) ? (1.f + c) : 1.f / (1.f - c);
    }

    float computeTimeScale() const
    {
        float noteFactor = powf(2.0f, (noteNumber - 60) / 12.0f);
        float noteScale = 1.0f + noteTracking * (noteFactor - 1.0f);
        return timeScale * noteScale;
    }

    float velocityScale() const
    {
        return 1.0f + velocitySensitivity * (velocity - 1.0f);
    }

    float clampTime(float t) const
    {
        const float MIN_T = 0.001f;
        const float MAX_T = 20.0f;
        return std::clamp(t, MIN_T, MAX_T);
    }
};
