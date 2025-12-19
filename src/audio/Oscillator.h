#pragma once
#include "audio/Module.h"
#include "core/CONSTANTS.h"
#include <algorithm>
#include <cmath>

class Oscillator : public Module {
public:
    enum class Waveform {
        Sine,
        Saw,
        Square,
        Triangle
    };

    // -----------------------
    // Modulate-able parameters
    // -----------------------
    float freq = 440.0f;
    float fine = 0.0f;
    float level = 1.0f;
    float phaseOffset = 0.0f;
    float pan = 0.0f;

    // -----------------------
    // Required functions (MISSING EARLIER)
    // -----------------------
    inline float midiNoteToFreq(int n) {
        return 440.0f * powf(2.0f, (n - 69) / 12.0f);
    }

    inline void setFrequency(float f) {
        freq = f;
    }

    // ModMatrix getters
    float* getFreqPtr()        { return &freq; }
    float* getFinePtr()        { return &fine; }
    float* getLevelPtr()       { return &level; }
    float* getPhaseOffsetPtr() { return &phaseOffset; }
    float* getPanPtr()         { return &pan; }

    void setWaveform(Waveform w) { waveform = w; }

    // -----------------------
    // Audio-rate DSP
    // -----------------------
    Sample process(float sr, const Sample&) override
    {
        float detunedFreq = freq * powf(2.0f, fine / 1200.0f);

        float s = 0.0f;

        switch (waveform) {
            case Waveform::Sine:     s = sinf(phase); break;
            case Waveform::Saw:      s = (2.0f * (phase * INV_2PI)) - 1.0f; break;
            case Waveform::Square:   s = (phase < PI ? 1.0f : -1.0f); break;
            case Waveform::Triangle: s = 2.0f * fabsf((phase * INV_PI) - 1.0f) - 1.0f; break;
        }

        s *= level;

        phase += (TWO_PI * detunedFreq) / sr;
        if (phase >= TWO_PI)
            phase -= TWO_PI;

        float angle = (pan * 0.5f + 0.5f) * HALF_PI;
        float l = cosf(angle);
        float r = sinf(angle);

        return { s * l, s * r };
    }

public:
    float phase = 0.0f;
    Waveform waveform = Waveform::Triangle;
};
