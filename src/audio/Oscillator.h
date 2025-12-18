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

    Sample process(float sr, const Sample& in) override { 
        (void) in; // ignore warnings from Oscillator not using an input

        float s = 0.0f;

        switch (waveform) {
            case Waveform::Sine:
                s = sinf(phase);
                break;

            case Waveform::Saw:
                // ranges from -1 to +1 as phase goes 0..TWO_PI
                s = (2.0f * (phase / TWO_PI)) - 1.0f;
                break;

            case Waveform::Square:
                // 50% duty cycle square wave
                s = (phase < PI ? 1.0f : -1.0f);
                break;

            case Waveform::Triangle:
                // triangle wave from -1 to +1
                s = 2.0f * fabsf((phase / PI) - 1.0f) - 1.0f;
                break;
        }


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
