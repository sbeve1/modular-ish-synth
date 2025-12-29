#pragma once

// Flat, discrete engine-side Reverb parameter block.
// No nested structs — every parameter is a top-level field.
// Each mode has an explicit "enabled" flag (per your request).
// The host/UI should copy relevant fields into the audio FX Controls.

#include <cstdint>

enum class ReverbMode : int {
    OFF = -1,
    PLATE = 0,
    HALL = 1,
    VINTAGE = 2,
    NITROUS = 3,
    BASIN = 4
};

// -----------------------------
// ReverbParams (flat)
// -----------------------------
struct ReverbParams {
    // Global
    bool enabled = true;              // global rev enable / bypass
    ReverbMode mode = ReverbMode::PLATE;

    // -----------------------------
    // Plate (exposed + modulatable)
    // -----------------------------
    bool plate_enabled = true;
    float plate_sizeBase       = 0.35f;    // 0..1
    float plate_sizeMod        = 0.0f;
    float plate_preDelayBase   = 0.00f;   // seconds
    float plate_preDelayMod    = 0.0f;
    float plate_dampBase       = 0.25f;    // 0..1
    float plate_dampMod        = 0.0f;
    float plate_widthBase      = 0.2f;    // 0..1
    float plate_widthMod       = 0.0f;
    float plate_lowCutBase     = 0.0f;   // Hz
    float plate_lowCutMod      = 0.0f;
    float plate_highCutBase    = 1.0f; // 0..1
    float plate_highCutMod     = 0.0f;

    // -----------------------------
    // Hall
    // -----------------------------
    bool hall_enabled = true;
    float hall_sizeBase        = 0.7f;    // 0..1
    float hall_sizeMod         = 0.0f;
    float hall_preDelayBase    = 0.02f;   // seconds
    float hall_preDelayMod     = 0.0f;
    float hall_decayBase       = 2.5f;    // seconds
    float hall_decayMod        = 0.0f;
    float hall_spinRateBase    = 0.3f;    // Hz
    float hall_spinRateMod     = 0.0f;
    float hall_spinDepthBase   = 0.2f;    // 0..1
    float hall_spinDepthMod    = 0.0f;
    float hall_lowCutBase      = 20.0f;   // Hz
    float hall_lowCutMod       = 0.0f;
    float hall_highCutBase     = 16000.0f;// Hz
    float hall_highCutMod      = 0.0f;

    // -----------------------------
    // Vintage
    // -----------------------------
    bool vintage_enabled = true;
    float vintage_lowCutBase     = 20.0f;   // Hz
    float vintage_lowCutMod      = 0.0f;
    float vintage_highCutBase    = 14000.0f;// Hz
    float vintage_highCutMod     = 0.0f;
    float vintage_sizeBase       = 0.5f;    // 0..1
    float vintage_sizeMod        = 0.0f;
    float vintage_preDelayBase   = 0.01f;   // seconds
    float vintage_preDelayMod    = 0.0f;
    float vintage_decayBase      = 1.8f;    // seconds
    float vintage_decayMod       = 0.0f;
    float vintage_erSizeBase     = 0.025f;  // seconds
    float vintage_erSizeMod      = 0.0f;
    float vintage_dampBase       = 0.35f;   // 0..1
    float vintage_dampMod        = 0.0f;
    float vintage_diffA_Base     = 0.5f;    // 0..1
    float vintage_diffA_Mod      = 0.0f;
    float vintage_diffB_Base     = 0.5f;    // 0..1
    float vintage_diffB_Mod      = 0.0f;
    float vintage_chorusRateBase = 0.25f;   // Hz
    float vintage_chorusRateMod  = 0.0f;
    float vintage_chorusDepthBase= 0.15f;   // 0..1
    float vintage_chorusDepthMod = 0.0f;

    // -----------------------------
    // Nitrous
    // -----------------------------
    bool nitrous_enabled = true;
    float nitrous_lowCutBase     = 20.0f;   // Hz
    float nitrous_lowCutMod      = 0.0f;
    float nitrous_highCutBase    = 16000.0f;// Hz
    float nitrous_highCutMod     = 0.0f;
    float nitrous_sizeBase       = 0.6f;    // 0..1
    float nitrous_sizeMod        = 0.0f;
    float nitrous_preDelayBase   = 0.02f;   // seconds
    float nitrous_preDelayMod    = 0.0f;
    float nitrous_diffusionBase  = 0.5f;    // 0..1
    float nitrous_diffusionMod   = 0.0f;
    float nitrous_feedbackBase   = 0.0f;    // -1..1 (handled/clamped in DSP)
    float nitrous_feedbackMod    = 0.0f;
    int   nitrous_modeIndex      = 0;       // 0=Space,1=Marble,2=Rectangle,3=Hexagon,4=Box
    float nitrous_chorusRateBase = 0.3f;    // Hz
    float nitrous_chorusRateMod  = 0.0f;
    float nitrous_chorusDepthBase= 0.12f;   // 0..1
    float nitrous_chorusDepthMod = 0.0f;

    // -----------------------------
    // Basin
    // -----------------------------
    bool basin_enabled = true;
    float basin_lowCutBase       = 20.0f;   // Hz
    float basin_lowCutMod        = 0.0f;
    float basin_highCutBase      = 16000.0f;// Hz
    float basin_highCutMod       = 0.0f;
    float basin_sizeBase         = 0.6f;    // 0..1
    float basin_sizeMod          = 0.0f;
    float basin_preDelayBase     = 0.02f;   // seconds
    float basin_preDelayMod      = 0.0f;
    float basin_feedbackBase     = 0.0f;    // -1..1
    float basin_feedbackMod      = 0.0f;
    float basin_chorusRateBase   = 0.25f;   // Hz
    float basin_chorusRateMod    = 0.0f;
    float basin_chorusDepthBase  = 0.1f;    // 0..1
    float basin_chorusDepthMod   = 0.0f;
};
