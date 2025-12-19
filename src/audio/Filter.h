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

    Filter(){
        cutoff = 1000.0f;
        resonance = 0.0f;
        drive = 0.0f;
        postDrive = 0.0f;
        mix = 1.0f;
        keytrack = 0.0f;
        mode = Mode::LP;
        modeParam = 0.0f;
        oversample2x = false;
    }

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

        // --- Dry/Wet Mix ---
        float l = filtered.left  * mix + in.left  * (1.0f - mix);
        float r = filtered.right * mix + in.right * (1.0f - mix);

        // --- Post-drive saturation ---
        l *= (1.0f + postDrive);
        r *= (1.0f + postDrive);

        l = l / (1.0f + fabsf(l));
        r = r / (1.0f + fabsf(r));

        return { l, r };
    }

    // -----------------------------
    // Pointer getters for ModMatrix
    // -----------------------------
    float* getCutoffPtr()    { return &cutoff; }
    float* getResonancePtr() { return &resonance; }
    float* getDrivePtr()     { return &drive; }
    float* getMixPtr()       { return &mix; }
    float* getKeytrackPtr()  { return &keytrack; }
    float* getPostDrivePtr() { return &postDrive; }

private:
    struct State {
        float ic1eq = 0.0f;
        float ic2eq = 0.0f;
    };

    State stL, stR;

    Mode  mode;
    float cutoff;
    float resonance;
    float drive;
    float postDrive;
    float mix;
    float keytrack;
    float modeParam;
    bool  oversample2x;

    inline float sat(float x) const {
        return x / (1.0f + fabsf(x));
    }

    inline float mapRes(float r) const {
        return FILTER_RESONANCE_MIN + r * FILTER_RESONANCE_RANGE;
    }

    float processSample(float x, float sr, State& s) {
        int im = (int)modeParam;
        im = std::clamp(im, 0, 3);

        switch (im) {
            case 0: mode = Mode::LP;    break;
            case 1: mode = Mode::HP;    break;
            case 2: mode = Mode::BP;    break;
            case 3: mode = Mode::NOTCH; break;
        }

        // Apply simple keytracking
        float trackedCutoff = cutoff;

        // 440 Hz reference MIDI 69
        // Use 0..1 input keytrack meaning 0–100% following
        trackedCutoff *= powf(2.0f, keytrack * ((currentNote - 60) / 12.0f));

        float fc = std::clamp(trackedCutoff,
                              MIN_FREQ,
                              sr * NYQUIST_SCALE);

        float g = tanf(PI * fc / sr);
        float R = mapRes(resonance);

        float a = 1.0f + 2.0f * R * g + g * g;
        if (a < EPSILON) a = EPSILON;

        float v3 = (x - 2.0f * R * g * s.ic1eq - g * g * s.ic2eq) / a;
        float v1 = g * v3 + s.ic1eq;
        float v2 = g * v1 + s.ic2eq;

        s.ic1eq = 2.0f * v1 - s.ic1eq;
        s.ic2eq = 2.0f * v2 - s.ic2eq;

        float lp = v2;
        float bp = v1;
        float hp = x - 2.0f * R * g * v1 - g * g * v2;

        float out = 0.0f;
        switch (mode) {
            case Mode::LP:    out = lp; break;
            case Mode::HP:    out = hp; break;
            case Mode::BP:    out = bp; break;
            case Mode::NOTCH: out = x - bp * R; break;
            default:          out = x;  break;
        }

        out *= 1.0f / (1.0f + resonance * FILTER_RESONANCE_K);
        return sat(out);
    }

    Sample processSingle(float sr, const Sample& in) {
        float il = in.left  * (1.0f + drive);
        float ir = in.right * (1.0f + drive);

        if (drive > 0.0f) {
            il = sat(il);
            ir = sat(ir);
        }

        return {
            processSample(il, sr, stL),
            processSample(ir, sr, stR)
        };
    }

public:
    // Voice sets this on note start
    int currentNote = 60;
};
