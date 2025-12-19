enum class ModSourceID : int {
    NONE = -1,

    // --- ENVELOPES ---
    ENV1 = 0,
    ENV2 = 1,
    ENV3 = 2,

    // --- LFOS ---
    LFO1 = 3,
    LFO2 = 4,
    LFO3 = 5,
    LFO4 = 6,

    // --- PERFORMANCE ---
    NOTE = 7,
    VELOCITY = 8,
    MOD_WHEEL = 9,
    AFTERTOUCH = 10,
    PITCH_BEND = 11,
    ALTERNATE = 12,

    // --- RANDOM ---
    RANDOM_PER_VOICE = 13,
    RANDOM_PER_NOTE = 14,

    // --- MACROS ---
    MACRO1 = 15,
    MACRO2 = 16,
    MACRO3 = 17,
    MACRO4 = 18,

    COUNT = 19
};
