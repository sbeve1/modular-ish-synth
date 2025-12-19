#pragma once
#include "audio/Module.h"
#include "core/CONSTANTS.h"
#include <cmath>

class Filter : public Module {
public:
    enum class Mode {
        BYPASS,
        LP12,
        LP24,
        HP12,
        BP12,
        NOTCH
    };

    Filter()
    {
        setCutoff(1000.0f);
        setResonance(0.0f);
        setDrive(0.0f);
        setMode(Mode::LP12);
        setOversample(false);
    }

    Sample process(float sampleRate, const Sample& in) override
    {
        if (mode == Mode::BYPASS)
            return in;

        if (oversample2x) {
            Sample s1 = processSingle(sampleRate * 2.0f, in);
            Sample s2 = processSingle(sampleRate * 2.0f, in);
            return { (s1.left + s2.left) * 0.5f, (s1.right + s2.right) * 0.5f };
        }
        else {
            return processSingle(sampleRate, in);
        }
    }

    void setCutoff(float c) { cutoff = std::max(20.0f, std::min(c, 20000.0f)); }
    void setResonance(float r) { resonance = std::max(0.0f, std::min(r, 1.0f)); }
    void setDrive(float d) { drive = std::max(0.0f, d); }
    void setMode(Mode m) { mode = m; }
    void setOversample(bool v) { oversample2x = v; }

    float* getCutoffPtr()    { return &cutoff; }
    float* getResonancePtr() { return &resonance; }

private:
    struct State {
        float lp = 0.0f;
        float bp = 0.0f;
    };

    State stL1, stR1;
    State stL2, stR2;

    Mode mode = Mode::LP12;

    float cutoff = 1000.0f;
    float resonance = 0.0f;
    float drive = 0.0f;
    bool oversample2x = false;

    inline float softsat(float x) const
    {
        return x / (1.0f + fabsf(x));
    }

    inline void svfProcessSample(float in, float sr, State& st, float& out_lp, float& out_bp)
    {
        float fc = std::max(20.0f, std::min(cutoff, sr * 0.49f));
        float f  = 2.0f * sinf(PI * fc / sr);
        float d  = 1.0f - resonance * 0.98f;

        float hp = in - st.lp - d * st.bp;
        st.bp += f * hp;
        st.lp += f * st.bp;

        out_lp = st.lp;
        out_bp = st.bp;
    }

    Sample processSingle(float sr, const Sample& in)
    {
        float il = in.left  * (1.0f + drive);
        float ir = in.right * (1.0f + drive);

        if (drive > 0.0f) {
            il = softsat(il);
            ir = softsat(ir);
        }

        float lp1_l, bp1_l;
        float lp1_r, bp1_r;

        svfProcessSample(il, sr, stL1, lp1_l, bp1_l);
        svfProcessSample(ir, sr, stR1, lp1_r, bp1_r);

        float lp2_l = 0.0f, bp2_l = 0.0f;
        float lp2_r = 0.0f, bp2_r = 0.0f;

        if (mode == Mode::LP24) {
            svfProcessSample(lp1_l, sr, stL2, lp2_l, bp2_l);
            svfProcessSample(lp1_r, sr, stR2, lp2_r, bp2_r);
        }

        float outL = 0.0f;
        float outR = 0.0f;

        const float normK = 4.0f;
        float comp = 1.0f / (1.0f + resonance * normK);

        switch (mode) {
            case Mode::LP12:
                outL = lp1_l * comp;
                outR = lp1_r * comp;
                break;

            case Mode::LP24:
                outL = lp2_l * comp;
                outR = lp2_r * comp;
                break;

            case Mode::HP12: {
                float fc = std::max(20.0f, std::min(cutoff, sr * 0.49f));
                float f  = 2.0f * sinf(PI * fc / sr);
                float d  = 1.0f - resonance * 0.98f;
                float hpL = il - lp1_l - d * bp1_l;
                float hpR = ir - lp1_r - d * bp1_r;
                outL = hpL * comp;
                outR = hpR * comp;
                break;
            }

            case Mode::BP12:
                outL = bp1_l * comp;
                outR = bp1_r * comp;
                break;

            case Mode::NOTCH: {
                float notchL = il - resonance * bp1_l;
                float notchR = ir - resonance * bp1_r;
                outL = notchL * comp;
                outR = notchR * comp;
                break;
            }

            default:
                outL = in.left;
                outR = in.right;
                break;
        }

        outL = softsat(outL);
        outR = softsat(outR);

        return { outL, outR };
    }
};
