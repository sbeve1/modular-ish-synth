#pragma once
#include "ControlModule.h"
#include "core/CONSTANTS.h"
#include <cmath>

class LFO : public ControlModule {
public: 
    enum class DeviceMode { 
        BYPASS,
        VIBRATO
    };
    float process(float sampleRate) override {
        float out = 0.0f; 

        switch(type) {
            case DeviceMode::BYPASS: {
                out = 0.0f;
                break;
            } 

            case DeviceMode::VIBRATO: {
                out = sinf(phase);

                phase += (TWO_PI * freq) / sampleRate;
                if (phase >= TWO_PI) { phase -= TWO_PI; }

                break;
            }

            default: {
                out = 0.0f;
                break;
            }
        }

        return out;
    }   

    void setFrequency(float f) { freq = f; }
    void setMode(DeviceMode d) { type = d; }

private:
    float phase = 0.0f;
    float freq = 5.0f;
    DeviceMode type = DeviceMode::VIBRATO;
};