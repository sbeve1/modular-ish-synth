#pragma once

struct OscParams {
    // ------------------------
    // PITCH
    // ------------------------
    float freqBase     = 0.0f;
    float freqMod      = 0.0f;

    float semitoneBase = 0.0f;
    float semitoneMod  = 0.0f;

    float fineBase     = 0.0f;
    float fineMod      = 0.0f;

    // ------------------------
    // AMP / STEREO
    // ------------------------
    float levelBase    = 1.0f;
    float levelMod     = 0.0f;

    float panBase      = 0.0f;
    float panMod       = 0.0f;

    // ------------------------
    // WAVETABLE
    // ------------------------
    float wtPosBase    = 0.0f;
    float wtPosMod     = 0.0f;

    // ------------------------
    // UNISON
    // ------------------------
    float unisonVoices = 1.0f;
    float detune       = 0.0f;
    float stereoSpread = 0.0f;
    float phaseSpread  = 0.0f;
    bool  randomPhase  = true;
};
