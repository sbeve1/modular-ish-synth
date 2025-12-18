#pragma once
#include "ControlModule.h"
#include <cstdio>

class Envelope : public ControlModule {
public: 
    void noteOn() {
        state = State::ATTACK;
    }

    void noteOff() {
        state = State::RELEASE;
    }

    float process(float sampleRate) override{
        switch (state) {
            case State::ATTACK: {
                float step = 1.0f / (attack * sampleRate);
                value += step;
                if (value >= 1.0f) {
                    value = 1.0f;
                    state = State::DECAY;
                }
                break;
            }

            case State::DECAY: {
                float step = (1.0f - sustain) / (decay * sampleRate);
                value -= step;
                if (value <= sustain) {
                    value = sustain;
                    state = State::SUSTAIN;
                }
                break;
            }

            case State::SUSTAIN:
                // value just stays at sustain level
                break;

            // linear release
            case State::RELEASE: {
                float step = 1.0f / (release * sampleRate);
                value -= step;
                if (value <= 0.0f) {
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
    void setDecay(float d) { decay = d; }
    void setSustain(float s) { sustain = s; }
    void setRelease(float r) { release = r; }

private: 
    enum class State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

    State state = State::IDLE;

    float attack  = 0.01f;   // seconds
    float decay   = 0.20f;   // seconds
    float sustain = 0.7f;    // level from 0–1
    float release = 1.0f;   // seconds

    float value = 0.0f;      // current envelope output
};
