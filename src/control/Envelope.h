#pragma once
#include "control/ControlModule.h"
#include "core/CONSTANTS.h"
#include <cmath>
#include <algorithm>

class Envelope : public ControlModule {
public:

    // -----------------------------
    // PUBLIC ADSR PARAMETERS
    // -----------------------------
    float attack  = 0.01f;   // seconds
    float hold    = 0.00f;   // seconds
    float decay   = 0.20f;
    float sustain = 0.70f;
    float release = 0.30f;

    // -----------------------------
    // Curve controls
    // -----------------------------
    float attackCurve  = 0.0f;
    float decayCurve   = 0.0f;
    float releaseCurve = 0.0f;

    // -----------------------------
    // Envelope depth (modulatable)
    // -----------------------------
    float amount = 1.0f; // Output multiplier

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
    // Retrigger smoothing
    // -----------------------------
    bool smoothRetrigger = true;

    void noteOn(int midiNote = 60, float vel = 1.0f)
    {
        velocity = vel;
        noteNumber = midiNote;

        // If currently loud, softly jump down a bit to avoid clicks when retriggering
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
        float scaledHold    = clampTime(hold); // hold treated as plain seconds
        float scaledDecay   = clampTime(decay   * computeTimeScale());
        float scaledRelease = clampTime(release * computeTimeScale());

        elapsed += dt;

        switch (state)
        {
            case State::ATTACK:
            {
                float curve = computeCurve(attackCurve);
                // Note: this is perceptual-style integration (fine for musical behavior)
                value += ((1.0f - value) * curve * dt) / scaledAttack;

                if (value >= 0.999999f) {
                    value = 1.0f;
                    elapsed = 0.0f;
                    state = (scaledHold > 0.0f) ? State::HOLD : State::DECAY;
                }
                break;
            }

            case State::HOLD:
                if (elapsed >= scaledHold) {
                    elapsed = 0.0f;
                    state = State::DECAY;
                }
                break;

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

            case State::SUSTAIN:
                value = sustain * velocityScale();
                break;

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

            case State::IDLE:
                value = 0.0f;
                break;
        }

        // apply amount as final multiplier
        return value * amount;
    }

    bool isActive() const { return state != State::IDLE; }

private:

    enum class State { IDLE, ATTACK, HOLD, DECAY, SUSTAIN, RELEASE };
    State state = State::IDLE;

    // Use the protected 'value' defined in ControlModule (do NOT redeclare it here)
    float elapsed = 0.0f;

    // ------------------------------------
    float computeCurve(float c) const
    {
        // simple curve mapping: >=0 boosts speed, <0 slows (sane mapping)
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
        return std::clamp(t, ENV_MIN_TIME, ENV_MAX_TIME);
    }
};
