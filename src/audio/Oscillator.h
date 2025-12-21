#pragma once

#include <cmath>
#include <array>
#include <algorithm>
#include <cstdlib>

#include "core/CONSTANTS.h"
#include "core/Sample.h"
#include "audio/Wavetable.h"
#include "engine/OscParams.h"

class Oscillator {
public:
    static constexpr int MAX_UNISON = 8;

    std::array<float, MAX_UNISON> phase{};
    const Wavetable* wavetable = nullptr;

    // ============================
    // MIDI → frequency helper (RESTORED)
    // ============================
    float midiNoteToFreq(int note) const
    {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    void resetPhase(bool randomize)
    {
        for (auto& p : phase)
            p = randomize ? ((float)std::rand() / RAND_MAX) * TWO_PI : 0.0f;
    }

    Sample process(float sr, const OscParams& p)
    {
        if (!wavetable || wavetable->numFrames == 0)
            return {};

        int voices = std::clamp(
            (int)std::round(p.unisonVoices),
            1,
            MAX_UNISON
        );

        // ----------------------------
        // PITCH (base + mod semantics)
        // ----------------------------
        float semitone = p.semitoneBase + p.semitoneMod;
        float fine     = p.fineBase + p.fineMod;

        float pitchRatio =
            std::pow(2.0f, semitone / 12.0f) *
            std::pow(2.0f, fine / 1200.0f);

        float baseFreq = std::clamp(
            (p.freqBase + p.freqMod) * pitchRatio,
            MIN_FREQ,
            sr * 0.45f
        );

        // ----------------------------
        // WAVETABLE POSITION
        // ----------------------------
        float wtPos = std::clamp(p.wtPosBase + p.wtPosMod, 0.0f, 1.0f);
        float framePos = wtPos * (wavetable->numFrames - 1);
        int f0 = (int)framePos;
        int f1 = std::min(f0 + 1, wavetable->numFrames - 1);
        float frameFrac = framePos - f0;

        float outL = 0.0f;
        float outR = 0.0f;

        // ----------------------------
        // UNISON LOOP
        // ----------------------------
        for (int i = 0; i < voices; ++i) {
            float spread = (voices == 1) ? 0.0f
                : (float(i) / (voices - 1) * 2.0f - 1.0f);

            float detuneRatio = std::pow(2.0f, (spread * p.detune) / 1200.0f);
            float freq = baseFreq * detuneRatio;

            phase[i] += TWO_PI * freq / sr;
            if (phase[i] >= TWO_PI)
                phase[i] -= TWO_PI;

            float ph = phase[i] * INV_2PI;
            float idx = ph * wavetable->tableSize;
            int i0 = (int)idx & (wavetable->tableSize - 1);
            int i1 = (i0 + 1) & (wavetable->tableSize - 1);
            float frac = idx - i0;

            float s00 = wavetable->sample(f0, i0);
            float s01 = wavetable->sample(f0, i1);
            float s10 = wavetable->sample(f1, i0);
            float s11 = wavetable->sample(f1, i1);

            float s0 = s00 + (s01 - s00) * frac;
            float s1 = s10 + (s11 - s10) * frac;
            float s  = s0  + (s1  - s0)  * frameFrac;

            float pan = spread * p.stereoSpread;
            float ang = (pan + 1.0f) * 0.25f * PI;

            outL += s * std::cos(ang);
            outR += s * std::sin(ang);
        }

        // ----------------------------
        // LEVEL (base + mod semantics)
        // ----------------------------
        float norm  = 1.0f / std::sqrt((float)voices);
        float level = std::max(p.levelBase + p.levelMod, 0.0f);

        return {
            outL * level * norm * 0.8f,
            outR * level * norm * 0.8f
        };
    }

};
