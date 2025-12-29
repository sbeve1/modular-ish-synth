#pragma once

enum class ModTargetID : int {

    // ========================
    // OSC A
    // ========================
    OSC_A_SEMITONE = 0,
    OSC_A_FINE,
    OSC_A_LEVEL,
    OSC_A_PAN,
    OSC_A_WT_POS,

    // ---- UNISON (OSC A) ----
    OSC_A_UNISON,
    OSC_A_DETUNE,
    OSC_A_STEREO,
    OSC_A_PHASE,

    // ========================
    // OSC B
    // ========================
    OSC_B_SEMITONE,
    OSC_B_FINE,
    OSC_B_LEVEL,
    OSC_B_PAN,
    OSC_B_WT_POS,

    // ---- UNISON (OSC B) ----
    OSC_B_UNISON,
    OSC_B_DETUNE,
    OSC_B_STEREO,
    OSC_B_PHASE,

    // ========================
    // OSC C
    // ========================
    OSC_C_SEMITONE,
    OSC_C_FINE,
    OSC_C_LEVEL,
    OSC_C_PAN,
    OSC_C_WT_POS,

    // ---- UNISON (OSC C) ----
    OSC_C_UNISON,
    OSC_C_DETUNE,
    OSC_C_STEREO,
    OSC_C_PHASE,

    // ========================
    // SUB
    // ========================
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
    // FILTER 0
    // ========================
    FILTER0_CUTOFF,
    FILTER0_RES,
    FILTER0_DRIVE,
    FILTER0_FAT,
    FILTER0_MIX,
    FILTER0_PAN,
    FILTER0_LEVEL,

    // ========================
    // FILTER 1
    // ========================
    FILTER1_CUTOFF,
    FILTER1_RES,
    FILTER1_DRIVE,
    FILTER1_FAT,
    FILTER1_MIX,
    FILTER1_PAN,
    FILTER1_LEVEL,

    // ---- BUS SENDS ----
    BUS1_SEND,
    BUS2_SEND,

    // ---- FILTER BUS SENDS ----
    FILTER0_BUS1_SEND,
    FILTER0_BUS2_SEND,
    FILTER1_BUS1_SEND,
    FILTER1_BUS2_SEND,

    FX_WET,

    // ========================
    // REVERB (GLOBAL FX)
    // ========================
    REVERB_SIZE,
    REVERB_PREDELAY,
    REVERB_LOW_CUT,
    REVERB_HIGH_CUT,

    // ---- PLATE ----
    REVERB_PLATE_DAMP,
    REVERB_PLATE_WIDTH,

    // ---- HALL ----
    REVERB_HALL_DECAY,
    REVERB_HALL_SPIN_RATE,
    REVERB_HALL_SPIN_DEPTH,

    // ---- VINTAGE ----
    REVERB_VINTAGE_ER_SIZE,
    REVERB_VINTAGE_DAMP,
    REVERB_VINTAGE_DIFF_A,
    REVERB_VINTAGE_DIFF_B,
    REVERB_VINTAGE_CHORUS_RATE,
    REVERB_VINTAGE_CHORUS_DEPTH,

    // ---- NITROUS ----
    REVERB_NITROUS_DIFFUSION,
    REVERB_NITROUS_FEEDBACK,
    REVERB_NITROUS_MODE,
    REVERB_NITROUS_CHORUS_RATE,
    REVERB_NITROUS_CHORUS_DEPTH,

    // ---- BASIN ----
    REVERB_BASIN_FEEDBACK,
    REVERB_BASIN_CHORUS_RATE,
    REVERB_BASIN_CHORUS_DEPTH,

    COUNT
};
