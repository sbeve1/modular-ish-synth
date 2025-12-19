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

    Filter() {
        // base (knob) values
        cutoffBase     = 1000.0f;
        resonanceBase  = 0.0f;
        driveBase      = 0.0f;
        postDriveBase  = 0.0f;
        mixBase        = 1.0f;
        keytrackBase   = 0.0f;

        // mod shadow initialized to base
        resetModValues();

        mode = Mode::LP;
        modeParam = 0.0f;
        oversample2x = false;
    }

    // --------------------------------------------------
    // MAIN PROCESS — now uses modShadow values
    // --------------------------------------------------
    Sample process(float sr, const Sample& in) override {
        if (mode == Mode::BYPASS)
            return in;

        Sample filtered;

        if (oversample2x) {
            Sample a = processSingle(sr * OVERSAMPLE_2X, in);
            Sample b = processSingle(sr * OVERSAMPLE_2X, in);
            filtered = { (a.left + b.left) * 0.5f,
                         (a.right + b.right) * 0.5f };
        } else {
            filtered = processSingle(sr, in);
        }

        // Dry/Wet
        float l = filtered.left  * modMix + in.left  * (1.0f - modMix);
        float r = filtered.right * modMix + in.right * (1.0f - modMix);

        // Post-drive
        l *= (1.0f + modPostDrive);
        r *= (1.0f + modPostDrive);

        l = l / (1.0f + fabsf(l));
        r = r / (1.0f + fabsf(r));

        return { l, r };
    }

    // --------------------------------------------------
    // POINTERS FOR MOD MATRIX (modShadow + base)
    // --------------------------------------------------
    float* getCutoffModPtr()     { return &modCutoff; }
    float  getCutoffBase() const { return cutoffBase; }

    float* getResonanceModPtr()     { return &modResonance; }
    float  getResonanceBase() const { return resonanceBase; }

    float* getDriveModPtr()     { return &modDrive; }
    float  getDriveBase() const { return driveBase; }

    float* getPostDriveModPtr() { return &modPostDrive; }
    float  getPostDriveBase() const { return postDriveBase; }

    float* getMixModPtr()       { return &modMix; }
    float  getMixBase() const   { return mixBase; }

    float* getKeytrackModPtr()  { return &modKeytrack; }
    float  getKeytrackBase() const { return keytrackBase; }

    // Used by Voice to reset mod shadow on note start
    void resetModValues() {
        modCutoff    = cutoffBase;
        modResonance = resonanceBase;
        modDrive     = driveBase;
        modPostDrive = postDriveBase;
        modMix       = mixBase;
        modKeytrack  = keytrackBase;
    }

public:
    // Voice sets this during start()
    int currentNote = 60;

private:
    struct State {
        float ic1eq = 0.0f;
        float ic2eq = 0.0f;
    };

    State stL, stR;

    Mode mode;

    // --------- BASE PARAMETERS (never modulated directly) ---------
    float cutoffBase;
    float resonanceBase;
    float driveBase;
    float postDriveBase;
    float mixBase;
    float keytrackBase;

    // --------- MOD-SHADOW PARAMETERS (ModMatrix writes here) ---------
    float modCutoff;
    float modResonance;
    float modDrive;
    float modPostDrive;
    float modMix;
    float modKeytrack;

    float modeParam;
    bool oversample2x;

    inline float sat(float x) const {
        return x / (1.0f + fabsf(x));
    }

    inline float mapRes(float r) const {
        return FILTER_RESONANCE_MIN + r * FILTER_RESONANCE_RANGE;
    }

    float processSample(float x, float sr, State& s) {

        float trackedCutoff = modCutoff;
        trackedCutoff *= powf(2.0f, modKeytrack * ((currentNote - 60) / 12.0f));

        float fc = std::clamp(trackedCutoff,
                              MIN_FREQ,
                              sr * NYQUIST_SCALE);

        float g = tanf(PI * fc / sr);
        float R = mapRes(modResonance);

        float a = 1.0f + 2.0f * R * g + g * g;
        if (a < EPSILON) a = EPSILON;

        float v3 = (x - 2.0f * R * g * s.ic1eq - g * g * s.ic2eq) / a;
        float v1 = g * v3 + s.ic1eq;
        float v2 = g * v1 + s.ic2eq;

        s.ic1eq = 2.0f * v1 - s.ic1eq;
        s.ic2eq = 2.0f * v2 - s.ic2eq;

        float lp = v2;
        float bp = v1;
        float hp = x - 2.0f * R * v1 * g - g * g * v2;

        float out = 0.0f;
        switch (mode) {
            case Mode::LP:    out = lp; break;
            case Mode::HP:    out = hp; break;
            case Mode::BP:    out = bp; break;
            case Mode::NOTCH: out = x - bp * R; break;
            default:          out = x;  break;
        }

        out *= 1.0f / (1.0f + modResonance * FILTER_RESONANCE_K);

        return sat(out);
    }

    Sample processSingle(float sr, const Sample& in) {
        float il = in.left  * (1.0f + modDrive);
        float ir = in.right * (1.0f + modDrive);

        if (modDrive > 0.0f) {
            il = sat(il);
            ir = sat(ir);
        }

        return {
            processSample(il, sr, stL),
            processSample(ir, sr, stR)
        };
    }
};
