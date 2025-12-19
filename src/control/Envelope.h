#pragma once
#include "control/ControlModule.h"
#include <cmath>

class Envelope : public ControlModule {
public:
    void noteOn() {
        state = State::ATTACK;
    }

    void noteOff() {
        state = State::RELEASE;
    }

    float process(float sr) override {
        switch (state) {
            case State::ATTACK: {
                // exponential attack curve
                float coeff = expf(-1.0f / (attack * sr));
                value = value * coeff + (1.0f - coeff);

                if (value >= 0.999f) {
                    value = 1.0f;
                    state = State::DECAY;
                }
                break;
            }

            case State::DECAY: {
                float coeff = expf(-1.0f / (decay * sr));
                value = value * coeff + (sustain * (1.0f - coeff));

                if (fabs(value - sustain) < 0.001f) {
                    value = sustain;
                    state = State::SUSTAIN;
                }
                break;
            }

            case State::SUSTAIN:
                // hold steady
                value = sustain;
                break;

            case State::RELEASE: {
                float coeff = expf(-1.0f / (release * sr));
                value = value * coeff;    // decays toward 0

                if (value < 0.0001f) {
                    value = 0.0f;
                    state = State::IDLE;
                }
                break;
            }

            case State::IDLE:
                value = 0.0f;
                break;
        }

        return value;
    }

    bool isActive() const {
        return state != State::IDLE;
    }

    void setAttack(float a) { attack = a; }
    void setDecay(float d)  { decay = d; }
    void setSustain(float s){ sustain = s; }
    void setRelease(float r){ release = r; }

private:
    enum class State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

    State state = State::IDLE;

    float attack  = 0.01f;   // seconds
    float decay   = 0.20f;
    float sustain = 0.7f;
    float release = 0.5f;

    float value = 0.0f;
};
