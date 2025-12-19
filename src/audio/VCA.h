#pragma once
#include "audio/Module.h"
#include "core/CONSTANTS.h"
#include <algorithm>
#include <cmath>

class VCA : public Module {
public:
    Sample process(float sampleRate, const Sample& in) override
    {
        (void)sampleRate;

        // 1. Clamp gain
        float g = std::clamp(gain, 0.0f, SAT_CLIP_K);

        // 2. Apply gain
        float l = in.left  * g;
        float r = in.right * g;

        // 3. Equal-power panning (-1 = left, +1 = right)
        float p = std::clamp(pan, -1.0f, 1.0f);
        float angle = (p * 0.5f + 0.5f) * (PI * 0.5f); // 0..pi/2

        float leftGain  = cosf(angle);
        float rightGain = sinf(angle);

        l *= leftGain;
        r *= rightGain;

        // 4. Soft saturation safety
        l = l / (1.0f + fabsf(l));
        r = r / (1.0f + fabsf(r));

        return { l, r };
    }

    void setGain(float g) { gain = g; }
    float* getGainPtr()   { return &gain; }

    void setPan(float p)  { pan = p; }
    float* getPanPtr()    { return &pan; }

private:
    float gain = 0.0f;  // 0..1
    float pan  = 0.0f;  // -1..1
};
