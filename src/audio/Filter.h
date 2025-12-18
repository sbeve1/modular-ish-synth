#pragma once
#include "audio/Module.h"
#include "core/CONSTANTS.h"
#include <cmath>

class Filter : public Module {
public:
    enum class DeviceMode { 
        BYPASS, 
        ONE_POLE_LOWPASS,
        ONE_POLE_HIGHPASS
    };

    Sample process(float sampleRate, const Sample& in) override {

        // compute coefficient. convert cutoff freq into a smoothing amount
        float a = 1 - expf(-2 * TWO_PI * cutoff / sampleRate);
        Sample out;
        switch(type) {
            case DeviceMode::ONE_POLE_LOWPASS: {
                // LP[n] = LP[n-1] + a * (in[n] - LP[n-1])
                out.left = prevLPOutput.left + a * (in.left - prevLPOutput.left);
                out.right = prevLPOutput.right + a * (in.right - prevLPOutput.right);

                prevLPOutput = out;
                break;
            }
            case DeviceMode::ONE_POLE_HIGHPASS: {
                // LP[n]
                float lowPassLeft = prevLPOutput.left + a * (in.left - prevLPOutput.left);
                float lowPassRight = prevLPOutput.right + a * (in.right - prevLPOutput.right);

                // HP[n] = in[n] - LP[n]
                out.left = in.left - lowPassLeft;
                out.right = in.right - lowPassRight;

                prevLPOutput.left = lowPassLeft;
                prevLPOutput.right = lowPassRight;

                break;
            }
            case DeviceMode::BYPASS: {
                out.left = in.left;
                out.right = in.right;
                break;
            }
        }
        return out;
    }   

    void setCutoff(float c) { cutoff = c; }
    void setFilterType(DeviceMode f) { type = f; }
    float* getCutoffPtr() { return &cutoff; }
    float* getResonancePtr() { return &resonance; }

private: 
    DeviceMode type = DeviceMode::BYPASS;

    float cutoff = 1000.0f;
    float resonance = 0.0f;

    
    Sample prevLPOutput = {0.0f, 0.0f};
    // Sample prevLPInput = {0.0f, 0.0f};
};
