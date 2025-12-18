#pragma once
#include "audio/Module.h"

class VCA : public Module {
public:
    Sample process(float sampleRate, const Sample& in) override {
        (void) sampleRate;
        
        return { in.left * gain, in.right * gain };
    }

    void setGain(float g) { gain = g; }
    float getGain() { return gain; }
    float* getGainPtr() { return &gain; }

private: 
    float gain = 0.0f; // control signal 
};
