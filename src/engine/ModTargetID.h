#pragma once

enum class ModTargetID : int {

    // ========================
    // FILTER 1
    // ========================
    FILTER1_CUTOFF = 0,
    FILTER1_RES,
    FILTER1_DRIVE,
    FILTER1_FAT,
    FILTER1_MIX,
    FILTER1_PAN,
    FILTER1_LEVEL,

    // ========================
    // FILTER 2
    // ========================
    FILTER2_CUTOFF,
    FILTER2_RES,
    FILTER2_DRIVE,
    FILTER2_FAT,
    FILTER2_MIX,
    FILTER2_PAN,
    FILTER2_LEVEL,

    // ========================
    // OSC A
    // ========================
    OSC_A_FREQ,
    OSC_A_SEMITONE,
    OSC_A_FINE,
    OSC_A_LEVEL,
    OSC_A_PAN,
    OSC_A_WT_POS,

    // ========================
    // OSC B
    // ========================
    OSC_B_FREQ,
    OSC_B_SEMITONE,
    OSC_B_FINE,
    OSC_B_LEVEL,
    OSC_B_PAN,
    OSC_B_WT_POS,

    // ========================
    // OSC C
    // ========================
    OSC_C_FREQ,
    OSC_C_SEMITONE,
    OSC_C_FINE,
    OSC_C_LEVEL,
    OSC_C_PAN,
    OSC_C_WT_POS,

    // ========================
    // SUB
    // ========================
    SUB_FREQ,
    SUB_SEMITONE,
    SUB_FINE,
    SUB_LEVEL,
    SUB_PAN,
    SUB_WT_POS,

    // ========================
    // NOISE
    // ========================
    NOISE_PITCH,
    NOISE_LEVEL,
    NOISE_PAN,

    // ========================
    // ROUTING
    // ========================
    OSC_A_TO_DIRECT,
    OSC_A_TO_MAIN,
    OSC_A_TO_BUS1,
    OSC_A_TO_BUS2,

    OSC_B_TO_DIRECT,
    OSC_B_TO_MAIN,
    OSC_B_TO_BUS1,
    OSC_B_TO_BUS2,

    OSC_C_TO_DIRECT,
    OSC_C_TO_MAIN,
    OSC_C_TO_BUS1,
    OSC_C_TO_BUS2,

    SUB_TO_DIRECT,
    SUB_TO_MAIN,
    SUB_TO_BUS1,
    SUB_TO_BUS2,

    NOISE_TO_DIRECT,
    NOISE_TO_MAIN,
    NOISE_TO_BUS1,
    NOISE_TO_BUS2,

    COUNT
};
