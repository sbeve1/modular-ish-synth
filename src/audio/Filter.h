#pragma once
#include "audio/Module.h"
#include "core/CONSTANTS.h"
#include <cmath>
#include <algorithm>

class Filter : public Module {
public:
    enum class Mode {
        BYPASS,
        LP,
        HP,
        BP,
        NOTCH
    };

    Filter()
    {
        cutoffBase    = 1000.0f;
        resonanceBase = 0.0f;
        driveBase     = 0.0f;
        mixBase       = 1.0f;
        fatBase       = 0.0f;

        resetModValues();
        mode = Mode::LP;
    }

    Sample process(float sr, const Sample& in) override
    {
        if (mode == Mode::BYPASS)
            return in;

        bool oversample = (modFat > 0.5f);
        Sample filtered = oversample
            ? processSingle(sr * 2.0f, in)
            : processSingle(sr, in);

        float l = filtered.left  * modMix + in.left  * (1.0f - modMix);
        float r = filtered.right * modMix + in.right * (1.0f - modMix);

        return { sat(l), sat(r) };
    }

    // ============================
    // MOD TARGET POINTERS
    // ============================
    float* getCutoffPtr() { return &modCutoff; }
    float  getCutoffBase() const { return cutoffBase; }

    float* getResPtr() { return &modRes; }
    float  getResBase() const { return resonanceBase; }

    float* getDrivePtr() { return &modDrive; }
    float  getDriveBase() const { return driveBase; }

    float* getMixPtr() { return &modMix; }
    float  getMixBase() const { return mixBase; }

    float* getFatPtr() { return &modFat; }
    float  getFatBase() const { return fatBase; }

    void resetModValues()
    {
        modCutoff = cutoffBase;
        modRes    = resonanceBase;
        modDrive  = driveBase;
        modMix    = mixBase;
        modFat    = fatBase;
    }

private:
    struct State { float ic1 = 0, ic2 = 0; } stL, stR;

    Mode mode;

    float cutoffBase, resonanceBase, driveBase, mixBase, fatBase;
    float modCutoff,  modRes,  modDrive,  modMix,  modFat;

    inline float sat(float x) const { return x / (1.0f + fabsf(x)); }

    float processSample(float x, float sr, State& s)
    {
        float fc = std::clamp(modCutoff, MIN_FREQ, sr * NYQUIST_SCALE);
        float g = tanf(PI * fc / sr);
        float R = FILTER_RESONANCE_MIN + modRes * FILTER_RESONANCE_RANGE;

        float a = 1.0f + 2.0f * R * g + g * g;
        float v3 = (x - 2.0f * R * g * s.ic1 - g * g * s.ic2) / a;
        float v1 = g * v3 + s.ic1;
        float v2 = g * v1 + s.ic2;

        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;

        return sat(v2);
    }

    Sample processSingle(float sr, const Sample& in)
    {
        float l = sat(in.left  * (1.0f + modDrive));
        float r = sat(in.right * (1.0f + modDrive));
        return { processSample(l, sr, stL), processSample(r, sr, stR) };
    }
};
