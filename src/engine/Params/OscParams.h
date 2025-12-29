#pragma once

enum class OscMode : uint8_t {
    WAVETABLE = 0,
    SINE = 1
};

struct OscParams {
    OscMode mode = OscMode::WAVETABLE;
    // ------------------------
    // PITCH
    // ------------------------
    int midiNote = 60;
    
    float semitoneBase = 0.0f;
    float semitoneMod  = 0.0f;

    float fineBase     = 0.0f;
    float fineMod      = 0.0f;

    // ------------------------
    // AMP / STEREO
    // ------------------------
    float levelBase    = dbToLin(-5.0f);
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
