#include "audio/FX/FXUtils.h"

struct DelayFX : public FX {

    explicit DelayFX(float sr = 44100.0f)
        : delaySamples(size_t(0.4f * sr)),
          bufL(delaySamples, 0.0f),
          bufR(delaySamples, 0.0f)
    {
        wetBase = 0.35f; // default delay mix
    }

    Sample processWet(const Sample& in, float) override
    {
        float dl = killDenorm(bufL[idx]);
        float dr = killDenorm(bufR[idx]);

        constexpr float DAMP = 0.35f;
        dl *= (1.0f - DAMP);
        dr *= (1.0f - DAMP);

        constexpr float FEEDBACK = 0.45f;
        float fbL = dl * FEEDBACK;
        float fbR = dr * FEEDBACK;

        bufL[idx] = killDenorm(in.left  + fbL);
        bufR[idx] = killDenorm(in.right + fbR);

        idx = (idx + 1) % delaySamples;

        // IMPORTANT: return 100% WET
        Sample out{};
        out.left  = dl;
        out.right = dr;
        return out;
    }

private:
    size_t delaySamples;
    std::vector<float> bufL, bufR;
    size_t idx = 0;
};
