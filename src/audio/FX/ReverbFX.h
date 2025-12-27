#pragma once
#include <vector>
#include <cmath>
#include "audio/fx/FX.h"

struct Comb {
    std::vector<float> buffer;
    int idx = 0;
    float feedback = 0.7f;

    Comb(int size, float fb) : buffer(size, 0.0f), feedback(fb) {}

    inline float process(float input) {
        float out = buffer[idx];
        buffer[idx] = input + out * feedback;
        idx = (idx + 1) % buffer.size();
        return out;
    }
};

struct Allpass {
    std::vector<float> buffer;
    int idx = 0;
    float feedback = 0.5f;

    Allpass(int size, float fb) : buffer(size, 0.0f), feedback(fb) {}

    inline float process(float input) {
        float bufout = buffer[idx];
        float out = -input + bufout;
        buffer[idx] = input + bufout * feedback;
        idx = (idx + 1) % buffer.size();
        return out;
    }
};

struct ReverbFX : public FX {
    std::vector<Comb> combsL;
    std::vector<Comb> combsR;
    std::vector<Allpass> allpassL;
    std::vector<Allpass> allpassR;

    float wet = 0.3f;

    ReverbFX(float sr) {
        // Prime-ish delay lengths (classic)
        combsL = {
            Comb(int(0.0297f * sr), 0.77f),
            Comb(int(0.0371f * sr), 0.75f),
            Comb(int(0.0411f * sr), 0.73f),
            Comb(int(0.0437f * sr), 0.71f)
        };
        combsR = {
            Comb(int(0.0307f * sr), 0.77f),
            Comb(int(0.0323f * sr), 0.75f),
            Comb(int(0.0399f * sr), 0.73f),
            Comb(int(0.0421f * sr), 0.71f)
        };

        allpassL = {
            Allpass(int(0.005f * sr), 0.5f),
            Allpass(int(0.0017f * sr), 0.5f)
        };
        allpassR = {
            Allpass(int(0.0053f * sr), 0.5f),
            Allpass(int(0.0019f * sr), 0.5f)
        };
    }

    Sample process(const Sample& in, float) override {
        float l = 0.0f;
        float r = 0.0f;

        for (auto& c : combsL) l += c.process(in.left);
        for (auto& c : combsR) r += c.process(in.right);

        for (auto& a : allpassL) l = a.process(l);
        for (auto& a : allpassR) r = a.process(r);

        Sample out;
        out.left  = in.left  * (1.0f - wet) + l * wet;
        out.right = in.right * (1.0f - wet) + r * wet;
        return out;
    }
};
