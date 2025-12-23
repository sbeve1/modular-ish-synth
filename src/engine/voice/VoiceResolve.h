#pragma once
#include <cmath>
#include <algorithm>

#include "engine/OscParams.h"
#include "audio/Oscillator.h"
#include "core/CONSTANTS.h"

struct VoiceResolve {

    static inline OscResolved resolveOsc(const OscParams& p, float sr)
    {
        OscResolved r{};

        float midi =
            (float)p.midiNote
            + p.semitoneBase + p.semitoneMod
            + (p.fineBase + p.fineMod) / 100.0f;

        r.freq = 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
        r.freq = std::clamp(r.freq, MIN_FREQ, sr * 0.45f);

        r.wtPos = std::clamp(p.wtPosBase + p.wtPosMod, 0.0f, 1.0f);
        r.pan   = std::clamp(p.panBase   + p.panMod,   -1.0f, 1.0f);
        r.level = std::max(p.levelBase   + p.levelMod, 0.0f);

        r.unison = std::clamp(
            (int)std::round(p.unisonVoices),
            1,
            Oscillator::MAX_UNISON
        );

        r.detune       = p.detune;
        r.stereoSpread = p.stereoSpread;
        r.phaseSpread  = p.phaseSpread;

        return r;
    }
};
