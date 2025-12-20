#pragma once

#include <cmath>
#include <algorithm>

#include "core/CONSTANTS.h"
#include "core/Sample.h"
#include "audio/Wavetable.h"
#include "engine/OscParams.h"

class Oscillator {
public:
    // ==================================================
    // DSP STATE ONLY
    // ==================================================
    float phase = 0.0f;

    // ==================================================
    // WAVETABLE
    // ==================================================
    const Wavetable* wavetable = nullptr;

    // ==================================================
    // HELPERS
    // ==================================================
    float midiNoteToFreq(int n) const
    {
        return 440.0f * powf(2.0f, (n - 69) / 12.0f);
    }

    // ==================================================
    // PROCESS (PURE DSP)
    // ==================================================
    Sample process(float sr, const OscParams& p)
    {
        if (!wavetable || wavetable->numFrames == 0)
            return {};

        // ----------------------------
        // PITCH
        // ----------------------------
        // semitone + fine (cents)
        float pitchRatio =
            powf(2.0f, p.semitoneMod / 12.0f) *
            powf(2.0f, p.fineMod / 1200.0f);

        float freq = p.freqMod * pitchRatio;

        phase += (TWO_PI * freq) / sr;
        if (phase >= TWO_PI)
            phase -= TWO_PI;

        float phaseNorm = phase * INV_2PI;

        // ----------------------------
        // WAVETABLE POSITION
        // ----------------------------
        float wtPos = std::clamp(p.wtPosMod, 0.0f, 1.0f);

        float framePos = wtPos * (wavetable->numFrames - 1);
        int   f0 = (int)framePos;
        int   f1 = std::min(f0 + 1, wavetable->numFrames - 1);
        float frameFrac = framePos - f0;

        // ----------------------------
        // SAMPLE INTERPOLATION
        // ----------------------------
        float idx = phaseNorm * wavetable->tableSize;
        int   i0  = (int)idx % wavetable->tableSize;
        int   i1  = (i0 + 1) % wavetable->tableSize;
        float sampleFrac = idx - i0;

        float s00 = wavetable->sample(f0, i0);
        float s01 = wavetable->sample(f0, i1);
        float s10 = wavetable->sample(f1, i0);
        float s11 = wavetable->sample(f1, i1);

        float s0 = s00 + (s01 - s00) * sampleFrac;
        float s1 = s10 + (s11 - s10) * sampleFrac;
        float s  = s0  + (s1  - s0)  * frameFrac;

        // ----------------------------
        // LEVEL
        // ----------------------------
        s *= p.levelMod;

        // ----------------------------
        // PAN (-1 .. +1)
        // equal-power panning
        // ----------------------------
        float pan = std::clamp(p.panMod, -1.0f, 1.0f);

        float angle = (pan + 1.0f) * 0.25f * PI; // -1..1 → 0..π/2
        float l = cosf(angle);
        float r = sinf(angle);

        return { s * l, s * r };
    }
};
