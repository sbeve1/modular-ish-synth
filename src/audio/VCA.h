#pragma once
#include "audio/Module.h"
#include "core/CONSTANTS.h"
#include <algorithm>
#include <cmath>

class VCA : public Module {
public:
    VCA() {
        gainBase = 0.0f;
        panBase  = 0.0f;
        gainMod = gainBase;
        panMod  = panBase;
    }

    Sample process(float sr, const Sample& in) override {
        (void)sr;

        float g = std::clamp(gainMod, 0.0f, SAT_CLIP_K);
        float p = std::clamp(panMod,  -1.0f, 1.0f);

        float angle = (p * 0.5f + 0.5f) * (PI * 0.5f);

        float leftGain  = cosf(angle);
        float rightGain = sinf(angle);

        float l = in.left  * g * leftGain;
        float r = in.right * g * rightGain;

        // soft saturate
        l = l / (1.0f + fabsf(l));
        r = r / (1.0f + fabsf(r));

        return { l, r };
    }

    // ModMatrix registration
    float* getGainModPtr() { return &gainMod; }
    float  getGainBase() const { return gainBase; }

    float* getPanModPtr() { return &panMod; }
    float  getPanBase() const { return panBase; }

    // UI setters update base + modShadow
    void setGain(float g) { gainBase = g; gainMod = g; }
    void setPan(float p)  { panBase  = p; panMod  = p; }

private:
    // permanent UI knob values
    float gainBase;
    float panBase;

    // modulated shadow values
    float gainMod;
    float panMod;
};

