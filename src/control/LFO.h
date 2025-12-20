#pragma once
#include "control/ControlModule.h"
#include "core/CONSTANTS.h"
#include <cmath>
#include <algorithm>

constexpr int LFO_TABLE_SIZE = 256;
static_assert((LFO_TABLE_SIZE & (LFO_TABLE_SIZE - 1)) == 0,
              "LFO_TABLE_SIZE must be power of two");

extern float LFO_SHAPE[LFO_TABLE_SIZE];

class LFO : public ControlModule {
public:
    float freq = 2.0f;
    float bpm = 120.0f;
    float syncDivision = 1.0f;

    float delay  = 0.0f;
    float rise   = 0.0f;
    float amount = 1.0f;

    enum class PlayMode { FREE, TRIGGER, ENVELOPE };
    enum class Polarity { BIPOLAR, UNIPOLAR };
    enum class RateMode { HZ, SYNC };

    float process(float sr) override
    {
        if (mode == PlayMode::TRIGGER && retriggerPending) {
            phase = 0.0f;
            elapsed = 0.0f;
            retriggerPending = false;
            finished = false;
        }

        if (mode == PlayMode::ENVELOPE && finished)
            return 0.0f;

        elapsed += 1.0f / sr;
        if (elapsed < delay)
            return 0.0f;

        float t = elapsed - delay;

        phase += computePhaseIncrement(sr);
        if (phase >= 1.0f) {
            if (mode == PlayMode::ENVELOPE) {
                phase = 1.0f;
                finished = true;
            } else {
                phase -= 1.0f;
            }
        }

        float pos = phase * LFO_TABLE_SIZE;
        int i0 = (int)pos & (LFO_TABLE_SIZE - 1);
        int i1 = (i0 + 1) & (LFO_TABLE_SIZE - 1);
        float frac = pos - (float)i0;

        float raw = LFO_SHAPE[i0] * (1.0f - frac) + LFO_SHAPE[i1] * frac;
        raw = applyPolarity(raw);

        float fade = (rise <= 0.0f) ? 1.0f : std::min(t / rise, 1.0f);
        return raw * fade * amount;
    }

    void retrigger() { retriggerPending = true; }
    void setPlayMode(PlayMode m) { mode = m; }

private:
    float computePhaseIncrement(float sr)
    {
        if (rateMode == RateMode::HZ)
            return freq / sr;

        float bps = bpm / 60.0f;
        float cps = bps / syncDivision;
        return cps / sr;
    }

    float applyPolarity(float v)
    {
        if (polarity == Polarity::UNIPOLAR)
            return 0.5f * (v + 1.0f);
        return v;
    }

private:
    float phase = 0.0f;
    float elapsed = 0.0f;

    PlayMode mode = PlayMode::FREE;
    Polarity polarity = Polarity::BIPOLAR;
    RateMode rateMode = RateMode::HZ;

    bool retriggerPending = false;
    bool finished = false;
};
