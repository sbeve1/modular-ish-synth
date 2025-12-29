#pragma once
#include <cmath>
#include <algorithm>

#include "engine/Params/OscParams.h"
#include "engine/Params/NoiseParams.h"
#include "Resolved.h"
#include "core/CONSTANTS.h"

struct VoiceResolve {

    static inline OscResolved resolveOsc(const OscParams& p, float sr)
    {
        OscResolved r{};

        r.mode = p.mode;
        
        float midi =
            (float)p.midiNote
            + p.semitoneBase + p.semitoneMod
            + (p.fineBase + p.fineMod) / 100.0f;

        r.freq = 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
        r.freq = std::clamp(r.freq, MIN_FREQ, sr * 0.45f);

        r.wtPos = std::clamp(p.wtPosBase + p.wtPosMod, 0.0f, 1.0f);
        r.pan   = std::clamp(p.panBase   + p.panMod,   -1.0f, 1.0f);
        r.level = std::max(p.levelBase   + p.levelMod, 0.0f);

        r.unison = std::clamp((int)std::round(p.unisonVoices), 1, MAX_UNISON);


        r.detune       = p.detune;
        r.stereoSpread = p.stereoSpread;
        r.phaseSpread  = p.phaseSpread;

        return r;
    }

    static inline NoiseResolved resolveNoise(const NoiseParams& p)
    {
        NoiseResolved r{};

        r.mode = p.mode;

        r.level = std::clamp(
            std::max(p.levelBase + p.levelMod, 0.0f),
            0.0f, 1.0f
        );

        r.pan = std::clamp(p.panBase + p.panMod, -1.0f, 1.0f);

        float norm = std::clamp(p.pitchBase + p.pitchMod, 0.0f, 1.0f);
        float semitones = (norm - 0.5f) * 48.0f;
        r.pitch = std::pow(2.0f, semitones / 12.0f);

        return r;
    }


    static inline FilterResolved resolveFilter(const FilterParams& p, float sr)
    {
        FilterResolved r{};

        float cutoff = p.cutoffBase + p.cutoffMod;
        r.cutoffHz = std::clamp(cutoff, 20.0f, sr * 0.45f);

        float res = p.resBase + p.resMod;
        r.resonance = std::clamp(res, 0.05f, 0.99f);

        return r;
    }


};
