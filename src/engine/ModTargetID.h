enum class ModTargetID : int {

    // --- AMP ---
    VCA_GAIN = 0,
    VCA_PAN = 1,

    // --- FILTER ---
    FILTER_CUTOFF = 2,
    FILTER_RESONANCE = 3,
    FILTER_DRIVE = 4,
    FILTER_MORPH = 5,

    // --- OSCILLATOR ---
    OSC_FREQUENCY = 6,
    OSC_FINE = 7,
    OSC_PHASE = 8,
    OSC_LEVEL = 9,
    OSC_WAVEPOS = 10,
    OSC_PAN = 11,

    // --- ENVELOPES ---
    ENV1_ATTACK = 12,
    ENV1_DECAY = 13,
    ENV1_SUSTAIN = 14,
    ENV1_RELEASE = 15,

    ENV2_ATTACK = 16,
    ENV2_DECAY = 17,
    ENV2_SUSTAIN = 18,
    ENV2_RELEASE = 19,

    ENV3_ATTACK = 20,
    ENV3_DECAY = 21,
    ENV3_SUSTAIN = 22,
    ENV3_RELEASE = 23,

    // --- LFO PARAMETERS ---
    LFO1_RATE = 24,
    LFO1_WARP = 25,
    LFO1_PHASE = 26,

    LFO2_RATE = 27,
    LFO2_WARP = 28,
    LFO2_PHASE = 29,

    LFO3_RATE = 30,
    LFO3_WARP = 31,
    LFO3_PHASE = 32,

    LFO4_RATE = 33,
    LFO4_WARP = 34,
    LFO4_PHASE = 35,

    COUNT = 36
};
