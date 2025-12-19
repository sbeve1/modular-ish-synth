#pragma once
#include "audio/Module.h"
#include "core/CONSTANTS.h"
#include <cmath>

class Oscillator : public Module {
public:

    // ============================
    // WAVEFORMS
    // ============================
    enum class Waveform {
        Sine,
        Saw,
        Square,
        Triangle
    };

    // ============================
    // BASE PARAMETERS (UI / stored)
    // ============================
    float freqBase        = 440.0f;  // Hz
    float fineBase        = 0.0f;    // cents
    float levelBase       = 1.0f;    // amplitude
    float phaseOffsetBase = 0.0f;    // 0..1 normalized offset
    float panBase         = 0.0f;    // -1..+1

    // ============================
    // MOD-SHADOW PARAMETERS
    // (ModMatrix writes into these)
    // ============================
    float freqMod        = 440.0f;
    float fineMod        = 0.0f;
    float levelMod       = 1.0f;
    float phaseOffsetMod = 0.0f;
    float panMod         = 0.0f;

    // ============================
    // RUNTIME STATE
    // ============================
    float phase = 0.0f;
    Waveform waveform = Waveform::Triangle;

    Oscillator() = default;

    // Reset shadow parameters to base each note
    void resetModValues() {
        freqMod        = freqBase;
        fineMod        = fineBase;
        levelMod       = levelBase;
        phaseOffsetMod = phaseOffsetBase;
        panMod         = panBase;
    }

    // ============================
    // MOD MATRIX POINTERS
    // ============================
    float* getFreqModPtr()        { return &freqMod; }
    float  getFreqBase()    const { return freqBase; }

    float* getFineModPtr()        { return &fineMod; }
    float  getFineBase()    const { return fineBase; }

    float* getLevelModPtr()       { return &levelMod; }
    float  getLevelBase()   const { return levelBase; }

    float* getPhaseOffsetModPtr() { return &phaseOffsetMod; }
    float  getPhaseOffsetBase() const { return phaseOffsetBase; }

    float* getPanModPtr()         { return &panMod; }
    float  getPanBase()     const { return panBase; }

    // ============================
    // Helper: MIDI note → frequency
    // ============================
    float midiNoteToFreq(int n) {
        return 440.0f * powf(2.0f, (n - 69) / 12.0f);
    }

    // Set frequency base (UI/preset)
    void setFrequency(float f) {
        freqBase = f;
        freqMod  = f;
    }

    // ============================
    // AUDIO DSP
    // ============================
    Sample process(float sr, const Sample&) override
    {
        float freq = freqMod * powf(2.0f, fineMod / 1200.0f);

        float s = 0.0f;

        switch (waveform) {
            case Waveform::Sine:
                s = sinf(phase + phaseOffsetMod * TWO_PI);
                break;

            case Waveform::Saw:
                s = (2.0f * (phase * INV_2PI)) - 1.0f;
                break;

            case Waveform::Square:
                s = (phase < PI ? 1.0f : -1.0f);
                break;

            case Waveform::Triangle:
                s = 2.0f * fabsf((phase * INV_PI) - 1.0f) - 1.0f;
                break;
        }

        s *= levelMod;

        // Advance phase
        phase += (TWO_PI * freq) / sr;
        if (phase >= TWO_PI)
            phase -= TWO_PI;

        // Stereo panning
        float angle = (panMod * 0.5f + 0.5f) * HALF_PI;
        float left  = cosf(angle);
        float right = sinf(angle);

        return { s * left, s * right };
    }
};
