#pragma once

// Add targets as your synth grows
enum class ModTargetID {
    VCA_GAIN,
    FILTER_CUTOFF,
    OSC_FREQUENCY,
    FILTER_RESONANCE,

    // Add more as needed:
    // ENV_ATTACK,
    // ENV_DECAY,
    // OSC_WAVEPOS,
    // LFO_RATE,
    // FX_DELAY_TIME,
};
