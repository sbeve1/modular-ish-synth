#pragma once

enum class ModTargetID : int {

    // ========================
    // VCA PARAMETERS
    // ========================
    VCA_GAIN = 0,
    VCA_PAN  = 1,

    // ========================
    // FILTER PARAMETERS
    // ========================
    FILTER_CUTOFF     = 2,
    FILTER_RESONANCE  = 3,
    FILTER_DRIVE      = 4,
    FILTER_POST_DRIVE = 5,
    FILTER_MIX        = 6,
    FILTER_KEYTRACK   = 7,

    // ========================
    // OSCILLATOR PARAMETERS
    // ========================
    OSC_FREQUENCY = 8,
    OSC_FINE      = 9,
    OSC_LEVEL     = 10,
    OSC_PHASE_OFF = 11,
    OSC_PAN       = 12,

    // ========================
    // ENVELOPE 1 PARAMETERS
    // ========================
    ENV1_ATTACK  = 13,
    ENV1_HOLD    = 14,
    ENV1_DECAY   = 15,
    ENV1_SUSTAIN = 16,
    ENV1_RELEASE = 17,
    ENV1_AMOUNT  = 18,

    // ========================
    // ENVELOPE 2 PARAMETERS
    // ========================
    ENV2_ATTACK  = 19,
    ENV2_HOLD    = 20,
    ENV2_DECAY   = 21,
    ENV2_SUSTAIN = 22,
    ENV2_RELEASE = 23,
    ENV2_AMOUNT  = 24,

    // ========================
    // ENVELOPE 3 PARAMETERS
    // ========================
    ENV3_ATTACK  = 25,
    ENV3_HOLD    = 26,
    ENV3_DECAY   = 27,
    ENV3_SUSTAIN = 28,
    ENV3_RELEASE = 29,
    ENV3_AMOUNT  = 30,

    // ========================
    // LFO 1 PARAMETERS
    // ========================
    LFO1_RATE     = 31,
    LFO1_BPM      = 32,
    LFO1_SYNC_DIV = 33,
    LFO1_DELAY    = 34,
    LFO1_RISE     = 35,
    LFO1_AMOUNT   = 36,

    // ========================
    // LFO 2 PARAMETERS
    // ========================
    LFO2_RATE     = 37,
    LFO2_BPM      = 38,
    LFO2_SYNC_DIV = 39,
    LFO2_DELAY    = 40,
    LFO2_RISE     = 41,
    LFO2_AMOUNT   = 42,

    // ========================
    // LFO 3 PARAMETERS
    // ========================
    LFO3_RATE     = 43,
    LFO3_BPM      = 44,
    LFO3_SYNC_DIV = 45,
    LFO3_DELAY    = 46,
    LFO3_RISE     = 47,
    LFO3_AMOUNT   = 48,

    // ========================
    // LFO 4 PARAMETERS
    // ========================
    LFO4_RATE     = 49,
    LFO4_BPM      = 50,
    LFO4_SYNC_DIV = 51,
    LFO4_DELAY    = 52,
    LFO4_RISE     = 53,
    LFO4_AMOUNT   = 54,

    COUNT = 55
};
