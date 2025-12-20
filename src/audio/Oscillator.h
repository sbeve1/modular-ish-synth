#pragma once

#include "audio/Module.h"
#include "audio/Wavetable.h"
#include "core/CONSTANTS.h"
#include <cmath>
#include <algorithm>

class Oscillator : public Module {
public:
    // ============================
    // BASE PARAMETERS
    // ============================
    float freqBase     = 440.0f;
    float semitoneBase = 0.0f;
    float fineBase     = 0.0f;

    // Wavetable position (0..1)
    float positionBase = 0.0f;

    // ============================
    // MOD SHADOWS
    // ============================
    float freqMod     = 440.0f;
    float semitoneMod = 0.0f;
    float fineMod     = 0.0f;
    float positionMod = 0.0f;

    float phase = 0.0f;

    // ============================
    // WAVETABLE
    // ============================
    const Wavetable* wavetable = nullptr;

    void resetModValues()
    {
        freqMod     = freqBase;
        semitoneMod = semitoneBase;
        fineMod     = fineBase;
        positionMod = positionBase;
    }

    // ============================
    // MOD TARGET POINTERS
    // ============================
    float* getFreqPtr()        { return &freqMod; }
    float  getFreqBase() const { return freqBase; }

    float* getSemitonePtr()        { return &semitoneMod; }
    float  getSemitoneBase() const { return semitoneBase; }

    float* getFinePtr()        { return &fineMod; }
    float  getFineBase() const { return fineBase; }

    float* getPositionPtr()        { return &positionMod; }
    float  getPositionBase() const { return positionBase; }

    float midiNoteToFreq(int n) const
    {
        return 440.0f * powf(2.0f, (n - 69) / 12.0f);
    }

    // ============================
    // DSP
    // ============================
    Sample process(float sr, const Sample&) override
    {
        if (!wavetable || wavetable->numFrames == 0)
            return { 0.f, 0.f };

        float pitchRatio =
            powf(2.0f, semitoneMod / 12.0f) *
            powf(2.0f, fineMod / 1200.0f);

        float freq = freqMod * pitchRatio;

        phase += (TWO_PI * freq) / sr;
        if (phase >= TWO_PI)
            phase -= TWO_PI;

        float phaseNorm = phase * INV_2PI;
        float pos = std::clamp(positionMod, 0.0f, 1.0f);

        // ============================
        // FRAME INTERPOLATION
        // ============================
        float framePos = pos * (wavetable->numFrames - 1);
        int   f0 = (int)framePos;
        int   f1 = std::min(f0 + 1, wavetable->numFrames - 1);
        float frameFrac = framePos - f0;

        // ============================
        // SAMPLE INTERPOLATION
        // ============================
        float idx = phaseNorm * wavetable->tableSize;
        int   i0  = (int)idx % wavetable->tableSize;
        int   i1  = (i0 + 1) % wavetable->tableSize;
        float sampleFrac = idx - i0;

        float s00 = wavetable->sample(f0, i0);
        float s01 = wavetable->sample(f0, i1);
        float s10 = wavetable->sample(f1, i0);
        float s11 = wavetable->sample(f1, i1);

        // ============================
        // EXPLICIT BILINEAR INTERPOLATION
        // ============================
        float s0 = s00 + (s01 - s00) * sampleFrac;
        float s1 = s10 + (s11 - s10) * sampleFrac;
        float s  = s0  + (s1  - s0)  * frameFrac;

        return { s, s };
    }
};
