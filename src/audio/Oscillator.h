#pragma once
#include "audio/Module.h"
#include "core/CONSTANTS.h"
#include <cmath>

class Oscillator : public Module {
public:

    enum class Waveform {
        Sine,
        Saw,
        Square,
        Triangle
    };

    void setFrequency(float f) {
        freq = f;
    }
    float midiNoteToFreq(int n) {
        return 440.0f * powf(2.0f, (n - 69) / 12.0f);
    }

    inline float normalize(float s) const {
        switch (waveform) {
            case Waveform::Sine:     return s * 1.0f;
            case Waveform::Triangle: return s * 1.22f;   // 1 / 0.577 RMS
            case Waveform::Saw:      return s * 1.22f;
            case Waveform::Square:   return s * 0.707f;  // RMS match to sine
        }
        return s;
    }

    Sample process(float sr, const Sample& in) override {
        (void)in;

        float s = 0.0f;

        switch (waveform) {
            case Waveform::Sine:
                s = sinf(phase);
                break;

            case Waveform::Saw:
                s = (2.0f * (phase / TWO_PI)) - 1.0f;
                break;

            case Waveform::Square:
                s = (phase < PI ? 1.0f : -1.0f);
                break;

            case Waveform::Triangle:
                s = 2.0f * fabsf((phase / PI) - 1.0f) - 1.0f;
                break;
        }

        s = normalize(s);  // *** BAKED-IN NORMALIZATION ***

        phase += (TWO_PI * freq) / sr;
        if (phase >= TWO_PI)
            phase -= TWO_PI;

        return { s, s };
    }

    void setWaveform(Waveform w) { waveform = w; }
    float* getFreqPtr() { return &freq; }

private:
    Waveform waveform = Waveform::Triangle;
    float phase = 0.f;
    float freq = 440.f;
};
