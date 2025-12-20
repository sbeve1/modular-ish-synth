#pragma once

enum class ModSourceID : int {

    NONE = -1,

    // ========================
    // ENVELOPES
    // ========================
    ENV1 = 0,
    ENV2 = 1,
    ENV3 = 2,
    ENV4 = 3,

    // ========================
    // LFOS
    // ========================
    LFO1 = 4,
    LFO2 = 5,
    LFO3 = 6,
    LFO4 = 7,
    LFO5 = 8,
    LFO6 = 9,

    // ========================
    // PERFORMANCE
    // ========================
    NOTE        = 10,
    VELOCITY    = 11,
    MOD_WHEEL   = 12,
    AFTERTOUCH  = 13,
    PITCH_BEND  = 14,
    ALTRNATE    = 15,

    // ========================
    // RANDOM
    // ========================
    RANDOM_PER_VOICE = 16,
    RANDOM_PER_NOTE  = 17,

    // ========================
    // MACROS
    // ========================
    MACRO1 = 18,
    MACRO2 = 19,
    MACRO3 = 20,
    MACRO4 = 21,
    MACRO5 = 22,
    MACRO6 = 23,
    MACRO7 = 24,
    MACRO8 = 25,

    COUNT = 26
};
