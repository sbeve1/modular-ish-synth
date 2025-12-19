#pragma once
#include "control/ControlModule.h"
#include "core/CONSTANTS.h"
#include <cmath>

constexpr int LFO_TABLE_SIZE = 256;
extern float LFO_SHAPE[LFO_TABLE_SIZE];

class LFO : public ControlModule {
public:

    // ============================
    // Public modulatable parameters
    // ============================
    float freq = 2.0f;         // rate in Hz
    float bpm = 120.0f;        // if sync mode
    float syncDivision = 1.0f; // 1/1, 1/2, etc.

    float delay = 0.0f;        // seconds before LFO starts
    float rise  = 0.0f;        // fade-in time
    float amount = 1.0f;       // output depth

    // ============================
    // Modes
    // ============================
    enum class PlayMode { FREE, TRIGGER, ENVELOPE };
    enum class Polarity { BIPOLAR, UNIPOLAR };
    enum class RateMode { HZ, SYNC };

    float process(float sampleRate) override
    {
        // On retrigger
        if (mode == PlayMode::TRIGGER && retriggerPending) {
            phase = 0.0f;
            elapsed = 0.0f;
            retriggerPending = false;
            finished = false;
        }

        // One-shot envelope LFO
        if (mode == PlayMode::ENVELOPE && finished)
            return 0.0f;

        elapsed += 1.0f / sampleRate;

        // Delay handling
        if (elapsed < delay)
            return 0.0f;

        float t = elapsed - delay;

        // Advance phase
        phase += computePhaseIncrement(sampleRate);
        if (phase >= 1.0f) {
            if (mode == PlayMode::ENVELOPE) {
                phase = 1.0f;
                finished = true;
            } else {
                phase -= 1.0f;
            }
        }

        float raw = readTable(phase);
        raw = applyPolarity(raw);

        // Fade-in (rise)
        float fade = (rise <= 0.0f) ? 1.0f : std::min(t / rise, 1.0f);

        return raw * fade * amount;
    }

    // ModMatrix pointers
    float* getFrequencyPtr()     { return &freq; }
    float* getBPMPtr()           { return &bpm; }
    float* getSyncDivisionPtr()  { return &syncDivision; }
    float* getDelayPtr()         { return &delay; }
    float* getRisePtr()          { return &rise; }
    float* getAmountPtr()        { return &amount; }

    // Controls
    void retrigger() { retriggerPending = true; }
    void setPlayMode(PlayMode m) { mode = m; }

private:
    float readTable(float ph)
    {
        float pos = ph * LFO_TABLE_SIZE;
        int i0 = (int)pos & (LFO_TABLE_SIZE - 1);
        int i1 = (i0 + 1) & (LFO_TABLE_SIZE - 1);
        float frac = pos - (float)i0;
        return LFO_SHAPE[i0] * (1.0f - frac) + LFO_SHAPE[i1] * frac;
    }

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
